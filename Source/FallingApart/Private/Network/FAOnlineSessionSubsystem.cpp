// Fill out your copyright notice in the Description page of Project Settings.


#include "Network/FAOnlineSessionSubsystem.h"

#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Network/FAOnlineBeaconClient.h"
#include "Online/OnlineSessionNames.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogFAOSS);

namespace
{
	const FName ProjectSettingKey(TEXT("SPARTA_PROJECT")); //480 세션구분용
	const FString ProjectSettingValue(TEXT("FALLING_APART")); //480 세션구분용
	const FName NameSettingKey(TEXT("SERVER_NAME")); // 커스텀 로비 이름
	const FName HasPasswordSettingKey(TEXT("HAS_PASSWORD")); // 비밀번호는 존재여부만, 확인은 서버에서.
	const FName OwningUserNickname(TEXT("OwningUserNickname")); // 세션 정보에 호스트 닉네임 공개용(OwningUserName은 기본플랫폼닉네임임)

	TAutoConsoleVariable<float> CVarJoinTokenLifetimeSeconds(
		TEXT("FA.OSS.JoinTokenLifetimeSeconds"),
		60.0f,
		TEXT("Lifetime in seconds for one-time password-authenticated lobby join tokens. Server authoritative."),
		ECVF_Default);

	FString GetTokenLogId(const FString& Token)
	{
		return Token.Left(8);
	}


	EFASessionRequestResult ConvertJoinResult(EOnJoinSessionCompleteResult::Type Result)
	{
		switch (Result)
		{
		case EOnJoinSessionCompleteResult::Success:
			return EFASessionRequestResult::Success;
		case EOnJoinSessionCompleteResult::SessionIsFull:
			return EFASessionRequestResult::SessionIsFull;
		case EOnJoinSessionCompleteResult::SessionDoesNotExist:
			return EFASessionRequestResult::SessionDoesNotExist;
		case EOnJoinSessionCompleteResult::CouldNotRetrieveAddress:
			return EFASessionRequestResult::CouldNotRetrieveAddress;
		case EOnJoinSessionCompleteResult::AlreadyInSession:
			return EFASessionRequestResult::AlreadyInSession;
		default:
			return EFASessionRequestResult::UnknownError;
		}
	}
}

UFAOnlineSessionSubsystem::UFAOnlineSessionSubsystem():
	CreateSessionCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(
		this, &ThisClass::OnCreateSessionComplete)),
	FindSessionsCompleteDelegate(FOnFindSessionsCompleteDelegate::CreateUObject(
		this,&ThisClass::OnFindSessionsComplete)),
	JoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(
		this,&ThisClass::OnJoinSessionComplete)),
	DestroySessionCompleteDelegate(FOnDestroySessionCompleteDelegate::CreateUObject(
		this,&ThisClass::OnDestroySessionComplete)),
	StartSessionCompleteDelegate(FOnStartSessionCompleteDelegate::CreateUObject(
		this,&ThisClass::OnStartSessionComplete)),
	EndSessionCompleteDelegate(FOnEndSessionCompleteDelegate::CreateUObject(
		this, &ThisClass::HandleEndSessionForReopenSession)),
	UpdateSessionCompleteDelegate(FOnUpdateSessionCompleteDelegate::CreateUObject(
		this,&ThisClass::OnUpdateSessionComplete))
{

}

void UFAOnlineSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(UObject::GetWorld());
	if (!OnlineSubsystem)
	{
		return;
	}
	SessionInterface = OnlineSubsystem->GetSessionInterface();
	
	if (SessionInterface.IsValid())
	{
		OnSessionUserInviteAcceptedDelegateHandle = 
			SessionInterface->AddOnSessionUserInviteAcceptedDelegate_Handle(
				FOnSessionUserInviteAcceptedDelegate::CreateUObject(
					this, 
					&ThisClass::OnSessionUserInviteAccepted));
	}
}

void UFAOnlineSessionSubsystem::Deinitialize()
{
	if (SessionInterface.IsValid())
	{
		if (OnCreateSessionCompleteDelegateHandle.IsValid())
		{
			SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegateHandle);
		}
		if (FindSessionsCompleteDelegateHandle.IsValid())
		{
			SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		}
		if (JoinSessionCompleteDelegateHandle.IsValid())
		{
			SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		}
		if (DestroySessionCompleteDelegateHandle.IsValid())
		{
			SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		}
		if (StartSessionCompleteDelegateHandle.IsValid())
		{
			SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
		}
		if (UpdateSessionCompleteDelegateHandle.IsValid())
		{
			SessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionCompleteDelegateHandle);
		}
		if (EndSessionCompleteDelegateHandle.IsValid())
		{
			SessionInterface->ClearOnEndSessionCompleteDelegate_Handle(EndSessionCompleteDelegateHandle);
		}
		if (OnSessionUserInviteAcceptedDelegateHandle.IsValid())
		{
			SessionInterface->ClearOnSessionUserInviteAcceptedDelegate_Handle(OnSessionUserInviteAcceptedDelegateHandle);
		}
		SessionInterface.Reset();
	}
	OperationState = ESessionOperationState::Idle;
	UpdateSessionPurpose = EFAUpdateSessionPurpose::None;
	bReopenLobbyRequested = false;
	ClearPendingMatchContext();
	LastSessionSearch.Reset();
	LastSessionSettings.Reset();
	ClearJoinAuthorizationState();
	Super::Deinitialize();
}

