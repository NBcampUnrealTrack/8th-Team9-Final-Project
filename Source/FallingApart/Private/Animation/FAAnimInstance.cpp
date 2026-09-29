// Fill out your copyright notice in the Description page of Project Settings.


#include "Animation/FAAnimInstance.h"

#include "AbilitySystemComponent.h"
#include "Animation/FAAnimStatusPolicy.h"
#include "Player/FACharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Interact/FALadder.h"

void UFAAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	CacheOwnerReferences();
	UpdateTraversalAnimationData();
	UpdateGASStatusOverride();
}

void UFAAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!OwningCharacter || !OwningMovementComponent || !OwningAbilitySystemComponent)
	{
		CacheOwnerReferences();
	}

	UpdateTraversalAnimationData();
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
	FGameplayTagContainer ActiveTags;
	if (OwningAbilitySystemComponent)
	{
		OwningAbilitySystemComponent->GetOwnedGameplayTags(ActiveTags);
	}
	StatusOverride = FAAnimStatusPolicy::Resolve(ActiveTags);
}

void UFAAnimInstance::UpdateTraversalAnimationData()
{
	bIsClimbing = false;
	ClimbSpeed = 0.0f;
	ClimbPlayRate = 0.0f;

	if (!OwningCharacter)
	{
		return;
	}

	const AFALadder* Ladder = OwningCharacter->GetClimbingLadder();
	if (!Ladder)
	{
		return;
	}

	bIsClimbing = true;

	// 월드 Z 속도가 아니라 실제 사다리 방향을 기준으로 계산한다.
	// 따라서 사다리가 약간 기울어져 있어도 위·아래 방향이 정확하다.
	ClimbSpeed = FVector::DotProduct(
		OwningCharacter->GetVelocity(),
		Ladder->GetClimbUpVector());

	const float ReferenceSpeed = FMath::Max(Ladder->ClimbSpeed, 1.0f);

	ClimbPlayRate = FMath::Clamp(
		ClimbSpeed / ReferenceSpeed,
		-1.5f,
		1.5f);
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
