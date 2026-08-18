// Fill out your copyright notice in the Description page of Project Settings.

#include "SettingsKitMenuWidget.h"

#include "SettingsKitLog.h"
#include "SettingsKitSubsystem.h"

#include "Blueprint/WidgetTree.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "SettingsKit"

namespace SKMenu
{
	// 인덱스 = EWindowMode::Type (0=Fullscreen, 1=WindowedFullscreen, 2=Windowed)
	static const TCHAR* WindowModeLabels[] = { TEXT("전체화면"), TEXT("테두리 없는 창"), TEXT("창모드") };

	static const TCHAR* QualityLabels[] = { TEXT("낮음"), TEXT("중간"), TEXT("높음"), TEXT("최고") };
	static const TCHAR* CustomQualityLabel = TEXT("커스텀");

	static const TCHAR* FrameLimitLabels[] = { TEXT("30"), TEXT("60"), TEXT("120"), TEXT("144"), TEXT("무제한") };
	static const float  FrameLimitValues[] = { 30.f, 60.f, 120.f, 144.f, 0.f };

	static const TCHAR* ColorblindLabels[] = { TEXT("없음"), TEXT("녹색맹"), TEXT("적색맹"), TEXT("청색맹") };

	struct FLangOption { const TCHAR* Label; const TCHAR* Culture; };
	static const FLangOption Languages[] = {
		{ TEXT("시스템 기본"), TEXT("") },
		{ TEXT("한국어"),      TEXT("ko") },
		{ TEXT("English"),     TEXT("en") },
	};

	static FString FrameLimitToLabel(float Limit)
	{
		for (int32 i = 0; i < UE_ARRAY_COUNT(FrameLimitValues); ++i)
		{
			if (FMath::IsNearlyEqual(FrameLimitValues[i], Limit))
			{
				return FrameLimitLabels[i];
			}
		}
		return FrameLimitLabels[UE_ARRAY_COUNT(FrameLimitLabels) - 1];
	}

	//~ 임시 레이아웃 색.
	static const FLinearColor PanelColor(0.05f, 0.05f, 0.07f, 0.97f);
	static const FLinearColor DimColor(0.f, 0.f, 0.f, 0.6f);
	static const FLinearColor LabelColor(0.85f, 0.85f, 0.88f, 1.f);
	static const FLinearColor TitleColor(1.f, 1.f, 1.f, 1.f);
}

USKSettingsSubsystem* USKSettingsMenuWidget::GetSettings() const
{
	return USKSettingsSubsystem::Get(this);
}

// ============================================================
// 수명 주기
// ============================================================
TSharedRef<SWidget> USKSettingsMenuWidget::RebuildWidget()
{
	// WBP 가 지정되지 않았을 때만 임시 레이아웃을 만든다.
	// IsDesignTime() 가드가 빠지면 UMG 디자이너 프리뷰가 오염된다 — 조립된 위젯이 WidgetTree 에
	// 없어서 Hierarchy 에 안 뜨고 선택·삭제가 전부 막힌다.
	if (WidgetTree && !WidgetTree->RootWidget && !IsDesignTime())
	{
		BuildFallbackLayout();
	}

	return Super::RebuildWidget();
}

void USKSettingsMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 바인딩은 여기서 한다 — Initialize() 는 임시 레이아웃(RebuildWidget)보다 먼저 돌아서
	// 그 시점엔 붙일 위젯이 아직 없다.
	ApplySliderRanges();
	BindCallbacks();

	OpenSettings();
}

void USKSettingsMenuWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RevertTimerHandle);
		World->GetTimerManager().ClearTimer(CloseTimerHandle);
	}
	Super::NativeDestruct();
}

void USKSettingsMenuWidget::OpenSettings()
{
	SetIsFocusable(true);

	if (USKSettingsSubsystem* Settings = GetSettings())
	{
		Settings->TakeSnapshot();
	}
	else
	{
		UE_LOG(LogSettingsKit, Error, TEXT("설정 서브시스템을 못 찾았다. 설정창이 아무것도 못 한다."));
	}

	PopulateStaticCombos();
	PopulateResolutionCombo();
	PopulateFromSettings();
	RequestKeybindListRefresh();

	if (Popup_Unsaved)          Popup_Unsaved->SetVisibility(ESlateVisibility::Collapsed);
	if (Popup_ResolutionRevert) Popup_ResolutionRevert->SetVisibility(ESlateVisibility::Collapsed);
	if (Txt_KeybindWarning)     Txt_KeybindWarning->SetVisibility(ESlateVisibility::Collapsed);

	// 되돌림 기준 = 창을 여는 시점의 해상도/모드.
	if (const UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		PrevResolution = GUS->GetScreenResolution();
		PrevWindowMode = static_cast<int32>(GUS->GetFullscreenMode());
	}

	bClosing = false;
	SetTab(ESKSettingsTab::Audio);
	PlaySlideIn();
	SetKeyboardFocus();
}

