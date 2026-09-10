// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "FACharacterMovementComponent.generated.h"

class UAbilitySystemComponent;
class UFAAttributeSet;
struct FGameplayTag;
class FSavedMove_FACharacter;

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
	virtual bool DoJump(bool bReplayingMoves) override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void PhysicsRotation(float DeltaTime) override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	//~ End

	/** 로컬의 현재 시점 요청. 각 이동 기록에 담아 서버 전송·예측 보정에도 적용한다. */
	void SetFirstPersonFacingRequested(bool bRequested);

	/** 기존 특수 상태 제한을 확인한 뒤, 현재 이동이 시선 방향을 따라야 하는지 판단한다. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Movement")
	bool IsUsingFirstPersonFacing() const;

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

	/** 역행(Z06) — 앞/뒤 키 입력을 모두 후방 이동으로 변환하고 좌우 입력은 유지한다. */
	FVector ComputeBackwardOnlyAcceleration(const FVector& InputAcceleration) const;
	

	/** 이동 입력 반전(Z13/H10) — 컨트롤러 시점 기준 앞뒤와 좌우 성분을 뒤집는다. */
	FVector ComputeFlippedLRAcceleration(const FVector& InputAcceleration) const;
	
	

private:
	friend class FSavedMove_FACharacter;

	// 예측 보정으로 과거 이동을 재생해도 현재 시점 요청이 덮어써지지 않도록 분리한다.
	bool bFirstPersonFacingRequested = false;
	bool bFirstPersonFacingForMove = false;

	mutable TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

	float DefaultGroundFriction = 8.0f;

	/** 저마찰 룰 상태에서 최대 이동속도로 달리다 입력을 놓았을 때 정지까지 걸리는 목표 시간. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Movement|LowFriction", meta = (ClampMin = "0.1"))
	float LowFrictionStopDuration = 1.5f;
	
	
};
