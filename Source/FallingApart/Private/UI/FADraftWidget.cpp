// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FADraftWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
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

	RefreshCards();
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

void UFADraftWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// WBP 로 만든 버튼은 여기서 처음 유효해진다 — 임시 레이아웃과 같은 핸들러로 묶는다.
	GatherCardWidgets();
	BindCardButtons();
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
		RefreshTimerText(WholeSeconds);
		OnSelectionTimeChanged(Remaining, WholeSeconds);
	}

	if (Remaining <= 0.0f)
	{
		CloseWithoutSelection();
	}
}

//~ ---------------- 카드 위젯 채우기 (WBP · 임시 레이아웃 공통) ----------------

TSharedRef<SWidget> UFADraftWidget::RebuildWidget()
{
	// UMG 디자이너 프리뷰에서는 아무것도 조립하지 않는다.
	// 폴백을 만들면 WBP 의 WidgetTree 에 없는 위젯이 프리뷰에 섞여 Hierarchy 에서 선택·삭제가 안 되고,
	// 후보가 0장인 상태로 RefreshCards() 를 돌리면 디자이너가 배치한 카드가 전부 숨겨진다.
	if (IsDesignTime())
	{
		return Super::RebuildWidget();
	}

	// WBP 를 상속해 만들면 RootWidget 이 이미 차 있다 — 그때는 디자이너가 만든 트리를 그대로 쓴다.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildFallbackLayout();
	}

	GatherCardWidgets();
	RefreshCards();

	return Super::RebuildWidget();
}

void UFADraftWidget::GatherCardWidgets()
{
	// 개별 변수로 두어야 WBP 가 이름으로 물릴 수 있고, 채울 때는 인덱스가 편하다.
	CardRoots = { Card0, Card1, Card2 };
	CardButtons = { Btn_Card0, Btn_Card1, Btn_Card2 };
	CardNameTexts = { Txt_Card0Name, Txt_Card1Name, Txt_Card2Name };
	CardDescTexts = { Txt_Card0Desc, Txt_Card1Desc, Txt_Card2Desc };
	CardMetaTexts = { Txt_Card0Meta, Txt_Card1Meta, Txt_Card2Meta };
	CardIcons = { Img_Card0Icon, Img_Card1Icon, Img_Card2Icon };

	static_assert(MaxCards == 3, "카드 슬롯 수를 바꾸면 CardN 변수와 HandleCardNClicked 도 같이 늘려야 한다");
}

void UFADraftWidget::BindCardButtons()
{
	// UButton::OnClicked 는 파라미터가 없어 인덱스를 실어 보낼 수 없다 — 슬롯 수만큼 핸들러를 둔다.
	if (Btn_Card0)
	{
		Btn_Card0->OnClicked.AddUniqueDynamic(this, &UFADraftWidget::HandleCard0Clicked);
	}
	if (Btn_Card1)
	{
		Btn_Card1->OnClicked.AddUniqueDynamic(this, &UFADraftWidget::HandleCard1Clicked);
	}
	if (Btn_Card2)
	{
		Btn_Card2->OnClicked.AddUniqueDynamic(this, &UFADraftWidget::HandleCard2Clicked);
	}
}

