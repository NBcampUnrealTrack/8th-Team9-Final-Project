// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interact/FAInteractableActor.h"
#include "FADoor.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UAnimMontage;

/**
 * 여닫이 문.
 */
UCLASS(Blueprintable)
class FALLINGAPART_API AFADoor : public AFAInteractableActor
{
	GENERATED_BODY()

public:
	AFADoor();

	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//~ IFAInteractable
	//~ CanInteract 는 밑판 그대로 쓴다 — 잠긴 문도 "잠겨 있다" 프롬프트는 떠야 하므로
	//~ 잠금은 탐지가 아니라 Interact 에서 막는다.
	virtual FText GetInteractPrompt_Implementation(AFACharacter* Interactor) const override;
	virtual void Interact_Implementation(AFACharacter* Interactor) override;
	//~ End

	/** 서버 전용. 열림/닫힘을 뒤집는다. ByCharacter 가 있으면 그 사람 반대편으로 열린다. */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Door")
	void ToggleDoor(AFACharacter* ByCharacter = nullptr);

	/** 서버 전용. 상태를 직접 지정한다. */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Door")
	void SetDoorOpen(bool bOpen, AFACharacter* ByCharacter = nullptr);

	/** 서버 전용. 잠그거나 푼다. 잠긴 문은 프롬프트가 "잠김" 으로 바뀌고 열리지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Door")
	void SetLocked(bool bLock);

	UFUNCTION(BlueprintPure, Category = "FallingApart|Door")
	bool IsOpen() const { return bIsOpen; }

	UFUNCTION(BlueprintPure, Category = "FallingApart|Door")
	bool IsLocked() const { return bIsLocked; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_IsOpen();

	UFUNCTION()
	void OnRep_IsLocked();

	/** 복제된 상태를 목표 회전으로 바꾸고 보간을 켠다. 서버·클라 공통 경로. */
	void ApplyOpenState(bool bPlaySound);

	/** 상호작용한 캐릭터의 문 열기/닫기 몽타주를 모든 클라이언트에서 재생한다. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayDoorInteractionMontage(AFACharacter* Interactor, bool bOpening);

	/** 지금 상태에 해당하는 피벗의 목표 상대 회전. */
	FRotator GetTargetPivotRotation() const;

	/** 거울 배치 보정 배수. 정상 배치면 1, Scale 부호가 뒤집힌 배치면 -1. */
	float GetMirrorSign() const;

public:
	//~ ---------------- 컴포넌트 ----------------

	/** 문틀. 안 움직인다. 비워둬도 된다(문짝만 있는 배치). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> DoorFrame;

	/** 경첩. 실제로 회전하는 것은 이것이다. 에디터에서 경첩 위치로 옮겨둘 것. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> HingePivot;

	/** 문짝. HingePivot 의 자식이다. Mobility 는 Movable 이어야 한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	//~ ---------------- 설정 ----------------

	/** 열릴 때 도는 각도(도). 부호는 신경 쓰지 않아도 된다 — 방향은 아래 옵션이 정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Door", meta = (ClampMin = "10.0", ClampMax = "170.0"))
	float OpenAngleDegrees = 90.0f;

	/** 회전 속도(도/초). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Door", meta = (ClampMin = "10.0"))
	float OpenSpeedDegreesPerSecond = 220.0f;

	/**
	 * 미는 사람 반대편으로 열릴지.
	 * 끄면 항상 같은 방향(양의 Yaw)으로 열린다 — 벽에 붙어 한쪽으로만 열 수 있는 문에 쓴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Door")
	bool bOpenAwayFromInteractor = true;

	/** 열림 방향을 통째로 뒤집는다. 배치해 보니 반대로 열릴 때 켠다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Door")
	bool bInvertOpenDirection = false;

	/**
	 * 거울 배치(음수 스케일) 자동 보정.
	 * 레벨의 벽·문이 Scale -1 로 뒤집혀 놓인 경우 열림 방향이 반대가 되는 것을 막는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Door")
	bool bAutoFlipForMirroredScale = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Door")
	bool bStartOpen = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Door")
	bool bStartLocked = false;

	//~ ---------------- 캐릭터 애니메이션 ----------------

	/** 캐릭터가 문을 열 때 재생할 몽타주. 비워두면 재생하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallingApart|Door|Animation")
	TObjectPtr<UAnimMontage> DoorOpenMontage;

	/** 캐릭터가 문을 닫을 때 재생할 몽타주. 비워두면 재생하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallingApart|Door|Animation")
	TObjectPtr<UAnimMontage> DoorCloseMontage;

	//~ ---------------- 문구 ----------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Door|Prompt")
	FText OpenPromptText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Door|Prompt")
	FText ClosePromptText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Door|Prompt")
	FText LockedPromptText;

	//~ ---------------- 소리 ----------------
	//~ DT_Sound_Game 의 RowName. 비워두면 소리를 내지 않는다(행이 없어도 경고가 안 뜬다).
	//~ 기본값이 표준 행 이름이라 BP 에서 따로 적을 필요는 없다 — 철문·나무문처럼 재질을 나눌 때만 덮어쓴다.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Door|Audio")
	FName OpenSoundId = TEXT("Door_Open");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Door|Audio")
	FName CloseSoundId = TEXT("Door_Close");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Door|Audio")
	FName LockedSoundId = TEXT("Door_Locked");

protected:
	UPROPERTY(ReplicatedUsing = OnRep_IsOpen, BlueprintReadOnly, Category = "FallingApart|Door")
	bool bIsOpen = false;

	UPROPERTY(ReplicatedUsing = OnRep_IsLocked, BlueprintReadOnly, Category = "FallingApart|Door")
	bool bIsLocked = false;

	/**
	 * 이번에 열린 방향. +1 / -1. 서버가 미는 사람 위치를 보고 정하고 복제한다.
	 * bIsOpen 과 같은 프레임에 도착한다는 보장이 없어서 OnRep 은 둘 다 ApplyOpenState 를 부른다.
	 */
	UPROPERTY(Replicated)
	int32 OpenDirectionSign = 1;

private:
	/** 배치된 그대로의 피벗 상대 회전. 배치 시 Yaw 가 0 이 아닌 경우가 많아 반드시 실측값을 쓴다. */
	FRotator ClosedPivotRotation = FRotator::ZeroRotator;

	FRotator TargetPivotRotation = FRotator::ZeroRotator;

	bool bRotationInitialized = false;

	/** 소리를 낸 마지막 개폐 상태. 접속 직후 초기 복제로 소리가 나는 것을 막는다. */
	bool bLastSoundedOpenState = false;
};
