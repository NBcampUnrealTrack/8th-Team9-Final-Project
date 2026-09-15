// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FAHUDWidget.h"

#include "Audio/FAAudioSubsystem.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Game/FAGameState.h"
#include "Player/FACharacter.h"
#include "Player/FAPlayerState.h"
#include "TimerManager.h"
#include "Interact/FAInteractionComponent.h"

namespace FAHUDAudio
{
	/** 표의 RowName 과 같아야 한다 (DT_Sound_Game). */
	static const FName RoundTimerTick(TEXT("Round_Timer_Tick"));
	static const FName RoundLastMinute(TEXT("Round_LastMinute"));
	static const FName ScorePersonal(TEXT("Score_Personal"));
	static const FName ScoreTeam(TEXT("Score_Team"));
	static const FName ScoreVoided(TEXT("Score_Voided"));

	/** 감염 확정 순간 로컬 플레이어에게만 나는 소리 (DT_Sound_Music · DT_Sound_Character). */
	static const FName StingInfected(TEXT("Sting_Infected"));
	static const FName InfectBlackout(TEXT("Infect_Blackout"));
}

namespace
{
	/** 남은 시간 초를 "M:SS" 로. 올림이라 라운드 시작 순간이 3:30 으로 보이고 0:00 은 실제 만료 때만 뜸. */
	FText FormatRoundTime(float SecondsRemaining)
	{
		const int32 TotalSeconds = FMath::Max(0, FMath::CeilToInt(SecondsRemaining));
		return FText::FromString(FString::Printf(TEXT("%d:%02d"), TotalSeconds / 60, TotalSeconds % 60));
	}
}

void UFAHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RefreshBindings();
	UpdatePolledValues(0.0f);

	// 첫 프레임에 지운다. 값이 붙는 건 각 핸들러가 알아서 한다.
	RefreshScoreTexts();
	RefreshTeamText();
	RefreshTeamCountTexts();
	RefreshRoundText();

	// 게이지·프롬프트의 시작 상태를 여기서 한 번 못박는다.
	OnCaptureGaugeHidden();
	OnContestPromptHidden();
	SetInteractKeyVisible(false);
}

void UFAHUDWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ScoreVoidedTimerHandle);
	}

	UnbindPlayerState();
	UnbindGameState();

	Super::NativeDestruct();
}

void UFAHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 포인터 비교뿐이라 매 프레임 불러도 부담이 없움.
	RefreshBindings();
	UpdatePolledValues(InDeltaTime);

	// UpdatePolledValues 는 GameState 가 없으면 그냥 빠져나가므로 게이지·프롬프트는 따로 부른다.
	UpdateCaptureGauge();
	UpdateContestPrompt();
	UpdateInteractPrompt();
}

float UFAHUDWidget::GetRoundTimeRemaining() const
{
	const AFAGameState* FAGameState = CachedGameState.Get();
	return FAGameState ? FAGameState->GetRoundTimeRemaining() : 0.0f;
}

FText UFAHUDWidget::GetRoundTimeText() const
{
	return FormatRoundTime(GetRoundTimeRemaining());
}

FText UFAHUDWidget::GetRoundText() const
{
	const AFAGameState* FAGameState = CachedGameState.Get();
	if (!FAGameState || FAGameState->GetCurrentRound() <= 0)
	{
		// 매치 시작 전에는 "0 / 5" 가 아니라 아무것도 띄우지 않음.
		return FText::GetEmpty();
	}

	return FText::FromString(FString::Printf(TEXT("%d / %d"), FAGameState->GetCurrentRound(), FAGameState->GetMaxRound()));
}

int32 UFAHUDWidget::GetSurvivorCount() const
{
	const AFAGameState* FAGameState = CachedGameState.Get();
	return FAGameState ? FAGameState->GetSurvivorCount() : 0;
}

int32 UFAHUDWidget::GetZombieCount() const
{
	const AFAGameState* FAGameState = CachedGameState.Get();
	return FAGameState ? FAGameState->GetZombieCount() : 0;
}

FText UFAHUDWidget::GetSurvivorCountText() const
{
	return FText::AsNumber(GetSurvivorCount());
}