void UFAOnlineSessionSubsystem::CreateSession(int32 NumPublicConnections, const FString& ServerName, const FString& Password)
{
	if (OperationState != ESessionOperationState::Idle)
	{
		MultiplayerOnCreateSessionComplete.Broadcast(EFASessionRequestResult::Busy);
		return;
	}

	if (!SessionInterface.IsValid())
	{
		MultiplayerOnCreateSessionComplete.Broadcast(EFASessionRequestResult::InvalidSessionInterface);
		return;
	}

	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld());
	const ULocalPlayer* LocalPlayer = GetWorld() ? GetWorld()->GetFirstLocalPlayerFromController() : nullptr;
	if (!OnlineSubsystem || !LocalPlayer || !LocalPlayer->GetPreferredUniqueNetId().IsValid())
	{
		MultiplayerOnCreateSessionComplete.Broadcast(EFASessionRequestResult::InvalidLocalPlayer);
		return;
	}
	
	FNamedOnlineSession* ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
	// 기존 세션 파괴와 새 세션 생성을 겹치지 않고 호출자가 먼저 이탈하도록 알린다.
	if (ExistingSession != nullptr)
	{
		MultiplayerOnCreateSessionComplete.Broadcast(EFASessionRequestResult::AlreadyInSession);
		return;
	}
	// 대리자 등록하고 핸들을 저장함. 작업후 핸들을 이용해 등록을 해제함.
	OnCreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);
	// 세션 세팅을 위한 구조체를 생성
	LastSessionSettings = MakeShareable(new FOnlineSessionSettings());
	
	// 만약 스팀연결 여부에 따라 랜매치 사용하도록 설정
	LastSessionSettings->bIsLANMatch = OnlineSubsystem->GetSubsystemName() == "NULL";
	LastSessionSettings->NumPublicConnections = NumPublicConnections;// 파라미터로 받은 값
	LastSessionSettings->bAllowJoinInProgress = true;//중도 참가 허용
	LastSessionSettings->bAllowJoinViaPresence = true;//Presence는 플레이어상태를 노출하는 기능이며, 이를 통한 접근을 허용
	LastSessionSettings->bShouldAdvertise = true;//세션을 공개하여 다른이들이 볼 수 있도록 함
	LastSessionSettings->bUsesPresence = true;//내 상태를 플랫폼에 공개할것인지 여부 (친구창에 공개 등)
	LastSessionSettings->bUseLobbiesIfAvailable = true;// 스팀 API구조 문제로, 반드시 로비 기능을 활성화해야한다고함
	
	//초대 기능 허용
	LastSessionSettings->bAllowInvites = true;
	
	// STEAM AppID 480에서 구분을 위한 고유 키값
	LastSessionSettings->Set(ProjectSettingKey, ProjectSettingValue, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	
	// 검색 결과가 처음부터 로비 맵을 나타내도록 기본 맵 패키지명을 광고한다.
	LastSessionSettings->Set(
		SETTING_MAPNAME,
		DefaultLobbyMapPackageName.ToString(),
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	//로비인지 게임인지를 구분하는 값 - 게임 진행중인지? 대기중인지
	LastSessionSettings->Set(
		FName(TEXT("SESSION_STATE")),
		FString(TEXT("Lobby")),
		EOnlineDataAdvertisementType::ViaOnlineService);

	LastSessionSettings->Set(NameSettingKey, ServerName, EOnlineDataAdvertisementType::ViaOnlineService);
	
	//호스트 닉네임을 설정.
	LastSessionSettings->Set(OwningUserNickname, PendingNickname , EOnlineDataAdvertisementType::ViaOnlineService);

	SetHostPassword(Password);
	LastSessionSettings->Set(HasPasswordSettingKey, !Password.IsEmpty(), EOnlineDataAdvertisementType::ViaOnlineService);
	LastSessionSettings->Set(SETTING_BEACONPORT, DEFAULT_BEACON_PORT, EOnlineDataAdvertisementType::ViaOnlineService);
	UE_LOG(LogFAOSS, Log, TEXT("[SessionCreate] Requesting lobby; HasPassword=%s BeaconPort=%d MaxPlayers=%d"),
	       Password.IsEmpty() ? TEXT("false") : TEXT("true"), DEFAULT_BEACON_PORT, NumPublicConnections);

	
	OperationState = ESessionOperationState::Creating;
	if(!SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, *LastSessionSettings))
	{
		OperationState = ESessionOperationState::Idle;
		
		// CreateSession이 실패했다면 콜백 연결을 해제.
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegateHandle);
		OnCreateSessionCompleteDelegateHandle.Reset();
		ClearHostPassword();
		
		// 생성 실패를 외부에 알리기 위해 Broadcast
		MultiplayerOnCreateSessionComplete.Broadcast(EFASessionRequestResult::RequestFailed);
	}
} 

void UFAOnlineSessionSubsystem::FindSession(int32 MaxSearchResults)
{
	if (OperationState != ESessionOperationState::Idle)
	{
		MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), EFASessionRequestResult::Busy);
		return;
	}

	// 세션 검색과 접속
	if (!SessionInterface.IsValid())
	{
		MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), EFASessionRequestResult::InvalidSessionInterface);
		return;
	}

	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld());
	const ULocalPlayer* LocalPlayer = GetWorld() ? GetWorld()->GetFirstLocalPlayerFromController() : nullptr;
	if (!OnlineSubsystem || !LocalPlayer || !LocalPlayer->GetPreferredUniqueNetId().IsValid())
	{
		MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), EFASessionRequestResult::InvalidLocalPlayer);
		return;
	}

	FNamedOnlineSession* ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
	// 참가 중인 세션을 비동기로 파괴하면서 검색을 겹쳐 실행하지 않는다.
	if (ExistingSession != nullptr)
	{
		MultiplayerOnFindSessionsComplete.Broadcast(
			TArray<FOnlineSessionSearchResult>(),
			EFASessionRequestResult::AlreadyInSession);
		return;
	}
	// 대리자 등록
	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);
	
	// 검색 조건 설정, 검색결과는 SessionSearch포인터에 즉시 저장되므로 맴버변수로 사용해야함.
	LastSessionSearch = MakeShareable(new FOnlineSessionSearch());
	LastSessionSearch->MaxSearchResults = MaxSearchResults;
	LastSessionSearch->bIsLanQuery = OnlineSubsystem->GetSubsystemName() == "NULL";
	//SessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals); // 5.8에서 SEARCH_PRESENCE는 폐기
	LastSessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	
	OperationState = ESessionOperationState::Searching;
	if (!SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), LastSessionSearch.ToSharedRef()))
	{
		OperationState = ESessionOperationState::Idle;
		//실패시 콜백 바인딩 제거.
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		
		// 외부에 빈 값과 false반환을 알려 실패를 알림.
		MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), EFASessionRequestResult::RequestFailed);
	}

}

