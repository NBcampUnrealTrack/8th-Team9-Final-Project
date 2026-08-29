// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DSP/EnvelopeFollower.h"
#include "HAL/CriticalSection.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Templates/UniquePtr.h"
#include "FAVoiceCalibration.generated.h"

namespace Audio { class FAudioCapture; }

/** 캘리브레이션 결과 상태. */
UENUM(BlueprintType)
enum class EFAVoiceCalibrationStatus : uint8
{
	/** 측정에 성공했고 게이트 값을 적용했다. */
	Success,

	/** 노이즈 바닥이 너무 높아 슬라이더 상한(0.08)으로 잘렸다. 설정만으로는 못 잡는다. */
	ClampedTooNoisy,

	/** 마이크를 열지 못했다. 윈도우 입력 장치가 없거나 다른 앱이 독점 중. */
	NoCaptureDevice,

	/**
	 * 장치는 열렸는데 신호가 완전한 무음이었다.
	 * 윈도우에서 마이크가 음소거돼 있거나, 입력 볼륨 0 이거나, 엉뚱한 장치가 기본으로 잡힌 경우다.
	 */
	MicSilent,

	/** 측정 내내 말소리 수준이었다. 조용히 하고 다시 해야 한다. */
	TooLoud,

	/** 이미 측정 중. */
	AlreadyRunning
};

/** 캘리브레이션 한 번의 결과. */
USTRUCT(BlueprintType)
struct FALLINGAPART_API FFAVoiceCalibrationReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Voice")
	EFAVoiceCalibrationStatus Status = EFAVoiceCalibrationStatus::NoCaptureDevice;

	/** 측정한 노이즈 바닥(정규화 진폭 0~1). 게이트 임계와 같은 단위다. */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Voice")
	float NoiseFloor = 0.f;

	/** 실제로 넣은 voice.MicNoiseGateThreshold 값. */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Voice")
	float AppliedGate = 0.f;

	/** 실제로 넣은 설정 슬라이더 값(0~1). */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Voice")
	float AppliedSliderValue = 0.5f;

	/** 측정에 쓴 입력 장치 이름. "내 마이크가 이게 맞나" 를 확인시켜 주는 용도. */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Voice")
	FString DeviceName;

	/** 사람에게 보여줄 한 줄. 위젯이 그대로 띄우면 된다. */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Voice")
	FText Message;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFAOnVoiceCalibrationProgress, float, Alpha, float, CurrentLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFAOnVoiceCalibrationFinished, const FFAVoiceCalibrationReport&, Report);

/**
 * 마이크 자동 조정.
 */
UCLASS()
class FALLINGAPART_API UFAVoiceCalibration : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFAVoiceCalibration();
	UFAVoiceCalibration(FVTableHelper& Helper);

	virtual ~UFAVoiceCalibration() override;

	UFUNCTION(BlueprintPure, Category = "FallingApart|Voice", meta = (WorldContext = "WorldContextObject"))
	static UFAVoiceCalibration* Get(const UObject* WorldContextObject);

	/**
	 * 측정을 시작한다. 끝나면 OnCalibrationFinished 가 온다.
	 *
	 * @param DurationSeconds 조용히 있어야 하는 시간. 3초면 충분하고, 너무 짧으면 팬 소음의 굴곡을 놓친다.
	 * @param bSaveImmediately 결과를 ini 까지 바로 굳힐지.
	 *        설정창 안에서는 false 로 둔다
	 */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Voice")
	bool StartCalibration(float DurationSeconds = 3.f, bool bSaveImmediately = false);

	/** 진행 중인 측정을 버린다. 설정은 건드리지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Voice")
	void CancelCalibration();

	UFUNCTION(BlueprintPure, Category = "FallingApart|Voice")
	bool IsCalibrating() const { return bIsCalibrating; }

	/** 지금 마이크 진폭(0~1). 측정 중이 아니면 0. 레벨 미터용. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Voice")
	float GetCurrentLevel() const;

	/** 마지막 결과. 창을 다시 열었을 때 그대로 보여줄 수 있게 남겨 둔다. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Voice")
	const FFAVoiceCalibrationReport& GetLastReport() const { return LastReport; }

	UPROPERTY(BlueprintAssignable, Category = "FallingApart|Voice")
	FFAOnVoiceCalibrationProgress OnCalibrationProgress;

	UPROPERTY(BlueprintAssignable, Category = "FallingApart|Voice")
	FFAOnVoiceCalibrationFinished OnCalibrationFinished;

protected:
	virtual void Deinitialize() override;

private:
	/** 캡처 스레드에서 불린다. 게임 스레드가 아니다. */
	void HandleCapturedAudio(const float* InAudio, int32 NumFrames, int32 NumChannels, int32 SampleRate);

	void TickProgress();
	void FinishCalibration();

	/** 스트림을 닫고 상태를 되돌린다. 결과 보고는 하지 않는다. */
	void TeardownStream();

	void ReportFailure(EFAVoiceCalibrationStatus Status, const FText& Message);

	TUniquePtr<Audio::FAudioCapture> Capture;

	/** 캡처 스레드와 게임 스레드가 같이 만지는 것 전부를 덮는다. */
	mutable FCriticalSection CaptureLock;

	/** 엔진 게이트와 같은 설정으로 초기화한다. 잠금 안에서만 만진다. */
	Audio::FInlineEnvelopeFollower LevelDetector;

	/** LevelDetector 를 초기화할 때 쓴 샘플레이트. 장치가 바뀌면 다시 잡는다. */
	float DetectorSampleRate = 0.f;

	/** 콜백 한 덩어리마다의 봉투 최댓값. 마지막에 퍼센타일을 뽑는다. */
	TArray<float> BlockPeaks;

	float LatestLevel = 0.f;

	bool bIsCalibrating = false;

	/** 이번 측정 결과를 ini 까지 굳힐지. StartCalibration 인자로 정해진다. */
	bool bSaveResultImmediately = false;

	double CalibrationStartTime = 0.0;
	float CalibrationDuration = 3.f;

	FString CaptureDeviceName;

	FFAVoiceCalibrationReport LastReport;

	FTimerHandle FinishTimerHandle;
	FTimerHandle ProgressTimerHandle;
};