// ============================================================
// 슬라이더 범위
// ============================================================
void USKSettingsMenuWidget::ApplySliderRanges()
{
	auto SetRange = [](USlider* Slider, float Min, float Max, bool bIntegerSteps = false)
	{
		if (!Slider)
		{
			return;
		}
		Slider->SetMinValue(Min);
		Slider->SetMaxValue(Max);
		if (bIntegerSteps)
		{
			Slider->SetStepSize(1.f);
			Slider->MouseUsesStep = true;
		}
	};

	//~ 0~1 짜리 (볼륨·보이스). UMG 기본값과 같지만 명시해 둔다.
	SetRange(Sld_Master, 0.f, 1.f);
	SetRange(Sld_Music, 0.f, 1.f);
	SetRange(Sld_SFX, 0.f, 1.f);
	SetRange(Sld_UI, 0.f, 1.f);
	SetRange(Sld_VoiceOutput, 0.f, 1.f);
	SetRange(Sld_VoiceInput, 0.f, 1.f);
	SetRange(Sld_VoiceGate, 0.f, 1.f);

	//~ 0~1 이 아닌 것들. 이걸 안 걸면 WBP 에 놓는 순간 값이 잘린다.
	SetRange(Sld_Gamma, 1.f, 4.f);
	SetRange(Sld_FOV, 60.f, 120.f);
	SetRange(Sld_SubtitleScale, 0.5f, 2.f);
	SetRange(Sld_MouseSensitivity, 0.1f, 3.f);

	//~ 세부 화질은 0~3 정수 단계다.
	SetRange(Sld_ShadowQuality, 0.f, 3.f, true);
	SetRange(Sld_TextureQuality, 0.f, 3.f, true);
	SetRange(Sld_EffectsQuality, 0.f, 3.f, true);
	SetRange(Sld_ViewDistance, 0.f, 3.f, true);
	SetRange(Sld_AntiAliasing, 0.f, 3.f, true);
	SetRange(Sld_PostProcess, 0.f, 3.f, true);
}

// ============================================================
// 바인딩
// ============================================================
void USKSettingsMenuWidget::BindCallbacks()
{
	// 탭
	if (Btn_TabAudio)    Btn_TabAudio->OnClicked.AddUniqueDynamic(this, &USKSettingsMenuWidget::ShowAudioTab);
	if (Btn_TabVoice)    Btn_TabVoice->OnClicked.AddUniqueDynamic(this, &USKSettingsMenuWidget::ShowVoiceTab);
	if (Btn_TabVideo)    Btn_TabVideo->OnClicked.AddUniqueDynamic(this, &USKSettingsMenuWidget::ShowVideoTab);
	if (Btn_TabControls) Btn_TabControls->OnClicked.AddUniqueDynamic(this, &USKSettingsMenuWidget::ShowControlsTab);

	// 오디오
	if (Sld_Master) Sld_Master->OnValueChanged.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleMasterChanged);
	if (Sld_Music)  Sld_Music->OnValueChanged.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleMusicChanged);
	if (Sld_SFX)    Sld_SFX->OnValueChanged.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleSFXChanged);
	if (Sld_UI)     Sld_UI->OnValueChanged.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleUIChanged);
	if (Chk_Mute)   Chk_Mute->OnCheckStateChanged.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleMuteChanged);

	// 보이스
	if (Sld_VoiceOutput) Sld_VoiceOutput->OnValueChanged.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleVoiceOutputChanged);
	if (Sld_VoiceInput)  Sld_VoiceInput->OnValueChanged.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleVoiceInputChanged);
	if (Sld_VoiceGate)   Sld_VoiceGate->OnValueChanged.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleVoiceGateChanged);
	if (Chk_VoiceMute)   Chk_VoiceMute->OnCheckStateChanged.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleVoiceMuteChanged);

	// 화면
	if (Cmb_Resolution)    Cmb_Resolution->OnSelectionChanged.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleResolutionSelected);
	if (Cmb_WindowMode)    Cmb_WindowMode->OnSelectionChanged.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleWindowModeSelected);
	if (Cmb_QualityPreset) Cmb_QualityPreset->OnSelectionChanged.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleQualityPresetSelected);
	if (Cmb_FrameLimit)    Cmb_FrameLimit->OnSelectionChanged.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleFrameLimitSelected);
	if (Cmb_Language)      Cmb_Language->OnSelectionChanged.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleLanguageSelected);
	if (Cmb_Colorblind)    Cmb_Colorblind->OnSelectionChanged.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleColorblindSelected);
	if (Chk_VSync)         Chk_VSync->OnCheckStateChanged.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleVSyncChanged);
	if (Chk_Subtitles)     Chk_Subtitles->OnCheckStateChanged.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleSubtitlesChanged);
	if (Sld_Gamma)         Sld_Gamma->OnValueChanged.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleGammaChanged);
	if (Sld_FOV)           Sld_FOV->OnValueChanged.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleFOVChanged);
	if (Sld_SubtitleScale) Sld_SubtitleScale->OnValueChanged.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleSubtitleScaleChanged);

	if (Sld_ShadowQuality)  Sld_ShadowQuality->OnValueChanged.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleShadowQualityChanged);
	if (Sld_TextureQuality) Sld_TextureQuality->OnValueChanged.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleTextureQualityChanged);
	if (Sld_EffectsQuality) Sld_EffectsQuality->OnValueChanged.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleEffectsQualityChanged);
	if (Sld_ViewDistance)   Sld_ViewDistance->OnValueChanged.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleViewDistanceChanged);
	if (Sld_AntiAliasing)   Sld_AntiAliasing->OnValueChanged.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleAntiAliasingChanged);
	if (Sld_PostProcess)    Sld_PostProcess->OnValueChanged.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandlePostProcessChanged);

	// 조작
	if (Sld_MouseSensitivity) Sld_MouseSensitivity->OnValueChanged.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleMouseSensitivityChanged);
	if (Chk_InvertY)          Chk_InvertY->OnCheckStateChanged.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleInvertYChanged);
	if (Btn_ResetKeybinds)    Btn_ResetKeybinds->OnClicked.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleResetKeybindsClicked);

	// 하단 바
	if (Btn_Apply)    Btn_Apply->OnClicked.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleApplyClicked);
	if (Btn_Cancel)   Btn_Cancel->OnClicked.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleCancelClicked);
	if (Btn_ResetTab) Btn_ResetTab->OnClicked.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleResetTabClicked);

	// 미저장 팝업
	if (Btn_UnsavedSave)    Btn_UnsavedSave->OnClicked.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleUnsavedSave);
	if (Btn_UnsavedDiscard) Btn_UnsavedDiscard->OnClicked.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleUnsavedDiscard);
	if (Btn_UnsavedCancel)  Btn_UnsavedCancel->OnClicked.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleUnsavedCancel);

	// 해상도 되돌림 팝업
	if (Btn_RevertKeep) Btn_RevertKeep->OnClicked.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleRevertKeep);
	if (Btn_RevertBack) Btn_RevertBack->OnClicked.AddUniqueDynamic(this, &USKSettingsMenuWidget::HandleRevertBack);

	// 호버 사운드 — 버튼 전부 공용.
	UButton* const HoverButtons[] = {
		Btn_TabAudio, Btn_TabVoice, Btn_TabVideo, Btn_TabControls,
		Btn_Apply, Btn_Cancel, Btn_ResetTab, Btn_ResetKeybinds,
		Btn_UnsavedSave, Btn_UnsavedDiscard, Btn_UnsavedCancel,
		Btn_RevertKeep, Btn_RevertBack
	};
	for (UButton* Button : HoverButtons)
	{
		if (Button)
		{
			Button->OnHovered.AddUniqueDynamic(this, &USKSettingsMenuWidget::PlayHoverSound);
		}
	}
}