FText UFAHUDWidget::GetZombieCountText() const
{
	return FText::AsNumber(GetZombieCount());
}

FText UFAHUDWidget::GetLocalScoreText() const
{
	return FText::AsNumber(GetLocalDisplayScore());
}

int32 UFAHUDWidget::GetLocalDisplayTeamScore() const
{
	const AFAPlayerState* FAPlayerState = CachedPlayerState.Get();
	return FAPlayerState ? FAPlayerState->GetDisplayTeamScore() : 0;
}

FText UFAHUDWidget::GetLocalTeamScoreText() const
{
	return FText::AsNumber(GetLocalDisplayTeamScore());
}

int32 UFAHUDWidget::GetLocalDisplayPersonalScore() const
{
	const AFAPlayerState* FAPlayerState = CachedPlayerState.Get();
	return FAPlayerState ? FAPlayerState->GetDisplayPersonalScore() : 0;
}

FText UFAHUDWidget::GetLocalPersonalScoreText() const
{
	return FText::AsNumber(GetLocalDisplayPersonalScore());
}

FText UFAHUDWidget::GetLocalTeamText() const
{
	switch (GetLocalTeam())
	{
	case EFATeam::Human:
		return NSLOCTEXT("FallingApart", "TeamHuman", "인간");

	case EFATeam::Zombie:
		return NSLOCTEXT("FallingApart", "TeamZombie", "좀비");

	default:
		return NSLOCTEXT("FallingApart", "TeamNone", "-");
	}
}

EFARoundPhase UFAHUDWidget::GetRoundPhase() const
{
	const AFAGameState* FAGameState = CachedGameState.Get();
	return FAGameState ? FAGameState->GetRoundPhase() : EFARoundPhase::WaitingToStart;
}

EFATeam UFAHUDWidget::GetLocalTeam() const
{
	const AFAPlayerState* FAPlayerState = CachedPlayerState.Get();
	return FAPlayerState ? FAPlayerState->GetTeam() : EFATeam::None;
}

bool UFAHUDWidget::IsLocalPlayerZombie() const
{
	return GetLocalTeam() == EFATeam::Zombie;
}

int32 UFAHUDWidget::GetLocalDisplayScore() const
{
	const AFAPlayerState* FAPlayerState = CachedPlayerState.Get();
	return FAPlayerState ? FAPlayerState->GetDisplayTotalScore() : 0;
}

AFAGameState* UFAHUDWidget::GetFAGameState() const
{
	return CachedGameState.Get();
}

AFAPlayerState* UFAHUDWidget::GetLocalFAPlayerState() const
{
	return CachedPlayerState.Get();
}

void UFAHUDWidget::ShowCaptureGauge(bool bLocalPlayerIsVictim)
{
	bCaptureGaugeVisible = true;

	// 이전 포획의 잔여값이 한 프레임 보이지 않도록 비교 기준을 비움.
	LastInfectionRatio = -1.0f;
	LastResistRatio = -1.0f;

	OnCaptureGaugeShown(bLocalPlayerIsVictim);
	SetCaptureProgress(0.0f, 0.0f);
}

void UFAHUDWidget::SetCaptureProgress(float InfectionRatio, float ResistRatio)
{
	if (!bCaptureGaugeVisible)
	{
		return;
	}

	const float ClampedInfection = FMath::Clamp(InfectionRatio, 0.0f, 1.0f);
	const float ClampedResist = FMath::Clamp(ResistRatio, 0.0f, 1.0f);

	if (FMath::IsNearlyEqual(ClampedInfection, LastInfectionRatio, 0.001f)
		&& FMath::IsNearlyEqual(ClampedResist, LastResistRatio, 0.001f))
	{
		return;
	}

	LastInfectionRatio = ClampedInfection;
	LastResistRatio = ClampedResist;

	OnCaptureProgressChanged(ClampedInfection, ClampedResist);
}

void UFAHUDWidget::HideCaptureGauge()
{
	if (!bCaptureGaugeVisible)
	{
		return;
	}

	bCaptureGaugeVisible = false;
	LastInfectionRatio = -1.0f;
	LastResistRatio = -1.0f;

	OnCaptureGaugeHidden();
}

