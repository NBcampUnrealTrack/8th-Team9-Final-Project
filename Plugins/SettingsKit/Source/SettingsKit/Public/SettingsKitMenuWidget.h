// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SettingsKitTypes.h"
#include "SettingsKitMenuWidget.generated.h"

class UButton;
class UCheckBox;
class UComboBoxString;
class UScrollBox;
class USKSettingsSubsystem;
class USlider;
class UTextBlock;
class UVerticalBox;
class UWidget;
class UWidgetSwitcher;
class USKSettingsMenuWidget;

/**
 * 설정창이 닫히기 시작했다는 신호. 띄운 쪽이 자기 화면을 되돌리거나 소리를 낼 때 쓴다.
 * 슬라이드 아웃이 시작되는 시점에 온다 — 실제 제거(FinishClose)보다 앞이다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSKOnSettingsClosed);

/**
 * 임시 레이아웃의 키 목록 한 행이 "내가 어느 매핑인지" 를 기억하는 통로.
 *
 * `UButton::OnClicked` 는 다이나믹 델리게이트라 람다를 못 문다. 행마다 이 조각을 하나씩
 * 만들어 두면 버튼 하나가 자기 매핑 이름을 들고 있게 된다. WBP 를 만들면 이 경로는 안 쓴다.
 */
UCLASS()
class SETTINGSKIT_API USKKeybindRowProxy : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient)
	TWeakObjectPtr<USKSettingsMenuWidget> Menu;

	UPROPERTY(Transient)
	FName MappingName;

	UFUNCTION()
	void HandleClicked();
};

/**
 * 설정창.
 *
 * WBP 를 만들면 같은 이름의 위젯을 놓는 것만으로 전부 물린다(`BindWidgetOptional`). WBP 가
 * 없거나 위젯을 덜 놓았어도 동작한다 — 없는 위젯은 그냥 건너뛴다.
 *
 * WBP 를 아예 지정하지 않으면 C++ 이 임시 레이아웃을 조립
 */
