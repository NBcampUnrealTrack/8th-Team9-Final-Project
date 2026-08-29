// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/FAVoiceCalibrationWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

#define LOCTEXT_NAMESPACE "FAVoiceCalibrationWidget"

namespace FAVoiceCalibrationUI
{
	static const FLinearColor SectionColor(1.f, 0.78f, 0.25f, 1.f);
	static const FLinearColor LabelColor(0.85f, 0.85f, 0.88f, 1.f);
	static const FLinearColor HintColor(0.6f, 0.6f, 0.65f, 1.f);

	/** 미터가 담당할 범위. -60dB(거의 무음) ~ 0dB(최대). */
	static constexpr float MeterFloorDb = -60.f;

	static UTextBlock* MakeText(UWidgetTree* WidgetTree, const FText& Text, int32 FontSize, const FLinearColor& Color)
	{
		UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Block->SetText(Text);
		Block->SetColorAndOpacity(FSlateColor(Color));
		Block->SetAutoWrapText(true);

		FSlateFontInfo Font = Block->GetFont();
		Font.Size = FontSize;
		Block->SetFont(Font);

		return Block;
	}
}

float UFAVoiceCalibrationWidget::ToMeterAlpha(float Level)
{
	if (Level <= SMALL_NUMBER)
	{
		return 0.f;
	}

	const float Db = 20.f * FMath::LogX(10.f, Level);
	return FMath::Clamp((Db - FAVoiceCalibrationUI::MeterFloorDb) / -FAVoiceCalibrationUI::MeterFloorDb, 0.f, 1.f);
}

TSharedRef<SWidget> UFAVoiceCalibrationWidget::RebuildWidget()
{
	// WBP 를 지정하지 않았을 때만 임시 레이아웃을 만든다.
	if (WidgetTree && !WidgetTree->RootWidget && !IsDesignTime())
	{
		BuildFallbackLayout();
	}

	return Super::RebuildWidget();
}

void UFAVoiceCalibrationWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Calibrate)
	{
		Btn_Calibrate->OnClicked.RemoveAll(this);
		Btn_Calibrate->OnClicked.AddUniqueDynamic(this, &UFAVoiceCalibrationWidget::HandleCalibrateClicked);
	}

	if (UFAVoiceCalibration* Calibration = UFAVoiceCalibration::Get(this))
	{
		Calibration->OnCalibrationProgress.RemoveAll(this);
		Calibration->OnCalibrationProgress.AddUniqueDynamic(this, &UFAVoiceCalibrationWidget::HandleProgress);

		Calibration->OnCalibrationFinished.RemoveAll(this);
		Calibration->OnCalibrationFinished.AddUniqueDynamic(this, &UFAVoiceCalibrationWidget::HandleFinished);

		RefreshForState(Calibration->IsCalibrating());

		// 창을 다시 열었을 때 지난 결과가 그대로 보이게.
		const FFAVoiceCalibrationReport& Last = Calibration->GetLastReport();
		if (Text_Status && !Last.Message.IsEmpty())
		{
			Text_Status->SetText(Last.Message);
		}
	}
	else
	{
		RefreshForState(false);
	}
}

void UFAVoiceCalibrationWidget::NativeDestruct()
{
	if (UFAVoiceCalibration* Calibration = UFAVoiceCalibration::Get(this))
	{
		Calibration->OnCalibrationProgress.RemoveAll(this);
		Calibration->OnCalibrationFinished.RemoveAll(this);

		// 창을 닫으면 측정도 접는다. 안 그러면 마이크 스트림이 열린 채 남는다.
		Calibration->CancelCalibration();
	}

	Super::NativeDestruct();
}

void UFAVoiceCalibrationWidget::HandleCalibrateClicked()
{
	UFAVoiceCalibration* Calibration = UFAVoiceCalibration::Get(this);
	if (Calibration == nullptr || Calibration->IsCalibrating())
	{
		return;
	}

	if (Calibration->StartCalibration(CalibrationSeconds))
	{
		RefreshForState(true);
	}
}

void UFAVoiceCalibrationWidget::HandleProgress(float Alpha, float CurrentLevel)
{
	if (Bar_Level)
	{
		Bar_Level->SetPercent(ToMeterAlpha(CurrentLevel));
	}

	if (Text_Status)
	{
		const int32 Remaining = FMath::CeilToInt32(CalibrationSeconds * (1.f - Alpha));
		Text_Status->SetText(FText::Format(
			LOCTEXT("Measuring", "측정 중입니다. 말하지 말고 기다려 주세요 — {0}초"), FText::AsNumber(Remaining)));
	}
}

void UFAVoiceCalibrationWidget::HandleFinished(const FFAVoiceCalibrationReport& Report)
{
	RefreshForState(false);

	if (Bar_Level)
	{
		Bar_Level->SetPercent(0.f);
	}

	if (Text_Status)
	{
		Text_Status->SetText(Report.Message);
	}
}

void UFAVoiceCalibrationWidget::RefreshForState(bool bRunning)
{
	if (Btn_Calibrate)
	{
		Btn_Calibrate->SetIsEnabled(!bRunning);
	}

	if (Text_ButtonLabel)
	{
		Text_ButtonLabel->SetText(bRunning
			? LOCTEXT("Measuring_Button", "측정 중…")
			: LOCTEXT("Calibrate", "마이크 자동 조정"));
	}

	if (!bRunning && Text_Status && Text_Status->GetText().IsEmpty())
	{
		Text_Status->SetText(LOCTEXT("Hint",
			"조용한 상태에서 눌러 주세요. 몇 초간 주변 소음을 재서 마이크 감도를 자동으로 맞춥니다."));
	}
}

void UFAVoiceCalibrationWidget::BuildFallbackLayout()
{
	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	WidgetTree->RootWidget = Root;

	if (UVerticalBoxSlot* TitleSlot = Root->AddChildToVerticalBox(
		FAVoiceCalibrationUI::MakeText(WidgetTree, LOCTEXT("Title", "마이크 자동 조정"), 18, FAVoiceCalibrationUI::SectionColor)))
	{
		TitleSlot->SetPadding(FMargin(0.f, 16.f, 0.f, 6.f));
	}

	Text_Status = FAVoiceCalibrationUI::MakeText(WidgetTree, FText::GetEmpty(), 11, FAVoiceCalibrationUI::HintColor);
	Root->AddChildToVerticalBox(Text_Status);

	Bar_Level = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	Bar_Level->SetPercent(0.f);
	if (UVerticalBoxSlot* MeterSlot = Root->AddChildToVerticalBox(Bar_Level))
	{
		MeterSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		MeterSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}

	Btn_Calibrate = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Text_ButtonLabel = FAVoiceCalibrationUI::MakeText(WidgetTree, FText::GetEmpty(), 12, FAVoiceCalibrationUI::LabelColor);
	Btn_Calibrate->AddChild(Text_ButtonLabel);

	if (UVerticalBoxSlot* ButtonSlot = Root->AddChildToVerticalBox(Btn_Calibrate))
	{
		ButtonSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		ButtonSlot->SetHorizontalAlignment(HAlign_Left);
	}
}

#undef LOCTEXT_NAMESPACE
