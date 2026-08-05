#include "Rule/FARuleDraftSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Player/FAPlayerState.h"
#include "TimerManager.h"

namespace FADraft
{
	static constexpr float SelectionTimeLimit = 10.0f;
}

void UFARuleDraftSubsystem::StartDraftForAllPlayers(int32 DraftIndex)
{
	// 살아있는(관전자 아닌) 모든 플레이어에게 같은 후보를 보낸다. 지금은 전원 동일 후보 — 진영별 필터는 다음 단계.
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	if (!GameState)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Draft] StartDraftForAllPlayers — DraftIndex=%d"), DraftIndex);

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		AFAPlayerState* FAPlayerState = Cast<AFAPlayerState>(PlayerState);
		if (!FAPlayerState)
		{
			continue;
		}

		// 플레이어 1명당 후보 3장을 각각 새로 뽑는다 (전원 동일 후보가 아니라 개인별 후보).
		const TArray<FFADraftCandidate> Candidates = GenerateCandidates(FAPlayerState->GetTeam(), DraftIndex);

		TWeakObjectPtr<AFAPlayerState> WeakPlayer(FAPlayerState);

		// "이 플레이어에게 지금 뭘 보냈는지" 기억해둔다 — 나중에 ConfirmDraft 검증에 쓴다.
		PendingCandidates.Add(WeakPlayer, Candidates);

		TArray<FName> CandidateIds;
		for (const FFADraftCandidate& Candidate : Candidates)
		{
			CandidateIds.Add(Candidate.RuleId);
		}

		FAPlayerState->Client_ShowDraftUI(CandidateIds);

		// 혹시 이전 드래프트의 타이머가 아직 남아있으면 지우고 새로 건다.
		if (FTimerHandle* ExistingHandle = PendingTimers.Find(WeakPlayer))
		{
			GetWorld()->GetTimerManager().ClearTimer(*ExistingHandle);
		}

		FTimerHandle NewHandle;
		FTimerDelegate TimeoutDelegate = FTimerDelegate::CreateUObject(
			this, &UFARuleDraftSubsystem::HandleDraftTimeout, WeakPlayer);

		GetWorld()->GetTimerManager().SetTimer(NewHandle, TimeoutDelegate, FADraft::SelectionTimeLimit, false);
		PendingTimers.Add(WeakPlayer, NewHandle);
	}
}

void UFARuleDraftSubsystem::ConfirmDraft(AFAPlayerState* ForPlayer, int32 SelectedIndex)
{
	if (!ForPlayer)
	{
		return;
	}

	TWeakObjectPtr<AFAPlayerState> WeakPlayer(ForPlayer);

	const TArray<FFADraftCandidate>* Candidates = PendingCandidates.Find(WeakPlayer);
	if (!Candidates)
	{
		// 이미 확정됐거나(타임아웃 포함), 애초에 후보를 보낸 적 없는 플레이어 — 무시.
		UE_LOG(LogTemp, Warning, TEXT("[Draft] ConfirmDraft — %s 에 대한 대기 중인 후보가 없음 (중복 확정 시도?)"),
			*ForPlayer->GetPlayerName());
		return;
	}

	if (!Candidates->IsValidIndex(SelectedIndex))
	{
		// 클라이언트가 잘못된(또는 조작된) 인덱스를 보낸 경우 — 서버가 반드시 걸러야 함.
		UE_LOG(LogTemp, Warning, TEXT("[Draft] ConfirmDraft — %s 가 잘못된 인덱스(%d) 전송"),
			*ForPlayer->GetPlayerName(), SelectedIndex);
		return;
	}

	const FFADraftCandidate Chosen = (*Candidates)[SelectedIndex];

	// 확정됐으니 남은 타임아웃 타이머를 끈다.
	if (FTimerHandle* Handle = PendingTimers.Find(WeakPlayer))
	{
		GetWorld()->GetTimerManager().ClearTimer(*Handle);
		PendingTimers.Remove(WeakPlayer);
	}

	PendingCandidates.Remove(WeakPlayer);

	ApplyDraftResult(ForPlayer, Chosen);
}

void UFARuleDraftSubsystem::HandleDraftTimeout(TWeakObjectPtr<AFAPlayerState> ForPlayer)
{
	if (!ForPlayer.IsValid())
	{
		// 타이머가 도는 사이에 플레이어가 접속을 끊은 경우 — 안전하게 무시.
		PendingCandidates.Remove(ForPlayer);
		PendingTimers.Remove(ForPlayer);
		return;
	}

	const TArray<FFADraftCandidate>* Candidates = PendingCandidates.Find(ForPlayer);
	if (!Candidates || Candidates->Num() == 0)
	{
		return;
	}

	// 서버 RNG로 0~2 중 무작위 확정.
	const int32 RandomIndex = FMath::RandRange(0, Candidates->Num() - 1);
	const FFADraftCandidate Chosen = (*Candidates)[RandomIndex];

	UE_LOG(LogTemp, Log, TEXT("[Draft] %s 타임아웃 — 서버가 무작위 확정: %s"),
		*ForPlayer->GetPlayerName(), *Chosen.RuleId.ToString());

	PendingCandidates.Remove(ForPlayer);
	PendingTimers.Remove(ForPlayer);

	ApplyDraftResult(ForPlayer.Get(), Chosen);
}

