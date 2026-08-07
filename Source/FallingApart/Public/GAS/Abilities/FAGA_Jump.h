// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GAS/FAGameplayAbility.h"
#include "FAGA_Jump.generated.h"

/**
 * 점프 어빌리티.
 *
 *   앞으로 추가될 봉인 계열 룰(탈진·등반 불가·구출 봉인)도 전부 같은 방식.
 */
UCLASS()
class FALLINGAPART_API UFAGA_Jump : public UFAGameplayAbility
{
	GENERATED_BODY()

public:
	UFAGA_Jump();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
};
