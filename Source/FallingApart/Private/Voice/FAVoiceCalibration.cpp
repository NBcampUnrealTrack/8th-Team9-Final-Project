// Fill out your copyright notice in the Description page of Project Settings.

#include "Voice/FAVoiceCalibration.h"

#include "AudioCaptureCore.h"
#include "Engine/GameInstance.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Modules/ModuleManager.h"
#include "Net/VoiceConfig.h"
#include "Settings/FASettingsBridge.h"
#include "SettingsKitSubsystem.h"
#include "TimerManager.h"
#include "Voice/FAVoiceStatics.h"
#include "Voice/FAVoiceTypes.h"

#define LOCTEXT_NAMESPACE "FAVoiceCalibration"

namespace FAVoiceCalibrationInternal
{
	/** 진행률·레벨 미터 갱신 주기(초). */
	static constexpr float ProgressInterval = 0.05f;

	/** 콜백 한 번에 받을 프레임 수. 엔진의 FAudioCaptureSynth 와 같은 값. */
	static constexpr uint32 CaptureFramesDesired = 1024;

	static constexpr float NoiseMargin = 1.6f;

	/**
	 * 봉투 표본에서 노이즈 바닥으로 삼을 퍼센타일.
	 */
	static constexpr float NoiseFloorPercentile = 0.95f;

	/**
	 * 이 위면 "조용히 있지 않았다" 로 본다.
	 */
	static constexpr float SpeechLevelGuard = 0.15f;

	/** 표본이 이보다 적으면 측정이 성립하지 않는다(마이크가 데이터를 안 준 것). */
	static constexpr int32 MinBlockSamples = 8;

	/**
	 * 이 아래는 "신호가 아예 없다" 로 본다(≈ -100dBFS).
	 */
	static constexpr float SilenceGuard = 1.e-5f;

	/** 진폭을 사람이 읽을 수 있게. 0 은 그대로 두면 -inf 라 바닥을 깐다. */
	static FText FormatLevel(float Level)
	{
		if (Level <= SilenceGuard)
		{
			return NSLOCTEXT("FAVoiceCalibration", "LevelSilent", "무음");
		}

		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = 0;
		Options.MaximumFractionalDigits = 0;

		return FText::Format(NSLOCTEXT("FAVoiceCalibration", "LevelDb", "{0}dB"),
			FText::AsNumber(20.f * FMath::LogX(10.f, Level), &Options));
	}

	static void SetCVarFloat(const TCHAR* Name, float Value)
	{
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			CVar->Set(Value, ECVF_SetByGameSetting);
		}
	}
}

// 세 생성자/소멸자 모두 여기 있어야 한다 —
// Audio::FAudioCapture 는 헤더에서 불완전 타입이라 TUniquePtr 소멸자를 이 파일에서 인스턴스화해야 한다.
UFAVoiceCalibration::UFAVoiceCalibration() = default;
UFAVoiceCalibration::UFAVoiceCalibration(FVTableHelper& Helper)
	: Super(Helper)
{
}
UFAVoiceCalibration::~UFAVoiceCalibration() = default;

UFAVoiceCalibration* UFAVoiceCalibration::Get(const UObject* WorldContextObject)
{
	const UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);
	return GameInstance != nullptr ? GameInstance->GetSubsystem<UFAVoiceCalibration>() : nullptr;
}

void UFAVoiceCalibration::Deinitialize()
{
	CancelCalibration();

	Super::Deinitialize();
}

float UFAVoiceCalibration::GetCurrentLevel() const
{
	FScopeLock Lock(&CaptureLock);
	return LatestLevel;
}

