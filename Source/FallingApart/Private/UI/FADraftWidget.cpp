// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FADraftWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Game/FAGameState.h"
#include "GameFramework/PlayerController.h"
#include "Player/FAPlayerController.h"
#include "Player/FAPlayerState.h"
#include "Rule/FARuleSettings.h"
#include "Styling/CoreStyle.h"

namespace FADraftUI
{
	FSlateFontInfo MakeFont(int32 Size, bool bBold)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}

	FString TierToString(EFARuleTier Tier)
	{
		switch (Tier)
		{
		case EFARuleTier::T1: return TEXT("T1");
		case EFARuleTier::T2: return TEXT("T2");
		case EFARuleTier::T3: return TEXT("T3");
		default: return TEXT("T?");
		}
	}

	FString TeamToString(EFATeam Team)
	{
		switch (Team)
		{
		case EFATeam::Human:  return TEXT("Human");
		case EFATeam::Zombie: return TEXT("Zombie");
		default: return TEXT("-");
		}
	}

	/** 티어가 한눈에 구분되도록 카드 배경색을 바꾼다 — 임시 레이아웃 전용. */
	FLinearColor TierToColor(EFARuleTier Tier)
	{
		switch (Tier)
		{
		case EFARuleTier::T1: return FLinearColor(0.20f, 0.45f, 0.25f, 1.0f);
		case EFARuleTier::T2: return FLinearColor(0.50f, 0.40f, 0.12f, 1.0f);
		case EFARuleTier::T3: return FLinearColor(0.50f, 0.16f, 0.16f, 1.0f);
		default: return FLinearColor(0.25f, 0.25f, 0.25f, 1.0f);
		}
	}
}

void UFADraftWidget::ShowCandidates(const TArray<FName>& CandidateRuleIds, float SelectionTime)
{
	Cards.Reset();
	for (const FName& RuleId : CandidateRuleIds)
	{
		if (Cards.Num() >= MaxCards)
		{
			// 서버가 3장을 넘겨보내면 Server_ConfirmDraft_Validate 가 어차피 막는다 — 여기서 잘라둔다.
			UE_LOG(LogTemp, Warning, TEXT("[Draft] 후보가 %d장 왔다 — 앞 %d장만 표시한다"), CandidateRuleIds.Num(), MaxCards);
			break;
		}

		Cards.Add(MakeCardInfo(RuleId));
	}

	SelectionDuration = FMath::Max(0.0f, SelectionTime);
	SelectionDeadline = GetWorld() ? GetWorld()->GetTimeSeconds() + SelectionDuration : -1.0f;
	LastDisplayedSeconds = -1;
	bIsOpen = true;

	RefreshFallbackCards();
	ApplyInputMode(true);

	OnDraftShown(Cards, SelectionDuration);
}

void UFADraftWidget::ConfirmSelection(int32 Index)
{
	if (!bIsOpen)
	{
		return;
	}

	if (!Cards.IsValidIndex(Index))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Draft] ConfirmSelection — 잘못된 인덱스 %d (후보 %d장)"), Index, Cards.Num());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Draft] 카드 선택: [%d] %s"), Index, *Cards[Index].RuleId.ToString());

	if (AFAPlayerState* FAPlayerState = GetOwningPlayerState<AFAPlayerState>())
	{
		// 실제 룰 적용은 서버가 한다. 클라이언트는 인덱스만 보낸다.
		FAPlayerState->Server_ConfirmDraft(Index);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Draft] PlayerState 가 없어 선택을 서버로 보내지 못했다"));
	}

	Close(true, Index);
}

void UFADraftWidget::CloseWithoutSelection()
{
	if (bIsOpen)
	{
		// 제한 시간이 지났다면 서버가 이미 무작위로 확정한 상태다 (UFARuleDraftSubsystem::HandleDraftTimeout).
		UE_LOG(LogTemp, Log, TEXT("[Draft] 선택 없이 닫는다 (남은 시간 %.1f초)"), GetTimeRemaining());
		Close(false, INDEX_NONE);
	}
}

void UFADraftWidget::Close(bool bConfirmed, int32 SelectedIndex)
{
	bIsOpen = false;
	SelectionDeadline = -1.0f;

	ApplyInputMode(false);
	OnDraftClosed(bConfirmed, SelectedIndex);

	RemoveFromParent();
}

float UFADraftWidget::GetTimeRemaining() const
{
	if (!bIsOpen || SelectionDeadline < 0.0f || !GetWorld())
	{
		return 0.0f;
	}

	return FMath::Max(0.0f, SelectionDeadline - GetWorld()->GetTimeSeconds());
}

FText UFADraftWidget::GetTimeRemainingText() const
{
	return FText::FromString(FString::Printf(TEXT("%d"), FMath::CeilToInt(GetTimeRemaining())));
}

