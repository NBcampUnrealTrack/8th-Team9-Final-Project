// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GAS/FAGameplayAbility.h"
#include "FAGA_Grab.generated.h"

class AFACharacter;
class UGameplayEffect;

/**
 * 좀비 전용 포획(Grab) 어빌리티
 */
UCLASS()
class FALLINGAPART_API UFAGA_Grab : public UFAGameplayAbility
{
	GENERATED_BODY()

public:
	UFAGA_Grab();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

public:
	/**
	 * 이 ASC 에서 지금 돌고 있는 포획 어빌리티 인스턴스. 없으면 nullptr.
	 */
	static UFAGA_Grab* FindActiveInstance(UAbilitySystemComponent* ASC);

	/** QTE 진입 시 외부에서 감염 진행을 일시 정지하기 위한 함수 */
	void PauseInfectTimer();

	/** QTE 종료 후 필요 시 감염 진행을 재개하기 위한 함수 */
	void ResumeInfectTimer();

	/** 현재 붙잡고 있는 피해자 반환 */
	AFACharacter* GetGrabbedVictim() const { return GrabbedVictim.Get(); }

protected:
	/** 잡힌 인간에게 적용할 Effect (State.Grabbed + 이동속도 감소) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Effects")
	TSubclassOf<UGameplayEffect> VictimGrabbedEffectClass;

	/** 잡은 좀비 자신에게 적용할 Effect (State.Grabbing + 이동속도 감소) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Effects")
	TSubclassOf<UGameplayEffect> GrabberGrabbingEffectClass;

	/** 포획 실패 시 적용할 Effect (경직) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Effects")
	TSubclassOf<UGameplayEffect> FailStunEffectClass;
  
	/**
	 * 인질이 최대로 저항했을 때 감염 시간이 몇 배로 늘어나는가.
	 *
	 * InfectDuration Attribute(= 저항 0 기준 시간, 기본 5초)에 곱한다. 기본 3.0 이면 최대 15초.
	 * 저항은 감염을 취소하지 못한다 — 구조자가 도착할 시간을 버는 수단이다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Grab", meta = (ClampMin = "1.0"))
	float MaxResistTimeMultiplier = 3.0f;

	/**
	 * 감염 진행 누적과 거리 이탈 검사를 함께 도는 서버 틱 주기(초).
	 * 게이지는 클라이언트가 속도로 외삽하므로 이 값이 곧 게이지의 부드러움은 아니다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GAS|Grab", meta = (ClampMin = "0.02", ClampMax = "0.5"))
	float GrabTickInterval = 0.1f;

private:
	AFACharacter* FindTargetInFront(AFACharacter* Avatar, float Range, float Radius, struct FGameplayTag RequiredTag);

	/** 서버 틱 — 거리 이탈 검사 + 저항 강도에 따른 감염 진행도 누적. */
	void TickGrab();

	/** 진행도가 1 에 닿았다. 진영 전환·점수·연속 드래프트는 게임모드가 맡는다. */
	void CompleteInfection();

	/** 지금 진행도와 속도를 좀비·인질 양쪽 게이지로 내보낸다. */
	void PushCaptureProgress();

private:
	UPROPERTY()
	TWeakObjectPtr<AFACharacter> GrabbedVictim;

	FTimerHandle GrabTickHandle;

	/** 감염 진행도 0~1. 고정 타이머가 아니라 저항 강도에 따라 차는 속도가 달라진다. */
	float InfectProgress = 0.0f;

	/** 저항이 0 일 때의 감염 시간(초). 포획 시작 시 InfectDuration Attribute 에서 받아 둔다. */
	float BaseInfectDuration = 0.0f;

	/** 지금의 초당 진행 속도. 게이지로 내보낼 값이라 매 틱 갱신한다. */
	float CurrentInfectRate = 0.0f;

	/** 구출 QTE 로 감염이 멈춰 있는가. */
	bool bInfectPaused = false;

	FActiveGameplayEffectHandle VictimEffectHandle;
	FActiveGameplayEffectHandle GrabberEffectHandle;
};
