// Fill out your copyright notice in the Description page of Project Settings.
//
// WBP 를 지정하지 않았을 때 C++ 이 조립하는 임시 설정창.
// UI 담당이 WBP 를 만들면 이 파일의 코드는 한 줄도 돌지 않는다 — 같은 이름의 위젯을
// 배치하기만 하면 `BindWidgetOptional` 이 전부 물어간다.

#include "SettingsKitMenuWidget.h"

#include "SettingsKitSubsystem.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/EditableText.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "SettingsKit"

namespace SKFallback
{
	static const FLinearColor PanelColor(0.05f, 0.05f, 0.07f, 0.97f);
	static const FLinearColor PopupColor(0.09f, 0.09f, 0.12f, 1.f);
	static const FLinearColor DimColor(0.f, 0.f, 0.f, 0.6f);
	static const FLinearColor LabelColor(0.85f, 0.85f, 0.88f, 1.f);
	static const FLinearColor SectionColor(0.62f, 0.72f, 0.95f, 1.f);

	static constexpr float PanelWidth = 980.f;
	static constexpr float PanelHeight = 720.f;
	static constexpr float RowPadding = 4.f;
}

void USKKeybindRowProxy::HandleClicked()
{
	if (Menu.IsValid())
	{
		Menu->BeginRebind(MappingName);
	}
}

// ============================================================
// 조립 헬퍼
// ============================================================
UTextBlock* USKSettingsMenuWidget::MakeFallbackText(const FText& Text, int32 FontSize, const FLinearColor& Color) const
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Block->SetText(Text);
	Block->SetColorAndOpacity(FSlateColor(Color));

	FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", FontSize);
	Block->SetFont(Font);

	return Block;
}

UButton* USKSettingsMenuWidget::MakeFallbackButton(UPanelWidget* Parent, const FText& Label)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Button->AddChild(MakeFallbackText(Label, 14, FLinearColor::Black));

	if (Parent)
	{
		Parent->AddChild(Button);
	}
	return Button;
}

UVerticalBox* USKSettingsMenuWidget::MakeFallbackTab(UWidgetSwitcher* Switcher, const FText& Title)
{
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	Switcher->AddChild(Scroll);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Scroll->AddChild(Column);

	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(MakeFallbackText(Title, 18, SKFallback::SectionColor)))
	{
		TitleSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 10.f));
	}

	return Column;
}

USlider* USKSettingsMenuWidget::AddFallbackSlider(UVerticalBox* Column, const FText& Label, float Min, float Max, TObjectPtr<UWidget>& OutValueLabel)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

	if (UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(MakeFallbackText(Label, 14, SKFallback::LabelColor)))
	{
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}

	USlider* Slider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
	Slider->SetMinValue(Min);
	Slider->SetMaxValue(Max);
	if (UHorizontalBoxSlot* SliderSlot = Row->AddChildToHorizontalBox(Slider))
	{
		// 라벨보다 슬라이더가 넓어야 조작이 된다.
		SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SliderSlot->SetVerticalAlignment(VAlign_Center);
		SliderSlot->SetPadding(FMargin(12.f, 0.f));
	}

	// 값 칸은 직접 입력이 되도록 EditableText 로 만든다.
	UEditableText* ValueField = WidgetTree->ConstructWidget<UEditableText>(UEditableText::StaticClass());
	FEditableTextStyle ValueStyle = ValueField->WidgetStyle;
	ValueStyle.SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 14));
	ValueStyle.SetColorAndOpacity(FSlateColor(SKFallback::LabelColor));
	ValueField->SetWidgetStyle(ValueStyle);
	ValueField->SetMinimumDesiredWidth(48.f);
	ValueField->SetJustification(ETextJustify::Right);
	OutValueLabel = ValueField;
	if (UHorizontalBoxSlot* ValueSlot = Row->AddChildToHorizontalBox(OutValueLabel))
	{
		ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		ValueSlot->SetVerticalAlignment(VAlign_Center);
	}

	if (UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(Row))
	{
		RowSlot->SetPadding(FMargin(0.f, SKFallback::RowPadding));
	}

	return Slider;
}

UCheckBox* USKSettingsMenuWidget::AddFallbackCheckBox(UVerticalBox* Column, const FText& Label)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

	if (UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(MakeFallbackText(Label, 14, SKFallback::LabelColor)))
	{
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}

	UCheckBox* CheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass());
	if (UHorizontalBoxSlot* CheckSlot = Row->AddChildToHorizontalBox(CheckBox))
	{
		CheckSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		CheckSlot->SetVerticalAlignment(VAlign_Center);
	}

	if (UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(Row))
	{
		RowSlot->SetPadding(FMargin(0.f, SKFallback::RowPadding));
	}

	return CheckBox;
}

