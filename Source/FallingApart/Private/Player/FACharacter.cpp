// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/FACharacter.h"
#include "Audio/FAAudioSubsystem.h"
#include "AttributeSet/FAAttributeSet.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "GAS/FAAbilitySystemComponent.h"
#include "GAS/FAGameplayAbility.h"
#include "GAS/FAGameplayTags.h"
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


namespace FACharacterAudio
{
	// 표의 RowName 과 같아야 한다 (DT_Sound_Character).
	static const FName CrouchIn(TEXT("Crouch_In"));
	static const FName CrouchOut(TEXT("Crouch_Out"));
	static const FName JumpStart(TEXT("Jump_Start"));
	static const FName JumpLand(TEXT("Jump_Land"));
	static const FName LandHeavy(TEXT("Land_Heavy"));
	static const FName GrowlZombieLoop(TEXT("Growl_Zombie_Loop"));

	/** QTE 연타 피드백. 누른 본인에게만 나는 2D 소리라 멀티캐스트를 타지 않는다. */
	static const FName QTEPress(TEXT("QTE_Press"));
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

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// 3인칭이어도 오디오 리스너는 카메라가 아니라 캐릭터 헤드에 붙는다(기획 확정).
	// 카메라/스프링암 쪽에서 SetAudioListenerOverride 를 호출하면 이 동작이 깨진다.
	VoiceComponent = CreateDefaultSubobject<UFAVoiceComponent>(TEXT("VoiceComponent"));

	AbilitySystemComponent = CreateDefaultSubobject<UFAAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AttributeSet = CreateDefaultSubobject<UFAAttributeSet>(TEXT("AttributeSet"));

	CharacterStatusComponent =CreateDefaultSubobject<UFACharacterStatusComponent>(TEXT("CharacterStatusComponent"));
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

	// QTE 로 멈춰 있는 동안은 서버 시각이 계속 흐르므로 저장해 둔 남은 시간을 그대로 쓴다.
	const float Remaining = CaptureState.PausedRemaining > 0.0f
		? CaptureState.PausedRemaining
		: CaptureState.EndServerTime - GetServerWorldTime();

	return FMath::Clamp(1.0f - Remaining / CaptureState.TotalDuration, 0.0f, 1.0f);
}

void AFACharacter::BeginCaptureState(bool bInIsVictim, float Duration)
{
	if (!HasAuthority() || Duration <= 0.0f)
	{
		return;
	}

	CaptureState.bIsVictim = bInIsVictim;
	CaptureState.TotalDuration = Duration;
	CaptureState.EndServerTime = GetServerWorldTime() + Duration;
	CaptureState.PausedRemaining = 0.0f;
	CaptureState.ResistRatio = 0.0f;
}

void AFACharacter::PauseCaptureState(float RemainingSeconds)
{
	// 남은 시간이 0 이하로 들어오면 '멈춤'과 '진행 중'을 구분할 수 없다 — 그냥 무시한다.
	if (!HasAuthority() || !CaptureState.IsActive() || RemainingSeconds <= 0.0f)
	{
		return;
	}

	CaptureState.PausedRemaining = RemainingSeconds;
}

void AFACharacter::ResumeCaptureState(float RemainingSeconds)
{
	if (!HasAuthority() || !CaptureState.IsActive())
	{
		return;
	}

	CaptureState.PausedRemaining = 0.0f;
	CaptureState.EndServerTime = GetServerWorldTime() + FMath::Max(0.0f, RemainingSeconds);
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
}

void AFACharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	InitializeAbilitySystem();
	BindTeamNotify();
}

void AFACharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	InitializeAbilitySystem();
	BindTeamNotify();
}

void AFACharacter::OnRep_Controller()
{
	Super::OnRep_Controller();

	if (const AFAPlayerState* FAPlayerState = GetPlayerState<AFAPlayerState>())
	{
		HandleTeamChanged(FAPlayerState->GetTeam());
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

	Super::EndPlay(EndPlayReason);
}

void AFACharacter::BindTeamNotify()
{
	AFAPlayerState* FAPlayerState = GetPlayerState<AFAPlayerState>();
	if (!FAPlayerState || TeamNotifySource.Get() == FAPlayerState)
	{
		return;
	}

	if (AFAPlayerState* PreviousState = TeamNotifySource.Get())
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
	ApplyTeamMaterial(NewTeam);
	UpdateGrowlLoop(NewTeam);
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
		const float PitchSign = UFASettingsBridge::IsYAxisInverted(this) ? -1.f : 1.f;

		AddControllerYawInput(LookAxisVector.X * Sensitivity);
		AddControllerPitchInput(LookAxisVector.Y * Sensitivity * PitchSign);
	}
}

void AFACharacter::Input_Jump()
{
	if (!AbilitySystemComponent) return;

	const FAGameplayTags& FATags = FAGameplayTags::Get();

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
