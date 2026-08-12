#pragma once

#include "CoreMinimal.h"
#include "GAS/FAGameplayAbility.h"
#include "FAGA_ContestMash.generated.h"

class AFACharacter;

/**
 * 스페이스바 입력 시 발동하는 QTE 연타 어빌리티.
 * 진영에 따라 가중치를 계산해 방장(좀비)의 ContestGauge 를 조작한다.
 */
UCLASS()
class FALLINGAPART_API UFAGA_ContestMash : public UFAGameplayAbility
{
	GENERATED_BODY()

public:
	UFAGA_ContestMash();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Effects")
	TSubclassOf<class UGameplayEffect> MashEffectClass;

private:
	/** 나와 연관된 QTE 방장(좀비)을 찾는다. */
	AFACharacter* FindTargetZombie(AFACharacter* Avatar);
};
