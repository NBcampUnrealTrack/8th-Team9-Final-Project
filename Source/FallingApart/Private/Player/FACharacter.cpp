// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/FACharacter.h"
#include "Audio/FAAudioSubsystem.h"
#include "AttributeSet/FAAttributeSet.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "GAS/FAAbilitySystemComponent.h"
#include "GAS/FAGameplayAbility.h"
#include "GAS/FAGameplayTags.h"
#include "Animation/FAFirstPersonAnimInstance.h"
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
#include "Audio/FAPhantomAudioComponent.h"
#include "Settings/FASettingsBridge.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "UI/FAOverHeadWidget.h"
#include "Player/FADetectionUnitComponent.h"
#include "Player/FATrailComponent.h"

namespace FACharacterAudio
{
	// 표의 RowName 과 같아야 한다 (DT_Sound_Character).
	static const FName CrouchIn(TEXT("Crouch_In"));
	static const FName CrouchOut(TEXT("Crouch_Out"));
	static const FName JumpStart(TEXT("Jump_Start"));
	static const FName JumpNoisy(TEXT("Jump_Noisy")); // H13 삐걱대는 무릎
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

	FirstPersonArms = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonArms"));
	FirstPersonArms->SetupAttachment(GetMesh());
	FirstPersonArms->SetOnlyOwnerSee(true);
	FirstPersonArms->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonArms->SetGenerateOverlapEvents(false);
	FirstPersonArms->SetCastShadow(false);
	FirstPersonArms->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	FirstPersonArms->SetVisibility(false);
	FirstPersonArms->PrimaryComponentTick.bStartWithTickEnabled = false;
	FirstPersonArms->PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	FirstPersonArms->PrimaryComponentTick.EndTickGroup = TG_PostUpdateWork;

	// 3인칭이어도 오디오 리스너는 카메라가 아니라 캐릭터 헤드에 붙는다(기획 확정).
	// 카메라/스프링암 쪽에서 SetAudioListenerOverride 를 호출하면 이 동작이 깨진다.
	VoiceComponent = CreateDefaultSubobject<UFAVoiceComponent>(TEXT("VoiceComponent"));

	// 상호작용 탐지. 로컬 컨트롤 캐릭터에서만 틱이 돈다(컴포넌트가 BeginPlay 에서 스스로 끈다).
	InteractionComponent = CreateDefaultSubobject<UFAInteractionComponent>(TEXT("InteractionComponent"));

	AbilitySystemComponent = CreateDefaultSubobject<UFAAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AttributeSet = CreateDefaultSubobject<UFAAttributeSet>(TEXT("AttributeSet"));

	CharacterStatusComponent = CreateDefaultSubobject<UFACharacterStatusComponent>(TEXT("CharacterStatusComponent"));
	PhantomAudioComponent = CreateDefaultSubobject<UFAPhantomAudioComponent>(TEXT("PhantomAudioComponent"));
	TrailComponent = CreateDefaultSubobject<UFATrailComponent>(TEXT("TrailComponent"));

