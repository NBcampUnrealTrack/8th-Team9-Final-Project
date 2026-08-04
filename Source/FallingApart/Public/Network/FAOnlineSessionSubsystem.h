// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"

#include "FAOnlineSessionSubsystem.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnCreateSessionComplete, bool, bWasSuccessful);
// 특이사항으로 FOnlineSessionSearchResult는 UClass가 붙지 않았기 때문에(리플렉션 미등록?)블루프린트에서 연겷하려면 복잡함.
// 여기서는 구현을 C++에서 하기때문에 굳이 Dynamic(블루프린트 공걔)를 사용하지 않고 진행함.
// 아래의 EOnJoinSessionCompleteResult도 같은이유로 블루프린트와 호환되지않음
DECLARE_MULTICAST_DELEGATE_TwoParams(FMultiplayerOnFindSessionsComplete, const TArray<FOnlineSessionSearchResult>& SessionResult, bool bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_OneParam(FMultiplayerOnJoinSessionComplete, EOnJoinSessionCompleteResult::Type Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnDestroySessionComplete, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnStartSessionComplete, bool, bWasSuccessful);

/**
 * 
 * 
 */
UCLASS()
class FALLINGAPART_API UFAOnlineSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	UFAOnlineSessionSubsystem();
	
		
	// 메뉴에서 세션을 관리하기위해 호출하는 함수들
	
	/** 세션 생성 요청
	 * @param NumPublicConnections 최대 연결 플레이어 수
	 * @param MatchType 매치 타입(검색용 키워드, 확장 필요)*/
	void CreateSession(int32 NumPublicConnections, FString MatchType);
	
	/** 세션 검색 
	 * @param MaxSearchResults 최대 검색 개수*/
	void FindSession(int32 MaxSearchResults);
	
	/** 특정 세션에 접속 시도
	 * @param SessionResult FindSession()를 통해 가져온 접속할 세션정보*/
	void JoinSession(const FOnlineSessionSearchResult& SessionResult);
	
	/** 서버 : 호스팅중인 세션 종료\n
	 *  클라이언트 : 접속중인 세션에서 이탈*/
	void DestroySession();
	
	/// TODO : 플랫폼에 "게임중" 상태를 알리는 기능, 이후 플레이어의 참가를 막고 경기 상태를 변경하는데 이용함.
	void StartSession();
	
	/// TODO : 플랫폼의 친구에게 참가
	void FindFriendSession();
	
	/// TODO : 플랫폼 친구에게 세션 초대 발송
	void SendSessionInviteToFriends();
	
	// 각각 콜백 등록용
	
	FMultiplayerOnCreateSessionComplete MultiplayerOnCreateSessionComplete;
	FMultiplayerOnFindSessionsComplete MultiplayerOnFindSessionsComplete;
	FMultiplayerOnJoinSessionComplete MultiplayerOnJoinSessionComplete;
	FMultiplayerOnDestroySessionComplete MultiplayerOnDestroySessionComplete;
	FMultiplayerOnStartSessionComplete MultiplayerOnStartSessionComplete;
	
protected:
	// 온라인 세션 인터페이스에 등록될 내부 콜백 함수들.
	// 직접 호출하지 않는 함수들임.
	
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnFindSessionsComplete(bool bWasSuccessful);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void OnStartSessionComplete(FName SessionName, bool bWasSuccessful);
	
	
private:
	IOnlineSessionPtr SessionInterface;
	
	TSharedPtr<FOnlineSessionSettings> LastSessionSettings;
	TSharedPtr<FOnlineSessionSearch> LastSessionSearch;
	
	// 온라인 세션 인터페이스 대리자에 추가할 리스트
	// 멀티플레이 세션 서브시스템의 콜백함수를 등록해야함.
	// FDelegateHandle는 델리게이트 바인딩 식별을 위한 핸들 클래스로, 나중에 등록 된 함수를 해제(Unbind)할때 사용.
	
	FOnCreateSessionCompleteDelegate	CreateSessionCompleteDelegate;
	FDelegateHandle 					OnCreateSessionCompleteDelegateHandle;
	FOnFindSessionsCompleteDelegate		FindSessionsCompleteDelegate;
	FDelegateHandle 					FindSessionsCompleteDelegateHandle;
	FOnJoinSessionCompleteDelegate		JoinSessionCompleteDelegate;
	FDelegateHandle 					JoinSessionCompleteDelegateHandle;
	FOnDestroySessionCompleteDelegate	DestroySessionCompleteDelegate;
	FDelegateHandle 					DestroySessionCompleteDelegateHandle;
	FOnStartSessionCompleteDelegate		StartSessionCompleteDelegate;
	FDelegateHandle 					StartSessionCompleteDelegateHandle;
};