void UFAHUDWidget::UpdateCaptureGauge()
{
	const APlayerController* OwningController = GetOwningPlayer();
	const AFACharacter* OwningCharacter = OwningController ? Cast<AFACharacter>(OwningController->GetPawn()) : nullptr;

	// 포획이 끝났거나, 감염돼서 Pawn 이 갈렸거나, 아직 빙의 전이거나 — 전부 게이지를 내린다.
	if (!OwningCharacter || !OwningCharacter->GetCaptureState().IsActive())
	{
		HideCaptureGauge();
		return;
	}

	const FFACaptureState& State = OwningCharacter->GetCaptureState();

	// 잡은 쪽과 잡힌 쪽이 다른 화면을 봐야 하므로 역할은 띄우는 순간에 한 번만 넘긴다.
	if (!bCaptureGaugeVisible)
	{
		ShowCaptureGauge(State.bIsVictim);
	}

	SetCaptureProgress(OwningCharacter->GetCaptureInfectionRatio(), State.ResistRatio);
}

EFAContestRole UFAHUDWidget::GetLocalContestRole() const
{
	const APlayerController* OwningController = GetOwningPlayer();
	const AFACharacter* OwningCharacter = OwningController ? Cast<AFACharacter>(OwningController->GetPawn()) : nullptr;

	return OwningCharacter ? OwningCharacter->GetContestState().Role : EFAContestRole::None;
}

float UFAHUDWidget::GetContestTimeRemaining() const
{
	const APlayerController* OwningController = GetOwningPlayer();
	const AFACharacter* OwningCharacter = OwningController ? Cast<AFACharacter>(OwningController->GetPawn()) : nullptr;

	return OwningCharacter ? OwningCharacter->GetContestTimeRemaining() : 0.0f;
}

void UFAHUDWidget::UpdateContestPrompt()
{
	const APlayerController* OwningController = GetOwningPlayer();
	const AFACharacter* OwningCharacter = OwningController ? Cast<AFACharacter>(OwningController->GetPawn()) : nullptr;

	// QTE 가 끝났거나, 아직 빙의 전이거나 — 전부 프롬프트를 내린다.
	if (!OwningCharacter || !OwningCharacter->GetContestState().IsActive())
	{
		if (bContestPromptVisible)
		{
			bContestPromptVisible = false;
			LastContestGauge = -1.0f;

			OnContestPromptHidden();
		}
		return;
	}

	const FFAContestState& State = OwningCharacter->GetContestState();

	// 역할은 띄우는 순간에 한 번만 넘긴다 — QTE 도중에 바뀌지 않는다.
	if (!bContestPromptVisible)
	{
		bContestPromptVisible = true;
		LastContestGauge = -1.0f;

		OnContestPromptShown(State.Role);
	}

	if (!FMath::IsNearlyEqual(State.Gauge, LastContestGauge, 0.001f))
	{
		LastContestGauge = State.Gauge;

		OnContestGaugeChanged(State.Gauge);
	}
}

void UFAHUDWidget::RefreshBindings()
{
	const UWorld* World = GetWorld();
	// GC 로 파괴 예약된 오브젝트도 그대로 돌려받아야 한다.
	AFAGameState* CurrentGameState = World ? World->GetGameState<AFAGameState>() : nullptr;
	if (CurrentGameState != CachedGameState.Get(/*bEvenIfGarbage=*/true))
	{
		UnbindGameState();
		BindGameState(CurrentGameState);
	}

	AFAPlayerState* CurrentPlayerState = Cast<AFAPlayerState>(GetOwningPlayerState());
	if (CurrentPlayerState != CachedPlayerState.Get(/*bEvenIfGarbage=*/true))
	{
		UnbindPlayerState();
		BindPlayerState(CurrentPlayerState);
	}
}

void UFAHUDWidget::BindGameState(AFAGameState* NewGameState)
{
	CachedGameState = NewGameState;

	if (!NewGameState)
	{
		return;
	}

	NewGameState->OnRoundPhaseChanged.AddDynamic(this, &UFAHUDWidget::HandleRoundPhaseChanged);
	NewGameState->OnTeamCountsChanged.AddDynamic(this, &UFAHUDWidget::HandleTeamCountsChanged);
	NewGameState->OnRoundEnded.AddDynamic(this, &UFAHUDWidget::HandleRoundEnded);

	HandleRoundPhaseChanged(NewGameState->GetRoundPhase());
	HandleTeamCountsChanged();
}

