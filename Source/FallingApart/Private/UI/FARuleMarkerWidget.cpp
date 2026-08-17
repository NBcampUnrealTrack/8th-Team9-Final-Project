// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FARuleMarkerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Rule/FARuleSettings.h"

namespace FARuleMarkerUI
{
	/** 임시 레이아웃의 아이콘 한 칸 크기(픽셀). 머리 위라 HUD 칸보다 작다. */
	static constexpr float IconSize = 26.0f;

	static constexpr float DotSize = 10.0f;

	static constexpr float Gap = 4.0f;
}

TSharedRef<SWidget> UFARuleMarkerWidget::RebuildWidget()
{
	if (IsDesignTime())
	{
		return Super::RebuildWidget();
	}

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildFallbackLayout();
	}

	GatherMarkerWidgets();
	ApplyRulesToWidgets();

	return Super::RebuildWidget();
}

void UFARuleMarkerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	GatherMarkerWidgets();
	ApplyRulesToWidgets();
}

void UFARuleMarkerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UpdateAnimatedColors(InDeltaTime);
}

void UFARuleMarkerWidget::UpdateMarker(const TArray<FFARuleDisplayInfo>& InRules, float Scale, bool bInAbbreviated)
{
	// 거리 감쇠 — 스크린 스페이스 위젯은 거리에 따라 작아지지 않으므로 렌더 스케일로 직접 줄인다.
	SetRenderScale(FVector2D(Scale, Scale));

	bool bContentChanged = InRules.Num() != LastRuleIds.Num();
	if (!bContentChanged)
	{
		for (int32 Index = 0; Index < InRules.Num(); ++Index)
		{
			if (InRules[Index].RuleId != LastRuleIds[Index])
			{
				bContentChanged = true;
				break;
			}
		}
	}

	const bool bModeChanged = (bInAbbreviated != bAbbreviated) || !bHasAbbreviatedBaseline;

	bAbbreviated = bInAbbreviated;
	bHasAbbreviatedBaseline = true;

	if (!bContentChanged && !bModeChanged)
	{
		return;
	}

	if (bContentChanged)
	{
		Rules = InRules;

		LastRuleIds.Reset(Rules.Num());
		for (const FFARuleDisplayInfo& Rule : Rules)
		{
			LastRuleIds.Add(Rule.RuleId);
		}
	}

	ApplyRulesToWidgets();

	if (bContentChanged)
	{
		OnMarkerRulesChanged(Rules);
	}

	if (bModeChanged)
	{
		OnAbbreviatedChanged(bAbbreviated);
	}
}

//~ ---------------- 그리기 ----------------

void UFARuleMarkerWidget::GatherMarkerWidgets()
{
	IconImages = { Img_Rule0, Img_Rule1, Img_Rule2, Img_Rule3 };
	DotImages = { Img_Dot0, Img_Dot1, Img_Dot2 };
}

