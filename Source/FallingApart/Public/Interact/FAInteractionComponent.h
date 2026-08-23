// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FAInteractionComponent.generated.h"

class AFACharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFAInteractTargetChanged, AActor*, NewTarget, const FText&, Prompt);

/**
 * 캐릭터 쪽 상호작용 창구 — 탐지 · 프롬프트 · 길게 누르기 · 서버 요청
 * HUD 는 UFAHUDWidget 이 이 컴포넌트를 폴링해 그린다
 */
UCLASS(ClassGroup = (FallingApart), meta = (BlueprintSpawnableComponent))
class FALLINGAPART_API UFAInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFAInteractionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	//~ ---------------- 입력 ----------------
	//~ AFACharacter 의 InteractAction 바인딩이 부른다.

	/** 상호작용 키를 누른 순간. 즉시형이면 여기서 바로 서버로 간다. */
	void OnInteractPressed();

	/** 상호작용 키를 뗀 순간. 길게 누르기 도중이면 취소된다. */
	void OnInteractReleased();

	//~ ---------------- HUD 폴링 진입점 ----------------

	/** 지금 조준하고 있는 상호작용 대상. 없으면 nullptr. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Interact")
	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

	UFUNCTION(BlueprintPure, Category = "FallingApart|Interact")
	bool HasTarget() const { return CurrentTarget.IsValid(); }

	/** 화면에 띄울 문구. 대상이 없으면 빈 문자열. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Interact")
	FText GetPromptText() const { return CurrentPrompt; }

	/** 길게 누르기 진행도 0~1. 즉시형이거나 누르고 있지 않으면 0. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Interact")
	float GetHoldProgress() const;

	/** 지금 길게 누르기 진행 중인가. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Interact")
	bool IsHolding() const { return bIsHolding; }

	/** 대상이 바뀔 때만 발생. WBP 가 폴링 대신 이벤트로 받고 싶을 때 쓴다. */
	UPROPERTY(BlueprintAssignable, Category = "FallingApart|Interact")
	FFAInteractTargetChanged OnTargetChanged;

	void RefreshTickState();

protected:
	virtual void BeginPlay() override;

	/** 소유 캐릭터. 아니면 nullptr. */
	AFACharacter* GetFACharacter() const;

	/**
	 * 이 캐릭터가 지금 상호작용을 할 수 있는 상태인가(포획 · QTE · 경직 검사).
	 */
	bool CanCharacterInteract() const;

	/** 시야 기준점과 방향. 1인칭/3인칭 모두 카메라를 기준으로 한다. */
	void GetViewPoint(FVector& OutLocation, FVector& OutDirection) const;

	/** 주변에서 가장 알맞은 대상을 고른다. 없으면 nullptr. */
	AActor* FindBestTarget() const;

	/** 대상과 문구를 갱신하고, 바뀌었으면 이벤트를 낸다. */
	void SetCurrentTarget(AActor* NewTarget);

	/** 조준하고 있는 대상의 문구를 다시 읽는다(문 열기 ↔ 문 닫기처럼 상태에 따라 바뀐다). */
	void RefreshPrompt();

	/** 길게 누르기를 접는다. 서버 쪽 진행 중이던 것도 같이 취소한다. */
	void CancelHold();

	/** 서버에 실행을 요청한다. 즉시형은 즉시, 길게 누르기형은 채워진 뒤. */
	UFUNCTION(Server, Reliable)
	void Server_RequestInteract(AActor* Target);

	/** 길게 누르기 시작을 서버에 알린다 — 시간은 서버가 잰다. */
	UFUNCTION(Server, Reliable)
	void Server_BeginHold(AActor* Target);

	/** 길게 누르기 취소. */
	UFUNCTION(Server, Reliable)
	void Server_CancelHold();

	/** 서버가 잰 시간이 다 찼을 때. */
	void ServerCompleteHold();

	/**
	 * 서버 재검증 — 클라가 보낸 액터를 실제로 쓸 수 있는지 처음부터 다시 본다.
	 * 거리는 클라 예측 오차를 감안해 DetectRange * ServerRangeTolerance 까지 허용한다.
	 */
	bool ValidateServerInteract(AActor* Target) const;

	/** 대상이 요구하는 사거리. 0 이면 컴포넌트 기본값. */
	float GetTargetRange(const AActor* Target) const;

public:
	//~ ---------------- 에디터 설정 ----------------

	/** 기본 탐지 사거리(cm). 물건이 GetInteractRange 로 따로 정하면 그쪽이 이긴다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FallingApart|Interact", meta = (ClampMin = "50.0"))
	float DetectRange = 250.0f;

	/**
	 * 시야 중심에서 이 각도(도) 안에 들어와야 대상으로 잡는다.
	 * 넓히면 옆에 있는 문이 자꾸 잡히고, 좁히면 3인칭에서 조준이 까다로워진다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FallingApart|Interact", meta = (ClampMin = "5.0", ClampMax = "90.0"))
	float DetectHalfAngleDegrees = 55.0f;

	/** 탐지 주기(초). 매 프레임 트레이스할 이유가 없다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FallingApart|Interact", meta = (ClampMin = "0.0"))
	float DetectInterval = 0.1f;

	/** 벽 뒤의 물건은 잡지 않는다. 끄면 가림 검사를 건너뛴다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FallingApart|Interact")
	bool bRequireLineOfSight = true;

	/** 서버 재검증에서 허용하는 사거리 배수. 클라와 서버의 위치 차이를 흡수한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "FallingApart|Interact", meta = (ClampMin = "1.0", ClampMax = "2.0"))
	float ServerRangeTolerance = 1.35f;

private:
	/** 지금 조준 중인 대상. 로컬 화면 전용이라 복제하지 않는다. */
	TWeakObjectPtr<AActor> CurrentTarget;

	FText CurrentPrompt;

	/** 마지막 탐지 이후 흐른 시간(초). */
	float DetectTimer = 0.0f;

	//~ 길게 누르기 — 클라는 게이지를 그리려고 재고, 서버는 판정하려고 잰다.
	bool bIsHolding = false;
	float HoldElapsed = 0.0f;
	float HoldDuration = 0.0f;

	/** 서버가 잰 길게 누르기. 클라 타이머를 믿지 않기 위해 따로 둔다. */
	TWeakObjectPtr<AActor> ServerHoldTarget;
	float ServerHoldDuration = 0.0f;
	FTimerHandle ServerHoldTimerHandle;
};