void UFAHUDWidget::UnbindGameState()
{
	if (AFAGameState* FAGameState = CachedGameState.Get(/*bEvenIfGarbage=*/true))
	{
		FAGameState->OnRoundPhaseChanged.RemoveDynamic(this, &UFAHUDWidget::HandleRoundPhaseChanged);
		FAGameState->OnTeamCountsChanged.RemoveDynamic(this, &UFAHUDWidget::HandleTeamCountsChanged);
		FAGameState->OnRoundEnded.RemoveDynamic(this, &UFAHUDWidget::HandleRoundEnded);
	}

	CachedGameState = nullptr;

	LastDisplayedSeconds = -1;
	LastCurrentRound = -1;
	LastMaxRound = -1;

	TickAccumulator = 0.0f;
	bLastMinuteAnnounced = false;
	bSawAboveTickWindow = false;

	RefreshTeamCountTexts();
	RefreshRoundText();
}

void UFAHUDWidget::BindPlayerState(AFAPlayerState* NewPlayerState)
{
	CachedPlayerState = NewPlayerState;

	if (!NewPlayerState)
	{
		return;
	}

	NewPlayerState->OnTeamChanged.AddDynamic(this, &UFAHUDWidget::HandleLocalTeamChanged);
	NewPlayerState->OnScoreChanged.AddDynamic(this, &UFAHUDWidget::HandleLocalScoreChanged);

	HandleLocalTeamChanged(NewPlayerState->GetTeam());
	HandleLocalScoreChanged();
}

void UFAHUDWidget::UnbindPlayerState()
{
	if (AFAPlayerState* FAPlayerState = CachedPlayerState.Get(/*bEvenIfGarbage=*/true))
	{
		FAPlayerState->OnTeamChanged.RemoveDynamic(this, &UFAHUDWidget::HandleLocalTeamChanged);
		FAPlayerState->OnScoreChanged.RemoveDynamic(this, &UFAHUDWidget::HandleLocalScoreChanged);
	}

	CachedPlayerState = nullptr;

	// 다른 PlayerState 로 갈아타는 중이다 — 이전 주인의 점수·진영을 기준으로 삼으면 안 된다.
	LastPersonalScore = -1;
	LastTeamScore = -1;
	bLastWasInfectedThisRound = false;
	LastLocalTeam = EFATeam::None;
	bHasTeamBaseline = false;

	// 이전 주인의 점수·진영이 화면에 남지 않도록 되돌린다. 새 PlayerState 가 붙으면 곧바로 덮인다.
	RefreshScoreTexts();
	RefreshTeamText();
}

void UFAHUDWidget::UpdatePolledValues(float DeltaTime)
{
	const AFAGameState* FAGameState = CachedGameState.Get();
	if (!FAGameState)
	{
		return;
	}

	const int32 CurrentRound = FAGameState->GetCurrentRound();
	const int32 CurrentMaxRound = FAGameState->GetMaxRound();
	if (CurrentRound != LastCurrentRound || CurrentMaxRound != LastMaxRound)
	{
		LastCurrentRound = CurrentRound;
		LastMaxRound = CurrentMaxRound;

		RefreshRoundText();
		OnRoundChanged(CurrentRound, CurrentMaxRound);
	}

	const float SecondsRemaining = FAGameState->GetRoundTimeRemaining();
	const bool bRoundInProgress = FAGameState->IsRoundInProgress();

	const int32 DisplayedSeconds = FMath::Max(0, FMath::CeilToInt(SecondsRemaining));
	if (DisplayedSeconds != LastDisplayedSeconds)
	{
		LastDisplayedSeconds = DisplayedSeconds;

		const bool bIsFinalCountdown = bRoundInProgress && SecondsRemaining <= FinalCountdownSeconds;
		OnRoundTimeChanged(FormatRoundTime(SecondsRemaining), SecondsRemaining, bIsFinalCountdown);
	}

	// 초침은 표시 초와 따로 논다 — 표시 초에 묶으면 1초보다 촘촘해질 수가 없다.
	UpdateRoundTimerAudio(SecondsRemaining, bRoundInProgress, DeltaTime);
}

