// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Rule/FARuleTypes.h"
#include "FARuleSlotsWidget.generated.h"

class UBorder;
class UFAAbilitySystemComponent;
class UImage;
class UTextBlock;

/**
 * 좌하단 보유 룰 아이콘 4칸.
 *
 * 내 ASC 의 OnActiveRulesChanged 를 직접 받아 보유 순서대로 그린다. 상한(기본 4장)에 닿으면
 * 가장 오래된 칸이 깜빡이며 '다음 드래프트에 이게 사라진다'를 예고한다.
 *
 * WBP 없이도 동작한다 — 디자이너 트리가 비어 있으면 RebuildWidget() 이 임시 레이아웃을 조립한다.
 */
UCLASS()
class FALLINGAPART_API UFARuleSlotsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 그릴 수 있는 최대 칸 수. UFARuleSettings::MaxActiveRules 가 이보다 크면 앞에서 자른다. */
	static constexpr int32 MaxSlots = 4;

	/** 보유 순서대로 정렬된 현재 룰. 없으면 빈 배열. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Rule")
	const TArray<FFARuleDisplayInfo>& GetSlots() const { return Slots; }

	/** 다음 드래프트에 밀려날 칸의 인덱스. 상한에 안 닿았으면 INDEX_NONE(-1). */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Rule")
	int32 GetExpiringSlotIndex() const { return ExpiringSlotIndex; }

	/**
	 * 보유 룰이 바뀔 때. 슬롯은 0~4개다 — 4개라고 가정하지 말 것.
	 * @param InExpiringSlotIndex 소멸 예고 칸. 없으면 -1
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|Rule")
	void OnRuleSlotsChanged(const TArray<FFARuleDisplayInfo>& InSlots, int32 InExpiringSlotIndex);

	/** 룰 1장이 새로 붙었을 때 (연출·소리는 C++ 이 이미 낸다 — 추가 연출용). */
	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|Rule")
	void OnRuleAdded(const FFARuleDisplayInfo& Rule);

	/** 룰 1장이 빠졌을 때. 감염으로 진영 룰이 통째로 빠지면 여러 번 불린다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|Rule")
	void OnRuleRemoved(const FFARuleDisplayInfo& Rule);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 소멸 예고 칸의 깜빡임 주기(초). 0 이하면 깜빡이지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Rule")
	float ExpiringBlinkPeriod = 0.8f;

	/** 깜빡임이 내려가는 최저 불투명도. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Rule")
	float ExpiringBlinkMinOpacity = 0.25f;

	/**
	 * 칸 배경을 계열 색으로 칠할지. WBP 에서 색을 직접 잡았다면 꺼둘 것.
	 * 임시 레이아웃에서는 이 값과 무관하게 항상 칠한다 — 아이콘이 없어 색이 유일한 단서다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Rule")
	bool bTintSlotsByCategory = true;

	/** 룰이 붙고 빠질 때 소리를 낼지. 드래프트 창이 이미 소리를 내는 구성이면 꺼둘 것. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Rule|Audio")
	bool bPlayRuleSounds = true;

	//~ 슬롯 0
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Rule")
	TObjectPtr<UImage> Img_Slot0Background;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Rule")
	TObjectPtr<UImage> Img_Slot0Icon;
	
	//~ 슬롯 1
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Rule")
	TObjectPtr<UImage> Img_Slot1Background;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Rule")
	TObjectPtr<UImage> Img_Slot1Icon;
	
	//~ 슬롯 2
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Rule")
	TObjectPtr<UImage> Img_Slot2Background;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Rule")
	TObjectPtr<UImage> Img_Slot2Icon;
	
	//~ 슬롯 3
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Rule")
	TObjectPtr<UImage> Img_Slot3Background;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Rule")
	TObjectPtr<UImage> Img_Slot3Icon;

private:
	UFUNCTION()
	void HandleActiveRulesChanged();

	/**
	 * 내 폰의 ASC 를 다시 묶는다. 폰이 바뀌지 않았으면 아무것도 하지 않는다.
	 * 감염돼도 폰은 그대로지만, 늦은 빙의·리스폰으로 폰이 갈릴 수 있어 매 프레임 포인터만 비교한다.
	 */
	void RefreshBinding();

	/** ASC 의 보유 목록을 읽어 Slots 를 다시 만든다. 바뀐 게 없으면 이벤트도 내지 않는다. */
	void RefreshSlots(bool bForceEvent);

	/** 개별 Slot0 … 을 인덱스 배열로 모은다. RebuildWidget · NativeConstruct 에서 호출. */
	void GatherSlotWidgets();

	/** Slots 내용을 물려 있는 위젯에 그린다. */
	void ApplySlotsToWidgets();

	/** 깜빡임·무지개처럼 매 프레임 움직이는 색만 갱신한다. */
	void UpdateAnimatedVisuals(float InDeltaTime);

	/** 룰 추가·제거 소리. 로컬 화면 소리라 여기서 낸다. */
	void PlayRuleAddedSound(const FFARuleDisplayInfo& Rule);
	void PlayRuleRemovedSound();

	UPROPERTY(Transient)
	TArray<FFARuleDisplayInfo> Slots;

	UPROPERTY(Transient)
	TWeakObjectPtr<UFAAbilitySystemComponent> BoundASC;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> SlotBackgrounds;
	
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> SlotIcons;

	/** 직전 프레임의 보유 ID — 추가·제거를 구분해 소리와 이벤트를 나누는 기준. */
	TArray<FName> LastRuleIds;

	int32 ExpiringSlotIndex = INDEX_NONE;

	/** 깜빡임·무지개용 누적 시간. */
	float AnimTime = 0.0f;

	/** 임시 레이아웃으로 조립한 상태인가 — 색·숨김을 무조건 적용해도 되는지 판단용. */
	bool bUsingFallbackLayout = false;

	/** 첫 갱신인가. 접속 직후 이미 들고 있던 룰로는 '새로 붙었다' 소리를 내지 않는다. */
	bool bHasBaseline = false;
};
