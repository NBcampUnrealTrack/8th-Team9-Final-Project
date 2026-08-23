// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "FAInteractable.generated.h"

class AFACharacter;

/**
 * 상호작용 가능한 것들의 공통 규약.
 *
 * 문 · 사다리 · 캐비넷(숨기)이 전부 이걸 구현
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UFAInteractable : public UInterface
{
	GENERATED_BODY()
};

class FALLINGAPART_API IFAInteractable
{
	GENERATED_BODY()

public:
	/**
	 * 지금 이 사람이 상호작용할 수 있는가..
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FallingApart|Interact")
	bool CanInteract(AFACharacter* Interactor) const;
	virtual bool CanInteract_Implementation(AFACharacter* Interactor) const { return true; }

	/** 화면 가운데 뜨는 문구. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FallingApart|Interact")
	FText GetInteractPrompt(AFACharacter* Interactor) const;
	virtual FText GetInteractPrompt_Implementation(AFACharacter* Interactor) const
	{
		return NSLOCTEXT("FallingApart", "InteractDefault", "상호작용");
	}

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FallingApart|Interact")
	void Interact(AFACharacter* Interactor);
	virtual void Interact_Implementation(AFACharacter* Interactor) {}

	/**
	 * 길게 누르기 시간(초). 0 이면 누른 순간 즉시 실행.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FallingApart|Interact")
	float GetInteractHoldSeconds() const;
	virtual float GetInteractHoldSeconds_Implementation() const { return 0.0f; }

	/** 이 물건만의 탐지 사거리(cm). 0 이면 컴포넌트 기본값(DetectRange)을 쓴다. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FallingApart|Interact")
	float GetInteractRange() const;
	virtual float GetInteractRange_Implementation() const { return 0.0f; }

	/**
	 * 조준 · 거리 · 가림 판정의 기준점(월드 좌표).
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FallingApart|Interact")
	FVector GetInteractFocusLocation() const;
	virtual FVector GetInteractFocusLocation_Implementation() const { return FVector::ZeroVector; }

	//~ ---------------- 점유 ----------------

	/** 한 번에 한 명만 쓸 수 있는가. 사다리 · 캐비넷은 true, 문은 false. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FallingApart|Interact")
	bool IsInteractionExclusive() const;
	virtual bool IsInteractionExclusive_Implementation() const { return false; }

	/** 지금 이걸 쓰고 있는 사람. 없으면 nullptr. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FallingApart|Interact")
	AFACharacter* GetInteractionOccupant() const;
	virtual AFACharacter* GetInteractionOccupant_Implementation() const { return nullptr; }

	/** 서버 전용. 점유를 잡는다. 이미 남이 쓰고 있으면 false — 이때 Interact() 는 불리지 않는다. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FallingApart|Interact")
	bool TryOccupyInteraction(AFACharacter* Interactor);
	virtual bool TryOccupyInteraction_Implementation(AFACharacter* Interactor) { return true; }

	/**
	 * 서버 전용. 점유를 놓는다.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FallingApart|Interact")
	void ReleaseInteraction(AFACharacter* Interactor);
	virtual void ReleaseInteraction_Implementation(AFACharacter* Interactor) {}
};
