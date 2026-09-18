// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "FACharacterStatusComponent.generated.h"

class ACharacter;
class AFACharacter;
class UPrimitiveComponent;
class UAbilitySystemComponent;
class UGameplayEffect;
class UAnimMontage;
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
	void HandlePhantomRuleChanged(FGameplayTag Tag, int32 NewCount);

	void HandleFreezeRuleChanged(FGameplayTag Tag, int32 NewCount);
	void ScanFreezeTargets();
	void ClearFreeze();

	/** 에디터에서 만든 경직 GE. Has Duration 0.8초, Granted Tag = State.Stunned로 설정한다. */
	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Status|Freeze")
	TSubclassOf<UGameplayEffect> FreezeStunEffect;

	FTimerHandle FreezeDetectionTimerHandle;
	TSet<TWeakObjectPtr<AFACharacter>> PreviousFreezeTargets;
	FActiveGameplayEffectHandle FreezeEffectHandle;

	/** Z41 태그가 있을 때만 기존 캡슐의 Hit 이벤트를 연결한다. */
	void HandleKnockbackRuleChanged(FGameplayTag Tag, int32 NewCount);

	/** 서버에서 판정한 Z41 반발 몽타주를 모든 클라이언트에서 재생한다. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayKnockbackMontage();

	/** Z41: 기존 캡슐의 blocking Hit에서 인간 접촉을 판정한다. */
	UFUNCTION()
	void HandleKnockbackHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	double NextKnockbackTime = 0.0;

	UPROPERTY(EditDefaultsOnly, Category = "Rules|Knockback", meta = (ClampMin = "0.0", Units = "cm/s"))
	float KnockbackSpeed = 600.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Rules|Knockback", meta = (ClampMin = "0.0", Units = "cm/s"))
	float KnockbackUpSpeed = 150.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Rules|Knockback", meta = (ClampMin = "0.05", Units = "s"))
	float KnockbackInterval = 0.5f;

	/** Z41 반발이 발생할 때 재생할 몽타주. BP_FACharacter에서 새 몽타주를 지정한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Rules|Knockback")
	TObjectPtr<UAnimMontage> KnockbackMontage;

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

	void RefreshGhostVisual();

	/** H50 태그 추가·제거에 따라 무작위 이동속도 주기를 시작하거나 정리한다. */
	void HandleGambleSpeedRuleChanged(const FGameplayTag Tag,int32 NewCount);
	/** 현재 H50 배율을 제거하고 새 0.5~1.5배 효과를 적용한다. */
	void RollGambleSpeed();
	/** H50 타이머와 현재 속도 효과를 모두 제거한다. */
	void ClearGambleSpeed();

	/** H31 절규 태그 추가/제거 처리 */
	void HandleForceScreamRuleChanged(const FGameplayTag Tag, int32 NewCount);

	/** H31 비명 1회 재생 */
	void PlayForcedScream();

	/** H31 반복 타이머 정리 */
	void ClearForcedScream();

	// Rule.State.Recall 추가·제거 이벤트 처리
	void HandleRecallRuleChanged(const FGameplayTag Tag,int32 NewCount);

	// H43 / Z40: 공유 스폰 지점(Human PlayerStart) 중 무작위 위치로 이동
	void RecallToRandomPlayerStart();

	// H43 / Z40 공용 귀환 반복 타이머 정리
	void ClearRecall();

	/** Z34 신음 강박 태그 추가/제거 처리 */
	void HandleForceNoiseRuleChanged(const FGameplayTag Tag, int32 NewCount);

	/** Z34 소음 1회 발생 */
	void PlayForcedNoise();

	/** Z34 반복 타이머 정리 */
	void ClearForcedNoise();

	/** 현재 캐릭터 메시의 GhostOpacity 값을 변경한다. */
	void SetGhostVisualActive(bool bActive);

	/** Z46 Gameplay Tag 추가·제거 이벤트를 처리한다. */
	void HandleWallPhaseRuleChanged(const FGameplayTag Tag,int32 NewCount);
	/** 캐릭터 캡슐의 PhaseWall 충돌 응답을 변경한다. */
	void SetWallPhaseActive(bool bActive);

	/** 정상/반전 구간을 5초마다 전환한다. 서버가 상태 태그를 복제한다. */
	void ToggleInputFlip();

	void SetInputFlipActive(bool bActive);

	//Rule.State.Heartbeat 태그가 추가되거나 제거됐을 때 호출된다.
	void HandleHeartbeatRuleChanged(const FGameplayTag Tag,int32 NewCount);
	void HandleTrailRuleChanged(const FGameplayTag Tag, int32 NewCount);

	// [Z22 귀먹음] 기존 룰처럼 Status에서 태그 추가·제거 이벤트를 처리한다.
	void HandleDeafRuleChanged(const FGameplayTag Tag, int32 NewCount);
	void HandleNoStepRuleChanged(const FGameplayTag Tag, int32 NewCount);

	//각 클라이언트에서 H41 보유자의 노출 조건을 주기적으로 확인한다.
	void StartHeartbeatDetection();

	// H41 타이머를 중지하고 보유자의 로컬 아웃라인을 해제한다.
	void StopHeartbeatDetection();

	// 로컬 관찰자가 15m 이내 좀비이면 H41 보유자의 위치를 점멸 표시한다.
	void ScanHeartbeatTargets();

	// Z42 붕괴
	void HandleDecayRuleChanged(const FGameplayTag Tag, int32 NewCount);
	void UpdateDecay();
	void ClearDecay();

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
	// 서버에서 H45가 추가한 Ghost 태그 한 개만 추적한다. Z44의 태그는 제거하지 않는다.
	bool bHideGhostTagApplied = false;

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

	UPROPERTY(EditDefaultsOnly,Category="FallingApart|Status|ForceScream",meta = (ClampMin = "0.1"))
	float ForceScreamInterval = 8.0f;

	UPROPERTY(EditDefaultsOnly,Category="FallingApart|Status|ForceScream")
	FName ForceScreamSoundId = TEXT("Human_ForceScream");

	FTimerHandle ForceScreamTimerHandle;


	// H43 / Z40 공용 귀환 반복 간격.
	// 룰 획득 후 40초 뒤 첫 순간이동이 발생한다.
	UPROPERTY(EditDefaultsOnly,Category="FallingApart|Status|Recall",meta=(ClampMin="1.0"))
	float RecallInterval = 40.0f;

	// H43 / Z40 공용 귀환 반복 타이머
	FTimerHandle RecallTimerHandle;

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

	// Z34 신음 강박
	/** Z34 신음 발생 주기 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Status|ForceNoise", meta = (ClampMin = "0.1"))
	float ForceNoiseInterval = 3.0f;

	/** DT_Sound_Character에 추가할 SoundId */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Status|ForceNoise")
	FName ForceNoiseSoundId = TEXT("Zombie_ForceNoise");

	FTimerHandle ForceNoiseTimerHandle;

	// Z42 붕괴
	// 반복 검사
	FTimerHandle DecayTimerHandle;

	// 탐지 반경 10m
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Decay",meta = (ClampMin = "1.0"))
	float DecayRadius = 1000.0f;

	// 최대 감속 비율 35%
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Decay",meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DecayMaxSlowRatio = 0.35f;

	// 거리 검사 주기
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Decay",meta = (ClampMin = "0.05"))
	float DecayUpdateInterval = 0.2f;
};
