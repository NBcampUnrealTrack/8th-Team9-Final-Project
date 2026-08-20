// Fill out your copyright notice in the Description page of Project Settings.


#include "Network/FAOnlineBeaconClient.h"

#include "Network/FAOnlineSessionSubsystem.h"

AFAOnlineBeaconClient::AFAOnlineBeaconClient()
{
}

void AFAOnlineBeaconClient::OnConnected()
{
	Super::OnConnected();
	
	// 편의상 연결과 동시에 검증 시도.
	ServerVerifyPassword(PendingPassword);
}

void AFAOnlineBeaconClient::OnFailure(EBeaconFailureReason Reason, FStringView ErrorMessage)
{
	// ??
	//Super::OnFailure(Reason, ErrorMessage);
	
	Super::OnFailure();
	
	PendingPassword.Reset();

	OnVerificationComplete.Broadcast(
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
		ClientPasswordVerificationResult(
			EFABeaconPasswordVerifyResult::InternalError,
			FString());

		return;
	}
	
	//서브시스템의 비밀번호 검증 사용
	if (!SessionSubsystem->ValidateHostPassword(Password))
	{
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
	//임시저장된 비밀번호를 제거하고 콜백에 결과와 토큰을 전달.
	PendingPassword.Reset();

	OnVerificationComplete.Broadcast(Result, JoinToken);
}
