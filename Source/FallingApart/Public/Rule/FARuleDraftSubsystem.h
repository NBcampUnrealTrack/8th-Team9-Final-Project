#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FARuleDraftSubsystem.generated.h"

class AFAPlayerState;

/*
 * 30초 주기 룰 드래프트와, 감염 시 연속 드래프트를 담당하는 서버 권한 서브시스템.
 * AFAGameMode 가 라운드 진행 중 이 서브시스템의 함수를 호출.
 */
UCLASS()
class FALLINGAPART_API UFARuleDraftSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// 30초 주기 정규 드래프트. GameMode::OnDraftTimer() 가 호출.	
	void StartDraftForAllPlayers(int32 DraftIndex);

	// 감염 시 연속 드래프트. GameMode::HandleInfection() 이 호출.
	void StartChainDraft(AFAPlayerState* Victim, int32 RemovedRuleCount);
};