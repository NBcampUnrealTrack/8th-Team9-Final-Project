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

	UFUNCTION()
	void OnRep_OpenDirectionSign();

	/** 복제된 상태를 목표 회전으로 바꾸고 보간을 켠다. 서버·클라 공통 경로. */
	void ApplyOpenState(bool bPlaySound);

	/** 상호작용한 캐릭터의 문 열기/닫기 몽타주를 모든 클라이언트에서 재생한다. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayDoorInteractionMontage(AFACharacter* Interactor, bool bOpening);

	/** 지금 상태에 해당하는 피벗의 목표 상대 회전. */
	FRotator GetTargetPivotRotation() const;

	/**
	 * 미는 사람 반대편으로 열리려면 어느 쪽으로 돌아야 하는가. +1 / -1 (월드 Yaw 기준).
	 * 경첩에서 문짝으로 향하는 실측 방향을 기준으로 잡으므로 배치 회전이나 BP 마다
	 * 다른 메시 정렬에 영향을 받지 않는다.
	 */
	int32 ComputeOpenDirectionSign(const AFACharacter* ByCharacter) const;

	/**
	 * 닫힌 자세에서 경첩 → 문짝 중심 방향을 한 번 재둔다(수평 성분만, 월드 공간).
	 * 열린 뒤에 재면 이미 돌아간 자세가 잡히므로 반드시 BeginPlay 의 배치 그대로에서 잰다.
	 */
	void CacheClosedLeafDirection();

	/** 거울 배치 보정 배수. 정상 배치면 1, Scale 부호가 뒤집힌 배치면 -1. */
	float GetMirrorSign() const;

	/** 회전 중에는 문짝의 Pawn 충돌을 끄고, 멈추면 되살린다. 서버·클라가 각자 로컬로 한다. */
	void UpdatePawnCollisionForMotion(float DeltaTime, bool bMoving);

	void SetPawnCollisionSuppressed(bool bSuppress);

	/** 문짝 자리에 캐릭터가 들어와 있는가. 충돌을 되살리는 순간 밀어내는 것을 막기 위한 검사. */
	bool IsPawnOverlappingDoorMesh() const;

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

	//~ ---------------- 충돌 ----------------

	/**
	 * 여닫는 동안 문짝이 캐릭터를 밀지 않게 한다(Pawn 채널만 끈다 — 총알·시야는 그대로 막는다).
	 * 회전이 끝나면 되살리되, 문짝 자리에 사람이 서 있으면 비켜날 때까지 기다린다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Door|Collision")
	bool bDisablePawnCollisionWhileMoving = true;

	/**
	 * 회전이 끝난 뒤 사람이 안 비켜도 충돌을 되살리기까지의 시간(초).
	 * 0 이면 즉시 되살린다(엔진이 밀어낸다). 문틈에 서서 통과하는 것을 막는 안전장치다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Door|Collision", meta = (ClampMin = "0.0"))
	float PawnCollisionRestoreGraceSeconds = 2.0f;

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
	 *
	 * bIsOpen 보다 뒤에 선언돼 있어 같은 번치로 와도 OnRep_IsOpen 이 먼저 불린다 —
	 * 그 시점에는 아직 이전 방향이 들어 있다. 그래서 이쪽에도 OnRep 을 달아 값이 도착한 뒤
	 * 목표 회전을 한 번 더 잡는다. 없으면 클라이언트는 늘 직전(첫 개폐면 기본값 +1) 방향으로 연다.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_OpenDirectionSign)
	int32 OpenDirectionSign = 1;

private:
	/** 배치된 그대로의 피벗 상대 회전. 배치 시 Yaw 가 0 이 아닌 경우가 많아 반드시 실측값을 쓴다. */
	FRotator ClosedPivotRotation = FRotator::ZeroRotator;

	FRotator TargetPivotRotation = FRotator::ZeroRotator;

	bool bRotationInitialized = false;

	/** 닫힌 자세에서 경첩 → 문짝 중심 방향(수평, 월드). CacheClosedLeafDirection 이 채운다. */
	FVector ClosedLeafDirWorld = FVector::ZeroVector;

	/** 문짝 중심이 경첩 바로 위라 방향을 못 잰 경우 false. 그때는 액터 앞뒤로 가르는 예전 방식으로 돈다. */
	bool bLeafDirectionValid = false;

	/** 소리를 낸 마지막 개폐 상태. 접속 직후 초기 복제로 소리가 나는 것을 막는다. */
	bool bLastSoundedOpenState = false;

	/** 배치 당시 문짝의 Pawn 채널 응답. 충돌을 되살릴 때 이 값으로 돌린다. */
	TEnumAsByte<ECollisionResponse> SavedPawnCollisionResponse = ECR_Block;

	bool bPawnCollisionSuppressed = false;

	/** 회전이 멈춘 뒤 아직 충돌을 못 되살린 채 흐른 시간. */
	float PawnCollisionSuppressedSeconds = 0.0f;
};
