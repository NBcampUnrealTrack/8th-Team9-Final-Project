// Fill out your copyright notice in the Description page of Project Settings.

#include "Voice/FAVoiceStatics.h"

#include "Components/AudioComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Interfaces/VoiceCapture.h"
#include "Interfaces/VoiceInterface.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DefinePrivateMemberPtr.h"
#include "Net/OnlineEngineInterface.h"
#include "Net/VoiceConfig.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemNames.h"
#include "OnlineSubsystemUtils.h"
#include "UObject/UObjectIterator.h"
#include "VoiceEngineImpl.h"
#include "VoiceInterfaceImpl.h"
#include "VoipListenerSynthComponent.h"
#include "Voice/FAVoiceComponent.h"
#include "Voice/FAVoiceTalker.h"
#include "Voice/FAVoiceTypes.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
#include <mmsystem.h>
#include <dsound.h>
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"
#endif

// 엔진 캡처(IVoiceCapture)는 FOnlineVoiceImpl::VoiceEngine → FVoiceEngineImpl::VoiceCapture 로
// 두 겹 모두 protected 안에 있다(GetVoiceCapture 도 protected). 장치를 바꾸려면 이 캡처에 닿는 수밖에 없어서
// 멤버 포인터로 꺼낸다. 전역 범위에 있어야 하는 매크로다.
UE_DEFINE_PRIVATE_MEMBER_PTR(IVoiceEnginePtr, GFAOnlineVoiceEnginePtr, FOnlineVoiceImpl, VoiceEngine);
UE_DEFINE_PRIVATE_MEMBER_PTR(TSharedPtr<IVoiceCapture>, GFAVoiceEngineCapturePtr, FVoiceEngineImpl, VoiceCapture);
// 엔진이 "지금 녹음하려는 중"인지(FVoiceEngineImpl::IsRecording 도 protected). 장치를 바꾼 뒤 녹음을 다시 켤지 판단한다.
UE_DEFINE_PRIVATE_MEMBER_PTR(bool, GFAVoiceEngineIsCapturingPtr, FVoiceEngineImpl, bIsCapturing);

namespace FAVoiceStaticsInternal
{
	/** 출력 볼륨의 실체는 UFAVoiceTalker 가 읽는 CVar 이다. 이름을 한 곳에만 둔다. */
	static IConsoleVariable* FindOutputVolumeCVar()
	{
		return IConsoleManager::Get().FindConsoleVariable(TEXT("FAVoice.OutputVolume"));
	}

	/**
	 * 마이크 방식(0 = 오픈 마이크, 1 = 푸시투토크).
	 *
	 * 새로 스폰되는 UFAVoiceComponent 가 BeginPlay 에서 이 값을 집어간다.
	 */
	static TAutoConsoleVariable<int32> CVarPushToTalk(
		TEXT("FAVoice.PushToTalk"),
		0,
		TEXT("내 마이크를 누르는 동안만 열지 여부 (사용자 설정). 0 = 오픈 마이크, 1 = 푸시투토크"),
		ECVF_Default);

	/**
	 * 마이크 입력 장치 이름 (DirectSound 장치 설명 문자열). 비우면 시스템 기본.
	 * 캡처는 처음 말할 때 생기므로 그 전에 고른 값은 여기 보관해 두었다가 ApplyCaptureDevice 가 넣는다.
	 */
	static TAutoConsoleVariable<FString> CVarCaptureDevice(
		TEXT("FAVoice.CaptureDevice"),
		TEXT(""),
		TEXT("마이크 입력 장치 이름 (사용자 설정). 비우면 시스템 기본. 목록은 FAVoice.Devices"),
		ECVF_Default);

#if PLATFORM_WINDOWS
	static BOOL CALLBACK EnumerateCaptureDeviceCallback(LPGUID Guid, LPCWSTR Description, LPCWSTR /*Module*/, LPVOID Context)
	{
		// 첫 항목은 GUID 가 없는 "주 사운드 캡처 드라이버"(= 기본 장치의 별칭)다. 엔진도 건너뛴다.
		// 그 다음부터는 윈도우 기본 장치가 맨 앞에 온다.
		if (Guid != nullptr && Description != nullptr)
		{
			static_cast<TArray<FString>*>(Context)->AddUnique(FString(Description));
		}
		return true;
	}
#endif

