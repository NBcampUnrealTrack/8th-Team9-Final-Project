#pragma once

#include "CoreMinimal.h"
#include "GAS/FAGameplayAbility.h"
#include "FAGA_ContestMash.generated.h"

class AFACharacter;

/**
 * 스페이스바 입력 시 발동하는 QTE 연타 어빌리티.
 * 진영에 따라 가중치를 계산해 방장(좀비)의 ContestGauge 를 조작한다.
 */
UCLASS()
class FALLINGAPART_API UFAGA_ContestMash : public UFAGameplayAbility
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Effects")
	TSubclassOf<class UGameplayEffect> MashEffectClass;

	/**
	 * 역할별 연타 1회당 게이지 변화량(게이지 0~100, 시작 50).
	 * −2.5 대 3 에서 시작한다 — 셋이 같은 속도로 누르면 인간이 조금 앞선다.
	 * (09/23 플레이테스트에서 −3.5 는 인간 1/9승이라 낮췄다. 같은 표본을 −2.5 로 다시 계산하면 4/9승.)
	 *
	 * 같은 인질을 거듭 구출당하면 UFAGA_RescueContest 가 계산한 누적 보정이 좀비 쪽에만 얹혀
	 * −3, −3.5 … 로 무거워진다. 같은 사람이 아니면 다시 −2.5 이다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "QTE|Weight")
	float ZombieMashDelta = -2.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "QTE|Weight")
	float RescuerMashDelta = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "QTE|Weight")
	float VictimMashDelta = 1.0f;

private:
	/** 나와 연관된 QTE 방장(좀비)을 찾는다. */
	AFACharacter* FindTargetZombie(AFACharacter* Avatar);

	/**
	 * 몸부림 소리를 마지막으로 멀티캐스트한 서버 시각(초).
	 * InstancedPerActor 라 연타 사이에도 값이 남는다.
	 */
	float LastStruggleSendTime = -1.0f;
};
