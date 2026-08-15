#include "Rule/FARuleDraftSubsystem.h"

#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GAS/FAAbilitySystemComponent.h"
#include "Game/FAGameState.h"
#include "GameFramework/GameStateBase.h"
#include "Player/FACharacter.h"
#include "Player/FAPlayerState.h"
#include "Rule/FARuleSettings.h"
#include "Rule/FARuleTypes.h"
#include "TimerManager.h"

bool UFARuleDraftSubsystem::IsDraftAllowed() const
{
	const AFAGameState* FAGameState = GetWorld() ? GetWorld()->GetGameState<AFAGameState>() : nullptr;
	if (!FAGameState)
	{
		return false;
	}

	// 라운드가 실제로 굴러가는 중에만 룰을 쥐여준다.
	return FAGameState->IsRoundInProgress();
}

void UFARuleDraftSubsystem::CancelAllPendingDrafts()
{
	if (PendingCandidates.Num() == 0 && PendingTimers.Num() == 0 && PendingChainCounts.Num() == 0)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Draft] 라운드 종료 — 대기 중인 드래프트 %d건을 취소한다"), PendingCandidates.Num());

	if (UWorld* World = GetWorld())
	{
		for (TPair<TWeakObjectPtr<AFAPlayerState>, FTimerHandle>& Pair : PendingTimers)
		{
			World->GetTimerManager().ClearTimer(Pair.Value);
		}
	}

	// 선택창이 떠 있는 클라이언트에게 닫으라고 알린다.
	for (const TPair<TWeakObjectPtr<AFAPlayerState>, TArray<FFADraftCandidate>>& Pair : PendingCandidates)
	{
		if (AFAPlayerState* PlayerState = Pair.Key.Get())
		{
			PlayerState->Client_HideDraftUI();
		}
	}

	PendingTimers.Empty();
	PendingCandidates.Empty();
	PendingChainCounts.Empty();
}

void UFARuleDraftSubsystem::StartDraftForAllPlayers(int32 DraftIndex)
{
	// 살아있는(관전자 아닌) 모든 플레이어에게 같은 후보를 보낸다. 지금은 전원 동일 후보 — 진영별 필터는 다음 단계.
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	if (!GameState)
	{
		return;
	}

	if (!IsDraftAllowed())
	{
		UE_LOG(LogTemp, Log, TEXT("[Draft] 데스매치·매치 종료 중이라 정규 드래프트를 돌리지 않는다 (DraftIndex=%d)"), DraftIndex);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Draft] StartDraftForAllPlayers — DraftIndex=%d"), DraftIndex);

	// 코드 재컴파일 없이 Project Settings → Falling Apart | Rule 에서 바꿀 수 있게 설정값으로 조회한다.
	const float SelectionTimeLimit = UFARuleSettings::GetSelectionTimeLimit();

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		AFAPlayerState* FAPlayerState = Cast<AFAPlayerState>(PlayerState);
		if (!FAPlayerState)
		{
			continue;
		}

		// 이미 걸려 있는 룰은 후보에서 뺀다 — 뽑아도 ApplyRuleCard 가 거부해 선택이 통째로 낭비된다.
		TArray<FName> HeldRuleIds;
		if (const AFACharacter* Character = Cast<AFACharacter>(FAPlayerState->GetPawn()))
		{
			if (const UFAAbilitySystemComponent* ASC = Character->GetFAAbilitySystemComponent())
			{
				HeldRuleIds = ASC->GetActiveRuleIds();
			}
		}

		// 플레이어 1명당 후보 3장을 각각 새로 뽑는다 (전원 동일 후보가 아니라 개인별 후보).
		const TArray<FFADraftCandidate> Candidates = GenerateCandidates(FAPlayerState->GetTeam(), DraftIndex, HeldRuleIds);
		if (Candidates.Num() == 0)
		{
			// 이 진영에 뽑을 카드가 남지 않았다. 빈 창을 띄우거나 빈 타임아웃을 걸지 않는다.
			UE_LOG(LogTemp, Warning, TEXT("[Draft] %s 에게 줄 후보가 없다 (진영=%d, 보유=%d장)"),
				*FAPlayerState->GetPlayerName(), static_cast<int32>(FAPlayerState->GetTeam()), HeldRuleIds.Num());
			continue;
		}

		SendCandidatesToPlayer(FAPlayerState, Candidates, SelectionTimeLimit);
	}
}

