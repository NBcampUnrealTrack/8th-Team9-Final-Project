// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/FALobbyPlayerController.h"
#include "UI/FALobbyMainWidget.h"
#include "Game/FALobbyGameMode.h"
#include "UI/FASessionCreateWidget.h"

AFALobbyPlayerController::AFALobbyPlayerController()
{
	bReplicates = true;
}

void AFALobbyPlayerController::SetReadyGame(bool bReady)
{
	if (IsLocalController())
	{
		// 검증은 서버가, 오류로 난 레디안했는데 서버는 했다고 판단같은 일이?
		Server_SetLobbyReady(bReady);
	}
}

void AFALobbyPlayerController::Server_SetLobbyReady_Implementation(bool bReady)
{
	if (!HasAuthority())
		return;
	

	if (AFALobbyGameMode* LobbyGameMode = GetWorld()->GetAuthGameMode<AFALobbyGameMode>())
	{
		LobbyGameMode->SetPlayerReady(this, bReady);
	}
}

void AFALobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		FInputModeGameAndUI InputModeData;
		SetInputMode(InputModeData);
		bShowMouseCursor = true;
		
		CreateLobbyWidget();
		BindLobbyState();
	}
}

void AFALobbyPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	if (IsLocalController())
	{
		BindLobbyState();
	}
}

void AFALobbyPlayerController::PreClientTravel(const FString& PendingURL, ETravelType TravelType,
	bool bIsSeamlessTravel)
{
	if (LobbyWidgetInstance)
	{
		LobbyWidgetInstance->ShowLoadingScreen();
		LobbyWidgetInstance->SetReadyInteractionEnabled(false);
		
	}
	Super::PreClientTravel(PendingURL, TravelType, bIsSeamlessTravel);
}

void AFALobbyPlayerController::CreateLobbyWidget()
{
	if (LobbyWidgetClass)
	{
		LobbyWidgetInstance = CreateWidget<UFALobbyMainWidget>(this, LobbyWidgetClass);
		if (LobbyWidgetInstance)
		{
			LobbyWidgetInstance->AddToViewport();
		}
	}
}

void AFALobbyPlayerController::BindLobbyState()
{
	// UI이벤트 바인딩
	if (LobbyWidgetInstance)
	{
		LobbyWidgetInstance->InitializeLobby();
	}
	
}

void AFALobbyPlayerController::RemoveLobbyWidget()
{
}
