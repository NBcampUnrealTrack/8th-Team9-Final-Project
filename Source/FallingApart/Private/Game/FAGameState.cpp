// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/FAGameState.h"

#include "Net/UnrealNetwork.h"
#include "Player/FAPlayerState.h"

AFAGameState::AFAGameState()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AFAGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFAGameState, RoundPhase);
	DOREPLIFETIME(AFAGameState, CurrentRound);
	DOREPLIFETIME(AFAGameState, MaxRound);
	DOREPLIFETIME(AFAGameState, LastRoundResult);
	DOREPLIFETIME(AFAGameState, RoundStartServerTime);
	DOREPLIFETIME(AFAGameState, RoundDuration);
	DOREPLIFETIME(AFAGameState, DraftInterval);
	DOREPLIFETIME(AFAGameState, MaxDraftsPerRound);
	DOREPLIFETIME(AFAGameState, DraftsCompleted);
	DOREPLIFETIME(AFAGameState, SurvivorCount);
	DOREPLIFETIME(AFAGameState, ZombieCount);
}

float AFAGameState::GetRoundTimeRemaining() const
{
	switch (RoundPhase)
	{
	case EFARoundPhase::InProgress:
		return FMath::Max(0.0f, RoundStartServerTime + RoundDuration - GetServerWorldTimeSeconds());

	case EFARoundPhase::RoundStarting:
		return RoundDuration;

	default:
		return 0.0f;
	}
}

float AFAGameState::GetRoundElapsedTime() const
{
	if (RoundPhase != EFARoundPhase::InProgress)
	{
		return 0.0f;
	}

	return FMath::Clamp(GetServerWorldTimeSeconds() - RoundStartServerTime, 0.0f, RoundDuration);
}

float AFAGameState::GetTimeToNextDraft() const
{
	if (RoundPhase != EFARoundPhase::InProgress || DraftsCompleted >= MaxDraftsPerRound)
	{
		return -1.0f;
	}

	const float NextDraftTime = RoundStartServerTime + DraftInterval * static_cast<float>(DraftsCompleted + 1);
	return FMath::Max(0.0f, NextDraftTime - GetServerWorldTimeSeconds());
}

void AFAGameState::GetPlayerStatesByTeam(EFATeam InTeam, TArray<AFAPlayerState*>& OutPlayerStates) const
{
	OutPlayerStates.Reset();

	for (APlayerState* PlayerState : PlayerArray)
	{
		AFAPlayerState* FAPlayerState = Cast<AFAPlayerState>(PlayerState);
		if (FAPlayerState && FAPlayerState->GetTeam() == InTeam)
		{
			OutPlayerStates.Add(FAPlayerState);
		}
	}
}

void AFAGameState::GetRankedPlayerStates(TArray<AFAPlayerState*>& OutRanked) const
{
	OutRanked.Reset();

	for (APlayerState* PlayerState : PlayerArray)
	{
		if (AFAPlayerState* FAPlayerState = Cast<AFAPlayerState>(PlayerState))
		{
			if (!FAPlayerState->IsOnlyASpectator())
			{
				OutRanked.Add(FAPlayerState);
			}
		}
	}

	OutRanked.Sort([](const AFAPlayerState& A, const AFAPlayerState& B)
	{
		return CompareRank(A, B);
	});
}

bool AFAGameState::CompareRank(const AFAPlayerState& A, const AFAPlayerState& B)
{
	// 1차 — 누적 총점
	const int32 TotalA = A.GetMatchTotalScore();
	const int32 TotalB = B.GetMatchTotalScore();
	if (TotalA != TotalB)
	{
		return TotalA > TotalB;
	}

	// 2차 — 진영 기여도. 주 진영이 같으면 절대값, 다르면 기여 비율로 비교 (GDD 5.3).
	if (A.GetMainSide() == B.GetMainSide())
	{
		const int32 SideScoreA = A.GetMainSideScore();
		const int32 SideScoreB = B.GetMainSideScore();
		if (SideScoreA != SideScoreB)
		{
			return SideScoreA > SideScoreB;
		}
	}
	else
	{
		const float RatioA = A.GetMainSideRatio();
		const float RatioB = B.GetMainSideRatio();
		if (!FMath::IsNearlyEqual(RatioA, RatioB, 0.0001f))
		{
			return RatioA > RatioB;
		}
	}

	// 3차 — 누적 생존 시간
	const float SurvivalA = A.GetMatchSurvivalTime();
	const float SurvivalB = B.GetMatchSurvivalTime();
	if (!FMath::IsNearlyEqual(SurvivalA, SurvivalB, 0.01f))
	{
		return SurvivalA > SurvivalB;
	}

	// 4차 — 완전 동점. (공동 순위).
	return false;
}

