// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "FAAnimInstance.generated.h"

class AFACharacter;
class UAbilitySystemComponent;
class UCharacterMovementComponent;

/*
  일반 이동·몽타주보다 우선해서 표시할 상태. 값과 순서는 기존 AnimBP 직렬화 호환성을 유지한다.
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

	/* 현재 태그를 한 번 수집하고 FAAnimStatusPolicy에 표시 상태 선택을 맡긴다. */
	void UpdateGASStatusOverride();

	/** 사다리 등반 여부와 등반 애니메이션 재생 속도를 갱신한다. */
	void UpdateTraversalAnimationData();

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
	  AnimGraph의 Blend Poses by Enum이 읽는 값. 포즈 혼합은 AnimBP가 담당한다.
	  우선순위는 FAAnimStatusPolicy의 규칙표에서 관리한다.
	 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|GAS")
	EFAAnimStatusOverride StatusOverride = EFAAnimStatusOverride::None;
};
