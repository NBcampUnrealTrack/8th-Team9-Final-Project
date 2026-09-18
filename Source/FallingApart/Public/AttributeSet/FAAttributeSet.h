#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "FAAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)
UCLASS()
class FALLINGAPART_API UFAAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UFAAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// CurrentValue 가 변경되기전 호출
	//GameplayEffect로 계산된 NewValue를 Attribute별 유효 범위로 제한합니다.
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute,float& NewValue) override;

	//  Instant 가 실행된 후 호출
	// 최종 Attribute 값을 다시 검사하고 유효 범위 제한
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

#pragma region Movement

public:
	// 기본 이동속도
	//Move.SpeedDown.S / Move.SpeedDown.L
	//
	UPROPERTY(BlueprintReadOnly,ReplicatedUsing = OnRep_MoveSpeed,Category = "Attribute|Movement")
	FGameplayAttributeData MoveSpeed;
	ATTRIBUTE_ACCESSORS(UFAAttributeSet, MoveSpeed)

	// 전력질주 속도 배율 기본값 1.5
	// MoveSpeed * SprintMultiplier
	UPROPERTY(BlueprintReadOnly,ReplicatedUsing = OnRep_SprintMultiplier,Category = "Attribute|Movement")
	FGameplayAttributeData SprintMultiplier;
	ATTRIBUTE_ACCESSORS(UFAAttributeSet, SprintMultiplier)

	// 지면 마찰력 기본값 8.0
	// Move.LowFriction
	UPROPERTY(BlueprintReadOnly,ReplicatedUsing = OnRep_GroundFriction,Category = "Attribute|Movement")
	FGameplayAttributeData GroundFriction;
	ATTRIBUTE_ACCESSORS(UFAAttributeSet, GroundFriction)

	// 공중 제어 기본값 0.35
	//Move.NoAirControl
	UPROPERTY(BlueprintReadOnly,ReplicatedUsing = OnRep_AirControl,Category = "Attribute|Movement")
	FGameplayAttributeData AirControl;
	ATTRIBUTE_ACCESSORS(UFAAttributeSet, AirControl)

	// 카메라 회전 속도  기본값 1.0
	// Move.SlowTurn
	UPROPERTY(BlueprintReadOnly,ReplicatedUsing = OnRep_TurnRateScale,Category = "Attribute|Movement")
	FGameplayAttributeData TurnRateScale;
	ATTRIBUTE_ACCESSORS(UFAAttributeSet, TurnRateScale)

	// 물속 이동속도 배율 기본값 1.0
	// 적용 0.2
	//Move.WaterFear
	UPROPERTY(BlueprintReadOnly,ReplicatedUsing = OnRep_WaterMoveSpeedMultiplier,Category = "Attribute|Movement")
	FGameplayAttributeData WaterMoveSpeedMultiplier;
	ATTRIBUTE_ACCESSORS(UFAAttributeSet,WaterMoveSpeedMultiplier)

	// H50 저주받은 행운이 10초마다 정하는 이동속도 배율. 기본값 1.0.
	UPROPERTY(BlueprintReadOnly,ReplicatedUsing=OnRep_GambleSpeedMultiplier,Category="Attribute|Movement")
	FGameplayAttributeData GambleSpeedMultiplier;
	ATTRIBUTE_ACCESSORS(UFAAttributeSet,GambleSpeedMultiplier)

	/** H03/Z03 이동 피로 배율. 다른 속도 룰과 독립적으로 곱한다. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_FatigueSpeedMultiplier, Category="Attribute|Movement")
	FGameplayAttributeData FatigueSpeedMultiplier;
	ATTRIBUTE_ACCESSORS(UFAAttributeSet, FatigueSpeedMultiplier)

	UFUNCTION()
	void OnRep_FatigueSpeedMultiplier(const FGameplayAttributeData& OldValue);

	// 점프 기본값 420.0
	// GameplayEffect에서 배율 또는 고정값으로 변경
	UPROPERTY(BlueprintReadOnly,ReplicatedUsing = OnRep_RuleJumpZVelocity,Category = "Attribute|Movement")
	FGameplayAttributeData RuleJumpZVelocity;
	ATTRIBUTE_ACCESSORS(UFAAttributeSet,RuleJumpZVelocity)

	UPROPERTY(BlueprintReadOnly,ReplicatedUsing = OnRep_DecaySpeedMultiplier,Category = "Attribute|Movement")
	FGameplayAttributeData DecaySpeedMultiplier;
	ATTRIBUTE_ACCESSORS(UFAAttributeSet, DecaySpeedMultiplier)

#pragma endregion



#pragma region Stamina

public:
	// 스태미나 기본값 5.0
	UPROPERTY(BlueprintReadOnly,ReplicatedUsing = OnRep_Stamina,Category = "Attribute|Stamina")
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS(UFAAttributeSet, Stamina)

	UPROPERTY(BlueprintReadOnly,ReplicatedUsing = OnRep_MaxStamina,Category = "Attribute|Stamina")
	FGameplayAttributeData MaxStamina;
	ATTRIBUTE_ACCESSORS(UFAAttributeSet, MaxStamina)

#pragma endregion

#pragma region Vision

public:
	/**
	 * VisionRange 기본값이자 상한(uu). InitVisionRange, PreAttributeChange 클램프,
	 * PostGameplayEffectExecute 클램프, FACharacter::UpdateVisionEffects() 의 안개 세기 계산이
	 * 전부 이 값을 참조한다 — 예전엔 각자 6000.0f 를 따로 박아둬서 하나만 바꾸면 나머지가 어긋났다.
	 */
	static constexpr float MaxVisionRange = 6000.0f;

	// 시야 기본값 6000
	//Vision.Fog 적용값: 800uu
	UPROPERTY(BlueprintReadOnly,ReplicatedUsing = OnRep_VisionRange,Category = "Attribute|Vision")
	FGameplayAttributeData VisionRange;
	ATTRIBUTE_ACCESSORS(UFAAttributeSet, VisionRange)

	// 카메라 시야각
	//Vision.NarrowFOV
	UPROPERTY(BlueprintReadOnly,ReplicatedUsing = OnRep_FieldOfView,Category = "Attribute|Vision")
	FGameplayAttributeData FieldOfView;
	ATTRIBUTE_ACCESSORS(UFAAttributeSet, FieldOfView)