	DetectionUnitComponent =CreateDefaultSubobject<UFADetectionUnitComponent>(TEXT("DetectionUnitComponent"));
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

bool AFACharacter::ShouldHideOverheadUI() const
{
	if (!AbilitySystemComponent)
	{
		return false;
	}

	const FAGameplayTags& GameplayTags = FAGameplayTags::Get();
	// H45 발동과 Z44가 공유하는 실제 반투명 태그를 확인한다.
	return AbilitySystemComponent->HasMatchingGameplayTag(GameplayTags.Rule_Trade_Ghost);
}

bool AFACharacter::ShouldShowCaptureMarker() const
{
	if (ShouldHideOverheadUI())
	{
		return false;
	}

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
void AFACharacter::SetupNicknameMarker()
{
	if (OverHeadWidgetClass == nullptr || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	NicknameMarker = NewObject<UWidgetComponent>(this);
	NicknameMarker->SetWidgetSpace(EWidgetSpace::Screen);
	NicknameMarker->SetDrawAtDesiredSize(false);
	NicknameMarker->SetDrawSize(RuleMarkerDrawSize);
	NicknameMarker->SetWidgetClass(OverHeadWidgetClass);
	NicknameMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NicknameMarker->RegisterComponent();
	NicknameMarker->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	NicknameMarker->SetRelativeLocation(NicknameMarkerOffset);
	NicknameMarker->SetVisibility(true);
	NicknameMarker->InitWidget();
	RefreshNicknameMarker();
}

void AFACharacter::RefreshNicknameMarker()
{
	if (!NicknameMarker)
	{
		return;
	}

	// PlayerState는 Pawn보다 늦게 복제될 수 있다. 같은 캐릭터를 다시 주입하면 이름도 함께 갱신된다.
	NicknameMarker->InitWidget();
	if (UFAOverHeadWidget* OverHeadWidget = Cast<UFAOverHeadWidget>(NicknameMarker->GetUserWidgetObject()))
	{
		OverHeadWidget->SetTargetCharacter(this);
	}
}
bool AFACharacter::ShouldShowRuleMarker() const
{
	if (ShouldHideOverheadUI())
	{
		return false;
	}

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

	bool bShouldReveal = bHeartbeatRevealActive; // H41: 로컬 좀비에게 보유자 위치 점멸
	bool bSilhouetteOnly = false; // [H23 투명한 적] 본체는 숨기고 아웃라인만 남긴다.
	const FAGameplayTags& FATags = FAGameplayTags::Get();

	const UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	AFACharacter* LocalCharacter = PC ? Cast<AFACharacter>(PC->GetPawn()) : nullptr;

	// [H36 뿌리내림] 기존 상시 노출과 같은 X-Ray 아웃라인 경로를 사용한다.
	if (AbilitySystemComponent->HasMatchingGameplayTag(FATags.State_Exposed) ||
		AbilitySystemComponent->HasMatchingGameplayTag(FATags.State_Camp))
	{
		bShouldReveal = true;
	}
	else if (LocalCharacter && LocalCharacter != this && LocalCharacter->GetFAAbilitySystemComponent())
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

	// [H23 투명한 적] 로컬 뷰어가 NoZombie 룰을 갖고 있으면 좀비·인간 구분 없이 모든 상대를 실루엣만 보이게 한다.
	// 좀비한테만 적용하면 실루엣 형태 자체가 "저게 좀비다"라는 정보가 되어버려서 전원 대상으로 바꿨다(팀 결정).
	// 위 노출 판정과 별개로 항상 검사한다.
	if (LocalCharacter && LocalCharacter != this && LocalCharacter->GetFAAbilitySystemComponent() &&
		LocalCharacter->GetFAAbilitySystemComponent()->HasMatchingGameplayTag(FATags.Rule_Vision_NoZombie))
	{
		bSilhouetteOnly = true;
		bShouldReveal = true; // 실루엣을 그리려면 아웃라인 렌더 자체는 켜져 있어야 한다.
	}

	CharacterMesh->SetRenderCustomDepth(bShouldReveal);
	CharacterMesh->SetCustomDepthStencilValue(bShouldReveal ? 1 : 0);
	// SetRenderInMainPass 도 렌더 패스만 뺀다 — 콜리전(캡슐 블로킹·메시 오버랩)은 그대로다.
	// 실루엣만 보여도 부딪히고 잡히는 건 정상이어야 한다.
	CharacterMesh->SetRenderInMainPass(!bSilhouetteOnly);
}

bool AFACharacter::ShouldShowOverheadWidget()
{
	//참고 : Local~ : 플레이어의 소유 / this, GetActor~ : 대상 캐릭터
	
	// 이름표, 이름 상단의 위젯이 보여야하는지 조건 체크.
	// 가벼운 조건부터 체크하여, 무거운 라인트레이스를 마지막에 체크함.

	//로컬에서 자신의 위젯을 보일 필요가 없음.
	if (IsLocallyControlled() == true)
	{
		bRuleMarkerOccluded = true;
		return false;
	}
	
	
	const UWorld* World = GetWorld();
	const APlayerController* LocalController = World ? World->GetFirstPlayerController() : nullptr;

	//이름이 출력되는 위치, 캐릭터의 Capsule HalfHeight+10
	FVector NameLocation = GetActorLocation() + FVector(0.0f, 0.0f, 80.0f);
	//발 위치. 캐릭터의 Capsule HalfHeight * -1
	FVector FootLocation = GetActorLocation() + FVector(0.0f, 0.0f, -70.0f);
	if (LocalController)
	{
		AFACharacter* LocalCharacter = Cast<AFACharacter>(LocalController->GetCharacter());
		
		//특수 룰카드 존재 체크
		if (LocalCharacter 
			&& LocalCharacter->GetAbilitySystemComponent() )
		{
			//H45 은둔의 반투명 상태 / Z44 유령의 반투명 상태. 
			if (this->GetAbilitySystemComponent()->HasMatchingGameplayTag(FAGameplayTags::Get().State_Hidden)
				|| this->GetAbilitySystemComponent()->HasMatchingGameplayTag(FAGameplayTags::Get().Rule_Trade_Ghost))
			{
				if (this->LastKnownTeam != LocalCharacter->LastKnownTeam)
				{
					bRuleMarkerOccluded = true;
					return false;
				}
			}
			
			//H24 고독 / Z37 고립, 같은 팀원이 보이지 않음.
			if (LocalCharacter->GetAbilitySystemComponent()->HasMatchingGameplayTag(FAGameplayTags::Get().Rule_Vision_NoAlly))
			{
				if (this->LastKnownTeam == LocalCharacter->LastKnownTeam)
				{
					bRuleMarkerOccluded = true;
					return false;
				}
			}
			
			//H23 투명한 적. 내가 생존자일때, 좀비가 보이지 않음.
			if (LocalCharacter->GetAbilitySystemComponent()->HasMatchingGameplayTag(FAGameplayTags::Get().Rule_Vision_NoZombie))
			{
				if (this->LastKnownTeam == EFATeam::Zombie 
					&& LocalCharacter->LastKnownTeam == EFATeam::Human)
				{
					bRuleMarkerOccluded = true;
					return false;
				}
			}
			
		}
		
		// 거리 제한, 룰마커와 동일한 거리값을 임시로 사용.
		FVector ViewLocation;
		FRotator ViewRotation;
		LocalController->GetPlayerViewPoint(ViewLocation, ViewRotation);

		//Z18 안개 / H19 안개 보이는 거리 제한.
		float CullDistance = OverheadWidgetCullDistance;
		if (LocalCharacter && LocalCharacter->GetAbilitySystemComponent() &&
			LocalCharacter->GetAbilitySystemComponent()->HasMatchingGameplayTag(FAGameplayTags::Get().Rule_Vision_Fog))
		{
			//시야 제한 거리를 가져올 데이터가 있다면 여기 변경.
			CullDistance = 800.f;
		}

		if (FVector::DistSquared(ViewLocation, NameLocation) > FMath::Square(CullDistance))
		{
			bRuleMarkerOccluded = true;
			return false;
		}

		//이름표가 보일 시야각의 제한, 기본은 90
		float PlayerFOV = 90.f;
		if (LocalCharacter && LocalCharacter->GetAbilitySystemComponent())
		{
			if (LocalCharacter->GetAbilitySystemComponent()->HasMatchingGameplayTag(FAGameplayTags::Get().Rule_Vision_NarrowFOV))
			{
				PlayerFOV -= 35.f;
			}
			if (LocalCharacter->GetAbilitySystemComponent()->HasMatchingGameplayTag(FAGameplayTags::Get().Rule_Vision_ForceFPS))
			{
				PlayerFOV -= 10.f;
			}
		}
		
		const FVector ToTarget = (NameLocation - ViewLocation).GetSafeNormal();
		const FVector CameraForward = ViewRotation.Vector();
		if (FVector::DotProduct(CameraForward, ToTarget) 
			< FMath::Cos(FMath::DegreesToRadians(PlayerFOV/2)))
		{
			bRuleMarkerOccluded = true;
			return false;
		}

		//화면내 영역에서만 처리하도록 위치가 Viewport내에 존재하는지 확인. 사람마다 화면 비율이 다를수 있으므로, 시야각과 같이 더블체크함.
		FVector2D ScreenPosition;
		if (!LocalController->ProjectWorldLocationToScreen(
				NameLocation, ScreenPosition, false))
		{
			bRuleMarkerOccluded = true;
			return false;
		}

		int32 ViewportX;
		int32 ViewportY;
		LocalController->GetViewportSize(ViewportX, ViewportY);

		//가장자리 비율을 제외함.
		const float HorizontalMargin = ViewportX * 0.10f;
		const float VerticalMargin   = ViewportY * 0.12f;

		const bool bInsideSafeArea =
			ScreenPosition.X >= HorizontalMargin &&
			ScreenPosition.X <= ViewportX - HorizontalMargin &&
			ScreenPosition.Y >= VerticalMargin &&
			ScreenPosition.Y <= ViewportY - VerticalMargin;

		if (!bInsideSafeArea)
		{
			bRuleMarkerOccluded = true;
			return false;
		}

		//계산이 무거운 라인트레이스를 마지막에 계산하도록 함.
		FCollisionQueryParams Params(
		SCENE_QUERY_STAT(FANameplateOcclusion),
		false);

		Params.AddIgnoredActor(this);

		if (const APawn* LocalPawn = LocalController->GetPawn())
		{
			Params.AddIgnoredActor(LocalPawn);
		}


		//채널을 변경하여 세부적인 트레이스 조건 수정 가능.
		FHitResult Hit;
		const bool bHeadBlocked = World->LineTraceSingleByChannel(
			Hit,
			ViewLocation,
			NameLocation,
			ECC_Visibility,
			Params);
		const bool bBodyBlocked = World->LineTraceSingleByChannel(
			Hit,
			ViewLocation,
			FootLocation,
			ECC_Visibility,
			Params);

		//머리와 발 모두 보일때 표기함. 머리카락 조금 보였다고 이름표가 보이면 숨는 의미가없음.
		if (bHeadBlocked || bBodyBlocked)
		{
			bRuleMarkerOccluded = true;
			return false;
		}
		bRuleMarkerOccluded = false;
		return true;
	}
	bRuleMarkerOccluded = true;
	return false;
}

void AFACharacter::UpdateGroupRules()
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	const FAGameplayTags& FATags = FAGameplayTags::Get();
	const bool bIsHuman = AbilitySystemComponent->HasMatchingGameplayTag(FATags.State_Human);
	const bool bIsZombie = AbilitySystemComponent->HasMatchingGameplayTag(FATags.State_Zombie);

	// 각 진영별 룰 보유 여부 체크
	const bool bHasLeash = bIsHuman && AbilitySystemComponent->HasMatchingGameplayTag(FATags.Rule_Group_Leash);
	const bool bHasRepel = bIsHuman && AbilitySystemComponent->HasMatchingGameplayTag(FATags.Rule_Group_Repel);
	const bool bHasZombieRepel = bIsZombie && AbilitySystemComponent->HasMatchingGameplayTag(FATags.Rule_Social_Repel);

	// 거리 룰을 하나도 안 가지고 있다면 연산 없이 태그만 지우고 종료
	if (!bHasLeash && !bHasRepel && !bHasZombieRepel)
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(FATags.State_Group_Leash);
		AbilitySystemComponent->RemoveLooseGameplayTag(FATags.State_Group_Repel);
		AbilitySystemComponent->RemoveLooseGameplayTag(FATags.State_Social_Repel);
		return;
	}

	// 나와 같은 진영인 가장 가까운 유저 찾기
	float MinDistSq = MAX_flt;
	for (TActorIterator<AFACharacter> It(GetWorld()); It; ++It)
	{
		AFACharacter* Other = *It;

		if (IsValid(Other) && Other != this && Other->GetFAAbilitySystemComponent())
		{
			bool bSameTeam = (bIsHuman && Other->GetFAAbilitySystemComponent()->HasMatchingGameplayTag(FATags.State_Human)) ||
				(bIsZombie && Other->GetFAAbilitySystemComponent()->HasMatchingGameplayTag(FATags.State_Zombie));

			if (bSameTeam)
			{
				const float DistSq = FVector::DistSquared(GetActorLocation(), Other->GetActorLocation());
				MinDistSq = FMath::Min(MinDistSq, DistSq);
			}
		}
	}

	// 인간 룰 (H34, H35)
	if (bIsHuman)
	{
		if (bHasLeash)
		{
			if (MinDistSq > FMath::Square(1500.0f)) AbilitySystemComponent->AddLooseGameplayTag(FATags.State_Group_Leash);
			else AbilitySystemComponent->RemoveLooseGameplayTag(FATags.State_Group_Leash);
		}
		if (bHasRepel)
		{
			if (MinDistSq <= FMath::Square(500.0f)) AbilitySystemComponent->AddLooseGameplayTag(FATags.State_Group_Repel);
			else AbilitySystemComponent->RemoveLooseGameplayTag(FATags.State_Group_Repel);
		}
	}

	// 좀비 룰 (Z39)
	if (bIsZombie)
	{
		if (bHasZombieRepel)
		{
			if (MinDistSq <= FMath::Square(300.0f)) AbilitySystemComponent->AddLooseGameplayTag(FATags.State_Social_Repel);
			else AbilitySystemComponent->RemoveLooseGameplayTag(FATags.State_Social_Repel);
		}
	}
}

void AFACharacter::SetHeartbeatRevealActive(bool bActive)
{
	if (bHeartbeatRevealActive == bActive)
	{
		return;
	}
	bHeartbeatRevealActive = bActive;
	RefreshCustomDepth();
}

void AFACharacter::UpdateOverheadWidget(float DeltaTime)
{
	if (!RuleMarker || !NicknameMarker)
	{
		return;
	}

	
	
	OverheadWidgetOcclusionTimer += DeltaTime;
	if (OverheadWidgetOcclusionTimer >= OverheadWidgetOcclusionInterval)
	{
		OverheadWidgetOcclusionTimer = 0.0f;
		bool IsCharacterVisible = ShouldShowOverheadWidget();
		if (IsCharacterVisible != NicknameMarker->IsVisible())
		{
			NicknameMarker->SetVisibility(IsCharacterVisible);
		}
	}
	
	const bool bShouldShowRuleMaker = ShouldShowRuleMarker();

	if (bShouldShowRuleMaker != RuleMarker->IsVisible())
	{
		RuleMarker->SetVisibility(bShouldShowRuleMaker);
	}

	if (!bShouldShowRuleMaker)
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
		const float FadeEnd = FMath::Max(OverheadWidgetCullDistance, RuleMarkerFullScaleDistance + 1.0f);
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
	// 이전 BP에 카메라 부착값이 남아 있어도 전신과 같은 기준 좌표로 맞춘다.
	FirstPersonArms->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale);
	FirstPersonArms->SetRelativeTransform(FTransform::Identity);
	GetMesh()->AddTickPrerequisiteActor(this);
	// 전신 포즈와 카메라 갱신이 모두 끝난 뒤 팔이 이번 프레임의 포즈를 복사한다.
	FirstPersonArms->AddTickPrerequisiteComponent(GetMesh());
	FirstPersonArms->AddTickPrerequisiteComponent(CameraBoom);

