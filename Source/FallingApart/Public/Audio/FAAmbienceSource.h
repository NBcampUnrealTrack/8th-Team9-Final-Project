// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FAAmbienceSource.generated.h"

class UAudioComponent;

/**
 * 구역 앰비언스 1개. 맵에 끌어다 놓고 SoundId 만 고르면 된다.
 */
UCLASS(meta = (DisplayName = "FA Ambience Source"))
class FALLINGAPART_API AFAAmbienceSource : public AActor
{
	GENERATED_BODY()

public:
	AFAAmbienceSource();

	/** 지금 울리고 있는 루프를 끄고 새 ID 로 다시 건다. */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Audio")
	void SetAmbienceSound(FName NewSoundId);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** DT_Sound_Music 의 RowName. Amb_Pool · Amb_Gym · Amb_Locker … */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FallingApart|Audio")
	FName SoundId;

	/**
	 * 레벨 시작 몇 초 뒤에 켤지. 0 이면 즉시.
	 * 여러 개가 동시에 시작하면 루프 위상이 겹쳐 맥놀이가 생기므로, 배치할 때 서로 조금씩 어긋나게 두면 좋다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FallingApart|Audio", meta = (ClampMin = "0.0"))
	float StartDelay = 0.0f;

private:
	void StartAmbience();

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> AmbienceComponent;

	FTimerHandle StartTimerHandle;
};