void UFAOnlineSessionSubsystem::JoinSession(const FOnlineSessionSearchResult& SessionResult)
{
	if (OperationState != ESessionOperationState::Idle)
	{
		ClearClientJoinAuthorization();
		MultiplayerOnJoinSessionComplete.Broadcast(EFASessionRequestResult::Busy, FString());
		return;
	}

	if (!SessionInterface.IsValid())
	{
		ClearClientJoinAuthorization();
		MultiplayerOnJoinSessionComplete.Broadcast(EFASessionRequestResult::InvalidSessionInterface, FString());
		return;
	}
	if (!SessionResult.IsValid())
	{
		ClearClientJoinAuthorization();
		MultiplayerOnJoinSessionComplete.Broadcast(EFASessionRequestResult::InvalidSessionResult, FString());
		return;
	}

	const ULocalPlayer* LocalPlayer = GetWorld() ? GetWorld()->GetFirstLocalPlayerFromController() : nullptr;
	if (!LocalPlayer || !LocalPlayer->GetPreferredUniqueNetId().IsValid())
	{
		ClearClientJoinAuthorization();
		MultiplayerOnJoinSessionComplete.Broadcast(EFASessionRequestResult::InvalidLocalPlayer, FString());
		return;
	}
	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);

	OperationState = ESessionOperationState::Joining;
	if (!SessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, SessionResult))
	{
		OperationState = ESessionOperationState::Idle;
		// 콜백해제, 외부에 알림
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		JoinSessionCompleteDelegateHandle.Reset();
		ClearClientJoinAuthorization();
		MultiplayerOnJoinSessionComplete.Broadcast(EFASessionRequestResult::RequestFailed, FString());
	}
}

void UFAOnlineSessionSubsystem::PrepareSessionForMatch(FName MapId, int32 ExpectedPlayerCount)
{
	// 유효한 유휴 세션에서만 매치용 Update 요청을 시작한다.
	if (OperationState != ESessionOperationState::Idle
		|| !SessionInterface.IsValid()
		|| !LastSessionSettings.IsValid()
		|| !SessionInterface->GetNamedSession(NAME_GameSession)
		|| MapId.IsNone()
		|| ExpectedPlayerCount <= 0)
	{
		ClearPendingMatchContext();
		OnSessionPreparedForMatch.Broadcast(false);
		return;
	}

	SetPendingMatchContext(MapId, ExpectedPlayerCount);
	bReopenLobbyRequested = false;
	OperationState = ESessionOperationState::Updating;
	UpdateSessionPurpose = EFAUpdateSessionPurpose::PrepareMatch;
	
	LastSessionSettings->bAllowJoinInProgress = false;
	LastSessionSettings->bAllowJoinViaPresence = false;
	LastSessionSettings->bAllowInvites = false;
	
	LastSessionSettings->Set(
		SETTING_MAPNAME,MapId.ToString(), EOnlineDataAdvertisementType::ViaOnlineService);
	LastSessionSettings->Set(
		FName(TEXT("SESSION_STATE")), FString(TEXT("Playing")),	EOnlineDataAdvertisementType::ViaOnlineService);
	
	UpdateSessionCompleteDelegateHandle = SessionInterface->AddOnUpdateSessionCompleteDelegate_Handle(UpdateSessionCompleteDelegate);
	
	if (!SessionInterface->UpdateSession(NAME_GameSession, *LastSessionSettings))
	{
		SessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionCompleteDelegateHandle);
		OperationState = ESessionOperationState::Idle;
		UpdateSessionPurpose = EFAUpdateSessionPurpose::None;
		ClearPendingMatchContext();
		OnSessionPreparedForMatch.Broadcast(false);
	}
}

void UFAOnlineSessionSubsystem::DestroySession()
{
	if (OperationState != ESessionOperationState::Idle)
	{
		MultiplayerOnDestroySessionComplete.Broadcast(false);
		return;
	}

	if (!SessionInterface.IsValid())
	{
		MultiplayerOnDestroySessionComplete.Broadcast(false);
		return;
	}
	OperationState = ESessionOperationState::Destroying;
	DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegate);
	if (!SessionInterface->DestroySession(NAME_GameSession))
	{
		OperationState = ESessionOperationState::Idle;
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		MultiplayerOnDestroySessionComplete.Broadcast(false);
	}
}

void UFAOnlineSessionSubsystem::StartSession()
{
	// Update 완료 뒤 세션을 InProgress로 전환하고 완료 시 매치 이동을 허용한다.
	OperationState = ESessionOperationState::Starting;
	StartSessionCompleteDelegateHandle = SessionInterface->AddOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegate);
	if (!SessionInterface->StartSession(NAME_GameSession))
	{
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
		OperationState = ESessionOperationState::Idle;

		// Start 요청 직전에 들어온 롤백이 있다면 준비 실패 대신 로비 복구를 계속한다.
		if (bReopenLobbyRequested)
		{
			ReopenSessionForLobby();
			return;
		}

		ClearPendingMatchContext();
		OnSessionPreparedForMatch.Broadcast(false);
	}

}

