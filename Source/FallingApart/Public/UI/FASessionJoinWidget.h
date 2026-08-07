// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSessionSettings.h"
#include "Blueprint/UserWidget.h"
#include "FASessionJoinWidget.generated.h"

class UEditableText;
class UFASessionEntryWidget;
class UScrollBox;
class UButton;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE(FOnSessionRefreshRequested);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSessionJoinRequested, const FOnlineSessionSearchResult& SessionResult, const FString& Password);

/**
 * 세션 참가 선택시 출력할 위젯, SessionScrollBox에서 Entry 하나 선택후 입장하는 구성
 */
UCLASS()
class FALLINGAPART_API UFASessionJoinWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual bool Initialize() override;
	
	UPROPERTY(EditDefaultsOnly, Category="FallingApart|EntryWidgetClass")
	TSubclassOf<UFASessionEntryWidget> UFASessionEntryWidgetClass;
	
	void ActivatePage();
	void SetSearchInProgress(bool bIsSearching);
	void ShowSearchResults(const TArray<FOnlineSessionSearchResult>& SessionResults);
	void ShowStatusMessage(const FText& Message);
	void SetJoinInProgress(bool bIsJoining);

	FOnSessionRefreshRequested OnSessionRefreshRequested;
	FOnSessionJoinRequested OnSessionJoinRequested;

protected:
	UFUNCTION()
	void RefreshButtonClicked();

private:
	void RequestRefresh();
	void AddSessionSearchResult(const FOnlineSessionSearchResult& Result);
	void ClearList();
	void HandleSessionEntrySelected(const FOnlineSessionSearchResult& SessionResult);
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UScrollBox> SessionScrollBox;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> RefreshButton;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UEditableText> NicknameInput;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UEditableText> PasswordInput;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> SearchStatusText;
};
