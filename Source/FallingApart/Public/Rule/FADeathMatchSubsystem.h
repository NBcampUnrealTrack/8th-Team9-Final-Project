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
	
	/**
	 * 데스매치 중 좀비가 인간을 포획했을 때 호출 (GameMode 의 감염 처리 경로에서 연동 필요).
	 * 1분이 지나기 전에 호출되면 좀비 승으로 즉시 종료한다.
	 */
	void NotifyContenderCaught(AFAPlayerState* CaughtHuman);
	
private:
	// 1분 제한 시간 도달 — 인간 승 (데스매치 사양).
	void HandleTimeExpired();

	// 승패 확정 처리 — 타임아웃(인간 승)과 포획(좀비 승)이 공유하는 공통 로직.
	void EndDeathMatch(AFAPlayerState* Winner, AFAPlayerState* Loser);

	TWeakObjectPtr<AFAPlayerState> ActiveHuman;
	TWeakObjectPtr<AFAPlayerState> ActiveZombie;

	FTimerHandle DeathMatchTimerHandle;
};