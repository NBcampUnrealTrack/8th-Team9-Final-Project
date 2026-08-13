// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "FACharacterMovementComponent.generated.h"

class UAbilitySystemComponent;
class UFAAttributeSet;
struct FGameplayTag;

UCLASS()
class FALLINGAPART_API UFACharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UFACharacterMovementComponent();

	//~ UCharacterMovementComponent
	virtual void InitializeComponent() override;
	virtual float GetMaxSpeed() const override;
	virtual bool CanAttemptJump() const override;
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	virtual FVector GetAirControl(float DeltaTime, float TickAirControl, const FVector& FallAcceleration) override;
	virtual FVector ConstrainInputAcceleration(const FVector& InputAcceleration) const override;
	//~ End

	/** 경련·경직·QTE 등으로 이동이 완전히 봉인된 상태인가. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Movement")
	bool IsMovementFrozen() const;

protected:
	/** 소유 캐릭터의 ASC. 캐릭터 생성 순서 때문에 지연 캐싱한다. */
	UAbilitySystemComponent* GetOwnerASC() const;

	/** 이동 Attribute 묶음. ASC 가 아직 없으면 nullptr → 전부 CMC 기본값으로 폴백한다. */
	const UFAAttributeSet* GetOwnerAttributes() const;

	bool HasOwnerTag(const FGameplayTag& Tag) const;

	/** 자동 전진(Z05/H05) — 전진 성분은 최대로 고정하고 좌우 조향만 플레이어에게 남긴다. */
	FVector ComputeForceForwardAcceleration(const FVector& InputAcceleration) const;
	

	/** 이동 입력 반전(Z13/H10) — 컨트롤러 시점 기준 앞뒤와 좌우 성분을 뒤집는다. */
	FVector ComputeFlippedLRAcceleration(const FVector& InputAcceleration) const;
	
	

private:
	mutable TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

	float DefaultGroundFriction = 8.0f;
};
