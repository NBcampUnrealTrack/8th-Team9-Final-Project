// Fill out your copyright notice in the Description page of Project Settings.


#include "Network/FAOnlineBeaconClient.h"

#include "Network/FAOnlineSessionSubsystem.h"

AFAOnlineBeaconClient::AFAOnlineBeaconClient()
{
}

void AFAOnlineBeaconClient::OnConnected()
{
	Super::OnConnected();

	UE_LOG(LogFAOSS, Verbose, TEXT("[BeaconAuth] Connected; requesting password verification"));

	// 편의상 연결과 동시에 검증 시도.
	ServerVerifyPassword(PendingPassword);
}

void AFAOnlineBeaconClient::OnFailure()
{
	UE_LOG(LogFAOSS, Warning, TEXT("[BeaconAuth] Beacon connection failed"));

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	Super::OnFailure();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	CompleteVerification(
		EFABeaconPasswordVerifyResult::BeaconUnavailable,
		FString());
}

void AFAOnlineBeaconClient::SetPasswordForVerification(const FString& InPassword)
{
	PendingPassword = InPassword;
}

void AFAOnlineBeaconClient::ServerVerifyPassword_Implementation(const FString& Password)
{
	constexpr int32 MaxAttempts = 3;
	constexpr int32 MaxPasswordLength = 64;

	++VerificationAttemptCount;
	UE_LOG(LogFAOSS, Verbose, TEXT("[BeaconAuth] Server verification attempt %d; NetId=%s"),
		VerificationAttemptCount, *GetUniqueId().ToDebugString());

	if (VerificationAttemptCount > MaxAttempts)
	{
		ClientPasswordVerificationResult(
			EFABeaconPasswordVerifyResult::TooManyAttempts,
			FString());

		return;
	}

	if (Password.Len() > MaxPasswordLength)
	{
		ClientPasswordVerificationResult(
			EFABeaconPasswordVerifyResult::InvalidPassword,
			FString());

		return;
	}

	UFAOnlineSessionSubsystem* SessionSubsystem =
		GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFAOnlineSessionSubsystem>()
		: nullptr;

	if (!SessionSubsystem)
	{
		UE_LOG(LogFAOSS, Error, TEXT("[BeaconAuth] Server session subsystem is unavailable"));
		ClientPasswordVerificationResult(
			EFABeaconPasswordVerifyResult::InternalError,
			FString());

		return;
	}
	
	//서브시스템의 비밀번호 검증 사용
	if (!SessionSubsystem->ValidateHostPassword(Password))
	{
		UE_LOG(LogFAOSS, Log, TEXT("[BeaconAuth] Password rejected; NetId=%s"),
			*GetUniqueId().ToDebugString());
		ClientPasswordVerificationResult(
			EFABeaconPasswordVerifyResult::InvalidPassword,
			FString());

		return;
	}

	//GetUniqueId()는 서버입장에서 커넥션 상대 클라이언트의 UniqueID를 가져옴.
	//즉 대상에게만 유효한 토큰을 발급.
	const FString JoinToken =
		SessionSubsystem->IssueJoinToken(GetUniqueId());

	if (JoinToken.IsEmpty())
	{
		UE_LOG(LogFAOSS, Error, TEXT("[BeaconAuth] Join token issuance failed; NetId=%s"),
			*GetUniqueId().ToDebugString());
		ClientPasswordVerificationResult(
			EFABeaconPasswordVerifyResult::InternalError,
			FString());
		return;
	}
	ClientPasswordVerificationResult(
		EFABeaconPasswordVerifyResult::Success,
		JoinToken);
}

void AFAOnlineBeaconClient::ClientPasswordVerificationResult_Implementation(EFABeaconPasswordVerifyResult Result,
	const FString& JoinToken)
{
	UE_LOG(LogFAOSS, Verbose, TEXT("[BeaconAuth] Client received verification result: %s"),
		*UEnum::GetValueAsString(Result));
	CompleteVerification(Result, JoinToken);
}

void AFAOnlineBeaconClient::CompleteVerification(EFABeaconPasswordVerifyResult Result, const FString& JoinToken)
{
	if (bVerificationCompleted)
	{
		return;
	}

	bVerificationCompleted = true;
	PendingPassword.Reset();
	OnVerificationComplete.Broadcast(Result, JoinToken);
}