void UFARuleDraftSubsystem::ApplyDraftResult(AFAPlayerState* ForPlayer, const FFADraftCandidate& Chosen)
{
	if (!ForPlayer)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Draft] %s 룰 확정: %s"), *ForPlayer->GetPlayerName(), *Chosen.RuleId.ToString());

	// TODO: 다음 단계에서 실제 GE 적용(ApplyGameplayEffectToSelf), 4장 초과 시 최선입 제거 로직 추가
}

void UFARuleDraftSubsystem::StartChainDraft(AFAPlayerState* Victim, int32 RemovedRuleCount)
{
	if (!Victim)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Draft] StartChainDraft called — Victim=%s, RemovedRuleCount=%d"),
		*Victim->GetPlayerName(), RemovedRuleCount);
	
	// TODO: RemovedRuleCount 만큼 좀비 카드 연속 드래프트 (1장당 6초)
}

TArray<FFADraftCandidate> UFARuleDraftSubsystem::GenerateCandidates(EFATeam ForTeam, int32 DraftIndex, int32 Count) const
{
	// 이번 회차의 슬롯별 요구 티어 (예: 2회차 = [1, 1, 2]). GDD 4.2 티어 곡선.
	const TArray<int32> TierComposition = GetTierComposition(DraftIndex);
	const TArray<FFADummyRuleEntry> Pool = GetDummyRulePool();

	TArray<FFADraftCandidate> Result;
	TArray<FName> AlreadyPicked; // 같은 회차 안에서 같은 룰이 중복으로 안 뽑히게

	for (int32 Slot = 0; Slot < TierComposition.Num(); ++Slot)
	{
		const int32 WantedTier = TierComposition[Slot];

		// 이 진영 + 이 슬롯이 요구하는 정확한 티어 + 아직 안 뽑힌 것만 후보로 남긴다.
		TArray<FFADummyRuleEntry> SlotPool;
		for (const FFADummyRuleEntry& Entry : Pool)
		{
			if (Entry.Team == ForTeam && Entry.Tier == WantedTier && !AlreadyPicked.Contains(Entry.RuleId))
			{
				SlotPool.Add(Entry);
			}
		}

		if (SlotPool.Num() == 0)
		{
			// 더미 풀에 해당 티어가 부족해서 생기는 임시 한계 — URuleCardData 로 교체되면 해소된다.
			continue;
		}

		const FFADummyRuleEntry& Picked = SlotPool[FMath::RandRange(0, SlotPool.Num() - 1)];

		FFADraftCandidate Candidate;
		Candidate.RuleId = Picked.RuleId;
		Result.Add(Candidate);

		AlreadyPicked.Add(Picked.RuleId);
	}

	return Result;
}

TArray<FFADummyRuleEntry> UFARuleDraftSubsystem::GetDummyRulePool() const
{
	return {
			{ TEXT("H_T1_Quiet"),         EFATeam::Human, 1 },
			{ TEXT("H_T1_QuietFootstep"), EFATeam::Human, 1 },
			{ TEXT("H_T1_Footsetep"),	   EFATeam::Human, 1 },
			{ TEXT("H_T2_ExtraStamina"),  EFATeam::Human, 2 },
			{ TEXT("H_T2_Extra"),		   EFATeam::Human, 2 },
			{ TEXT("H_T2_Stamina"),       EFATeam::Human, 2 },
			{ TEXT("H_T3_SecondWind"),    EFATeam::Human, 3 },
			{ TEXT("H_T3_Wind"),          EFATeam::Human, 3 },
			{ TEXT("H_T3_Second"),        EFATeam::Human, 3 },

			{ TEXT("Z_T1_Quiet"),         EFATeam::Zombie, 1 },
			{ TEXT("Z_T1_Footstep"),      EFATeam::Zombie, 1 },
			{ TEXT("Z_T1_QuietFootstep"), EFATeam::Zombie, 1 },
			{ TEXT("Z_T2_GrabRange"),     EFATeam::Zombie, 2 },
			{ TEXT("Z_T2_Grab"),		   EFATeam::Zombie, 2 },
			{ TEXT("Z_T2_Range"),		   EFATeam::Zombie, 2 },
			{ TEXT("Z_T3_Frenzy"),        EFATeam::Zombie, 3 },
			{ TEXT("Z_T3_Frenzystep"),    EFATeam::Zombie, 3 },
			{ TEXT("Z_T3_step"),		   EFATeam::Zombie, 3 },
		};
}

TArray<int32> UFARuleDraftSubsystem::GetTierComposition(int32 DraftIndex) const
{
	// 밸런싱 워크북 "1_드래프트곡선" 시트 그대로 반영 (1회차 T1T1T1 ~ 6회차 T3T3T3).
	switch (DraftIndex)
	{
	case 1: return { 1, 1, 1 };
	case 2: return { 1, 1, 2 };
	case 3: return { 1, 2, 2 };
	case 4: return { 2, 2, 2 };
	case 5: return { 2, 2, 3 };
	default: return { 3, 3, 3 }; // 6회차
	}
}