	/**
	 * 우리가 관리 중인 엔진 캡처와 그 주인 엔진.
	 * 푸시투토크는 누를 때마다 RegisterLocalTalker → ApplyCaptureDevice 를 타므로,
	 * 이미 넣은 캡처에 버퍼를 다시 만들지 않으려고 기억한다. 감시 틱도 이것만 본다.
	 */
	static TWeakPtr<IVoiceCapture> AppliedCapture;
	static TWeakPtr<IVoiceEngine, ESPMode::ThreadSafe> AppliedEngine;

	/** 사용자가 고른 장치(CVar 값). 빈 문자열 = 시스템 기본. */
	static FString AppliedDeviceName;

	/**
	 * 실제로 열려 있는 장치. 고른 장치가 빠져서 다른 장치로 옮겨 갔으면 AppliedDeviceName 과 다르다.
	 * 빈 문자열 = 엔진이 게임 시작 때 잡은 기본 장치. bCaptureLost 면 아무 장치도 안 열려 있다.
	 */
	static FString ActiveDeviceName;
	static bool bCaptureLost = false;

	/** 직전 감시 때 보인 장치들. "새로 꽂혔다"를 알아내는 데만 쓴다. */
	static TArray<FString> LastSeenDevices;
	static double NextDeviceProbeTime = 0.0;

	static FTSTicker::FDelegateHandle WatchdogHandle;

	struct FEngineVoice
	{
		TSharedPtr<IVoiceEngine, ESPMode::ThreadSafe> Engine;
		TSharedPtr<IVoiceCapture> Capture;
	};

	/**
	 * 엔진이 지금 쓰는 캡처. 아직 아무도 말하지 않았으면(RegisterLocalTalker 전) 없다.
	 * STEAM / NULL 만 다룬다 — 둘 다 FOnlineVoiceImpl + FVoiceEngineImpl 계열이라 캐스트가 맞는다.
	 * 다른 백엔드는 구조를 모르므로 건드리지 않는다.
	 */
	static FEngineVoice FindEngineVoice(const UObject* WorldContextObject)
	{
		const UWorld* World = GEngine != nullptr && WorldContextObject != nullptr
			? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
			: nullptr;
		IOnlineSubsystem* OnlineSubsystem = World != nullptr ? Online::GetSubsystem(World) : IOnlineSubsystem::Get();
		if (OnlineSubsystem == nullptr)
		{
			return {};
		}

		const FName SubsystemName = OnlineSubsystem->GetSubsystemName();
		if (SubsystemName != STEAM_SUBSYSTEM && SubsystemName != NULL_SUBSYSTEM)
		{
			return {};
		}

		const IOnlineVoicePtr VoiceInterface = OnlineSubsystem->GetVoiceInterface();
		if (!VoiceInterface.IsValid())
		{
			return {};
		}

		const IVoiceEnginePtr& VoiceEngine = static_cast<FOnlineVoiceImpl*>(VoiceInterface.Get())->*GFAOnlineVoiceEnginePtr;
		if (!VoiceEngine.IsValid())
		{
			return {};
		}

		return { VoiceEngine, static_cast<FVoiceEngineImpl*>(VoiceEngine.Get())->*GFAVoiceEngineCapturePtr };
	}

	static bool IsEngineRecording(const IVoiceEngine& Engine)
	{
		return static_cast<const FVoiceEngineImpl&>(Engine).*GFAVoiceEngineIsCapturingPtr;
	}

	static TArray<FString> EnumerateCaptureDevices()
	{
		TArray<FString> Devices;
#if PLATFORM_WINDOWS
		const HRESULT Result = DirectSoundCaptureEnumerateW(&EnumerateCaptureDeviceCallback, &Devices);
		if (FAILED(Result))
		{
			UE_LOG(LogFAVoice, Warning, TEXT("마이크 장치 목록을 읽지 못했다 (0x%08x)."), static_cast<uint32>(Result));
		}
#endif
		return Devices;
	}

