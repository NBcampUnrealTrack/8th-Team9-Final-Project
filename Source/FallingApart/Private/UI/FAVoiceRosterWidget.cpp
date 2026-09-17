// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/FAVoiceRosterWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/EditableText.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "SettingsKitMenuWidget.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"
#include "Voice/FAVoiceTypes.h"
#include "Voice/FAVoiceUserSettings.h"

#define LOCTEXT_NAMESPACE "FAVoiceRoster"

namespace FAVoiceRosterUI
{
	static const FLinearColor LabelColor(0.85f, 0.85f, 0.88f, 1.f);
	static const FLinearColor SectionColor(1.f, 0.78f, 0.25f, 1.f);
	static const FLinearColor HintColor(0.6f, 0.6f, 0.65f, 1.f);

	/** 슬라이더 한 칸. 마우스로 끌 때 값이 미세하게 흔들리지 않게 한다. */
	static constexpr float VolumeStep = 0.05f;

	static UTextBlock* MakeText(UWidgetTree* WidgetTree, const FText& Text, int32 FontSize, const FLinearColor& Color)
	{
		UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Block->SetText(Text);
		Block->SetColorAndOpacity(FSlateColor(Color));

		FSlateFontInfo Font = Block->GetFont();
		Font.Size = FontSize;
		Block->SetFont(Font);

		return Block;
	}
}

// ============================================================
// 한 줄
// ============================================================
TSharedRef<SWidget> UFAVoiceRosterEntryWidget::RebuildWidget()
{
	// WBP 를 지정하지 않았을 때만 임시 레이아웃을 만든다.
	if (WidgetTree && !WidgetTree->RootWidget && !IsDesignTime())
	{
		BuildFallbackRow();
	}

	return Super::RebuildWidget();
}

void UFAVoiceRosterEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Slider_Volume)
	{
		Slider_Volume->SetMinValue(UFAVoiceUserSettings::MinPlayerVolume);
		Slider_Volume->SetMaxValue(UFAVoiceUserSettings::MaxPlayerVolume);
		Slider_Volume->SetStepSize(FAVoiceRosterUI::VolumeStep);

		Slider_Volume->OnValueChanged.RemoveAll(this);
		Slider_Volume->OnValueChanged.AddUniqueDynamic(this, &UFAVoiceRosterEntryWidget::HandleVolumeChanged);
	}

	// 값 칸이 EditableText 면 직접 입력(0~200). 재구성 때 이전 연결부터 뗀다.
	if (VolumeFieldBinding)
	{
		VolumeFieldBinding->Unbind();
		VolumeFieldBinding = nullptr;
	}
	if (Slider_Volume && Text_VolumeValue)
	{
		VolumeFieldBinding = NewObject<USKValueFieldBinding>(this);
		VolumeFieldBinding->Init(Slider_Volume, Text_VolumeValue, 100.f, 0);
	}

	if (Check_Mute)
	{
		Check_Mute->OnCheckStateChanged.RemoveAll(this);
		Check_Mute->OnCheckStateChanged.AddUniqueDynamic(this, &UFAVoiceRosterEntryWidget::HandleMuteChanged);
	}

	RefreshFromSettings();
}

void UFAVoiceRosterEntryWidget::Setup(const FString& InPlayerKey, const FText& InPlayerName)
{
	PlayerKey = InPlayerKey;

	if (Text_PlayerName)
	{
		Text_PlayerName->SetText(InPlayerName);
	}

	RefreshFromSettings();
}

void UFAVoiceRosterEntryWidget::RefreshFromSettings()
{
	const UFAVoiceUserSettings* VoiceSettings = UFAVoiceUserSettings::Get(this);
	if (VoiceSettings == nullptr || PlayerKey.IsEmpty())
	{
		return;
	}

	const float Volume = VoiceSettings->GetPlayerVolume(PlayerKey);
	const bool bMuted = VoiceSettings->IsPlayerMuted(PlayerKey);

	// 값을 채워 넣는 동안 슬라이더 콜백이 되돌아와 같은 값을 다시 저장하는 것을 막는다.
	TGuardValue<bool> Guard(bUpdatingWidgets, true);

	if (Slider_Volume)
	{
		Slider_Volume->SetValue(Volume);
		Slider_Volume->SetIsEnabled(!bMuted);
	}

	if (Text_VolumeValue)
	{
		// 음소거 중에는 슬라이더처럼 값 칸도 막는다 — 안내 문구를 숫자로 고쳐 쓰지 못하게.
		Text_VolumeValue->SetIsEnabled(!bMuted);
		if (VolumeFieldBinding)
		{
			if (bMuted)
			{
				VolumeFieldBinding->SetStatusText(LOCTEXT("Muted", "음소거"));
			}
			else
			{
				VolumeFieldBinding->SetDisplayedValue(Volume);
			}
		}
		else
		{
			USKValueFieldBinding::SetWidgetText(Text_VolumeValue,
				bMuted ? LOCTEXT("Muted", "음소거") : USKValueFieldBinding::FormatValue(Volume, 100.f, 0));
		}
	}

	if (Check_Mute)
	{
		Check_Mute->SetIsChecked(bMuted);
	}
}

