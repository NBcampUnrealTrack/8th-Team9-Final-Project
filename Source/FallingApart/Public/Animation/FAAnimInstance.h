// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "FAAnimInstance.generated.h"

class AFACharacter;
class UAbilitySystemComponent;
class UCharacterMovementComponent;

/*
  GAS 상태가 동시에 복제되는 짧은 구간에도 애니메이션 우선순위를 한 곳에서 결정하기 위한 상태.
 */
UENUM(BlueprintType)
enum class EFAAnimStatusOverride : uint8
{
	None,
	Seizure,
	Stunned,
	Grabbed,
	Grabbing,
	HumanContesting,
	ZombieContesting,
	Contesting
};


UCLASS()
class FALLINGAPART_API UFAAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;
protected:
	void CacheOwnerReferences();

	/* 현재 보유 태그를 애니메이션 전용 단일 상태로 변환한다. */
	void UpdateGASStatusOverride();

	/** 사다리 등반 여부와 등반 애니메이션 재생 속도를 갱신한다. */
	void UpdateTraversalAnimationData();

	[[nodiscard]] EFAAnimStatusOverride ResolveGASStatusOverride() const;

	UPROPERTY()
	AFACharacter* OwningCharacter;

	UPROPERTY()
	UCharacterMovementComponent* OwningMovementComponent;

	UPROPERTY()
	UAbilitySystemComponent* OwningAbilitySystemComponent;

	UPROPERTY(VisibleDefaultsOnly,BlueprintReadOnly, Category = "AnimData|LocomotionData")
	float GroundSpeed;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	float AirSpeed;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	bool bHasAcceleration;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	bool bIsFalling;

	/*
	  일반 이동 자세 선택에만 사용하며 GAS 상태 Override 와는 분리한다.
	 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	bool bIsCrouched = false;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|Traversal")
	bool bIsClimbing = false;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|Traversal")
	float ClimbSpeed = 0.0f;

	/**
	 * 등반 애니메이션 재생 배율.
	 * 1은 위로 정상 재생, 0은 정지, 음수는 아래 방향 역재생.
	 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|Traversal")
	float ClimbPlayRate = 0.0f;

	/*
	  AnimGraph 의 StatusOverride 상태머신이 직접 읽는 값.
	  우선순위: Grabbed > Contesting > Grabbing > Seizure > Stunned > None.
	 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|GAS")
	EFAAnimStatusOverride StatusOverride = EFAAnimStatusOverride::None;
};
