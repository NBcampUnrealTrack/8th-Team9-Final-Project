#include "Rule/FADeathMatchSubsystem.h"

#include "Player/FAPlayerState.h"

void UFADeathMatchSubsystem::StartDeathMatch(const TArray<AFAPlayerState*>& Contenders)
{
	UE_LOG(LogTemp, Log, TEXT("[DeathMatch] StartDeathMatch called — 참가자 %d명"), Contenders.Num());

	for (AFAPlayerState* Contender : Contenders)
	{
		if (Contender)
		{
			UE_LOG(LogTemp, Log, TEXT("[DeathMatch]   - %s"), *Contender->GetPlayerName());
		}
	}

	// TODO: 다음 단계에서 진영 랜덤 배정(1명 인간, 1명 좀비), 1분 타이머, 승패 판정 추가
}