	/**
	 * 고른 장치를 열고, 안 되면 지금 연결된 장치를 윈도우 기본 순서대로 시도한다.
	 *
	 * - 엔진의 ChangeDevice 는 버퍼를 새로 만들면서 녹음을 멈춘 채로 둔다. 엔진이 녹음 중이면 다시 켠다 —
	 *   안 그러면 엔진은 녹음 중이라고 믿는데 소리가 안 나간다.
	 * - 빈 이름("")은 엔진이 게임 시작 때 잡아 둔 기본 장치라, 그 장치가 빠졌으면 같이 죽는다.
	 *   그래서 지금 목록의 첫 장치(= 지금 윈도우 기본)를 먼저 시도하고 ""는 맨 끝에 둔다.
	 * - 엔진 장치 목록은 게임 시작 때 한 번만 만들어져서, 그 뒤에 꽂은 장치는 목록에 보여도 열리지 않는다.
	 */
	static void OpenBestCaptureDevice(IVoiceCapture& Capture, const IVoiceEngine& Engine, const FString& DesiredDevice)
	{
		const TArray<FString> Present = EnumerateCaptureDevices();

		TArray<FString> Candidates;
		if (!DesiredDevice.IsEmpty() && Present.Contains(DesiredDevice))
		{
			Candidates.Add(DesiredDevice);
		}
		for (const FString& Device : Present)
		{
			Candidates.AddUnique(Device);
		}
		Candidates.Add(FString());

		const int32 SampleRate = UVOIPStatics::GetVoiceSampleRate();
		const int32 NumChannels = UVOIPStatics::GetVoiceNumChannels();

		bCaptureLost = true;
		for (const FString& Candidate : Candidates)
		{
			if (Capture.ChangeDevice(Candidate, SampleRate, NumChannels))
			{
				ActiveDeviceName = Candidate;
				bCaptureLost = false;
				break;
			}
		}

		LastSeenDevices = Present;

		if (bCaptureLost)
		{
			// 실패한 ChangeDevice 는 캡처를 NotCapturing 으로 남긴다 — 엔진이 더는 매 틱 에러를 찍지 않는다.
			UE_LOG(LogFAVoice, Warning, TEXT("열 수 있는 마이크가 없다. 장치를 꽂으면 자동으로 다시 잡는다."));
			return;
		}

		if (IsEngineRecording(Engine))
		{
			Capture.Start();
		}

		if (ActiveDeviceName == DesiredDevice)
		{
			UE_LOG(LogFAVoice, Log, TEXT("마이크 장치 적용: %s"),
				DesiredDevice.IsEmpty() ? TEXT("시스템 기본") : *DesiredDevice);
		}
		else if (DesiredDevice.IsEmpty() && Present.Num() > 0 && ActiveDeviceName == Present[0])
		{
			// 시스템 기본 = 지금 윈도우 기본(목록 맨 앞). 대체가 아니라 원래 의도대로 연 것이다.
			UE_LOG(LogFAVoice, Log, TEXT("마이크 장치 적용: 시스템 기본 (%s)"), *ActiveDeviceName);
		}
		else
		{
			UE_LOG(LogFAVoice, Warning, TEXT("마이크 장치 '%s' 를 열 수 없어 '%s' 를 대신 쓴다. 다시 꽂으면 자동으로 돌아간다."),
				DesiredDevice.IsEmpty() ? TEXT("시스템 기본") : *DesiredDevice,
				ActiveDeviceName.IsEmpty() ? TEXT("시작 시 기본 장치") : *ActiveDeviceName);
		}
	}

