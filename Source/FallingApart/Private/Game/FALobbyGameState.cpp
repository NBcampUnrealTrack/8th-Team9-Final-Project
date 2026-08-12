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
	DOREPLIFETIME(AFALobbyGameState, CountdownEndServerTime);
	
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
	EFALobbyPhase InPhase, float InCountdownEndServerTime)
{
	if (!HasAuthority())
		return;
	ConnectedPlayerCount = InConnectedCount;
	ReadyPlayerCount = InReadyCount;
	MinPlayersToStart = InMinPlayers;
	LobbyPhase = InPhase;
	
	CountdownEndServerTime = 
		InPhase == EFALobbyPhase::Countdown
		? InCountdownEndServerTime
		: 0.f;
	
	OnRep_LobbyState();
	// 강제로 복제 실행하여 모든 클라이언트에 전파함.
	ForceNetUpdate();
}

float AFALobbyGameState::GetCounddownEndServerTime() const
{
	return CountdownEndServerTime;
}

float AFALobbyGameState::GetRemainingCountdownSeconds() const
{
	if (LobbyPhase != EFALobbyPhase::Countdown)
	{
		return 0;
	}

	const float RemainingSeconds = CountdownEndServerTime - GetServerWorldTimeSeconds();

	return FMath::Max(0, FMath::CeilToInt(RemainingSeconds));
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
	//TODO : 맵 선택시 UI갱신
	
}