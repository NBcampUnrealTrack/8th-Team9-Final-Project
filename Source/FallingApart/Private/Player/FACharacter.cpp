// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/FACharacter.h"
#include "Audio/FAAudioSubsystem.h"
#include "AttributeSet/FAAttributeSet.h"
#include "Camera/CameraComponent.h"
#include "GAS/FAAbilitySystemComponent.h"
#include "GAS/FAGameplayAbility.h"
#include "GAS/FAGameplayTags.h"
#include "GameFramework/SpringArmComponent.h"
#include "Player/FACharacterMovementComponent.h"
#include "Player/FAPlayerState.h"
#include "Components/InputComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Voice/FAVoiceComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"

AFACharacter::AFACharacter(const FObjectInitializer& ObjectInitializer)
	// 기본 CMC 를 룰 반응형 CMC 로 교체한다. 이동 룰은 전부 여기서 소비된다.
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UFACharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	bIsFirstPerson = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 500.f, 0.f);
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
	GetCharacterMovement()->MaxWalkSpeedCrouched = 300.f;

	ThirdPersonArmLength = 300.f;
	CameraInterpSpeed = 12.f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = ThirdPersonArmLength;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = CameraInterpSpeed;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// 3인칭이어도 오디오 리스너는 카메라가 아니라 캐릭터 헤드에 붙는다(기획 확정).
	// 카메라/스프링암 쪽에서 SetAudioListenerOverride 를 호출하면 이 동작이 깨진다.
	VoiceComponent = CreateDefaultSubobject<UFAVoiceComponent>(TEXT("VoiceComponent"));

	AbilitySystemComponent = CreateDefaultSubobject<UFAAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AttributeSet = CreateDefaultSubobject<UFAAttributeSet>(TEXT("AttributeSet"));
}

UAbilitySystemComponent* AFACharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UFACharacterMovementComponent* AFACharacter::GetFAMovementComponent() const
{
	return Cast<UFACharacterMovementComponent>(GetCharacterMovement());
}

void AFACharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	InitializeAbilitySystem();
}

void AFACharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	InitializeAbilitySystem();
}

void AFACharacter::InitializeAbilitySystem()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	if (!HasAuthority() || bAbilitiesGranted)
	{
		return;
	}

	// 어빌리티 지급은 서버만. 클라이언트에는 스펙이 복제.
	for (const TSubclassOf<UFAGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		if (!AbilityClass)
		{
			continue;
		}

		AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
	}

	bAbilitiesGranted = true;

	if (const AFAPlayerState* FAPlayerState = GetPlayerState<AFAPlayerState>())
	{
		SetTeamStateTag(FAPlayerState->GetTeam());
	}
}

void AFACharacter::SetTeamStateTag(EFATeam NewTeam)
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{ 
		return;
	}

	const FAGameplayTags& FATags = FAGameplayTags::Get();

	// 진영은 상호 배타. 켜는 것과 끄는 것을 한 번에 처리.
	AbilitySystemComponent->SetLooseGameplayTagCount(
		FATags.State_Human, NewTeam == EFATeam::Human ? 1 : 0, EGameplayTagReplicationState::TagAndCountToAll);
	AbilitySystemComponent->SetLooseGameplayTagCount(
		FATags.State_Zombie, NewTeam == EFATeam::Zombie ? 1 : 0, EGameplayTagReplicationState::TagAndCountToAll);
}

