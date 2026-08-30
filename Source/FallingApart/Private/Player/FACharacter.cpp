// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/FACharacter.h"
#include "Audio/FAAudioSubsystem.h"
#include "AttributeSet/FAAttributeSet.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "GAS/FAAbilitySystemComponent.h"
#include "GAS/FAGameplayAbility.h"
#include "GAS/FAGameplayTags.h"
#include "Interact/FAInteractable.h"
#include "Interact/FAInteractionComponent.h"
#include "Interact/FACabinet.h"
#include "Interact/FALadder.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInterface.h"
#include "Player/FACharacterMovementComponent.h"
#include "Player/FAPlayerState.h"
#include "Components/InputComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Rule/FARuleSettings.h"
#include "UI/FACaptureMarkerWidget.h"
#include "UI/FARuleMarkerWidget.h"
#include "Voice/FAVoiceComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"
#include "Player/FACharacterStatusComponent.h"
#include "Settings/FASettingsBridge.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"

namespace FACharacterAudio
{
	// 표의 RowName 과 같아야 한다 (DT_Sound_Character).
	static const FName CrouchIn(TEXT("Crouch_In"));
	static const FName CrouchOut(TEXT("Crouch_Out"));
	static const FName JumpStart(TEXT("Jump_Start"));
	static const FName JumpLand(TEXT("Jump_Land"));
	static const FName LandHeavy(TEXT("Land_Heavy"));
	static const FName GrowlZombieLoop(TEXT("Growl_Zombie_Loop"));

	/** 물 — 들어갈 때 한 번, 잠겨 있는 동안 루프, 나올 때 한 번. */
	static const FName WaterSplashEnter(TEXT("Water_Splash_Enter"));
	static const FName WaterWadeLoop(TEXT("Water_Wade_Loop"));
	static const FName WaterExit(TEXT("Water_Exit"));

	/** 캐비넷에 들어가고 나오는 소리. 캐비넷 문 자체의 소리(Cabinet_Open/Close)와는 별개다. */
	static const FName HideEnter(TEXT("Hide_Enter"));
	static const FName HideExit(TEXT("Hide_Exit"));

	/** 사다리. 등반 애니메이션이 없어 노티파이를 찍을 데가 없으므로 이동 거리로 낸다. */
	static const FName LadderMount(TEXT("Ladder_Mount"));
	static const FName LadderStep(TEXT("Ladder_Step"));
	static const FName LadderDismount(TEXT("Ladder_Dismount"));

	/** QTE 연타 피드백. 누른 본인에게만 나는 2D 소리라 멀티캐스트를 타지 않는다. */
	static const FName QTEPress(TEXT("QTE_Press"));
}

namespace FAClimb
{
	/** 사다리에 붙은 뒤 끝단 판정을 미루는 시간(초). 이게 없으면 바닥에서 탄 순간 바로 풀린다. */
	static constexpr float ExitGraceSeconds = 0.35f;

	/** 중심선으로 되돌아오는 속도 계수. 클수록 딱딱 붙고, 작을수록 부드럽게 미끄러져 들어온다. */
	static constexpr float LateralSnapSpeed = 10.0f;
}

AFACharacter::AFACharacter(const FObjectInitializer& ObjectInitializer)
	// 기본 CMC 를 룰 반응형 CMC 로 교체한다. 이동 룰은 전부 여기서 소비된다.
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UFACharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	// WBP_FARuleMarker 가 만들어지기 전에도 머리 위 룰 표시를 확인할 수 있어야 한다.
	RuleMarkerWidgetClass = UFARuleMarkerWidget::StaticClass();

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
	CameraBoom->ProbeSize = 20.0f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// 3인칭이어도 오디오 리스너는 카메라가 아니라 캐릭터 헤드에 붙는다(기획 확정).
	// 카메라/스프링암 쪽에서 SetAudioListenerOverride 를 호출하면 이 동작이 깨진다.
	VoiceComponent = CreateDefaultSubobject<UFAVoiceComponent>(TEXT("VoiceComponent"));

	// 상호작용 탐지. 로컬 컨트롤 캐릭터에서만 틱이 돈다(컴포넌트가 BeginPlay 에서 스스로 끈다).
	InteractionComponent = CreateDefaultSubobject<UFAInteractionComponent>(TEXT("InteractionComponent"));

	AbilitySystemComponent = CreateDefaultSubobject<UFAAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AttributeSet = CreateDefaultSubobject<UFAAttributeSet>(TEXT("AttributeSet"));

	CharacterStatusComponent = CreateDefaultSubobject<UFACharacterStatusComponent>(TEXT("CharacterStatusComponent"));
}

UAbilitySystemComponent* AFACharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UFACharacterMovementComponent* AFACharacter::GetFAMovementComponent() const
{
	return Cast<UFACharacterMovementComponent>(GetCharacterMovement());
}

void AFACharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 머리 위 감염 게이지를 주변 사람이 봐야 해서 전원에게 보낸다.
	// 포획 중이 아닐 때는 값이 안 움직이므로 평상시 대역폭은 0 이다.
	DOREPLIFETIME(AFACharacter, CaptureState);
	DOREPLIFETIME(AFACharacter, ContestState);

	// 등반은 남이 봐도 자세가 달라져야 하므로 전원에게 보낸다.
	DOREPLIFETIME(AFACharacter, ClimbingLadder);
	DOREPLIFETIME(AFACharacter, HidingCabinet);
}

//~ ---------------- 포획 감염 게이지 ----------------

float AFACharacter::GetServerWorldTime() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	const AGameStateBase* CurrentGameState = World->GetGameState();
	return CurrentGameState ? CurrentGameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}

float AFACharacter::GetCaptureInfectionRatio() const
{
	if (!CaptureState.IsActive())
	{
		return 0.0f;
	}

	// 서버는 스냅샷(진행도 + 그때의 속도)만 띄엄띄엄 보낸다 — 사이는 속도로 외삽해야 게이지가 끊기지 않는다.
	// 구출 QTE 로 멈춘 동안은 속도가 0 이라 저절로 제자리에 선다.
	const float Elapsed = FMath::Max(0.0f, GetServerWorldTime() - CaptureState.ProgressServerTime);

	return FMath::Clamp(CaptureState.InfectProgress + Elapsed * CaptureState.InfectRatePerSecond, 0.0f, 1.0f);
}

void AFACharacter::BeginCaptureState(bool bInIsVictim)
{
	if (!HasAuthority())
	{
		return;
	}

	CaptureState.bActive = true;
	CaptureState.bIsVictim = bInIsVictim;
	CaptureState.InfectProgress = 0.0f;
	CaptureState.InfectRatePerSecond = 0.0f;
	CaptureState.ProgressServerTime = GetServerWorldTime();
	CaptureState.ResistRatio = 0.0f;

	// 지난 포획의 저항이 남아 있으면 새로 잡힌 순간부터 감염이 느려진다.
	ResistLevel = 0.0f;
	LastResistInputTime = -1.0f;
	LastResistDirection = 0;

	// 사다리 위에서 잡혔다면 남아 있던 등반 속도를 여기서 끊는다.
	if (IsClimbing())
	{
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}
	}
}

void AFACharacter::UpdateCaptureProgress(float Progress, float RatePerSecond)
{
	if (!HasAuthority() || !CaptureState.IsActive())
	{
		return;
	}

	CaptureState.InfectProgress = FMath::Clamp(Progress, 0.0f, 1.0f);
	CaptureState.InfectRatePerSecond = FMath::Max(0.0f, RatePerSecond);
	CaptureState.ProgressServerTime = GetServerWorldTime();
}

void AFACharacter::SetCaptureResistRatio(float Ratio)
{
	if (!HasAuthority() || !CaptureState.IsActive())
	{
		return;
	}

	CaptureState.ResistRatio = FMath::Clamp(Ratio, 0.0f, 1.0f);
}

void AFACharacter::EndCaptureState()
{
	if (!HasAuthority())
	{
		return;
	}

	CaptureState = FFACaptureState();

	ResistLevel = 0.0f;
	LastResistInputTime = -1.0f;
	LastResistDirection = 0;
}

//~ ---------------- 감염 저항 (WASD 연타) ----------------

float AFACharacter::UpdateResistLevel(float DeltaSeconds)
{
	if (!HasAuthority())
	{
		return 0.0f;
	}

	// [H38 유리 몸] 포획 도중 룰이 적용되어도 이전 저항이 남지 않게 서버에서 즉시 0으로 만든다.
	if (AbilitySystemComponent &&
		(AbilitySystemComponent->HasMatchingGameplayTag(FAGameplayTags::Get().Rule_Trade_Hide) ||
		 AbilitySystemComponent->HasMatchingGameplayTag(FAGameplayTags::Get().Rule_State_NoStruggle) ||
		 AbilitySystemComponent->HasMatchingGameplayTag(FAGameplayTags::Get().State_NoStruggle)))
	{
		ResistLevel = 0.0f;
		return 0.0f;
	}

	ResistLevel = FMath::Clamp(ResistLevel - ResistDecayPerSecond * DeltaSeconds, 0.0f, 1.0f);

	return ResistLevel;
}

int32 AFACharacter::QuantizeResistDirection(const FVector2D& MovementVector)
{
	// 데드존을 넉넉히 잡는다 — 살짝 기울어진 스틱이 두 방향 사이에서 떨면 손을 대지 않아도 저항이 오른다.
	if (MovementVector.SizeSquared() < 0.25f)
	{
		return 0;
	}

	if (FMath::Abs(MovementVector.X) > FMath::Abs(MovementVector.Y))
	{
		return MovementVector.X > 0.0f ? 1 : 2;
	}

	return MovementVector.Y > 0.0f ? 3 : 4;
}

