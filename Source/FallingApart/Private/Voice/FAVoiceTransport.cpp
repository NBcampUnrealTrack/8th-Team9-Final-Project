// Fill out your copyright notice in the Description page of Project Settings.

#include "Voice/FAVoiceTransport.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Net/OnlineEngineInterface.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemNames.h"
#include "OnlineSubsystemUtils.h"
#include "Voice/FAVoiceStatics.h"
#include "Voice/FAVoiceTypes.h"

namespace FAVoice
{
	/**
	 * 보이스를 돌리기 위한 최소 세션의 이름.
	 */
	static const FName FallbackVoiceSessionName(TEXT("FAVoiceFallbackSession"));
}

bool UFAVoiceTransport::IsReadyToTalk(const APlayerController* OwningController) const
{
	return false;
}

void UFAVoiceTransport::SetLocalTalking(APlayerController* OwningController, bool bTalking)
{
}

FName UFAVoiceTransport::GetBackendName() const
{
	return NAME_None;
}

void UFAVoiceTransport::ShutdownTransport(APlayerController* OwningController)
{
}

bool UFAEngineVoiceTransport::IsReadyToTalk(const APlayerController* OwningController) const
{
	return OwningController != nullptr && OwningController->GetLocalPlayer() != nullptr;
}

void UFAEngineVoiceTransport::SetLocalTalking(APlayerController* OwningController, bool bTalking)
{
	if (!IsReadyToTalk(OwningController))
	{
		return;
	}

	if (bTalking)
	{
		EnsureVoiceSessionExists(OwningController);

		const ULocalPlayer* LocalPlayer = OwningController->GetLocalPlayer();
		IOnlineVoicePtr VoiceInterface = Online::GetVoiceInterface(OwningController->GetWorld());
		if (VoiceInterface.IsValid() && LocalPlayer != nullptr)
		{
			VoiceInterface->RegisterLocalTalker(static_cast<uint32>(LocalPlayer->GetControllerId()));

			// 엔진 캡처는 위 호출에서 처음 생기고 늘 기본 장치로 태어난다. 녹음이 켜지기(ToggleSpeaking) 전에
			// 설정창에서 고른 장치로 바꿔 둔다. 이미 넣은 캡처면 아무것도 안 한다.
			UFAVoiceStatics::ApplyCaptureDevice(OwningController);
		}
	}

	OwningController->ToggleSpeaking(bTalking);
}

FName UFAEngineVoiceTransport::GetBackendName() const
{
	// config 의 DefaultPlatformService 가 아니라 실제로 올라온 서브시스템을 돌려준다.
	// 스팀 클라이언트가 없거나 로그인이 실패하면 엔진이 조용히 NULL 로 떨어지는데,
	// 설정값을 읽으면 그 상황에도 Steam 이라고 답해서 원인 추적이 불가능해진다.
	if (const IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get())
	{
		return OnlineSubsystem->GetSubsystemName();
	}

	return NAME_None;
}

void UFAEngineVoiceTransport::ShutdownTransport(APlayerController* OwningController)
{
	if (OwningController == nullptr || !OwningController->IsLocalController())
	{
		return;
	}

	UWorld* World = OwningController->GetWorld();

	if (IOnlineVoicePtr VoiceInterface = Online::GetVoiceInterface(World))
	{
		VoiceInterface->RemoveAllRemoteTalkers();
	}

	IOnlineSessionPtr SessionInterface = Online::GetSessionInterface(World);
	if (SessionInterface.IsValid() && SessionInterface->GetNamedSession(FAVoice::FallbackVoiceSessionName) != nullptr)
	{
		SessionInterface->DestroySession(FAVoice::FallbackVoiceSessionName);
	}
}

void UFAEngineVoiceTransport::EnsureVoiceSessionExists(APlayerController* OwningController) const
{
	IOnlineSessionPtr SessionInterface = Online::GetSessionInterface(OwningController->GetWorld());
	if (!SessionInterface.IsValid())
	{
		return;
	}

	// 세션이 이미 있으면(서버 담당의 Steam Lobby 등)
	if (SessionInterface->GetNumSessions() > 0)
	{
		return;
	}

	const IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	const bool bNullBackend = OnlineSubsystem == nullptr || OnlineSubsystem->GetSubsystemName() == NULL_SUBSYSTEM;

	FOnlineSessionSettings SessionSettings;
	SessionSettings.NumPublicConnections = 8;
	SessionSettings.bIsLANMatch = bNullBackend;
	SessionSettings.bUseLobbiesIfAvailable = !bNullBackend;
	SessionSettings.bShouldAdvertise = false;
	SessionSettings.bAllowJoinInProgress = true;
	SessionSettings.bUsesPresence = false;
	SessionSettings.bIsDedicated = false;

	const ULocalPlayer* LocalPlayer = OwningController->GetLocalPlayer();
	const int32 LocalUserNum = LocalPlayer != nullptr ? LocalPlayer->GetControllerId() : 0;

	SessionInterface->CreateSession(LocalUserNum, FAVoice::FallbackVoiceSessionName, SessionSettings);

	UE_LOG(LogFAVoice, Warning, TEXT("보이스용 임시 세션 생성 (백엔드 %s) — 엔진 VoIP 는 세션이 없으면 패킷을 처리하지 않는다. "
		"정상 흐름(메뉴에서 서버 만들기/참가하기)을 탔다면 이 로그는 뜨지 않아야 한다."),
		bNullBackend ? TEXT("NULL") : TEXT("Steam"));
}
