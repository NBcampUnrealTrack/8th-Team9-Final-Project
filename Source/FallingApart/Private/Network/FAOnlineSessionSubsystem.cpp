// Fill out your copyright notice in the Description page of Project Settings.


#include "Network/FAOnlineSessionSubsystem.h"

#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Online/OnlineSessionNames.h"

DEFINE_LOG_CATEGORY(LogFAOSS);

namespace
{
	const FName ProjectSettingKey(TEXT("SPARTA_PROJECT"));
	const FString ProjectSettingValue(TEXT("FALLING_APART"));
	const FName ServerNameSettingKey(TEXT("SERVER_NAME"));
	const FName ServerPasswordSettingKey(TEXT("SERVER_PASSWORD"));

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
	if (OnlineSubsystem!=nullptr)
	{
		SessionInterface = OnlineSubsystem->GetSessionInterface();
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
		SessionInterface.Reset();
	}
	OperationState = ESessionOperationState::Idle;
	UpdateSessionPurpose = EFAUpdateSessionPurpose::None;
	bReopenLobbyRequested = false;
	ClearPendingMatchContext();
	LastSessionSearch.Reset();
	LastSessionSettings.Reset();
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
	
	// STEAM AppID480에서 구분을 위한 고유 키값
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

	LastSessionSettings->Set(ServerNameSettingKey, ServerName, EOnlineDataAdvertisementType::ViaOnlineService);
	if (!Password.IsEmpty())
	{
		LastSessionSettings->Set(ServerPasswordSettingKey, Password, EOnlineDataAdvertisementType::ViaOnlineService);
	}

	//함수 시그니처에서 직접 참조(&)를 사용하기 떄문에, uniqueId와 세션세팅의 포인터를 넘겨야 함에 주의
	
	OperationState = ESessionOperationState::Creating;
	if(!SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, *LastSessionSettings))
	{
		OperationState = ESessionOperationState::Idle;
		
		// CreateSession이 실패했다면 콜백 연결을 해제.
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegateHandle);
		
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

void UFAOnlineSessionSubsystem::JoinSession(const FOnlineSessionSearchResult& SessionResult, const FString& Password)
{
	if (OperationState != ESessionOperationState::Idle)
	{
		MultiplayerOnJoinSessionComplete.Broadcast(EFASessionRequestResult::Busy, FString());
		return;
	}

	if (!SessionInterface.IsValid())
	{
		MultiplayerOnJoinSessionComplete.Broadcast(EFASessionRequestResult::InvalidSessionInterface, FString());
		return;
	}
	if (!SessionResult.IsValid())
	{
		MultiplayerOnJoinSessionComplete.Broadcast(EFASessionRequestResult::InvalidSessionResult, FString());
		return;
	}

	FString ExpectedPassword;
	if (SessionResult.Session.SessionSettings.Get(ServerPasswordSettingKey, ExpectedPassword) && ExpectedPassword != Password)
	{
		MultiplayerOnJoinSessionComplete.Broadcast(EFASessionRequestResult::InvalidPassword, FString());
		return;
	}

	const ULocalPlayer* LocalPlayer = GetWorld() ? GetWorld()->GetFirstLocalPlayerFromController() : nullptr;
	if (!LocalPlayer || !LocalPlayer->GetPreferredUniqueNetId().IsValid())
	{
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
	//TODO : 스팀 인터페이스 연동
	//SessionInterface->FindFriendSession()
}

void UFAOnlineSessionSubsystem::SendSessionInviteToFriends()
{
	//TODO : 스팀 인터페이스 연동
	//SessionInterface->SendSessionInviteToFriends()
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

void UFAOnlineSessionSubsystem::CompleteReopenSessionForLobby(bool bWasSuccessful)
{
	// 모든 복구 종료 경로에서 내부 상태를 동일하게 정리한 뒤 결과를 한 번만 알린다.
	OperationState = ESessionOperationState::Idle;
	UpdateSessionPurpose = EFAUpdateSessionPurpose::None;
	bReopenLobbyRequested = false;
	ClearPendingMatchContext();
	OnSessionReopenedForLobby.Broadcast(bWasSuccessful);
}

ESessionOperationState UFAOnlineSessionSubsystem::GetCurrentOperationState() const
{
	return OperationState;
}

FNamedOnlineSession* UFAOnlineSessionSubsystem::GetCurrentSession() const
{
	FNamedOnlineSession* CurrentNamedSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (CurrentNamedSession)
	{
		return CurrentNamedSession;
	}
	return nullptr;
}

TSoftObjectPtr<UWorld> UFAOnlineSessionSubsystem::ResolveMap(FName GetSelectedMapId)
{
	FString PathString = FString::Printf(TEXT("/Game/FallingApart/Level/%s.%s"), *GetSelectedMapId.ToString(), *GetSelectedMapId.ToString());

	FSoftObjectPath SoftPath(PathString);

	TSoftObjectPtr<UWorld> WorldSoftPtr(SoftPath);
	
	return WorldSoftPtr;
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

	EFASessionRequestResult SessionResult = ConvertJoinResult(Result);
	FString ConnectString;
	if (SessionResult == EFASessionRequestResult::Success &&
		(!SessionInterface.IsValid() || !SessionInterface->GetResolvedConnectString(NAME_GameSession, ConnectString)))
	{
		SessionResult = EFASessionRequestResult::CouldNotRetrieveAddress;
	}

	MultiplayerOnJoinSessionComplete.Broadcast(SessionResult, ConnectString);
}

void UFAOnlineSessionSubsystem::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	OperationState = ESessionOperationState::Idle;
	if (SessionInterface)
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
	}
	// TODO : 세션 재생성 기능 추가
	MultiplayerOnDestroySessionComplete.Broadcast(bWasSuccessful);
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
