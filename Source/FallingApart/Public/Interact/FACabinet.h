// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interact/FADoor.h"
#include "FACabinet.generated.h"

class USceneComponent;

/**
 * 숨을 수 있는 캐비넷.
 *
 * 문(AFADoor)의 파생
 */
UCLASS(Blueprintable)
class FALLINGAPART_API AFACabinet : public AFADoor
{
	GENERATED_BODY()

public:
	AFACabinet();

	//~ IFAInteractable
	virtual bool CanInteract_Implementation(AFACharacter* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(AFACharacter* Interactor) const override;
	virtual float GetInteractHoldSeconds_Implementation(AFACharacter* Interactor) const override;
	virtual void Interact_Implementation(AFACharacter* Interactor) override;
	virtual bool TryOccupyInteraction_Implementation(AFACharacter* Interactor) override;
	virtual void ReleaseInteraction_Implementation(AFACharacter* Interactor) override;
	//~ End

	/** 지금 안에 숨어 있는 사람. 없으면 nullptr. 점유자(InteractionOccupant)와 같은 값. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Cabinet")
	AFACharacter* GetHider() const { return InteractionOccupant; }

	UFUNCTION(BlueprintPure, Category = "FallingApart|Cabinet")
	bool HasHider() const { return InteractionOccupant != nullptr; }

	/** 숨은 사람이 서 있을 자리(월드). */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Cabinet")
	FVector GetHideLocation() const;

	/** 숨은 동안 바라볼 방향. 캐비넷 정면(+X) 바깥쪽을 본다. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Cabinet")
	FRotator GetHideRotation() const;

	/** 숨어 있는 동안 카메라 붐이 매달릴 자리. 없으면 nullptr. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Cabinet")
	USceneComponent* GetHideCameraAnchor() const { return HideCameraSpot; }

	/** 나올 때 서게 되는 자리(월드). 캐비넷 앞 빈 바닥이어야 한다. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Cabinet")
	FVector GetExitLocation() const;

	UFUNCTION(BlueprintPure, Category = "FallingApart|Cabinet")
	FRotator GetExitRotation() const;

	/**
	 * 서버 전용. 안에 있는 사람을 밖으로 내보낸다.
	 * 좀비가 문을 연 경우와, 숨은 사람이 스스로 나오는 경우 모두 이 길을 지난다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Cabinet")
	void ServerEjectHider();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 열린 채로 방치된 캐비넷을 자동으로 닫는다. */
	void HandleAutoClose();

	/** 개폐 상태가 바뀔 때마다 자동 닫기 타이머를 다시 잡는다. */
	void UpdateAutoCloseTimer();

	/** 이 사람이 좀비인가. ASC 가 아직 없으면 false(= 인간 취급). */
	bool IsZombie(const AFACharacter* Character) const;

	/**
	 * 문짝이 캐비넷 바깥(+X)으로 나가는 회전 부호를 실제 배치에서 계산
	 */
	int32 ComputeOutwardOpenSign() const;

public:
	//~ ---------------- 컴포넌트 ----------------

	/**
	 * 숨어 있는 동안 카메라가 놓이는 자리. 캐비넷 위로 올려둔다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> HideCameraSpot;

	/** 숨은 사람이 서는 자리. 캐비넷 안쪽 바닥에 둔다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> HideSpot;

	/**
	 * 나올 때 서는 자리. 캐비넷 앞쪽 빈 바닥에 둔다.
	 * 벽 속이나 다른 물건 안에 두면 나오다가 낀다 — 사다리 TopExit 과 같은 함정.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> ExitSpot;

	//~ ---------------- 설정 ----------------

	/**
	 * 문이 항상 캐비넷 바깥으로 열리게 한다. 시작할 때 실제 배치를 보고 회전 부호를 정한다.
	 * 끄면 문(AFADoor)의 기본 규칙을 그대로 따른다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Cabinet")
	bool bAutoOpenOutward = true;

	/** 좀비가 문을 여는 데 붙들고 있어야 하는 시간(초). 0 이면 즉시. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Cabinet", meta = (ClampMin = "0.0"))
	float ZombieOpenHoldSeconds = 1.0f;

	/** 인간이 숨는 데 걸리는 시간(초). 기본은 즉시 — 쫓기는 중에 붙들고 있을 여유가 없다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Cabinet", meta = (ClampMin = "0.0"))
	float HideHoldSeconds = 0.0f;

	/**
	 * 열린 채로 방치되면 이 시간(초) 뒤에 저절로 닫힌다. 0 이면 안 닫힌다.
	 * 이게 없으면 좀비가 한 번 훑고 지나간 구역의 캐비넷이 전부 못 쓰는 물건이 된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Cabinet", meta = (ClampMin = "0.0"))
	float AutoCloseSeconds = 5.0f;

	//~ ---------------- 문구 ----------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Cabinet|Prompt")
	FText HidePromptText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Cabinet|Prompt")
	FText ExitPromptText;

private:
	FTimerHandle AutoCloseTimerHandle;
};