int32 UFADraftWidget::GetDraftIndex() const
{
	const AFAGameState* FAGameState = GetWorld() ? GetWorld()->GetGameState<AFAGameState>() : nullptr;
	return FAGameState ? FAGameState->GetDraftsCompleted() : 0;
}

FFADraftCardInfo UFADraftWidget::MakeCardInfo(FName RuleId)
{
	FFADraftCardInfo Info;
	Info.RuleId = RuleId;

	if (const FFARuleCardRow* Row = UFARuleSettings::FindRuleCard(RuleId))
	{
		Info.DisplayName = Row->DisplayName;
		Info.ShortDesc = Row->ShortDesc;
		Info.Team = Row->Team;
		Info.Tier = Row->Tier;
		Info.Type = Row->Type;
		Info.Icon = Row->Icon;
		Info.bFoundInTable = true;

		if (Info.DisplayName.IsEmpty())
		{
			// 표에 행은 있는데 이름 칸이 비었을 때 — 빈 카드가 뜨는 대신 ID 라도 보이게.
			Info.DisplayName = FText::FromName(RuleId);
		}

		return Info;
	}

	// DT 미지정·더미 룰 폴백. 드래프트 루프 자체는 계속 테스트할 수 있어야 한다.
	Info.DisplayName = FText::FromName(RuleId);
	Info.ShortDesc = FText::FromString(TEXT("DT 에 없는 카드 (더미 풀)"));
	return Info;
}

void UFADraftWidget::RestoreInputMode()
{
	ApplyInputMode(bIsOpen);
}

void UFADraftWidget::ApplyInputMode(bool bOpen)
{
	if (!bTakeMouseCursorWhileOpen)
	{
		return;
	}

	APlayerController* PlayerController = GetOwningPlayer();
	if (!PlayerController)
	{
		return;
	}

	// ESC 메뉴가 떠 있으면 입력은 그쪽으로.
	const AFAPlayerController* FAPlayerController = Cast<AFAPlayerController>(PlayerController);
	if (!bOpen && FAPlayerController && FAPlayerController->IsPauseMenuOpen())
	{
		return;
	}

	if (bOpen)
	{
		PlayerController->SetShowMouseCursor(true);

		// 드래프트 중에도 이동·시야는 살아 있어야 한다 (라운드는 계속 돌아간다).
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PlayerController->SetInputMode(InputMode);
	}
	else
	{
		PlayerController->SetShowMouseCursor(false);
		PlayerController->SetInputMode(FInputModeGameOnly());
	}
}

void UFADraftWidget::NativeDestruct()
{
	if (bIsOpen)
	{
		// 라운드 종료 등으로 위젯이 통째로 걷힐 때 커서가 남지 않게.
		bIsOpen = false;
		ApplyInputMode(false);
	}

	Super::NativeDestruct();
}

void UFADraftWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bIsOpen)
	{
		return;
	}

	const float Remaining = GetTimeRemaining();
	const int32 WholeSeconds = FMath::CeilToInt(Remaining);

	if (WholeSeconds != LastDisplayedSeconds)
	{
		LastDisplayedSeconds = WholeSeconds;
		RefreshFallbackTimer(WholeSeconds);
		OnSelectionTimeChanged(Remaining, WholeSeconds);
	}

	if (Remaining <= 0.0f)
	{
		CloseWithoutSelection();
	}
}

//~ ---------------- WBP 가 없을 때 쓰는 임시 레이아웃 ----------------

TSharedRef<SWidget> UFADraftWidget::RebuildWidget()
{
	// WBP 를 상속해 만들면 RootWidget 이 이미 차 있다 — 그때는 디자이너가 만든 트리를 그대로 쓴다.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildFallbackLayout();
		RefreshFallbackCards();
	}

	return Super::RebuildWidget();
}