#pragma endregion

#pragma region Bite

public:
	/** 좀비 포획(Bite) 기본 쿨타임 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_BiteCooldown, Category = "Attributes|Zombie")
	FGameplayAttributeData BiteCooldown;
	ATTRIBUTE_ACCESSORS(UFAAttributeSet, BiteCooldown)

#pragma endregion

#pragma region GrabAndRescue

public:
	// 포획 사거리 기본값 200.0uu (2.0m)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_GrabRange, Category = "Attribute|Grab")
	FGameplayAttributeData GrabRange;
	ATTRIBUTE_ACCESSORS(UFAAttributeSet, GrabRange)

	// 감염 소요 시간(저항 0 기준) 기본값 5.0초 클램프 0.5~15.0
	// 실제 감염 시간은 인질의 저항 강도에 따라 이 값의 1~3배 사이에서 움직인다.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_InfectDuration, Category = "Attribute|Grab")
	FGameplayAttributeData InfectDuration;
	ATTRIBUTE_ACCESSORS(UFAAttributeSet, InfectDuration)

	/**
	* 최대 저항 상태에서 감염 시간이 늘어나는 배율.
	*
	* 기본값 3.0:
	* InfectDuration이 5초라면 최대 저항에서 15초.
	*/
	UPROPERTY(BlueprintReadOnly,ReplicatedUsing = OnRep_MaxResistTimeMultiplier,Category = "Attribute|Grab")
	FGameplayAttributeData MaxResistTimeMultiplier;
	ATTRIBUTE_ACCESSORS(UFAAttributeSet, MaxResistTimeMultiplier)

	// 구출 QTE 진입 선딜레이 기본값 0.0초
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RescueDuration, Category = "Attribute|Rescue")
	FGameplayAttributeData RescueDuration;
	ATTRIBUTE_ACCESSORS(UFAAttributeSet, RescueDuration)

	// QTE 줄다리기 게이지 (기본 50. 0=좀비 승리, 100=인간 승리)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ContestGauge, Category = "Attribute|QTE")
	FGameplayAttributeData ContestGauge;
	ATTRIBUTE_ACCESSORS(UFAAttributeSet, ContestGauge)

#pragma endregion

#pragma region RepNotify

protected:
	UFUNCTION()
	void OnRep_MoveSpeed(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_SprintMultiplier(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_GroundFriction(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_AirControl(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_TurnRateScale(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_WaterMoveSpeedMultiplier(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_GambleSpeedMultiplier(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Stamina(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_VisionRange(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_FieldOfView(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_GrabRange(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_InfectDuration(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxResistTimeMultiplier(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_RescueDuration(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_ContestGauge(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_RuleJumpZVelocity(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_BiteCooldown(const FGameplayAttributeData& OldBiteCooldown);

	UFUNCTION()
	void OnRep_DecaySpeedMultiplier(const FGameplayAttributeData& OldValue);

#pragma endregion
};