void AFACharacter::HandleResistInput(const FVector2D& MovementVector)
{
	const int32 Direction = QuantizeResistDirection(MovementVector);
	if (Direction == 0 || Direction == LastResistDirection)
	{
		return;
	}

	LastResistDirection = Direction;

	// 서버에서 부르면 RPC 가 그 자리에서 실행되므로 호스트도 같은 경로를 탄다.
	Server_ReportResistInput();
}

void AFACharacter::Input_MoveReleased()
{
	LastResistDirection = 0;
}

void AFACharacter::Server_ReportResistInput_Implementation()
{
	RegisterResistInput();
}

void AFACharacter::RegisterResistInput()
{
	// 잡힌 사람만 저항한다. 잡고 있는 좀비 쪽 입력이 섞이면 자기 감염을 자기가 늦추는 꼴이 된다.
	if (!HasAuthority() || !CaptureState.IsActive() || !CaptureState.bIsVictim)
	{
		return;
	}

	// [H38 유리 몸] 클라이언트가 저항 RPC를 직접 보내도 서버에서 최종 차단한다.
	if (AbilitySystemComponent &&
		(AbilitySystemComponent->HasMatchingGameplayTag(FAGameplayTags::Get().Rule_Trade_Hide) ||
		 AbilitySystemComponent->HasMatchingGameplayTag(FAGameplayTags::Get().Rule_State_NoStruggle) ||
		 AbilitySystemComponent->HasMatchingGameplayTag(FAGameplayTags::Get().State_NoStruggle)))
	{
		return;
	}

	const float Now = GetServerWorldTime();
	if (LastResistInputTime >= 0.0f && Now - LastResistInputTime < ResistInputMinInterval)
	{
		return;
	}

	LastResistInputTime = Now;
	ResistLevel = FMath::Clamp(ResistLevel + ResistGainPerInput, 0.0f, 1.0f);
}

//~ ---------------- 구출 QTE ----------------

float AFACharacter::GetContestTimeRemaining() const
{
	if (!ContestState.IsActive())
	{
		return 0.0f;
	}

	return FMath::Max(0.0f, ContestState.EndServerTime - GetServerWorldTime());
}

void AFACharacter::BeginContestState(EFAContestRole InRole, AActor* InHostZombie, float Duration)
{
	if (!HasAuthority() || InRole == EFAContestRole::None)
	{
		return;
	}

	ContestState.Role = InRole;
	ContestState.HostZombie = InHostZombie;
	ContestState.EndServerTime = GetServerWorldTime() + FMath::Max(0.0f, Duration);

	// 줄다리기는 항상 중립에서 시작한다. 지난 QTE 의 마지막 값이 한 프레임 비치면 안 된다.
	ContestState.Gauge = 0.5f;
}

void AFACharacter::SetContestGauge(float Gauge01)
{
	if (!HasAuthority() || !ContestState.IsActive())
	{
		return;
	}

	ContestState.Gauge = FMath::Clamp(Gauge01, 0.0f, 1.0f);
}

void AFACharacter::EndContestState()
{
	if (!HasAuthority())
	{
		return;
	}

	ContestState = FFAContestState();
}

bool AFACharacter::IsInRescueContest() const
{
	if (ContestState.IsActive())
	{
		return true;
	}

	return AbilitySystemComponent
		&& AbilitySystemComponent->HasMatchingGameplayTag(FAGameplayTags::Get().State_Contesting);
}

//~ ---------------- 머리 위 감염 게이지 ----------------

void AFACharacter::SetupCaptureMarker()
{
	// 데디케이티드 서버는 아무것도 그리지 않는다.
	if (CaptureMarkerWidgetClass == nullptr || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	CaptureMarker = NewObject<UWidgetComponent>(this);
	CaptureMarker->SetWidgetSpace(EWidgetSpace::Screen);

	// DrawAtDesiredSize 를 켜면 WBP 원본 크기가 그대로 화면 픽셀이 된다.
	CaptureMarker->SetDrawAtDesiredSize(false);
	CaptureMarker->SetDrawSize(CaptureMarkerDrawSize);
	CaptureMarker->SetWidgetClass(CaptureMarkerWidgetClass);
	CaptureMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CaptureMarker->RegisterComponent();
	CaptureMarker->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	CaptureMarker->SetRelativeLocation(CaptureMarkerOffset);
	CaptureMarker->SetVisibility(false);
}

bool AFACharacter::ShouldShowCaptureMarker() const
{
	// 잡힌 쪽만 머리 위에 띄운다. 잡고 있는 좀비 머리 위에도 뜨면 누가 인질인지 헷갈린다.
	if (!CaptureState.IsActive() || !CaptureState.bIsVictim)
	{
		return false;
	}

	// 잡힌 본인은 HUD 게이지를 이미 보고 있다 — 3인칭에서 자기 머리 위에 같은 게 하나 더 뜨면 겹친다.
	if (IsLocallyControlled())
	{
		return false;
	}

	if (CaptureMarkerCullDistance > 0.0f)
	{
		const UWorld* World = GetWorld();
		const APlayerController* LocalController = World ? World->GetFirstPlayerController() : nullptr;
		if (!LocalController)
		{
			return false;
		}

		FVector ViewLocation;
		FRotator ViewRotation;
		LocalController->GetPlayerViewPoint(ViewLocation, ViewRotation);

		if (FVector::DistSquared(ViewLocation, GetActorLocation()) > FMath::Square(CaptureMarkerCullDistance))
		{
			return false;
		}
	}

	return true;
}

void AFACharacter::UpdateCaptureMarker()
{
	if (!CaptureMarker)
	{
		return;
	}

	const bool bShouldShow = ShouldShowCaptureMarker();

	if (bShouldShow != CaptureMarker->IsVisible())
	{
		CaptureMarker->SetVisibility(bShouldShow);
	}

	if (!bShouldShow)
	{
		return;
	}

	if (UFACaptureMarkerWidget* MarkerWidget = Cast<UFACaptureMarkerWidget>(CaptureMarker->GetUserWidgetObject()))
	{
		MarkerWidget->UpdateMarker(GetCaptureInfectionRatio(), ContestState.Role, ContestState.Gauge);
	}
}

//~ ---------------- 머리 위 보유 룰 아이콘 ----------------

void AFACharacter::SetupRuleMarker()
{
	if (RuleMarkerWidgetClass == nullptr || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	RuleMarker = NewObject<UWidgetComponent>(this);
	RuleMarker->SetWidgetSpace(EWidgetSpace::Screen);
	RuleMarker->SetDrawAtDesiredSize(false);
	RuleMarker->SetDrawSize(RuleMarkerDrawSize);
	RuleMarker->SetWidgetClass(RuleMarkerWidgetClass);
	RuleMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RuleMarker->RegisterComponent();
	RuleMarker->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	RuleMarker->SetRelativeLocation(RuleMarkerOffset);
	RuleMarker->SetVisibility(false);
}

bool AFACharacter::ShouldShowRuleMarker() const
{
	// 본인 룰은 HUD 좌하단 4칸이 이미 보여준다 — 3인칭에서 자기 머리 위에 또 뜨면 겹친다.
	if (IsLocallyControlled())
	{
		return false;
	}

	if (!AbilitySystemComponent || AbilitySystemComponent->GetActiveRuleCount() <= 0)
	{
		return false;
	}

	if (AbilitySystemComponent->HasMatchingGameplayTag(FAGameplayTags::Get().State_Camp))
	{
		return true;
	}

	if (bHideRuleMarkerWhenOccluded && bRuleMarkerOccluded)
	{
		return false;
	}

	if (RuleMarkerCullDistance > 0.0f)
	{
		const UWorld* World = GetWorld();
		const APlayerController* LocalController = World ? World->GetFirstPlayerController() : nullptr;
		if (!LocalController)
		{
			return false;
		}

		FVector ViewLocation;
		FRotator ViewRotation;
		LocalController->GetPlayerViewPoint(ViewLocation, ViewRotation);

		if (FVector::DistSquared(ViewLocation, GetActorLocation()) > FMath::Square(RuleMarkerCullDistance))
		{
			return false;
		}
	}

	return true;
}

void AFACharacter::UpdateRuleMarkerOcclusion()
{
	const UWorld* World = GetWorld();
	const APlayerController* LocalController = World ? World->GetFirstPlayerController() : nullptr;
	if (!LocalController)
	{
		bRuleMarkerOccluded = true;
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	LocalController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	// 위젯이 뜨는 지점(머리 위)을 겨눈다. 발치를 겨누면 난간 뒤에 서 있을 때 아이콘이 깜빡인다.
	const FVector MarkerLocation = GetActorLocation() + RuleMarkerOffset;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FARuleMarkerOcclusion), /*bTraceComplex=*/false);
	Params.AddIgnoredActor(this);
	if (const APawn* LocalPawn = LocalController->GetPawn())
	{
		Params.AddIgnoredActor(LocalPawn);
	}

	FHitResult Hit;
	bRuleMarkerOccluded = World->LineTraceSingleByChannel(Hit, ViewLocation, MarkerLocation, ECC_Visibility, Params);
}

void AFACharacter::OnExposedTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	// 노출 상태가 변했으니 렌더링을 갱신
	RefreshCustomDepth();
}

