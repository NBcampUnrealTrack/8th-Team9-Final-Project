#include "UI/FAHUDDetailsWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Game/FAGameState.h"
#include "GAS/FAAbilitySystemComponent.h"
#include "HAL/IConsoleManager.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"
#include "Player/FACharacter.h"
#include "Player/FAPlayerController.h"
#include "Player/FAPlayerState.h"
#include "Rule/FARuleSettings.h"
#include "UI/FAHUD.h"
#include "UI/FAHUDControlRowWidget.h"
#include "UI/FADraftWidget.h"

#define LOCTEXT_NAMESPACE "FAHUDDetails"

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarFAHUDDetailsPreview(TEXT("FA.HUDDetailsPreview"), 0,
	TEXT("Show the HUD details for layout inspection. 0 restores hold-Tab behavior."), ECVF_Cheat);
#endif

void UFAHUDDetailsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetIsFocusable(false);
	RulePanels.Reset();
	RuleNames.Reset();
	RuleDescriptions.Reset();
	ScoreDescriptions.Reset();
	ScorePoints.Reset();
	for (int32 Index = 0; Index < 4; ++Index)
	{
		UWidget* Panel = GetWidgetFromName(*FString::Printf(TEXT("RulePanel_%d"), Index));
		UTextBlock* Name = Cast<UTextBlock>(GetWidgetFromName(*FString::Printf(TEXT("RuleName_%d"), Index)));
		UTextBlock* Description = Cast<UTextBlock>(GetWidgetFromName(*FString::Printf(TEXT("RuleDescription_%d"), Index)));
		if (ensureMsgf(Panel && Name && Description, TEXT("HUD details: missing rule widgets at %d"), Index))
		{
			RulePanels.Add(Panel);
			RuleNames.Add(Name);
			RuleDescriptions.Add(Description);
		}
	}
	for (int32 Index = 0; Index < 7; ++Index)
	{
		UTextBlock* Description = Cast<UTextBlock>(GetWidgetFromName(*FString::Printf(TEXT("ScoreDescription_%d"), Index)));
		UTextBlock* Points = Cast<UTextBlock>(GetWidgetFromName(*FString::Printf(TEXT("ScorePoints_%d"), Index)));
		if (ensureMsgf(Description && Points, TEXT("HUD details: missing score widgets at %d"), Index))
		{
			ScoreDescriptions.Add(Description);
			ScorePoints.Add(Points);
		}
	}
	ControlSignature.Reset();
	bDetailsVisible = false;
	RefreshCountdown = 0.f;
	if (DetailsCanvas) { DetailsCanvas->SetVisibility(ESlateVisibility::Collapsed); }
}

void UFAHUDDetailsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	AFAPlayerController* PC = Cast<AFAPlayerController>(GetOwningPlayer());
	const AFAGameState* State = GetWorld() ? GetWorld()->GetGameState<AFAGameState>() : nullptr;
	const AFAHUD* HUD = PC ? Cast<AFAHUD>(PC->GetHUD()) : nullptr;
	const bool bDraftOpen = HUD && HUD->GetDraftWidget() && HUD->GetDraftWidget()->IsOpen();
	bool bPreview = false;
#if !UE_BUILD_SHIPPING
	bPreview = CVarFAHUDDetailsPreview.GetValueOnGameThread() != 0;
#endif
	const bool bShow = PC && (PC->IsInputKeyDown(EKeys::Tab) || bPreview) && !PC->bShowMouseCursor
		&& !PC->IsPauseMenuOpen() && !PC->IsMatchResultOpen() && !bDraftOpen && State
		&& (bPreview || State->GetRoundPhase() == EFARoundPhase::InProgress || State->GetRoundPhase() == EFARoundPhase::DeathMatch);
	if (!DetailsCanvas)
	{
		return;
	}
	if (bDetailsVisible != bShow)
	{
		bDetailsVisible = bShow;
		DetailsCanvas->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		RefreshCountdown = 0.f;
	}
	if (!bShow)
	{
		return;
	}
	RefreshCountdown -= InDeltaTime;
	if (RefreshCountdown <= 0.f)
	{
		RefreshCountdown = 0.1f;
		RefreshDetails();
		RefreshControls();
	}
}