// ============================================================
// 콤보 채우기
// ============================================================
void USKSettingsMenuWidget::PopulateStaticCombos()
{
	TGuardValue<bool> Guard(bPopulating, true);

	if (Cmb_WindowMode)
	{
		Cmb_WindowMode->ClearOptions();
		for (const TCHAR* Label : SKMenu::WindowModeLabels)
		{
			Cmb_WindowMode->AddOption(Label);
		}
	}
	if (Cmb_QualityPreset)
	{
		Cmb_QualityPreset->ClearOptions();
		for (const TCHAR* Label : SKMenu::QualityLabels)
		{
			Cmb_QualityPreset->AddOption(Label);
		}
		Cmb_QualityPreset->AddOption(SKMenu::CustomQualityLabel);
	}
	if (Cmb_FrameLimit)
	{
		Cmb_FrameLimit->ClearOptions();
		for (const TCHAR* Label : SKMenu::FrameLimitLabels)
		{
			Cmb_FrameLimit->AddOption(Label);
		}
	}
	if (Cmb_Language)
	{
		Cmb_Language->ClearOptions();
		for (const SKMenu::FLangOption& Option : SKMenu::Languages)
		{
			Cmb_Language->AddOption(Option.Label);
		}
	}
	if (Cmb_Colorblind)
	{
		Cmb_Colorblind->ClearOptions();
		for (const TCHAR* Label : SKMenu::ColorblindLabels)
		{
			Cmb_Colorblind->AddOption(Label);
		}
	}
}

void USKSettingsMenuWidget::PopulateResolutionCombo()
{
	if (!Cmb_Resolution)
	{
		return;
	}

	TGuardValue<bool> Guard(bPopulating, true);

	Cmb_Resolution->ClearOptions();

	TArray<FIntPoint> Resolutions;
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
	for (const FIntPoint& Resolution : Resolutions)
	{
		Cmb_Resolution->AddOption(FString::Printf(TEXT("%d x %d"), Resolution.X, Resolution.Y));
	}
}

// ============================================================
// 저장값 → 위젯
// ============================================================
void USKSettingsMenuWidget::SetPercentLabel(UTextBlock* Label, float Value01) const
{
	if (Label)
	{
		Label->SetText(FText::AsNumber(FMath::RoundToInt(Value01 * 100.f)));
	}
}

void USKSettingsMenuWidget::SetNumberLabel(UTextBlock* Label, float Value, int32 Decimals) const
{
	if (Label)
	{
		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = Decimals;
		Options.MaximumFractionalDigits = Decimals;
		Label->SetText(FText::AsNumber(Value, &Options));
	}
}