void UFAVoiceRosterEntryWidget::HandleVolumeChanged(float NewValue)
{
	if (bUpdatingWidgets || PlayerKey.IsEmpty())
	{
		return;
	}

	if (UFAVoiceUserSettings* VoiceSettings = UFAVoiceUserSettings::Get(this))
	{
		// 저장은 여기서 바로 한다(설정창의 적용 버튼과 무관). 잦은 ini 쓰기는 서브시스템이 모아 준다.
		VoiceSettings->SetPlayerVolume(PlayerKey, NewValue);
	}

	if (VolumeFieldBinding)
	{
		VolumeFieldBinding->SetDisplayedValue(NewValue);
	}
	else if (Text_VolumeValue)
	{
		USKValueFieldBinding::SetWidgetText(Text_VolumeValue, USKValueFieldBinding::FormatValue(NewValue, 100.f, 0));
	}
}

void UFAVoiceRosterEntryWidget::HandleMuteChanged(bool bChecked)
{
	if (bUpdatingWidgets || PlayerKey.IsEmpty())
	{
		return;
	}

	if (UFAVoiceUserSettings* VoiceSettings = UFAVoiceUserSettings::Get(this))
	{
		VoiceSettings->SetPlayerMuted(PlayerKey, bChecked);
	}

	RefreshFromSettings();
}

void UFAVoiceRosterEntryWidget::BuildFallbackRow()
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	WidgetTree->RootWidget = Row;

	Text_PlayerName = FAVoiceRosterUI::MakeText(WidgetTree, FText::GetEmpty(), 13, FAVoiceRosterUI::LabelColor);
	if (UHorizontalBoxSlot* NameSlot = Row->AddChildToHorizontalBox(Text_PlayerName))
	{
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		NameSlot->SetVerticalAlignment(VAlign_Center);
		NameSlot->SetPadding(FMargin(0.f, 2.f, 8.f, 2.f));
	}

	Slider_Volume = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
	if (UHorizontalBoxSlot* SliderSlot = Row->AddChildToHorizontalBox(Slider_Volume))
	{
		SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SliderSlot->SetVerticalAlignment(VAlign_Center);
		SliderSlot->SetPadding(FMargin(0.f, 2.f, 8.f, 2.f));
	}

	// 값 칸은 직접 입력이 되도록 EditableText 로 만든다.
	UEditableText* VolumeField = WidgetTree->ConstructWidget<UEditableText>(UEditableText::StaticClass());
	FEditableTextStyle VolumeStyle = VolumeField->WidgetStyle;
	VolumeStyle.Font.Size = 12;
	VolumeStyle.SetColorAndOpacity(FSlateColor(FAVoiceRosterUI::LabelColor));
	VolumeField->SetWidgetStyle(VolumeStyle);
	VolumeField->SetMinimumDesiredWidth(56.f);
	VolumeField->SetJustification(ETextJustify::Right);
	Text_VolumeValue = VolumeField;
	if (UHorizontalBoxSlot* ValueSlot = Row->AddChildToHorizontalBox(Text_VolumeValue))
	{
		ValueSlot->SetVerticalAlignment(VAlign_Center);
		ValueSlot->SetPadding(FMargin(0.f, 2.f, 8.f, 2.f));
	}

	Check_Mute = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass());
	Check_Mute->SetToolTipText(LOCTEXT("MuteTip", "이 사람 목소리만 끕니다. 나에게만 적용됩니다."));
	if (UHorizontalBoxSlot* MuteSlot = Row->AddChildToHorizontalBox(Check_Mute))
	{
		MuteSlot->SetVerticalAlignment(VAlign_Center);
	}
}

