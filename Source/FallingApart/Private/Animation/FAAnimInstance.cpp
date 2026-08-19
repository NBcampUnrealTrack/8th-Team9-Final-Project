// Fill out your copyright notice in the Description page of Project Settings.


#include "Animation/FAAnimInstance.h"

#include "AbilitySystemComponent.h"
#include "GAS/FAGameplayTags.h"
#include "Player/FACharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

void UFAAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	CacheOwnerReferences();
	UpdateGASStatusOverride();
}

void UFAAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!OwningCharacter || !OwningMovementComponent || !OwningAbilitySystemComponent)
	{
		CacheOwnerReferences();
	}

	UpdateGASStatusOverride();
}

void UFAAnimInstance::CacheOwnerReferences()
{
	OwningCharacter = Cast<AFACharacter>(TryGetPawnOwner());
	OwningMovementComponent = nullptr;
	OwningAbilitySystemComponent = nullptr;

	if (OwningCharacter)
	{
		OwningMovementComponent = OwningCharacter->GetCharacterMovement();
		OwningAbilitySystemComponent = OwningCharacter->GetAbilitySystemComponent();
	}
}

void UFAAnimInstance::UpdateGASStatusOverride()
{
	StatusOverride = ResolveGASStatusOverride();
}

EFAAnimStatusOverride UFAAnimInstance::ResolveGASStatusOverride() const
{
	if (!OwningAbilitySystemComponent)
	{
		return EFAAnimStatusOverride::None;
	}

	const FAGameplayTags& Tags = FAGameplayTags::Get();

	if (OwningAbilitySystemComponent->HasMatchingGameplayTag(Tags.State_Grabbed))
	{
		return EFAAnimStatusOverride::Grabbed;
	}

	if (OwningAbilitySystemComponent->HasMatchingGameplayTag(Tags.State_Contesting))
	{
		if (OwningAbilitySystemComponent->HasMatchingGameplayTag(Tags.State_Human))
		{
			return EFAAnimStatusOverride::HumanContesting;
		}

		if (OwningAbilitySystemComponent->HasMatchingGameplayTag(Tags.State_Zombie))
		{
			return EFAAnimStatusOverride::ZombieContesting;
		}

		return EFAAnimStatusOverride::Contesting;
	}

	if (OwningAbilitySystemComponent->HasMatchingGameplayTag(Tags.State_Grabbing))
	{
		return EFAAnimStatusOverride::Grabbing;
	}

	const bool bIsStunned = OwningAbilitySystemComponent->HasMatchingGameplayTag(Tags.State_Stunned);
	if (bIsStunned && OwningAbilitySystemComponent->HasMatchingGameplayTag(Tags.Rule_Move_Seizure))
	{
		return EFAAnimStatusOverride::Seizure;
	}

	if (bIsStunned)
	{
		return EFAAnimStatusOverride::Stunned;
	}

	return EFAAnimStatusOverride::None;
}

void UFAAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	if (!OwningCharacter || !OwningMovementComponent)
	{
		return;
	}

	GroundSpeed = OwningCharacter->GetVelocity().Size2D();
	bHasAcceleration = OwningMovementComponent->GetCurrentAcceleration().SizeSquared2D() > 0.f;
	bIsFalling = OwningMovementComponent->IsFalling();
	bIsCrouched = OwningCharacter->bIsCrouched;
	AirSpeed = OwningCharacter->GetVelocity().Z;
}
