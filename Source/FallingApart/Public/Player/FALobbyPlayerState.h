// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "FALobbyPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFAOnLobbyReadyChanged, bool, bIsReady);


/**
 *	로비에서의 준비상태를 관리하는 PlayerState
 */
UCLASS()
class FALLINGAPART_API AFALobbyPlayerState : public APlayerState
{
	GENERATED_BODY()
public:
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION(BlueprintPure)
	bool IsLobbyReady() const;

	/// GameMode에서 호출, PlayerController는 GameMode의 SetPlayerReady.를 사용
	void SetLobbyReady(bool bNewReady);

	void ResetLobbyState();
	
	/// bLobbyReady변경시 호출하여 콜백 호출 -> UI갱신.
	UFUNCTION()
	void OnRep_LobbyReady();

	UPROPERTY(BlueprintAssignable)
	FFAOnLobbyReadyChanged OnLobbyReadyChanged;
	
protected:

	UPROPERTY(ReplicatedUsing = OnRep_LobbyReady)
	bool bLobbyReady = false;
	
	
	
};
