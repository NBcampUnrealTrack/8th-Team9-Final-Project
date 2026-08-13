#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FADeathMatchSubsystem.generated.h"

class AFAPlayerState;
class APlayerStart;

/**
 * 1등 완전 동점자(2~3명) 대상 서든데스 데스매치 (GDD 5.4 / 5.5, MVP 제외).
 * AFAGameMode::HandleMatchHasEnded() 가 동점 판정 후 이 서브시스템을 호출한다.
 *
 * 아레나(1층 제한)는 맵에 미리 배치된 액터를 태그로 찾아 토글하는 방식.
 * 최종 순위는 AFAPlayerState::SetDeathMatchPlacement() 로 기록하고,
 * AFAGameState::CompareRank() 4차(완전 동점 단계)가 이 값을 읽어서 승부를 가른다.
 *
 * 3인 데스매치(좀비1 vs 인간2)는 GDD 상 "권장안 · 확정 필요" 항목 — 아래 승패/순위 해석은
 * 기획 확정 전까지 쓰는 임시 규칙이다.
 */
UCLASS()
class FALLINGAPART_API UFADeathMatchSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// 데스매치 시작. GameMode 가 1등 완전 동점자 목록(2~3명)을 넘긴다.
	void StartDeathMatch(const TArray<AFAPlayerState*>& Contenders);

	/**
	 * 데스매치 중 좀비가 인간을 포획했을 때 호출 (AFAGameMode::HandleInfection() 이 페이즈를 보고 분기).
	 * 1분이 지나기 전에 호출되면 좀비 승으로 즉시 종료.
	 */
	void NotifyContenderCaught(AFAPlayerState* CaughtHuman);

	/** 데스매치가 진행 중인가 (좀비·인간이 최소 1명씩 확정되어 타이머가 도는 상태). */
	bool IsDeathMatchActive() const { return ActiveZombie.IsValid() && ActiveHumans.Num() > 0; }

	/**
	 * 1층 봉쇄 액터를 켜고 끈다. 서버·클라이언트 각자 자기 월드에 대해 호출한다
	 * (AFAGameState::OnRep_DeathMatchArenaActive 가 호출부).
	 * 캐릭터 이동이 클라이언트 예측이라 봉쇄가 서버에만 있으면 벽을 뚫었다 되돌아온다.
	 */
	static void ApplyArenaBlockers(UWorld* World, bool bEnabled);

	/**
	 * PlayerStart 가 이 태그에 해당하는가.
	 * Player Start Tag 프로퍼티와 액터 태그(Actor Tags) 둘 다 인정한다 — 그레이박스 레벨이
	 * 액터 태그로 스폰을 구분해 두었기 때문에 어느 쪽으로 달아도 동작해야 한다.
	 */
	static bool PlayerStartHasTag(const APlayerStart* PlayerStart, const FName& Tag);

	/** 데스매치 전용 스폰인가. 일반 라운드 스폰 후보에서 빼는 데 쓴다. */
	static bool IsArenaPlayerStart(const APlayerStart* PlayerStart);

protected:
	/** 레벨에 배치된 봉쇄 액터는 평소에 꺼져 있어야 한다. 서버·클라이언트 양쪽에서 돈다. */
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	// 1분 제한 시간 도달 — 인간(들) 승 (데스매치 사양).
	void HandleTimeExpired();

	/**
	 * 승패 확정 공통 처리. 순위값은 낮을수록 상위(1등=1)로 AFAPlayerState::SetDeathMatchPlacement() 에 기록한다.
	 * @param TopPlacement    1등 그룹 (좀비 패배 시 생존 인간 전원, 좀비 승리 시 좀비 혼자)
	 * @param MidPlacement    2등 (3인일 때 포획 안 당한 인간). 없으면 nullptr.
	 * @param BottomPlacement 가장 낮은 순위값을 받을 플레이어들
	 */
	void EndDeathMatch(const TArray<AFAPlayerState*>& TopPlacement, AFAPlayerState* MidPlacement,
		const TArray<AFAPlayerState*>& BottomPlacement);

	/**
	 * 데스매치를 열지 못하고 물러날 때. 매치를 정상 종료 상태로 되돌린다.
	 * 이 경로가 없으면 GameMode 가 이미 켜둔 DeathMatch 페이즈에 매치가 영원히 멈춘다.
	 */
	void AbortDeathMatch(const FString& Reason);

	/** 참가자를 1층 전용 스폰으로 옮기고 봉쇄를 켠다. 보유 룰도 여기서 전부 걷어낸다. */
	void EnterArena(AFAPlayerState* Zombie, const TArray<AFAPlayerState*>& Humans);

	/** 봉쇄를 끄고 타이머·참가자 캐시를 비운다. 정상 종료·중단 양쪽에서 호출. */
	void LeaveArena();

	/**
	 * 데스매치 전용 스폰 지점을 진영별로 나눠 담는다.
	 * 진영 태그(Human / Zombie)가 없는 스폰은 OutAnyStarts 로 가고, 모자란 쪽을 채우는 데 쓰인다.
	 */
	void GatherArenaStarts(TArray<AActor*>& OutZombieStarts, TArray<AActor*>& OutHumanStarts,
		TArray<AActor*>& OutAnyStarts) const;

	TWeakObjectPtr<AFAPlayerState> ActiveZombie;

	// 2인이면 1명, 3인이면 2명 (GDD "좀비1 vs 인간2", 권장안 · 확정 필요).
	TArray<TWeakObjectPtr<AFAPlayerState>> ActiveHumans;

	FTimerHandle DeathMatchTimerHandle;
};
