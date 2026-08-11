// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FAHUDWidget.h"

#include "Game/FAGameState.h"
#include "Player/FAPlayerState.h"

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
	UpdatePolledValues();
}

void UFAHUDWidget::NativeDestruct()
{
	UnbindPlayerState();
	UnbindGameState();

	Super::NativeDestruct();
}

void UFAHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 포인터 비교뿐이라 매 프레임 불러도 부담이 없움.
	RefreshBindings();
	UpdatePolledValues();
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
}

void UFAHUDWidget::UpdatePolledValues()
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
	const int32 DisplayedSeconds = FMath::Max(0, FMath::CeilToInt(SecondsRemaining));
	if (DisplayedSeconds != LastDisplayedSeconds)
	{
		LastDisplayedSeconds = DisplayedSeconds;

		const bool bIsFinalCountdown = FAGameState->IsRoundInProgress() && SecondsRemaining <= FinalCountdownSeconds;
		OnRoundTimeChanged(FormatRoundTime(SecondsRemaining), SecondsRemaining, bIsFinalCountdown);
	}
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
	OnLocalTeamChanged(NewTeam);
}

void UFAHUDWidget::HandleLocalScoreChanged()
{
	OnLocalScoreChanged(GetLocalDisplayScore());
}