void UFAHUDDetailsWidget::RefreshDetails()
{
	ScoreHistoryTitle->SetText(LOCTEXT("ScoreHistory", "내 점수 기록"));
	ControlsTitle->SetText(LOCTEXT("Controls", "조작 안내"));
	EmptyScoreText->SetText(LOCTEXT("EmptyHistory", "아직 획득한 점수가 없습니다"));
	const APlayerController* PC = GetOwningPlayer();
	const AFACharacter* Character = PC ? Cast<AFACharacter>(PC->GetPawn()) : nullptr;
	const UFAAbilitySystemComponent* ASC = Character ? Character->GetFAAbilitySystemComponent() : nullptr;
	const TArray<FName> RuleIds = ASC ? ASC->GetActiveRuleIds() : TArray<FName>();
	const int32 Count = FMath::Min3(RuleIds.Num(), RulePanels.Num(), UFARuleSettings::GetMaxActiveRules());
	for (int32 Index = 0; Index < RulePanels.Num(); ++Index)
	{
		RulePanels[Index]->SetVisibility(Index < Count ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Index < Count)
		{
			const FFARuleDisplayInfo Info = UFARuleSettings::MakeRuleDisplayInfo(RuleIds[Index]);
			RuleNames[Index]->SetText(Info.DisplayName);
			RuleDescriptions[Index]->SetText(Info.ShortDesc);
		}
	}
	const AFAPlayerState* PS = PC ? PC->GetPlayerState<AFAPlayerState>() : nullptr;
	const TArray<FFAScoreFeedEntry>* History = PS ? &PS->GetScoreHistory() : nullptr;
	const int32 Num = History ? History->Num() : 0;
	EmptyScoreText->SetVisibility(Num ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	for (int32 Index = 0; Index < ScoreDescriptions.Num(); ++Index)
	{
		if (Index < Num)
		{
			const FFAScoreFeedEntry& Entry = (*History)[Num - Index - 1];
			ScoreDescriptions[Index]->SetText(FText::Format(LOCTEXT("HistoryRow", "R{0} · {1}"),
				FText::AsNumber(Entry.Round), Entry.GetDescription()));
			ScorePoints[Index]->SetText(Entry.Points > 0
				? FText::Format(LOCTEXT("PositivePoints", "+{0}"), FText::AsNumber(Entry.Points))
				: FText::AsNumber(Entry.Points));
		}
		else
		{
			ScoreDescriptions[Index]->SetText(FText::GetEmpty());
			ScorePoints[Index]->SetText(FText::GetEmpty());
		}
	}
}

void UFAHUDDetailsWidget::AddControl(const FText& Label, const TArray<FKey>& Keys)
{
	if (!Controls || !ControlRowClass) { return; }
	UFAHUDControlRowWidget* Row = CreateWidget<UFAHUDControlRowWidget>(this, ControlRowClass);
	if (Row)
	{
		Controls->AddChildToVerticalBox(Row);
		Row->SetControl(Label, Keys);
	}
}

void UFAHUDDetailsWidget::RefreshControls()
{
	const AFAPlayerController* PC = Cast<AFAPlayerController>(GetOwningPlayer());
	const AFACharacter* Character = PC ? Cast<AFACharacter>(PC->GetPawn()) : nullptr;
	const ULocalPlayer* Player = PC ? PC->GetLocalPlayer() : nullptr;
	const UEnhancedInputLocalPlayerSubsystem* Input = Player ? Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	struct FControl { FText Label; TArray<FKey> Keys; };
	TArray<FControl> Rows;
	auto AddAction = [&Rows, Input](const FText& Label, const UInputAction* Action)
	{
		TArray<FKey> Keys = Input && Action ? Input->QueryKeysMappedToAction(Action) : TArray<FKey>();
		Keys.RemoveAll([](const FKey& Key) { return Key.IsGamepadKey() || Key == EKeys::Mouse2D || !Key.IsValid(); });
		TArray<FKey> Unique;
		for (const FKey& Key : Keys) { Unique.AddUnique(Key); }
		Rows.Add({Label, MoveTemp(Unique)});
	};
	if (Character)
	{
		AddAction(LOCTEXT("Move", "이동 / 저항"), Character->MoveAction);
		AddAction(LOCTEXT("Jump", "점프 / QTE"), Character->JumpAction);
		AddAction(LOCTEXT("Crouch", "웅크리기"), Character->CrouchAction);
		AddAction(LOCTEXT("Perspective", "시점 전환"), Character->PerspectiveAction);
		AddAction(LOCTEXT("Primary", "포획 / 구출"), Character->PrimaryAction);
		AddAction(LOCTEXT("Interact", "상호작용"), Character->InteractAction);
	}
	Rows.Add({LOCTEXT("PushToTalk", "음성 채팅"), PC ? PC->GetPushToTalkKeys() : TArray<FKey>()});
	Rows.Add({LOCTEXT("Details", "상세 정보 (누르기)"), {EKeys::Tab}});
	Rows.Add({LOCTEXT("Pause", "메뉴"), PC ? PC->GetPauseMenuKeys() : TArray<FKey>()});
	FString Signature = FInternationalization::Get().GetCurrentCulture()->GetName();
	for (const FControl& Row : Rows)
	{
		Signature += TEXT("|") + Row.Label.ToString();
		for (const FKey& Key : Row.Keys) { Signature += TEXT(":") + Key.ToString(); }
	}
	if (Signature == ControlSignature) { return; }
	ControlSignature = MoveTemp(Signature);
	Controls->ClearChildren();
	for (const FControl& Row : Rows) { AddControl(Row.Label, Row.Keys); }
}

#undef LOCTEXT_NAMESPACE