UCLASS()
class SETTINGSKIT_API USKSettingsMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 열릴 때 호출 — 스냅샷을 잡고 위젯을 저장값으로 채운다. NativeConstruct 가 자동으로 부른다. */
	UFUNCTION(BlueprintCallable, Category = "SettingsKit")
	void OpenSettings();

	/** 조작 탭 키 목록. WBP 가 목록을 직접 그릴 때 쓴다. */
	UFUNCTION(BlueprintCallable, Category = "SettingsKit")
	TArray<FSKKeybindRow> GetKeybindRows() const;

	/** 이 매핑의 키 재설정을 시작한다. 다음에 눌리는 키/마우스 버튼을 잡는다. */
	UFUNCTION(BlueprintCallable, Category = "SettingsKit")
	void BeginRebind(FName MappingName);

	/** 닫히기 시작할 때 한 번. 띄운 쪽이 구독한다. */
	UPROPERTY(BlueprintAssignable, Category = "SettingsKit")
	FSKOnSettingsClosed OnSettingsClosed;

	/** 닫기 연출이 끝난 뒤 실제로 화면에서 제거. 타이머가 자동으로 부르지만 WBP 애니 종료에서 불러도 된다. */
	UFUNCTION(BlueprintCallable, Category = "SettingsKit")
	void FinishClose();

	UFUNCTION(BlueprintCallable, Category = "SettingsKit")
	void SetTab(ESKSettingsTab Tab);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	//~ ---------------- WBP 가 구현할 것 (미구현이면 no-op) ----------------

	UFUNCTION(BlueprintImplementableEvent, Category = "SettingsKit")
	void PlaySlideIn();

	UFUNCTION(BlueprintImplementableEvent, Category = "SettingsKit")
	void PlaySlideOut();

	/** 키 목록을 다시 그리라는 신호. 데이터는 GetKeybindRows() 로 가져간다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "SettingsKit")
	void RefreshKeybindList();

	/** 해상도 되돌림 카운트다운. Seconds <= 0 이면 끝난 것이다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "SettingsKit")
	void UpdateRevertCountdown(int32 Seconds);

	//~ ---------------- 탭 ----------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UWidgetSwitcher> TabSwitcher;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UButton> Btn_TabAudio;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UButton> Btn_TabVoice;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UButton> Btn_TabVideo;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UButton> Btn_TabControls;

	UPROPERTY(EditDefaultsOnly, Category = "SettingsKit|Style")
	FLinearColor TabActiveColor = FLinearColor(1.0f, 0.78f, 0.25f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category = "SettingsKit|Style")
	FLinearColor TabInactiveColor = FLinearColor::White;

	//~ ---------------- 오디오 탭 ----------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<USlider> Sld_Master;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<USlider> Sld_Music;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<USlider> Sld_SFX;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<USlider> Sld_UI;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UCheckBox> Chk_Mute;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UTextBlock> Txt_MasterValue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UTextBlock> Txt_MusicValue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UTextBlock> Txt_SFXValue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UTextBlock> Txt_UIValue;

	//~ ---------------- 보이스 탭 ----------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<USlider> Sld_VoiceOutput;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<USlider> Sld_VoiceInput;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<USlider> Sld_VoiceGate;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UCheckBox> Chk_VoiceMute;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UTextBlock> Txt_VoiceOutputValue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UTextBlock> Txt_VoiceInputValue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UTextBlock> Txt_VoiceGateValue;

	//~ ---------------- 화면 탭 ----------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UComboBoxString> Cmb_Resolution;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UComboBoxString> Cmb_WindowMode;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UComboBoxString> Cmb_QualityPreset;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UComboBoxString> Cmb_FrameLimit;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UComboBoxString> Cmb_Language;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UComboBoxString> Cmb_Colorblind;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UCheckBox> Chk_VSync;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UCheckBox> Chk_Subtitles;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<USlider> Sld_Gamma;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<USlider> Sld_FOV;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<USlider> Sld_SubtitleScale;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UTextBlock> Txt_GammaValue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UTextBlock> Txt_FOVValue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UTextBlock> Txt_SubtitleScaleValue;

	//~ 세부 화질 (0~3). 움직이면 프리셋이 "커스텀"으로 바뀐다.

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<USlider> Sld_ShadowQuality;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<USlider> Sld_TextureQuality;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<USlider> Sld_EffectsQuality;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<USlider> Sld_ViewDistance;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<USlider> Sld_AntiAliasing;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<USlider> Sld_PostProcess;

	//~ ---------------- 조작 탭 ----------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<USlider> Sld_MouseSensitivity;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UTextBlock> Txt_MouseSensitivityValue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UCheckBox> Chk_InvertY;

	/** 키 목록이 들어갈 상자. WBP 가 직접 채워도 되고, 임시 레이아웃이면 C++ 이 채운다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UScrollBox> Box_KeybindList;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UButton> Btn_ResetKeybinds;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UTextBlock> Txt_KeybindWarning;

	//~ ---------------- 하단 바 ----------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UButton> Btn_Apply;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UButton> Btn_Cancel;

	/** 지금 탭만 기본값으로. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UButton> Btn_ResetTab;

	//~ ---------------- 미저장 확인 팝업 ----------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UWidget> Popup_Unsaved;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UButton> Btn_UnsavedSave;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UButton> Btn_UnsavedDiscard;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UButton> Btn_UnsavedCancel;

	//~ ---------------- 해상도 되돌림 팝업 ----------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UWidget> Popup_ResolutionRevert;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UButton> Btn_RevertKeep;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UButton> Btn_RevertBack;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "SettingsKit")
	TObjectPtr<UTextBlock> Txt_RevertCountdown;

	//~ ---------------- 설정 ----------------

	/** 닫기 연출 길이(초). WBP 의 SlideOut 애니 길이에 맞춘다. 0 이면 즉시 닫힌다. */
	UPROPERTY(EditDefaultsOnly, Category = "SettingsKit")
	float SlideOutDuration = 0.f;

	/** 해상도를 바꾼 뒤 되돌림까지 기다리는 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "SettingsKit")
	int32 ResolutionRevertSeconds = 15;

	/** 설정창이 요청할 사운드 ID. 게임의 사운드 표 행 이름과 맞춰 둔다. 비우면 소리를 안 낸다. */
	UPROPERTY(EditDefaultsOnly, Category = "SettingsKit|Sound")
	FName ClickSoundId;

	UPROPERTY(EditDefaultsOnly, Category = "SettingsKit|Sound")
	FName HoverSoundId;

	/** UI 볼륨 슬라이더를 움직일 때 미리듣기로 낼 소리. */
	UPROPERTY(EditDefaultsOnly, Category = "SettingsKit|Sound")
	FName PreviewSoundId;