void AFACharacter::OnMutualSightTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	// 투시 룰을 획득/상실했을 때, 로컬이라면 맵 전체 캐릭터의 렌더링을 갱신
	if (IsLocallyControlled())
	{
		for (TActorIterator<AFACharacter> It(GetWorld()); It; ++It)
		{
			if (*It)
			{
				(*It)->RefreshCustomDepth();
			}
		}
	}
}

void AFACharacter::RefreshCustomDepth()
{
	USkeletalMeshComponent* CharacterMesh = GetMesh();
	if (!CharacterMesh || !AbilitySystemComponent) return;

	bool bShouldReveal = false;
	const FAGameplayTags& FATags = FAGameplayTags::Get();

	// [H36 뿌리내림] 기존 상시 노출과 같은 X-Ray 아웃라인 경로를 사용한다.
	if (AbilitySystemComponent->HasMatchingGameplayTag(FATags.State_Exposed) ||
		AbilitySystemComponent->HasMatchingGameplayTag(FATags.State_Camp))
	{
		bShouldReveal = true;
	}
	else
	{
		const UWorld* World = GetWorld();
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		AFACharacter* LocalCharacter = PC ? Cast<AFACharacter>(PC->GetPawn()) : nullptr;

		if (LocalCharacter && LocalCharacter != this && LocalCharacter->GetFAAbilitySystemComponent())
		{
			if (LocalCharacter->GetFAAbilitySystemComponent()->HasMatchingGameplayTag(FATags.Rule_Trade_MutualSight))
			{
				// 진영 비교
				const bool bLocalIsHuman = LocalCharacter->GetFAAbilitySystemComponent()->HasMatchingGameplayTag(FATags.State_Human);
				const bool bThisIsZombie = AbilitySystemComponent->HasMatchingGameplayTag(FATags.State_Zombie);

				const bool bLocalIsZombie = LocalCharacter->GetFAAbilitySystemComponent()->HasMatchingGameplayTag(FATags.State_Zombie);
				const bool bThisIsHuman = AbilitySystemComponent->HasMatchingGameplayTag(FATags.State_Human);

				if ((bLocalIsHuman && bThisIsZombie) || (bLocalIsZombie && bThisIsHuman))
				{
					bShouldReveal = true; // 다른 진영이므로 내 화면에 렌더링 갱신
				}
			}
		}
	}

	CharacterMesh->SetRenderCustomDepth(bShouldReveal);
	CharacterMesh->SetCustomDepthStencilValue(bShouldReveal ? 1 : 0);
}

void AFACharacter::UpdateRuleMarker(float DeltaTime)
{
	if (!RuleMarker)
	{
		return;
	}

	if (bHideRuleMarkerWhenOccluded)
	{
		RuleMarkerOcclusionTimer += DeltaTime;
		if (RuleMarkerOcclusionTimer >= RuleMarkerOcclusionInterval)
		{
			RuleMarkerOcclusionTimer = 0.0f;
			UpdateRuleMarkerOcclusion();
		}
	}

	const bool bShouldShow = ShouldShowRuleMarker();

	if (bShouldShow != RuleMarker->IsVisible())
	{
		RuleMarker->SetVisibility(bShouldShow);
	}

	if (!bShouldShow)
	{
		return;
	}

	// 보유 목록이 바뀔 때만 DT 를 다시 뒤진다. 매 프레임 8명분을 조회할 이유가 없다.
	const TArray<FName>& ActiveRuleIds = AbilitySystemComponent->GetActiveRuleIds();
	if (ActiveRuleIds != CachedRuleIds)
	{
		CachedRuleIds = ActiveRuleIds;

		CachedRuleDisplayInfos.Reset(CachedRuleIds.Num());
		for (const FName& RuleId : CachedRuleIds)
		{
			CachedRuleDisplayInfos.Add(UFARuleSettings::MakeRuleDisplayInfo(RuleId));
		}
	}

	const UWorld* World = GetWorld();
	const APlayerController* LocalController = World ? World->GetFirstPlayerController() : nullptr;

	float Scale = 1.0f;
	bool bAbbreviated = false;

	if (LocalController)
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		LocalController->GetPlayerViewPoint(ViewLocation, ViewRotation);

		const float Distance = FVector::Dist(ViewLocation, GetActorLocation());

		bAbbreviated = RuleMarkerAbbreviateDistance > 0.0f && Distance > RuleMarkerAbbreviateDistance;

		// 거리 감쇠 — 가까운 상대가 크게 보여야 '누구의 룰인지'가 헷갈리지 않는다.
		const float FadeEnd = FMath::Max(RuleMarkerCullDistance, RuleMarkerFullScaleDistance + 1.0f);
		const float Alpha = FMath::Clamp(
			(Distance - RuleMarkerFullScaleDistance) / (FadeEnd - RuleMarkerFullScaleDistance), 0.0f, 1.0f);

		Scale = FMath::Lerp(1.0f, RuleMarkerMinScale, Alpha);
	}

	if (UFARuleMarkerWidget* MarkerWidget = Cast<UFARuleMarkerWidget>(RuleMarker->GetUserWidgetObject()))
	{
		MarkerWidget->UpdateMarker(CachedRuleDisplayInfos, Scale, bAbbreviated);
	}
}

void AFACharacter::BeginPlay()
{
	Super::BeginPlay();

	// 로비는 AFAPlayerState 가 아니라 AFALobbyPlayerState 를 쓴다 — 진영 알림이 아예 없다.
	// 진영을 모르는 맥락에서는 인간 외형이 기본이므로, 여기서 한 번 기준선을 깔아둔다.
	// 인게임에서는 진영이 정해지는 순간 HandleTeamChanged 가 다시 덮는다.
	const AFAPlayerState* FAPlayerState = GetPlayerState<AFAPlayerState>();
	ApplyTeamMaterial(FAPlayerState ? FAPlayerState->GetTeam() : EFATeam::None);

	SetupCaptureMarker();
	SetupRuleMarker();

	if (FollowCamera)
	{
		// Vision.NarrowFOV 계산의 기준값. 이후로는 이 값을 실제 FOV로 되돌리지 않는다 — 카메라 FOV는 고정.
		DefaultCameraFOV = FollowCamera->FieldOfView;
	}

	if (IsLocallyControlled() && VisionFogMaterial && FollowCamera)
	{
		// 배열에는 아직 안 넣는다 — UpdateVisionEffects 가 FogAmount > 0 일 때만 끼워 넣는다.
		VisionFogMID = UMaterialInstanceDynamic::Create(VisionFogMaterial, this);
	}

	if (IsLocallyControlled() && VisionNarrowFOVMaterial && FollowCamera)
	{
		// Fog와 별개의 MID. UpdateVisionEffects 가 NarrowAmount > 0 일 때만 끼워 넣는다.
		VisionNarrowFOVMID = UMaterialInstanceDynamic::Create(VisionNarrowFOVMaterial, this);
	}
}

void AFACharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	InitializeAbilitySystem();
	BindTeamNotify();
	RefreshInteractionTick();
}

void AFACharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	InitializeAbilitySystem();
	BindTeamNotify();
	RefreshInteractionTick();
}

void AFACharacter::OnRep_Controller()
{
	Super::OnRep_Controller();

	if (const AFAPlayerState* FAPlayerState = GetPlayerState<AFAPlayerState>())
	{
		HandleTeamChanged(FAPlayerState->GetTeam());
	}

	RefreshInteractionTick();
}

void AFACharacter::UnPossessed()
{
	Super::UnPossessed();

	RefreshInteractionTick();
}

void AFACharacter::RefreshInteractionTick()
{
	if (InteractionComponent)
	{
		InteractionComponent->RefreshTickState();
	}
}

void AFACharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AFAPlayerState* FAPlayerState = TeamNotifySource.Get())
	{
		FAPlayerState->OnTeamChanged.RemoveDynamic(this, &AFACharacter::HandleTeamChanged);
	}
	TeamNotifySource.Reset();

	// bAutoDestroy 가 꺼진 루프라 액터가 사라져도 알아서 멎지 않는다.
	if (GrowlLoopComponent)
	{
		GrowlLoopComponent->Stop();
		GrowlLoopComponent = nullptr;
	}

	UpdateWadeLoop(/*bInWater=*/false);

	// 사다리를 탄 채 사라지면(감염 · 접속 끊김) 등반 상태가 남는다.
	// 사다리는 점유를 쓰지 않으므로 감시 타이머가 대신 치워주지 않는다 — 여기서 직접 놓는다.
	if (HasAuthority() && ClimbingLadder)
	{
		ServerStopClimbing(/*bLaunchOverLedge=*/false);
	}

	if (HasAuthority() && HidingCabinet)
	{
		ServerStopHiding();
	}

	Super::EndPlay(EndPlayReason);
}

void AFACharacter::BindTeamNotify()
{
	// GC 로 파괴 예약된 오브젝트도 그대로 돌려받아야 한다.
	AFAPlayerState* FAPlayerState = GetPlayerState<AFAPlayerState>();
	if (!FAPlayerState || TeamNotifySource.Get(/*bEvenIfGarbage=*/true) == FAPlayerState)
	{
		return;
	}

	if (AFAPlayerState* PreviousState = TeamNotifySource.Get(/*bEvenIfGarbage=*/true))
	{
		PreviousState->OnTeamChanged.RemoveDynamic(this, &AFACharacter::HandleTeamChanged);
	}

	TeamNotifySource = FAPlayerState;
	FAPlayerState->OnTeamChanged.AddDynamic(this, &AFACharacter::HandleTeamChanged);

	// 이미 진영이 정해진 뒤에 물렸을 수 있다 — 지금 상태로 한 번 맞춘다.
	HandleTeamChanged(FAPlayerState->GetTeam());
}

