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
		const TArray<FFADraftCandidate> Candidates = GenerateCandidates();

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

TArray<FFADraftCandidate> UFARuleDraftSubsystem::GenerateCandidates(int32 Count) const
{
	TArray<FName> Pool = GetDummyRulePool();
	TArray<FFADraftCandidate> Result;

	for (int32 Index = 0; Index < Count && Pool.Num() > 0; ++Index)
	{
		const int32 PickedIndex = FMath::RandRange(0, Pool.Num() - 1);

		FFADraftCandidate Candidate;
		Candidate.RuleId = Pool[PickedIndex];
		Result.Add(Candidate);

		// 같은 후보가 중복으로 안 뽑히게, 뽑은 건 풀에서 제거
		Pool.RemoveAtSwap(PickedIndex);
	}

	return Result;
}

TArray<FName> UFARuleDraftSubsystem::GetDummyRulePool() const
{
	return {
		TEXT("H01_DummyRule"),
		TEXT("H02_DummyRule"),
		TEXT("H03_DummyRule"),
		TEXT("Z01_DummyRule"),
		TEXT("Z02_DummyRule"),
		TEXT("Z03_DummyRule")
	};
}