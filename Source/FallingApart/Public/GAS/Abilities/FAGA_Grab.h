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
	/** QTE 진입 시 외부에서 감염 타이머를 일시 정지하기 위한 함수 */
	void PauseInfectTimer();

	/** QTE 종료 후 필요 시 타이머를 재개하기 위한 함수 */
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

private:
	AFACharacter* FindTargetInFront(AFACharacter* Avatar, float Range, float Radius, struct FGameplayTag RequiredTag);
	void OnInfectTimerExpired();
	void CheckDistanceDuringGrab();

private:
	UPROPERTY()
	TWeakObjectPtr<AFACharacter> GrabbedVictim;

	FTimerHandle InfectTimerHandle;
	FTimerHandle DistanceCheckTimerHandle;

	float RemainingInfectTime;

	FActiveGameplayEffectHandle VictimEffectHandle;
	FActiveGameplayEffectHandle GrabberEffectHandle;
};
