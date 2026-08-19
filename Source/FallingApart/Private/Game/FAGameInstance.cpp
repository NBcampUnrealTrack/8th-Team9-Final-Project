// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/FAGameInstance.h"

#include "Network/FAOnlineSessionSubsystem.h"

UFAGameInstance::UFAGameInstance()
{

	
}

void UFAGameInstance::Init()
{
	Super::Init();
	if (!GEngine)
		return;
	NetworkFailureDelegateHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::HandleNetworkFailure);
	TravelFailureDelegateHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::HandleTravelFailure);
}

void UFAGameInstance::Shutdown()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().RemoveAll(this);
		GEngine->OnTravelFailure().RemoveAll(this);
	}
	Super::Shutdown();
}

void UFAGameInstance::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType,
	const FString& ErrorString)
{
	// 서버 접속 실패, 접속 거부, Validate Fail 등 처리.
	UE_LOG(LogFAOSS, Error, TEXT("UFAGameInstance::HandleNetworkFailure : Message %s"), *ErrorString)
	
	ProcessJoinFailure();
}

void UFAGameInstance::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	// 서버 레벨 이동(Travel)실패 처리.
	UE_LOG(LogFAOSS, Error, TEXT("UFAGameInstance::HandleTravelFailure : Message %s"), *ErrorString)
	
	ProcessJoinFailure();
}

void UFAGameInstance::ProcessJoinFailure()
{
	if (UFAOnlineSessionSubsystem* SessionSubsystem =
	GetSubsystem<UFAOnlineSessionSubsystem>())
	{
		if (SessionSubsystem->GetCurrentSession())
		{
			SessionSubsystem->DestroySession();
		}
	}
}