void UFAOnlineSessionSubsystem::FindFriendSession()
{
	//SessionInterface->FindFriendSession()
}

void UFAOnlineSessionSubsystem::ReopenSessionForLobby()
{
	// 매치 컨텍스트는 롤백 요청 시점부터 더 이상 소비되지 않도록 즉시 폐기한다.
	ClearPendingMatchContext();

	if (!SessionInterface.IsValid())
	{
		CompleteReopenSessionForLobby(false);
		return;
	}

	// Prepare Update 또는 Start가 끝날 때까지 복구 요청을 보류해 OSS 비동기 호출을 직렬화한다.
	if ((OperationState == ESessionOperationState::Updating
			&& UpdateSessionPurpose == EFAUpdateSessionPurpose::PrepareMatch)
		|| OperationState == ESessionOperationState::Starting)
	{
		bReopenLobbyRequested = true;
		return;
	}

	// 이미 End 또는 Restore Update가 진행 중이면 기존 요청의 완료 결과를 기다린다.
	if (OperationState == ESessionOperationState::Ending
		|| (OperationState == ESessionOperationState::Updating
			&& UpdateSessionPurpose == EFAUpdateSessionPurpose::RestoreLobby))
	{
		bReopenLobbyRequested = true;
		return;
	}

	// 로비 복구 이외의 OSS 작업과 충돌하는 요청은 실패로 종료한다.
	if (OperationState != ESessionOperationState::Idle)
	{
		bReopenLobbyRequested = false;
		OnSessionReopenedForLobby.Broadcast(false);
		return;
	}

	bReopenLobbyRequested = true;
	FNamedOnlineSession* CurrentNamedSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (!CurrentNamedSession)
	{
		CompleteReopenSessionForLobby(false);
		return;
	}

	// InProgress 세션은 먼저 End한 뒤 로비 설정으로 Update한다.
	if (CurrentNamedSession->SessionState == EOnlineSessionState::InProgress)
	{
		OperationState = ESessionOperationState::Ending;
		EndSessionCompleteDelegateHandle = SessionInterface->AddOnEndSessionCompleteDelegate_Handle(EndSessionCompleteDelegate);
		if (!SessionInterface->EndSession(NAME_GameSession))
		{
			SessionInterface->ClearOnEndSessionCompleteDelegate_Handle(EndSessionCompleteDelegateHandle);
			CompleteReopenSessionForLobby(false);
		}
		return;
	}

	// Start 전 Pending 또는 Start 실패 후 Ended 상태는 End 없이 설정만 복구한다.
	RestoreLobbySetting();
}

void UFAOnlineSessionSubsystem::HandleEndSessionForReopenSession(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface
			->ClearOnEndSessionCompleteDelegate_Handle(
				EndSessionCompleteDelegateHandle);
	}

	if (!bWasSuccessful)
	{
		CompleteReopenSessionForLobby(false);
		return;
	}

	RestoreLobbySetting();
}

void UFAOnlineSessionSubsystem::RestoreLobbySetting()
{
	if (!SessionInterface.IsValid()
		|| !LastSessionSettings.IsValid()
		|| !SessionInterface->GetNamedSession(NAME_GameSession))
	{
		CompleteReopenSessionForLobby(false);
		return;
	}

	// 참가 허용, 로비 맵, 공개 상태를 한 번의 UpdateSession 요청으로 원상복구한다.
	OperationState = ESessionOperationState::Updating;
	UpdateSessionPurpose = EFAUpdateSessionPurpose::RestoreLobby;
	
	LastSessionSettings->bAllowJoinInProgress = true;
	LastSessionSettings->bAllowJoinViaPresence = true;

	LastSessionSettings->Set(
		SETTING_MAPNAME,
		DefaultLobbyMapPackageName.ToString(),
		EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	LastSessionSettings->Set(
		FName(TEXT("SESSION_STATE")),
		FString(TEXT("Lobby")),
		EOnlineDataAdvertisementType::ViaOnlineService);
	
	UpdateSessionCompleteDelegateHandle = SessionInterface->AddOnUpdateSessionCompleteDelegate_Handle(UpdateSessionCompleteDelegate);
	if (!SessionInterface->UpdateSession(NAME_GameSession, *LastSessionSettings))
	{
		SessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionCompleteDelegateHandle);
		CompleteReopenSessionForLobby(false);
	}
}

ESessionOperationState UFAOnlineSessionSubsystem::GetCurrentOperationState() const
{
	return OperationState;
}

FNamedOnlineSession* UFAOnlineSessionSubsystem::GetCurrentSession() const
{
	if (!SessionInterface.IsValid())
	{
		return nullptr;
	}

	FNamedOnlineSession* CurrentNamedSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (CurrentNamedSession)
	{
		return CurrentNamedSession;
	}
	return nullptr;
}

FUniqueNetIdRepl UFAOnlineSessionSubsystem::GetLocalUniqueNetId()
{
	const ULocalPlayer* LocalPlayer = GetWorld() ? GetWorld()->GetFirstLocalPlayerFromController() : nullptr;
	if (LocalPlayer)
		return LocalPlayer->GetPreferredUniqueNetId();
	else
		return nullptr;
}

void UFAOnlineSessionSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	// 콜백을 해제하고, 성공을 외부에 알림.
	OperationState = ESessionOperationState::Idle;
	if (SessionInterface)
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegateHandle);
	}
	MultiplayerOnCreateSessionComplete.Broadcast(
		bWasSuccessful ? EFASessionRequestResult::Success : EFASessionRequestResult::RequestFailed);
	if (bWasSuccessful)
	{
		UE_LOG(LogFAOSS, Log, TEXT("[SessionCreate] Completed: Success=true"));
	}
	else
	{
		UE_LOG(LogFAOSS, Warning, TEXT("[SessionCreate] Completed: Success=false"));
	}
	if (bWasSuccessful == false)
	{
		ClearJoinAuthorizationState();
	}
}

void UFAOnlineSessionSubsystem::OnFindSessionsComplete(bool bWasSuccessful)
{
	OperationState = ESessionOperationState::Idle;
	if (SessionInterface)
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
	}
	
	if (!bWasSuccessful || !LastSessionSearch.IsValid())
	{
		MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), EFASessionRequestResult::RequestFailed);
		return;
	}

	TArray<FOnlineSessionSearchResult> FilteredResults;
	FilteredResults.Reserve(LastSessionSearch->SearchResults.Num());
	for (const FOnlineSessionSearchResult& SearchResult : LastSessionSearch->SearchResults)
	{
		FString ProjectValue;
		if (!SearchResult.Session.SessionSettings.Get(ProjectSettingKey, ProjectValue) || ProjectValue != ProjectSettingValue)
		{
			continue;
		}

		// 매치 진행 중인 세션은 검색 목록에서 제외해 로비에만 참가할 수 있게 한다.
		FString AdvertisedSessionState;
		if (!SearchResult.Session.SessionSettings.Get(FName(TEXT("SESSION_STATE")), AdvertisedSessionState)
			|| AdvertisedSessionState != TEXT("Lobby"))
		{
			continue;
		}

		FOnlineSessionSearchResult JoinableResult = SearchResult;
		JoinableResult.Session.SessionSettings.bUseLobbiesIfAvailable = true;
		JoinableResult.Session.SessionSettings.bUsesPresence = true;
		FilteredResults.Add(MoveTemp(JoinableResult));
	}

	const EFASessionRequestResult Result = FilteredResults.IsEmpty()
		                                       ? EFASessionRequestResult::NoResults
		                                       : EFASessionRequestResult::Success;
	MultiplayerOnFindSessionsComplete.Broadcast(FilteredResults, Result);
}

void UFAOnlineSessionSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	OperationState = ESessionOperationState::Idle;
	if (SessionInterface)
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
	}
	JoinSessionCompleteDelegateHandle.Reset();

	EFASessionRequestResult SessionResult = ConvertJoinResult(Result);
	FString ConnectString;
	if (SessionResult == EFASessionRequestResult::Success &&
		(!SessionInterface.IsValid() || !SessionInterface->GetResolvedConnectString(NAME_GameSession, ConnectString)))
	{
		SessionResult = EFASessionRequestResult::CouldNotRetrieveAddress;
	}
	if (SessionResult != EFASessionRequestResult::Success)
	{
		ClearClientJoinAuthorization();
	}

	UE_LOG(LogFAOSS, Log, TEXT("[SessionJoin] JoinSession completed: %s; HasConnectString=%s"),
	       *UEnum::GetValueAsString(SessionResult), ConnectString.IsEmpty() ? TEXT("false") : TEXT("true"));

	MultiplayerOnJoinSessionComplete.Broadcast(SessionResult, ConnectString);
}

void UFAOnlineSessionSubsystem::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	OperationState = ESessionOperationState::Idle;
	if (SessionInterface)
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
	}
	
	MultiplayerOnDestroySessionComplete.Broadcast(bWasSuccessful);

	if (bWasSuccessful)
	{
		ClearPendingMatchContext();
		ClearJoinAuthorizationState();
	}
}

void UFAOnlineSessionSubsystem::OnStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
	}
	
	OperationState = ESessionOperationState::Idle;

	// Start 진행 중 들어온 롤백 요청은 매치 준비 이벤트를 내지 않고 로비 복구로 전환한다.
	if (bReopenLobbyRequested)
	{
		ReopenSessionForLobby();
		return;
	}

	// Start 실패 시 매치 진입 컨텍스트를 폐기한다.
	if (!bWasSuccessful)
	{
		ClearPendingMatchContext();
	}
	
	OnSessionPreparedForMatch.Broadcast(bWasSuccessful);
}

void UFAOnlineSessionSubsystem::OnUpdateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface
			->ClearOnUpdateSessionCompleteDelegate_Handle(
				UpdateSessionCompleteDelegateHandle);
	}
	const EFAUpdateSessionPurpose LastUpdateSessionPurpose = UpdateSessionPurpose;
	UpdateSessionPurpose = EFAUpdateSessionPurpose::None;

	// Prepare Update 도중 롤백이 요청됐으면 성공 여부와 무관하게 Start를 건너뛰고 복구한다.
	if (LastUpdateSessionPurpose == EFAUpdateSessionPurpose::PrepareMatch
		&& bReopenLobbyRequested)
	{
		OperationState = ESessionOperationState::Idle;
		RestoreLobbySetting();
		return;
	}
	
	if (!bWasSuccessful)
	{
		OperationState = ESessionOperationState::Idle;
		if (LastUpdateSessionPurpose == EFAUpdateSessionPurpose::PrepareMatch)
		{
			ClearPendingMatchContext();
			OnSessionPreparedForMatch.Broadcast(false);
		}
		if (LastUpdateSessionPurpose == EFAUpdateSessionPurpose::RestoreLobby)
		{
			CompleteReopenSessionForLobby(false);
		}
		return;
	}
	
	if (LastUpdateSessionPurpose == EFAUpdateSessionPurpose::PrepareMatch)
	{
		StartSession();
	}
	if (LastUpdateSessionPurpose == EFAUpdateSessionPurpose::RestoreLobby)
	{
		CompleteReopenSessionForLobby(true);
	}
}

