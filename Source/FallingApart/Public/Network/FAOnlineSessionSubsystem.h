// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"

#include "FAOnlineSessionSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogFAOSS, Log, All);

// 세션 상태 관리용
UENUM(BlueprintType)
enum class ESessionOperationState : uint8
{
	Idle		UMETA(DisplayName="Idle"),
	Creating	UMETA(DisplayName="Creating"),
	Searching	UMETA(DisplayName="Searching"),
	Joining		UMETA(DisplayName="Joining"),
	Updating	UMETA(DisplayName="Updating"),
	Starting	UMETA(DisplayName="Starting"),
	Ending		UMETA(DisplayName="Ending"),
	Destroying	UMETA(DisplayName="Destroying"),
	
};

// OSS 요청 관련 메세지 정의
UENUM(BlueprintType)
enum class EFASessionRequestResult : uint8
{
	Success					UMETA(DisplayName="Success"),
	NoResults				UMETA(DisplayName="No Results"),
	Busy					UMETA(DisplayName="Busy"),
	InvalidSessionInterface UMETA(DisplayName="Invalid Session Interface"),
	InvalidLocalPlayer		UMETA(DisplayName="Invalid Local Player"),
	InvalidSessionResult	UMETA(DisplayName="Invalid Session Result"),
	InvalidPassword			UMETA(DisplayName="Invalid Password"),
	RequestFailed			UMETA(DisplayName="Request Failed"),
	SessionIsFull			UMETA(DisplayName="Session Is Full"),
	SessionDoesNotExist		UMETA(DisplayName="Session Does Not Exist"),
	AlreadyInSession		UMETA(DisplayName="Already In Session"),
	CouldNotRetrieveAddress UMETA(DisplayName="Could Not Retrieve Address"),
	UnknownError			UMETA(DisplayName="Unknown Error")
};

//업데이트 원인 분기
enum class EFAUpdateSessionPurpose : uint8
{
	None,
	PrepareMatch,
	RestoreLobby
};

// 매치 게임 이동시 플레이어 수, 맵 관리 용
USTRUCT()
struct FFAPendingMatchContext
{
	GENERATED_BODY()

	UPROPERTY()
	bool bValid = false;

	UPROPERTY()
	int32 ExpectedPlayerCount = 0;

	UPROPERTY()
	FName MapId = NAME_None;

	void Reset()
	{
		bValid = false;
		ExpectedPlayerCount = 0;
		MapId = NAME_None;
	}
};


// FOnlineSessionSearchResult는 리플렉션 타입이 아니므로 세션 결과 이벤트는 C++ 전용으로 제공한다.
DECLARE_MULTICAST_DELEGATE_OneParam(FMultiplayerOnCreateSessionComplete, EFASessionRequestResult Result);
DECLARE_MULTICAST_DELEGATE_TwoParams(FMultiplayerOnFindSessionsComplete, const TArray<FOnlineSessionSearchResult>& SessionResults, EFASessionRequestResult Result);
DECLARE_MULTICAST_DELEGATE_TwoParams(FMultiplayerOnJoinSessionComplete, EFASessionRequestResult Result, const FString& ConnectString);
DECLARE_MULTICAST_DELEGATE_OneParam(FMultiplayerOnDestroySessionComplete, bool bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_OneParam(FMultiplayerOnStartSessionComplete, bool bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSessionPreparedForMatch, bool bSuccessed);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSessionReopenedForLobby,	bool bSuccessed);
/**
 * OSS의 기본적인 생명주기를 관리하는 서브시스템
 * 세션생성, 세션 검색과 참가, 세션 시작, 세션 종료
 */