void UFARuleMarkerWidget::ApplyRulesToWidgets()
{
	if (Panel_Icons)
	{
		Panel_Icons->SetVisibility(bAbbreviated ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (Panel_Dots)
	{
		Panel_Dots->SetVisibility(bAbbreviated ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	for (int32 Index = 0; Index < IconImages.Num(); ++Index)
	{
		UImage* Icon = IconImages[Index];
		if (!Icon)
		{
			continue;
		}

		if (!Rules.IsValidIndex(Index))
		{
			Icon->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}

		const FFARuleDisplayInfo& Rule = Rules[Index];

		// 아이콘이 없는 카드는 계열 색 사각형으로 대신한다 — 칸을 비우면 장수를 못 센다.
		if (UTexture2D* Texture = Rule.Icon.IsNull() ? nullptr : Rule.Icon.LoadSynchronous())
		{
			Icon->SetBrushFromTexture(Texture);
			Icon->SetColorAndOpacity(bTintIconsByCategory ? Rule.CategoryColor : FLinearColor::White);
		}
		else
		{
			Icon->SetBrushFromTexture(nullptr);
			Icon->SetColorAndOpacity(Rule.CategoryColor);
		}

		Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	// 축약 — 앞에서부터 3개 계열만 색 점으로. 4장을 다 보여줘도 멀리서는 구분이 안 된다.
	for (int32 Index = 0; Index < DotImages.Num(); ++Index)
	{
		UImage* Dot = DotImages[Index];
		if (!Dot)
		{
			continue;
		}

		if (!Rules.IsValidIndex(Index))
		{
			Dot->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}

		Dot->SetColorAndOpacity(Rules[Index].CategoryColor);
		Dot->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UFARuleMarkerWidget::UpdateAnimatedColors(float InDeltaTime)
{
	AnimTime += InDeltaTime;

	for (int32 Index = 0; Index < Rules.Num(); ++Index)
	{
		if (!Rules[Index].bAnimatedColor)
		{
			continue;
		}

		const FLinearColor Color = UFARuleSettings::GetGambleColor(AnimTime);

		if (IconImages.IsValidIndex(Index) && IconImages[Index] && !bAbbreviated)
		{
			IconImages[Index]->SetColorAndOpacity(Color);
		}

		if (DotImages.IsValidIndex(Index) && DotImages[Index] && bAbbreviated)
		{
			DotImages[Index]->SetColorAndOpacity(Color);
		}
	}
}

//~ ---------------- WBP 가 없을 때 쓰는 임시 레이아웃 ----------------

void UFARuleMarkerWidget::BuildFallbackLayout()
{
	bUsingFallbackLayout = true;

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = RootCanvas;

	//~ 아이콘 줄과 점 줄을 같은 자리에 겹쳐 둔다 — 둘 중 하나만 보인다.
	UHorizontalBox* IconRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UCanvasPanelSlot* RowSlot = RootCanvas->AddChildToCanvas(IconRow))
	{
		RowSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		RowSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		RowSlot->SetAutoSize(true);
	}
	Panel_Icons = IconRow;

	for (int32 Index = 0; Index < MaxIcons; ++Index)
	{
		UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Icon->SetDesiredSizeOverride(FVector2D(FARuleMarkerUI::IconSize, FARuleMarkerUI::IconSize));
		Icon->SetVisibility(ESlateVisibility::Collapsed);

		if (UHorizontalBoxSlot* IconSlot = IconRow->AddChildToHorizontalBox(Icon))
		{
			IconSlot->SetPadding(FMargin(0.0f, 0.0f, FARuleMarkerUI::Gap, 0.0f));
		}

		switch (Index)
		{
		case 0: Img_Rule0 = Icon; break;
		case 1: Img_Rule1 = Icon; break;
		case 2: Img_Rule2 = Icon; break;
		default: Img_Rule3 = Icon; break;
		}
	}

	UHorizontalBox* DotRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UCanvasPanelSlot* RowSlot = RootCanvas->AddChildToCanvas(DotRow))
	{
		RowSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		RowSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		RowSlot->SetAutoSize(true);
	}
	DotRow->SetVisibility(ESlateVisibility::Collapsed);
	Panel_Dots = DotRow;

	for (int32 Index = 0; Index < MaxDots; ++Index)
	{
		UImage* Dot = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Dot->SetDesiredSizeOverride(FVector2D(FARuleMarkerUI::DotSize, FARuleMarkerUI::DotSize));
		Dot->SetVisibility(ESlateVisibility::Collapsed);

		if (UHorizontalBoxSlot* DotSlot = DotRow->AddChildToHorizontalBox(Dot))
		{
			DotSlot->SetPadding(FMargin(0.0f, 0.0f, FARuleMarkerUI::Gap, 0.0f));
		}

		switch (Index)
		{
		case 0: Img_Dot0 = Dot; break;
		case 1: Img_Dot1 = Dot; break;
		default: Img_Dot2 = Dot; break;
		}
	}
}
