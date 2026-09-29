#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FAPhantomAudioComponent.generated.h"

class ACharacter;
class UAudioComponent;
class USoundBase;

/** H26/Z23 환청의 로컬 재생과 수명을 관리한다. 룰 활성 상태는 Status가 전달한다. */
UCLASS(ClassGroup=(Audio), meta=(BlueprintSpawnableComponent))
class FALLINGAPART_API UFAPhantomAudioComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFAPhantomAudioComponent();
	// 환청 룰 추가·제거에 따라 재생 상태를 초기화한다.
	void SetRuleActive(bool bActive);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	void UpdatePhantomAudio(float DeltaTime);
	void UpdateHumanPhantomAudio(float DeltaTime);
	void UpdateZombiePhantomAudio(float DeltaTime);
	void ResetPhantomAudio();

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwnerCharacter;

	/** H26: 인간에게만 들리는 가짜 비명. 기존 Z23 발소리와 태그를 공유한다. */
	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Phantom|Scream")
	TSoftObjectPtr<USoundBase> PhantomScreamSound = TSoftObjectPtr<USoundBase>(
		FSoftObjectPath(TEXT("/Game/FallingApart/Resources/Sound/SFX/Player/Growl_Zombie.Growl_Zombie")));

	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Phantom|Scream", meta=(ClampMin="0.1", Units="s"))
	float PhantomScreamMinInterval = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Phantom|Scream", meta=(ClampMin="0.1", Units="s"))
	float PhantomScreamMaxInterval = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Phantom|Scream", meta=(ClampMin="0.0", Units="cm"))
	float PhantomScreamMinDistance = 200.0f;

	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Phantom|Scream", meta=(ClampMin="0.0", Units="cm"))
	float PhantomScreamMaxDistance = 500.0f;

	/** 루프 음원도 한 번의 환청 뒤 종료하도록 재생 길이를 제한한다. */
	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Phantom|Scream", meta=(ClampMin="0.1", Units="s"))
	float PhantomScreamMaxDuration = 2.0f;

	bool bPhantomRuleActive = false;
	float PhantomScreamPlaybackRemaining = 0.0f;
	TWeakObjectPtr<UAudioComponent> PhantomScreamComponent;

	/** DT_Sound_Character의 발소리 RowName. 기존 음량·피치·감쇠 설정을 사용한다. */
	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Phantom")
	TArray<FName> PhantomFootstepSoundIds = { FName(TEXT("Foot_Human_Run")) };

	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Phantom", meta=(ClampMin="0.1", Units="s"))
	float PhantomMinInterval = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Phantom", meta=(ClampMin="0.1", Units="s"))
	float PhantomMaxInterval = 7.0f;

	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Phantom", meta=(ClampMin="0.0", Units="cm"))
	float PhantomMinDistance = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Phantom", meta=(ClampMin="0.0", Units="cm"))
	float PhantomMaxDistance = 50.0f;

	// 로컬 소유권이 늦게 도착해도 Tick에서 시작하고, 룰 제거/소유권 상실 시 초기화한다.
	float PhantomTimeRemaining = -1.0f;
	// 한 번 발동하면 세 개의 가상 위치에서 4~7초 동안 반복 재생한다.
	TArray<float> PhantomBurstRemainings;
	TArray<float> PhantomBurstStartDelays;
	TArray<float> PhantomBurstStepTimers;
	TArray<FVector> PhantomBurstLocations;

	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Phantom", meta=(ClampMin="0.1", Units="s"))
	float PhantomBurstMinDuration = 4.0f;

	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Phantom", meta=(ClampMin="0.1", Units="s"))
	float PhantomBurstMaxDuration = 7.0f;

	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Phantom", meta=(ClampMin="0.1", Units="s"))
	float PhantomBurstMinStepInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Phantom", meta=(ClampMin="0.1", Units="s"))
	float PhantomBurstMaxStepInterval = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Phantom", meta=(ClampMin="0.0"))
	float PhantomMinVolumeMultiplier = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Phantom", meta=(ClampMin="0.0"))
	float PhantomMaxVolumeMultiplier = 4.5f;

};