void UFAOnlineSessionSubsystem::CompleteReopenSessionForLobby(bool bWasSuccessful)
{
	// 모든 복구 종료 경로에서 내부 상태를 동일하게 정리한 뒤 결과를 한 번만 알린다.
	OperationState = ESessionOperationState::Idle;
	UpdateSessionPurpose = EFAUpdateSessionPurpose::None;
	bReopenLobbyRequested = false;
	ClearPendingMatchContext();
	OnSessionReopenedForLobby.Broadcast(bWasSuccessful);
}

void UFAOnlineSessionSubsystem::SetPendingMatchContext(FName MapId, int32 ExpectedPlayerCount)
{
	//값이 잘못들어오는 경우에 대한 대비
	PendingMatchContext.bValid = !MapId.IsNone() && ExpectedPlayerCount > 0;

	PendingMatchContext.MapId = MapId;
	PendingMatchContext.ExpectedPlayerCount =
		FMath::Max(0, ExpectedPlayerCount);
}

bool UFAOnlineSessionSubsystem::GetPendingMatchContext(FFAPendingMatchContext& OutContext) const
{
	if (!PendingMatchContext.bValid)
	{
		return false;
	}

	OutContext = PendingMatchContext;
	return true;
}


void UFAOnlineSessionSubsystem::ClearPendingMatchContext()
{
	PendingMatchContext.Reset();
}

void UFAOnlineSessionSubsystem::SetPendingNickname(const FString& Nickname)
{
	PendingNickname = Nickname;
}

const FString& UFAOnlineSessionSubsystem::GetPendingNickname() const
{
	return PendingNickname;
}

bool UFAOnlineSessionSubsystem::ValidateHostPassword(const FString& Password)
{
	return HostPassword.IsEmpty() || HostPassword == Password;
}

void UFAOnlineSessionSubsystem::SetHostPassword(const FString& Password)
{
	HostPassword = Password;
}

void UFAOnlineSessionSubsystem::ClearHostPassword()
{
	HostPassword.Empty();
}

FString UFAOnlineSessionSubsystem::IssueJoinToken(const FUniqueNetIdRepl& NetId)
{
	if (!NetId.IsValid())
	{
		UE_LOG(LogFAOSS, Warning, TEXT("[JoinToken] Refused issuance for an invalid NetId"));
		return FString();
	}
	
	RemoveExpiredJoinToken();

	// 플레이어당 유효 토큰을 하나만 유지한다. 새 인증은 이전 미사용 토큰을 폐기한다.
	for (auto It = ServerPendingJoinTokens.CreateIterator(); It; ++It)
	{
		if (It.Value().PlayerNetId == NetId)
		{
			It.RemoveCurrent();
		}
	}

	// GUID는 비밀번호 자체를 노출하지 않는 고엔트로피의 1회용 bearer token으로 사용한다.
	const FString Token =
		FGuid::NewGuid().ToString(EGuidFormats::Digits);

	FFAJoinAuthorization Authorization;
	Authorization.PlayerNetId = NetId;
	const double LifetimeSeconds = FMath::Max(
		1.0,
		static_cast<double>(CVarJoinTokenLifetimeSeconds.GetValueOnGameThread()));
	Authorization.ExpirationTime = FPlatformTime::Seconds() + LifetimeSeconds;

	ServerPendingJoinTokens.Add(Token, MoveTemp(Authorization));
	ScheduleJoinTokenCleanup();

	UE_LOG(LogFAOSS, Log, TEXT("[JoinToken] Issued TokenId=%s NetId=%s Lifetime=%.1fs Pending=%d"),
	       *GetTokenLogId(Token), *NetId.ToDebugString(), LifetimeSeconds, ServerPendingJoinTokens.Num());

	return Token;
}

bool UFAOnlineSessionSubsystem::ConsumeJoinToken(const FString& JoinToken, const FUniqueNetIdRepl& NetId)
{
	RemoveExpiredJoinToken();

	FGuid ParsedToken;
	if (!NetId.IsValid()
		|| !FGuid::ParseExact(JoinToken, EGuidFormats::Digits, ParsedToken))
	{
		UE_LOG(LogFAOSS, Warning, TEXT("[JoinToken] Rejected malformed token or invalid NetId"));
		return false;
	}

	FFAJoinAuthorization* Authorization =
		ServerPendingJoinTokens.Find(JoinToken);

	if (!Authorization)
	{
		UE_LOG(LogFAOSS, Warning, TEXT("[JoinToken] Missing or expired TokenId=%s NetId=%s"),
		       *GetTokenLogId(JoinToken), *NetId.ToDebugString());
		return false;
	}

	const bool bSamePlayer =
		Authorization->PlayerNetId == NetId;

	// 성공 여부와 관계없이 재사용을 막는다.
	ServerPendingJoinTokens.Remove(JoinToken);
	ScheduleJoinTokenCleanup();

	if (bSamePlayer)
	{
		UE_LOG(LogFAOSS, Log, TEXT("[JoinToken] Consumed TokenId=%s NetId=%s Match=true Pending=%d"),
		       *GetTokenLogId(JoinToken), *NetId.ToDebugString(), ServerPendingJoinTokens.Num());
	}
	else
	{
		UE_LOG(LogFAOSS, Warning, TEXT("[JoinToken] Consumed TokenId=%s NetId=%s Match=false Pending=%d"),
		       *GetTokenLogId(JoinToken), *NetId.ToDebugString(), ServerPendingJoinTokens.Num());
	}

	return bSamePlayer;
}