void USKSettingsMenuWidget::PopulateFromSettings()
{
	const USKSettingsSubsystem* Settings = GetSettings();
	if (!Settings)
	{
		return;
	}

	TGuardValue<bool> Guard(bPopulating, true);
	const FSKSettingsValues& V = Settings->GetValues();

	// 오디오
	if (Sld_Master) Sld_Master->SetValue(V.MasterVolume);
	if (Sld_Music)  Sld_Music->SetValue(V.MusicVolume);
	if (Sld_SFX)    Sld_SFX->SetValue(V.SFXVolume);
	if (Sld_UI)     Sld_UI->SetValue(V.UIVolume);
	if (Chk_Mute)   Chk_Mute->SetIsChecked(V.bMuted);
	SetPercentLabel(Txt_MasterValue, V.MasterVolume);
	SetPercentLabel(Txt_MusicValue, V.MusicVolume);
	SetPercentLabel(Txt_SFXValue, V.SFXVolume);
	SetPercentLabel(Txt_UIValue, V.UIVolume);

	// 보이스
	if (Sld_VoiceOutput) Sld_VoiceOutput->SetValue(V.VoiceOutputVolume);
	if (Sld_VoiceInput)  Sld_VoiceInput->SetValue(V.VoiceInputGain);
	if (Sld_VoiceGate)   Sld_VoiceGate->SetValue(V.VoiceGateThreshold);
	if (Chk_VoiceMute)   Chk_VoiceMute->SetIsChecked(V.bVoiceMuted);
	SetPercentLabel(Txt_VoiceOutputValue, V.VoiceOutputVolume);
	SetPercentLabel(Txt_VoiceInputValue, V.VoiceInputGain);
	SetPercentLabel(Txt_VoiceGateValue, V.VoiceGateThreshold);

	// 화면 — 우리 값
	if (Sld_Gamma)         Sld_Gamma->SetValue(V.Gamma);
	if (Sld_FOV)           Sld_FOV->SetValue(V.FieldOfView);
	if (Sld_SubtitleScale) Sld_SubtitleScale->SetValue(V.SubtitleScale);
	if (Chk_Subtitles)     Chk_Subtitles->SetIsChecked(V.bSubtitlesEnabled);
	if (Cmb_Colorblind)    Cmb_Colorblind->SetSelectedIndex(static_cast<int32>(V.ColorblindType));
	SetNumberLabel(Txt_GammaValue, V.Gamma, 2);
	SetNumberLabel(Txt_FOVValue, V.FieldOfView, 0);
	SetNumberLabel(Txt_SubtitleScaleValue, V.SubtitleScale, 2);

	if (Cmb_Language)
	{
		int32 LanguageIndex = 0;
		for (int32 i = 0; i < UE_ARRAY_COUNT(SKMenu::Languages); ++i)
		{
			if (V.LanguageCulture.Equals(SKMenu::Languages[i].Culture))
			{
				LanguageIndex = i;
				break;
			}
		}
		Cmb_Language->SetSelectedIndex(LanguageIndex);
	}

	// 화면 — 엔진 값
	if (const UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		if (Cmb_Resolution)
		{
			const FIntPoint Current = GUS->GetScreenResolution();
			Cmb_Resolution->SetSelectedOption(FString::Printf(TEXT("%d x %d"), Current.X, Current.Y));
		}
		if (Cmb_WindowMode)
		{
			Cmb_WindowMode->SetSelectedIndex(FMath::Clamp(static_cast<int32>(GUS->GetFullscreenMode()), 0, 2));
		}
		if (Cmb_QualityPreset)
		{
			const int32 Level = GUS->GetOverallScalabilityLevel(); // -1 = 커스텀
			Cmb_QualityPreset->SetSelectedIndex(Level < 0 ? UE_ARRAY_COUNT(SKMenu::QualityLabels) : FMath::Clamp(Level, 0, 3));
		}
		if (Cmb_FrameLimit)
		{
			Cmb_FrameLimit->SetSelectedOption(SKMenu::FrameLimitToLabel(GUS->GetFrameRateLimit()));
		}
		if (Chk_VSync) Chk_VSync->SetIsChecked(GUS->IsVSyncEnabled());

		if (Sld_ShadowQuality)  Sld_ShadowQuality->SetValue(GUS->GetShadowQuality());
		if (Sld_TextureQuality) Sld_TextureQuality->SetValue(GUS->GetTextureQuality());
		if (Sld_EffectsQuality) Sld_EffectsQuality->SetValue(GUS->GetVisualEffectQuality());
		if (Sld_ViewDistance)   Sld_ViewDistance->SetValue(GUS->GetViewDistanceQuality());
		if (Sld_AntiAliasing)   Sld_AntiAliasing->SetValue(GUS->GetAntiAliasingQuality());
		if (Sld_PostProcess)    Sld_PostProcess->SetValue(GUS->GetPostProcessingQuality());
	}

	// 조작
	if (Sld_MouseSensitivity) Sld_MouseSensitivity->SetValue(V.MouseSensitivity);
	if (Chk_InvertY)          Chk_InvertY->SetIsChecked(V.bInvertYAxis);
	SetNumberLabel(Txt_MouseSensitivityValue, V.MouseSensitivity, 2);
}

// ============================================================
// 오디오 콜백
// ============================================================
void USKSettingsMenuWidget::HandleMasterChanged(float V)
{
	if (USKSettingsSubsystem* Settings = GetSettings()) { Settings->SetMasterVolume(V); }
	SetPercentLabel(Txt_MasterValue, V);
}

void USKSettingsMenuWidget::HandleMusicChanged(float V)
{
	if (USKSettingsSubsystem* Settings = GetSettings()) { Settings->SetMusicVolume(V); }
	SetPercentLabel(Txt_MusicValue, V);
}

void USKSettingsMenuWidget::HandleSFXChanged(float V)
{
	if (USKSettingsSubsystem* Settings = GetSettings()) { Settings->SetSFXVolume(V); }
	SetPercentLabel(Txt_SFXValue, V);
}

void USKSettingsMenuWidget::HandleUIChanged(float V)
{
	if (USKSettingsSubsystem* Settings = GetSettings())
	{
		Settings->SetUIVolume(V);

		// 미리듣기 — 슬라이더를 움직이면서 바로 들려야 한다.
		if (!PreviewSoundId.IsNone())
		{
			Settings->RequestUISound(PreviewSoundId);
		}
	}
	SetPercentLabel(Txt_UIValue, V);
}

void USKSettingsMenuWidget::HandleMuteChanged(bool bChecked)
{
	if (USKSettingsSubsystem* Settings = GetSettings()) { Settings->SetMuted(bChecked); }
}

