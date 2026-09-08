// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "FAGameInstance.generated.h"

/**
 * 
 */
UCLASS()
class FALLINGAPART_API UFAGameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:
	UFAGameInstance();
	
	virtual void Init() override;
	virtual void Shutdown() override;
	
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

	/**  */
	void ProcessJoinFailure();


private:
	FDelegateHandle NetworkFailureDelegateHandle;// 접속 거부 핸들
	FDelegateHandle TravelFailureDelegateHandle;// Travel 실패 핸들
	
public:
	void SetLastSelectedMapId(FName MapId);
	
	FName GetLastSelectedMapId() const;
	
private:
	UPROPERTY(Transient)
	FName LastSelectedMapId;
};