UComboBoxString* USKSettingsMenuWidget::AddFallbackCombo(UVerticalBox* Column, const FText& Label)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

	if (UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(MakeFallbackText(Label, 14, SKFallback::LabelColor)))
	{
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}

	UComboBoxString* Combo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	if (UHorizontalBoxSlot* ComboSlot = Row->AddChildToHorizontalBox(Combo))
	{
		ComboSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ComboSlot->SetVerticalAlignment(VAlign_Center);
		ComboSlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
	}

	if (UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(Row))
	{
		RowSlot->SetPadding(FMargin(0.f, SKFallback::RowPadding));
	}

	return Combo;
}

// ============================================================
// 전체 조립
// ============================================================
/**
 * 게임 전용 확장 위젯이 들어갈 빈 자리를 탭 맨 아래에 만든다.
 * WBP 를 쓰면 같은 이름의 패널이 대신 물린다(BindWidgetOptional).
 */
UVerticalBox* USKSettingsMenuWidget::MakeFallbackExtensionBox(UVerticalBox* Tab)
{
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UVerticalBoxSlot* BoxSlot = Tab->AddChildToVerticalBox(Box))
	{
		BoxSlot->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
	}
	return Box;
}

void USKSettingsMenuWidget::BuildFallbackLayout()
{
	bUsingFallbackLayout = true;

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = RootCanvas;

	// 바탕 어둡게.
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Dim->SetBrushColor(SKFallback::DimColor);
	if (UCanvasPanelSlot* DimSlot = RootCanvas->AddChildToCanvas(Dim))
	{
		DimSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		DimSlot->SetOffsets(FMargin(0.f));
	}

	// 본체 패널.
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Panel->SetBrushColor(SKFallback::PanelColor);
	Panel->SetPadding(FMargin(24.f, 20.f));
	if (UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(Panel))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetPosition(FVector2D::ZeroVector);
		PanelSlot->SetSize(FVector2D(SKFallback::PanelWidth, SKFallback::PanelHeight));
	}

	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->AddChild(Root);

	// 제목.
	if (UVerticalBoxSlot* TitleSlot = Root->AddChildToVerticalBox(MakeFallbackText(LOCTEXT("Title", "설정"), 24, FLinearColor::White)))
	{
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
	}

	// 탭 버튼 줄.
	UHorizontalBox* TabBar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UVerticalBoxSlot* TabBarSlot = Root->AddChildToVerticalBox(TabBar))
	{
		TabBarSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		TabBarSlot->SetHorizontalAlignment(HAlign_Center);
	}

	Btn_TabAudio    = MakeFallbackButton(TabBar, LOCTEXT("TabAudio", "오디오"));
	Btn_TabVoice    = MakeFallbackButton(TabBar, LOCTEXT("TabVoice", "보이스"));
	Btn_TabVideo    = MakeFallbackButton(TabBar, LOCTEXT("TabVideo", "화면"));
	Btn_TabControls = MakeFallbackButton(TabBar, LOCTEXT("TabControls", "조작"));

	// 탭 내용.
	TabSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass());
	if (UVerticalBoxSlot* SwitcherSlot = Root->AddChildToVerticalBox(TabSwitcher))
	{
		SwitcherSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	//~ ---------------- 오디오 탭 ----------------
	{
		UVerticalBox* Tab = MakeFallbackTab(TabSwitcher, LOCTEXT("SecAudio", "볼륨"));
		Sld_Master = AddFallbackSlider(Tab, LOCTEXT("Master", "전체"), 0.f, 1.f, Txt_MasterValue);
		Sld_Music  = AddFallbackSlider(Tab, LOCTEXT("Music", "배경음"), 0.f, 1.f, Txt_MusicValue);
		Sld_SFX    = AddFallbackSlider(Tab, LOCTEXT("SFX", "효과음"), 0.f, 1.f, Txt_SFXValue);
		Sld_UI     = AddFallbackSlider(Tab, LOCTEXT("UISound", "UI"), 0.f, 1.f, Txt_UIValue);
		Chk_Mute   = AddFallbackCheckBox(Tab, LOCTEXT("Mute", "전체 음소거"));
		Box_AudioExtra = MakeFallbackExtensionBox(Tab);
	}

	//~ ---------------- 보이스 탭 ----------------
	{
		UVerticalBox* Tab = MakeFallbackTab(TabSwitcher, LOCTEXT("SecVoiceOut", "듣기"));
		Sld_VoiceOutput = AddFallbackSlider(Tab, LOCTEXT("VoiceOut", "보이스 음량"), 0.f, 1.f, Txt_VoiceOutputValue);
		Chk_VoiceMute   = AddFallbackCheckBox(Tab, LOCTEXT("VoiceMute", "보이스 끄기"));

		if (UVerticalBoxSlot* SecSlot = Tab->AddChildToVerticalBox(MakeFallbackText(LOCTEXT("SecVoiceIn", "내 마이크"), 18, SKFallback::SectionColor)))
		{
			SecSlot->SetPadding(FMargin(0.f, 16.f, 0.f, 10.f));
		}
		Cmb_VoiceInputDevice = AddFallbackCombo(Tab, LOCTEXT("VoiceInputDevice", "입력 장치"));
		Cmb_VoiceMicMode = AddFallbackCombo(Tab, LOCTEXT("VoiceMicMode", "마이크 방식"));
		Sld_VoiceInput = AddFallbackSlider(Tab, LOCTEXT("VoiceIn", "입력 감도"), 0.f, 1.f, Txt_VoiceInputValue);
		Sld_VoiceGate  = AddFallbackSlider(Tab, LOCTEXT("VoiceGate", "잡음 차단"), 0.f, 1.f, Txt_VoiceGateValue);

		Tab->AddChildToVerticalBox(MakeFallbackText(
			LOCTEXT("VoiceHint", "잡음 차단을 올리면 작은 소리가 잘립니다. 숨소리·키보드 소리가 들어가면 올리세요."),
			11, SKFallback::LabelColor));

		Box_VoiceExtra = MakeFallbackExtensionBox(Tab);
	}

	//~ ---------------- 화면 탭 ----------------
	{
		UVerticalBox* Tab = MakeFallbackTab(TabSwitcher, LOCTEXT("SecDisplay", "화면"));
		Cmb_Resolution    = AddFallbackCombo(Tab, LOCTEXT("Resolution", "해상도"));
		Cmb_WindowMode    = AddFallbackCombo(Tab, LOCTEXT("WindowMode", "화면 모드"));
		Cmb_FrameLimit    = AddFallbackCombo(Tab, LOCTEXT("FrameLimit", "프레임 제한"));
		Chk_VSync         = AddFallbackCheckBox(Tab, LOCTEXT("VSync", "수직 동기화"));
		Sld_Gamma         = AddFallbackSlider(Tab, LOCTEXT("Gamma", "밝기"), 1.f, 4.f, Txt_GammaValue);
		Sld_FOV           = AddFallbackSlider(Tab, LOCTEXT("FOV", "시야각"), 60.f, 120.f, Txt_FOVValue);

		if (UVerticalBoxSlot* SecSlot = Tab->AddChildToVerticalBox(MakeFallbackText(LOCTEXT("SecQuality", "화질"), 18, SKFallback::SectionColor)))
		{
			SecSlot->SetPadding(FMargin(0.f, 16.f, 0.f, 10.f));
		}
		Cmb_QualityPreset = AddFallbackCombo(Tab, LOCTEXT("Preset", "프리셋"));

		// 세부 화질은 0~3 단계라 옆에 숫자를 붙여도 읽히지 않는다. 라벨은 만들되 감춘다.
		auto AddQualitySlider = [this, Tab](const FText& Label) -> USlider*
		{
			TObjectPtr<UWidget> HiddenLabel = nullptr;
			USlider* Slider = AddFallbackSlider(Tab, Label, 0.f, 3.f, HiddenLabel);
			Slider->SetStepSize(1.f);
			Slider->MouseUsesStep = true;
			if (HiddenLabel)
			{
				HiddenLabel->SetVisibility(ESlateVisibility::Collapsed);
			}
			return Slider;
		};

		Sld_ShadowQuality  = AddQualitySlider(LOCTEXT("Shadow", "그림자"));
		Sld_TextureQuality = AddQualitySlider(LOCTEXT("Texture", "텍스처"));
		Sld_EffectsQuality = AddQualitySlider(LOCTEXT("Effects", "이펙트"));
		Sld_ViewDistance   = AddQualitySlider(LOCTEXT("ViewDist", "시야 거리"));
		Sld_AntiAliasing   = AddQualitySlider(LOCTEXT("AA", "안티앨리어싱"));
		Sld_PostProcess    = AddQualitySlider(LOCTEXT("PostProcess", "후처리"));

		if (UVerticalBoxSlot* SecSlot = Tab->AddChildToVerticalBox(MakeFallbackText(LOCTEXT("SecAccess", "표시"), 18, SKFallback::SectionColor)))
		{
			SecSlot->SetPadding(FMargin(0.f, 16.f, 0.f, 10.f));
		}
		Cmb_Language      = AddFallbackCombo(Tab, LOCTEXT("Language", "언어"));
		Cmb_Colorblind    = AddFallbackCombo(Tab, LOCTEXT("Colorblind", "색약 보정"));
		Chk_Subtitles     = AddFallbackCheckBox(Tab, LOCTEXT("Subtitles", "자막"));
		Sld_SubtitleScale = AddFallbackSlider(Tab, LOCTEXT("SubtitleScale", "자막 크기"), 0.5f, 2.f, Txt_SubtitleScaleValue);

		Box_VideoExtra = MakeFallbackExtensionBox(Tab);
	}

	//~ ---------------- 조작 탭 ----------------
	{
		UVerticalBox* Tab = MakeFallbackTab(TabSwitcher, LOCTEXT("SecMouse", "마우스"));
		Sld_MouseSensitivity = AddFallbackSlider(Tab, LOCTEXT("Sensitivity", "감도"), 0.1f, 3.f, Txt_MouseSensitivityValue);
		Chk_InvertY          = AddFallbackCheckBox(Tab, LOCTEXT("InvertY", "Y축 반전"));

		if (UVerticalBoxSlot* SecSlot = Tab->AddChildToVerticalBox(MakeFallbackText(LOCTEXT("SecKeys", "키 설정"), 18, SKFallback::SectionColor)))
		{
			SecSlot->SetPadding(FMargin(0.f, 16.f, 0.f, 10.f));
		}

		Txt_KeybindWarning = MakeFallbackText(FText::GetEmpty(), 13, FLinearColor(1.f, 0.55f, 0.35f, 1.f));
		Txt_KeybindWarning->SetVisibility(ESlateVisibility::Collapsed);
		Tab->AddChildToVerticalBox(Txt_KeybindWarning);

		Box_KeybindList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		if (UVerticalBoxSlot* ListSlot = Tab->AddChildToVerticalBox(Box_KeybindList))
		{
			ListSlot->SetPadding(FMargin(0.f, 4.f));
		}

		Btn_ResetKeybinds = MakeFallbackButton(Tab, LOCTEXT("ResetKeys", "키 배치 초기화"));
		if (UVerticalBoxSlot* ResetSlot = Cast<UVerticalBoxSlot>(Btn_ResetKeybinds->Slot))
		{
			ResetSlot->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
			ResetSlot->SetHorizontalAlignment(HAlign_Left);
		}

		Box_ControlsExtra = MakeFallbackExtensionBox(Tab);
	}

	// 하단 바.
	UHorizontalBox* BottomBar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UVerticalBoxSlot* BottomSlot = Root->AddChildToVerticalBox(BottomBar))
	{
		BottomSlot->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
		BottomSlot->SetHorizontalAlignment(HAlign_Right);
	}

	Btn_ResetTab = MakeFallbackButton(BottomBar, LOCTEXT("ResetTab", "이 탭 초기화"));
	Btn_Apply    = MakeFallbackButton(BottomBar, LOCTEXT("Apply", "적용"));
	Btn_Cancel   = MakeFallbackButton(BottomBar, LOCTEXT("Close", "닫기"));

	// 미저장 확인 팝업.
	{
		UBorder* Popup = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Popup->SetBrushColor(SKFallback::PopupColor);
		Popup->SetPadding(FMargin(24.f, 20.f));
		Popup->SetVisibility(ESlateVisibility::Collapsed);
		Popup_Unsaved = Popup;

		if (UCanvasPanelSlot* PopupSlot = RootCanvas->AddChildToCanvas(Popup))
		{
			PopupSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			PopupSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			PopupSlot->SetPosition(FVector2D::ZeroVector);
			PopupSlot->SetAutoSize(true);
		}

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Popup->AddChild(Column);

		if (UVerticalBoxSlot* MsgSlot = Column->AddChildToVerticalBox(
			MakeFallbackText(LOCTEXT("UnsavedMsg", "저장하지 않은 변경이 있습니다."), 16, FLinearColor::White)))
		{
			MsgSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
			MsgSlot->SetHorizontalAlignment(HAlign_Center);
		}

		UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Column->AddChildToVerticalBox(ButtonRow);

		Btn_UnsavedSave    = MakeFallbackButton(ButtonRow, LOCTEXT("Save", "저장하고 닫기"));
		Btn_UnsavedDiscard = MakeFallbackButton(ButtonRow, LOCTEXT("Discard", "버리고 닫기"));
		Btn_UnsavedCancel  = MakeFallbackButton(ButtonRow, LOCTEXT("KeepEditing", "계속 편집"));
	}

	// 해상도 되돌림 팝업.
	{
		UBorder* Popup = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Popup->SetBrushColor(SKFallback::PopupColor);
		Popup->SetPadding(FMargin(24.f, 20.f));
		Popup->SetVisibility(ESlateVisibility::Collapsed);
		Popup_ResolutionRevert = Popup;

		if (UCanvasPanelSlot* PopupSlot = RootCanvas->AddChildToCanvas(Popup))
		{
			PopupSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			PopupSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			PopupSlot->SetPosition(FVector2D::ZeroVector);
			PopupSlot->SetAutoSize(true);
		}

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Popup->AddChild(Column);

		if (UVerticalBoxSlot* MsgSlot = Column->AddChildToVerticalBox(
			MakeFallbackText(LOCTEXT("RevertMsg", "이 해상도를 유지할까요?"), 16, FLinearColor::White)))
		{
			MsgSlot->SetHorizontalAlignment(HAlign_Center);
		}

		Txt_RevertCountdown = MakeFallbackText(FText::GetEmpty(), 22, FLinearColor(1.f, 0.78f, 0.25f, 1.f));
		if (UVerticalBoxSlot* CountSlot = Column->AddChildToVerticalBox(Txt_RevertCountdown))
		{
			CountSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 14.f));
			CountSlot->SetHorizontalAlignment(HAlign_Center);
		}

		UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Column->AddChildToVerticalBox(ButtonRow);

		Btn_RevertKeep = MakeFallbackButton(ButtonRow, LOCTEXT("Keep", "유지"));
		Btn_RevertBack = MakeFallbackButton(ButtonRow, LOCTEXT("Revert", "되돌리기"));
	}
}

