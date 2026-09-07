// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FACharacterStatusComponent.generated.h"

class ACharacter;
class AFACharacter;
class UAbilitySystemComponent;
class UGameplayEffect;
class UMaterialInstanceDynamic; // 동적 머티리얼 전방 선언
struct FGameplayTag;

namespace FACollision
{
	// Config/DefaultEngine.ini의 PhaseWall 채널과 동일해야 한다.
	constexpr ECollisionChannel PhaseWall =ECC_GameTraceChannel1;
}

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class FALLINGAPART_API UFACharacterStatusComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UFACharacterStatusComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;
	
private:
	UFUNCTION()
	void HandleLanded(const FHitResult& Hit);

	/** Rule.Input.FlipLR의 추가/제거에 맞춰 5초 주기를 시작하거나 정리한다. */
	void HandleInputFlipRuleChanged(const FGameplayTag Tag, int32 NewCount);

	//* 포복용
	void HandleCrouchOnlyRuleChanged(const FGameplayTag Tag, int32 NewCount);
	
	/** Rule.Trade.Ghost 태그 추가·제거에 따라 유령 외형을 변경한다. */
	void HandleGhostRuleChanged(const FGameplayTag Tag,int32 NewCount);

	void HandleSecondLifeRuleChanged(const FGameplayTag Tag,int32 NewCount);
	void UpdateHideRule(float DeltaTime);
	void SetHideActive(bool bActive);
	
	void UpdateCampRule(float DeltaTime);
	void SetCampActive(bool bActive);
	
	void HandleHiddenStateChanged(const FGameplayTag Tag,int32 NewCount);
	void RefreshGhostVisual();

	/** H50 태그 추가·제거에 따라 무작위 이동속도 주기를 시작하거나 정리한다. */
	void HandleGambleSpeedRuleChanged(const FGameplayTag Tag,int32 NewCount);
	/** 현재 H50 배율을 제거하고 새 0.5~1.5배 효과를 적용한다. */
	void RollGambleSpeed();
	/** H50 타이머와 현재 속도 효과를 모두 제거한다. */
	void ClearGambleSpeed();
	
	// Rule.Gamble.Blink 추가·제거 이벤트 처리
	void HandleGambleBlinkRuleChanged(const FGameplayTag Tag,int32 NewCount);

	// 무작위 Human PlayerStart로 순간이동
	void BlinkToRandomPlayerStart();

	// H43 반복 타이머 정리
	void ClearGambleBlink();


	/** 현재 캐릭터 메시의 GhostOpacity 값을 변경한다. */
	void SetGhostVisualActive(bool bActive);
	
	/** Z46 Gameplay Tag 추가·제거 이벤트를 처리한다. */
	void HandleWallPhaseRuleChanged(const FGameplayTag Tag,int32 NewCount);
	/** 캐릭터 캡슐의 PhaseWall 충돌 응답을 변경한다. */
	void SetWallPhaseActive(bool bActive);

	/** 정상/반전 구간을 5초마다 전환한다. 서버가 상태 태그를 복제한다. */
	void ToggleInputFlip();

	void SetInputFlipActive(bool bActive);
	
	/**
	  * Rule.State.Heartbeat 태그가
	  * 추가되거나 제거됐을 때 호출된다.
	  */
	void HandleHeartbeatRuleChanged(const FGameplayTag Tag,int32 NewCount);

	/**
	 * 각 클라이언트에서 H41 보유자의 노출 조건을 주기적으로 확인한다.
	 */
	void StartHeartbeatDetection();

	/**
	 * H41 타이머를 중지하고 보유자의 로컬 아웃라인을 해제한다.
	 */
	void StopHeartbeatDetection();

	/**
	 * 로컬 관찰자가 15m 이내 좀비이면 H41 보유자의 위치를 점멸 표시한다.
	 */
	void ScanHeartbeatTargets();
	
	UAbilitySystemComponent* GetOwnerAbilitySystem() const;
	
	/* 착지 시 적용할 상태 GE. Has Duration 1초, Granted Tag는 State.Stunned로 설정한다. */
	UPROPERTY(EditDefaultsOnly,Category="FallingApart|Status|Landing")
	TSubclassOf<UGameplayEffect> LandStunEffect;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwnerCharacter;

	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Status|Input", meta=(ClampMin="0.1"))
	float InputFlipInterval = 5.0f;

	FTimerHandle InputFlipTimerHandle;
	bool bInputFlipActive = false;
	
	UPROPERTY(EditDefaultsOnly,Category="FallingApart|Status|Ghost")
	float GhostActiveOpacity = 0.35f;

	/**
	 * Z44가 적용될 때 캐릭터 메시 슬롯마다 생성하는
	 * 동적 머티리얼 인스턴스.
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> GhostDynamicMaterials;

	UPROPERTY(EditDefaultsOnly,Category="FallingApart|Status|Hide",meta=(ClampMin="0.0"))
	float HideDelay = 2.0f;

	UPROPERTY(EditDefaultsOnly,Category="FallingApart|Status|Hide",meta=(ClampMin="0.0"))
	float HideStationarySpeedTolerance = 3.0f;

	float HideStationaryElapsed = 0.0f;

	UPROPERTY(EditDefaultsOnly,Category="FallingApart|Status|Camp",meta=(ClampMin="0.0"))
	float CampDelay = 4.0f;

	UPROPERTY(EditDefaultsOnly,Category="FallingApart|Status|Camp",meta=(ClampMin="0.0"))
	float CampStationarySpeedTolerance = 3.0f;
	
	float CampStationaryElapsed = 0.0f;

	UPROPERTY(EditDefaultsOnly,Category="FallingApart|Status|Gamble",meta=(ClampMin="0.1"))
	float GambleSpeedInterval = 10.0f;

	UPROPERTY(EditDefaultsOnly,Category="FallingApart|Status|Gamble",meta=(ClampMin="0.0"))
	float GambleSpeedMinMultiplier = 0.5f;

	UPROPERTY(EditDefaultsOnly,Category="FallingApart|Status|Gamble",meta=(ClampMin="0.0"))
	float GambleSpeedMaxMultiplier = 1.5f;

	FTimerHandle GambleSpeedTimerHandle;
		
	
	// H43 순간이동 반복 간격.
	// 룰 획득 후 40초 뒤 첫 순간이동이 발생한다.
	UPROPERTY(EditDefaultsOnly,Category="FallingApart|Status|Gamble",meta=(ClampMin="1.0"))
	float GambleBlinkInterval = 40.0f;

	// H43 반복 타이머
	FTimerHandle GambleBlinkTimerHandle;

	// [H41 추가 시작]

	/** H41 주변 좀비 탐색 타이머 */
	FTimerHandle HeartbeatDetectionTimerHandle;

	/** 로컬 좀비가 범위에 진입한 시각. 범위 밖에서는 -1. */
	double HeartbeatRevealStartTime = -1.0;

	/** H41 탐색 거리: 1500uu = 15m */
	UPROPERTY(EditDefaultsOnly,Category="FallingApart|Heartbeat",meta=(ClampMin="0.0"))
	float HeartbeatDetectionRadius = 1500.0f;

	/** H41 탐색 주기: 초당 5회 */
	UPROPERTY(EditDefaultsOnly,Category="FallingApart|Heartbeat",meta=(ClampMin="0.05"))
	float HeartbeatDetectionInterval = 0.2f;

	/** 위치 점멸 주기와 한 주기 내 표시 시간 (초). */
	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Heartbeat", meta=(ClampMin="0.1"))
	float HeartbeatBlinkPeriod = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Heartbeat", meta=(ClampMin="0.0"))
	float HeartbeatBlinkVisibleDuration = 0.4f;

	// [H41 추가 끝]
};