	// 로비는 AFAPlayerState 가 아니라 AFALobbyPlayerState 를 쓴다 — 진영 알림이 아예 없다.
	// 진영을 모르는 맥락에서는 인간 외형이 기본이므로, 여기서 한 번 기준선을 깔아둔다.
	// 인게임에서는 진영이 정해지는 순간 HandleTeamChanged 가 다시 덮는다.
	const AFAPlayerState* FAPlayerState = GetPlayerState<AFAPlayerState>();
	ApplyTeamMaterial(FAPlayerState ? FAPlayerState->GetTeam() : EFATeam::None);

	SetupCaptureMarker();
	SetupRuleMarker();
	SetupNicknameMarker();

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
	RefreshNicknameMarker();
	RefreshInteractionTick();
}

void AFACharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	InitializeAbilitySystem();
	BindTeamNotify();
	RefreshNicknameMarker();
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
	GetFAMovementComponent()->SetFirstPersonFacingRequested(false);
	UpdateFirstPersonCameraPresentation(false);
	SetFirstPersonArmsVisible(false);

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
	GetFAMovementComponent()->SetFirstPersonFacingRequested(false);
	UpdateFirstPersonCameraPresentation(false);
	UpdateFirstPersonPoseSource(false);
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

	if (HasAuthority())
	{
		if (NewTeam == EFATeam::Human || NewTeam == EFATeam::Zombie)
		{
			GetWorldTimerManager().SetTimer(GroupRuleTimerHandle, this, &AFACharacter::UpdateGroupRules, 0.5f, true);
		}
		else
		{
			GetWorldTimerManager().ClearTimer(GroupRuleTimerHandle);
		}

		// 진영이 바뀔 때 캐릭터에 남아있는 조건 태그들 일괄 초기화
		if (AbilitySystemComponent)
		{
			AbilitySystemComponent->RemoveLooseGameplayTag(FAGameplayTags::Get().State_Group_Leash);
			AbilitySystemComponent->RemoveLooseGameplayTag(FAGameplayTags::Get().State_Group_Repel);
			AbilitySystemComponent->RemoveLooseGameplayTag(FAGameplayTags::Get().State_Social_Repel);
		}
	}
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
	for (int32 SlotIndex = 0; SlotIndex < FirstPersonArms->GetNumMaterials(); ++SlotIndex)
	{
		FirstPersonArms->SetMaterial(SlotIndex, TargetMaterial);
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
			// H13 룰 카드 확인
			const bool bNoisyJump = AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(FAGameplayTags::Get().Rule_Move_NoisyJump);
			const FName JumpSoundId = bNoisyJump ? FACharacterAudio::JumpNoisy : FACharacterAudio::JumpStart;
			Audio->PlaySoundAtLocation(JumpSoundId, GetActorLocation());
		}
		return;
	}

	// 공중 -> 땅. 얼마나 세게 떨어졌는지는 낙하 중에 재둔 값으로 판정한다.
	if (PrevMovementMode == MOVE_Falling && NewMode != MOVE_Falling)
	{
		const bool bHeavy = PeakFallSpeed >= HeavyLandSpeed;
		PeakFallSpeed = 0.0f;

		Audio->PlaySoundAtLocation(bHeavy ? FACharacterAudio::LandHeavy : FACharacterAudio::JumpLand, GetActorLocation());
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

	// [H23 투명한 적] NoZombie 도 MutualSight 와 똑같이 "로컬 뷰어의 룰 태그가 바뀌면 맵 전체를 다시 그린다" 패턴이라
	// 같은 핸들러를 재사용한다 — RefreshCustomDepth() 안에서 실제 판정을 한다.
	AbilitySystemComponent->RegisterGameplayTagEvent(
		FAGameplayTags::Get().Rule_Vision_NoZombie, EGameplayTagEventType::NewOrRemoved
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

void AFACharacter::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	Super::CalcCamera(DeltaTime, OutResult);
	if (IsLocallyControlled() && bIsFirstPerson && !IsHiding())
	{
		// 위치·FOV·흔들림은 기존 카메라가 계산하고, 근접면만 현재 일인칭 시야에 적용한다.
		OutResult.PerspectiveNearClipPlane = FMath::Min(
			OutResult.GetFinalPerspectiveNearClipPlane(), FirstPersonNearClipPlane);
	}
}

void AFACharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateFirstPersonViewMode();
	if (IsLocallyControlled())
	{
		// 전환 타이머가 끝난 후에도 앉은 채 이동하면 목표 눈높이가 계속 달라진다.
		if (bIsFirstPerson && bIsCrouched && !IsHiding()
			&& !GetWorldTimerManager().IsTimerActive(CrouchTimerHandle)
			&& !GetWorldTimerManager().IsTimerActive(PerspectiveTimerHandle))
		{
			UpdateCrouchCamera();
		}
	}

	// 착지 순간에는 속도가 이미 죽어 있다 — 낙하하는 동안 최고 속도를 재둬야 세게 떨어졌는지 안다.
	// 복제된 Velocity 를 보므로 남의 캐릭터에서도 같은 판정이 나온다.
	if (const UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		if (Movement->MovementMode == MOVE_Falling)
		{
			PeakFallSpeed = FMath::Max(PeakFallSpeed, -Movement->Velocity.Z);
		}
	}

	if (!bIsFirstPerson)
	{
		UpdateCameraCollision(DeltaTime);
	}

	bool bShouldHideMesh = (CameraBoom->TargetArmLength < 40.f);
	if (GetMesh()->bOwnerNoSee != bShouldHideMesh)
	{
		GetMesh()->SetOwnerNoSee(bShouldHideMesh);
	}

	// 카메라 충돌로 3인칭 붐이 짧아진 경우에는 팔을 표시하지 않는다.
	const bool bShowFirstPersonArms = IsLocallyControlled() && bIsFirstPerson
		&& bShouldHideMesh && !IsHiding() && !IsHidden();
	SetFirstPersonArmsVisible(bShowFirstPersonArms);

	// 포획 상태는 복제로만 들어오므로 이벤트가 없다 — HUD 게이지와 같은 방식으로 폴링한다.
	UpdateCaptureMarker();
	UpdateOverheadWidget(DeltaTime);
	/*
	//이름표 임시, RuleMarker와 중복된 내용이 있어 병합 필요함.
	//ShouldShowOverheadWidget의 내용과 UpdateRuleMarkerOcclusion의 내용 팀원과 확인 후 병합작업 필요.
	NicknameMarkerOcclusionTimer += DeltaTime;
	if (NicknameMarker && NicknameMarkerOcclusionTimer >= 0.1f)
	{
		NicknameMarkerOcclusionTimer = 0.0f;
		bool ShouldVisible = ShouldShowOverheadWidget();
		if (ShouldVisible != NicknameMarker->IsVisible())
		{
			NicknameMarker->SetVisibility(ShouldVisible);
		}
	}
	*/
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

void AFACharacter::UpdateFirstPersonViewMode()
{
	const bool bLocalPlayer = IsLocallyControlled();
	UpdateFirstPersonCameraPresentation(bLocalPlayer && bIsFirstPerson);
	if (bLocalPlayer)
	{
		GetFAMovementComponent()->SetFirstPersonFacingRequested(bIsFirstPerson);
	}
}

void AFACharacter::SetFirstPersonArmsVisible(bool bVisible)
{
	UpdateFirstPersonPoseSource(bVisible);
	if (FirstPersonArms->IsVisible() != bVisible)
	{
		FirstPersonArms->SetVisibility(bVisible);
	}
	// 애니메이션 초기화가 틱을 다시 켤 수 있으므로 표시 여부와 별도로 맞춘다.
	if (FirstPersonArms->IsComponentTickEnabled() != bVisible)
	{
		FirstPersonArms->SetComponentTickEnabled(bVisible);
	}
}

void AFACharacter::UpdateFirstPersonCameraPresentation(bool bFirstPersonView)
{
	if (!CameraBoom || !FollowCamera || bFirstPersonView == bFirstPersonCameraPresentationActive)
	{
		return;
	}
	if (bFirstPersonView)
	{
		bSavedCameraTranslationLag = CameraBoom->bEnableCameraLag;
		// 삼인칭 추종 지연을 그대로 쓰면 달릴 때 눈이 몸 뒤로 밀린다.
		// 기존 시점·앉기 보간과 게임플레이 카메라 흔들림은 유지한다.
		CameraBoom->bEnableCameraLag = false;
		bSavedMotionBlurOverride = FollowCamera->PostProcessSettings.bOverride_MotionBlurAmount;
		SavedMotionBlurAmount = FollowCamera->PostProcessSettings.MotionBlurAmount;
		FollowCamera->PostProcessSettings.bOverride_MotionBlurAmount = true;
		FollowCamera->PostProcessSettings.MotionBlurAmount = 0.0f;
	}
	else
	{
		CameraBoom->bEnableCameraLag = bSavedCameraTranslationLag;
		FollowCamera->PostProcessSettings.bOverride_MotionBlurAmount = bSavedMotionBlurOverride;
		FollowCamera->PostProcessSettings.MotionBlurAmount = SavedMotionBlurAmount;
	}
	bFirstPersonCameraPresentationActive = bFirstPersonView;
}

void AFACharacter::UpdateFirstPersonPoseSource(bool bShowArms)
{
	if (bShowArms == bFirstPersonSourceActive || !GetMesh())
	{
		return;
	}
	if (bShowArms)
	{
		SavedBodyVisibilityTickOption = static_cast<uint8>(GetMesh()->VisibilityBasedAnimTickOption);
		bSavedBodyUpdateRateOptimizations = GetMesh()->bEnableUpdateRateOptimizations;
		// 본인 화면에서 전신을 숨겨도 팔이 복사할 본은 계속 갱신해야 한다.
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		GetMesh()->bEnableUpdateRateOptimizations = false;
	}
	else
	{
		GetMesh()->VisibilityBasedAnimTickOption = static_cast<EVisibilityBasedAnimTickOption>(SavedBodyVisibilityTickOption);
		GetMesh()->bEnableUpdateRateOptimizations = bSavedBodyUpdateRateOptimizations;
	}
	bFirstPersonSourceActive = bShowArms;
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

float AFACharacter::GetFirstPersonEyeHeight() const
{
	if (!bIsCrouched)
	{
		return BaseEyeHeight + FirstPersonEyeHeightOffset;
	}
	const float MovingWeight = FMath::Clamp(GetVelocity().Size2D()
		/ FMath::Max(FirstPersonCrouchedEyeReferenceSpeed, 1.0f), 0.0f, 1.0f);
	return BaseEyeHeight + FMath::Lerp(FirstPersonCrouchedIdleEyeHeightOffset,
		FirstPersonCrouchedMovingEyeHeightOffset, MovingWeight);
}

void AFACharacter::UpdateCrouchCamera()
{
	if (IsHiding())
	{
		GetWorldTimerManager().ClearTimer(CrouchTimerHandle);
		return;
	}
	float BaseZ = bIsFirstPerson ? GetFirstPersonEyeHeight() : 0.f;
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
	UpdateFirstPersonViewMode();
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
	const bool bForcedBlind = AbilitySystemComponent->HasMatchingGameplayTag(FATags.State_Vision_Blind); // [Z32 포식 후 실명]
	const bool bNoAlly = AbilitySystemComponent->HasMatchingGameplayTag(FATags.Rule_Vision_NoAlly); // [H24 고독]

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
		const float CurrentVisionRange = Attributes->GetVisionRange();

		// 0 = 정상 시야, 1 = VisionRange 가 바닥(800uu 근처)까지 떨어진 상태.
		// 분모는 UFAAttributeSet::MaxVisionRange 하나만 본다 — 예전엔 여기 따로 6000.0f 를 복사해와서
		// AttributeSet 쪽 상한을 바꾸면 이 계산만 몰래 안 맞는 문제가 있었다.
		FogAmount = FMath::Clamp(1.0f - (CurrentVisionRange / UFAAttributeSet::MaxVisionRange), 0.0f, 1.0f);
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
		UpdateOpponentDistanceCulling(FogAmount > 0.0f ? FullDistance : NoCullingDistance, bNoAlly);
	}

	// 명멸/암전 (Vision.Blackout) — GE 는 태그만 부여. 주기적인 깜빡임은 여기서 직접 계산한다.
	// 주기·지속시간은 BlackoutPeriod/BlackoutFlashDuration(Class Defaults)에서 튜닝한다.
	float BlackoutAmount = 0.0f;
	if (bForcedBlind) // Z32 포식 후 실명 — 5초간 계속 완전 암전
	{
		BlackoutAmount = 1.0f;
	}
	else if (bBlackout && GetWorld()) // Vision.Blackout — 주기적인 명멸
	{
		const float TimeInCycle = FMath::Fmod(GetWorld()->GetTimeSeconds(), BlackoutPeriod);
		BlackoutAmount = (TimeInCycle < BlackoutFlashDuration) ? 1.0f : 0.0f;
	}

	// 심박 왜곡 (Vision.Shake, Z25) — 가장 가까운 인간까지 거리가 가까울수록 화면을 흔든다.
	// 카메라 셰이크 애셋 없이 FollowCamera 의 RelativeRotation 을 직접 흔든다.
	const bool bShake = AbilitySystemComponent->HasMatchingGameplayTag(FATags.Rule_Vision_Shake);
	float ShakeAlpha = 0.0f;
	if (bShake)
	{
		UpdateClosestHumanDistance();

		const float ClosestDistance = FMath::Sqrt(ClosestHumanDistanceSq);
		// ShakeMinDistance 이하면 1(최대), ShakeMaxDistance 이상이면 0.
		ShakeAlpha = 1.0f - FMath::Clamp(
			(ClosestDistance - ShakeMinDistance) / FMath::Max(ShakeMaxDistance - ShakeMinDistance, 1.0f),
			0.0f, 1.0f);
	}

	if (ShakeAlpha > 0.0f && GetWorld())
	{
		// 축마다 다른 위상을 줘서 피치·요가 같은 파형으로 안 겹치게 한다.
		const float NoiseTime = GetWorld()->GetTimeSeconds() * ShakeFrequency;
		const float PitchNoise = (FMath::PerlinNoise1D(NoiseTime) - 0.5f) * 2.0f;
		const float YawNoise   = (FMath::PerlinNoise1D(NoiseTime + 37.0f) - 0.5f) * 2.0f;
		FollowCamera->SetRelativeRotation(FRotator(
			PitchNoise * ShakeAlpha * ShakeMaxAngle,
			YawNoise * ShakeAlpha * ShakeMaxAngle,
			0.0f));
	}
	else
	{
		// 룰이 없거나 인간이 사거리 밖이면 항상 원위치로 — 다른 카메라 로직과 안 겹치니 무조건 되돌려도 안전하다.
		FollowCamera->SetRelativeRotation(FRotator::ZeroRotator);
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
	// 시야 효과가 없는 평상시에도 일인칭 모션 블러 설정은 적용해야 한다.
	FollowCamera->PostProcessBlendWeight = (bAnyVisualEffectActive || bFirstPersonCameraPresentationActive) ? 1.0f : 0.0f;
}

void AFACharacter::UpdateOpponentDistanceCulling(float FullDistance, bool bHideAllies)
{
	// 너무 잦은 전체 순회는 비용이 크므로 일정 주기로만 수행한다.
	TimeSinceLastOpponentCullCheck += GetWorld()->GetDeltaSeconds();
	if (TimeSinceLastOpponentCullCheck < OpponentCullCheckInterval)
	{
		return;
	}
	TimeSinceLastOpponentCullCheck = 0.0f;

	const FAGameplayTags& FATags = FAGameplayTags::Get();
	const FVector MyLocation = GetActorLocation();
	const float FullDistanceSq = FMath::Square(FullDistance);

	const bool bLocalIsHuman = AbilitySystemComponent->HasMatchingGameplayTag(FATags.State_Human);
	const bool bLocalIsZombie = AbilitySystemComponent->HasMatchingGameplayTag(FATags.State_Zombie);

	for (TActorIterator<AFACharacter> It(GetWorld()); It; ++It)
	{
		AFACharacter* Other = *It;

		if (!IsValid(Other) || Other == this || !Other->GetMesh())
		{
			continue;
		}

		// 캐비넷 등 다른 로직이 이미 액터 전체를 숨긴 상태라면 여기서 건드리지 않음.
		if (Other->bHideStateApplied)
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(MyLocation, Other->GetActorLocation());

		// Vision.Fog 거리 컬링.
		bool bShouldHide = DistSq > FullDistanceSq;

		// [H24 고독 / Z37 고립]
		// Rule.Vision.NoAlly 보유 시 자신과 같은 진영의 다른 플레이어를 숨긴다.
		if (bHideAllies)
		{
			if (const UFAAbilitySystemComponent* OtherASC = Other->GetFAAbilitySystemComponent())
			{
				const bool bOtherIsHuman = OtherASC->HasMatchingGameplayTag(FATags.State_Human);
				const bool bOtherIsZombie = OtherASC->HasMatchingGameplayTag(FATags.State_Zombie);

				if ((bLocalIsHuman && bOtherIsHuman) || (bLocalIsZombie && bOtherIsZombie))
				{
					bShouldHide = true;
				}
			}
		}

		// 로컬 화면에서만 적용.
		Other->GetMesh()->SetHiddenInGame(bShouldHide);
	}
}

void AFACharacter::UpdateClosestHumanDistance()
{
	// 너무 잦은 전체 순회는 비용이 크므로 일정 주기로만 갱신한다.
	TimeSinceLastShakeCheck += GetWorld()->GetDeltaSeconds();
	if (TimeSinceLastShakeCheck < ShakeCheckInterval)
	{
		return;
	}
	TimeSinceLastShakeCheck = 0.0f;

	const FAGameplayTags& FATags = FAGameplayTags::Get();
	const FVector MyLocation = GetActorLocation();
	float ClosestSq = TNumericLimits<float>::Max();

	for (TActorIterator<AFACharacter> It(GetWorld()); It; ++It)
	{
		AFACharacter* Other = *It;
		if (!IsValid(Other) || Other == this || !Other->GetFAAbilitySystemComponent())
		{
			continue;
		}

		if (!Other->GetFAAbilitySystemComponent()->HasMatchingGameplayTag(FATags.State_Human))
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(MyLocation, Other->GetActorLocation());
		if (DistSq < ClosestSq)
		{
			ClosestSq = DistSq;
		}
	}

	ClosestHumanDistanceSq = ClosestSq;
}

void AFACharacter::UpdatePerspectiveCamera()
{
	float DeltaTime = GetWorld()->GetDeltaSeconds();
	float TargetOffsetZ = bIsFirstPerson ? GetFirstPersonEyeHeight() : 0.f;
	CameraBoom->TargetOffset.Z = FMath::FInterpTo(CameraBoom->TargetOffset.Z, TargetOffsetZ, DeltaTime, CameraInterpSpeed);

	if (bIsFirstPerson)
	{
		CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, 0.f, DeltaTime, CameraInterpSpeed);

		if (FMath::IsNearlyEqual(CameraBoom->TargetArmLength, 0.f, 1.f) &&
			FMath::IsNearlyEqual(CameraBoom->TargetOffset.Z, TargetOffsetZ, 1.f))
		{
			CameraBoom->TargetArmLength = 0.f;
			CameraBoom->TargetOffset.Z = TargetOffsetZ;
			GetWorldTimerManager().ClearTimer(PerspectiveTimerHandle);
		}
	}
	else
	{
		if (FMath::IsNearlyEqual(CameraBoom->TargetOffset.Z, TargetOffsetZ, 1.f))
		{
			CameraBoom->TargetOffset.Z = TargetOffsetZ;
			GetWorldTimerManager().ClearTimer(PerspectiveTimerHandle);
		}
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

	if (GetWorld()->SweepSingleByChannel(HitResult, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(CameraBoom->ProbeSize), QueryParams))
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
	CameraBoom->TargetOffset = FVector(0.0f, 0.0f, bIsFirstPerson ? GetFirstPersonEyeHeight() : 0.0f);

	GetMesh()->SetOwnerNoSee(CameraBoom->TargetArmLength < 40.0f);
}
