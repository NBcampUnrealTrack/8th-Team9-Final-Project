// Fill out your copyright notice in the Description page of Project Settings.

#include "Voice/FAVoiceStatics.h"

#include "Components/AudioComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Interfaces/VoiceInterface.h"
#include "Misc/ConfigCacheIni.h"
#include "Net/OnlineEngineInterface.h"
#include "Net/VoiceConfig.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "UObject/UObjectIterator.h"
#include "VoipListenerSynthComponent.h"
#include "Voice/FAVoiceComponent.h"
#include "Voice/FAVoiceTalker.h"
#include "Voice/FAVoiceTypes.h"

namespace FAVoiceStaticsInternal
{
	/** 출력 볼륨의 실체는 UFAVoiceTalker 가 읽는 CVar 이다. 이름을 한 곳에만 둔다. */
	static IConsoleVariable* FindOutputVolumeCVar()
	{
		return IConsoleManager::Get().FindConsoleVariable(TEXT("FAVoice.OutputVolume"));
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
}