void UFAHUDWidget::UpdateRoundTimerAudio(float SecondsRemaining, bool bRoundInProgress, float DeltaTime)
{
	// 라운드가 도는 동안, 그것도 마지막 1분에만 들린다. 3분 내내 째깍거리면 아무도 못 견딘다.
	if (!bRoundInProgress || SecondsRemaining > TickStartSeconds || SecondsRemaining <= 0.0f)
	{
		TickAccumulator = 0.0f;
		bLastMinuteAnnounced = false;

		// 구간 위에 있는 동안만 기록을 세운다.
		bSawAboveTickWindow = bRoundInProgress && SecondsRemaining > TickStartSeconds;
		return;
	}

	// "1분 남았다"는 신호는 초침과 별개로 딱 한 번.
	if (!bLastMinuteAnnounced)
	{
		bLastMinuteAnnounced = true;

		// 스팅어와 첫 초침이 같은 프레임에 겹치지 않도록 여기서부터 새로 센다.
		TickAccumulator = 0.0f;

		if (UFAAudioSubsystem* Audio = UFAAudioSubsystem::Get(this))
		{
			Audio->EnterFinalCountdown();
		}

		if (!bSawAboveTickWindow)
		{
			bSawAboveTickWindow = true;
			return;
		}

		UFAAudioSubsystem::PlayFASound(this, FAHUDAudio::RoundLastMinute);
		return;
	}

	TickAccumulator += DeltaTime;
	if (TickAccumulator < GetRoundTickInterval(SecondsRemaining))
	{
		return;
	}

	TickAccumulator = 0.0f;
	UFAAudioSubsystem::PlayFASound(this, FAHUDAudio::RoundTimerTick);
}

