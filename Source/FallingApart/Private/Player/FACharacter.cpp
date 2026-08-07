// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/FACharacter.h"
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
	float DeltaTime = GetWorld()->GetDeltaSeconds();

	CameraBoom->TargetOffset.Z = FMath::FInterpTo(CameraBoom->TargetOffset.Z, 0.f, DeltaTime, CameraInterpSpeed);

	if (FMath::IsNearlyEqual(CameraBoom->TargetOffset.Z, 0.f, 1.0f))
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

	if (FMath::IsNearlyEqual(CameraBoom->TargetArmLength, TargetArmLength, 1.0f))
	{
		CameraBoom->TargetArmLength = TargetArmLength;
		CameraBoom->SocketOffset.Z = TargetSocketOffsetZ;
		GetWorldTimerManager().ClearTimer(PerspectiveTimerHandle);
	}
}

void AFACharacter::TriggerPrimary()
{

}