	/**
	 * 캡처 감시. 엔진은 장치가 빠지면 캡처를 Error 로 두고 복구하지 않은 채 매 틱 경고를 찍는다
	 * (VoiceCaptureWindows.cpp ProcessData, DSERR_NODRIVER 0x88780078). 그대로 두면 마이크가 끝까지 죽고
	 * 로그가 초당 100줄 넘게 쌓인다. 여기서 Error 를 잡아 다른 장치로 옮기고, 고른 장치가 다시 꽂히면 되돌린다.
	 */
	static bool WatchdogTick(float /*DeltaTime*/)
	{
		const TSharedPtr<IVoiceCapture> Capture = AppliedCapture.Pin();
		const TSharedPtr<IVoiceEngine, ESPMode::ThreadSafe> Engine = AppliedEngine.Pin();
		if (!Capture.IsValid() || !Engine.IsValid())
		{
			return true;
		}

		uint32 AvailableBytes = 0;
		if (Capture->GetCaptureState(AvailableBytes) == EVoiceCaptureState::Error)
		{
			UE_LOG(LogFAVoice, Warning, TEXT("마이크 장치 '%s' 가 끊겼다. 다른 장치로 옮긴다."),
				ActiveDeviceName.IsEmpty() ? TEXT("시스템 기본") : *ActiveDeviceName);
			OpenBestCaptureDevice(*Capture, *Engine, AppliedDeviceName);
			return true;
		}

		// 고른 장치를 못 쓰고 있을 때만 장치 목록을 살핀다. 평소엔 열거 비용을 안 쓴다.
		const bool bWaitingForDesired = !AppliedDeviceName.IsEmpty() && ActiveDeviceName != AppliedDeviceName;
		if (!bWaitingForDesired && !bCaptureLost)
		{
			return true;
		}

		const double Now = FPlatformTime::Seconds();
		if (Now < NextDeviceProbeTime)
		{
			return true;
		}
		NextDeviceProbeTime = Now + 2.0;

		// "새로 나타난" 장치가 있을 때만 다시 연다. 매번 시도하면 게임 실행 뒤 꽂은(엔진이 못 여는)
		// 장치 때문에 2초마다 실패 로그가 쌓인다.
		const TArray<FString> Present = EnumerateCaptureDevices();
		const bool bSomethingNew = Present.ContainsByPredicate([](const FString& Device) { return !LastSeenDevices.Contains(Device); });
		LastSeenDevices = Present;

		if (bSomethingNew && (bCaptureLost || Present.Contains(AppliedDeviceName)))
		{
			UE_LOG(LogFAVoice, Log, TEXT("마이크 장치가 새로 연결됐다. 다시 잡는다."));
			OpenBestCaptureDevice(*Capture, *Engine, AppliedDeviceName);
		}
		return true;
	}

	static void EnsureWatchdog()
	{
		if (!WatchdogHandle.IsValid())
		{
			WatchdogHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&WatchdogTick), 0.25f);
		}
	}
}

void UFAVoiceStatics::SetMicThreshold(float Threshold)
{
	UVOIPStatics::SetMicThreshold(FMath::Clamp(Threshold, 0.f, 1.f));
}

void UFAVoiceStatics::SetVoiceOutputVolume(float Volume)
{
	if (IConsoleVariable* CVar = FAVoiceStaticsInternal::FindOutputVolumeCVar())
	{
		// Sink 가 지금 발화 중인 화자들에게 새 값을 밀어준다 (UFAVoiceTalker::RefreshAllTalkingVolumes).
		CVar->Set(FMath::Max(0.f, Volume), ECVF_SetByGameSetting);
	}
}

float UFAVoiceStatics::GetVoiceOutputVolume()
{
	const IConsoleVariable* CVar = FAVoiceStaticsInternal::FindOutputVolumeCVar();
	return CVar != nullptr ? CVar->GetFloat() : 1.f;
}

void UFAVoiceStatics::SetPushToTalkEnabled(const UObject* WorldContextObject, bool bEnabled)
{
	FAVoiceStaticsInternal::CVarPushToTalk->Set(bEnabled ? 1 : 0, ECVF_SetByGameSetting);

	// 앞으로 스폰될 Pawn 은 CVar 을 보고 시작하고, 지금 말하고 있는 Pawn 은 여기서 바로 바뀐다.
	if (UFAVoiceComponent* VoiceComponent = GetLocalVoiceComponent(WorldContextObject))
	{
		VoiceComponent->SetPushToTalkEnabled(bEnabled);
	}
}

bool UFAVoiceStatics::IsPushToTalkEnabled()
{
	return FAVoiceStaticsInternal::CVarPushToTalk.GetValueOnGameThread() != 0;
}

TArray<FString> UFAVoiceStatics::GetCaptureDevices()
{
	return FAVoiceStaticsInternal::EnumerateCaptureDevices();
}