// ============================================================
// 보이스 콜백
// ============================================================
void USKSettingsMenuWidget::HandleVoiceOutputChanged(float V)
{
	if (USKSettingsSubsystem* Settings = GetSettings()) { Settings->SetVoiceOutputVolume(V); }
	SetPercentLabel(Txt_VoiceOutputValue, V);
}

void USKSettingsMenuWidget::HandleVoiceInputChanged(float V)
{
	if (USKSettingsSubsystem* Settings = GetSettings()) { Settings->SetVoiceInputGain(V); }
	SetPercentLabel(Txt_VoiceInputValue, V);
}

void USKSettingsMenuWidget::HandleVoiceGateChanged(float V)
{
	if (USKSettingsSubsystem* Settings = GetSettings()) { Settings->SetVoiceGateThreshold(V); }
	SetPercentLabel(Txt_VoiceGateValue, V);
}

void USKSettingsMenuWidget::HandleVoiceMuteChanged(bool bChecked)
{
	if (USKSettingsSubsystem* Settings = GetSettings()) { Settings->SetVoiceMuted(bChecked); }
}

// ============================================================
// 화면 콜백
// ============================================================
void USKSettingsMenuWidget::HandleResolutionSelected(FString Item, ESelectInfo::Type Type)
{
	if (bPopulating || Type == ESelectInfo::Direct) { return; }

	UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!GUS) { return; }

	// "1920 x 1080" 파싱.
	TArray<FString> Parts;
	Item.ParseIntoArray(Parts, TEXT("x"), true);
	if (Parts.Num() == 2)
	{
		const int32 Width = FCString::Atoi(*Parts[0].TrimStartAndEnd());
		const int32 Height = FCString::Atoi(*Parts[1].TrimStartAndEnd());
		if (Width > 0 && Height > 0)
		{
			GUS->SetScreenResolution(FIntPoint(Width, Height));
			if (USKSettingsSubsystem* Settings = GetSettings()) { Settings->MarkDirty(); }
		}
	}
}

void USKSettingsMenuWidget::HandleWindowModeSelected(FString Item, ESelectInfo::Type Type)
{
	if (bPopulating || Type == ESelectInfo::Direct || !Cmb_WindowMode) { return; }

	if (UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		const int32 Index = FMath::Clamp(Cmb_WindowMode->GetSelectedIndex(), 0, 2);
		GUS->SetFullscreenMode(static_cast<EWindowMode::Type>(Index));
		if (USKSettingsSubsystem* Settings = GetSettings()) { Settings->MarkDirty(); }
	}
}

void USKSettingsMenuWidget::HandleQualityPresetSelected(FString Item, ESelectInfo::Type Type)
{
	if (bPopulating || Type == ESelectInfo::Direct || !Cmb_QualityPreset) { return; }

	UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!GUS) { return; }

	const int32 Index = Cmb_QualityPreset->GetSelectedIndex();
	if (Index >= 0 && Index <= 3)
	{
		GUS->SetOverallScalabilityLevel(Index);
		GUS->ApplyNonResolutionSettings();
		if (USKSettingsSubsystem* Settings = GetSettings()) { Settings->MarkDirty(); }
		PopulateFromSettings(); // 세부 슬라이더도 프리셋값으로 따라간다.
	}
	// "커스텀" 선택은 아무것도 하지 않는다 — 세부 슬라이더로 조절하는 상태다.
}

void USKSettingsMenuWidget::HandleFrameLimitSelected(FString Item, ESelectInfo::Type Type)
{
	if (bPopulating || Type == ESelectInfo::Direct || !Cmb_FrameLimit) { return; }

	UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!GUS) { return; }

	const int32 Index = Cmb_FrameLimit->GetSelectedIndex();
	if (Index >= 0 && Index < UE_ARRAY_COUNT(SKMenu::FrameLimitValues))
	{
		GUS->SetFrameRateLimit(SKMenu::FrameLimitValues[Index]);
		GUS->ApplyNonResolutionSettings();
		if (USKSettingsSubsystem* Settings = GetSettings()) { Settings->MarkDirty(); }
	}
}

void USKSettingsMenuWidget::HandleLanguageSelected(FString Item, ESelectInfo::Type Type)
{
	if (bPopulating || Type == ESelectInfo::Direct || !Cmb_Language) { return; }

	const int32 Index = Cmb_Language->GetSelectedIndex();
	if (Index >= 0 && Index < UE_ARRAY_COUNT(SKMenu::Languages))
	{
		if (USKSettingsSubsystem* Settings = GetSettings())
		{
			Settings->SetLanguageCulture(SKMenu::Languages[Index].Culture);
		}
	}
}

void USKSettingsMenuWidget::HandleColorblindSelected(FString Item, ESelectInfo::Type Type)
{
	if (bPopulating || Type == ESelectInfo::Direct || !Cmb_Colorblind) { return; }

	if (USKSettingsSubsystem* Settings = GetSettings())
	{
		const int32 Index = FMath::Clamp(Cmb_Colorblind->GetSelectedIndex(), 0, 3);
		Settings->SetColorblindType(static_cast<ESKColorblindType>(Index));
	}
}

void USKSettingsMenuWidget::HandleVSyncChanged(bool bChecked)
{
	if (bPopulating) { return; }

	if (UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		GUS->SetVSyncEnabled(bChecked);
		GUS->ApplyNonResolutionSettings();
		if (USKSettingsSubsystem* Settings = GetSettings()) { Settings->MarkDirty(); }
	}
}

void USKSettingsMenuWidget::HandleSubtitlesChanged(bool bChecked)
{
	if (USKSettingsSubsystem* Settings = GetSettings()) { Settings->SetSubtitlesEnabled(bChecked); }
}

