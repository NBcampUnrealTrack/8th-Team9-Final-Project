// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FAGenericPopup.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UFAGenericPopup::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!TitleText || !MessageText || !ConfirmButton || !ConfirmButtonText)
	{
		BuildFallbackLayout();
	}

	if (ConfirmButton)
	{
		ConfirmButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleConfirmClicked);
	}
	if (NoButton)
	{
		NoButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleNoClicked);
		NoButton->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UFAGenericPopup::NativeDestruct()
{
	FOnFAConfirmationResult CancelledCallback;
	if (bConfirmationDialog)
	{
		CancelledCallback = ConfirmationResultCallback;
		ConfirmationResultCallback.Unbind();
		bConfirmationDialog = false;
	}

	if (ConfirmButton)
	{
		ConfirmButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleConfirmClicked);
	}
	if (NoButton)
	{
		NoButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleNoClicked);
	}

	BroadcastClosedOnce();
	Super::NativeDestruct();
	CancelledCallback.ExecuteIfBound(false);
}

void UFAGenericPopup::SetupMessage(const FText& Title, const FText& Message, const FText& ConfirmText)
{
	bConfirmationDialog = false;
	ConfirmationResultCallback.Unbind();

	if (TitleText)
	{
		TitleText->SetText(Title);
	}
	if (MessageText)
	{
		MessageText->SetText(Message);
	}
	if (ConfirmButtonText)
	{
		ConfirmButtonText->SetText(ConfirmText.IsEmpty()
			? NSLOCTEXT("FAGenericPopup", "Confirm", "확인")
			: ConfirmText);
	}
	if (ConfirmButton)
	{
		ConfirmButton->SetVisibility(ESlateVisibility::Visible);
	}
	if (NoButton)
	{
		NoButton->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UFAGenericPopup::SetupConfirmation(const FText& Title, const FText& Message,
	const FText& YesText, const FText& NoText, const FOnFAConfirmationResult& ResultCallback)
{
	// 기존 WBP에 아니오 버튼이 아직 없다면, 뷰포트에 추가되기 전에 완전한 네이티브 레이아웃으로 대체한다.
	if (!SupportsConfirmationLayout())
	{
		if (ConfirmButton)
		{
			ConfirmButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleConfirmClicked);
		}

		BuildFallbackLayout();
		if (ConfirmButton)
		{
			ConfirmButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleConfirmClicked);
		}
		if (NoButton)
		{
			NoButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleNoClicked);
		}
	}

	bConfirmationDialog = true;
	ConfirmationResultCallback = ResultCallback;

	if (TitleText)
	{
		TitleText->SetText(Title);
	}
	if (MessageText)
	{
		MessageText->SetText(Message);
	}
	if (ConfirmButtonText)
	{
		ConfirmButtonText->SetText(YesText.IsEmpty()
			? NSLOCTEXT("FAGenericPopup", "Yes", "예")
			: YesText);
	}
	if (NoButtonText)
	{
		NoButtonText->SetText(NoText.IsEmpty()
			? NSLOCTEXT("FAGenericPopup", "No", "아니오")
			: NoText);
	}
	if (ConfirmButton)
	{
		ConfirmButton->SetVisibility(ESlateVisibility::Visible);
	}
	if (NoButton)
	{
		NoButton->SetVisibility(ESlateVisibility::Visible);
	}
}

void UFAGenericPopup::ClosePopup()
{
	RemoveFromParent();
	BroadcastClosedOnce();
}

void UFAGenericPopup::HandleConfirmClicked()
{
	if (bConfirmationDialog)
	{
		ResolveConfirmation(true);
		return;
	}

	OnConfirmed.Broadcast();
	ClosePopup();
}

void UFAGenericPopup::HandleNoClicked()
{
	ResolveConfirmation(false);
}

