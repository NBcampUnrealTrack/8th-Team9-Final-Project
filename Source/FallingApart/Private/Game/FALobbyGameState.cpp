// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/FALobbyGameState.h"

#include "Net/UnrealNetwork.h"

void AFALobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFALobbyGameState, LobbyPhase);
	DOREPLIFETIME(AFALobbyGameState, ConnectedPlayerCount);
	DOREPLIFETIME(AFALobbyGameState, ReadyPlayerCount);
	DOREPLIFETIME(AFALobbyGameState, MinPlayersToStart);
	DOREPLIFETIME(AFALobbyGameState, SelectedMapId);
	
}

int32 AFALobbyGameState::GetConnectedPlayerCount() const
{
	return ConnectedPlayerCount;
}

int32 AFALobbyGameState::GetReadyPlayerCount() const
{
	return ReadyPlayerCount;
}

int32 AFALobbyGameState::GetMinPlayersToStart() const
{
	return MinPlayersToStart;
}

EFALobbyPhase AFALobbyGameState::GetLobbyPhase() const
{
	return LobbyPhase;
}

FName AFALobbyGameState::GetSelectedMapId() const
{
	return SelectedMapId;
}

bool AFALobbyGameState::HasEnoughPlayers() const
{
	return ConnectedPlayerCount >= MinPlayersToStart;
}

bool AFALobbyGameState::IsAllPlayerReady() const
{
	return ConnectedPlayerCount > 0 && ReadyPlayerCount == ConnectedPlayerCount;
}

bool AFALobbyGameState::IsInteractionLocked() const
{
	return LobbyPhase == EFALobbyPhase::LockingSession
		|| LobbyPhase == EFALobbyPhase::Traveling;
}

void AFALobbyGameState::SetLobbySnapshot(int32 InConnectedCount, int32 InReadyCount, int32 InMinPlayers,
	EFALobbyPhase InPhase)
{
	if (!HasAuthority())
		return;
	ConnectedPlayerCount = InConnectedCount;
	ReadyPlayerCount = InReadyCount;
	MinPlayersToStart = InMinPlayers;
	LobbyPhase = InPhase;
	
	OnRep_LobbyState();
	// 강제로 복제 실행하여 모든 클라이언트에 전파함.
	ForceNetUpdate();
}

void AFALobbyGameState::SetSelectedMapId(FName InMapId)
{
	SelectedMapId = InMapId;
}


void AFALobbyGameState::OnRep_LobbyState()
{
	OnLobbyStateChanged.Broadcast();
}

void AFALobbyGameState::OnRep_SelectedMapId()
{
	
}