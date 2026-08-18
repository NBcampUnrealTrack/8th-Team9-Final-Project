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

private:
	AFACharacter* FindTargetInRadius(AFACharacter* Avatar, float Radius, struct FGameplayTag RequiredTag);
	void StartRescueContest(AFACharacter* Rescuer, AFACharacter* GrabbedAlly);

	FTimerHandle RescueDelayTimerHandle;
};
