// Fill out your copyright notice in the Description page of Project Settings.

#include "LoadingScreenWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CircularThrobber.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

TSharedRef<SWidget> ULSKLoadingScreenWidget::RebuildWidget()
{
	// WBP 가 지정되지 않았을 때만 C++ 이 임시 레이아웃을 조립
	if (WidgetTree && !WidgetTree->RootWidget && !IsDesignTime())
	{
		BuildFallbackLayout();
	}

	TSharedRef<SWidget> Result = Super::RebuildWidget();

	// 조립·바인딩이 끝난 지금에야 값을 밀어넣을 수 있다.
	ApplyPendingValues();

	return Result;
}

void ULSKLoadingScreenWidget::SetTitleText(const FText& InText)
{
	PendingTitle = InText;

	if (Text_Title)
	{
		Text_Title->SetText(InText);
	}
}

void ULSKLoadingScreenWidget::SetTipText(const FText& InText)
{
	PendingTip = InText;

	if (Text_Tip)
	{
		Text_Tip->SetText(InText);

		// 팁이 비어 있는데 칸만 남아 있으면 레이아웃에 구멍이 생긴다.
		Text_Tip->SetVisibility(InText.IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::HitTestInvisible);
	}
}

void ULSKLoadingScreenWidget::SetProgress(float InPercent)
{
	PendingProgress = InPercent;

	if (Bar_Progress)
	{
		const bool bKnown = InPercent >= 0.f;

		Bar_Progress->SetVisibility(bKnown
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);

		if (bKnown)
		{
			Bar_Progress->SetPercent(FMath::Clamp(InPercent, 0.f, 1.f));
		}
	}

	// 진행률을 알면 회전자는 군더더기다. 둘 다 있으면 하나만 남긴다.
	if (Throbber_Loading)
	{
		Throbber_Loading->SetVisibility(InPercent >= 0.f
			? ESlateVisibility::Collapsed
			: ESlateVisibility::HitTestInvisible);
	}
}

void ULSKLoadingScreenWidget::ApplyPendingValues()
{
	SetTitleText(PendingTitle);
	SetTipText(PendingTip);
	SetProgress(PendingProgress);
}

void ULSKLoadingScreenWidget::NotifyShown()
{
	BP_OnShown();
}

void ULSKLoadingScreenWidget::NotifyHidden()
{
	BP_OnHidden();
}