bool UFAVoiceCalibration::StartCalibration(float DurationSeconds, bool bSaveImmediately)
{
	if (bIsCalibrating)
	{
		ReportFailure(EFAVoiceCalibrationStatus::AlreadyRunning,
			LOCTEXT("AlreadyRunning", "이미 마이크를 측정하고 있습니다."));
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance == nullptr)
	{
		ReportFailure(EFAVoiceCalibrationStatus::NoCaptureDevice,
			LOCTEXT("NoGameInstance", "아직 게임이 준비되지 않았습니다."));
		return false;
	}

	// 캡처 구현(AudioCaptureRtAudio/Wasapi)은 AudioCapture 플러그인 모듈이 올라올 때 등록된다.
	// 기본 활성 플러그인이라 보통 이미 올라와 있지만, 안 올라와 있으면 스트림이 조용히 실패한다.
	FModuleManager::Get().LoadModule(TEXT("AudioCapture"));

	Capture = MakeUnique<Audio::FAudioCapture>();

	Audio::FCaptureDeviceInfo DeviceInfo;
	if (!Capture->GetCaptureDeviceInfo(DeviceInfo))
	{
		Capture.Reset();
		ReportFailure(EFAVoiceCalibrationStatus::NoCaptureDevice,
			LOCTEXT("NoDevice", "마이크를 찾지 못했습니다. 윈도우 소리 설정에서 입력 장치를 확인해 주세요."));
		return false;
	}

	CaptureDeviceName = DeviceInfo.DeviceName;

	{
		FScopeLock Lock(&CaptureLock);
		BlockPeaks.Reset();
		// 3초 × 48kHz / 1024프레임 ≈ 140 덩어리. 콜백에서 재할당이 나지 않게 미리 잡는다.
		BlockPeaks.Reserve(256);
		LatestLevel = 0.f;
		DetectorSampleRate = 0.f;
	}

	Audio::FAudioCaptureDeviceParams Params;

	// 설정창에서 고른 장치를 재야 한다 — 기본 장치를 재면 게이트가 엉뚱한 마이크에 맞춰진다.
	// 이 스트림(WASAPI)과 보이스 캡처(DirectSound)는 장치 목록이 달라 이름으로 짝을 찾는다.
	// 대개 같은 이름이지만 한쪽이 잘려 있을 수 있어 앞부분 일치도 받아 준다.
	const FString SelectedDevice = UFAVoiceStatics::GetCaptureDevice();
	if (!SelectedDevice.IsEmpty())
	{
		TArray<Audio::FCaptureDeviceInfo> Devices;
		Capture->GetCaptureDevicesAvailable(Devices);

		int32 MatchIndex = Devices.IndexOfByPredicate([&SelectedDevice](const Audio::FCaptureDeviceInfo& Info)
		{
			return Info.DeviceName == SelectedDevice;
		});
		if (MatchIndex == INDEX_NONE)
		{
			MatchIndex = Devices.IndexOfByPredicate([&SelectedDevice](const Audio::FCaptureDeviceInfo& Info)
			{
				return Info.DeviceName.StartsWith(SelectedDevice) || SelectedDevice.StartsWith(Info.DeviceName);
			});
		}

		if (MatchIndex != INDEX_NONE)
		{
			Params.DeviceIndex = MatchIndex;
			CaptureDeviceName = Devices[MatchIndex].DeviceName;
		}
		else
		{
			// 결과 문구에 장치 이름이 찍히므로 사용자도 기본 장치로 쟀다는 걸 알 수 있다.
			UE_LOG(LogFAVoice, Warning, TEXT("[보이스] 마이크 측정: 고른 장치 '%s' 를 못 찾아 기본 장치 '%s' 로 잰다."),
				*SelectedDevice, *CaptureDeviceName);
		}
	}

	const bool bOpened = Capture->OpenAudioCaptureStream(Params,
		[this](const void* InAudio, int32 NumFrames, int32 NumChannels, int32 SampleRate, double, bool)
		{
			HandleCapturedAudio(static_cast<const float*>(InAudio), NumFrames, NumChannels, SampleRate);
		},
		FAVoiceCalibrationInternal::CaptureFramesDesired);

	if (!bOpened || !Capture->StartStream())
	{
		TeardownStream();
		ReportFailure(EFAVoiceCalibrationStatus::NoCaptureDevice,
			LOCTEXT("StreamFailed", "마이크를 열지 못했습니다. 다른 프로그램이 독점하고 있는지 확인해 주세요."));
		return false;
	}

	bIsCalibrating = true;
	bSaveResultImmediately = bSaveImmediately;
	CalibrationDuration = FMath::Max(1.f, DurationSeconds);
	CalibrationStartTime = FPlatformTime::Seconds();

	FTimerManager& TimerManager = GameInstance->GetTimerManager();
	TimerManager.SetTimer(FinishTimerHandle, this, &UFAVoiceCalibration::FinishCalibration, CalibrationDuration, false);
	TimerManager.SetTimer(ProgressTimerHandle, this, &UFAVoiceCalibration::TickProgress,
		FAVoiceCalibrationInternal::ProgressInterval, true);

	UE_LOG(LogFAVoice, Log, TEXT("[보이스] 마이크 측정 시작 — 장치 '%s', %.1f초"), *CaptureDeviceName, CalibrationDuration);

	return true;
}

