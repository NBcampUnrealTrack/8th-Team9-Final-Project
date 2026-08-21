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
	// UE 5.8의 상세 실패 경로도 최종적으로 이 deprecated 호환 오버로드를 호출한다.
	// AOnlineBeaconClient::OnFailure()를 재정의해야 초기화/타임아웃 실패를 빠짐없이 받는다.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	virtual void OnFailure() override;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
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
	void CompleteVerification(EFABeaconPasswordVerifyResult Result, const FString& JoinToken);

	FString PendingPassword;
	
	int32 VerificationAttemptCount = 0;
	bool bVerificationCompleted = false;
#pragma endregion 
};
