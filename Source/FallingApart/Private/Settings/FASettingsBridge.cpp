// Fill out your copyright notice in the Description page of Project Settings.

#include "Settings/FASettingsBridge.h"

#include "Audio/FAAudioSettings.h"
#include "Audio/FAAudioSubsystem.h"
#include "AudioDeviceManager.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "SettingsKitSubsystem.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "UObject/UObjectGlobals.h"
#include "Voice/FAVoiceStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogFASettings, Log, All);

namespace FASettingsMapping
{
	/**
	 * 슬라이더 0~1 을 실제 값으로 편다.
	 * 선언은 FASettingsBridge.h — 자동 캘리브레이션이 역방향으로도 쓴다.
	 */
	float MapWithMidpoint(float T, float Min, float Mid, float Max)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		return T <= 0.5f
			? FMath::Lerp(Min, Mid, T * 2.f)
			: FMath::Lerp(Mid, Max, (T - 0.5f) * 2.f);
	}

	float UnmapWithMidpoint(float Value, float Min, float Mid, float Max)
	{
		// 접힌 두 구간의 기울기가 다르므로 어느 쪽인지 먼저 가른다.
		if (Value <= Mid)
		{
			return FMath::IsNearlyEqual(Mid, Min)
				? 0.f
				: FMath::Clamp((Value - Min) / (Mid - Min) * 0.5f, 0.f, 0.5f);
		}

		return FMath::IsNearlyEqual(Max, Mid)
			? 1.f
			: FMath::Clamp(0.5f + (Value - Mid) / (Max - Mid) * 0.5f, 0.5f, 1.f);
	}

	//~ 아래 숫자들은 Config/DefaultEngine.ini 의 튜닝 결과
	//~ (게이트 환산은 헤더로 옮겼다 — 캘리브레이션과 같은 표를 써야 한다)

	/** 보이스 수신 배수. 0.5 → 1.0 (= 지금 음량). */
	static float ToVoiceOutputVolume(float T) { return MapWithMidpoint(T, 0.f, 1.f, 2.f); }

	/**
	 * 마이크 게인. 0.5 → 1.5 (= voice.MicInputGain 기본값).
	 */
	static float ToMicInputGain(float T) { return MapWithMidpoint(T, 0.5f, 1.5f, 2.f); }

	static void SetCVarFloat(const TCHAR* Name, float Value)
	{
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			CVar->Set(Value, ECVF_SetByGameSetting);
		}
	}
}

void UFASettingsBridge::RegisterAndKeepSoundClassTree(FAudioDeviceManager* DeviceManager, USoundClass* Root)
{
	if (!Root)
	{
		return;
	}

	// 자식 목록이 순환하면(에디터에서 잘못 엮으면 가능) 무한 재귀가 된다 — 본 것은 건너뛴다.
	TSet<USoundClass*> Visited;
	TArray<USoundClass*> Pending{ Root };

	while (!Pending.IsEmpty())
	{
		USoundClass* Current = Pending.Pop(EAllowShrinking::No);
		if (!Current || Visited.Contains(Current))
		{
			continue;
		}
		Visited.Add(Current);

		ReferencedSoundClasses.AddUnique(Current);

		if (DeviceManager)
		{
			DeviceManager->RegisterSoundClass(Current);
		}

		for (const TObjectPtr<USoundClass>& Child : Current->ChildClasses)
		{
			Pending.Add(Child);
		}
	}
}

void UFASettingsBridge::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 설정 서브시스템이 먼저 서 있어야 값을 읽을 수 있다.
	Collection.InitializeDependency(USKSettingsSubsystem::StaticClass());

	if (USKSettingsSubsystem* Settings = GetSettings())
	{
		Settings->OnSettingsChanged.AddDynamic(this, &UFASettingsBridge::HandleSettingsChanged);
		Settings->OnUISoundRequested.AddDynamic(this, &UFASettingsBridge::HandleUISoundRequested);

		// 플러그인은 오디오 장치를 모른다. 설정창의 '입력 장치' 목록은 여기서 공급한다.
		Settings->MicDeviceProvider.BindStatic(&UFAVoiceStatics::GetCaptureDevices);
	}
	else
	{
		UE_LOG(LogFASettings, Error, TEXT("[FA] SettingsKit 서브시스템이 없다. 설정값이 게임에 반영되지 않는다."));
	}

	// SoundMix 푸시는 월드에 걸리는 것이라 레벨이 바뀌면 사라진다.
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UFASettingsBridge::HandlePostLoadMap);

	// 이 시점엔 아직 월드가 없을 수 있다. 오디오는 PostLoadMap 에서 다시 걸린다.
	ApplyAll();
}