void UFAVoiceCalibration::HandleCapturedAudio(const float* InAudio, int32 NumFrames, int32 NumChannels, int32 SampleRate)
{
	if (InAudio == nullptr || NumFrames <= 0 || NumChannels <= 0 || SampleRate <= 0)
	{
		return;
	}

	FScopeLock Lock(&CaptureLock);

	if (!bIsCalibrating)
	{
		return;
	}

	// 장치 샘플레이트는 콜백에 와서야 확실히 안다. 바뀌면 검출기를 다시 잡는다.
	if (!FMath::IsNearlyEqual(DetectorSampleRate, static_cast<float>(SampleRate)))
	{
		const IConsoleVariable* AttackCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("voice.SilenceDetectionAttackTime"));
		const IConsoleVariable* ReleaseCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("voice.SilenceDetectionReleaseTime"));

		// 엔진 캡처 경로와 똑같은 설정이어야 잰 숫자로 게이트를 맞출 수 있다
		// (VoiceCaptureWindows.cpp:121~133).
		Audio::FInlineEnvelopeFollowerInitParams InitParams;
		InitParams.SampleRate = static_cast<float>(SampleRate);
		InitParams.AttackTimeMsec = AttackCVar != nullptr ? AttackCVar->GetFloat() : 5.f;
		InitParams.ReleaseTimeMsec = ReleaseCVar != nullptr ? ReleaseCVar->GetFloat() : 350.f;
		InitParams.Mode = MicSilenceDetectionConfig::LevelDetectionMode;
		InitParams.bIsAnalog = MicSilenceDetectionConfig::IsAnalog;

		LevelDetector.Init(InitParams);
		LevelDetector.Reset();

		DetectorSampleRate = static_cast<float>(SampleRate);
	}

	float BlockPeak = 0.f;
	for (int32 Frame = 0; Frame < NumFrames; ++Frame)
	{
		// 보이스 캡처는 모노다. 스테레오 장치여도 첫 채널만 봐야 조건이 같아진다.
		const float Envelope = LevelDetector.ProcessSample(InAudio[Frame * NumChannels]);
		BlockPeak = FMath::Max(BlockPeak, Envelope);
	}

	BlockPeaks.Add(BlockPeak);
	LatestLevel = BlockPeak;
}

void UFAVoiceCalibration::TickProgress()
{
	if (!bIsCalibrating)
	{
		return;
	}

	const float Elapsed = static_cast<float>(FPlatformTime::Seconds() - CalibrationStartTime);
	const float Alpha = FMath::Clamp(Elapsed / CalibrationDuration, 0.f, 1.f);

	OnCalibrationProgress.Broadcast(Alpha, GetCurrentLevel());
}