void USKSettingsMenuWidget::HandleGammaChanged(float V)
{
	if (USKSettingsSubsystem* Settings = GetSettings()) { Settings->SetGamma(V); }
	SetNumberLabel(Txt_GammaValue, V, 2);
}

void USKSettingsMenuWidget::HandleFOVChanged(float V)
{
	// 미리보기도 게임 쪽이 델리게이트로 받아 처리한다.
	// 여기서 PlayerCameraManager::SetFOV 를 부르면 안 된다 — 그건 FOV 를 '잠그는' 함수라
	// UnlockFOV 를 부를 때까지 카메라 컴포넌트의 시야각이 통째로 무시된다.
	if (USKSettingsSubsystem* Settings = GetSettings()) { Settings->SetFieldOfView(V); }
	SetNumberLabel(Txt_FOVValue, V, 0);
}

void USKSettingsMenuWidget::HandleSubtitleScaleChanged(float V)
{
	if (USKSettingsSubsystem* Settings = GetSettings()) { Settings->SetSubtitleScale(V); }
	SetNumberLabel(Txt_SubtitleScaleValue, V, 2);
}

void USKSettingsMenuWidget::SyncPresetComboToCustom()
{
	// 세부 화질도 엔진이 값을 들고 있어서 setter 를 안 거친다. 여기서 한 번에 dirty 를 찍는다.
	if (USKSettingsSubsystem* Settings = GetSettings()) { Settings->MarkDirty(); }

	if (Cmb_QualityPreset)
	{
		TGuardValue<bool> Guard(bPopulating, true);
		Cmb_QualityPreset->SetSelectedOption(SKMenu::CustomQualityLabel);
	}
}

namespace
{
	/** 세부 화질 슬라이더 공통 — 0~3 으로 반올림해 엔진에 넣고 즉시 적용. */
	template <typename TSetter>
	void ApplyQualityLevel(float Value, TSetter&& Setter)
	{
		if (UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr)
		{
			Setter(GUS, FMath::Clamp(FMath::RoundToInt(Value), 0, 3));
			GUS->ApplyNonResolutionSettings();
		}
	}
}

void USKSettingsMenuWidget::HandleShadowQualityChanged(float V)
{
	if (bPopulating) { return; }
	ApplyQualityLevel(V, [](UGameUserSettings* GUS, int32 Level) { GUS->SetShadowQuality(Level); });
	SyncPresetComboToCustom();
}

void USKSettingsMenuWidget::HandleTextureQualityChanged(float V)
{
	if (bPopulating) { return; }
	ApplyQualityLevel(V, [](UGameUserSettings* GUS, int32 Level) { GUS->SetTextureQuality(Level); });
	SyncPresetComboToCustom();
}

void USKSettingsMenuWidget::HandleEffectsQualityChanged(float V)
{
	if (bPopulating) { return; }
	ApplyQualityLevel(V, [](UGameUserSettings* GUS, int32 Level) { GUS->SetVisualEffectQuality(Level); });
	SyncPresetComboToCustom();
}

void USKSettingsMenuWidget::HandleViewDistanceChanged(float V)
{
	if (bPopulating) { return; }
	ApplyQualityLevel(V, [](UGameUserSettings* GUS, int32 Level) { GUS->SetViewDistanceQuality(Level); });
	SyncPresetComboToCustom();
}

void USKSettingsMenuWidget::HandleAntiAliasingChanged(float V)
{
	if (bPopulating) { return; }
	ApplyQualityLevel(V, [](UGameUserSettings* GUS, int32 Level) { GUS->SetAntiAliasingQuality(Level); });
	SyncPresetComboToCustom();
}

void USKSettingsMenuWidget::HandlePostProcessChanged(float V)
{
	if (bPopulating) { return; }
	ApplyQualityLevel(V, [](UGameUserSettings* GUS, int32 Level) { GUS->SetPostProcessingQuality(Level); });
	SyncPresetComboToCustom();
}

// ============================================================
// 조작 콜백
// ============================================================
void USKSettingsMenuWidget::HandleMouseSensitivityChanged(float V)
{
	if (USKSettingsSubsystem* Settings = GetSettings()) { Settings->SetMouseSensitivity(V); }
	SetNumberLabel(Txt_MouseSensitivityValue, V, 2);
}

void USKSettingsMenuWidget::HandleInvertYChanged(bool bChecked)
{
	if (USKSettingsSubsystem* Settings = GetSettings()) { Settings->SetInvertYAxis(bChecked); }
}

void USKSettingsMenuWidget::HandleResetKeybindsClicked()
{
	PlayClickSound();

	if (USKSettingsSubsystem* Settings = GetSettings())
	{
		Settings->ResetKeybindsToDefaults(GetOwningLocalPlayer());
	}
	if (Txt_KeybindWarning)
	{
		Txt_KeybindWarning->SetVisibility(ESlateVisibility::Collapsed);
	}
	RequestKeybindListRefresh();
}

// ============================================================
// 탭
// ============================================================
void USKSettingsMenuWidget::ShowAudioTab()    { PlayClickSound(); SetTab(ESKSettingsTab::Audio); }
void USKSettingsMenuWidget::ShowVoiceTab()    { PlayClickSound(); SetTab(ESKSettingsTab::Voice); }
void USKSettingsMenuWidget::ShowVideoTab()    { PlayClickSound(); SetTab(ESKSettingsTab::Video); }
void USKSettingsMenuWidget::ShowControlsTab() { PlayClickSound(); SetTab(ESKSettingsTab::Controls); }

