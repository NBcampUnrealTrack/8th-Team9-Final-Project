// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FAHUDWidget.h"

#include "Audio/FAAudioSubsystem.h"
#include "Engine/World.h"
#include "Game/FAGameState.h"
#include "Player/FAPlayerState.h"
#include "TimerManager.h"

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

void UFAHUDWidget::RefreshBindings()
{
	const UWorld* World = GetWorld();
	AFAGameState* CurrentGameState = World ? World->GetGameState<AFAGameState>() : nullptr;
	if (CurrentGameState != CachedGameState.Get())
	{
		UnbindGameState();
		BindGameState(CurrentGameState);
	}

	AFAPlayerState* CurrentPlayerState = Cast<AFAPlayerState>(GetOwningPlayerState());
	if (CurrentPlayerState != CachedPlayerState.Get())
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
	if (AFAGameState* FAGameState = CachedGameState.Get())
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
	if (AFAPlayerState* FAPlayerState = CachedPlayerState.Get())
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

	OnLocalTeamChanged(NewTeam);
}

void UFAHUDWidget::HandleLocalScoreChanged()
{
	UpdateScoreAudio();

	OnLocalScoreChanged(GetLocalDisplayScore());
}

void UFAHUDWidget::UpdateScoreAudio()
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
		if (Personal > LastPersonalScore)
		{
			UFAAudioSubsystem::PlayFASound(this, FAHUDAudio::ScorePersonal);
		}
		if (Team > LastTeamScore)
		{
			UFAAudioSubsystem::PlayFASound(this, FAHUDAudio::ScoreTeam);
		}
	}

	LastPersonalScore = Personal;
	LastTeamScore = Team;
	bLastWasInfectedThisRound = bVoided;
}
