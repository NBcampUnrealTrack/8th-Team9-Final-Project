#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FADeathMatchSubsystem.generated.h"

class AFAPlayerState;

/**
 * 1등 완전 동점자(2~3명) 대상 서든데스 데스매치 (GDD 5.4 / 5.5, MVP 제외).
 * AFAGameMode::HandleMatchHasEnded() 가 동점 판정 후 이 서브시스템을 호출한다.
 */
UCLASS()
class FALLINGAPART_API UFADeathMatchSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// 데스매치 시작. GameMode 가 1등 완전 동점자 목록(2~3명)을 넘긴다.
	void StartDeathMatch(const TArray<AFAPlayerState*>& Contenders);
};