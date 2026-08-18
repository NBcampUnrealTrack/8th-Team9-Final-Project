// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "FALobbyGameMode.generated.h"

/**
 *	박지석 임시 로비 시스템 흐름
 *	최소 2명 이상의 플레이가 접속 + 모든 플레이어가 준비했을때 호스트는 게임을 시작
 *	플레이어 진영 부여 등은 실제 매치가 이루어지는 GameMode에 구현을 확인하였기 때문에 추가설정 없이 Travel을 진행함.
 */
class UFAOnlineSessionSubsystem;
class AFALobbyGameState;
UCLASS()
class FALLINGAPART_API AFALobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AFALobbyGameMode();
	
	/// 멀티플레이어 레벨 입장 순서는 PreLogin-Login-InitNewPlayer-PostLogin순.
	/// PreLogin은 인원체크, 중복접속, 패스워드등 접속 허용 규칙에 적합
	/// InitNewPlayer는 로그인 처리 이후 플레이어의 정보를 초기화하는 부분으로, 이때 Replicate되기전 Controller나 PlayerState의 정보 처리에 적합.
	/// PostLogin은 초기화가 끝나고 복제가 활성화된후 호출되며, 접속 알림이나 UI연동 등에 적합함.
	
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	
	virtual void PostLogin(APlayerController* NewPlayer) override;
	
	virtual void Logout(AController* Exiting) override;
	
	
	/// PlayerController에서 준비 입력받아 처리
	void SetPlayerReady(APlayerController* NewPlayerController, bool bReady);

	
	
protected:
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal = L"") override;
	
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
private:
	/** GameState에 필요 데이터 전파. LobbyGameState->SetLobbySnapshot를 참고 */
	void RefreshLobbyState();
	/** 게임 시작이 가능한 상태인지 갱신 */
	void EvaluateLobby();
	/** 게임 시작 가능 여부를 ,최소시작가능 인원이 레디중이면 시작 가능(임시) */ 
	bool CanStartGame(int32& OutPlayerCount, int32& OutReadyCount) const;
	/** 게임 시작, 매치로 이동 */
	void CommitGameStart();
	
	void HandleSessionPrepared(bool bSuccess);
	
	void HandleSessionReopened(bool bSuccess);
	
	void TravelToSelectedMap();
	
	void RollbackGameStart();
	
	AFALobbyGameState* GetLobbyGameState() const;
	
	// 닉네임 중복 검사 등...
	FString SanitizeNickname(APlayerController* NewPlayerController, FString Nickname);
	
	void SetPlayerNickname(APlayerController* NewPlayerController, FString NewNickname);


	
	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Lobby")
	int32 MinPlayersToStart = 2;

	// 0: 최소 인원 이상인 현재 인원이 모두 Ready면 시작
	// 1 이상: 정확히 이 인원이 모여야 시작
	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Lobby")
	int32 RequiredPlayersToStart = 0;

	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Lobby")
	TSoftObjectPtr<UWorld> DestinationMap;
	
	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Lobby")
	FName DefaultMapId;
	
	UPROPERTY(Transient)
	TObjectPtr<UFAOnlineSessionSubsystem> OnlineSessionSubsystem;
	
	FDelegateHandle PreparedForMatchDelegateHandle;
	
	FDelegateHandle ReopenedForLobbyDelegateHandle;
	
	bool bGameStartCommitted = false;
	bool bRollbackInProgress = false;
	
	
#pragma region StartTimer
public:
	void RequestStartCountDown(APlayerController* RequestingController);
private:
	bool IsHostController(APlayerController* Controller);
	
	
	void StartGameCountdown();
	void CancelGameCountdown();
	void HandleGameCountdownFinished();
	bool IsCountdownActive() const;
	
	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Lobby")
	float GameStartCountdownDelayInSeconds = 5.f;
	
	FTimerHandle GameStartCountdownTimerHandle;
	
	float CountdownEndServerTime = 0.f;
	
#pragma endregion StartTimer
};

