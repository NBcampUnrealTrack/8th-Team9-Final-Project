// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Rule/FARuleTypes.h"
#include "FARuleMarkerWidget.generated.h"

class UBorder;
class UHorizontalBox;
class UImage;

/**
 * 다른 플레이어 머리 위에 뜨는 보유 룰 아이콘 (3D 위젯 베이스).
 *
 * WBP 없이도 동작한다 — 디자이너 트리가 비어 있으면 RebuildWidget() 이 임시 레이아웃을 조립한다.
 */
UCLASS()
class FALLINGAPART_API UFARuleMarkerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 그릴 수 있는 최대 아이콘 수. HUD 4칸과 같아야 한다. */
	static constexpr int32 MaxIcons = 4;

	/** 축약 표시에 쓰는 점 개수. */
	static constexpr int32 MaxDots = 3;

	/**
	 * 표시 내용 갱신. AFACharacter 가 매 프레임 부르지만 값이 실제로 바뀔 때만 이벤트가 나간다.
	 *
	 * @param InRules       보유 순서대로. 0~4장
	 * @param Scale         거리 감쇠 배율. 1 이면 원래 크기
	 * @param bAbbreviated  true 면 아이콘 대신 계열 색 점 3개
	 */
	void UpdateMarker(const TArray<FFARuleDisplayInfo>& InRules, float Scale, bool bAbbreviated);

	UFUNCTION(BlueprintPure, Category = "FallingApart|Rule")
	const TArray<FFARuleDisplayInfo>& GetRules() const { return Rules; }

	//~ UI 담당이 WBP 에서 구현할 것.
	/** 보유 룰이 바뀌었을 때. 0~4장이다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|Rule")
	void OnMarkerRulesChanged(const TArray<FFARuleDisplayInfo>& InRules);

	/** 축약 표시로 넘어가거나 돌아올 때. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|Rule")
	void OnAbbreviatedChanged(bool bInAbbreviated);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 아이콘을 계열 색으로 칠할지. 아이콘 텍스처가 이미 색을 갖고 있다면 꺼둘 것. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Rule")
	bool bTintIconsByCategory = true;

	//~ WBP 에 같은 이름의 위젯이 있으면 자동으로 물린다. 없으면 그 칸만 건너뛴다.
	/** 아이콘 줄 전체. 축약 표시로 넘어가면 통째로 숨긴다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Rule")
	TObjectPtr<UWidget> Panel_Icons;

	/** 점 3개 줄. 가까워지면 통째로 숨긴다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Rule")
	TObjectPtr<UWidget> Panel_Dots;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Rule")
	TObjectPtr<UImage> Img_Rule0;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Rule")
	TObjectPtr<UImage> Img_Rule1;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Rule")
	TObjectPtr<UImage> Img_Rule2;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Rule")
	TObjectPtr<UImage> Img_Rule3;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Rule")
	TObjectPtr<UImage> Img_Dot0;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Rule")
	TObjectPtr<UImage> Img_Dot1;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Rule")
	TObjectPtr<UImage> Img_Dot2;

private:
	void GatherMarkerWidgets();

	/** Rules 내용을 물려 있는 위젯에 그린다. */
	void ApplyRulesToWidgets();

	/** 도박(G) 카드의 무지개 색만 매 프레임 갱신한다. */
	void UpdateAnimatedColors(float InDeltaTime);

	/** WBP 가 없을 때만 쓰는 임시 레이아웃. */
	void BuildFallbackLayout();

	UPROPERTY(Transient)
	TArray<FFARuleDisplayInfo> Rules;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> IconImages;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> DotImages;

	/** 직전에 그린 ID 목록 — 매 프레임 텍스처를 다시 물리지 않기 위한 비교 기준. */
	TArray<FName> LastRuleIds;

	float AnimTime = 0.0f;

	bool bAbbreviated = false;
	bool bHasAbbreviatedBaseline = false;

	bool bUsingFallbackLayout = false;
};
