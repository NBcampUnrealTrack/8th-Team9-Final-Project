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
	
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	
	virtual void PostLogin(APlayerController* NewPlayer) override;
	
	virtual void Logout(AController* Exiting) override;
	
	/// PlayerController에서 준비 입력받아 처리
	void SetPlayerReady(APlayerController* NewPlayerController, bool bReady);

	
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
private:
	void RefreshLobbyState();
	
	void EvaluateLobby();
	
	bool CanStartGame(int32& OutPlayerCount, int32& OutReadyCount) const;
	
	void CommitGameStart();
	
	void HandleSessionPrepared(bool bSuccess);
	
	void TravelToSelectedMap();
	
	void RollbackGameStart();
	
	AFALobbyGameState* GetLobbyGameState() const;
	
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
	
	bool bGameStartCommitted = false;
};

