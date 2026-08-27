// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FADraftWidget.h"

#include "Audio/FAAudioSubsystem.h"
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

namespace FADraftAudio
{
	static const FName Open(TEXT("Draft_Open"));
	static const FName CardSelect(TEXT("Draft_CardSelect"));
	static const FName CardHover(TEXT("Draft_CardHover"));
	static const FName AutoPick(TEXT("Draft_AutoPick"));
	static const FName TimerTick(TEXT("Draft_TimerTick"));
	static const FName TimerUrgent(TEXT("Draft_TimerUrgent"));

	/** 남은 시간이 이 값 이하면 급한 소리로 바꾼다 (UX 가이드 §8.2 — 3초부터 붉게 점멸). */
	static constexpr float UrgentSeconds = 3.0f;

	//~ 라운드 시계(UFAHUDWidget)와 같은 방식 — 짧은 tick 을 간격만 좁혀가며 반복한다.
	//~ 루프를 피치로 빠르게 돌리면 음정까지 올라가 시계가 아닌 삑삑거리는 소리가 된다.

	/** 이 시간 아래부터 초침 간격이 좁아지기 시작한다. */
	static constexpr float AccelSeconds = 10.0f;

	/** AccelSeconds 위에서 쓰는 간격(초). */
	static constexpr float IntervalSlow = 1.0f;

	/** 남은 시간 0 에서의 간격(초). AccelSeconds 부터 여기까지 선형으로 좁아진다. */
	static constexpr float IntervalFast = 0.2f;

	/** 남은 시간에 대응하는 초침 간격. */
	float GetTickInterval(float SecondsRemaining)
	{
		if (SecondsRemaining >= AccelSeconds)
		{
			return IntervalSlow;
		}

		const float Alpha = FMath::Clamp(SecondsRemaining / AccelSeconds, 0.0f, 1.0f);
		return FMath::Lerp(IntervalFast, IntervalSlow, Alpha);
	}
}

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
	TickAccumulator = 0.0f;
	bIsOpen = true;

	RefreshCards();
	ApplyInputMode(true);

	// 화면을 안 보고 도망치는 중에도 "카드 왔다"가 먼저 도착해야 한다 (UX 가이드 §1).
	UFAAudioSubsystem::PlayFASound(this, FADraftAudio::Open);

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

	// 숫자키 1/2/3 선택도 이 경로를 탄다.
	UFAAudioSubsystem::PlayFASound(this, FADraftAudio::CardSelect);

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

		// 내가 안 고른 게 들어왔다는 신호. 서버는 이미 무작위로 확정한 상태다.
		UFAAudioSubsystem::PlayFASound(this, FADraftAudio::AutoPick);

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
		Info.BackgroundImage = Row->BackgroundImage;
		Info.CardBackgroundImage = Row->CardBackgroundImage;
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

	// 초침은 표시 초와 분리한다 — 표시 초에 묶여 있으면 간격이 1초 밑으로 못 내려간다.
	// 0 초는 AutoPick 이 대신 울리므로 여기서 겹쳐 내지 않는다.
	if (Remaining > 0.0f)
	{
		TickAccumulator += InDeltaTime;
		if (TickAccumulator >= FADraftAudio::GetTickInterval(Remaining))
		{
			TickAccumulator = 0.0f;

			UFAAudioSubsystem::PlayFASound(this, Remaining <= FADraftAudio::UrgentSeconds
				? FADraftAudio::TimerUrgent
				: FADraftAudio::TimerTick);
		}
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
	SlotBackgrounds = { Img_Slot0Background, Img_Slot1Background, Img_Slot2Background };
	SlotIcons = { Img_Slot0Icon, Img_Slot1Icon, Img_Slot2Icon };

	static_assert(MaxCards == 3, "카드 슬롯 수를 바꾸면 CardN 변수와 HandleCardNClicked 도 같이 늘려야 한다");
}

void UFADraftWidget::BindCardButtons()
{
	// UButton::OnClicked 는 파라미터가 없어 인덱스를 실어 보낼 수 없다 — 슬롯 수만큼 핸들러를 둔다.
	if (Btn_Card0)
	{
		Btn_Card0->OnClicked.AddUniqueDynamic(this, &UFADraftWidget::HandleCard0Clicked);
		Btn_Card0->OnHovered.AddUniqueDynamic(this, &UFADraftWidget::HandleCardHovered);
	}
	if (Btn_Card1)
	{
		Btn_Card1->OnClicked.AddUniqueDynamic(this, &UFADraftWidget::HandleCard1Clicked);
		Btn_Card1->OnHovered.AddUniqueDynamic(this, &UFADraftWidget::HandleCardHovered);
	}
	if (Btn_Card2)
	{
		Btn_Card2->OnClicked.AddUniqueDynamic(this, &UFADraftWidget::HandleCard2Clicked);
		Btn_Card2->OnHovered.AddUniqueDynamic(this, &UFADraftWidget::HandleCardHovered);
	}
}

void UFADraftWidget::HandleCardHovered()
{
	UFAAudioSubsystem::PlayFASound(this, FADraftAudio::CardHover);
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

		if (CardButtons.IsValidIndex(Index))
		{
			if (UButton* CardBtn = CardButtons[Index].Get())
			{
				if (UTexture2D* CardBgTex = Card.CardBackgroundImage.IsNull() ? nullptr : Card.CardBackgroundImage.LoadSynchronous())
				{
					FButtonStyle NewStyle = CardBtn->GetStyle();
					NewStyle.Normal.SetResourceObject(CardBgTex);
					NewStyle.Hovered.SetResourceObject(CardBgTex);
					NewStyle.Pressed.SetResourceObject(CardBgTex);
					CardBtn->SetStyle(NewStyle);
				}
				
				if (bTintCardButtonsByTier || bUsingFallbackLayout)
				{
					CardBtn->SetBackgroundColor(FADraftUI::TierToColor(Card.Tier));
				}
			}
		}
		
		if (SlotBackgrounds.IsValidIndex(Index))
		{
			if (UImage* BgImg = SlotBackgrounds[Index].Get())
			{
				if (UTexture2D* BgTexture = Card.BackgroundImage.IsNull() ? nullptr : Card.BackgroundImage.LoadSynchronous())
				{
					BgImg->SetBrushFromTexture(BgTexture, true);
					BgImg->SetVisibility(ESlateVisibility::HitTestInvisible);
				}
				else
				{
					BgImg->SetVisibility(ESlateVisibility::Collapsed);
				}
			}
		}
		
		if (SlotIcons.IsValidIndex(Index))
		{
			if (UImage* IconImg = SlotIcons[Index].Get())
			{
				if (UTexture2D* IconTexture = Card.Icon.IsNull() ? nullptr : Card.Icon.LoadSynchronous())
				{
					IconImg->SetBrushFromTexture(IconTexture, true);
					IconImg->SetVisibility(ESlateVisibility::HitTestInvisible);
				}
				else
				{
					IconImg->SetVisibility(ESlateVisibility::Collapsed);
				}
			}
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
	}
}

void UFADraftWidget::RefreshTimerText(int32 WholeSeconds)
{
	if (Txt_Timer)
	{
		Txt_Timer->SetText(FText::FromString(FString::Printf(TEXT("%d s"), FMath::Max(0, WholeSeconds))));
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