void UFAVoiceStatics::SetCaptureDevice(const UObject* WorldContextObject, const FString& DeviceName)
{
	FAVoiceStaticsInternal::CVarCaptureDevice->Set(*DeviceName, ECVF_SetByGameSetting);
	ApplyCaptureDevice(WorldContextObject);
}

FString UFAVoiceStatics::GetCaptureDevice()
{
	return FAVoiceStaticsInternal::CVarCaptureDevice.GetValueOnGameThread();
}

void UFAVoiceStatics::ApplyCaptureDevice(const UObject* WorldContextObject)
{
	using namespace FAVoiceStaticsInternal;

	const FEngineVoice EngineVoice = FindEngineVoice(WorldContextObject);
	if (!EngineVoice.Capture.IsValid())
	{
		// 아직 아무도 말하지 않았다. 캡처가 생기는 순간(RegisterLocalTalker 직후) 다시 불린다.
		return;
	}

	const FString DesiredDevice = GetCaptureDevice();
	const bool bSameCapture = AppliedCapture.Pin() == EngineVoice.Capture;
	if (bSameCapture && AppliedDeviceName == DesiredDevice)
	{
		return;
	}

	AppliedCapture = EngineVoice.Capture;
	AppliedEngine = EngineVoice.Engine;
	AppliedDeviceName = DesiredDevice;
	EnsureWatchdog();

	// 새 캡처는 엔진 기본 장치로 태어난다. 원하는 게 기본이면 버퍼를 다시 만들 이유가 없다.
	// (그 기본 장치가 이미 빠져 있으면 감시 틱이 Error 를 보고 옮긴다.)
	if (!bSameCapture && DesiredDevice.IsEmpty())
	{
		ActiveDeviceName.Reset();
		bCaptureLost = false;
		return;
	}

	OpenBestCaptureDevice(*EngineVoice.Capture, *EngineVoice.Engine, DesiredDevice);
}

UFAVoiceComponent* UFAVoiceStatics::GetLocalVoiceComponent(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine != nullptr
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		if (PlayerController == nullptr || !PlayerController->IsLocalController())
		{
			continue;
		}

		// 캐릭터인지 관전 Pawn 인지 묻지 않는다 — 둘 다 같은 컴포넌트를 달고 있다.
		if (const APawn* Pawn = PlayerController->GetPawn())
		{
			return Pawn->FindComponentByClass<UFAVoiceComponent>();
		}
	}

	return nullptr;
}

FName UFAVoiceStatics::GetVoiceBackendName()
{
	// 실제로 올라온 서브시스템. config 의 DefaultPlatformService 를 읽으면
	// 스팀 로그인 실패로 NULL 에 떨어진 상태에서도 Steam 이라고 답한다.
	if (const IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get())
	{
		return OnlineSubsystem->GetSubsystemName();
	}

	return NAME_None;
}

FName UFAVoiceStatics::GetConfiguredVoiceBackendName()
{
	return UOnlineEngineInterface::Get()->GetDefaultOnlineSubsystemName();
}

bool UFAVoiceStatics::IsVoiceEnabledInConfig()
{
	bool bVoiceEnabled = false;
	GConfig->GetBool(TEXT("Voice"), TEXT("bEnabled"), bVoiceEnabled, GEngineIni);

	bool bVoiceInterfaceEnabled = false;
	GConfig->GetBool(TEXT("OnlineSubsystem"), TEXT("bHasVoiceEnabled"), bVoiceInterfaceEnabled, GEngineIni);

	return bVoiceEnabled && bVoiceInterfaceEnabled;
}

bool UFAVoiceStatics::DoesConfigRequirePushToTalk()
{
	bool bRequiresPushToTalk = false;
	GConfig->GetBool(TEXT("/Script/Engine.GameSession"), TEXT("bRequiresPushToTalk"), bRequiresPushToTalk, GGameIni);

	return bRequiresPushToTalk;
}

