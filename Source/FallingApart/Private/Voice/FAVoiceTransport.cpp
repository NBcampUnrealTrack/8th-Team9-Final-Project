// Fill out your copyright notice in the Description page of Project Settings.

#include "Voice/FAVoiceTransport.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Net/OnlineEngineInterface.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemUtils.h"
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
		}
	}

	OwningController->ToggleSpeaking(bTalking);
}

FName UFAEngineVoiceTransport::GetBackendName() const
{
	return UOnlineEngineInterface::Get()->GetDefaultOnlineSubsystemName();
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

	FOnlineSessionSettings SessionSettings;
	SessionSettings.NumPublicConnections = 8;
	SessionSettings.bIsLANMatch = true;
	SessionSettings.bShouldAdvertise = false;
	SessionSettings.bAllowJoinInProgress = true;
	SessionSettings.bUsesPresence = false;
	SessionSettings.bIsDedicated = false;

	const ULocalPlayer* LocalPlayer = OwningController->GetLocalPlayer();
	const int32 LocalUserNum = LocalPlayer != nullptr ? LocalPlayer->GetControllerId() : 0;

	SessionInterface->CreateSession(LocalUserNum, FAVoice::FallbackVoiceSessionName, SessionSettings);

	UE_LOG(LogFAVoice, Log, TEXT("보이스용 임시 세션 생성 — 엔진 VoIP 는 세션이 없으면 패킷을 처리하지 않는다. "
		"서버 담당 세션이 붙으면 이 경로는 자동으로 건너뛴다."));
}