void UFADraftWidget::RefreshCards()
{
	if (CardButtons.Num() != MaxCards)
	{
		// 아직 위젯 트리가 만들어지기 전 (AddToViewport 이전).
		return;
	}

	if (Txt_Title)
	{
		Txt_Title->SetText(FText::FromString(
			FString::Printf(TEXT("DRAFT %d  ·  룰 카드를 고르세요"), GetDraftIndex())));
	}

	RefreshTimerText(FMath::CeilToInt(GetTimeRemaining()));

	for (int32 Index = 0; Index < MaxCards; ++Index)
	{
		// 카드 루트를 따로 만들지 않았으면 버튼 자체를 숨긴다.
		UWidget* CardRoot = CardRoots[Index].Get();
		if (!CardRoot)
		{
			CardRoot = CardButtons[Index].Get();
		}

		if (!Cards.IsValidIndex(Index))
		{
			// 후보가 3장이 안 될 수 있다 (해당 티어 카드가 부족한 경우).
			if (CardRoot)
			{
				CardRoot->SetVisibility(ESlateVisibility::Collapsed);
			}
			continue;
		}

		const FFADraftCardInfo& Card = Cards[Index];

		if (CardRoot)
		{
			CardRoot->SetVisibility(ESlateVisibility::Visible);
		}

		if (CardButtons[Index] && (bTintCardButtonsByTier || bUsingFallbackLayout))
		{
			CardButtons[Index]->SetBackgroundColor(FADraftUI::TierToColor(Card.Tier));
		}

		if (CardNameTexts[Index])
		{
			CardNameTexts[Index]->SetText(Card.DisplayName);
		}

		if (CardDescTexts[Index])
		{
			CardDescTexts[Index]->SetText(Card.ShortDesc);
		}

		if (CardMetaTexts[Index])
		{
			CardMetaTexts[Index]->SetText(FText::FromString(FString::Printf(TEXT("[%d]  %s · %s · %s"),
				Index + 1,
				*Card.RuleId.ToString(),
				*FADraftUI::TierToString(Card.Tier),
				*FADraftUI::TeamToString(Card.Team))));
		}

		if (UImage* Icon = CardIcons[Index])
		{
			// 카드가 3장뿐이라 동기 로드로 충분하다. 아이콘이 없는 룰은 이미지 칸을 숨긴다.
			UTexture2D* IconTexture = Card.Icon.IsNull() ? nullptr : Card.Icon.LoadSynchronous();
			if (IconTexture)
			{
				Icon->SetBrushFromTexture(IconTexture, true);
				Icon->SetVisibility(ESlateVisibility::Visible);
			}
			else
			{
				Icon->SetVisibility(ESlateVisibility::Hidden);
			}
		}
	}
}

void UFADraftWidget::RefreshTimerText(int32 WholeSeconds)
{
	if (Txt_Timer)
	{
		Txt_Timer->SetText(FText::FromString(FString::Printf(TEXT("%d s"), FMath::Max(0, WholeSeconds))));
	}
}

//~ ---------------- WBP 가 없을 때 쓰는 임시 레이아웃 ----------------

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

	Txt_Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Txt_Title->SetFont(FADraftUI::MakeFont(20, true));
	Txt_Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(Txt_Title))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
	}

	Txt_Timer = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Txt_Timer->SetFont(FADraftUI::MakeFont(16, false));
	Txt_Timer->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.75f, 0.8f, 1.0f)));
	if (UVerticalBoxSlot* TimerSlot = Column->AddChildToVerticalBox(Txt_Timer))
	{
		TimerSlot->SetHorizontalAlignment(HAlign_Center);
		TimerSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}

	UHorizontalBox* CardRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Column->AddChildToVerticalBox(CardRow);

	// 카드 1장씩 만들어 Card0 / Btn_Card0 … 에 그대로 넣는다 — 채우는 코드는 WBP 와 완전히 같다.
	TArray<UWidget*> BuiltRoots;
	TArray<UButton*> BuiltButtons;
	TArray<UTextBlock*> BuiltNames;
	TArray<UTextBlock*> BuiltDescs;
	TArray<UTextBlock*> BuiltMetas;

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

		BuiltRoots.Add(CardSize);
		BuiltButtons.Add(CardButton);
		BuiltNames.Add(NameText);
		BuiltDescs.Add(DescText);
		BuiltMetas.Add(MetaText);
	}

	Card0 = BuiltRoots[0];
	Card1 = BuiltRoots[1];
	Card2 = BuiltRoots[2];

	Btn_Card0 = BuiltButtons[0];
	Btn_Card1 = BuiltButtons[1];
	Btn_Card2 = BuiltButtons[2];

	Txt_Card0Name = BuiltNames[0];
	Txt_Card1Name = BuiltNames[1];
	Txt_Card2Name = BuiltNames[2];

	Txt_Card0Desc = BuiltDescs[0];
	Txt_Card1Desc = BuiltDescs[1];
	Txt_Card2Desc = BuiltDescs[2];

	Txt_Card0Meta = BuiltMetas[0];
	Txt_Card1Meta = BuiltMetas[1];
	Txt_Card2Meta = BuiltMetas[2];

	bUsingFallbackLayout = true;

	BindCardButtons();
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
