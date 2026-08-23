// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interact/FAInteractable.h"
#include "FAInteractableActor.generated.h"

class AFACharacter;
class USceneComponent;

/**
 * 배치용 상호작용 액터의 공통 밑판.
 * 블루프린트만으로 만드는 소품은 이 클래스를 부모로 잡고 OnInteracted 이벤트만 구현.
 */
UCLASS(Abstract, Blueprintable)
class FALLINGAPART_API AFAInteractableActor : public AActor, public IFAInteractable
{
	GENERATED_BODY()

public:
	AFAInteractableActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//~ IFAInteractable
	virtual bool CanInteract_Implementation(AFACharacter* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation(AFACharacter* Interactor) const override;
	virtual void Interact_Implementation(AFACharacter* Interactor) override;
	virtual float GetInteractHoldSeconds_Implementation() const override { return InteractHoldSeconds; }
	virtual float GetInteractRange_Implementation() const override { return InteractRange; }
	virtual FVector GetInteractFocusLocation_Implementation() const override;
	virtual bool IsInteractionExclusive_Implementation() const override { return bExclusiveInteraction; }
	virtual AFACharacter* GetInteractionOccupant_Implementation() const override { return InteractionOccupant; }
	virtual bool TryOccupyInteraction_Implementation(AFACharacter* Interactor) override;
	virtual void ReleaseInteraction_Implementation(AFACharacter* Interactor) override;
	//~ End

	/**
	 * 서버에서 상호작용이 실제로 일어난 순간.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|Interact")
	void OnInteracted(AFACharacter* Interactor);

	/** 점유자가 바뀐 순간. 서버·클라 양쪽에서 불린다(하이라이트 끄기 등). */
	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|Interact")
	void OnOccupantChanged(AFACharacter* NewOccupant);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 점유자가 사라졌는지(사망 · 접속 끊김) 확인해 자동으로 풀어준다. 서버 전용. */
	void CheckOccupantAlive();

	UFUNCTION()
	void OnRep_InteractionOccupant();

public:
	/** 전부의 부모가 되는 빈 루트. 메시를 갈아끼워도 액터 원점이 안 흔들리게 하는 용도다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> InteractRoot;

	//~ ---------------- 에디터 설정 ----------------

	/** 끄면 프롬프트도 안 뜨고 상호작용도 안 된다. 연출용으로 잠깐 막을 때. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Interact")
	bool bInteractEnabled = true;

	/** 화면에 뜨는 문구. 비워두면 클래스 기본 문구가 쓰인다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Interact")
	FText InteractPromptText;

	/** 0 이면 즉시. 0 보다 크면 그만큼 키를 붙들고 있어야 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Interact", meta = (ClampMin = "0.0"))
	float InteractHoldSeconds = 0.0f;

	/** 이 물건만의 사거리(cm). 0 이면 캐릭터 컴포넌트의 기본 사거리. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Interact", meta = (ClampMin = "0.0"))
	float InteractRange = 0.0f;

	/** 액터 원점 기준 조준 기준점 오프셋(로컬 좌표). 문 손잡이처럼 원점과 떨어진 물건용. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Interact")
	FVector InteractFocusOffset = FVector::ZeroVector;

	/** 한 번에 한 명만 쓸 수 있는가. 켜면 사용 중에는 남에게 프롬프트가 안 뜬다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Interact")
	bool bExclusiveInteraction = false;

	/** 좀비도 쓸 수 있는가. 끄면 인간만. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Interact")
	bool bZombieCanUse = true;

	/** 인간도 쓸 수 있는가. 끄면 좀비만. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FallingApart|Interact")
	bool bHumanCanUse = true;

protected:
	/**
	 * 지금 이걸 쓰고 있는 사람. bExclusiveInteraction 이 켜진 물건만 채운다.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_InteractionOccupant, BlueprintReadOnly, Category = "FallingApart|Interact")
	TObjectPtr<AFACharacter> InteractionOccupant;

private:
	/** 점유자 생존 확인 타이머. 점유 중일 때만 돈다. */
	FTimerHandle OccupantWatchdogHandle;
};