void UFAVoiceStatics::ShutdownVoicePlaybackForWorld(UWorld* World)
{
	if (World == nullptr)
	{
		return;
	}

	// 원격 토커 정리를 먼저 요청한다. 여기서 UVOIPTalker::OnTalkingEnd() 가 불려
	// 발화 아이콘 등 게임 쪽 상태가 정상적으로 닫힌다.
	if (const IOnlineVoicePtr VoiceInterface = Online::GetVoiceInterface(World))
	{
		VoiceInterface->RemoveAllRemoteTalkers();
	}

	// 위 호출만으로는 컴포넌트 등록이 풀리지 않는다.
	// 다른 월드(PIE 다중 인스턴스)의 것까지 건드리지 않도록 이 월드 소속만 고른다.
	for (TObjectIterator<UVoipListenerSynthComponent> It; It; ++It)
	{
		UVoipListenerSynthComponent* SynthComponent = *It;
		if (!IsValid(SynthComponent) || SynthComponent->GetWorld() != World)
		{
			continue;
		}

		SynthComponent->Stop();

		// AudioComponent 는 SynthComponent 의 자식이 아니라 별도로 등록되므로 따로 떼야 한다.
		if (UAudioComponent* AudioComponent = SynthComponent->GetAudioComponent())
		{
			if (AudioComponent->IsRegistered())
			{
				AudioComponent->UnregisterComponent();
			}
		}

		if (SynthComponent->IsRegistered())
		{
			SynthComponent->UnregisterComponent();
		}
	}
}

namespace FAVoiceDebug
{