// ============================================================
// 목록
// ============================================================
TSharedRef<SWidget> UFAVoiceRosterWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget && !IsDesignTime())
	{
		BuildFallbackLayout();
	}

	return Super::RebuildWidget();
}

void UFAVoiceRosterWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_ResetAll)
	{
		Btn_ResetAll->OnClicked.RemoveAll(this);
		Btn_ResetAll->OnClicked.AddUniqueDynamic(this, &UFAVoiceRosterWidget::HandleResetAllClicked);
	}

	// 콘솔 명령이나 다른 화면에서 값이 바뀌어도 목록이 따라가게.
	if (UFAVoiceUserSettings* VoiceSettings = UFAVoiceUserSettings::Get(this))
	{
		VoiceSettingsChangedHandle = VoiceSettings->OnPlayerVoiceSettingsChanged.AddUObject(
			this, &UFAVoiceRosterWidget::HandlePlayerVoiceSettingsChanged);
	}

	RefreshRoster();

	// PlayerArray 에는 변경 알림이 없다. 설정창이 떠 있는 동안만 도는 가벼운 폴링으로 따라간다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RefreshTimerHandle, this, &UFAVoiceRosterWidget::RefreshRoster,
			FMath::Max(0.2f, RefreshInterval), true);
	}
}

void UFAVoiceRosterWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}

	if (VoiceSettingsChangedHandle.IsValid())
	{
		if (UFAVoiceUserSettings* VoiceSettings = UFAVoiceUserSettings::Get(this))
		{
			VoiceSettings->OnPlayerVoiceSettingsChanged.Remove(VoiceSettingsChangedHandle);
		}
		VoiceSettingsChangedHandle.Reset();
	}

	Super::NativeDestruct();
}

void UFAVoiceRosterWidget::CollectPlayers(TArray<TPair<FString, FText>>& OutPlayers) const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
	if (GameState == nullptr)
	{
		return;
	}

	const APlayerState* LocalPlayerState = GetOwningPlayerState();

	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		// 내 목소리는 나에게 재생되지 않는다. 줄을 만들면 아무 일도 안 하는 슬라이더가 된다.
		if (PlayerState == nullptr || PlayerState == LocalPlayerState)
		{
			continue;
		}

		if (PlayerState->IsABot())
		{
			continue;
		}

		// UniqueNetId 가 아직 안 온 사람은 다음 갱신에서 잡힌다. 이름으로 키를 대신 만들면 안 된다.
		const FString PlayerKey = UFAVoiceUserSettings::MakePlayerKey(PlayerState);
		if (PlayerKey.IsEmpty())
		{
			continue;
		}

		OutPlayers.Emplace(PlayerKey, FText::FromString(PlayerState->GetPlayerName()));
	}
}