bool AFAGameState::AreFullyTied(const AFAPlayerState& A, const AFAPlayerState& B)
{
	return !CompareRank(A, B) && !CompareRank(B, A);
}

void AFAGameState::SetRoundPhase(EFARoundPhase NewPhase)
{
	if (!ensure(HasAuthority()) || RoundPhase == NewPhase)
	{
		return;
	}

	RoundPhase = NewPhase;
	OnRep_RoundPhase();
}

void AFAGameState::SetCurrentRound(int32 InRound)
{
	if (!ensure(HasAuthority()))
	{
		return;
	}

	CurrentRound = InRound;
}

void AFAGameState::SetMaxRound(int32 InMaxRound)
{
	if (!ensure(HasAuthority()))
	{
		return;
	}

	MaxRound = FMath::Max(1, InMaxRound);
}

void AFAGameState::SetLastRoundResult(EFARoundResult InResult)
{
	if (!ensure(HasAuthority()))
	{
		return;
	}

	LastRoundResult = InResult;

	if (InResult != EFARoundResult::None)
	{
		Multicast_RoundEnded(CurrentRound, InResult);
	}
}

void AFAGameState::StartRoundTimer(float ServerStartTime, float InRoundDuration, float InDraftInterval, int32 InMaxDraftsPerRound)
{
	if (!ensure(HasAuthority()))
	{
		return;
	}

	RoundStartServerTime = ServerStartTime;
	RoundDuration = FMath::Max(1.0f, InRoundDuration);
	DraftInterval = FMath::Max(1.0f, InDraftInterval);
	MaxDraftsPerRound = FMath::Max(0, InMaxDraftsPerRound);
	DraftsCompleted = 0;
}

void AFAGameState::SetDraftsCompleted(int32 InCount)
{
	if (!ensure(HasAuthority()))
	{
		return;
	}

	DraftsCompleted = FMath::Clamp(InCount, 0, MaxDraftsPerRound);
}

void AFAGameState::SetTeamCounts(int32 InSurvivorCount, int32 InZombieCount)
{
	if (!ensure(HasAuthority()))
	{
		return;
	}

	const bool bChanged = (SurvivorCount != InSurvivorCount) || (ZombieCount != InZombieCount);

	SurvivorCount = InSurvivorCount;
	ZombieCount = InZombieCount;

	if (bChanged)
	{
		OnTeamCountsChanged.Broadcast();
	}
}

void AFAGameState::NotifyDraftRoundStarted(int32 DraftIndex)
{
	if (!ensure(HasAuthority()))
	{
		return;
	}

	Multicast_DraftRoundStarted(DraftIndex);
}

void AFAGameState::Multicast_DraftRoundStarted_Implementation(int32 DraftIndex)
{
	OnDraftRoundStarted.Broadcast(DraftIndex);
}

void AFAGameState::Multicast_RoundEnded_Implementation(int32 RoundIndex, EFARoundResult Result)
{
	OnRoundEnded.Broadcast(RoundIndex, Result);
}

void AFAGameState::OnRep_RoundPhase()
{
	OnRoundPhaseChanged.Broadcast(RoundPhase);
}

void AFAGameState::OnRep_SurvivorCount()
{
	OnTeamCountsChanged.Broadcast();
}

void AFAGameState::OnRep_ZombieCount()
{
	OnTeamCountsChanged.Broadcast();
}
