#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FADeathMatchSubsystem.generated.h"

class AFAPlayerState;

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

	/**
	 * 이 플레이어의 데스매치 순위값. 숫자가 클수록 상위.
	 * 데스매치 미참가자는 0 — 원래 타이브레이크로 계산한 순위를 그대로 쓰면 된다.
	 */
	int32 GetDeathMatchPlacement(const AFAPlayerState* PlayerState) const;

private:
	// 1분 제한 시간 도달 — 인간(들) 승 (데스매치 사양).
	void HandleTimeExpired();

	/**
	 * 승패 확정 공통 처리.
	 * @param TopPlacement    가장 높은 순위값을 받을 플레이어들 (좀비 패배 시 인간 전원, 좀비 승리 시 좀비 혼자)
	 * @param BottomPlacement 가장 낮은 순위값을 받을 플레이어들
	 * @param MidPlacement    중간 순위값 (3인일 때 포획 안 당한 인간). 없으면 nullptr.
	 */
	void EndDeathMatch(const TArray<AFAPlayerState*>& TopPlacement, AFAPlayerState* MidPlacement,
		const TArray<AFAPlayerState*>& BottomPlacement);

	TWeakObjectPtr<AFAPlayerState> ActiveZombie;

	// 2인이면 1명, 3인이면 2명 (GDD "좀비1 vs 인간2", 권장안 · 확정 필요).
	TArray<TWeakObjectPtr<AFAPlayerState>> ActiveHumans;

	FTimerHandle DeathMatchTimerHandle;

	// 최근 데스매치 결과. 점수는 안 건드리고 순위 재정렬에만 쓴다.
	TMap<TWeakObjectPtr<AFAPlayerState>, int32> LastPlacements;
};