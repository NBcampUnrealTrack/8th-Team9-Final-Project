// Fill out your copyright notice in the Description page of Project Settings.
//
// WBP 를 지정하지 않았을 때 C++ 이 조립하는 임시 로딩 화면.

#include "LoadingScreenWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CircularThrobber.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

namespace LSKFallback
{
	/** 로딩 화면은 뒤가 비쳐서는 안 된다 — 알파 1. 트래블 도중의 깨진 프레임을 가리는 게 일이다. */
	static const FLinearColor BackgroundColor(0.02f, 0.02f, 0.03f, 1.f);
	static const FLinearColor TitleColor(0.92f, 0.92f, 0.95f, 1.f);
	static const FLinearColor TipColor(0.62f, 0.62f, 0.68f, 1.f);

	static constexpr float BarWidth = 420.f;
	static constexpr float BarHeight = 6.f;
}

void ULSKLoadingScreenWidget::BuildFallbackLayout()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("LSK_Root"));
	WidgetTree->RootWidget = Root;

	// ---- 배경: 화면 전체를 덮는 단색 ----
	UImage* Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("LSK_Background"));
	Background->SetColorAndOpacity(LSKFallback::BackgroundColor);

	if (UCanvasPanelSlot* BackgroundSlot = Root->AddChildToCanvas(Background))
	{
		// 앵커를 네 귀퉁이에 물리고 오프셋 0 — 해상도가 뭐든 꽉 찬다.
		BackgroundSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		BackgroundSlot->SetOffsets(FMargin(0.f));
	}

	// ---- 가운데 세로 묶음: 제목 · 회전자 · 진행 막대 ----
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LSK_Column"));

	Text_Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Title"));
	Text_Title->SetColorAndOpacity(FSlateColor(LSKFallback::TitleColor));
	Text_Title->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 28));
	Text_Title->SetJustification(ETextJustify::Center);

	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(Text_Title))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
	}

	Throbber_Loading = WidgetTree->ConstructWidget<UCircularThrobber>(UCircularThrobber::StaticClass(), TEXT("Throbber_Loading"));

	if (UVerticalBoxSlot* ThrobberSlot = Column->AddChildToVerticalBox(Throbber_Loading))
	{
		ThrobberSlot->SetHorizontalAlignment(HAlign_Center);
	}

	Bar_Progress = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Bar_Progress"));
	Bar_Progress->SetPercent(0.f);

	// 심리스 트래블은 진행률을 알 수 없어서 기본은 숨김이다. SetProgress(0~1) 를 부르면 나타난다.
	Bar_Progress->SetVisibility(ESlateVisibility::Collapsed);

	// ProgressBar 자체에는 크기를 지정하는 길이 없다(스타일만 있다). 세로 묶음에 그냥 넣으면
	// 원하는 크기(Desired Size)가 0 이라 선 하나로 찌그러진다 — SizeBox 로 감싸 크기를 못박는다.
	USizeBox* BarSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("LSK_BarSizeBox"));
	BarSizeBox->SetWidthOverride(LSKFallback::BarWidth);
	BarSizeBox->SetHeightOverride(LSKFallback::BarHeight);
	BarSizeBox->AddChild(Bar_Progress);

	if (UVerticalBoxSlot* BarSlot = Column->AddChildToVerticalBox(BarSizeBox))
	{
		BarSlot->SetHorizontalAlignment(HAlign_Center);
		BarSlot->SetPadding(FMargin(0.f, 18.f, 0.f, 0.f));
		BarSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}

	if (UCanvasPanelSlot* ColumnSlot = Root->AddChildToCanvas(Column))
	{
		// 한 점 앵커(0.5, 0.5) + Alignment 0.5 = 해상도와 무관하게 정확히 화면 중앙.
		ColumnSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		ColumnSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		ColumnSlot->SetAutoSize(true);
	}

	// ---- 아래쪽 팁 한 줄 ----
	Text_Tip = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Tip"));
	Text_Tip->SetColorAndOpacity(FSlateColor(LSKFallback::TipColor));
	Text_Tip->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 14));
	Text_Tip->SetJustification(ETextJustify::Center);
	Text_Tip->SetAutoWrapText(true);
	Text_Tip->SetVisibility(ESlateVisibility::Collapsed);

	if (UCanvasPanelSlot* TipSlot = Root->AddChildToCanvas(Text_Tip))
	{
		// 아래 가장자리에 가로로 붙인다. 좌우 여백은 넉넉히 — 긴 팁이 화면 밖으로 나가면 안 된다.
		TipSlot->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
		TipSlot->SetOffsets(FMargin(120.f, -110.f, 120.f, 60.f));
		TipSlot->SetAlignment(FVector2D(0.f, 0.f));
	}
}
