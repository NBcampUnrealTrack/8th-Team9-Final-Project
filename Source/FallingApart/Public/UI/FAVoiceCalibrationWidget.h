// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Voice/FAVoiceCalibration.h"
#include "FAVoiceCalibrationWidget.generated.h"

class UButton;
class UProgressBar;
class UTextBlock;

/**
 * 설정창 보이스 탭의 "마이크 자동 조정".
 */
UCLASS()
class FALLINGAPART_API UFAVoiceCalibrationWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Voice")
	TObjectPtr<UButton> Btn_Calibrate;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Voice")
	TObjectPtr<UTextBlock> Text_ButtonLabel;

	/** 안내문 겸 결과 표시. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Voice")
	TObjectPtr<UTextBlock> Text_Status;

	/** 지금 마이크에 들어오는 소리 크기. 측정 중이 아니어도 "내 마이크가 살아 있나" 확인용으로 쓸 수 있다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Voice")
	TObjectPtr<UProgressBar> Bar_Level;

	/** 조용히 있어야 하는 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Voice", meta = (ClampMin = "1.0"))
	float CalibrationSeconds = 3.f;

private:
	UFUNCTION()
	void HandleCalibrateClicked();

	UFUNCTION()
	void HandleProgress(float Alpha, float CurrentLevel);

	UFUNCTION()
	void HandleFinished(const FFAVoiceCalibrationReport& Report);

	void BuildFallbackLayout();

	/** 버튼/안내문을 지금 상태에 맞춘다. */
	void RefreshForState(bool bRunning);

	/**
	 * 진폭(0~1) 을 미터 눈금으로 편다.
	 * 노이즈 바닥은 보통 0.01 언저리라 선형으로 그리면 눈에 보이지 않는다 — dB 로 편다.
	 */
	static float ToMeterAlpha(float Level);
};