void UFAOnlineSessionSubsystem::RemoveExpiredJoinToken()
{
	const double CurrentTime = FPlatformTime::Seconds();
	int32 RemovedCount = 0;

	for (auto It = ServerPendingJoinTokens.CreateIterator();
	     It;
	     ++It)
	{
		if (It.Value().ExpirationTime <= CurrentTime)
		{
			UE_LOG(LogFAOSS, Verbose, TEXT("[JoinToken] Expired TokenId=%s NetId=%s"),
			       *GetTokenLogId(It.Key()), *It.Value().PlayerNetId.ToDebugString());
			It.RemoveCurrent();
			++RemovedCount;
		}
	}

	if (RemovedCount > 0)
	{
		UE_LOG(LogFAOSS, Log, TEXT("[JoinToken] Removed %d expired token(s); Pending=%d"),
		       RemovedCount, ServerPendingJoinTokens.Num());
	}

	ScheduleJoinTokenCleanup();
}

void UFAOnlineSessionSubsystem::CleanupActiveOnlineBeacon()
{
	if (AFAOnlineBeaconClient* Beacon = ActiveOnlineBeacon.Get())
	{
		Beacon->OnVerificationComplete.RemoveAll(this);
		Beacon->DestroyBeacon();
	}
	
	ActiveOnlineBeacon = nullptr;
}

void UFAOnlineSessionSubsystem::CompleteOnlineVerificationFailure()
{
	// InitClient실패 또는 OnFailure 중복 방지.
	if (OperationState != ESessionOperationState::Authenticating)
	{
		return;
	}
	
	// Verification실패에 대한 처리.
	CleanupActiveOnlineBeacon();

	OperationState = ESessionOperationState::Idle;
	ClearClientJoinAuthorization();

	MultiplayerOnJoinSessionComplete.Broadcast(
		EFASessionRequestResult::RequestFailed,
		FString());
}

void UFAOnlineSessionSubsystem::ClearClientJoinAuthorization()
{
	ClientPendingJoinToken.Reset();
	PendingJoinSearchResult = FOnlineSessionSearchResult();
}

void UFAOnlineSessionSubsystem::VerifyPasswordAndJoin(
	const FOnlineSessionSearchResult& SessionResult,
	const FString& Password)
{
	if (OperationState != ESessionOperationState::Idle)
	{
		MultiplayerOnJoinSessionComplete.Broadcast(
			EFASessionRequestResult::Busy,
			FString());

		return;
	}

	if (!SessionResult.IsValid())
	{
		MultiplayerOnJoinSessionComplete.Broadcast(
			EFASessionRequestResult::InvalidSessionResult,
			FString());
		return;
	}

	ClearClientJoinAuthorization();

	FString BeaconConnectString;

	if (!SessionInterface.IsValid() ||
		!SessionInterface->GetResolvedConnectString(
			SessionResult,
			NAME_BeaconPort,
			BeaconConnectString))
	{
		MultiplayerOnJoinSessionComplete.Broadcast(
			EFASessionRequestResult::CouldNotRetrieveAddress,
			FString());

		return;
	}

	UE_LOG(LogFAOSS, Log, TEXT("[BeaconAuth] Starting password verification; BeaconAddress=%s"),
	       *BeaconConnectString);

	PendingJoinSearchResult = SessionResult;
	OperationState = ESessionOperationState::Authenticating;

	UWorld* World = GetWorld();

	//임시 비컨 객체 생성하여 사용함
	ActiveOnlineBeacon =
		World
			? World->SpawnActor<AFAOnlineBeaconClient>()
			: nullptr;

	if (!ActiveOnlineBeacon)
	{
		CompleteOnlineVerificationFailure();
		return;
	}

	ActiveOnlineBeacon->OnVerificationComplete.AddUObject(
		this,
		&ThisClass::HandlePasswordVerificationComplete);
	
	//비밀번호를 저장해두면 비컨이 연결될때 자동으로 검증 시도 및 콜백에서 결과 반환함.
	ActiveOnlineBeacon->SetPasswordForVerification(Password);

	FURL BeaconURL(
		nullptr,
		*BeaconConnectString,
		TRAVEL_Absolute);

	if (!ActiveOnlineBeacon->InitClient(BeaconURL))
	{
		CompleteOnlineVerificationFailure();
	}
}

void UFAOnlineSessionSubsystem::HandlePasswordVerificationComplete(EFABeaconPasswordVerifyResult Result,
                                                                   const FString& JoinToken)
{
	// 중복 방지.
	if (OperationState != ESessionOperationState::Authenticating)
	{
		return;
	}
	
	CleanupActiveOnlineBeacon();
	
	OperationState = ESessionOperationState::Idle;

	FGuid ParsedJoinToken;
	const bool bValidSuccessToken = Result == EFABeaconPasswordVerifyResult::Success
		&& FGuid::ParseExact(JoinToken, EGuidFormats::Digits, ParsedJoinToken);
	if (!bValidSuccessToken)
	{
		const EFASessionRequestResult SessionResult =
			Result == EFABeaconPasswordVerifyResult::InvalidPassword
				? EFASessionRequestResult::InvalidPassword
				: EFASessionRequestResult::RequestFailed;

		UE_LOG(LogFAOSS, Warning, TEXT("[BeaconAuth] Verification failed: %s"),
		       *UEnum::GetValueAsString(Result));
		ClearClientJoinAuthorization();

		MultiplayerOnJoinSessionComplete.Broadcast(
			SessionResult,
			FString());

		return;
	}

	ClientPendingJoinToken = JoinToken;
	UE_LOG(LogFAOSS, Log, TEXT("[BeaconAuth] Verification succeeded; TokenId=%s"),
	       *GetTokenLogId(JoinToken));

	// 여기서 처음으로 OnlineSession에 참가한다.
	const FOnlineSessionSearchResult SearchResult = PendingJoinSearchResult;
	PendingJoinSearchResult = FOnlineSessionSearchResult();
	JoinSession(SearchResult);
}