float UFAHUDWidget::GetRoundTickInterval(float SecondsRemaining) const
{
	if (SecondsRemaining >= FinalCountdownSeconds)
	{
		return TickIntervalSlow;
	}

	// 0 초에 가까워질수록 선형으로 촘촘해진다. 음정은 그대로라 '빨라지는 시계'로 들린다.
	const float Alpha = FMath::Clamp(SecondsRemaining / FMath::Max(FinalCountdownSeconds, UE_KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	return FMath::Lerp(TickIntervalFast, TickIntervalSlow, Alpha);
}

void UFAHUDWidget::HandleRoundPhaseChanged(EFARoundPhase NewPhase)
{
	if (NewPhase != EFARoundPhase::InProgress)
	{
		HideCaptureGauge();

		// 라운드가 끝나면 QTE 도 같이 끝난다. 복제가 한 프레임 늦게 와도 프롬프트가 남지 않게 여기서 내린다.
		if (bContestPromptVisible)
		{
			bContestPromptVisible = false;
			LastContestGauge = -1.0f;

			OnContestPromptHidden();
		}
	}

	OnRoundPhaseChanged(NewPhase);
}

void UFAHUDWidget::HandleTeamCountsChanged()
{
	const AFAGameState* FAGameState = CachedGameState.Get();
	if (!FAGameState)
	{
		return;
	}

	RefreshTeamCountTexts();
	OnTeamCountsChanged(FAGameState->GetSurvivorCount(), FAGameState->GetZombieCount());
}

void UFAHUDWidget::HandleRoundEnded(int32 RoundIndex, EFARoundResult Result)
{
	OnRoundEnded(RoundIndex, Result);
}

void UFAHUDWidget::HandleLocalTeamChanged(EFATeam NewTeam)
{
	// 감염은 되돌아오지 않는다 — Human -> Zombie 전환이 곧 "내가 당했다"는 신호다.
	// 이 위젯은 로컬 플레이어 것만 들고 있으므로 여기서 내는 소리는 당사자에게만 들린다.
	//
	// 첫 호출은 기준을 잡기만 한다. BindPlayerState() 가 현재 진영으로 이 함수를 한 번 부르므로,
	// 이미 좀비인 상태에서 HUD 가 다시 묶이면(늦은 접속 · 위젯 재생성) 감염 소리가 또 난다.
	if (bHasTeamBaseline && LastLocalTeam == EFATeam::Human && NewTeam == EFATeam::Zombie)
	{
		UFAAudioSubsystem::PlayFASound(this, FAHUDAudio::InfectBlackout);
		UFAAudioSubsystem::PlayFASound(this, FAHUDAudio::StingInfected);
	}

	LastLocalTeam = NewTeam;
	bHasTeamBaseline = true;

	RefreshTeamText();
	OnLocalTeamChanged(NewTeam);
}

void UFAHUDWidget::HandleLocalScoreChanged()
{
	UpdateScoreFeedback();

	RefreshScoreTexts();
	OnLocalScoreChanged(GetLocalDisplayScore());
}

void UFAHUDWidget::UpdateScoreFeedback()
{
	const AFAPlayerState* FAPlayerState = CachedPlayerState.Get();
	if (!FAPlayerState)
	{
		return;
	}

	// 진영이 바뀌어도 같은 칸을 계속 본다 — 인간 계와 좀비 계는 동시에 늘지 않는다.
	const FFAPlayerScore& RoundScore = FAPlayerState->GetRoundScore();
	const int32 Personal = RoundScore.HumanPersonal + RoundScore.ZombiePersonal;
	const int32 Team = RoundScore.HumanTeam + RoundScore.ZombieTeam;
	const bool bVoided = RoundScore.bWasInfectedThisRound;

	// 첫 갱신은 기준을 잡기만 한다. 라운드 도중 접속해도 없던 점수가 울리지 않는다.
	const bool bHasBaseline = LastPersonalScore >= 0 && LastTeamScore >= 0;

	if (bHasBaseline)
	{
		// 감염으로 인간 팀 점수 자격을 잃는 순간. false -> true 로 바뀔 때만 울린다
		// (라운드 정산에서 true -> false 로 되돌아가는 건 초기화지 무효화가 아니다).
		if (bVoided && !bLastWasInfectedThisRound)
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(ScoreVoidedTimerHandle,
					FTimerDelegate::CreateWeakLambda(this, [this]()
					{
						UFAAudioSubsystem::PlayFASound(this, FAHUDAudio::ScoreVoided);
					}),
					ScoreVoidedDelay, /*bLoop=*/false);
			}
		}

		// 늘어날 때만. CommitRoundScore() 가 라운드 점수를 0 으로 비우는 건 소리를 낼 일이 아니다.
		//
		// "+N" 도 같은 판정을 쓴다. 라운드 점수 기준이라 정산 시점의 이동(라운드 -> 매치 누적)에는
		// 반응하지 않고, 표시 총점은 그 이동으로 변하지 않으므로 숫자와 팝업이 어긋나지 않는다.
		const int32 DisplayTotal = GetLocalDisplayScore();

		if (Personal > LastPersonalScore)
		{
			UFAAudioSubsystem::PlayFASound(this, FAHUDAudio::ScorePersonal);
			OnScorePopup(Personal - LastPersonalScore, /*bIsTeamScore=*/false, DisplayTotal);
		}
		if (Team > LastTeamScore)
		{
			UFAAudioSubsystem::PlayFASound(this, FAHUDAudio::ScoreTeam);
			OnScorePopup(Team - LastTeamScore, /*bIsTeamScore=*/true, DisplayTotal);
		}
	}

	LastPersonalScore = Personal;
	LastTeamScore = Team;
	bLastWasInfectedThisRound = bVoided;
}

//~ ---------------- 상호작용 프롬프트 ----------------

UFAInteractionComponent* UFAHUDWidget::GetLocalInteractionComponent() const
{
	const APlayerController* PC = GetOwningPlayer();
	const AFACharacter* Character = PC ? Cast<AFACharacter>(PC->GetPawn()) : nullptr;
	return Character ? Character->GetInteractionComponent() : nullptr;
}

float UFAHUDWidget::GetInteractHoldProgress() const
{
	const UFAInteractionComponent* Interaction = GetLocalInteractionComponent();
	return Interaction ? Interaction->GetHoldProgress() : 0.0f;
}

