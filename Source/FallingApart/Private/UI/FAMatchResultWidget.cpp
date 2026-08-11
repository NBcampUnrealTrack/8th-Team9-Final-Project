// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FAMatchResultWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Game/FAGameState.h"
#include "Player/FAPlayerController.h"
#include "Player/FAPlayerState.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "FAMatchResult"

namespace FAMatchResultUI
{
	FSlateFontInfo MakeFont(int32 Size, bool bBold)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}

	/** 순위 · 이름 · 점수 · 생존 시간을 한 줄로. 임시 레이아웃 전용 표기. */
	FText MakeRowText(const FFAMatchResultEntry& Entry)
	{
		return FText::FromString(FString::Printf(TEXT("%d위   %s   %d점   생존 %.0f초%s"),
			Entry.Rank,
			*Entry.PlayerName,
			Entry.TotalScore,
			Entry.SurvivalTime,
			Entry.DeathMatchPlacement > 0 ? TEXT("   (데스매치)") : TEXT("")));
	}
}

//~ ---------------- 순위표 한 줄 ----------------

void UFAMatchResultRowWidget::SetEntry(const FFAMatchResultEntry& InEntry)
{
	Entry = InEntry;

	if (Text_Rank)
	{
		Text_Rank->SetText(FText::AsNumber(Entry.Rank));
	}
	if (Text_Name)
	{
		Text_Name->SetText(FText::FromString(Entry.PlayerName));
	}
	if (Text_Score)
	{
		Text_Score->SetText(FText::AsNumber(Entry.TotalScore));
	}
	if (Text_SurvivalTime)
	{
		Text_SurvivalTime->SetText(FText::FromString(FString::Printf(TEXT("%.0f초"), Entry.SurvivalTime)));
	}

	OnEntrySet(Entry);
}

//~ ---------------- 순위표 ----------------

void UFAMatchResultWidget::OnReturnToLobbyClicked()
{
	// 세션 정리 · 레벨 이동은 컨트롤러가 한 곳에서 책임진다 (ESC 메뉴의 '메인 메뉴' 와 같은 경로).
	if (AFAPlayerController* FAPlayerController = Cast<AFAPlayerController>(GetOwningPlayer()))
	{
		FAPlayerController->ReturnToMainMenu();
	}
}

void UFAMatchResultWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);

	if (Btn_ReturnToLobby)
	{
		Btn_ReturnToLobby->OnClicked.AddUniqueDynamic(this, &UFAMatchResultWidget::OnReturnToLobbyClicked);
	}

	// WBP 에서 제목을 직접 써 넣었으면 그대로 둔다 — 비어 있을 때만 기본값을 채운다.
	if (Text_Title && Text_Title->GetText().IsEmpty())
	{
		Text_Title->SetText(LOCTEXT("MatchResultTitle", "최종 순위"));
	}

	RefreshEntries();
}

void UFAMatchResultWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	TimeSinceRefresh += InDeltaTime;
	if (TimeSinceRefresh < RefreshInterval)
	{
		return;
	}
	TimeSinceRefresh = 0.0f;

	RefreshEntries();
}

void UFAMatchResultWidget::RefreshEntries()
{
	const AFAGameState* FAGameState = GetWorld() ? GetWorld()->GetGameState<AFAGameState>() : nullptr;
	if (!FAGameState)
	{
		return;
	}

	TArray<AFAPlayerState*> Ranked;
	FAGameState->GetRankedPlayerStates(Ranked);

	const APlayerState* LocalPlayerState = GetOwningPlayer() ? GetOwningPlayer()->PlayerState : nullptr;

	TArray<FFAMatchResultEntry> NewEntries;
	NewEntries.Reserve(Ranked.Num());

	for (int32 Index = 0; Index < Ranked.Num(); ++Index)
	{
		const AFAPlayerState* FAPlayerState = Ranked[Index];
		if (!FAPlayerState)
		{
			continue;
		}

		FFAMatchResultEntry Entry;
		// 앞 사람과 완전 동점이면 같은 순위를 준다 (공동 1등 등).
		Entry.Rank = (Index > 0 && Ranked[Index - 1] && AFAGameState::AreFullyTied(*Ranked[Index - 1], *FAPlayerState))
			? NewEntries.Last().Rank
			: Index + 1;
		Entry.PlayerName = FAPlayerState->GetPlayerName();
		Entry.TotalScore = FAPlayerState->GetMatchTotalScore();
		Entry.SurvivalTime = FAPlayerState->GetMatchSurvivalTime();
		Entry.bIsLocalPlayer = (FAPlayerState == LocalPlayerState);
		Entry.DeathMatchPlacement = FAPlayerState->GetDeathMatchPlacement();

		NewEntries.Add(Entry);
	}

	// 점수·데스매치 결과는 이 위젯과 다른 액터를 타고 오므로 순위표가 뜬 뒤에 값이 바뀔 수 있다.
	// 실제로 달라졌을 때만 다시 그린다.
	bool bChanged = NewEntries.Num() != Entries.Num();
	for (int32 Index = 0; !bChanged && Index < NewEntries.Num(); ++Index)
	{
		const FFAMatchResultEntry& Old = Entries[Index];
		const FFAMatchResultEntry& New = NewEntries[Index];

		bChanged = Old.Rank != New.Rank
			|| Old.PlayerName != New.PlayerName
			|| Old.TotalScore != New.TotalScore
			|| Old.DeathMatchPlacement != New.DeathMatchPlacement;
	}

	if (!bChanged)
	{
		return;
	}

	Entries = MoveTemp(NewEntries);

	if (bUsingFallbackLayout)
	{
		RebuildFallbackRows();
	}
	else if (RowWidgetClass)
	{
		RebuildRowWidgets();
	}

	OnResultsChanged(Entries);
}