private:
	USKSettingsSubsystem* GetSettings() const;

	//~ 초기화
	void BindCallbacks();

	/**
	 * 슬라이더의 최소·최대를 코드가 못박는다. WBP 로 만들든 임시 레이아웃이든 똑같이 적용된다.
	 *
	 * 범위는 취향이 아니라 의미다 — 감마는 1~4, 시야각은 60~120 도다. UMG 슬라이더 기본값이
	 * 0~1 이라 이걸 안 걸면 WBP 에 슬라이더를 놓은 순간 감마가 0~1 로 잘려서, 화면이 새까매지는데
	 * 원인이 안 보이는 상태가 된다. 디자이너가 매번 손으로 맞추게 두면 안 되는 종류의 값이다.
	 */
	void ApplySliderRanges();
	void PopulateStaticCombos();
	void PopulateResolutionCombo();
	void PopulateFromSettings();
	void UpdateTabButtonColors();

	//~ 값 라벨
	void SetPercentLabel(UTextBlock* Label, float Value01) const;
	void SetNumberLabel(UTextBlock* Label, float Value, int32 Decimals) const;

	//~ 오디오 콜백
	UFUNCTION() void HandleMasterChanged(float V);
	UFUNCTION() void HandleMusicChanged(float V);
	UFUNCTION() void HandleSFXChanged(float V);
	UFUNCTION() void HandleUIChanged(float V);
	UFUNCTION() void HandleMuteChanged(bool bChecked);

	//~ 보이스 콜백
	UFUNCTION() void HandleVoiceOutputChanged(float V);
	UFUNCTION() void HandleVoiceInputChanged(float V);
	UFUNCTION() void HandleVoiceGateChanged(float V);
	UFUNCTION() void HandleVoiceMuteChanged(bool bChecked);

	//~ 화면 콜백
	UFUNCTION() void HandleResolutionSelected(FString Item, ESelectInfo::Type Type);
	UFUNCTION() void HandleWindowModeSelected(FString Item, ESelectInfo::Type Type);
	UFUNCTION() void HandleQualityPresetSelected(FString Item, ESelectInfo::Type Type);
	UFUNCTION() void HandleFrameLimitSelected(FString Item, ESelectInfo::Type Type);
	UFUNCTION() void HandleLanguageSelected(FString Item, ESelectInfo::Type Type);
	UFUNCTION() void HandleColorblindSelected(FString Item, ESelectInfo::Type Type);
	UFUNCTION() void HandleVSyncChanged(bool bChecked);
	UFUNCTION() void HandleSubtitlesChanged(bool bChecked);
	UFUNCTION() void HandleGammaChanged(float V);
	UFUNCTION() void HandleFOVChanged(float V);
	UFUNCTION() void HandleSubtitleScaleChanged(float V);
	UFUNCTION() void HandleShadowQualityChanged(float V);
	UFUNCTION() void HandleTextureQualityChanged(float V);
	UFUNCTION() void HandleEffectsQualityChanged(float V);
	UFUNCTION() void HandleViewDistanceChanged(float V);
	UFUNCTION() void HandleAntiAliasingChanged(float V);
	UFUNCTION() void HandlePostProcessChanged(float V);

	//~ 조작 콜백
	UFUNCTION() void HandleMouseSensitivityChanged(float V);
	UFUNCTION() void HandleInvertYChanged(bool bChecked);
	UFUNCTION() void HandleResetKeybindsClicked();

	//~ 탭 버튼
	UFUNCTION() void ShowAudioTab();
	UFUNCTION() void ShowVoiceTab();
	UFUNCTION() void ShowVideoTab();
	UFUNCTION() void ShowControlsTab();

	//~ 하단 바
	UFUNCTION() void HandleApplyClicked();
	UFUNCTION() void HandleCancelClicked();
	UFUNCTION() void HandleResetTabClicked();

	//~ 미저장 팝업
	UFUNCTION() void HandleUnsavedSave();
	UFUNCTION() void HandleUnsavedDiscard();
	UFUNCTION() void HandleUnsavedCancel();
	void ShowUnsavedPopup(bool bShow);

	//~ 해상도 되돌림
	UFUNCTION() void HandleRevertKeep();
	UFUNCTION() void HandleRevertBack();
	void StartResolutionRevertCountdown();
	void TickRevertCountdown();
	void FinishRevert(bool bKeep);

	//~ 공통
	UFUNCTION() void PlayHoverSound();
	void PlayClickSound();
	void ApplyAll();
	void CloseInternal();

	/** 세부 화질을 건드렸을 때 프리셋 콤보를 "커스텀"으로. */
	void SyncPresetComboToCustom();

	/** 임시 레이아웃일 때 키 목록을 C++ 이 직접 채운다. */
	void RebuildFallbackKeybindList();

	/** 키 목록 갱신 — 임시 레이아웃이면 직접, WBP 면 RefreshKeybindList 이벤트로. */
	void RequestKeybindListRefresh();

	//~ ---------------- 임시 레이아웃 ----------------

	void BuildFallbackLayout();
	UVerticalBox* MakeFallbackTab(UWidgetSwitcher* Switcher, const FText& Title);
	USlider* AddFallbackSlider(UVerticalBox* Column, const FText& Label, float Min, float Max, TObjectPtr<UTextBlock>& OutValueLabel);
	UCheckBox* AddFallbackCheckBox(UVerticalBox* Column, const FText& Label);
	UComboBoxString* AddFallbackCombo(UVerticalBox* Column, const FText& Label);
	UButton* MakeFallbackButton(class UPanelWidget* Parent, const FText& Label);
	UTextBlock* MakeFallbackText(const FText& Text, int32 FontSize, const FLinearColor& Color) const;

	/** 임시 레이아웃으로 만들어진 창인가. 키 목록을 누가 그릴지 이걸로 가른다. */
	bool bUsingFallbackLayout = false;

	/** 임시 키 목록의 행 조각들. GC 에 안 쓸려가게 붙들어 둔다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USKKeybindRowProxy>> KeybindProxies;

	//~ ---------------- 상태 ----------------

	ESKSettingsTab CurrentTab = ESKSettingsTab::Audio;

	/** 콤보를 코드로 채우는 동안 콜백이 값을 되받아 쓰는 것을 막는다. */
	bool bPopulating = false;

	bool bAwaitingKey = false;
	FName PendingMappingName;

	/** 되돌림 기준 해상도/모드. 창을 열 때와 적용할 때 갱신된다. */
	FIntPoint PrevResolution = FIntPoint(0, 0);
	int32 PrevWindowMode = 0;
	int32 RevertSecondsLeft = 0;
	FTimerHandle RevertTimerHandle;

	FTimerHandle CloseTimerHandle;
	bool bClosing = false;
};