void UFAGenericPopup::ResolveConfirmation(bool bConfirmed)
{
	if (!bConfirmationDialog)
	{
		return;
	}

	// 먼저 창과 관리 참조를 정리해야 콜백 안에서 다음 팝업을 안전하게 열 수 있다.
	FOnFAConfirmationResult ResultCallback = ConfirmationResultCallback;
	ConfirmationResultCallback.Unbind();
	bConfirmationDialog = false;
	ClosePopup();
	ResultCallback.ExecuteIfBound(bConfirmed);
}

void UFAGenericPopup::BroadcastClosedOnce()
{
	if (bClosedBroadcast)
	{
		return;
	}

	bClosedBroadcast = true;
	OnClosed.Broadcast(this);
	OnClosed.Clear();
}

void UFAGenericPopup::BuildFallbackLayout()
{
	if (!WidgetTree)
	{
		return;
	}

	UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("PopupRoot"));
	WidgetTree->RootWidget = RootOverlay;

	UBorder* ScreenDim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ScreenDim"));
	ScreenDim->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.65f));
	if (UOverlaySlot* DimSlot = RootOverlay->AddChildToOverlay(ScreenDim))
	{
		DimSlot->SetHorizontalAlignment(HAlign_Fill);
		DimSlot->SetVerticalAlignment(VAlign_Fill);
	}

	USizeBox* DialogSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DialogSize"));
	DialogSize->SetWidthOverride(560.0f);
	DialogSize->SetMinDesiredHeight(260.0f);
	if (UOverlaySlot* DialogSlot = RootOverlay->AddChildToOverlay(DialogSize))
	{
		DialogSlot->SetHorizontalAlignment(HAlign_Center);
		DialogSlot->SetVerticalAlignment(VAlign_Center);
	}

	UBorder* DialogPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DialogPanel"));
	DialogPanel->SetBrushColor(FLinearColor(0.035f, 0.045f, 0.065f, 0.98f));
	DialogPanel->SetPadding(FMargin(36.0f, 28.0f));
	DialogSize->AddChild(DialogPanel);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DialogColumn"));
	DialogPanel->AddChild(Column);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetJustification(ETextJustify::Center);
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	TitleText->SetAutoWrapText(true);
	Column->AddChildToVerticalBox(TitleText);

	USpacer* TitleGap = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("TitleGap"));
	TitleGap->SetSize(FVector2D(1.0f, 24.0f));
	Column->AddChildToVerticalBox(TitleGap);

	MessageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MessageText"));
	MessageText->SetJustification(ETextJustify::Center);
	MessageText->SetColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.85f, 0.9f, 1.0f)));
	MessageText->SetAutoWrapText(true);
	Column->AddChildToVerticalBox(MessageText);

	USpacer* ButtonGap = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("ButtonGap"));
	ButtonGap->SetSize(FVector2D(1.0f, 32.0f));
	Column->AddChildToVerticalBox(ButtonGap);

	UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ButtonRow"));
	if (UVerticalBoxSlot* ButtonRowSlot = Column->AddChildToVerticalBox(ButtonRow))
	{
		ButtonRowSlot->SetHorizontalAlignment(HAlign_Center);
	}

	ConfirmButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ConfirmButton"));
	ButtonRow->AddChildToHorizontalBox(ConfirmButton);

	ConfirmButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ConfirmButtonText"));
	ConfirmButtonText->SetJustification(ETextJustify::Center);
	ConfirmButtonText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	ConfirmButton->AddChild(ConfirmButtonText);

	NoButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("NoButton"));
	if (UHorizontalBoxSlot* NoButtonSlot = ButtonRow->AddChildToHorizontalBox(NoButton))
	{
		NoButtonSlot->SetPadding(FMargin(16.0f, 0.0f, 0.0f, 0.0f));
	}

	NoButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NoButtonText"));
	NoButtonText->SetJustification(ETextJustify::Center);
	NoButtonText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	NoButton->AddChild(NoButtonText);
}
