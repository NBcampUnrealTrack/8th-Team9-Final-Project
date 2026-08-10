// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSessionSettings.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "FASessionEntryWidget.generated.h"


class UButton;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSessionEntrySelected, const FOnlineSessionSearchResult& SessionResult);
/**
 * 
 */
UCLASS()
class FALLINGAPART_API UFASessionEntryWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()
public:
	virtual bool Initialize() override;
	
	void SetSessionInfo(const FOnlineSessionSearchResult& Result);

	FOnSessionEntrySelected OnSessionEntrySelected;
	
protected:
	UFUNCTION()
	void OnJoinButtonClicked();
private:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> SessionTitleText;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> SessionPingText;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> CurrentConnectionText;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> SessionStateText;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_Join;
	
	FOnlineSessionSearchResult EntryData;
};
