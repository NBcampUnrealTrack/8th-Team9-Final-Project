// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSessionSettings.h"
#include "Blueprint/UserWidget.h"
#include "FASessionJoinWidget.generated.h"

//class UFASessionListWidget;
class UEditableText;
class UFASessionEntryWidget;
class UScrollBox;
class UButton;
/**
 * 세션 참가 선택시 출력할 위젯, FASessionListWidget에서 하나 선택후 입장하는 구성
 */
UCLASS()
class FALLINGAPART_API UFASessionJoinWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual bool Initialize() override;
	
	void AddSessionSearchResult(FOnlineSessionSearchResult& Result);
	
	void ClearList();
	
	UPROPERTY(EditDefaultsOnly, Category="FallingApart|EntryWidgetClass")
	TSubclassOf<UFASessionEntryWidget> UFASessionEntryWidgetClass;
	
	void RefreshSessionList();
protected:
	
	UFUNCTION()
	void RefreshButtonClicked();
private:
	UFUNCTION()
	void VisibilityChanged(ESlateVisibility NewVisibility);
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UScrollBox> SessionScrollBox;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> RefreshButton;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UEditableText> NicknameInput;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UEditableText> PasswordInput;
	
	//UPROPERTY(meta=(BindWidget))
	//TObjectPtr<UFASessionListWidget> SessionListWidget;
	
	FOnlineSessionSearchResult LastSelectedSession;
};
