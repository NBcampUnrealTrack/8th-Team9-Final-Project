// Fill out your copyright notice in the Description page of Project Settings.

#include "Voice/FAVoiceStatics.h"

#include "Misc/ConfigCacheIni.h"
#include "Net/OnlineEngineInterface.h"
#include "Net/VoiceConfig.h"

void UFAVoiceStatics::SetMicThreshold(float Threshold)
{
	UVOIPStatics::SetMicThreshold(FMath::Clamp(Threshold, 0.f, 1.f));
}

FName UFAVoiceStatics::GetVoiceBackendName()
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