void UFARuleDraftSubsystem::SendCandidatesToPlayer(AFAPlayerState* ForPlayer, const TArray<FFADraftCandidate>& Candidates, float TimeLimit)
{
	if (!ForPlayer || Candidates.Num() == 0)
	{
		return;
	}

	TWeakObjectPtr<AFAPlayerState> WeakPlayer(ForPlayer);

	// "이 플레이어에게 지금 뭘 보냈는지" 기억해둔다 — 나중에 ConfirmDraft 검증에 쓴다.
	PendingCandidates.Add(WeakPlayer, Candidates);

	TArray<FName> CandidateIds;
	for (const FFADraftCandidate& Candidate : Candidates)
	{
		CandidateIds.Add(Candidate.RuleId);
	}

	ForPlayer->Client_ShowDraftUI(CandidateIds, TimeLimit);

	// 혹시 이전 드래프트의 타이머가 아직 남아있으면 지우고 새로 건다.
	if (FTimerHandle* ExistingHandle = PendingTimers.Find(WeakPlayer))
	{
		GetWorld()->GetTimerManager().ClearTimer(*ExistingHandle);
	}

	FTimerHandle NewHandle;
	FTimerDelegate TimeoutDelegate = FTimerDelegate::CreateUObject(
		this, &UFARuleDraftSubsystem::HandleDraftTimeout, WeakPlayer);

	GetWorld()->GetTimerManager().SetTimer(NewHandle, TimeoutDelegate, TimeLimit, false);
	PendingTimers.Add(WeakPlayer, NewHandle);
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
		PendingChainCounts.Remove(ForPlayer);
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

	const AFACharacter* Character = Cast<AFACharacter>(ForPlayer->GetPawn());
	if (!Character)
	{
		// 관전 중이거나 폰이 아직 없는 상태. 카드는 버려진다 — 드래프트는 라운드 진행 중에만 돈다.
		UE_LOG(LogTemp, Warning, TEXT("[Draft] %s 의 캐릭터가 없어 룰을 적용하지 못했다"), *ForPlayer->GetPlayerName());

		// 연속 드래프트 중이었다면 여기서 끊는다. 안 지우면 남은 장수가 표에 영원히 남는다.
		PendingChainCounts.Remove(TWeakObjectPtr<AFAPlayerState>(ForPlayer));
		return;
	}

	if (UFAAbilitySystemComponent* ASC = Character->GetFAAbilitySystemComponent())
	{
		// 상한(4장) 초과 시 최선입 제거까지 ASC 가 알아서 한다.
		ASC->ApplyRuleCard(Chosen.RuleId);
	}

	// 연속 드래프트 중이었다면 여기서 다음 장을 이어간다.
	ContinueChainDraftIfPending(ForPlayer);
}

void UFARuleDraftSubsystem::ContinueChainDraftIfPending(AFAPlayerState* ForPlayer)
{
	TWeakObjectPtr<AFAPlayerState> WeakPlayer(ForPlayer);

	const int32* RemainingPtr = PendingChainCounts.Find(WeakPlayer);
	if (RemainingPtr == nullptr)
	{
		// 정규 드래프트였다. 이어갈 것이 없다.
		return;
	}

	const int32 Remaining = *RemainingPtr - 1;
	if (Remaining <= 0)
	{
		PendingChainCounts.Remove(WeakPlayer);
		UE_LOG(LogTemp, Log, TEXT("[Draft] %s 연속 드래프트 종료"), *ForPlayer->GetPlayerName());
		return;
	}

	PendingChainCounts.Add(WeakPlayer, Remaining);

	// 방금 고른 카드가 뭔지 보고 넘어가도록 한 박자 둔다.
	FTimerDelegate NextDelegate = FTimerDelegate::CreateUObject(
		this, &UFARuleDraftSubsystem::ProcessNextChainDraft, WeakPlayer, Remaining);

	FTimerHandle Handle;
	GetWorld()->GetTimerManager().SetTimer(Handle, NextDelegate, UFARuleSettings::GetChainDraftInterval(), false);
}