void USKSettingsMenuWidget::SetTab(ESKSettingsTab Tab)
{
	CurrentTab = Tab;
	if (TabSwitcher)
	{
		TabSwitcher->SetActiveWidgetIndex(static_cast<int32>(Tab));
	}
	UpdateTabButtonColors();
}

void USKSettingsMenuWidget::UpdateTabButtonColors()
{
	auto Tint = [this](UButton* Button, ESKSettingsTab Tab)
	{
		if (Button)
		{
			Button->SetBackgroundColor(CurrentTab == Tab ? TabActiveColor : TabInactiveColor);
		}
	};

	Tint(Btn_TabAudio, ESKSettingsTab::Audio);
	Tint(Btn_TabVoice, ESKSettingsTab::Voice);
	Tint(Btn_TabVideo, ESKSettingsTab::Video);
	Tint(Btn_TabControls, ESKSettingsTab::Controls);
}

// ============================================================
// 하단 바
// ============================================================
void USKSettingsMenuWidget::ApplyAll()
{
	USKSettingsSubsystem* Settings = GetSettings();
	if (!Settings)
	{
		return;
	}

	// 해상도/화면모드가 바뀌었는지 먼저 본다 — 바뀌었으면 되돌림 카운트다운을 띄운다.
	bool bResolutionChanged = false;
	FIntPoint NewResolution = PrevResolution;
	int32 NewWindowMode = PrevWindowMode;

	if (const UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		NewResolution = GUS->GetScreenResolution();
		NewWindowMode = static_cast<int32>(GUS->GetFullscreenMode());
		bResolutionChanged = (NewResolution != PrevResolution || NewWindowMode != PrevWindowMode);
	}

	Settings->ApplyAndSave();

	if (bResolutionChanged)
	{
		StartResolutionRevertCountdown();
	}
	else
	{
		PrevResolution = NewResolution;
		PrevWindowMode = NewWindowMode;
	}
}

void USKSettingsMenuWidget::HandleApplyClicked()
{
	PlayClickSound();
	ApplyAll();
}

void USKSettingsMenuWidget::HandleCancelClicked()
{
	PlayClickSound();

	const USKSettingsSubsystem* Settings = GetSettings();
	if (Settings && Settings->IsDirty())
	{
		ShowUnsavedPopup(true);
	}
	else
	{
		CloseInternal();
	}
}

void USKSettingsMenuWidget::HandleResetTabClicked()
{
	PlayClickSound();

	if (USKSettingsSubsystem* Settings = GetSettings())
	{
		Settings->ResetCategoryToDefaults(CurrentTab);
	}
	PopulateFromSettings();
}

// ============================================================
// 미저장 팝업
// ============================================================
void USKSettingsMenuWidget::ShowUnsavedPopup(bool bShow)
{
	if (Popup_Unsaved)
	{
		Popup_Unsaved->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	else if (bShow)
	{
		// 팝업 위젯이 없는 WBP 라면 물어볼 방법이 없다. 조용히 버리고 닫는다.
		if (USKSettingsSubsystem* Settings = GetSettings())
		{
			Settings->RevertToSnapshot();
		}
		CloseInternal();
	}
}

void USKSettingsMenuWidget::HandleUnsavedSave()
{
	PlayClickSound();
	ApplyAll();
	ShowUnsavedPopup(false);
	CloseInternal();
}

void USKSettingsMenuWidget::HandleUnsavedDiscard()
{
	PlayClickSound();
	if (USKSettingsSubsystem* Settings = GetSettings())
	{
		Settings->RevertToSnapshot();
	}
	ShowUnsavedPopup(false);
	CloseInternal();
}

void USKSettingsMenuWidget::HandleUnsavedCancel()
{
	PlayClickSound();
	ShowUnsavedPopup(false); // 계속 편집
}

void USKSettingsMenuWidget::CloseInternal()
{
	// 닫기 버튼 연타 · 팝업 경유 중복 호출 방지.
	if (bClosing)
	{
		return;
	}
	bClosing = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RevertTimerHandle);
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);
	PlaySlideOut();

	if (UWorld* World = GetWorld(); World && SlideOutDuration > 0.f)
	{
		World->GetTimerManager().SetTimer(
			CloseTimerHandle, this, &USKSettingsMenuWidget::FinishClose, SlideOutDuration, false);
	}
	else
	{
		FinishClose();
	}
}

void USKSettingsMenuWidget::FinishClose()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CloseTimerHandle);
	}
	RemoveFromParent();
}

// ============================================================
// 해상도 되돌림
// ============================================================
void USKSettingsMenuWidget::StartResolutionRevertCountdown()
{
	RevertSecondsLeft = FMath::Max(ResolutionRevertSeconds, 1);

	if (Popup_ResolutionRevert)
	{
		Popup_ResolutionRevert->SetVisibility(ESlateVisibility::Visible);
	}
	if (Txt_RevertCountdown)
	{
		Txt_RevertCountdown->SetText(FText::AsNumber(RevertSecondsLeft));
	}
	UpdateRevertCountdown(RevertSecondsLeft);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RevertTimerHandle, this, &USKSettingsMenuWidget::TickRevertCountdown, 1.f, true);
	}
}

void USKSettingsMenuWidget::TickRevertCountdown()
{
	--RevertSecondsLeft;

	if (Txt_RevertCountdown)
	{
		Txt_RevertCountdown->SetText(FText::AsNumber(FMath::Max(RevertSecondsLeft, 0)));
	}
	UpdateRevertCountdown(RevertSecondsLeft);

	if (RevertSecondsLeft <= 0)
	{
		FinishRevert(false); // 대답이 없으면 화면이 안 나오는 것으로 본다.
	}
}