	static void DumpVoiceState(UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogFAVoice, Warning, TEXT("FAVoice.Dump: 유효한 월드가 없습니다."));
			return;
		}

		UE_LOG(LogFAVoice, Display, TEXT("========== 근접 보이스 상태 =========="));

		const FName ActiveBackend = UFAVoiceStatics::GetVoiceBackendName();
		const FName ConfiguredBackend = UFAVoiceStatics::GetConfiguredVoiceBackendName();
		UE_LOG(LogFAVoice, Display, TEXT("백엔드    : 실제 %s / 설정 %s%s"),
			*ActiveBackend.ToString(), *ConfiguredBackend.ToString(),
			ActiveBackend == ConfiguredBackend ? TEXT("") : TEXT("   <<< 불일치. 스팀 미실행/로그인 실패 의심"));

		UE_LOG(LogFAVoice, Display, TEXT("config    : 보이스 %s / 푸시투토크 %s"),
			UFAVoiceStatics::IsVoiceEnabledInConfig() ? TEXT("켜짐") : TEXT("꺼짐   <<< 소리가 날 수 없음"),
			UFAVoiceStatics::DoesConfigRequirePushToTalk() ? TEXT("필요") : TEXT("불필요(오픈 마이크)"));

		// 고른 장치가 빠져 있으면 둘이 다르다 — 감시 틱이 다른 장치로 옮겨 둔 상태.
		const FString CaptureDevice = UFAVoiceStatics::GetCaptureDevice();
		const FString& ActiveDevice = FAVoiceStaticsInternal::ActiveDeviceName;
		UE_LOG(LogFAVoice, Display, TEXT("마이크 장치 : 고른 것 %s / 실제 %s"),
			CaptureDevice.IsEmpty() ? TEXT("시스템 기본") : *CaptureDevice,
			!FAVoiceStaticsInternal::AppliedCapture.IsValid() ? TEXT("(캡처 아직 없음)")
			: FAVoiceStaticsInternal::bCaptureLost ? TEXT("없음   <<< 열 수 있는 마이크가 없다")
			: ActiveDevice.IsEmpty() ? TEXT("시작 시 기본 장치") : *ActiveDevice);

		// FOnlineVoiceImpl::Tick 은 세션이 0개면 음성 패킷을 아예 처리하지 않는다.
		// 호스트·클라이언트 양쪽 모두 로컬 세션이 있어야 한다.
		if (IOnlineSessionPtr SessionInterface = Online::GetSessionInterface(World))
		{
			const int32 NumSessions = SessionInterface->GetNumSessions();
			UE_LOG(LogFAVoice, Display, TEXT("세션      : %d 개%s"), NumSessions,
				NumSessions > 0 ? TEXT("") : TEXT("   <<< 0 이면 음성 패킷이 한 개도 안 나간다"));
		}
		else
		{
			UE_LOG(LogFAVoice, Display, TEXT("세션      : 세션 인터페이스 없음   <<< 보이스 불가"));
		}

		// UniqueNetId 가 없으면 UFAVoiceComponent 가 Talker 등록을 못 하고 10초 뒤 포기한다.
		// AGameSession::RegisterPlayer 경로가 끊겼거나 스팀 로그인이 안 된 경우다.
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* PlayerController = It->Get();
			if (PlayerController == nullptr || !PlayerController->IsLocalController())
			{
				continue;
			}

			const APlayerState* PlayerState = PlayerController->PlayerState;
			const bool bHasValidId = PlayerState != nullptr && PlayerState->GetUniqueId().IsValid();
			UE_LOG(LogFAVoice, Display, TEXT("로컬 PC   : %s / UniqueNetId %s"),
				*GetNameSafe(PlayerController),
				bHasValidId ? TEXT("유효") : TEXT("없음   <<< Talker 등록 불가, 보이스 안 붙음"));
		}

		if (IOnlineVoicePtr VoiceInterface = Online::GetVoiceInterface(World))
		{
			UE_LOG(LogFAVoice, Display, TEXT("보이스 인터페이스 덤프:\n%s"), *VoiceInterface->GetVoiceDebugState());
		}
		else
		{
			UE_LOG(LogFAVoice, Display, TEXT("보이스 인터페이스 : 없음   <<< bHasVoiceEnabled 확인"));
		}

		// 기본값 0.08(≈ -22dBFS)은 웹캠·노트북 마이크가 통과하지 못한다.
		const IConsoleVariable* SilenceCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("voice.SilenceDetectionThreshold"));
		const IConsoleVariable* NoiseGateCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("voice.MicNoiseGateThreshold"));
		const IConsoleVariable* MicGainCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("voice.MicInputGain"));

		// 게인은 ini 를 봐도 실제 적용값을 알 수 없다 — 섹션 우선순위와 콘솔 입력이 덮어쓰기 때문에
		// 반드시 런타임 값을 찍어서 확인해야 한다.
		const float MicGain = MicGainCVar != nullptr ? MicGainCVar->GetFloat() : -1.f;

		UE_LOG(LogFAVoice, Display, TEXT("마이크 게이트 : Silence %.4f / NoiseGate %.4f / Gain %.2f%s"),
			SilenceCVar != nullptr ? SilenceCVar->GetFloat() : -1.f,
			NoiseGateCVar != nullptr ? NoiseGateCVar->GetFloat() : -1.f,
			MicGain,
			// 게인이 곱해진 뒤 int16 로 하드 클리핑되므로, 높은 게인은 큰 목소리를 그대로 뭉갠다.
			MicGain > 2.f ? TEXT("   <<< 클리핑 위험 (큰 목소리가 깨진다)") : TEXT(""));

		// "작게 들린다"의 답은 대부분 여기 있다. 마이크 게인은 클리핑 때문에 못 올리므로
		// 음량은 재생 배수로 번다 — 엔진 기본은 1.5 다.
		const float UserOutputVolume = UFAVoiceStatics::GetVoiceOutputVolume();
		UE_LOG(LogFAVoice, Display, TEXT("출력 볼륨 : 사용자 %.2f (FAVoice.OutputVolume)"), UserOutputVolume);

		for (TObjectIterator<UFAVoiceTalker> It; It; ++It)
		{
			const UFAVoiceTalker* Talker = *It;
			if (Talker == nullptr || Talker->GetWorld() != World)
			{
				continue;
			}

			// 개별 배수(설정창 접속자 목록)는 이미 Talker 의 배수에 곱해져 있다.
			// 따로 찍어 주지 않으면 "왜 이 사람만 작지" 의 답이 로그에 안 남는다.
			const UFAVoiceComponent* VoiceComponent = Talker->GetOwner() != nullptr
				? Talker->GetOwner()->FindComponentByClass<UFAVoiceComponent>()
				: nullptr;
			const float ListenerScale = VoiceComponent != nullptr ? VoiceComponent->GetListenerVolumeScale() : 1.f;

			FString ListenerNote;
			if (ListenerScale <= 0.f)
			{
				ListenerNote = TEXT("   (내가 음소거함)");
			}
			else if (!FMath::IsNearlyEqual(ListenerScale, 1.f))
			{
				ListenerNote = FString::Printf(TEXT("   (내가 개별 x%.2f)"), ListenerScale);
			}

			// 엔진 VoiceTalkerMap 에서 밀려나 있으면 재생 자체가 이 화자를 안 거친다 —
			// 즉 위에 찍힌 배수·뮤트가 전부 무의미하다. "뮤트가 안 먹는다" 의 첫 확인 지점.
			const bool bRegistered = Talker->IsRegisteredWithVoiceEngine();

			UE_LOG(LogFAVoice, Display, TEXT("  화자 %-24s 배수 %.2f → 최종 %.2f%s%s%s"),
				*GetNameSafe(Talker->GetOwner()),
				Talker->GetPlaybackVolumeMultiplier(),
				Talker->GetEffectivePlaybackVolume(),
				*ListenerNote,
				Talker->IsTalking() ? TEXT("   (발화 중)") : TEXT(""),
				bRegistered ? TEXT("") : TEXT("   <<< 엔진 등록 풀림. 음량·뮤트·감쇠 안 먹는다"));
		}

		UE_LOG(LogFAVoice, Display, TEXT("======================================"));
	}

	static FAutoConsoleCommandWithWorld DumpVoiceStateCommand(
		TEXT("FAVoice.Dump"),
		TEXT("근접 보이스 상태를 한 번에 덤프한다 (백엔드/세션/UniqueNetId/마이크 게이트)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&DumpVoiceState));

	static FAutoConsoleCommand ListCaptureDevicesCommand(
		TEXT("FAVoice.Devices"),
		TEXT("마이크 장치 목록을 번호와 함께 찍는다. FAVoice.Device <번호> 로 고른다."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			const TArray<FString> Devices = UFAVoiceStatics::GetCaptureDevices();
			const FString Current = UFAVoiceStatics::GetCaptureDevice();

			UE_LOG(LogFAVoice, Display, TEXT("  0: 시스템 기본%s"), Current.IsEmpty() ? TEXT("   <<< 선택됨") : TEXT(""));
			for (int32 Index = 0; Index < Devices.Num(); ++Index)
			{
				UE_LOG(LogFAVoice, Display, TEXT("  %d: %s%s"), Index + 1, *Devices[Index],
					Devices[Index] == Current ? TEXT("   <<< 선택됨") : TEXT(""));
			}
		}));

	/** 설정창을 거치지 않으므로 저장되지 않고, 설정창에서 적용을 누르면 저장값으로 덮인다. 테스트용. */
	static void SelectCaptureDevice(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() == 0)
		{
			UE_LOG(LogFAVoice, Display, TEXT("사용법: FAVoice.Device <번호|이름조각>  (0 = 시스템 기본, 목록은 FAVoice.Devices)"));
			return;
		}

		const TArray<FString> Devices = UFAVoiceStatics::GetCaptureDevices();
		const FString& Query = Args[0];

		FString Chosen;
		bool bFound = false;
		if (Query.IsNumeric())
		{
			const int32 Number = FCString::Atoi(*Query);
			bFound = Number == 0 || Devices.IsValidIndex(Number - 1);
			if (bFound && Number > 0)
			{
				Chosen = Devices[Number - 1];
			}
		}
		else if (const FString* Match = Devices.FindByPredicate([&Query](const FString& Name) { return Name.Contains(Query); }))
		{
			Chosen = *Match;
			bFound = true;
		}

		if (!bFound)
		{
			UE_LOG(LogFAVoice, Warning, TEXT("'%s' 에 맞는 마이크 장치가 없다. FAVoice.Devices 로 목록을 볼 것."), *Query);
			return;
		}

		UFAVoiceStatics::SetCaptureDevice(World, Chosen);
	}

	static FAutoConsoleCommandWithWorldAndArgs SelectCaptureDeviceCommand(
		TEXT("FAVoice.Device"),
		TEXT("마이크 장치를 바꾼다. FAVoice.Device <번호|이름조각> (0 = 시스템 기본). 저장되지 않는 테스트용."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SelectCaptureDevice));
}