void AFACharacter::HandleTeamChanged(EFATeam NewTeam)
{
	// 감염은 되돌아오지 않는다 — Human -> Zombie 전환이 곧 "감염됐다" 는 신호다.
	if (bHasTeamBaseline && LastKnownTeam == EFATeam::Human && NewTeam == EFATeam::Zombie)
	{
		StartInfectionSequence();
	}

	LastKnownTeam = NewTeam;
	bHasTeamBaseline = true;

	ApplyTeamMaterial(NewTeam);
	UpdateGrowlLoop(NewTeam);
}

void AFACharacter::StartInfectionSequence()
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	InfectionSequenceStartTime = World->GetTimeSeconds();
	bInfectionSequenceActive = true;

	// 첫 프레임부터 화면이 검어야 한다.
	InfectionDarkenAmount = 1.0f;
	InfectionRedAmount = InfectionRedTintStrength;

	if (InfectionGetUpMontage)
	{
		PlayAnimMontage(InfectionGetUpMontage);
	}
}

void AFACharacter::UpdateInfectionSequence()
{
	if (!bInfectionSequenceActive)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const float Elapsed = World->GetTimeSeconds() - InfectionSequenceStartTime;

	// 1단계 — 완전 암전. 빨강은 이미 최대치로 깔아 둔다(밝아질 때 빨강부터 드러나도록).
	if (Elapsed < InfectionBlackoutHoldSeconds)
	{
		InfectionDarkenAmount = 1.0f;
		InfectionRedAmount = InfectionRedTintStrength;
		return;
	}

	// 2단계 — 암전이 풀리고 빨강이 빠진다. 둘은 같은 시점에 시작하지만 길이가 달라서,
	// 화면이 먼저 밝아지고 그 뒤로 붉은 기가 천천히 가신다.
	const float SinceBlackout = Elapsed - InfectionBlackoutHoldSeconds;

	InfectionDarkenAmount = InfectionDarkenReleaseSeconds > 0.0f
		? FMath::Clamp(1.0f - SinceBlackout / InfectionDarkenReleaseSeconds, 0.0f, 1.0f)
		: 0.0f;

	InfectionRedAmount = InfectionRedFadeSeconds > 0.0f
		? FMath::Clamp(1.0f - SinceBlackout / InfectionRedFadeSeconds, 0.0f, 1.0f) * InfectionRedTintStrength
		: 0.0f;

	if (InfectionDarkenAmount <= 0.0f && InfectionRedAmount <= 0.0f)
	{
		bInfectionSequenceActive = false;
	}
}

void AFACharacter::ApplyTeamMaterial(EFATeam NewTeam)
{
	USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!MeshComponent)
	{
		return;
	}

	// 진영 미배정(라운드 시작 전 · 관전)은 인간 외형으로 둔다.
	UMaterialInterface* TargetMaterial = (NewTeam == EFATeam::Zombie) ? ZombieMaterial : HumanMaterial;
	if (!TargetMaterial)
	{
		// 아직 BP 에 안 물린 단계
		return;
	}

	const int32 SlotCount = MeshComponent->GetNumMaterials();
	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		if (MeshComponent->GetMaterial(SlotIndex) != TargetMaterial)
		{
			MeshComponent->SetMaterial(SlotIndex, TargetMaterial);
		}
	}
}

void AFACharacter::UpdateGrowlLoop(EFATeam NewTeam)
{
	// 그르렁은 '인간이 좀비의 위치를 눈치채는' 단서다. 오디오 리스너가 캐릭터 머리에 붙어 있어
	// (생성자 주석 참고) 본인 폰에 걸면 거리 0 · 감쇠 0 으로 라운드 내내 자기 소리만 듣게 된다.
	const bool bWantsGrowl = (NewTeam == EFATeam::Zombie) && !IsLocallyControlled();

	if (!bWantsGrowl)
	{
		if (GrowlLoopComponent)
		{
			GrowlLoopComponent->Stop();
			GrowlLoopComponent = nullptr;
		}
		return;
	}

	// 이미 돌고 있으면 그대로 둔다. 다시 걸면 루프가 처음부터 튄다.
	if (GrowlLoopComponent)
	{
		return;
	}

	if (UFAAudioSubsystem* Audio = UFAAudioSubsystem::Get(this))
	{
		GrowlLoopComponent = Audio->PlaySoundAttached(FACharacterAudio::GrowlZombieLoop, GetMesh());
	}
}

void AFACharacter::UpdateWadeLoop(bool bInWater)
{
	if (!bInWater)
	{
		if (WadeLoopComponent)
		{
			WadeLoopComponent->Stop();
			WadeLoopComponent = nullptr;
		}
		return;
	}

	// 이미 돌고 있으면 그대로 둔다. 다시 걸면 루프가 처음부터 튄다.
	if (WadeLoopComponent)
	{
		return;
	}

	if (UFAAudioSubsystem* Audio = UFAAudioSubsystem::Get(this))
	{
		WadeLoopComponent = Audio->PlaySoundAttached(FACharacterAudio::WaterWadeLoop, GetRootComponent());
	}
}

void AFACharacter::Multicast_PlayFASoundAtLocation_Implementation(FName SoundId, bool bSkipLocalOwner)
{
	if (bSkipLocalOwner && IsLocallyControlled())
	{
		return;
	}

	if (UFAAudioSubsystem* Audio = UFAAudioSubsystem::Get(this))
	{
		Audio->PlaySoundAtLocation(SoundId, GetActorLocation());
	}
}

void AFACharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	UFAAudioSubsystem* Audio = UFAAudioSubsystem::Get(this);
	if (!Audio)
	{
		return;
	}

	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	const EMovementMode NewMode = Movement ? Movement->MovementMode.GetValue() : MOVE_None;

	// 물 안팎이 먼저다. 물로 뛰어들면 Falling -> Swimming 이라, 아래 착지 분기가 먼저 걸리면
	// 첨벙 대신 착지음이 난다.
	const bool bWasSwimming = (PrevMovementMode == MOVE_Swimming);
	const bool bIsSwimming = (NewMode == MOVE_Swimming);

	if (bIsSwimming != bWasSwimming)
	{
		// 물에 들어간 낙하는 착지로 치지 않는다.
		PeakFallSpeed = 0.0f;

		if (bIsSwimming)
		{
			Audio->PlaySoundAtLocation(FACharacterAudio::WaterSplashEnter, GetActorLocation());
		}
		else
		{
			Audio->PlaySoundAtLocation(FACharacterAudio::WaterExit, GetActorLocation());
		}

		UpdateWadeLoop(bIsSwimming);
		return;
	}

	// 땅 -> 공중.
	if (NewMode == MOVE_Falling && PrevMovementMode != MOVE_Falling)
	{
		PeakFallSpeed = 0.0f;

		// MOVE_Falling 은 점프 말고도 턱에서 걸어 내려오거나 경사 마루를 넘을 때도 걸린다.
		//
		// 위로 솟는 속도가 있으면 스스로 뛴 것.
		if (Movement && Movement->Velocity.Z > JumpDetectSpeed)
		{
			Audio->PlaySoundAtLocation(FACharacterAudio::JumpStart, GetActorLocation());
		}
		return;
	}

	// 공중 -> 땅. 얼마나 세게 떨어졌는지는 낙하 중에 재둔 값으로 판정한다.
	if (PrevMovementMode == MOVE_Falling && NewMode != MOVE_Falling)
	{
		const bool bHeavy = PeakFallSpeed >= HeavyLandSpeed;
		PeakFallSpeed = 0.0f;

		Audio->PlaySoundAtLocation(bHeavy ? FACharacterAudio::LandHeavy : FACharacterAudio::JumpLand,
			GetActorLocation());
	}
}

void AFACharacter::InitializeAbilitySystem()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	AbilitySystemComponent->RegisterGameplayTagEvent(
		FAGameplayTags::Get().State_Exposed, EGameplayTagEventType::NewOrRemoved
	).AddUObject(this, &AFACharacter::OnExposedTagChanged);

	AbilitySystemComponent->RegisterGameplayTagEvent(
		FAGameplayTags::Get().Rule_Trade_MutualSight, EGameplayTagEventType::NewOrRemoved
	).AddUObject(this, &AFACharacter::OnMutualSightTagChanged);

	// [H36 뿌리내림] State.Camp 복제 시 각 클라이언트의 아웃라인을 즉시 갱신한다.
	AbilitySystemComponent->RegisterGameplayTagEvent(
		FAGameplayTags::Get().State_Camp, EGameplayTagEventType::NewOrRemoved
	).AddUObject(this, &AFACharacter::OnExposedTagChanged);

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

	// 숨은 채로 진영이 바뀌는 경로(룰 · 디버그 명령).
	// 좀비가 남을 경우 아무에게도 안 보이고 잡히지도 않는 폰이 그대로 게임에 남는다.
	if (NewTeam != EFATeam::Human && HidingCabinet)
	{
		ServerStopHiding();
	}
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

void AFACharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 착지 순간에는 속도가 이미 죽어 있다 — 낙하하는 동안 최고 속도를 재둬야 세게 떨어졌는지 안다.
	// 복제된 Velocity 를 보므로 남의 캐릭터에서도 같은 판정이 나온다.
	if (const UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		if (Movement->MovementMode == MOVE_Falling)
		{
			PeakFallSpeed = FMath::Max(PeakFallSpeed, -Movement->Velocity.Z);
		}
	}

	if (!bIsFirstPerson && !GetWorldTimerManager().IsTimerActive(PerspectiveTimerHandle))
	{
		UpdateCameraCollision(DeltaTime);
	}

	// 포획 상태는 복제로만 들어오므로 이벤트가 없다 — HUD 게이지와 같은 방식으로 폴링한다.
	UpdateCaptureMarker();
	UpdateRuleMarker(DeltaTime);

	UpdateClimbing(DeltaTime);

	// 캐비넷 액터가 파괴된 경우의 복구.
	if (!HidingCabinet && bHideStateApplied)
	{
		ApplyHideState();
	}

	// 시야 효과보다 먼저 계산한다
	UpdateInfectionSequence();

	UpdateVisionEffects();
}

void AFACharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AFACharacter::Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &AFACharacter::Input_MoveReleased);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AFACharacter::Look);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AFACharacter::Input_Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AFACharacter::Input_StopJumping);
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &AFACharacter::ToggleCrouch);
		EnhancedInputComponent->BindAction(PerspectiveAction, ETriggerEvent::Started, this, &AFACharacter::TogglePerspective);
		EnhancedInputComponent->BindAction(PrimaryAction, ETriggerEvent::Triggered, this, &AFACharacter::TriggerPrimary);

		if (!InteractAction)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Interact] InteractAction 이 비어 있다 — BP_FACharacter 의 Class Defaults > Input > Interact Action 에 IA_Interact 를 지정할 것. 상호작용 키가 동작하지 않는다."));
		}

		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &AFACharacter::Input_InteractPressed);
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Completed, this, &AFACharacter::Input_InteractReleased);
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Canceled, this, &AFACharacter::Input_InteractReleased);
	}
}

void AFACharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	CameraBoom->TargetOffset.Z += HalfHeightAdjust;

	// 크라우치는 bIsCrouched 가 복제되면서 모든 머신에서 이 함수가 불리므로 주변 사람에게도 들린다.
	if (UFAAudioSubsystem* Audio = UFAAudioSubsystem::Get(this))
	{
		Audio->PlaySoundAtLocation(FACharacterAudio::CrouchIn, GetActorLocation());
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
		Audio->PlaySoundAtLocation(FACharacterAudio::CrouchOut, GetActorLocation());
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

	// 잡혀 있는 동안의 WASD 는 이동이 아니라 저항이다 — 감염을 취소하지는 못하고 늦출 뿐이다.
	// 이동 입력 자체는 그대로 흘려보낸다. 어차피 State.Grabbed GE 가 속도를 눌러 놓는다.
	if (CaptureState.IsActive() && CaptureState.bIsVictim)
	{
		// [H38 유리 몸] 일반 포획 저항 RPC만 막는다. QTE는 Ability.ContestMash 경로라 영향받지 않는다.
		const bool bNoStruggle = AbilitySystemComponent &&
			(AbilitySystemComponent->HasMatchingGameplayTag(FAGameplayTags::Get().Rule_State_NoStruggle) ||
			 AbilitySystemComponent->HasMatchingGameplayTag(FAGameplayTags::Get().State_NoStruggle));
		if (!bNoStruggle)
		{
			HandleResistInput(MovementVector);
		}
	}

	// 감염 전환 연출 중에는 움직이지 않는다
	// 화면이 다 밝아지는 순간(InfectionDarkenAmount == 0) 조작이 돌아온다.
	if (bInfectionSequenceActive && InfectionDarkenAmount > 0.0f)
	{
		return;
	}

	// 캐비넷 안에서는 움직이지 않는다. 나가는 것은 상호작용 키(F)와 점프 키뿐이다.
	if (IsHiding())
	{
		return;
	}

	// 사다리 위에서는 시선이 아니라 사다리 축이 기준이다 — 카메라를 돌려도 오르내림은 그대로다.
	if (IsClimbing())
	{
		ClimbMove(MovementVector);
		return;
	}

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
		// 설정창의 감도 · Y축 반전. 설정이 없으면 각각 1.0 / false 라 예전과 똑같이 움직인다.
		const float Sensitivity = UFASettingsBridge::GetMouseSensitivity(this);
		float PitchSign = UFASettingsBridge::IsYAxisInverted(this) ? -1.f : 1.f;

		// 상하 반전 룰 (Vision.FlipY). 사용자 설정 반전과는 곱셈으로 합쳐져서,
		// 둘 다 켜져 있으면 서로 상쇄돼 원래대로 보인다 — 의도된 동작이다.
		if (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(FAGameplayTags::Get().Rule_Vision_FlipY))
		{
			PitchSign *= -1.f;
		}

		// 카메라 회전 속도 룰 (H14/Z10 무거운 목 등, SlowTurn 계열).
		const float TurnRateScale = IsValid(AttributeSet) ? AttributeSet->GetTurnRateScale() : 1.f;

		AddControllerYawInput(LookAxisVector.X * Sensitivity * TurnRateScale);
		AddControllerPitchInput(LookAxisVector.Y * Sensitivity * PitchSign);
	}
}

