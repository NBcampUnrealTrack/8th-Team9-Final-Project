// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FAPauseMenuWidget.h"

#include "Audio/FAAudioSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Player/FAPlayerController.h"
#include "SettingsKitMenuWidget.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "FAPauseMenu"

namespace FAPauseMenuUI
{
	FSlateFontInfo MakeFont(int32 Size, bool bBold)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}

	static const FName SoundOpen(TEXT("UI_PauseOpen"));
	static const FName SoundClose(TEXT("UI_PauseClose"));
	static const FName PopupOpen(TEXT("UI_PopupOpen"));
	static const FName PopupClose(TEXT("UI_PopupClose"));
	static const FName SoundClick(TEXT("UI_Click"));
	static const FName SoundHover(TEXT("UI_Hover"));
}

void UFAPauseMenuWidget::OnButtonHovered()
{
	// 쿨다운 0.04 초가 표에 걸려 있어 목록을 빠르게 훑어도 연사되지 않는다.
	UFAAudioSubsystem::PlayFASound(this, FAPauseMenuUI::SoundHover);
}

void UFAPauseMenuWidget::CloseMenu()
{
	// 입력 모드 · 커서 복구는 컨트롤러가 한 곳에서 책임진다.
	if (AFAPlayerController* FAPlayerController = Cast<AFAPlayerController>(GetOwningPlayer()))
	{
		FAPlayerController->HidePauseMenu();
		return;
	}

	// 컨트롤러가 AFAPlayerController 가 아닌 경우의 폴백 — 최소한 화면에서는 걷는다.
	RemoveFromParent();
}

void UFAPauseMenuWidget::OnMainMenuClicked()
{
	UFAAudioSubsystem::PlayFASound(this, FAPauseMenuUI::SoundClick);

	if (AFAPlayerController* FAPlayerController = Cast<AFAPlayerController>(GetOwningPlayer()))
	{
		FAPlayerController->ReturnToMainMenu();
	}
}

void UFAPauseMenuWidget::OnSettingsClicked()
{
	UFAAudioSubsystem::PlayFASound(this, FAPauseMenuUI::SoundClick);

	// 이미 떠 있으면 중복 생성하지 않는다.
	if (SettingsWidget && SettingsWidget->IsInViewport())
	{
		return;
	}

	// WBP 를 안 지정했으면 플러그인의 C++ 설정창을 그대로 쓴다. 임시 레이아웃이지만 네 탭이 다 돈다.
	// WBP_FASettings 가 만들어지면 WBP_FAPauseMenu 의 SettingsWidgetClass 에 지정하면 된다.
	const TSubclassOf<UUserWidget> WidgetClass =
		SettingsWidgetClass ? SettingsWidgetClass : TSubclassOf<UUserWidget>(USKSettingsMenuWidget::StaticClass());

	SettingsWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), WidgetClass);
	if (SettingsWidget)
	{
		// 설정창은 자기가 닫히는 시점을 알려준다 — 자기 닫기 버튼 · ESC · 바깥 클릭이 다 이 한 줄로 모인다.
		if (USKSettingsMenuWidget* SettingsMenu = Cast<USKSettingsMenuWidget>(SettingsWidget))
		{
			SettingsMenu->OnSettingsClosed.AddUniqueDynamic(this, &ThisClass::HandleSettingsClosed);
		}

		SettingsWidget->AddToViewport(SettingsWidgetZOrder);
		UFAAudioSubsystem::PlayFASound(this, FAPauseMenuUI::PopupOpen);
	}
}

void UFAPauseMenuWidget::HandleSettingsClosed()
{
	UFAAudioSubsystem::PlayFASound(this, FAPauseMenuUI::PopupClose);

	if (AFAPlayerController* FAPlayerController = Cast<AFAPlayerController>(GetOwningPlayer()))
	{
		FAPlayerController->FocusPauseMenu();
	}
}

void UFAPauseMenuWidget::OnQuitClicked()
{
	UFAAudioSubsystem::PlayFASound(this, FAPauseMenuUI::SoundClick);

	if (AFAPlayerController* FAPlayerController = Cast<AFAPlayerController>(GetOwningPlayer()))
	{
		FAPlayerController->QuitGame();
	}
}

void UFAPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);

	SetVisibility(ESlateVisibility::Visible);

	BindButtons();

	UFAAudioSubsystem::PlayFASound(this, FAPauseMenuUI::SoundOpen);

	OnPauseMenuOpened();
}