void UFAVoiceCalibration::FinishCalibration()
{
	if (!bIsCalibrating)
	{
		return;
	}

	TArray<float> Peaks;
	{
		FScopeLock Lock(&CaptureLock);
		Peaks = MoveTemp(BlockPeaks);
		BlockPeaks.Reset();
	}

	TeardownStream();

	if (Peaks.Num() < FAVoiceCalibrationInternal::MinBlockSamples)
	{
		ReportFailure(EFAVoiceCalibrationStatus::NoCaptureDevice,
			LOCTEXT("NoSamples", "마이크에서 소리가 들어오지 않았습니다. 윈도우 소리 설정에서 입력 장치와 음소거를 확인해 주세요."));
		return;
	}

	Peaks.Sort();

	const int32 PercentileIndex = FMath::Clamp(
		FMath::FloorToInt32(Peaks.Num() * FAVoiceCalibrationInternal::NoiseFloorPercentile),
		0, Peaks.Num() - 1);
	const float NoiseFloor = Peaks[PercentileIndex];

	if (NoiseFloor <= FAVoiceCalibrationInternal::SilenceGuard)
	{
		// 설정은 건드리지 않는다. 여기서 게이트를 최소로 내려 두면 "조정했다" 는 착각만 남는다.
		LastReport = FFAVoiceCalibrationReport();
		LastReport.Status = EFAVoiceCalibrationStatus::MicSilent;
		LastReport.NoiseFloor = NoiseFloor;
		LastReport.DeviceName = CaptureDeviceName;
		LastReport.Message = FText::Format(
			LOCTEXT("MicSilent",
				"'{0}' 에서 소리가 전혀 들어오지 않습니다. 윈도우 소리 설정에서 이 마이크가 음소거는 아닌지, 입력 볼륨이 0 은 아닌지, 쓰려는 마이크가 기본 장치로 잡혀 있는지 확인해 주세요."),
			FText::FromString(CaptureDeviceName));

		UE_LOG(LogFAVoice, Warning,
			TEXT("[보이스] 마이크 측정 실패 — 장치 '%s' 가 완전한 무음이다(바닥 %.6f). 음소거·입력 볼륨·기본 장치를 확인할 것."),
			*CaptureDeviceName, NoiseFloor);

		OnCalibrationFinished.Broadcast(LastReport);
		return;
	}

	if (NoiseFloor > FAVoiceCalibrationInternal::SpeechLevelGuard)
	{
		LastReport = FFAVoiceCalibrationReport();
		LastReport.Status = EFAVoiceCalibrationStatus::TooLoud;
		LastReport.NoiseFloor = NoiseFloor;
		LastReport.DeviceName = CaptureDeviceName;
		LastReport.Message = LOCTEXT("TooLoud",
			"측정하는 동안 소리가 너무 컸습니다. 말하지 말고 다시 시도해 주세요. (스피커 소리가 마이크로 들어가는 경우도 있습니다)");

		UE_LOG(LogFAVoice, Warning, TEXT("[보이스] 마이크 측정 실패 — 노이즈 바닥 %.6f 가 말소리 수준이다."), NoiseFloor);

		OnCalibrationFinished.Broadcast(LastReport);
		return;
	}

	// 노이즈 바닥 위에 여유를 두고 게이트를 놓는다. 슬라이더로 표현 가능한 범위를 넘으면 잘린다.
	const float DesiredGate = NoiseFloor * FAVoiceCalibrationInternal::NoiseMargin;
	const float AppliedGate = FMath::Clamp(DesiredGate, FASettingsMapping::GateMin, FASettingsMapping::GateMax);
	const bool bClamped = DesiredGate > FASettingsMapping::GateMax;

	const float SliderValue = FASettingsMapping::FromNoiseGate(AppliedGate);

	LastReport = FFAVoiceCalibrationReport();
	LastReport.NoiseFloor = NoiseFloor;
	LastReport.AppliedGate = AppliedGate;
	LastReport.AppliedSliderValue = SliderValue;
	LastReport.DeviceName = CaptureDeviceName;

	// 설정에 써 두면 저장·재적용까지 플러그인이 해 준다.
	// (USKSettingsSubsystem 이 변경을 알리면 UFASettingsBridge::ApplyVoice 가 CVar 로 민다)
	if (USKSettingsSubsystem* Settings = USKSettingsSubsystem::Get(this))
	{
		Settings->SetVoiceGateThreshold(SliderValue);

		// 설정창 안에서는 굳히지 않는다 — 슬라이더와 똑같이 '적용' 으로 확정되고 '취소' 로 돌아가야 한다.
		if (bSaveResultImmediately)
		{
			Settings->ApplyAndSave();
		}
	}
	else
	{
		// 설정 서브시스템이 없으면(테스트 맵 등) 최소한 이번 판에는 먹게 직접 민다.
		FAVoiceCalibrationInternal::SetCVarFloat(TEXT("voice.MicNoiseGateThreshold"), AppliedGate);
		UVOIPStatics::SetMicThreshold(FASettingsMapping::ToSilenceThreshold(AppliedGate));
	}

	if (bClamped)
	{
		LastReport.Status = EFAVoiceCalibrationStatus::ClampedTooNoisy;
		LastReport.Message = FText::Format(
			LOCTEXT("ClampedTooNoisy",
				"마이크 잡음이 설정으로 잡을 수 있는 한계보다 큽니다({0}). 게이트를 최대로 올려 두었지만, 윈도우 소리 설정에서 입력 장치가 '{1}' 이 맞는지 확인해 주세요."),
			FAVoiceCalibrationInternal::FormatLevel(NoiseFloor), FText::FromString(CaptureDeviceName));

		UE_LOG(LogFAVoice, Warning,
			TEXT("[보이스] 마이크 측정 — 노이즈 바닥 %.6f (장치 '%s'). 필요한 게이트 %.6f 가 상한 %.6f 를 넘어 잘렸다."),
			NoiseFloor, *CaptureDeviceName, DesiredGate, FASettingsMapping::GateMax);
	}
	else
	{
		LastReport.Status = EFAVoiceCalibrationStatus::Success;
		LastReport.Message = FText::Format(
			LOCTEXT("Success", "마이크 조정 완료. 잡음 수준 {0} 에 맞춰 감도를 잡았습니다. ({1})"),
			FAVoiceCalibrationInternal::FormatLevel(NoiseFloor), FText::FromString(CaptureDeviceName));

		UE_LOG(LogFAVoice, Log,
			TEXT("[보이스] 마이크 측정 완료 — 노이즈 바닥 %.6f, 게이트 %.6f, 슬라이더 %.2f (장치 '%s')"),
			NoiseFloor, AppliedGate, SliderValue, *CaptureDeviceName);
	}

	OnCalibrationFinished.Broadcast(LastReport);
}

