// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSessionSettings.h"
#include "Blueprint/UserWidget.h"
#include "FASessionJoinWidget.generated.h"

class UEditableText;
class UEditableTextBox;
class UFASessionEntryWidget;
class UScrollBox;
class UButton;
class UTextBlock;
class UImage;
class UTexture2D;

DECLARE_MULTICAST_DELEGATE(FOnSessionRefreshRequested);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnSessionJoinRequested, const FOnlineSessionSearchResult& SessionResult, const FString& Password, const FString& Nickname);

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
	
	/** 초대를 통해 로비 접근시 해당 방을 리스트에 올림.*/
	void ShowInvitedSession(const FOnlineSessionSearchResult& InviteResult);
	
	FString GetNickname() const;

	/** 세션에 저장된 방 제목이 검색어와 부분 일치하는지 검사한다. */
	static bool DoesSessionTitleMatchSearch(const FOnlineSessionSearchResult& SessionResult, const FString& SearchText);

	FOnSessionRefreshRequested OnSessionRefreshRequested;
	FOnSessionJoinRequested OnSessionJoinRequested;

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void RefreshButtonClicked();

	UFUNCTION()
	void HandleButtonHovered();

	UFUNCTION()
	void HandleSessionSearchTextChanged(const FText& Text);

private:
	UFUNCTION()
	void HandlePasswordVisibilityClicked();
	void SetPasswordVisible(bool bVisible);
	void RebuildFilteredSessionList();
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

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> PasswordVisibilityButton;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> PasswordVisibilityIcon;

	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Password")
	TObjectPtr<UTexture2D> PasswordVisibleIcon;

	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Password")
	TObjectPtr<UTexture2D> PasswordHiddenIcon;

	/** WBP에 없으면 런타임에 세션 목록 바로 위에 자동 생성한다. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UEditableText> SessionSearchInput;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> SearchStatusText;

	TArray<FOnlineSessionSearchResult> CachedSessionResults;
};