void UFAPauseMenuWidget::NativeDestruct()
{
	// 일시정지 메뉴가 걷힐 때 그 위에 떠 있던 설정 창도 같이 지운다.
	if (SettingsWidget && SettingsWidget->IsInViewport())
	{
		SettingsWidget->RemoveFromParent();
	}
	SettingsWidget = nullptr;

	UFAAudioSubsystem::PlayFASound(this, FAPauseMenuUI::SoundClose);

	OnPauseMenuClosed();

	Super::NativeDestruct();
}

FReply UFAPauseMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const AFAPlayerController* FAPlayerController = Cast<AFAPlayerController>(GetOwningPlayer());
	const bool bIsCloseKey = FAPlayerController
		? FAPlayerController->IsPauseMenuKey(InKeyEvent.GetKey())
		: InKeyEvent.GetKey() == EKeys::Escape;

	if (bIsCloseKey)
	{
		CloseMenu();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UFAPauseMenuWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return FReply::Handled().SetUserFocus(TakeWidget(), EFocusCause::Mouse);
}

void UFAPauseMenuWidget::BindButtons()
{
	if (Btn_MainMenu)
	{
		Btn_MainMenu->OnClicked.AddUniqueDynamic(this, &UFAPauseMenuWidget::OnMainMenuClicked);
		Btn_MainMenu->OnHovered.AddUniqueDynamic(this, &UFAPauseMenuWidget::OnButtonHovered);
	}
	if (Btn_Settings)
	{
		Btn_Settings->OnClicked.AddUniqueDynamic(this, &UFAPauseMenuWidget::OnSettingsClicked);
		Btn_Settings->OnHovered.AddUniqueDynamic(this, &UFAPauseMenuWidget::OnButtonHovered);
	}
	if (Btn_Quit)
	{
		Btn_Quit->OnClicked.AddUniqueDynamic(this, &UFAPauseMenuWidget::OnQuitClicked);
		Btn_Quit->OnHovered.AddUniqueDynamic(this, &UFAPauseMenuWidget::OnButtonHovered);
	}
}

//~ ---------------- WBP 가 없을 때 쓰는 임시 레이아웃 ----------------

TSharedRef<SWidget> UFAPauseMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget && !IsDesignTime())
	{
		BuildFallbackLayout();
	}

	return Super::RebuildWidget();
}

void UFAPauseMenuWidget::BuildFallbackLayout()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = RootCanvas;

	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Dim->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
	if (UCanvasPanelSlot* DimSlot = RootCanvas->AddChildToCanvas(Dim))
	{
		DimSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		DimSlot->SetOffsets(FMargin(0.0f));
	}

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Panel->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.9f));
	Panel->SetPadding(FMargin(28.0f, 22.0f));

	UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PanelSlot->SetAutoSize(true);
	PanelSlot->SetPosition(FVector2D::ZeroVector);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(Column);

	UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TitleText->SetText(LOCTEXT("PauseMenuTitle", "일시정지"));
	TitleText->SetFont(FAPauseMenuUI::MakeFont(22, true));
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
	}

	Btn_MainMenu = MakeFallbackButton(Column, LOCTEXT("PauseMenuMainMenu", "메인 메뉴"));
	Btn_Settings = MakeFallbackButton(Column, LOCTEXT("PauseMenuSettings", "설정"));
	Btn_Quit = MakeFallbackButton(Column, LOCTEXT("PauseMenuQuit", "게임 종료"));

	UTextBlock* HintText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	HintText->SetText(LOCTEXT("PauseMenuHint", "ESC — 닫기"));
	HintText->SetFont(FAPauseMenuUI::MakeFont(12, false));
	HintText->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.65f, 1.0f)));
	if (UVerticalBoxSlot* HintSlot = Column->AddChildToVerticalBox(HintText))
	{
		HintSlot->SetHorizontalAlignment(HAlign_Center);
		HintSlot->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 0.0f));
	}
}

UButton* UFAPauseMenuWidget::MakeFallbackButton(UVerticalBox* Column, const FText& Label)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());

	UTextBlock* ButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ButtonText->SetText(Label);
	ButtonText->SetFont(FAPauseMenuUI::MakeFont(16, false));
	ButtonText->SetJustification(ETextJustify::Center);
	Button->SetContent(ButtonText);

	if (UVerticalBoxSlot* ButtonSlot = Column->AddChildToVerticalBox(Button))
	{
		ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
		ButtonSlot->SetPadding(FMargin(0.0f, 4.0f));
	}

	return Button;
}

#undef LOCTEXT_NAMESPACE