void UFASettingsBridge::Deinitialize()
{
	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}

	if (USKSettingsSubsystem* Settings = GetSettings())
	{
		Settings->OnSettingsChanged.RemoveAll(this);
		Settings->OnUISoundRequested.RemoveAll(this);
		Settings->MicDeviceProvider.Unbind();
	}

	Super::Deinitialize();
}

USKSettingsSubsystem* UFASettingsBridge::GetSettings() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<USKSettingsSubsystem>() : nullptr;
}

void UFASettingsBridge::HandlePostLoadMap(UWorld* LoadedWorld)
{
	ApplyAll();
}

void UFASettingsBridge::HandleSettingsChanged(ESKSettingsTab Category)
{
	switch (Category)
	{
	case ESKSettingsTab::Audio:    ApplyAudio(); break;
	case ESKSettingsTab::Voice:    ApplyVoice(); break;
	case ESKSettingsTab::Video:    ApplyVideo(); break;
	case ESKSettingsTab::Controls: break; // 감도는 캐릭터가 매 입력마다 읽어간다. 밀어줄 게 없다.
	default: break;
	}
}

void UFASettingsBridge::HandleUISoundRequested(FName SoundId)
{
	UFAAudioSubsystem::PlayFASound(this, SoundId);
}

void UFASettingsBridge::ApplyAll()
{
	ApplyAudio();
	ApplyVoice();
	ApplyVideo();
}

// ============================================================
// 오디오 — SoundMix 로 SoundClass 볼륨을 덮어쓴다
// ============================================================
void UFASettingsBridge::ApplyAudio()
{
	const USKSettingsSubsystem* Settings = GetSettings();
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!Settings || !World)
	{
		return;
	}

	const UFAAudioSettings* AudioSettings = UFAAudioSettings::Get();
	if (!AudioSettings)
	{
		return;
	}

	USoundMix* Mix = AudioSettings->UserSettingsSoundMix.LoadSynchronous();
	if (!Mix)
	{
		if (!bWarnedMissingSoundMix)
		{
			bWarnedMissingSoundMix = true;
			UE_LOG(LogFASettings, Warning,
				TEXT("[FA] 볼륨 슬라이더가 먹지 않는다 — Project Settings → Game → Falling Apart | Audio 의 "
				     "User Settings Sound Mix 가 비어 있다. SoundMix 와 SoundClass 4종을 만들어 지정할 것."));
		}
		return;
	}

	// 믹스도 붙들어야 한다 — 푸시된 믹스를 오디오 디바이스가 날 포인터로 들고 있다.
	ActiveSoundMix = Mix;

	const FSKSettingsValues& V = Settings->GetValues();

	// 음소거는 마스터를 0 으로 눌러서 처리한다. 개별 볼륨값은 그대로 남으므로 풀면 원래대로 돌아온다.
	const float MasterVolume = V.bMuted ? 0.f : V.MasterVolume;

	FAudioDeviceManager* DeviceManager = GEngine ? GEngine->GetAudioDeviceManager() : nullptr;

	// bApplyToChildren = false. SoundClass 볼륨은 계층을 따라 내려가며 이미 곱해지므로,
	// 여기서 자식까지 덮으면 마스터가 개별 볼륨을 지워버린다.
	auto Override = [this, World, Mix, DeviceManager](const TSoftObjectPtr<USoundClass>& SoftClass, float Volume)
	{
		USoundClass* SoundClass = SoftClass.LoadSynchronous();
		if (!SoundClass)
		{
			return;
		}

		RegisterAndKeepSoundClassTree(DeviceManager, SoundClass);

		// 페이드 0 — 슬라이더를 끄는 동안 초당 수십 번 불리므로 보간을 걸면 서로 겹친다.
		UGameplayStatics::SetSoundMixClassOverride(World, Mix, SoundClass, Volume, 1.f, 0.f, false);
	};

	Override(AudioSettings->MasterSoundClass, MasterVolume);
	Override(AudioSettings->MusicSoundClass, V.MusicVolume);
	Override(AudioSettings->SFXSoundClass, V.SFXVolume);
	Override(AudioSettings->UISoundClass, V.UIVolume);

	// 이미 올라와 있어도 다시 밀어 넣는 건 안전하다(중복 푸시는 무시된다).
	UGameplayStatics::PushSoundMixModifier(World, Mix);
}