void UFAMatchResultWidget::RebuildRowWidgets()
{
	if (!Panel_Rows)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[FA] 순위표: RowWidgetClass 는 지정됐는데 Panel_Rows 가 없다 — WBP 에 'Panel_Rows' 이름의 컨테이너를 둘 것"));
		return;
	}

	Panel_Rows->ClearChildren();

	for (const FFAMatchResultEntry& Entry : Entries)
	{
		UFAMatchResultRowWidget* Row = CreateWidget<UFAMatchResultRowWidget>(GetOwningPlayer(), RowWidgetClass);
		if (!Row)
		{
			continue;
		}

		// 위젯 트리는 부모에 붙는 시점에 만들어진다 — SetEntry 보다 먼저 붙여야 TextBlock 이 잡힌다.
		Panel_Rows->AddChild(Row);
		Row->SetEntry(Entry);
	}
}

//~ ---------------- WBP 가 없을 때 쓰는 임시 레이아웃 ----------------

TSharedRef<SWidget> UFAMatchResultWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget && !IsDesignTime())
	{
		BuildFallbackLayout();
		bUsingFallbackLayout = true;
	}

	return Super::RebuildWidget();
}

void UFAMatchResultWidget::BuildFallbackLayout()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = RootCanvas;

	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Dim->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.75f));
	if (UCanvasPanelSlot* DimSlot = RootCanvas->AddChildToCanvas(Dim))
	{
		DimSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		DimSlot->SetOffsets(FMargin(0.0f));
	}

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Panel->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.95f));
	Panel->SetPadding(FMargin(36.0f, 28.0f));

	UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PanelSlot->SetAutoSize(true);
	PanelSlot->SetPosition(FVector2D::ZeroVector);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(Column);

	Text_Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text_Title->SetText(LOCTEXT("MatchResultTitle", "최종 순위"));
	Text_Title->SetFont(FAMatchResultUI::MakeFont(26, true));
	Text_Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(Text_Title))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 20.0f));
	}

	// 행은 RefreshEntries() 가 채운다.
	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel_Rows = Rows;
	if (UVerticalBoxSlot* RowsSlot = Column->AddChildToVerticalBox(Rows))
	{
		RowsSlot->SetHorizontalAlignment(HAlign_Fill);
		RowsSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 20.0f));
	}

	Btn_ReturnToLobby = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());

	UTextBlock* ButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ButtonText->SetText(LOCTEXT("MatchResultReturnToLobby", "로비로 나가기"));
	ButtonText->SetFont(FAMatchResultUI::MakeFont(16, false));
	ButtonText->SetJustification(ETextJustify::Center);
	Btn_ReturnToLobby->SetContent(ButtonText);

	if (UVerticalBoxSlot* ButtonSlot = Column->AddChildToVerticalBox(Btn_ReturnToLobby))
	{
		ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
	}
}

void UFAMatchResultWidget::RebuildFallbackRows()
{
	if (!Panel_Rows || !WidgetTree)
	{
		return;
	}

	Panel_Rows->ClearChildren();

	for (const FFAMatchResultEntry& Entry : Entries)
	{
		UTextBlock* RowText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		RowText->SetText(FAMatchResultUI::MakeRowText(Entry));
		RowText->SetFont(FAMatchResultUI::MakeFont(Entry.bIsLocalPlayer ? 18 : 16, Entry.bIsLocalPlayer));
		RowText->SetColorAndOpacity(FSlateColor(Entry.bIsLocalPlayer
			? FLinearColor(1.0f, 0.85f, 0.35f, 1.0f)
			: FLinearColor(0.85f, 0.85f, 0.9f, 1.0f)));

		Panel_Rows->AddChild(RowText);
	}
}

#undef LOCTEXT_NAMESPACE