// ============================================================
// 키 목록 (임시 레이아웃 전용)
// ============================================================
void USKSettingsMenuWidget::RebuildFallbackKeybindList()
{
	if (!Box_KeybindList)
	{
		return;
	}

	Box_KeybindList->ClearChildren();
	KeybindProxies.Reset();

	const TArray<FSKKeybindRow> Rows = GetKeybindRows();

	if (Rows.IsEmpty())
	{
		// 목록이 비는 건 십중팔구 준비가 덜 된 것이지 버그가 아니다. 뭘 해야 하는지 화면에 남긴다.
		Box_KeybindList->AddChild(MakeFallbackText(
			LOCTEXT("NoKeybinds",
				"리바인딩 가능한 키가 없습니다. 세 가지가 모두 필요합니다 —\n"
				"1. Project Settings → Engine → Enhanced Input → Enable User Settings 켜기\n"
				"2. IMC 의 각 매핑(또는 Input Action)에 Player Mappable Key Settings 를 붙이고 Name 채우기\n"
				"3. Project Settings → Plugins → Settings Kit → Rebindable Mapping Contexts 에 그 IMC 추가"),
			13, FLinearColor(1.f, 0.75f, 0.4f, 1.f)));
		return;
	}

	for (const FSKKeybindRow& Row : Rows)
	{
		UHorizontalBox* RowBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		if (UHorizontalBoxSlot* LabelSlot = RowBox->AddChildToHorizontalBox(MakeFallbackText(Row.DisplayName, 14, SKFallback::LabelColor)))
		{
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}

		UButton* KeyButton = MakeFallbackButton(RowBox, Row.KeyName);
		if (UHorizontalBoxSlot* ButtonSlot = Cast<UHorizontalBoxSlot>(KeyButton->Slot))
		{
			ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			ButtonSlot->SetVerticalAlignment(VAlign_Center);
		}

		// 버튼마다 자기 매핑 이름을 들고 있는 조각을 하나씩 붙인다.
		USKKeybindRowProxy* Proxy = NewObject<USKKeybindRowProxy>(this);
		Proxy->Menu = this;
		Proxy->MappingName = Row.MappingName;
		KeybindProxies.Add(Proxy);

		KeyButton->OnClicked.AddUniqueDynamic(Proxy, &USKKeybindRowProxy::HandleClicked);

		Box_KeybindList->AddChild(RowBox);
	}
}

#undef LOCTEXT_NAMESPACE