void UFAVoiceRosterWidget::RefreshRoster()
{
	TArray<TPair<FString, FText>> Players;
	CollectPlayers(Players);

	TArray<FString> Keys;
	Keys.Reserve(Players.Num());
	for (const TPair<FString, FText>& Player : Players)
	{
		Keys.Add(Player.Key);
	}

	// 명단이 그대로면 다시 만들지 않는다 — 슬라이더를 잡고 있는 중에 위젯이 갈리면 손에서 놓친다.
	if (Keys == DisplayedKeys)
	{
		return;
	}

	DisplayedKeys = MoveTemp(Keys);

	// 내가 넣은 줄만 뺀다. ClearChildren 을 쓰면 WBP 가 Box_Players 안에 같이 넣어 둔
	// 안내문·초기화 버튼까지 같이 날아간다 — 상자를 어떻게 짰는지에 코드가 기대면 안 된다.
	for (UFAVoiceRosterEntryWidget* Existing : EntryWidgets)
	{
		if (Existing != nullptr)
		{
			Existing->RemoveFromParent();
		}
	}
	EntryWidgets.Reset();

	const TSubclassOf<UFAVoiceRosterEntryWidget> RowClass =
		EntryWidgetClass ? EntryWidgetClass : TSubclassOf<UFAVoiceRosterEntryWidget>(UFAVoiceRosterEntryWidget::StaticClass());

	for (const TPair<FString, FText>& Player : Players)
	{
		UFAVoiceRosterEntryWidget* Entry = CreateWidget<UFAVoiceRosterEntryWidget>(GetOwningPlayer(), RowClass);
		if (Entry == nullptr)
		{
			continue;
		}

		Entry->Setup(Player.Key, Player.Value);
		EntryWidgets.Add(Entry);

		if (Box_Players)
		{
			// 상자가 ScrollBox 인지 VerticalBox 인지에 따라 슬롯 타입이 다르다.
			// WBP 가 어느 쪽을 쓰든 여백은 같게 준다.
			UPanelSlot* EntrySlot = Box_Players->AddChild(Entry);
			if (UScrollBoxSlot* ScrollSlot = Cast<UScrollBoxSlot>(EntrySlot))
			{
				ScrollSlot->SetPadding(FMargin(0.f, 3.f));
			}
			else if (UVerticalBoxSlot* VerticalSlot = Cast<UVerticalBoxSlot>(EntrySlot))
			{
				VerticalSlot->SetPadding(FMargin(0.f, 3.f));
			}
		}
	}

	if (Text_Empty)
	{
		Text_Empty->SetVisibility(EntryWidgets.Num() > 0 ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	if (Btn_ResetAll)
	{
		Btn_ResetAll->SetIsEnabled(EntryWidgets.Num() > 0);
	}
}

void UFAVoiceRosterWidget::HandlePlayerVoiceSettingsChanged(const FString& PlayerKey, float NewScale)
{
	for (UFAVoiceRosterEntryWidget* Entry : EntryWidgets)
	{
		if (Entry != nullptr && Entry->GetPlayerKey() == PlayerKey)
		{
			Entry->RefreshFromSettings();
			return;
		}
	}
}

void UFAVoiceRosterWidget::HandleResetAllClicked()
{
	if (UFAVoiceUserSettings* VoiceSettings = UFAVoiceUserSettings::Get(this))
	{
		VoiceSettings->ResetAll();
	}

	for (UFAVoiceRosterEntryWidget* Entry : EntryWidgets)
	{
		if (Entry != nullptr)
		{
			Entry->RefreshFromSettings();
		}
	}
}

void UFAVoiceRosterWidget::BuildFallbackLayout()
{
	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	WidgetTree->RootWidget = Root;

	if (UVerticalBoxSlot* TitleSlot = Root->AddChildToVerticalBox(
		FAVoiceRosterUI::MakeText(WidgetTree, LOCTEXT("Title", "접속자별 음량"), 18, FAVoiceRosterUI::SectionColor)))
	{
		TitleSlot->SetPadding(FMargin(0.f, 16.f, 0.f, 6.f));
	}

	Root->AddChildToVerticalBox(FAVoiceRosterUI::MakeText(WidgetTree,
		LOCTEXT("Hint", "나에게만 적용되고 바로 저장됩니다. 상대의 마이크를 바꾸지는 않습니다."),
		11, FAVoiceRosterUI::HintColor));

	// 목록은 스크롤된다. 8인 방이면 줄이 7개라 VerticalBox 로는 아래쪽이 화면 밖으로 나간다.
	// ScrollBox 는 자식 높이만큼 계속 커지므로, 최대 높이를 정해 주지 않으면 스크롤이 생기지 않는다.
	USizeBox* ListBounds = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ListBounds->SetMaxDesiredHeight(FMath::Max(60.f, FallbackListMaxHeight));

	UScrollBox* PlayerList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	PlayerList->SetAllowOverscroll(false);
	ListBounds->AddChild(PlayerList);
	Box_Players = PlayerList;

	if (UVerticalBoxSlot* ListSlot = Root->AddChildToVerticalBox(ListBounds))
	{
		ListSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	}

	Text_Empty = FAVoiceRosterUI::MakeText(WidgetTree,
		LOCTEXT("Empty", "같이 있는 사람이 없습니다. 로비나 게임 중에 열면 목록이 뜹니다."),
		12, FAVoiceRosterUI::HintColor);
	Root->AddChildToVerticalBox(Text_Empty);

	Btn_ResetAll = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Btn_ResetAll->AddChild(FAVoiceRosterUI::MakeText(WidgetTree, LOCTEXT("ResetAll", "전부 기본값으로"), 12, FAVoiceRosterUI::LabelColor));
	if (UVerticalBoxSlot* ResetSlot = Root->AddChildToVerticalBox(Btn_ResetAll))
	{
		ResetSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		ResetSlot->SetHorizontalAlignment(HAlign_Left);
	}
}

#undef LOCTEXT_NAMESPACE
