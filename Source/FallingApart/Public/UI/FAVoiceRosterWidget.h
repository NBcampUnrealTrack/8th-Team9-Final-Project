// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FAVoiceRosterWidget.generated.h"

class UButton;
class UCheckBox;
class UPanelWidget;
class USlider;
class UTextBlock;
class UVerticalBox;

/**
 * 접속자 한 명의 음량 한 줄 — 이름 · 슬라이더 · 값 · 뮤트.
 */
UCLASS()
class FALLINGAPART_API UFAVoiceRosterEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 이 줄이 누구의 것인지 정한다. 목록이 다시 그려질 때마다 불린다. */
	void Setup(const FString& InPlayerKey, const FText& InPlayerName);

	/** 저장된 값을 다시 읽어 위젯에 반영. */
	void RefreshFromSettings();

	const FString& GetPlayerKey() const { return PlayerKey; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Voice")
	TObjectPtr<UTextBlock> Text_PlayerName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Voice")
	TObjectPtr<USlider> Slider_Volume;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Voice")
	TObjectPtr<UTextBlock> Text_VolumeValue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Voice")
	TObjectPtr<UCheckBox> Check_Mute;

private:
	UFUNCTION()
	void HandleVolumeChanged(float NewValue);

	UFUNCTION()
	void HandleMuteChanged(bool bChecked);

	void BuildFallbackRow();

	/** UniqueNetId 기준 저장 키. 이름이 아니다 — 이름은 중복·변경된다. */
	FString PlayerKey;

	/** 위젯에 값을 채워 넣는 동안 콜백이 되돌아와 다시 저장하는 것을 막는다. */
	bool bUpdatingWidgets = false;
};

/**
 * 설정창 보이스 탭에 들어가는 "접속자별 음량" 목록.
 */
UCLASS()
class FALLINGAPART_API UFAVoiceRosterWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 지금 접속해 있는 사람들로 목록을 다시 만든다. */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Voice")
	void RefreshRoster();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** 줄 하나에 쓸 위젯. 비우면 C++ 임시 레이아웃(UFAVoiceRosterEntryWidget)이 쓰인다. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Voice")
	TSubclassOf<UFAVoiceRosterEntryWidget> EntryWidgetClass;

	/** 목록이 들어갈 상자. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Voice")
	TObjectPtr<UPanelWidget> Box_Players;

	/** 다른 사람이 아무도 없을 때 대신 보여줄 문구. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Voice")
	TObjectPtr<UTextBlock> Text_Empty;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Voice")
	TObjectPtr<UButton> Btn_ResetAll;

	/** 접속·퇴장을 따라가는 갱신 주기(초). 델리게이트가 없는 값이라 폴링한다. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Voice", meta = (ClampMin = "0.2"))
	float RefreshInterval = 1.f;

private:
	UFUNCTION()
	void HandleResetAllClicked();

	void HandlePlayerVoiceSettingsChanged(const FString& PlayerKey, float NewScale);

	/** 지금 화면에 있어야 할 (키, 이름) 목록. 나 자신은 빼고, UniqueNetId 가 온 사람만. */
	void CollectPlayers(TArray<TPair<FString, FText>>& OutPlayers) const;

	void BuildFallbackLayout();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFAVoiceRosterEntryWidget>> EntryWidgets;

	/** 마지막으로 그린 키 목록. 바뀐 게 없으면 다시 만들지 않는다(슬라이더 잡고 있는 중일 수 있다). */
	TArray<FString> DisplayedKeys;

	FTimerHandle RefreshTimerHandle;

	FDelegateHandle VoiceSettingsChangedHandle;
};