void AFACharacter::ApplyInvulnerability(float Duration)
{
	if (!HasAuthority() || !AbilitySystemComponent || !InvulnerabilityEffect)
	{
		if (HasAuthority() && !InvulnerabilityEffect)
		{
			UE_LOG(LogTemp, Warning, TEXT("[GAS] InvulnerabilityEffect 가 지정되지 않았다 — 감염 직후 무적이 동작하지 않는다."));
		}
		return;
	}

	FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
	Context.AddSourceObject(this);

	const FGameplayEffectSpecHandle SpecHandle =
		AbilitySystemComponent->MakeOutgoingSpec(InvulnerabilityEffect, 1.0f, Context);

	if (SpecHandle.IsValid() && SpecHandle.Data.IsValid())
	{
		// GE 의 Duration Magnitude 를 Set By Caller 로 열어두면 호출부가 시간을 정할 수 있음.
		SpecHandle.Data->SetSetByCallerMagnitude(FAGameplayTags::Get().Data_Rule_Magnitude, Duration);
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
}

void AFACharacter::StartQTESync()
{
	GetWorldTimerManager().SetTimer(QTESyncTimerHandle, this, &AFACharacter::SyncQTEPressesWithServer, 0.15f, true);
}

void AFACharacter::StopQTESync()
{
	GetWorldTimerManager().ClearTimer(QTESyncTimerHandle);
	LocalQTEPressCount = 0;
	QTE_TargetVictim = nullptr;
}

void AFACharacter::ReceiveQTEPress(AFACharacter* InstigatorCharacter, int32 Presses)
{
	if (!HasAuthority() || !InstigatorCharacter)
		return;

	UAbilitySystemComponent* InstigatorASC = InstigatorCharacter->GetAbilitySystemComponent();

	if (!InstigatorASC || !AbilitySystemComponent)
		return;

	const FAGameplayTags& FATags = FAGameplayTags::Get();

	// 잡힌 사람에게 더 이상 'State_Contesting' 태그가 없다면, 이미 승패가 결정난 이후에 네트워크 지연으로 뒤늦게 도착한 패킷이므로 무시
	if (!AbilitySystemComponent->HasMatchingGameplayTag(FATags.State_Contesting))
		return;

	if (InstigatorASC->HasMatchingGameplayTag(FATags.State_Zombie) && InstigatorASC->HasMatchingGameplayTag(FATags.State_Grabbing))
	{
		CurrentQTEGauge -= Presses * 4.0f;
	}
	else if (InstigatorASC->HasMatchingGameplayTag(FATags.State_Human))
	{
		if (InstigatorASC->HasMatchingGameplayTag(FATags.State_Grabbed))
		{
			CurrentQTEGauge += Presses * 1.0f;
		}
		else
		{
			CurrentQTEGauge += Presses * 2.0f;
		}
	}

	CurrentQTEGauge = FMath::Clamp(CurrentQTEGauge, 0.f, 100.f);

	if (CurrentQTEGauge >= 100.f)
	{
		AbilitySystemComponent->SetLooseGameplayTagCount(FATags.State_Contesting, 0);
		StopQTESync();

		// TODO: 인간 승리 로직
	}
	else if (CurrentQTEGauge <= 0.f)
	{
		AbilitySystemComponent->SetLooseGameplayTagCount(FATags.State_Contesting, 0);
		StopQTESync();

		// TODO: 좀비 승리 로직
	}
}

void AFACharacter::ResetQTEGauge()
{
	if (HasAuthority())
	{
		CurrentQTEGauge = 50.f;
	}
}

void AFACharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsFirstPerson && !GetWorldTimerManager().IsTimerActive(PerspectiveTimerHandle))
	{
		UpdateCameraCollision(DeltaTime);
	}
}

void AFACharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFACharacter, CurrentQTEGauge);
}

void AFACharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AFACharacter::Move);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AFACharacter::Look);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AFACharacter::Input_Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AFACharacter::Input_StopJumping);
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &AFACharacter::ToggleCrouch);
		EnhancedInputComponent->BindAction(PerspectiveAction, ETriggerEvent::Started, this, &AFACharacter::TogglePerspective);
		EnhancedInputComponent->BindAction(PrimaryAction, ETriggerEvent::Triggered, this, &AFACharacter::TriggerPrimary);
	}
}

void AFACharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	CameraBoom->TargetOffset.Z += HalfHeightAdjust;

	// 크라우치는 bIsCrouched 가 복제되면서 모든 머신에서 이 함수가 불리므로 주변 사람에게도 들린다.
	if (UFAAudioSubsystem* Audio = UFAAudioSubsystem::Get(this))
	{
		Audio->PlaySoundAtLocation(TEXT("Crouch_In"), GetActorLocation());
	}

	GetWorldTimerManager().ClearTimer(CrouchTimerHandle);

	if (!GetWorldTimerManager().IsTimerActive(CrouchTimerHandle))
	{
		GetWorldTimerManager().SetTimer(CrouchTimerHandle, this, &AFACharacter::UpdateCrouchCamera, GetWorld()->GetDeltaSeconds(), true);
	}
}

void AFACharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	CameraBoom->TargetOffset.Z -= HalfHeightAdjust;

	if (UFAAudioSubsystem* Audio = UFAAudioSubsystem::Get(this))
	{
		Audio->PlaySoundAtLocation(TEXT("Crouch_Out"), GetActorLocation());
	}

	GetWorldTimerManager().ClearTimer(CrouchTimerHandle);

	if (!GetWorldTimerManager().IsTimerActive(CrouchTimerHandle))
	{
		GetWorldTimerManager().SetTimer(CrouchTimerHandle, this, &AFACharacter::UpdateCrouchCamera, GetWorld()->GetDeltaSeconds(), true);
	}
}

