// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/FALobbyPlayerState.h"

#include "Net/UnrealNetwork.h"

bool AFALobbyPlayerState::IsLobbyReady() const
{
	return bLobbyReady;
}

void AFALobbyPlayerState::SetLobbyReady(bool bNewReady)
{
	if (!ensure(HasAuthority()))
	{
		return;
	}
	bLobbyReady = bNewReady;
	OnRep_LobbyReady();
}

void AFALobbyPlayerState::ResetLobbyState()
{
	if (!HasAuthority())
		return;
	SetLobbyReady(false);
}

void AFALobbyPlayerState::OnRep_LobbyReady()
{
	OnLobbyReadyChanged.Broadcast(bLobbyReady);
}

void AFALobbyPlayerState::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFALobbyPlayerState, bLobbyReady);
}
