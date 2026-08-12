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
	UFAGA_RescueContest();
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

private:
	UPROPERTY()
	TWeakObjectPtr<AFACharacter> TargetZombie;

	UPROPERTY()
	TWeakObjectPtr<AFACharacter> Rescuer;

	UPROPERTY()
	TWeakObjectPtr<AFACharacter> GrabbedVictim;

	/** 어빌리티 종료 시 상태 일괄 해제를 위한 GE 핸들 맵 */
	TMap<AFACharacter*, FActiveGameplayEffectHandle> ContestingHandles;
};