void UFARuleDraftSubsystem::StartChainDraft(AFAPlayerState* Victim, int32 RemovedRuleCount)
{
	if (!Victim || RemovedRuleCount <= 0 || !IsDraftAllowed())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Draft] StartChainDraft called — Victim=%s, RemovedRuleCount=%d"),
		*Victim->GetPlayerName(), RemovedRuleCount);

	// 남은 장수를 기록해두면 ApplyDraftResult 가 확정될 때마다 알아서 다음 장을 이어간다.
	PendingChainCounts.Add(TWeakObjectPtr<AFAPlayerState>(Victim), RemovedRuleCount);

	ProcessNextChainDraft(TWeakObjectPtr<AFAPlayerState>(Victim), RemovedRuleCount);
}

TArray<FFADraftCandidate> UFARuleDraftSubsystem::GenerateCandidates(EFATeam ForTeam, int32 DraftIndex, const TArray<FName>& ExcludeRuleIds) const
{
	// 이번 회차의 슬롯별 요구 티어 (예: 2회차 = [1, 1, 2]). GDD 4.2 티어 곡선.
	const TArray<int32> TierComposition = GetTierComposition(DraftIndex);
	const TArray<FFADummyRuleEntry> Pool = GetRulePool();

	TArray<FFADraftCandidate> Result;
	TArray<FName> AlreadyPicked; // 같은 회차 안에서 같은 룰이 중복으로 안 뽑히게

	// 이 진영 + 지정 티어 + 아직 안 뽑혔고 보유 중도 아닌 카드만 모은다.
	auto CollectTier = [&](int32 Tier, TArray<FFADummyRuleEntry>& OutPool)
	{
		for (const FFADummyRuleEntry& Entry : Pool)
		{
			if (Entry.Team == ForTeam
				&& Entry.Tier == Tier
				&& !AlreadyPicked.Contains(Entry.RuleId)
				&& !ExcludeRuleIds.Contains(Entry.RuleId))
			{
				OutPool.Add(Entry);
			}
		}
	};

	for (int32 Slot = 0; Slot < TierComposition.Num(); ++Slot)
	{
		const int32 WantedTier = TierComposition[Slot];

		TArray<FFADummyRuleEntry> SlotPool;
		CollectTier(WantedTier, SlotPool);

		// 요구 티어가 바닥나면 가까운 티어에서 대신 채운다.
		// 카드가 14종을 다 채우기 전까지는 이 폴백이 없으면 후보가 1~2장으로 줄어든다.
		for (int32 Offset = 1; Offset <= 2 && SlotPool.Num() == 0; ++Offset)
		{
			CollectTier(WantedTier - Offset, SlotPool);
			CollectTier(WantedTier + Offset, SlotPool);
		}

		if (SlotPool.Num() == 0)
		{
			// 이 진영에 줄 수 있는 카드가 아예 남지 않았다 — 후보가 3장 미만이 된다.
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

TArray<FFADummyRuleEntry> UFARuleDraftSubsystem::GetRulePool() const
{
	const UDataTable* Table = UFARuleSettings::GetRuleCardTable();
	if (!Table)
	{
		return GetDummyRulePool();
	}

	TArray<FFADummyRuleEntry> Pool;

	for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
	{
		const FFARuleCardRow* Row = reinterpret_cast<const FFARuleCardRow*>(Pair.Value);
		if (!Row || !Row->IsDraftable())
		{
			// GE 애셋을 아직 안 만든 행은 자동으로 빠진다 — 표를 미리 다 채워둬도 안전하다.
			continue;
		}

		FFADummyRuleEntry Entry;
		Entry.RuleId = Pair.Key;
		Entry.Team = Row->Team;
		Entry.Tier = static_cast<int32>(Row->Tier);
		Pool.Add(Entry);
	}

	if (Pool.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Draft] DT 는 있으나 뽑을 수 있는 카드가 0장이다 — 더미 풀로 폴백"));
		return GetDummyRulePool();
	}

	return Pool;
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

void UFARuleDraftSubsystem::ProcessNextChainDraft(TWeakObjectPtr<AFAPlayerState> ForPlayer, int32 RemainingCount)
{
	if (RemainingCount <= 0 || !ForPlayer.IsValid())
	{
		// 타이머가 도는 사이 접속이 끊겼거나, 다 뽑았으면 조용히 종료.
		return;
	}

	if (!IsDraftAllowed())
	{
		// 카드 사이 간격 동안 매치가 끝났다면 남은 장수는 버린다.
		UE_LOG(LogTemp, Log, TEXT("[Draft] 매치가 끝나 %s 의 연속 드래프트를 중단한다 (남은 %d장)"),
			*ForPlayer->GetPlayerName(), RemainingCount);
		PendingChainCounts.Remove(ForPlayer);
		return;
	}

	AFAPlayerState* PlayerState = ForPlayer.Get();

	const AFACharacter* Character = Cast<AFACharacter>(PlayerState->GetPawn());
	UFAAbilitySystemComponent* ASC = Character ? Character->GetFAAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Draft] %s 의 캐릭터/ASC 가 없어 연속 드래프트를 중단한다 (남은 %d장)"),
			*PlayerState->GetPlayerName(), RemainingCount);
		PendingChainCounts.Remove(ForPlayer);
		return;
	}

	const TArray<FFADraftCandidate> Candidates = GenerateChainCandidates(ASC->GetActiveRuleIds());
	if (Candidates.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Draft] %s 에게 줄 좀비 룰이 남지 않아 연속 드래프트를 중단한다 (남은 %d장)"),
			*PlayerState->GetPlayerName(), RemainingCount);
		PendingChainCounts.Remove(ForPlayer);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Draft] %s 연속 드래프트 후보 %d장 전송 (남은 %d장)"),
		*PlayerState->GetPlayerName(), Candidates.Num(), RemainingCount);

	// 정규 드래프트와 같은 경로다 — 본인이 고르고, 안 고르면 타임아웃 때 서버가 무작위 확정한다.
	// 확정되면 ApplyDraftResult → ContinueChainDraftIfPending 이 다음 장을 이어간다.
	SendCandidatesToPlayer(PlayerState, Candidates, UFARuleSettings::GetChainSelectionTimeLimit());
}

TArray<FFADraftCandidate> UFARuleDraftSubsystem::GenerateChainCandidates(const TArray<FName>& HeldRuleIds) const
{
	// 연속 드래프트는 티어 곡선을 타지 않는다
	TArray<FFADummyRuleEntry> Pool;
	for (const FFADummyRuleEntry& Entry : GetRulePool())
	{
		if (Entry.Team == EFATeam::Zombie && !HeldRuleIds.Contains(Entry.RuleId))
		{
			Pool.Add(Entry);
		}
	}

	// 정규 드래프트와 같은 3장. 풀이 모자라면 있는 만큼만 보낸다.
	const int32 WantedCount = FMath::Min(3, Pool.Num());

	TArray<FFADraftCandidate> Result;
	for (int32 Index = 0; Index < WantedCount; ++Index)
	{
		const int32 PickedIndex = FMath::RandRange(0, Pool.Num() - 1);

		FFADraftCandidate Candidate;
		Candidate.RuleId = Pool[PickedIndex].RuleId;
		Result.Add(Candidate);

		// 같은 회차 안에서 같은 카드가 두 번 나오지 않게 뽑은 것은 빼낸다.
		Pool.RemoveAtSwap(PickedIndex);
	}

	return Result;
}
