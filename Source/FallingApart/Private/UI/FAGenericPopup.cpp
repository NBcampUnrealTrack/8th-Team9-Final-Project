// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FAGenericPopup.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
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
}

void UFAGenericPopup::NativeDestruct()
{
	if (ConfirmButton)
	{
		ConfirmButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleConfirmClicked);
	}

	BroadcastClosedOnce();
	Super::NativeDestruct();
}

void UFAGenericPopup::SetupMessage(const FText& Title, const FText& Message, const FText& ConfirmText)
{
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
}

void UFAGenericPopup::ClosePopup()
{
	RemoveFromParent();
	BroadcastClosedOnce();
}

void UFAGenericPopup::HandleConfirmClicked()
{
	OnConfirmed.Broadcast();
	ClosePopup();
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

	ConfirmButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ConfirmButton"));
	if (UVerticalBoxSlot* ButtonSlot = Column->AddChildToVerticalBox(ConfirmButton))
	{
		ButtonSlot->SetHorizontalAlignment(HAlign_Center);
	}

	ConfirmButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ConfirmButtonText"));
	ConfirmButtonText->SetJustification(ETextJustify::Center);
	ConfirmButtonText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	ConfirmButton->AddChild(ConfirmButtonText);
}
