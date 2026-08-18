// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FARoundResultWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Game/FAGameState.h"
#include "Player/FAPlayerState.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "FARoundResult"

namespace FARoundResultUI
{
	FSlateFontInfo MakeFont(int32 Size, bool bBold)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}

	/** 팀 점수 = 인간 팀 + 좀비 팀. 한 라운드에 둘 다 벌 수 있다(중간에 감염된 경우). */
	int32 GetTeamScore(const FFAPlayerScore& Score)
	{
		return Score.HumanTeam + Score.ZombieTeam;
	}

	int32 GetPersonalScore(const FFAPlayerScore& Score)
	{
		return Score.HumanPersonal + Score.ZombiePersonal;
	}

	/** 임시 레이아웃 전용 한 줄 표기. */
	FText MakeRowText(const FFARoundResultEntry& Entry)
	{
		return FText::FromString(FString::Printf(TEXT("%d위   %s   생존 %.0f초   팀 %+d / 개인 %+d   합계 %d%s"),
			Entry.Rank,
			*Entry.Row.PlayerName,
			Entry.Row.SurvivalTime,
			GetTeamScore(Entry.Row.Score),
			GetPersonalScore(Entry.Row.Score),
			Entry.Row.Score.GetTotal(),
			Entry.Row.bSurvived ? TEXT("   (생존)") : TEXT("   (감염)")));
	}

	FText MakeOutcomeText(EFARoundResult Result)
	{
		switch (Result)
		{
		case EFARoundResult::HumanWin:
			return LOCTEXT("RoundOutcomeHuman", "인간 승리");
		case EFARoundResult::ZombieWin:
			return LOCTEXT("RoundOutcomeZombie", "좀비 승리");
		default:
			return FText::GetEmpty();
		}
	}
}

//~ ---------------- 결과 한 줄 ----------------

void UFARoundResultRowWidget::SetEntry(const FFARoundResultEntry& InEntry)
{
	Entry = InEntry;

	// 칸이 숫자만 여섯 개면 어느 게 무엇인지 못 읽는다 — 머리글 행이 없으면 항목 이름을 칸 안에 같이 넣는다.
	const bool bLabels = bShowInlineLabels;

	if (Text_Rank)
	{
		Text_Rank->SetText(FText::FromString(bLabels
			? FString::Printf(TEXT("%d위"), Entry.Rank)
			: FString::Printf(TEXT("%d"), Entry.Rank)));
	}
	if (Text_Name)
	{
		Text_Name->SetText(FText::FromString(Entry.Row.PlayerName));
	}
	if (Text_SurvivalTime)
	{
		Text_SurvivalTime->SetText(FText::FromString(bLabels
			? FString::Printf(TEXT("생존 %.0f초"), Entry.Row.SurvivalTime)
			: FString::Printf(TEXT("%.0f초"), Entry.Row.SurvivalTime)));
	}
	if (Text_TeamScore)
	{
		const int32 TeamScore = FARoundResultUI::GetTeamScore(Entry.Row.Score);
		Text_TeamScore->SetText(FText::FromString(bLabels
			? FString::Printf(TEXT("팀 %+d"), TeamScore)
			: FString::Printf(TEXT("%+d"), TeamScore)));
	}
	if (Text_PersonalScore)
	{
		const int32 PersonalScore = FARoundResultUI::GetPersonalScore(Entry.Row.Score);
		Text_PersonalScore->SetText(FText::FromString(bLabels
			? FString::Printf(TEXT("개인 %+d"), PersonalScore)
			: FString::Printf(TEXT("%+d"), PersonalScore)));
	}
	if (Text_Total)
	{
		const int32 Total = Entry.Row.Score.GetTotal();
		Text_Total->SetText(FText::FromString(bLabels
			? FString::Printf(TEXT("합계 %d"), Total)
			: FString::Printf(TEXT("%d"), Total)));
	}
	if (Text_Status)
	{
		Text_Status->SetText(Entry.Row.bSurvived
			? LOCTEXT("RoundResultSurvived", "생존")
			: LOCTEXT("RoundResultInfected", "감염"));
	}

	OnEntrySet(Entry);
}

//~ ---------------- 결과 화면 ----------------

void UFARoundResultWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 결과를 읽는 화면이라 입력을 가져가지 않는다 — 다음 라운드로 그대로 이어져야 한다.
	SetIsFocusable(false);

	RefreshEntries();
}

void UFARoundResultWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
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