void AFACharacter::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AFACharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void AFACharacter::Input_Jump()
{
	// QTE 진행 중이면 점프를 막고 연타 카운트 증가
	const FGameplayTag QTETag = FAGameplayTags::Get().State_Contesting;

	if (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(QTETag))
	{
		LocalQTEPressCount++;
		return;
	}

	const FGameplayTag JumpTag = FAGameplayTags::Get().Ability_Jump;

	if (AbilitySystemComponent)
	{
		if (AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(JumpTag)))
		{
			return;
		}

		for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.Ability->GetAssetTags().HasTagExact(JumpTag))
			{
				return;
			}
		}
	}

	// BP_FACharacter 의 DefaultAbilities 에 GA_Jump 를 아직 안 넣은 상태 — 기존 동작 유지.
	Jump();
}

void AFACharacter::Input_StopJumping()
{
	StopJumping();
}

void AFACharacter::ToggleCrouch()
{
	if (GetCharacterMovement()->IsFalling())
		return;

	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

void AFACharacter::UpdateCrouchCamera()
{
	CameraBoom->TargetOffset.Z = FMath::FInterpTo(CameraBoom->TargetOffset.Z, 0.f, GetWorld()->GetDeltaSeconds(), CameraInterpSpeed);

	if (FMath::IsNearlyEqual(CameraBoom->TargetOffset.Z, 0.f, 1.f))
	{
		CameraBoom->TargetOffset.Z = 0.f;
		GetWorldTimerManager().ClearTimer(CrouchTimerHandle);
	}
}

void AFACharacter::TogglePerspective()
{
	bIsFirstPerson = !bIsFirstPerson;
	GetWorldTimerManager().ClearTimer(PerspectiveTimerHandle);

	if (!GetWorldTimerManager().IsTimerActive(PerspectiveTimerHandle))
	{
		GetWorldTimerManager().SetTimer(PerspectiveTimerHandle, this, &AFACharacter::UpdatePerspectiveCamera, GetWorld()->GetDeltaSeconds(), true);
	}
}

void AFACharacter::UpdatePerspectiveCamera()
{
	float DeltaTime = GetWorld()->GetDeltaSeconds();
	float TargetArmLength = bIsFirstPerson ? 0.f : ThirdPersonArmLength;
	float TargetSocketOffsetZ = bIsFirstPerson ? BaseEyeHeight : 0.f;

	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetArmLength, DeltaTime, CameraInterpSpeed);
	CameraBoom->SocketOffset.Z = FMath::FInterpTo(CameraBoom->SocketOffset.Z, TargetSocketOffsetZ, DeltaTime, CameraInterpSpeed);

	GetMesh()->SetOwnerNoSee(CameraBoom->TargetArmLength < 40.f);

	if (FMath::IsNearlyEqual(CameraBoom->TargetArmLength, TargetArmLength, 1.f))
	{
		CameraBoom->TargetArmLength = TargetArmLength;
		CameraBoom->SocketOffset.Z = TargetSocketOffsetZ;
		GetWorldTimerManager().ClearTimer(PerspectiveTimerHandle);
	}
}

void AFACharacter::UpdateCameraCollision(float DeltaTime)
{
	float TargetArmLength = ThirdPersonArmLength;
	FVector Start = CameraBoom->GetComponentLocation();
	FVector Direction = (FollowCamera->GetComponentLocation() - Start).GetSafeNormal();

	if (Direction.IsNearlyZero())
	{
		Direction = -CameraBoom->GetForwardVector();
	}

	FVector End = Start + (Direction * ThirdPersonArmLength);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams(FName("CameraCollision"), false, this);
	QueryParams.AddIgnoredActor(this);


	bool bHit = GetWorld()->SweepSingleByChannel(
		HitResult,
		Start,
		End,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(CameraBoom->ProbeSize),
		QueryParams
	);

	if (bHit)
	{
		TargetArmLength = FMath::Clamp(HitResult.Distance - CameraBoom->ProbeSize, 30.f, ThirdPersonArmLength);
	}

	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetArmLength, DeltaTime, CameraInterpSpeed);
}

void AFACharacter::TriggerPrimary()
{

}

void AFACharacter::OnRep_QTEGauge()
{
	// UI 처리

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Cyan, FString::Printf(TEXT("QTE 게이지: %.1f"), CurrentQTEGauge));
	}
}

void AFACharacter::SyncQTEPressesWithServer()
{
	if (LocalQTEPressCount > 0)
	{
		Server_SendQTEPresses(LocalQTEPressCount);
		LocalQTEPressCount = 0;
	}
}

bool AFACharacter::Server_SendQTEPresses_Validate(int32 Presses)
{
	return (Presses >= 0 && Presses <= 15); // 비정상 입력 차단
}

void AFACharacter::Server_SendQTEPresses_Implementation(int32 Presses)
{
	if (IsValid(QTE_TargetVictim))
	{
		// 지정된 타겟(잡힌 인간)에게 클릭 횟수 전달
		QTE_TargetVictim->ReceiveQTEPress(this, Presses);
	}
}