FText UFAHUDWidget::GetInteractKeyText() const
{
	const APlayerController* PC = GetOwningPlayer();
	const AFACharacter* Character = PC ? Cast<AFACharacter>(PC->GetPawn()) : nullptr;
	if (!Character || !Character->InteractAction)
	{
		return FText::GetEmpty();
	}

	const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
	const UEnhancedInputLocalPlayerSubsystem* Input =
		LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Input)
	{
		return FText::GetEmpty();
	}

	const TArray<FKey> Keys = Input->QueryKeysMappedToAction(Character->InteractAction);

	// 키보드·마우스를 먼저 고른다 — 패드가 꽂혀 있으면 그쪽이 먼저 잡혀서 "F" 자리에 패드 버튼이 뜬다.
	for (const FKey& Key : Keys)
	{
		if (!Key.IsGamepadKey())
		{
			return Key.GetDisplayName(/*bLongDisplayName=*/false);
		}
	}

	return Keys.Num() > 0 ? Keys[0].GetDisplayName(/*bLongDisplayName=*/false) : FText::GetEmpty();
}

void UFAHUDWidget::UpdateInteractPrompt()
{
	const UFAInteractionComponent* Interaction = GetLocalInteractionComponent();

	// 컴포넌트가 아직 없으면(빙의 전 · 관전) 떠 있던 프롬프트만 내린다.
	const bool bShouldShow = Interaction && Interaction->HasTarget();

	if (!bShouldShow)
	{
		if (bInteractPromptVisible)
		{
			bInteractPromptVisible = false;
			LastInteractPrompt = FText::GetEmpty();
			LastInteractHoldProgress = -1.0f;

			SetInteractKeyVisible(false);
			OnInteractPromptHidden();
		}
		return;
	}

	const FText Prompt = Interaction->GetPromptText();

	if (!bInteractPromptVisible)
	{
		bInteractPromptVisible = true;
		LastInteractPrompt = Prompt;

		// 키 표기는 뜨는 순간에 한 번만 읽는다.
		if (Text_InteractKey)
		{
			const FText KeyText = GetInteractKeyText();
			if (!KeyText.IsEmpty())
			{
				Text_InteractKey->SetText(KeyText);
			}
		}

		SetInteractKeyVisible(true);
		OnInteractPromptShown(Prompt);
	}
	else if (!Prompt.EqualTo(LastInteractPrompt))
	{
		// 대상은 그대로인데 문구만 바뀌는 경우가 흔하다 — 문 열기 <-> 문 닫기.
		LastInteractPrompt = Prompt;
		OnInteractPromptTextChanged(Prompt);
	}

	const float Progress = Interaction->GetHoldProgress();
	if (!FMath::IsNearlyEqual(Progress, LastInteractHoldProgress, 0.01f))
	{
		LastInteractHoldProgress = Progress;
		OnInteractHoldProgressChanged(Progress);
	}
}

void UFAHUDWidget::SetInteractKeyVisible(bool bVisible)
{
	// 문구(Text_InteractPrompt)의 WBP 바인딩과 같은 규칙 — 보일 때도 마우스 입력은 통과시킨다.
	const ESlateVisibility NewVisibility = bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;

	if (Text_InteractKey)
	{
		Text_InteractKey->SetVisibility(NewVisibility);
	}
	if (Image_InteractKey)
	{
		Image_InteractKey->SetVisibility(NewVisibility);
	}
}

//~ ---------------- 직결 위젯 갱신 ----------------

void UFAHUDWidget::RefreshScoreTexts()
{
	if (Text_TeamScore)
	{
		Text_TeamScore->SetText(GetLocalTeamScoreText());
	}
	if (Text_PlayerScore)
	{
		Text_PlayerScore->SetText(GetLocalPersonalScoreText());
	}
}

void UFAHUDWidget::RefreshTeamText()
{
	if (Text_Team)
	{
		Text_Team->SetText(GetLocalTeamText());
	}
}

void UFAHUDWidget::RefreshTeamCountTexts()
{
	if (Text_Survivor)
	{
		Text_Survivor->SetText(GetSurvivorCountText());
	}
	if (Text_Zombie)
	{
		Text_Zombie->SetText(GetZombieCountText());
	}
}

void UFAHUDWidget::RefreshRoundText()
{
	if (Text_Round)
	{
		Text_Round->SetText(GetRoundText());
	}
}