void UFARoundResultWidget::RefreshEntries()
{
	const AFAGameState* FAGameState = GetWorld() ? GetWorld()->GetGameState<AFAGameState>() : nullptr;
	if (!FAGameState)
	{
		return;
	}

	const FFARoundResultSnapshot& Snapshot = FAGameState->GetRoundResultSnapshot();

	// 스냅샷은 라운드당 한 번만 바뀐다. 위젯을 띄우라는 RPC 가 복제보다 먼저 도착할 수 있어
	// 뜬 직후 잠깐은 비어 있을 수 있고, 그동안은 여기서 계속 지켜본다.
	if (!Snapshot.IsValid()
		|| (Snapshot.RoundIndex == DisplayedRoundIndex
			&& Snapshot.Result == DisplayedResult
			&& Snapshot.Rows.Num() == Entries.Num()))
	{
		return;
	}

	DisplayedRoundIndex = Snapshot.RoundIndex;
	DisplayedResult = Snapshot.Result;

	const APlayerState* LocalPlayerState = GetOwningPlayer() ? GetOwningPlayer()->PlayerState : nullptr;

	Entries.Reset(Snapshot.Rows.Num());
	for (int32 Index = 0; Index < Snapshot.Rows.Num(); ++Index)
	{
		FFARoundResultEntry Entry;
		Entry.Row = Snapshot.Rows[Index];
		Entry.bIsLocalPlayer = (LocalPlayerState != nullptr && Entry.Row.PlayerState == LocalPlayerState);

		// 생존 시간이 같으면 같은 순위를 준다 — 끝까지 살아남은 사람들이 전부 1위로 묶인다.
		Entry.Rank = (Index > 0
			&& FMath::IsNearlyEqual(Snapshot.Rows[Index - 1].SurvivalTime, Entry.Row.SurvivalTime, 0.01f))
			? Entries.Last().Rank
			: Index + 1;

		Entries.Add(Entry);
	}

	UpdateHeaderTexts();

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

void UFARoundResultWidget::UpdateHeaderTexts()
{
	if (Text_Title)
	{
		Text_Title->SetText(FText::Format(
			LOCTEXT("RoundResultTitle", "라운드 {0} 결과"), FText::AsNumber(DisplayedRoundIndex)));
	}
	if (Text_Outcome)
	{
		Text_Outcome->SetText(FARoundResultUI::MakeOutcomeText(DisplayedResult));
	}
}

void UFARoundResultWidget::RebuildRowWidgets()
{
	if (!Panel_Rows)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[FA] 라운드 결과: RowWidgetClass 는 지정됐는데 Panel_Rows 가 없다 — WBP 에 'Panel_Rows' 이름의 컨테이너를 둘 것"));
		return;
	}

	Panel_Rows->ClearChildren();

	for (const FFARoundResultEntry& Entry : Entries)
	{
		UFARoundResultRowWidget* Row = CreateWidget<UFARoundResultRowWidget>(GetOwningPlayer(), RowWidgetClass);
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

TSharedRef<SWidget> UFARoundResultWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget && !IsDesignTime())
	{
		BuildFallbackLayout();
		bUsingFallbackLayout = true;
	}

	return Super::RebuildWidget();
}

void UFARoundResultWidget::BuildFallbackLayout()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = RootCanvas;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Panel->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.88f));
	Panel->SetPadding(FMargin(32.0f, 24.0f));

	// 최종 순위표와 달리 화면을 덮지 않는다 — 라운드 사이에도 월드가 보여야 한다.
	UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PanelSlot->SetAutoSize(true);
	PanelSlot->SetPosition(FVector2D::ZeroVector);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(Column);

	Text_Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text_Title->SetFont(FARoundResultUI::MakeFont(24, true));
	Text_Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(Text_Title))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}

	Text_Outcome = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text_Outcome->SetFont(FARoundResultUI::MakeFont(18, false));
	Text_Outcome->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.9f)));
	if (UVerticalBoxSlot* OutcomeSlot = Column->AddChildToVerticalBox(Text_Outcome))
	{
		OutcomeSlot->SetHorizontalAlignment(HAlign_Center);
		OutcomeSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
	}

	// 행은 RefreshEntries() 가 채운다.
	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel_Rows = Rows;
	if (UVerticalBoxSlot* RowsSlot = Column->AddChildToVerticalBox(Rows))
	{
		RowsSlot->SetHorizontalAlignment(HAlign_Fill);
	}
}

void UFARoundResultWidget::RebuildFallbackRows()
{
	if (!Panel_Rows || !WidgetTree)
	{
		return;
	}

	Panel_Rows->ClearChildren();

	for (const FFARoundResultEntry& Entry : Entries)
	{
		UTextBlock* RowText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		RowText->SetText(FARoundResultUI::MakeRowText(Entry));
		RowText->SetFont(FARoundResultUI::MakeFont(16, Entry.bIsLocalPlayer));
		RowText->SetColorAndOpacity(FSlateColor(Entry.bIsLocalPlayer
			? FLinearColor(1.0f, 0.85f, 0.35f)
			: FLinearColor(0.85f, 0.85f, 0.85f)));

		if (UPanelSlot* RowSlot = Panel_Rows->AddChild(RowText))
		{
			if (UVerticalBoxSlot* BoxSlot = Cast<UVerticalBoxSlot>(RowSlot))
			{
				BoxSlot->SetPadding(FMargin(0.0f, 3.0f));
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