void USKSettingsMenuWidget::HandleRevertKeep() { PlayClickSound(); FinishRevert(true); }
void USKSettingsMenuWidget::HandleRevertBack() { PlayClickSound(); FinishRevert(false); }

void USKSettingsMenuWidget::FinishRevert(bool bKeep)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RevertTimerHandle);
	}

	if (UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		if (!bKeep && PrevResolution != FIntPoint(0, 0))
		{
			GUS->SetScreenResolution(PrevResolution);
			GUS->SetFullscreenMode(static_cast<EWindowMode::Type>(PrevWindowMode));
			GUS->ApplyResolutionSettings(false);
			GUS->SaveSettings();
			PopulateFromSettings();
		}
		else if (bKeep)
		{
			PrevResolution = GUS->GetScreenResolution();
			PrevWindowMode = static_cast<int32>(GUS->GetFullscreenMode());
		}
	}

	if (Popup_ResolutionRevert)
	{
		Popup_ResolutionRevert->SetVisibility(ESlateVisibility::Collapsed);
	}
	UpdateRevertCountdown(0);
}

// ============================================================
// 키 리바인딩
// ============================================================
TArray<FSKKeybindRow> USKSettingsMenuWidget::GetKeybindRows() const
{
	if (const USKSettingsSubsystem* Settings = GetSettings())
	{
		return Settings->GetKeybindRows(GetOwningLocalPlayer());
	}
	return TArray<FSKKeybindRow>();
}

void USKSettingsMenuWidget::BeginRebind(FName MappingName)
{
	bAwaitingKey = true;
	PendingMappingName = MappingName;

	if (Txt_KeybindWarning)
	{
		Txt_KeybindWarning->SetText(LOCTEXT("PressAnyKey", "누를 키를 입력하세요. (ESC = 취소)"));
		Txt_KeybindWarning->SetVisibility(ESlateVisibility::Visible);
	}

	// 버튼 클릭 직후엔 Slate 가 포커스를 그 버튼으로 되돌린다. 같은 프레임에 포커스를 잡으면
	// 곧바로 뺏겨서 다음 키 입력을 못 받는다 — 한 프레임 뒤에 잡아야 한다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick([WeakThis = TWeakObjectPtr<USKSettingsMenuWidget>(this)]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->SetKeyboardFocus();
			}
		});
	}
	else
	{
		SetKeyboardFocus();
	}
}

void USKSettingsMenuWidget::RequestKeybindListRefresh()
{
	if (bUsingFallbackLayout)
	{
		RebuildFallbackKeybindList();
	}
	else
	{
		RefreshKeybindList();
	}
}

// ============================================================
// 입력
// ============================================================
FReply USKSettingsMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	if (bAwaitingKey)
	{
		const FName Mapping = PendingMappingName;
		bAwaitingKey = false;
		PendingMappingName = NAME_None;

		if (Key == EKeys::Escape)
		{
			if (Txt_KeybindWarning)
			{
				Txt_KeybindWarning->SetVisibility(ESlateVisibility::Collapsed);
			}
			return FReply::Handled();
		}

		FText Error;
		if (USKSettingsSubsystem* Settings = GetSettings();
			Settings && !Settings->TryRebindKey(GetOwningLocalPlayer(), Mapping, Key, Error))
		{
			if (Txt_KeybindWarning)
			{
				Txt_KeybindWarning->SetText(Error);
				Txt_KeybindWarning->SetVisibility(ESlateVisibility::Visible);
			}
		}
		else if (Txt_KeybindWarning)
		{
			Txt_KeybindWarning->SetVisibility(ESlateVisibility::Collapsed);
		}

		RequestKeybindListRefresh();
		return FReply::Handled();
	}

	if (Key == EKeys::Escape)
	{
		// 되돌림 팝업이 떠 있는 동안엔 ESC 로 빠져나갈 수 없다 — 대답을 받아야 한다.
		if (Popup_ResolutionRevert && Popup_ResolutionRevert->IsVisible())
		{
			return FReply::Handled();
		}
		HandleCancelClicked();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply USKSettingsMenuWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bAwaitingKey)
	{
		const FName Mapping = PendingMappingName;
		bAwaitingKey = false;
		PendingMappingName = NAME_None;

		FText Error;
		if (USKSettingsSubsystem* Settings = GetSettings();
			Settings && !Settings->TryRebindKey(GetOwningLocalPlayer(), Mapping, InMouseEvent.GetEffectingButton(), Error))
		{
			if (Txt_KeybindWarning)
			{
				Txt_KeybindWarning->SetText(Error);
				Txt_KeybindWarning->SetVisibility(ESlateVisibility::Visible);
			}
		}
		else if (Txt_KeybindWarning)
		{
			Txt_KeybindWarning->SetVisibility(ESlateVisibility::Collapsed);
		}

		RequestKeybindListRefresh();
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

// ============================================================
// 사운드
// ============================================================
void USKSettingsMenuWidget::PlayClickSound()
{
	if (USKSettingsSubsystem* Settings = GetSettings())
	{
		Settings->RequestUISound(ClickSoundId);
	}
}

void USKSettingsMenuWidget::PlayHoverSound()
{
	if (USKSettingsSubsystem* Settings = GetSettings())
	{
		Settings->RequestUISound(HoverSoundId);
	}
}

#undef LOCTEXT_NAMESPACE
