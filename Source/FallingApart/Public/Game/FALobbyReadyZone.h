// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FALobbyReadyZone.generated.h"

class UBoxComponent;

/**
 *	로비의 '준비 구역'. 이 박스 안에 서 있는 동안만 해당 플레이어가 Ready 로 잡힌다.
 *	레벨에 그냥 배치해서 쓴다.
 */
UCLASS()
class FALLINGAPART_API AFALobbyReadyZone : public AActor
{
	GENERATED_BODY()

public:
	AFALobbyReadyZone();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

private:
	/** 구역 안에 있는 한 명. 체류 타이머와, 그 Pawn 을 조종하던 컨트롤러를 같이 들고 있는다. */
	struct FZoneOccupant
	{
		TWeakObjectPtr<APlayerController> Controller;
		FTimerHandle DwellTimer;
	};

	/** 체류 시간이 다 찬 뒤 실제로 Ready 를 켠다. 그 사이 나가버렸으면 아무것도 안 한다. */
	void HandleDwellElapsed(TWeakObjectPtr<APawn> WeakPawn);

	/** GameMode 에 Ready 상태를 넘긴다. 서버 전용. */
	void ApplyReady(APlayerController* PlayerController, bool bReady);

	UPROPERTY(VisibleAnywhere, Category = "FallingApart|Lobby")
	TObjectPtr<UBoxComponent> ZoneVolume;

	/**
	 * 이 시간만큼 계속 안에 있어야 Ready 가 켜진다.
	 * 0 이면 즉시. 가장자리를 스치듯 지나갈 때 Ready 가 깜빡이는 걸 막는 용도라 짧게 둔다.
	 */
	UPROPERTY(EditAnywhere, Category = "FallingApart|Lobby", meta = (ClampMin = "0.0"))
	float ReadyDwellSeconds = 0.35f;

	/**
	 * 지금 구역 안에 있는 Pawn 들.
	 */
	TMap<TWeakObjectPtr<APawn>, FZoneOccupant> Occupants;
};
