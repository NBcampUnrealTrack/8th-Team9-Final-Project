// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "FALobbyGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFALobbyStateChanged);

UENUM(BlueprintType)
enum class EFALobbyPhase : uint8
{
	WaitingForPlayers	UMETA(DisplayName = "플레이어 대기중"),
	WaitingForReady		UMETA(DisplayName = "준비 대기중"),
	Countdown			UMETA(DisplayName = "카운트다운"),
	LockingSession		UMETA(DisplayName = "게임 시작 중"),
	Traveling			UMETA(DisplayName = "매치로 이동 중"),
};
/**
 * 
 */
UCLASS()
class FALLINGAPART_API AFALobbyGameState : public AGameStateBase
{
	GENERATED_BODY()
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	int32 GetConnectedPlayerCount() const;
	
	int32 GetReadyPlayerCount() const;
	
	int32 GetMinPlayersToStart() const;
	
	EFALobbyPhase GetLobbyPhase() const;
	
	FName GetSelectedMapId() const;
	
	//최소인원수만 체크
	bool HasEnoughPlayers() const;
	
	bool IsAllPlayerReady() const;
	
	bool IsInteractionLocked() const;
	
	// GameMode용
	void SetLobbySnapshot(
		int32 InConnectedCount,
		int32 InReadyCount,
		int32 InMinPlayers,
		EFALobbyPhase InPhase
	);
	
	void SetSelectedMapId(FName InMapId);
	
	UPROPERTY(BlueprintAssignable)
	FFALobbyStateChanged OnLobbyStateChanged;
	
	

	
protected:
	//로비 상태 변경
	UFUNCTION()
	void OnRep_LobbyState();
	// TODO : 게임 맵 변경
	UFUNCTION()
	void OnRep_SelectedMapId();
	
private:
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState)
	int32 ConnectedPlayerCount = 1;

	UPROPERTY(ReplicatedUsing = OnRep_LobbyState)
	int32 ReadyPlayerCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_LobbyState)
	int32 MinPlayersToStart = 2;
	
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState)
	EFALobbyPhase LobbyPhase = EFALobbyPhase::WaitingForPlayers;

	UPROPERTY(ReplicatedUsing = OnRep_SelectedMapId)
	FName SelectedMapId;
	
};
