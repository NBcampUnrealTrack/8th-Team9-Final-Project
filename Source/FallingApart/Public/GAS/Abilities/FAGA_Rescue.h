// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GAS/FAGameplayAbility.h"
#include "FAGA_Rescue.generated.h"

class AFACharacter;

/**
 * 인간 전용 구출(Rescue) 어빌리티
 */
UCLASS()
class FALLINGAPART_API UFAGA_Rescue : public UFAGameplayAbility
{
	GENERATED_BODY()

public:
	UFAGA_Rescue();

	/** 줄다리기가 끝난 구조자에게 재구출 쿨타임을 건다. 서버에서 GA_RescueContest 가 부른다. */
	static void StartRescueCooldown(UAbilitySystemComponent* RescuerASC);

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags) const override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

protected:
	/** QTE 가 끝난 뒤 같은 구조자가 다시 구출을 시도할 수 있기까지의 시간(초). 승패와 무관하게 건다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Rescue", meta = (ClampMin = "0"))
	float RescueCooldown = 2.0f;

private:
	AFACharacter* FindTargetInRadius(AFACharacter* Avatar, float Radius, struct FGameplayTag RequiredTag);
	void StartRescueContest(AFACharacter* Rescuer, AFACharacter* GrabbedAlly);

	FTimerHandle RescueDelayTimerHandle;

	// 서버 월드 시각. 쿨다운 GE 애셋 대신 InstancedPerActor 인스턴스가 직접 들고 있다 —
	// 서버만 알기 때문에 클라 예측 발동은 통과하고, 서버가 거절해서 되돌린다.
	float NextRescueAllowedTime = 0.0f;
};
