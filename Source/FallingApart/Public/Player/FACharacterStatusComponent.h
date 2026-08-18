// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FACharacterStatusComponent.generated.h"

class ACharacter;
class UAbilitySystemComponent;
class UGameplayEffect;
struct FGameplayTag;

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
	
private:
	UFUNCTION()
	void HandleLanded(const FHitResult& Hit);

	/** Rule.Input.FlipLR의 추가/제거에 맞춰 5초 주기를 시작하거나 정리한다. */
	void HandleInputFlipRuleChanged(const FGameplayTag Tag, int32 NewCount);

	// [H07 포복 수정안 — 설명용 주석이므로 현재 실행되지 않음]
	// Rule.Move.CrouchOnly 태그가 추가되는 순간 Crouch()를 호출할 콜백이다.
	// 실제 적용할 때 아래 선언의 주석을 해제한다.
	void HandleCrouchOnlyRuleChanged(const FGameplayTag Tag, int32 NewCount);

	/** 정상/반전 구간을 5초마다 전환한다. 서버가 상태 태그를 복제한다. */
	void ToggleInputFlip();

	void SetInputFlipActive(bool bActive);
	
	UAbilitySystemComponent* GetOwnerAbilitySystem() const;
	
	/**
		* 착지 시 적용할 상태 GE.
		* Has Duration 1초, Granted Tag는 State.Stunned로 설정한다.
		*/
	UPROPERTY(EditDefaultsOnly,Category="FallingApart|Status|Landing")
	TSubclassOf<UGameplayEffect> LandStunEffect;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwnerCharacter;

	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Status|Input", meta=(ClampMin="0.1"))
	float InputFlipInterval = 5.0f;

	FTimerHandle InputFlipTimerHandle;
	bool bInputFlipActive = false;
		
};