void UFAVoiceCalibration::CancelCalibration()
{
	if (!bIsCalibrating)
	{
		return;
	}

	TeardownStream();

	UE_LOG(LogFAVoice, Log, TEXT("[보이스] 마이크 측정을 취소했다."));
}

void UFAVoiceCalibration::TeardownStream()
{
	{
		FScopeLock Lock(&CaptureLock);
		bIsCalibrating = false;
		LatestLevel = 0.f;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		FTimerManager& TimerManager = GameInstance->GetTimerManager();
		TimerManager.ClearTimer(FinishTimerHandle);
		TimerManager.ClearTimer(ProgressTimerHandle);
	}

	if (Capture.IsValid())
	{
		// StopStream 은 콜백이 끝날 때까지 기다려 준다. 그 뒤에야 버퍼를 만져도 안전하다.
		if (Capture->IsCapturing())
		{
			Capture->StopStream();
		}

		if (Capture->IsStreamOpen())
		{
			Capture->CloseStream();
		}

		Capture.Reset();
	}
}

void UFAVoiceCalibration::ReportFailure(EFAVoiceCalibrationStatus Status, const FText& Message)
{
	LastReport = FFAVoiceCalibrationReport();
	LastReport.Status = Status;
	LastReport.DeviceName = CaptureDeviceName;
	LastReport.Message = Message;

	UE_LOG(LogFAVoice, Warning, TEXT("[보이스] 마이크 측정 실패 — %s"), *Message.ToString());

	OnCalibrationFinished.Broadcast(LastReport);
}

// ============================================================
// 콘솔 명령 — 설정창 없이도 바로 재 볼 수 있게
// ============================================================
namespace FAVoiceCalibrationDebug
{
	static void CalibrateMic(const TArray<FString>& Args, UWorld* World)
	{
		UFAVoiceCalibration* Calibration = UFAVoiceCalibration::Get(World);
		if (Calibration == nullptr)
		{
			UE_LOG(LogFAVoice, Warning, TEXT("FAVoice.Calibrate: 아직 게임 인스턴스가 없다."));
			return;
		}

		const float Duration = Args.Num() >= 1 ? FCString::Atof(*Args[0]) : 3.f;

		UE_LOG(LogFAVoice, Display, TEXT("FAVoice.Calibrate: %.1f초간 조용히 있을 것. 결과는 로그에 찍힌다."), Duration);

		// 콘솔로 부르는 쪽에는 확정해 줄 '적용' 버튼이 없다. 바로 굳힌다.
		Calibration->StartCalibration(Duration, /*bSaveImmediately=*/true);
	}

	static FAutoConsoleCommandWithWorldAndArgs CalibrateCommand(
		TEXT("FAVoice.Calibrate"),
		TEXT("FAVoice.Calibrate [초] — 마이크 잡음을 재서 게이트를 자동으로 맞춘다. 재는 동안 말하지 말 것."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CalibrateMic));
}

#undef LOCTEXT_NAMESPACE