void UFADraftWidget::BuildFallbackLayout()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = RootCanvas;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Panel->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.85f));
	Panel->SetPadding(FMargin(20.0f, 14.0f));

	UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 1.0f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	PanelSlot->SetAutoSize(true);
	PanelSlot->SetPosition(FVector2D(0.0f, -90.0f));

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(Column);

	FallbackTitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FallbackTitleText->SetFont(FADraftUI::MakeFont(20, true));
	FallbackTitleText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(FallbackTitleText))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
	}

	FallbackTimerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FallbackTimerText->SetFont(FADraftUI::MakeFont(16, false));
	FallbackTimerText->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.75f, 0.8f, 1.0f)));
	if (UVerticalBoxSlot* TimerSlot = Column->AddChildToVerticalBox(FallbackTimerText))
	{
		TimerSlot->SetHorizontalAlignment(HAlign_Center);
		TimerSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}

	UHorizontalBox* CardRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Column->AddChildToVerticalBox(CardRow);

	FallbackCardSlots.Reset();
	FallbackCardButtons.Reset();
	FallbackNameTexts.Reset();
	FallbackDescTexts.Reset();
	FallbackMetaTexts.Reset();

	for (int32 Index = 0; Index < MaxCards; ++Index)
	{
		USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		CardSize->SetWidthOverride(230.0f);
		CardSize->SetHeightOverride(150.0f);

		if (UHorizontalBoxSlot* CardSlot = CardRow->AddChildToHorizontalBox(CardSize))
		{
			CardSlot->SetPadding(FMargin(6.0f, 0.0f));
		}

		UButton* CardButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		CardSize->SetContent(CardButton);

		UVerticalBox* CardColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		CardButton->SetContent(CardColumn);

		UTextBlock* NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		NameText->SetFont(FADraftUI::MakeFont(18, true));
		NameText->SetJustification(ETextJustify::Center);
		NameText->SetAutoWrapText(true);
		if (UVerticalBoxSlot* NameSlot = CardColumn->AddChildToVerticalBox(NameText))
		{
			NameSlot->SetHorizontalAlignment(HAlign_Fill);
			NameSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		}

		UTextBlock* DescText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		DescText->SetFont(FADraftUI::MakeFont(13, false));
		DescText->SetJustification(ETextJustify::Center);
		DescText->SetAutoWrapText(true);
		DescText->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.9f, 0.9f, 1.0f)));
		if (UVerticalBoxSlot* DescSlot = CardColumn->AddChildToVerticalBox(DescText))
		{
			DescSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		// 한글 폰트가 깨지더라도 무엇을 뽑았는지는 알 수 있게, ID·티어·진영은 ASCII 로 따로 찍는다.
		UTextBlock* MetaText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		MetaText->SetFont(FADraftUI::MakeFont(11, false));
		MetaText->SetJustification(ETextJustify::Center);
		MetaText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.75f, 1.0f)));
		if (UVerticalBoxSlot* MetaSlot = CardColumn->AddChildToVerticalBox(MetaText))
		{
			MetaSlot->SetHorizontalAlignment(HAlign_Fill);
			MetaSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
		}

		FallbackCardSlots.Add(CardSize);
		FallbackCardButtons.Add(CardButton);
		FallbackNameTexts.Add(NameText);
		FallbackDescTexts.Add(DescText);
		FallbackMetaTexts.Add(MetaText);
	}

	// UButton::OnClicked 는 파라미터가 없어 인덱스를 실어 보낼 수 없다 — 슬롯 수만큼 핸들러를 둔다.
	FallbackCardButtons[0]->OnClicked.AddDynamic(this, &UFADraftWidget::HandleCard0Clicked);
	FallbackCardButtons[1]->OnClicked.AddDynamic(this, &UFADraftWidget::HandleCard1Clicked);
	FallbackCardButtons[2]->OnClicked.AddDynamic(this, &UFADraftWidget::HandleCard2Clicked);
}

void UFADraftWidget::RefreshFallbackCards()
{
	if (FallbackCardSlots.Num() != MaxCards)
	{
		// WBP 를 쓰는 중 — 그리기는 전부 블루프린트 몫이다.
		return;
	}

	if (FallbackTitleText)
	{
		FallbackTitleText->SetText(FText::FromString(
			FString::Printf(TEXT("DRAFT %d  ·  룰 카드를 고르세요"), GetDraftIndex())));
	}

	RefreshFallbackTimer(FMath::CeilToInt(GetTimeRemaining()));

	for (int32 Index = 0; Index < MaxCards; ++Index)
	{
		if (!Cards.IsValidIndex(Index))
		{
			// 후보가 3장이 안 될 수 있다 (해당 티어 카드가 부족한 경우).
			FallbackCardSlots[Index]->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}

		const FFADraftCardInfo& Card = Cards[Index];

		FallbackCardSlots[Index]->SetVisibility(ESlateVisibility::Visible);
		FallbackCardButtons[Index]->SetBackgroundColor(FADraftUI::TierToColor(Card.Tier));
		FallbackNameTexts[Index]->SetText(Card.DisplayName);
		FallbackDescTexts[Index]->SetText(Card.ShortDesc);
		FallbackMetaTexts[Index]->SetText(FText::FromString(FString::Printf(TEXT("[%d]  %s · %s · %s"),
			Index + 1,
			*Card.RuleId.ToString(),
			*FADraftUI::TierToString(Card.Tier),
			*FADraftUI::TeamToString(Card.Team))));
	}
}

void UFADraftWidget::RefreshFallbackTimer(int32 WholeSeconds)
{
	if (FallbackTimerText)
	{
		FallbackTimerText->SetText(FText::FromString(FString::Printf(TEXT("%d s"), FMath::Max(0, WholeSeconds))));
	}
}

void UFADraftWidget::HandleCard0Clicked()
{
	ConfirmSelection(0);
}

void UFADraftWidget::HandleCard1Clicked()
{
	ConfirmSelection(1);
}

void UFADraftWidget::HandleCard2Clicked()
{
	ConfirmSelection(2);
}
