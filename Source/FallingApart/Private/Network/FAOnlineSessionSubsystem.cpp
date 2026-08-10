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
	StartSessionCompleteDelegate(FOnStartSessionCompleteDelegate::CreateUObject
		(this,&ThisClass::OnStartSessionComplete))
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
		SessionInterface.Reset();
	}
	OperationState = ESessionOperationState::Idle;
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
	// 세션이 이미 존재한다면
	if (ExistingSession != nullptr)
	{
		SessionInterface->DestroySession(NAME_GameSession);
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
	
	// TODO : 맵 설정?
	// LastSessionSettings->Set(SETTING_MAPNAME, FString(TEXT("")), EOnlineDataAdvertisementType::ViaOnlineService);

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
	// 세션이 이미 존재한다면
	if (ExistingSession != nullptr)
	{
		SessionInterface->DestroySession(NAME_GameSession);
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
	//TODO : StartSession기능 확인
	//SessionInterface->StartSession(NAME_GameSession);

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

ESessionOperationState UFAOnlineSessionSubsystem::GetCurrentOperationState() const
{
	return OperationState;
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
	OperationState = ESessionOperationState::Idle;
}