void UFAOnlineSessionSubsystem::SaveLoginUserId(const FUniqueNetIdRepl& UserId)
{
	if (AuthenticatedMemberIds.Contains(UserId) == false)
		AuthenticatedMemberIds.Add(UserId);
}

bool UFAOnlineSessionSubsystem::IsAuthenticatedUserId(const FUniqueNetIdRepl& UserId) const
{
	return AuthenticatedMemberIds.Contains(UserId);
}

void UFAOnlineSessionSubsystem::RemoveLogoutUserId(const FUniqueNetIdRepl& UserId)
{
	if (AuthenticatedMemberIds.Contains(UserId))
		AuthenticatedMemberIds.Remove(UserId);
}

void UFAOnlineSessionSubsystem::ClearJoinAuthorizationState()
{
	HostPassword.Reset();
	ClearClientJoinAuthorization();

	ServerPendingJoinTokens.Empty();
	AuthenticatedMemberIds.Empty();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(JoinTokenCleanupTimerHandle);
	}

	CleanupActiveOnlineBeacon();
}

const FString& UFAOnlineSessionSubsystem::GetPendingJoinToken() const
{
	return ClientPendingJoinToken;
}

void UFAOnlineSessionSubsystem::ScheduleJoinTokenCleanup()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(JoinTokenCleanupTimerHandle);
	if (ServerPendingJoinTokens.IsEmpty())
	{
		return;
	}

	double EarliestExpirationTime = TNumericLimits<double>::Max();
	for (const TPair<FString, FFAJoinAuthorization>& Pair : ServerPendingJoinTokens)
	{
		EarliestExpirationTime = FMath::Min(EarliestExpirationTime, Pair.Value.ExpirationTime);
	}

	const float DelaySeconds = static_cast<float>(FMath::Max(
		0.01,
		EarliestExpirationTime - FPlatformTime::Seconds()));
	World->GetTimerManager().SetTimer(
		JoinTokenCleanupTimerHandle,
		this,
		&ThisClass::RemoveExpiredJoinToken,
		DelaySeconds,
		false);
}

void UFAOnlineSessionSubsystem::SendSessionInviteToFriends()
{
	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld());
	
	if (!OnlineSubsystem || !SessionInterface.IsValid())
	{
		UE_LOG(LogFAOSS, Warning, TEXT("Steam Subsystem is unavailable"));
		return;
	}
	
	if (!SessionInterface->GetNamedSession(NAME_GameSession))
	{
		UE_LOG(LogFAOSS, Warning, TEXT("No active session to invite friends to."));
		return;
	}
	IOnlineExternalUIPtr ExternalUI = OnlineSubsystem->GetExternalUIInterface();
	if (!ExternalUI.IsValid() ||
		!ExternalUI->ShowInviteUI(0, NAME_GameSession))
	{
		UE_LOG(LogFAOSS, Warning, TEXT("Failed to open Steam invite overlay."));
	}
}

bool UFAOnlineSessionSubsystem::HasPendingInviteJoin() const
{
	return PendingInviteJoinResult.IsValid();
}

bool UFAOnlineSessionSubsystem::ConsumePendingInviteJoin(FOnlineSessionSearchResult& OutResult)
{
	if (!PendingInviteJoinResult.IsValid())
	{
		return false;
	}
	OutResult = PendingInviteJoinResult;
	PendingInviteJoinResult = FOnlineSessionSearchResult();
	return true;
}

void UFAOnlineSessionSubsystem::ClearPendingInviteJoin()
{
	PendingInviteJoinResult = FOnlineSessionSearchResult();
}

void UFAOnlineSessionSubsystem::OnSessionUserInviteAccepted(bool bWasSuccessful, int32 ControllerId,
                                                            FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
{
	if (!bWasSuccessful || !InviteResult.IsValid())
	{
		UE_LOG(LogFAOSS, Warning,
		       TEXT("Invalid Steam invite result."));
		return;
	}

	FString ProjectValue;
	FString SessionState;
	
	//우리 프로젝트 맞는지 검증
	const bool bCorrectProject =
		InviteResult.Session.SessionSettings.Get(ProjectSettingKey,ProjectValue) &&
		ProjectValue == ProjectSettingValue;

	const bool bLobbyIsOpen =
		InviteResult.Session.SessionSettings.Get(
			FName(TEXT("SESSION_STATE")),
			SessionState) &&
		SessionState == TEXT("Lobby");

	if (!bCorrectProject || !bLobbyIsOpen)
	{
		UE_LOG(LogFAOSS, Warning,
		       TEXT("Rejected incompatible or closed Steam lobby."));
		return;
	}
	
	if (InviteResult.Session.NumOpenPublicConnections <= 0)
	{
		UE_LOG(LogFAOSS, Warning,
		       TEXT("Invite lobby is full."));
		return;
	}
	
	//현재 세션이 있다면, 동일세션인지 확인.
	if (FNamedOnlineSession* CurrentSession = GetCurrentSession())
	{
		if (CurrentSession->GetSessionIdStr() == InviteResult.GetSessionIdStr())
		{
			UE_LOG(LogFAOSS, Warning,
				   TEXT("Already in invited Session"));
			return;
		}
	}
	
	//초대 세션 정보를 저장, 
	PendingInviteJoinResult = InviteResult;
	
	UE_LOG(LogFAOSS, Log,
	       TEXT("Pending Invite Session : %s"), *InviteResult.GetSessionIdStr());
	OnPendingInviteJoinAvailable.Broadcast();
	// VerifyPasswordAndJoin(InviteResult, EnteredPassword);
}