UCLASS()
class FALLINGAPART_API UFAOnlineSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	UFAOnlineSessionSubsystem();
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// 메뉴에서 세션을 관리하기위해 호출하는 함수들
	
	/** 세션 생성 요청
	 * @param NumPublicConnections 최대 연결 플레이어 수
	 * @param ServerName 공개될 세션 이름
	 * @param Password 세션 입장에 필요한 비밀번호
	 */
	void CreateSession(int32 NumPublicConnections, const FString& ServerName, const FString& Password = "");
	
	/** 세션 검색 
	 * @param MaxSearchResults 최대 검색 개수*/
	void FindSession(int32 MaxSearchResults);
	
	/** 특정 세션에 접속 시도
	 * @param SessionResult FindSession()를 통해 가져온 접속할 세션정보
	 * @param Password Oprional, 세션입장에 필요한 비밀번호
	 */
	void JoinSession(const FOnlineSessionSearchResult& SessionResult, const FString& Password);
	
	void PrepareSessionForMatch(FName MapId, int32 ExpectedPlayerCount);
	
	/**
	 * 서버 : 호스팅중인 세션 종료\n
	 *  클라이언트 : 접속중인 세션에서 이탈*/
	void DestroySession();
	
	/// TODO : 플랫폼에 "게임중" 상태를 알리는 기능, 이후 플레이어의 참가를 막고 경기 상태를 변경하는데 이용함.
	void StartSession();
	
	/// TODO : 플랫폼의 친구에게 참가
	void FindFriendSession();
	
	/// TODO : 플랫폼 친구에게 세션 초대 발송
	void SendSessionInviteToFriends();
	
	void ReopenSessionForLobby();
	
	void HandleEndSessionForReopenSession(FName SessionName, bool bWasSuccessful);
	
	void RestoreLobbySetting();
	
	ESessionOperationState GetCurrentOperationState() const;
	
	FNamedOnlineSession* GetCurrentSession() const;
	TSoftObjectPtr<UWorld> ResolveMap(FName GetSelectedMapId);

	// 각각 콜백 등록용
	
	FMultiplayerOnCreateSessionComplete MultiplayerOnCreateSessionComplete;
	FMultiplayerOnFindSessionsComplete MultiplayerOnFindSessionsComplete;
	FMultiplayerOnJoinSessionComplete MultiplayerOnJoinSessionComplete;
	FMultiplayerOnDestroySessionComplete MultiplayerOnDestroySessionComplete;
	FMultiplayerOnStartSessionComplete MultiplayerOnStartSessionComplete;
	FOnSessionPreparedForMatch OnSessionPreparedForMatch;
	FOnSessionReopenedForLobby OnSessionReopenedForLobby;

protected:
	// 온라인 세션 인터페이스에 등록될 내부 콜백 함수들.
	// 직접 호출하지 않는 함수들임.
	
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnFindSessionsComplete(bool bWasSuccessful);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void OnStartSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnUpdateSessionComplete(FName SessionName, bool bWasSuccessful);

	
private:
	void CompleteReopenSessionForLobby(bool bWasSuccessful);
	
	IOnlineSessionPtr SessionInterface;
	
	FName DefaultLobbyMapPackageName = FName("/Game/FallingApart/Level/L_LobbyLevel");
	
	bool bReopenLobbyRequested = false;
	
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
	FOnEndSessionCompleteDelegate		EndSessionCompleteDelegate;
	FDelegateHandle 					EndSessionCompleteDelegateHandle;
	FOnUpdateSessionCompleteDelegate	UpdateSessionCompleteDelegate;
	FDelegateHandle 					UpdateSessionCompleteDelegateHandle;
	
	ESessionOperationState OperationState = ESessionOperationState::Idle;
	
	EFAUpdateSessionPurpose UpdateSessionPurpose = EFAUpdateSessionPurpose::None;
	
#pragma region MatchContext
public:
	// 이동할 맵과 플레이어 수 저장
	void SetPendingMatchContext(
		FName MapId,
		int32 ExpectedPlayerCount);
	// 매치에서 플레이어수와 맵 확인
	bool GetPendingMatchContext(
		FFAPendingMatchContext& OutContext) const;
	
	// 매치에서 적용한 후 정리
	void ClearPendingMatchContext();
	
private:
	UPROPERTY(Transient)
	FFAPendingMatchContext PendingMatchContext;
	
#pragma endregion MatchContext
	
#pragma region Nickname
public:
	// 세션 생성/입장 요청 전 닉네임 저장
	void SetPendingNickname(const FString& Nickname);
	
	// 생성/입장 직전에 가져가 URL파라미터에 적용.
	const FString& GetPendingNickname() const;

private:
	FString PendingNickname;
	
#pragma endregion 
};
