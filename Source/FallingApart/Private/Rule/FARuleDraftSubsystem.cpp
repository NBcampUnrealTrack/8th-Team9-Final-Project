#include "Rule/FARuleDraftSubsystem.h"

#include "Player/FAPlayerState.h"

void UFARuleDraftSubsystem::StartDraftForAllPlayers(int32 DraftIndex)
{
	UE_LOG(LogTemp, Log, TEXT("[Draft] StartDraftForAllPlayers called — DraftIndex=%d"), DraftIndex);
}

void UFARuleDraftSubsystem::StartChainDraft(AFAPlayerState* Victim, int32 RemovedRuleCount)
{
	if (!Victim)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Draft] StartChainDraft called — Victim=%s, RemovedRuleCount=%d"),
		*Victim->GetPlayerName(), RemovedRuleCount);
}