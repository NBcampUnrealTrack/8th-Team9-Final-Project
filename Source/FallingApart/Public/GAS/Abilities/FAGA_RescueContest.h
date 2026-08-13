#pragma once

#include "CoreMinimal.h"
#include "GAS/FAGameplayAbility.h"
#include "FAGA_RescueContest.generated.h"

class AFACharacter;
class UGameplayEffect;

/**
 * 좀비 서버에서 1개만 실행되어 3명(좀비, 인질, 구조자)의 줄다리기를 통제하는 어빌리티.
 * 게이지 0/100 도달 또는 3초 타임아웃 시 승패를 판정한다 (설계문서 §3.3).
 */
UCLASS()
class FALLINGAPART_API UFAGA_RescueContest : public UFAGameplayAbility
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** QTE 중 부여할 상태 (State.Contesting + 이동/행동 불가) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Effects")
	TSubclassOf<UGameplayEffect> ContestingEffectClass;

	/** 인간 승리 시 좀비에게 적용할 기절 GE (기본 2초 / Z30 룰 적용 시 8초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Effects")
	TSubclassOf<UGameplayEffect> ZombieStunEffectClass;

private:
	//~ GAS Tasks 콜백
	UFUNCTION()
	void OnHumanWin();

	UFUNCTION()
	void OnZombieWin();

	UFUNCTION()
	void OnTimeOut();

	/** 대결 결과 확정 및 처리 */
	void ResolveContestOutcome(bool bHumanWon);

	/**
	 * 줄다리기 게이지가 움직일 때마다 잡힌 인간 · 좀비 양쪽 HUD 로 흘려보낸다.
	 */
	void HandleContestGaugeChanged(const struct FOnAttributeChangeData& Data);

	/** 참가자 전원의 포획 게이지에 줄다리기 진행도를 써 넣는다. */
	void BroadcastResistRatio(float Ratio);

private:
	UPROPERTY()
	TWeakObjectPtr<AFACharacter> TargetZombie;

	UPROPERTY()
	TWeakObjectPtr<AFACharacter> Rescuer;

	UPROPERTY()
	TWeakObjectPtr<AFACharacter> GrabbedVictim;

	/** 어빌리티 종료 시 상태 일괄 해제를 위한 GE 핸들 맵 */
	TMap<AFACharacter*, FActiveGameplayEffectHandle> ContestingHandles;

	/** ContestGauge 변경 구독. EndAbility 에서 반드시 해제한다. */
	FDelegateHandle ContestGaugeChangedHandle;

	/**
	 * 승패를 이미 확정했는가.
	 * 완료 경로가 게이지 100 · 게이지 0 · 3초 타임아웃 세 갈래라 같은 프레임에 둘이 겹칠 수 있고,
	 * 그러면 구출 점수가 두 번 붙는다.
	 */
	bool bOutcomeResolved = false;
};
