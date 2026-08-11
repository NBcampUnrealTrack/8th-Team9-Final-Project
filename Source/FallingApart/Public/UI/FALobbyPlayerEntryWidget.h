// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "FALobbyPlayerEntryWidget.generated.h"

class UTextBlock;
class UCheckBox;
class AFALobbyPlayerState;

/*
 *
 */
UCLASS()
class FALLINGAPART_API UFALobbyPlayerEntryWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()
	
public:
	void Setup(AFALobbyPlayerState* InPlayerState);
	
	void UnBindPlayerState();

	UFUNCTION()
	void HandleReadyChanged(bool bIsReady);
protected:
	virtual void NativeDestruct() override;
	
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_Nickname;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UCheckBox> Check_Ready;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_Ready;
private:
	UPROPERTY()
	TObjectPtr<AFALobbyPlayerState> LobbyPlayerState;
	
	
	
	
};
