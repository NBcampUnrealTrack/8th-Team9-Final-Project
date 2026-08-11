// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Player/FAPlayerController.h"
#include "FALobbyPlayerController.generated.h"


class UFALobbyMainWidget;
/**
 * 
 */
UCLASS()
class FALLINGAPART_API AFALobbyPlayerController : public AFAPlayerController
{
	GENERATED_BODY()
public:
	AFALobbyPlayerController();
	
	UFUNCTION(Exec, BlueprintCallable, Category="FallingApart|Lobby")
	void SetReadyGame(bool bReady);
	
	UFUNCTION(Server, Reliable)
	void Server_SetLobbyReady(bool bReady);
	
	virtual void OnRep_PlayerState() override;
	
	virtual void PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel) override;
	
protected:
	virtual void BeginPlay() override;
	

private:
	void CreateLobbyWidget();
	
	void BindLobbyState();
	
	void RemoveLobbyWidget();
	
	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Lobby")
	TSubclassOf<UFALobbyMainWidget> LobbyWidgetClass;
	
	UPROPERTY(VisibleDefaultsOnly, Category="FallingApart|Lobby")
	TObjectPtr<UFALobbyMainWidget> LobbyWidgetInstance;
	
};