void AFACharacter::Input_Jump()
{
	if (!AbilitySystemComponent) return;

	const FAGameplayTags& FATags = FAGameplayTags::Get();

	// 캐비넷 안에서 점프는 '나가기' 다. F 를 다시 누르는 것과 같은 결과다.
	if (IsHiding())
	{
		Server_RequestStopHiding();
		return;
	}

	// 사다리에 붙어 있으면 점프는 '뛰어내리기' 다. 서버만 상태를 바꿀 수 있으므로 요청만 보낸다.
	if (IsClimbing())
	{
		// 잡힌 채로 뛰어내리면 그게 곧 포획 탈출이 된다 — 저항으로 시간을 버는 것 말고는 못 벗어난다.
		if (!IsLadderMovementLocked())
		{
			Server_RequestStopClimbing(/*bLaunchOverLedge=*/false);
		}
		return;
	}

	// 줄다리기(QTE) 중인 경우 -> 점프 대신 연타 어빌리티
	if (IsInRescueContest())
	{
		// 연타 감각은 누른 본인에게만 필요.
		if (IsLocallyControlled())
		{
			UFAAudioSubsystem::PlayFASound(this, FACharacterAudio::QTEPress);
		}

		AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(FATags.Ability_ContestMash));
		return;
	}

	// 평상시 점프 어빌리티
	const FGameplayTag JumpTag = FATags.Ability_Jump;

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

	const bool bCrouchOnly = AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(
		FAGameplayTags::Get().Rule_Move_CrouchOnly);


	if (bIsCrouched)
	{
		if (bCrouchOnly)
		{
			return;
		}
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

void AFACharacter::UpdateCrouchCamera()
{
	float BaseZ = bIsFirstPerson ? BaseEyeHeight : 0.f;
	CameraBoom->TargetOffset.Z = FMath::FInterpTo(CameraBoom->TargetOffset.Z, BaseZ, GetWorld()->GetDeltaSeconds(), CameraInterpSpeed);

	if (FMath::IsNearlyEqual(CameraBoom->TargetOffset.Z, BaseZ, 1.f))
	{
		CameraBoom->TargetOffset.Z = BaseZ;
		GetWorldTimerManager().ClearTimer(CrouchTimerHandle);
	}
}

void AFACharacter::TogglePerspective()
{
	// Vision.ForceFPS 가 강제한 1인칭 중에는 플레이어의 수동 토글을 막는다.
	if (AbilitySystemComponent &&
		AbilitySystemComponent->HasMatchingGameplayTag(FAGameplayTags::Get().Rule_Vision_ForceFPS))
	{
		return;
	}

	ForceTogglePerspective();
}

void AFACharacter::ForceTogglePerspective()
{
	bIsFirstPerson = !bIsFirstPerson;
	GetWorldTimerManager().ClearTimer(PerspectiveTimerHandle);

	// 캐비넷 안에서는 붐이 캐비넷에 매달려 있다.
	if (IsHiding())
	{
		return;
	}

	if (!GetWorldTimerManager().IsTimerActive(PerspectiveTimerHandle))
	{
		GetWorldTimerManager().SetTimer(PerspectiveTimerHandle, this, &AFACharacter::UpdatePerspectiveCamera, GetWorld()->GetDeltaSeconds(), true);
	}
}

void AFACharacter::SetVisionBlendableActive(UMaterialInstanceDynamic* MID, bool bShouldBeActive) const
{
	if (!MID || !FollowCamera)
	{
		return;
	}

	TArray<FWeightedBlendable>& Blendables = FollowCamera->PostProcessSettings.WeightedBlendables.Array;
	const int32 ExistingIndex = Blendables.IndexOfByPredicate(
		[MID](const FWeightedBlendable& Blendable) { return Blendable.Object == MID; });

	if (bShouldBeActive && ExistingIndex == INDEX_NONE)
	{
		Blendables.Add(FWeightedBlendable(1.0f, MID));
	}
	else if (!bShouldBeActive && ExistingIndex != INDEX_NONE)
	{
		Blendables.RemoveAt(ExistingIndex);
	}
}

void AFACharacter::UpdateVisionEffects()
{
	// 카메라 PostProcess 는 이 카메라를 실제로 보고 있는 클라이언트에서만 의미가 있다.
	// 서버·다른 클라이언트에서 계산해봐야 아무 화면에도 안 나가니 여기서 걸러낸다.
	if (!IsLocallyControlled() || !FollowCamera || !AbilitySystemComponent)
	{
		return;
	}

	const FAGameplayTags& FATags = FAGameplayTags::Get();
	const bool bForceFirstPerson = AbilitySystemComponent->HasMatchingGameplayTag(FATags.Rule_Vision_ForceFPS);
	const bool bDesaturate = AbilitySystemComponent->HasMatchingGameplayTag(FATags.Rule_Vision_Desaturate);
	const bool bBlackout = AbilitySystemComponent->HasMatchingGameplayTag(FATags.Rule_Vision_Blackout);

	// 시야각 축소 (Vision.NarrowFOV) — 카메라 실제 FOV는 더 이상 건드리지 않는다.
	// 대신 화면 테두리를 검게 좁혀 "시야가 좁아진 것"처럼 보이게 하는 별도 PP 머티리얼(M_VisionNarrowFOV)을 쓴다.
	// ※ Vision.Fog 는 VisionFogMID 라는 별도 블렌더블에서 자기 파라미터만 갱신하므로 여기서는 절대 건드리지 않는다.
	float NarrowAmount = 0.0f;
	if (const UFAAttributeSet* Attributes = AbilitySystemComponent->GetSet<UFAAttributeSet>())
	{
		float TargetFOV = Attributes->GetFieldOfView();
		// (예전 실제 카메라 FOV를 줄이던 시절의 "1인칭 강제 시 -10%" 보정은 제거했다.
		// 지금은 이 값이 화면 암전 마스크(NarrowAmount) 계산에만 쓰이는데, ForceFPS만으로도
		// NarrowAmount가 0이 아니게 되어 Vision.NarrowFOV 룰이 없어도 테두리가 옅게 켜지는 부작용이 있었다.
		// NarrowAmount는 이제 순수하게 FieldOfView 어트리뷰트(=Vision.NarrowFOV 룰)에만 반응한다.

		// 기준 FOV 대비 얼마나 좁아졌는지를 0~1 비율로 환산 — 이 비율이 곧 화면 테두리 암전 강도.
		// NarrowAmountIntensity 로 증폭 — GE가 FOV를 조금만 줄여도 마스크는 훨씬 세게 닫히게 만든다.
		const float RawRatio = 1.0f - (TargetFOV / DefaultCameraFOV);
		NarrowAmount = FMath::Clamp(RawRatio * NarrowAmountIntensity, 0.0f, 1.0f);
	}

	if (!VisionNarrowFOVMID && VisionNarrowFOVMaterial && FollowCamera)
	{
		VisionNarrowFOVMID = UMaterialInstanceDynamic::Create(VisionNarrowFOVMaterial, this);
	}

	if (VisionNarrowFOVMID)
	{
		VisionNarrowFOVMID->SetScalarParameterValue(TEXT("NarrowAmount"), NarrowAmount);
	}
	// NarrowAmount == 0 이면 이 MID는 배열에서 완전히 빠진다 — Fog 등 다른 이펙트가
	// PostProcessBlendWeight 를 켜도 얘가 같이 묻어 나올 일이 없다.
	SetVisionBlendableActive(VisionNarrowFOVMID, NarrowAmount > 0.0f);

	// 흑백 (Vision.Desaturate) — Attribute 없이 태그 유무로만 켜고 끈다.
	// 단, 감염 전환 연출이 도는 몇 초 동안은 비켜준다.
	const bool bApplyDesaturate = bDesaturate && InfectionRedAmount <= 0.0f;
	FollowCamera->PostProcessSettings.bOverride_ColorSaturation = bApplyDesaturate;
	if (bApplyDesaturate)
	{
		FollowCamera->PostProcessSettings.ColorSaturation = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
	}

	// 안개 (Vision.Fog) — VisionRange 가 GE 로 낮아진 만큼 거리 기반 포스트프로세스 안개로 흐릿함을 흉내낸다.
	// 개인별 로컬 효과라 실제 볼류메트릭 안개 대신 카메라 PostProcess 로 대체한다.
	float FogAmount = 0.0f;
	if (const UFAAttributeSet* Attributes = AbilitySystemComponent->GetSet<UFAAttributeSet>())
	{
		constexpr float MaxVisionRange = 6000.0f; // AttributeSet 기본값과 맞춤.
		const float CurrentVisionRange = Attributes->GetVisionRange();

		// 0 = 정상 시야, 1 = VisionRange 가 바닥(800uu 근처)까지 떨어진 상태.
		FogAmount = FMath::Clamp(1.0f - (CurrentVisionRange / MaxVisionRange), 0.0f, 1.0f);
	}

	// 거리 기반 안개 — Custom PP 머티리얼(M_VisionFog)에 매 프레임 값 전달.
	if (!VisionFogMID && VisionFogMaterial && FollowCamera)
	{
		VisionFogMID = UMaterialInstanceDynamic::Create(VisionFogMaterial, this);
	}

	if (VisionFogMID)
	{
		const float StartDistance = FMath::Lerp(VisionFogStartDistanceMax, VisionFogStartDistanceMin, FogAmount);
		const float FullDistance  = FMath::Lerp(VisionFogFullDistanceMax, VisionFogFullDistanceMin, FogAmount);

		VisionFogMID->SetScalarParameterValue(TEXT("FogStartDistance"), StartDistance);
		VisionFogMID->SetScalarParameterValue(TEXT("FogFullDistance"), FullDistance);
		VisionFogMID->SetScalarParameterValue(TEXT("FogAmount"), FogAmount);

		// FogAmount == 0 이면 이 MID는 배열에서 완전히 빠진다 — Start/Full 값이 뭐든(예: 1500/1500처럼
		// 서로 같아 컷오프가 생기는 값이어도) NarrowFOV 등 다른 이펙트 때문에 같이 렌더링되지 않는다.
		SetVisionBlendableActive(VisionFogMID, FogAmount > 0.0f);

		// FullDistance 보다 멀리 있는 상대는 로컬 화면에서 아예 안 보이게 컬링한다.
		// (숨바꼭질 특성상 "멀어지면 형체도 안 보임" 이 목표라 안개만으론 부족 — 안개는 흐릿하게만 만든다)
		// Fog가 꺼져 있을 때(FogAmount==0)는 컬링도 같이 꺼야, NarrowFOV만 걸렸을 때 상대가 사라지는 부작용이 없다.
		// 주의: 0을 넘기면 FullDistanceSq가 0이 되어 오히려 전원이 컬링되는 정반대 버그가 나므로,
		// "컬링 안 함"은 반드시 아주 큰 거리값으로 표현한다.
		constexpr float NoCullingDistance = 1000000.0f; // 어떤 맵보다도 확실히 큰 값.
		UpdateOpponentDistanceCulling(FogAmount > 0.0f ? FullDistance : NoCullingDistance);
	}

	// 명멸/암전 (Vision.Blackout) — GE 는 태그만 부여. 주기적인 깜빡임은 여기서 직접 계산한다.
	// 5초 주기로 1초간 완전 암전.
	float BlackoutAmount = 0.0f;
	if (bBlackout && GetWorld())
	{
		constexpr float BlackoutPeriod = 5.0f;
		constexpr float BlackoutFlashDuration = 1.0f;

		const float TimeInCycle = FMath::Fmod(GetWorld()->GetTimeSeconds(), BlackoutPeriod);
		BlackoutAmount = (TimeInCycle < BlackoutFlashDuration) ? 1.0f : 0.0f;
	}

	// 화면을 어둡게 만드는 세 효과(안개 · 명멸 룰 · 감염 연출)를 ColorGain 하나로 합쳐서 적용
	const float DarkenAmount = FMath::Max3(FogAmount * 0.25f, BlackoutAmount, InfectionDarkenAmount);

	// ColorGain 은 XYZ = RGB 배수, W = 전체 밝기다. 감염 연출의 빨강은 초록·파랑을 눌러서 만들고 암전은 W 로
	FollowCamera->PostProcessSettings.bOverride_SceneColorTint = false;
	FollowCamera->PostProcessSettings.bOverride_ColorGain = DarkenAmount > 0.0f || InfectionRedAmount > 0.0f;
	FollowCamera->PostProcessSettings.ColorGain = FVector4(
		1.0f,
		1.0f - InfectionRedAmount,
		1.0f - InfectionRedAmount,
		1.0f - DarkenAmount);

	// 1인칭 강제 (Vision.ForceFPS)
	if (bForceFirstPerson && !bIsFirstPerson)
	{
		bVisionForcedFirstPerson = true;
		ForceTogglePerspective();
	}
	else if (!bForceFirstPerson && bVisionForcedFirstPerson)
	{
		bVisionForcedFirstPerson = false;
		if (bIsFirstPerson)
		{
			ForceTogglePerspective();
		}
	}

	// PostProcess 전체를 켤지는 맨 마지막에 한 번만 판단 — 위 효과(흑백·안개·암전·시야축소·감염 연출)
	// 중 하나라도 켜져 있으면 블렌드. 감염 연출을 빠뜨리면 값만 넣고 화면에는 아무것도 안 나온다.
	const bool bAnyVisualEffectActive = bApplyDesaturate || FogAmount > 0.0f || BlackoutAmount > 0.0f
		|| NarrowAmount > 0.0f || InfectionDarkenAmount > 0.0f || InfectionRedAmount > 0.0f;
	FollowCamera->PostProcessBlendWeight = bAnyVisualEffectActive ? 1.0f : 0.0f;
}

void AFACharacter::UpdateOpponentDistanceCulling(float FullDistance)
{
	// 너무 잦은 전체 순회는 비용이 크므로 일정 주기로만 수행한다.
	TimeSinceLastOpponentCullCheck += GetWorld()->GetDeltaSeconds();
	if (TimeSinceLastOpponentCullCheck < OpponentCullCheckInterval)
	{
		return;
	}
	TimeSinceLastOpponentCullCheck = 0.0f;

	const FVector MyLocation = GetActorLocation();
	const float FullDistanceSq = FMath::Square(FullDistance);

	for (TActorIterator<AFACharacter> It(GetWorld()); It; ++It)
	{
		AFACharacter* Other = *It;
		if (!IsValid(Other) || Other == this || !Other->GetMesh())
		{
			continue;
		}

		// 캐비넷 등 다른 로직이 이미 액터 전체를 숨긴 상태라면 여기서 건드리지 않는다.
		if (Other->bHideStateApplied)
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(MyLocation, Other->GetActorLocation());
		const bool bShouldHide = DistSq > FullDistanceSq;

		// 로컬(내 화면)에서만 적용 — 서버·다른 클라이언트 화면엔 영향 없다.
		Other->GetMesh()->SetHiddenInGame(bShouldHide);
	}
}

void AFACharacter::UpdatePerspectiveCamera()
{
	float DeltaTime = GetWorld()->GetDeltaSeconds();
	float TargetArmLength = bIsFirstPerson ? 0.f : ThirdPersonArmLength;
	float TargetOffsetZ = bIsFirstPerson ? BaseEyeHeight : 0.f;

	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetArmLength, DeltaTime, CameraInterpSpeed);
	CameraBoom->TargetOffset.Z = FMath::FInterpTo(CameraBoom->TargetOffset.Z, TargetOffsetZ, DeltaTime, CameraInterpSpeed);

	GetMesh()->SetOwnerNoSee(CameraBoom->TargetArmLength < 40.f);

	if (FMath::IsNearlyEqual(CameraBoom->TargetArmLength, TargetArmLength, 1.f))
	{
		CameraBoom->TargetArmLength = TargetArmLength;
		CameraBoom->TargetOffset.Z = TargetOffsetZ;
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
	const FAGameplayTags& FATags = FAGameplayTags::Get();

	// 행동 불가 상태 검사 (QTE 중이거나 기절 상태면 무시)
	if (IsInRescueContest() || AbilitySystemComponent->HasMatchingGameplayTag(FATags.State_Stunned))
		return;

	if (AbilitySystemComponent->HasMatchingGameplayTag(FATags.State_Zombie))
	{
		AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(FATags.Ability_Grab));
	}
	else if (AbilitySystemComponent->HasMatchingGameplayTag(FATags.State_Human))
	{
		AbilitySystemComponent->TryActivateAbilitiesByTag(FGameplayTagContainer(FATags.Ability_Rescue));
	}
}

