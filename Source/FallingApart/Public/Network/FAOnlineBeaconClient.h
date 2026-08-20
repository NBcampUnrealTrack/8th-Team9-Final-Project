// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OnlineBeaconClient.h"
#include "FAOnlineBeaconClient.generated.h"

UENUM()
enum class EFABeaconPasswordVerifyResult : uint8
{
	Success,
	InvalidPassword,
	BeaconUnavailable,
	TooManyAttempts,
	InternalError
};

DECLARE_MULTICAST_DELEGATE_TwoParams( FOnBeaconVerifyComplete, EFABeaconPasswordVerifyResult, const FString&);

UCLASS(Transient, NotPlaceable)
class FALLINGAPART_API AFAOnlineBeaconClient : public AOnlineBeaconClient
{
	GENERATED_BODY()
	
	
public:
	AFAOnlineBeaconClient();
	
	virtual void OnConnected() override;
	
protected:
	virtual void OnFailure(EBeaconFailureReason Reason, FStringView ErrorMessage) override;
	
#pragma region Password
public:
	void SetPasswordForVerification(const FString& InPassword);
	
	FOnBeaconVerifyComplete OnVerificationComplete;
	
protected:
	//클라이언트->서버 검증요청
	UFUNCTION(Server, Reliable)
	void ServerVerifyPassword(const FString& Password);
	
	//서버->클라이언트 검증결과전달.
	UFUNCTION(Client, Reliable)
	void ClientPasswordVerificationResult(EFABeaconPasswordVerifyResult Result, const FString& JoinToken);
	
private:
	FString PendingPassword;
	
	int32 VerificationAttemptCount = 0;
#pragma endregion 
};
