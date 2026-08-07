// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/FAGameplayAbility.h"

#include "GAS/FAAbilitySystemComponent.h"
#include "Player/FACharacter.h"

UFAGameplayAbility::UFAGameplayAbility()
{
	// 액터당 1개로 재사용.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 포획·QTE 판정만 예외적으로 서버 권한(ServerOnly)
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

AFACharacter* UFAGameplayAbility::GetFACharacter() const
{
	return CurrentActorInfo ? Cast<AFACharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
}

UFAAbilitySystemComponent* UFAGameplayAbility::GetFAAbilitySystemComponent() const
{
	return CurrentActorInfo ? Cast<UFAAbilitySystemComponent>(CurrentActorInfo->AbilitySystemComponent.Get()) : nullptr;
}
