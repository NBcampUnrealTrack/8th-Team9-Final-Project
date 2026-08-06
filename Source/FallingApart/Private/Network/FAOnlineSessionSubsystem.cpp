// Fill out your copyright notice in the Description page of Project Settings.


#include "Network/FAOnlineSessionSubsystem.h"

#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Online/OnlineSessionNames.h"
#include "Runtime/Renderer/Internal/SceneTextures.h"

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
	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(UObject::GetWorld());
	if (OnlineSubsystem!=nullptr)
	{
		SessionInterface = OnlineSubsystem->GetSessionInterface();
	}
}

void UFAOnlineSessionSubsystem::Deinitialize()
{
	SessionInterface.Reset();
	Super::Deinitialize();
}

void UFAOnlineSessionSubsystem::CreateSession(int32 NumPublicConnections, const FString& ServerName, const FString& Password)
{
	if (!SessionInterface.IsValid())
		return;
	
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
	LastSessionSettings->bIsLANMatch = Online::GetSubsystem(GetWorld())->GetSubsystemName() == "NULL";
	LastSessionSettings->NumPublicConnections = NumPublicConnections;// 파라미터로 받은 값
	LastSessionSettings->bAllowJoinInProgress = true;//중도 참가 허용
	LastSessionSettings->bAllowJoinViaPresence = true;//Presence는 플레이어상태를 노출하는 기능이며, 이를 통한 접근을 허용
	LastSessionSettings->bShouldAdvertise = true;//세션을 공개하여 다른이들이 볼 수 있도록 함
	LastSessionSettings->bUsesPresence = true;//내 상태를 플랫폼에 공개할것인지 여부 (친구창에 공개 등)
	LastSessionSettings->bUseLobbiesIfAvailable = true;// 스팀 API구조 문제로, 반드시 로비 기능을 활성화해야한다고함
	
	// STEAM AppID480에서 구분을 위한 고유 키값
	LastSessionSettings->Set(FName("SPARTA_PROJECT"), FString("FALLING_APART"), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	
	// TODO : 맵 설정?
	// LastSessionSettings->Set(SETTING_MAPNAME, FString(TEXT("")), EOnlineDataAdvertisementType::ViaOnlineService);

	LastSessionSettings->Set(FName("SERVER_NAME"), ServerName, EOnlineDataAdvertisementType::ViaOnlineService);
	if (!Password.IsEmpty())
	{
		LastSessionSettings->Set(FName("SERVER_PASSWORD"), Password, EOnlineDataAdvertisementType::ViaOnlineService);
	}
	
	// 월드의 로컬 플레이어를 가져옴, 세션생서시 uniqueNetId에 사용.
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();

	//함수 시그니처에서 직접 참조(&)를 사용하기 떄문에, uniqueId와 세션세팅의 포인터를 넘겨야 함에 주의
	if(!SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, *LastSessionSettings))
	{
		// CreateSession이 실패했다면 콜백 연결을 해제.
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegateHandle);
		
		// 생성 실패를 외부에 알리기 위해 Broadcast
		MultiplayerOnCreateSessionComplete.Broadcast(false);
	}
} 

void UFAOnlineSessionSubsystem::FindSession(int32 MaxSearchResults)
{
	// 세션 검색과 접속
	if (!SessionInterface.IsValid())
		return;
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
	LastSessionSearch->bIsLanQuery = Online::GetSubsystem(GetWorld())->GetSubsystemName() == "NULL";
	//SessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals); // 5.8에서 SEARCH_PRESENCE는 폐기
	LastSessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	
	if (!SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), LastSessionSearch.ToSharedRef()))
	{
		//실패시 콜백 바인딩 제거.
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		
		// 외부에 빈 값과 false반환을 알려 실패를 알림.
		MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
	}

}

void UFAOnlineSessionSubsystem::JoinSession(const FOnlineSessionSearchResult& SessionResult, FString Password)
{
	if (!SessionInterface.IsValid())
	{
		MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
		return;
	}
	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);
	
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (!SessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, SessionResult))
	{
		// 콜백해제, 외부에 알림
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
	}
}

void UFAOnlineSessionSubsystem::DestroySession()
{
	if (!SessionInterface.IsValid())
	{
		MultiplayerOnDestroySessionComplete.Broadcast(false);
		return;
	}
	DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegate);
	if (!SessionInterface->DestroySession(NAME_GameSession))
	{
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

void UFAOnlineSessionSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	// 콜백을 해제하고, 성공을 외부에 알림.
	if (SessionInterface)
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegateHandle);
	}
	MultiplayerOnCreateSessionComplete.Broadcast(bWasSuccessful);
}

void UFAOnlineSessionSubsystem::OnFindSessionsComplete(bool bWasSuccessful)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
	}
	
	if (LastSessionSearch->SearchResults.Num() <= 0)
	{
		//아에 결과가 없을때 실패 반환할수도 있긴함. 실패, 성공(없음), 성공(있음)으로 나누는것이 좋을수도.
		MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
		return;
	}
	
	// 성공과는 별개로 로비가 없을 수 있다는점을 참고.
	MultiplayerOnFindSessionsComplete.Broadcast(LastSessionSearch->SearchResults, bWasSuccessful);
	
	// 결과를 UI에 연동하고, 플레이어는 그중에 선택하여 접속하는것이 올바름.
}

void UFAOnlineSessionSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
	}
	MultiplayerOnJoinSessionComplete.Broadcast(Result);
}

void UFAOnlineSessionSubsystem::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
	}
	// TODO : 세션 재생성 기능 추가
	MultiplayerOnDestroySessionComplete.Broadcast(bWasSuccessful);
}

void UFAOnlineSessionSubsystem::OnStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
}