//~ ---------------- 상호작용 ----------------

void AFACharacter::Input_InteractPressed()
{
	if (InteractionComponent)
	{
		InteractionComponent->OnInteractPressed();
	}
}

void AFACharacter::Input_InteractReleased()
{
	if (InteractionComponent)
	{
		InteractionComponent->OnInteractReleased();
	}
}

//~ ---------------- 사다리 등반 ----------------

void AFACharacter::ServerStartClimbing(AFALadder* Ladder)
{
	if (!HasAuthority() || !Ladder || ClimbingLadder == Ladder)
	{
		return;
	}

	// 다른 사다리를 타고 있었다면 먼저 정리한다(점유도 같이 풀린다).
	if (ClimbingLadder)
	{
		ServerStopClimbing(/*bLaunchOverLedge=*/false);
	}

	ClimbingLadder = Ladder;
	ClimbStartTime = GetServerWorldTime();

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AddLooseGameplayTag(FAGameplayTags::Get().State_Climbing);
	}

	// 서버는 OnRep 이 안 불리므로 같은 경로를 직접 탄다.
	ApplyClimbState();
}

void AFACharacter::ServerStopClimbing(bool bLaunchOverLedge)
{
	if (!HasAuthority() || !ClimbingLadder)
	{
		return;
	}

	AFALadder* Ladder = ClimbingLadder;
	ClimbingLadder = nullptr;

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(FAGameplayTags::Get().State_Climbing);
	}

	ApplyClimbState();

	// ★ 점유 해제를 빼먹으면 그 사다리는 아무도 못 쓰게 된다.
	if (IsValid(Ladder))
	{
		IFAInteractable::Execute_ReleaseInteraction(Ladder, this);

		if (bLaunchOverLedge)
		{
			// 난간 위로 올려놓는다. 속도를 주는 방식은 난간 높이·각도에 따라 실패해서
			// TopExit 마커 자리로 그냥 옮긴다 — 어디로 내려서는지가 배치로 확정된다.
			SetActorLocation(Ladder->GetTopExitLocation(), /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
}

void AFACharacter::Server_RequestStopClimbing_Implementation(bool bLaunchOverLedge)
{
	// 클라이언트가 보내는 유일한 하차 경로다.
	if (IsLadderMovementLocked())
	{
		return;
	}

	ServerStopClimbing(bLaunchOverLedge);
}

void AFACharacter::OnRep_ClimbingLadder()
{
	ApplyClimbState();
}

void AFACharacter::ApplyClimbState()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	// 저장 플래그가 곧 '직전에 타고 있었는가' 다. 이 함수는 서버·OnRep·사다리 소멸 복구
	// 세 경로로 들어오므로, 소리는 상태가 실제로 뒤집힌 순간에만 내야 한다.
	const bool bWasClimbing = bHasSavedClimbMovementSettings;

	if (ClimbingLadder)
	{
		// 원래 값은 한 번만 저장한다 — 등반 중에 다시 저장하면 등반용 값이 '원래 값' 이 돼버린다.
		if (!bHasSavedClimbMovementSettings)
		{
			SavedMaxFlySpeed = Movement->MaxFlySpeed;
			SavedBrakingDecelerationFlying = Movement->BrakingDecelerationFlying;
			bSavedOrientRotationToMovement = Movement->bOrientRotationToMovement;
			bHasSavedClimbMovementSettings = true;
		}

		if (!bWasClimbing)
		{
			ClimbStepAccumulator = 0.0f;

			if (UFAAudioSubsystem* Audio = UFAAudioSubsystem::Get(this))
			{
				Audio->PlaySoundAtLocation(FACharacterAudio::LadderMount, GetActorLocation());
			}
		}

		Movement->MaxFlySpeed = ClimbingLadder->ClimbSpeed;
		// Flying 의 제동 기본값은 0 이다. 그대로 두면 키를 놓아도 사다리를 계속 타고 올라간다.
		Movement->BrakingDecelerationFlying = ClimbingLadder->ClimbSpeed * 8.0f;
		// 등반 중에는 사다리를 마주 본 채로 고정한다.
		Movement->bOrientRotationToMovement = false;

		Movement->StopMovementImmediately();
		Movement->SetMovementMode(MOVE_Flying);

		// 남의 화면(시뮬레이티드 프록시)에서는 스냅하지 않는다 — 복제된 위치와 싸운다.
		if (HasAuthority() || IsLocallyControlled())
		{
			SetActorLocation(ClimbingLadder->GetAttachLocation(GetActorLocation()),
				/*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
			SetActorRotation(ClimbingLadder->GetClimbFacingRotation());
		}
	}
	else
	{
		if (bWasClimbing)
		{
			if (UFAAudioSubsystem* Audio = UFAAudioSubsystem::Get(this))
			{
				Audio->PlaySoundAtLocation(FACharacterAudio::LadderDismount, GetActorLocation());
			}
		}

		if (bHasSavedClimbMovementSettings)
		{
			Movement->MaxFlySpeed = SavedMaxFlySpeed;
			Movement->BrakingDecelerationFlying = SavedBrakingDecelerationFlying;
			Movement->bOrientRotationToMovement = bSavedOrientRotationToMovement;
			bHasSavedClimbMovementSettings = false;
		}

		if (Movement->MovementMode == MOVE_Flying)
		{
			Movement->SetMovementMode(MOVE_Falling);
		}
	}
}

bool AFACharacter::IsLadderMovementLocked() const
{
	// 포획은 좀비·인질 양쪽에 CaptureState 를 켜므로 한 번의 검사로 둘 다 묶인다.
	return CaptureState.IsActive() || ContestState.IsActive();
}

void AFACharacter::ClimbMove(const FVector2D& MovementVector)
{
	if (!ClimbingLadder)
	{
		return;
	}

	// 잡힌 채로 계속 오를 수 있으면 인질이 거리를 벌려 포획을 스스로 푼다.
	// 이때의 WASD 는 Move() 앞단에서 이미 저항 입력으로 소비된 뒤다.
	if (IsLadderMovementLocked())
	{
		return;
	}

	// 좌우 입력은 버린다. 사다리 위에서 옆으로 가는 동작은 없다.
	AddMovementInput(ClimbingLadder->GetClimbUpVector(), MovementVector.Y);
}

void AFACharacter::UpdateClimbing(float DeltaTime)
{
	if (!ClimbingLadder)
	{
		// 사다리 액터가 파괴되면 이 참조는 null 로 지워지지만 OnRep 은 오지 않는다.
		// 되돌리지 않으면 Flying 인 채로 공중에 떠버린다.
		if (bHasSavedClimbMovementSettings)
		{
			ApplyClimbState();
		}
		return;
	}

	const FVector Up = ClimbingLadder->GetClimbUpVector();

	// 중심선 유지 — 등반축 성분은 그대로 두고 옆으로 벌어진 만큼만 되돌린다.
	if (HasAuthority() || IsLocallyControlled())
	{
		const FVector Current = GetActorLocation();
		FVector Lateral = ClimbingLadder->GetAttachLocation(Current) - Current;
		Lateral -= Up * FVector::DotProduct(Lateral, Up);

		if (!Lateral.IsNearlyZero(0.5f))
		{
			SetActorLocation(Current + Lateral * FMath::Clamp(DeltaTime * FAClimb::LateralSnapSpeed, 0.0f, 1.0f),
				/*bSweep=*/false);
		}
	}

	// 등반 애니메이션이 없어 노티파이를 찍을 데가 없다. 등반축으로 실제 움직인 거리를 누산해
	// 한 칸마다 낸다 — 속도가 룰로 눌려도 간격이 거리에 맞춰 저절로 벌어진다.
	// 권한 분기보다 위에 있어야 남의 화면에서도 들린다(속도는 복제된다).
	ClimbStepAccumulator += FMath::Abs(FVector::DotProduct(GetVelocity(), Up)) * DeltaTime;
	if (ClimbStepDistance > 0.0f && ClimbStepAccumulator >= ClimbStepDistance)
	{
		ClimbStepAccumulator -= ClimbStepDistance;

		if (UFAAudioSubsystem* Audio = UFAAudioSubsystem::Get(this))
		{
			Audio->PlaySoundAtLocation(FACharacterAudio::LadderStep, GetActorLocation());
		}
	}

	if (!HasAuthority())
	{
		return;
	}

	// 붙은 직후에는 판정하지 않는다 — 바닥에서 탄 사람이 그 프레임에 바로 튕겨 내려온다.
	if (GetServerWorldTime() - ClimbStartTime < FAClimb::ExitGraceSeconds)
	{
		return;
	}

	const float Alpha = ClimbingLadder->GetClimbAlpha(GetActorLocation());

	if (Alpha >= 1.0f)
	{
		// 꼭대기 — 난간 위로 올라선다.
		ServerStopClimbing(/*bLaunchOverLedge=*/true);
		return;
	}

	// 바닥 — 내려오는 중일 때만 발을 뗀다. 속도 조건이 없으면 탄 자리에서 바로 풀린다.
	if (Alpha <= 0.0f && FVector::DotProduct(GetVelocity(), Up) < -1.0f)
	{
		ServerStopClimbing(/*bLaunchOverLedge=*/false);
	}
}

//~ ---------------- 캐비넷 숨기 ----------------

void AFACharacter::ServerStartHiding(AFACabinet* Cabinet)
{
	if (!HasAuthority() || !Cabinet || HidingCabinet == Cabinet)
	{
		return;
	}

	// 사다리에 붙은 채로 캐비넷에 들어갈 수는 없다. 다른 캐비넷에 있었다면 그쪽을 먼저 비운다.
	if (ClimbingLadder)
	{
		ServerStopClimbing(/*bLaunchOverLedge=*/false);
	}
	if (HidingCabinet)
	{
		ServerStopHiding();
	}

	HidingCabinet = Cabinet;

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AddLooseGameplayTag(FAGameplayTags::Get().State_Hiding);
	}

	// 서버는 OnRep 이 안 불리므로 같은 경로를 직접 탄다.
	ApplyHideState();
}