// ============================================================
// 보이스
// ============================================================
void UFASettingsBridge::ApplyVoice()
{
	const USKSettingsSubsystem* Settings = GetSettings();
	if (!Settings)
	{
		return;
	}

	const FSKSettingsValues& V = Settings->GetValues();

	// 수신 볼륨 — 지금 말하고 있는 화자에게도 즉시 반영된다.
	const float OutputVolume = V.bVoiceMuted ? 0.f : FASettingsMapping::ToVoiceOutputVolume(V.VoiceOutputVolume);
	UFAVoiceStatics::SetVoiceOutputVolume(OutputVolume);

	// 내 마이크 — 게인과 게이트는 서로 다른 CVar 이고 둘 다 런타임 변경이 먹는다.
	const float MicGain = FASettingsMapping::ToMicInputGain(V.VoiceInputGain);
	FASettingsMapping::SetCVarFloat(TEXT("voice.MicInputGain"), MicGain);

	const float NoiseGate = FASettingsMapping::ToNoiseGate(V.VoiceGateThreshold);
	FASettingsMapping::SetCVarFloat(TEXT("voice.MicNoiseGateThreshold"), NoiseGate);

	// 전송 임계는 UVOIPStatics 를 거쳐야 캡처 쪽까지 반영된다.
	UFAVoiceStatics::SetMicThreshold(FASettingsMapping::ToSilenceThreshold(NoiseGate));

	// 마이크 방식 — 지금 조종 중인 Pawn 에 즉시 반영되고, 다음 라운드에 새로 스폰되는 Pawn 도 이 값으로 시작한다.
	const bool bPushToTalk = V.MicMode == ESKVoiceMicMode::PushToTalk;
	UFAVoiceStatics::SetPushToTalkEnabled(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr, bPushToTalk);

	// 입력 장치 — 캡처가 이미 있으면 바로 바뀌고, 없으면(메인 메뉴) 처음 말할 때 들어간다.
	// 슬라이더를 끄는 동안에도 불리지만 같은 장치면 아무것도 안 한다.
	UFAVoiceStatics::SetCaptureDevice(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr, V.MicDeviceName);

	UE_LOG(LogFASettings, Verbose,
		TEXT("[FA] 보이스 적용 — 출력 %.2f / 게인 %.2f / 게이트 %.4f / 마이크 %s / 장치 %s"),
		OutputVolume, MicGain, NoiseGate, bPushToTalk ? TEXT("푸시투토크") : TEXT("오픈 마이크"),
		V.MicDeviceName.IsEmpty() ? TEXT("시스템 기본") : *V.MicDeviceName);
}

// ============================================================
// 화면
// ============================================================
void UFASettingsBridge::ApplyVideo()
{
	const USKSettingsSubsystem* Settings = GetSettings();
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!Settings || !World)
	{
		return;
	}

	// 감마·해상도·화질은 플러그인과 엔진이 이미 처리했다. 여기서는 시야각만 남는다.
	//
	// ⚠ 시야 계열 룰(NarrowFOV 등)이 붙으면 최종 시야각의 주인은 그쪽이 된다.
	// 여기서는 카메라 컴포넌트를 직접 찾아 넣기만 하므로, 룰이 나중에 덮어써도 충돌하지 않는다.
	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC)
	{
		return;
	}

	if (const APawn* Pawn = PC->GetPawn())
	{
		if (UCameraComponent* Camera = Pawn->FindComponentByClass<UCameraComponent>())
		{
			Camera->SetFieldOfView(Settings->GetValues().FieldOfView);
		}
	}
}

// ============================================================
// 조작 — 값을 읽어가는 쪽에서 부른다
// ============================================================
float UFASettingsBridge::GetMouseSensitivity(const UObject* WorldContextObject)
{
	if (const USKSettingsSubsystem* Settings = USKSettingsSubsystem::Get(WorldContextObject))
	{
		return Settings->GetValues().MouseSensitivity;
	}
	return 1.f;
}

bool UFASettingsBridge::IsYAxisInverted(const UObject* WorldContextObject)
{
	if (const USKSettingsSubsystem* Settings = USKSettingsSubsystem::Get(WorldContextObject))
	{
		return Settings->GetValues().bInvertYAxis;
	}
	return false;
}
