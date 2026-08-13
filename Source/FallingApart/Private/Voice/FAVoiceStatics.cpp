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
#include "Voice/FAVoiceTypes.h"

void UFAVoiceStatics::SetMicThreshold(float Threshold)
{
	UVOIPStatics::SetMicThreshold(FMath::Clamp(Threshold, 0.f, 1.f));
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
		UE_LOG(LogFAVoice, Display, TEXT("마이크 게이트 : Silence %.4f / NoiseGate %.4f"),
			SilenceCVar != nullptr ? SilenceCVar->GetFloat() : -1.f,
			NoiseGateCVar != nullptr ? NoiseGateCVar->GetFloat() : -1.f);

		UE_LOG(LogFAVoice, Display, TEXT("======================================"));
	}

	static FAutoConsoleCommandWithWorld DumpVoiceStateCommand(
		TEXT("FAVoice.Dump"),
		TEXT("근접 보이스 상태를 한 번에 덤프한다 (백엔드/세션/UniqueNetId/마이크 게이트)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&DumpVoiceState));
}