void AFACharacter::ServerStopHiding()
{
	if (!HasAuthority() || !HidingCabinet)
	{
		return;
	}

	AFACabinet* Cabinet = HidingCabinet;
	HidingCabinet = nullptr;

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(FAGameplayTags::Get().State_Hiding);
	}

	// 위치를 먼저 옮기고 나서 충돌을 되살린다. 캐비넷 메시 안에서 켜면 낀 채로 밀려난다.
	if (IsValid(Cabinet))
	{
		SetActorLocation(Cabinet->GetExitLocation(), /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
		SetActorRotation(Cabinet->GetExitRotation());
	}

	ApplyHideState();

	if (IsValid(Cabinet))
	{
		IFAInteractable::Execute_ReleaseInteraction(Cabinet, this);
	}
}

void AFACharacter::Server_RequestStopHiding_Implementation()
{
	ServerStopHiding();
}

void AFACharacter::OnRep_HidingCabinet()
{
	ApplyHideState();
}

void AFACharacter::ApplyHideState()
{
	const bool bHiding = (HidingCabinet != nullptr);

	// 소리는 '바뀐 순간' 에만 낸다 — 이 함수는 서버·OnRep·복구 틱 세 경로로 들어온다.
	// 캐비넷 문 소리(Cabinet_Open/Close)는 AFADoor 가 따로 내므로 여기서는 사람 쪽만 낸다.
	if (bHiding != bHideStateApplied)
	{
		if (UFAAudioSubsystem* Audio = UFAAudioSubsystem::Get(this))
		{
			// 나올 때는 HidingCabinet 이 이미 비어 있다. 그 시점의 액터 위치가 곧 캐비넷 앞이다.
			const FVector SoundLocation = HidingCabinet ? HidingCabinet->GetActorLocation() : GetActorLocation();
			Audio->PlaySoundAtLocation(bHiding ? FACharacterAudio::HideEnter : FACharacterAudio::HideExit,
				SoundLocation);
		}
	}

	bHideStateApplied = bHiding;

	// 메시와 머리 위 위젯 3종(감염 게이지 · 룰 아이콘 · 발화 아이콘)이 액터 숨김 하나로 같이 사라진다.
	SetActorHiddenInGame(bHiding);
	// 문이 열리면 되살아난다.
	SetActorEnableCollision(!bHiding);

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		if (bHiding)
		{
			Movement->StopMovementImmediately();
			Movement->DisableMovement();
		}
		else if (Movement->MovementMode == MOVE_None)
		{
			// 나온 자리가 공중일 수 있으므로 Walking 이 아니라 Falling 으로 되돌린다.
			Movement->SetMovementMode(MOVE_Falling);
		}
	}

	// 카메라는 자기 화면에만 있는 것이라 로컬에서만
	if (IsLocallyControlled())
	{
		ApplyHideCamera(bHiding);
	}

	if (!bHiding)
	{
		return;
	}

	// 남의 화면(시뮬레이티드 프록시)에서는 스냅하지 않는다
	if (HasAuthority() || IsLocallyControlled())
	{
		SetActorLocation(HidingCabinet->GetHideLocation(), /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
		SetActorRotation(HidingCabinet->GetHideRotation());
	}

	// 들어간 순간에는 밖을 마주 보게 해준다. 컨트롤 회전은 각자의 머신에만 있는 값이라
	// 서버가 남의 것을 대신 돌려줄 수 없다.
	if (IsLocallyControlled())
	{
		if (AController* OwningController = GetController())
		{
			OwningController->SetControlRotation(HidingCabinet->GetHideRotation());
		}
	}
}

void AFACharacter::ApplyHideCamera(bool bHiding)
{
	if (!CameraBoom)
	{
		return;
	}

	if (bHiding)
	{
		USceneComponent* Anchor = HidingCabinet ? HidingCabinet->GetHideCameraAnchor() : nullptr;
		if (!Anchor || bHideCameraApplied)
		{
			return;
		}

		SavedBoomRelativeTransform = CameraBoom->GetRelativeTransform();
		bHideCameraApplied = true;

		// 보간 타이머를 먼저 끈다. 켜둔 채로 옮기면 붐 길이와 오프셋을 도로 끌고 간다.
		GetWorldTimerManager().ClearTimer(PerspectiveTimerHandle);
		GetWorldTimerManager().ClearTimer(CrouchTimerHandle);

		CameraBoom->AttachToComponent(Anchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

		// 붐을 접어 카메라를 캐비넷 위에 그대로 놓는다. 3인칭 거리를 남겨두면
		// 벽에 등을 붙인 캐비넷에서는 카메라가 벽 안으로 들어간다.
		CameraBoom->TargetArmLength = 0.0f;
		CameraBoom->SocketOffset = FVector::ZeroVector;
		CameraBoom->TargetOffset = FVector::ZeroVector;
		return;
	}

	if (!bHideCameraApplied)
	{
		return;
	}

	bHideCameraApplied = false;

	CameraBoom->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	CameraBoom->SetRelativeTransform(SavedBoomRelativeTransform);

	// 숨어 있는 동안 시점 토글이나 Vision.ForceFPS 로 bIsFirstPerson 이 바뀌었을 수 있다.
	// 저장해둔 값이 아니라 지금 상태가 요구하는 값으로 되돌린다.
	CameraBoom->TargetArmLength = bIsFirstPerson ? 0.0f : ThirdPersonArmLength;
	CameraBoom->SocketOffset = FVector::ZeroVector;
	CameraBoom->TargetOffset = FVector(0.0f, 0.0f, bIsFirstPerson ? BaseEyeHeight : 0.0f);

	GetMesh()->SetOwnerNoSee(CameraBoom->TargetArmLength < 40.0f);
}
