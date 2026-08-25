// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interact/FAInteractableActor.h"
#include "FALadder.generated.h"

class UStaticMeshComponent;
class USceneComponent;

/**
 * 오르내리는 사다리.
 *
 *   배치 규약 — 액터의 정면(+X)이 벽 반대쪽(사람이 서는 쪽) 을 향하게 놓는다.
 *   이 규약이 어긋나면 캐릭터가 벽을 보고 등반한다.
 */
UCLASS(Blueprintable)
class FALLINGAPART_API AFALadder : public AFAInteractableActor
{
	GENERATED_BODY()

public:
	AFALadder();

	//~ IFAInteractable
	virtual bool CanInteract_Implementation(AFACharacter* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(AFACharacter* Interactor) const override;
	virtual void Interact_Implementation(AFACharacter* Interactor) override;
	//~ End

	/** 등반 방향(월드, 정규화). ClimbBottom → ClimbTop. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Ladder")
	FVector GetClimbUpVector() const;

	UFUNCTION(BlueprintPure, Category = "FallingApart|Ladder")
	FVector GetClimbBottomLocation() const;

	UFUNCTION(BlueprintPure, Category = "FallingApart|Ladder")
	FVector GetClimbTopLocation() const;

	/** 두 점 사이의 길이(cm). 0 이면 배치가 잘못된 것이다. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Ladder")
	float GetClimbLength() const;

	/**
	 * 이 위치가 사다리의 어디쯤인가. 0 = 바닥, 1 = 꼭대기. 구간 밖이면 0~1 을 벗어난다.
	 * 자동 하차 판정이 이 값을 본다.
	 */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Ladder")
	float GetClimbAlpha(const FVector& WorldLocation) const;

	/** 그 위치에서 사다리에 붙을 자리(월드). 구간을 벗어나면 끝으로 물린다. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Ladder")
	FVector GetAttachLocation(const FVector& WorldLocation) const;

	/** 등반 중 캐릭터가 볼 방향. 사다리를 마주 본다. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Ladder")
	FRotator GetClimbFacingRotation() const;

	/** 꼭대기에서 내릴 때 올라설 지점(월드). 난간 너머 바닥이다. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Ladder")
	FVector GetTopExitLocation() const;

public:
	//~ ---------------- 컴포넌트 ----------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LadderMesh;

	/** 등반 구간의 아래 끝. 캐릭터 발이 닿는 높이에 둔다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> ClimbBottom;

	/** 등반 구간의 위 끝. 여기까지 오르면 자동으로 난간 위로 올라선다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> ClimbTop;

	/** 꼭대기에서 내려설 지점. 보통 ClimbTop 보다 앞(-X 반대쪽, 즉 난간 안쪽)에 둔다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> TopExit;

	//~ ---------------- 설정 ----------------

	/** 사다리 중심선에서 캐릭터가 떨어져 붙는 거리(cm). 캡슐 반지름 + 여유. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Ladder", meta = (ClampMin = "0.0"))
	float AttachForwardOffset = 45.0f;

	/** 오르내리는 속도(cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Ladder", meta = (ClampMin = "20.0"))
	float ClimbSpeed = 200.0f;

	/** 꼭대기에서 내릴 때 앞으로 튀어나가는 속도(cm/s). 난간을 넘기 위한 것이다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Ladder", meta = (ClampMin = "0.0"))
	float TopExitLaunchSpeed = 400.0f;

	//~ ---------------- 문구 ----------------
	//~ 진영 제한은 AFAInteractableActor 의 bZombieCanUse / bHumanCanUse 로 한다.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Ladder|Prompt")
	FText ClimbPromptText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Ladder|Prompt")
	FText DismountPromptText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Ladder|Prompt")
	FText NoClimbPromptText;
};
