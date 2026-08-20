// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FACharacterStatusComponent.generated.h"

class ACharacter;
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

	/** 현재 캐릭터 메시의 GhostOpacity 값을 변경한다. */
	void SetGhostVisualActive(bool bActive);
	
	/** Z46 Gameplay Tag 추가·제거 이벤트를 처리한다. */
	void HandleWallPhaseRuleChanged(const FGameplayTag Tag,int32 NewCount);
	/** 캐릭터 캡슐의 PhaseWall 충돌 응답을 변경한다. */
	void SetWallPhaseActive(bool bActive);

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
	
	UPROPERTY(EditDefaultsOnly,Category="FallingApart|Status|Ghost")
	float GhostActiveOpacity = 0.35f;

	/**
	 * Z44가 적용될 때 캐릭터 메시 슬롯마다 생성하는
	 * 동적 머티리얼 인스턴스.
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> GhostDynamicMaterials;
		
};
