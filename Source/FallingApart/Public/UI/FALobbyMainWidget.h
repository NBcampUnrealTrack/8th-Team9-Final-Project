// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FALobbyMainWidget.generated.h"


class UButton;
class UTextBlock;
class UListView;
class UFALobbyPlayerEntryWidget;
enum class EFALobbyPhase : uint8;
/**
 * 
 */
UCLASS()
class FALLINGAPART_API UFALobbyMainWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	
	/**메뉴 열기*/
	UFUNCTION(BlueprintCallable)
	void MenuSetup();
	
	void SetReadyInteractionEnabled(bool bEnable);
	
	void ShowLoadingScreen();
	
	void InitializeLobby();
	
protected:
	virtual void NativeOnInitialized() override;
	
	virtual void NativeConstruct() override;
	
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleLobbyStateChanged();
	
	void RefreshLobby();
	
	void RefreshPlayerList(TArray<TObjectPtr<APlayerState>> PlayerArray);
		
	void RefreshInteractionState(EFALobbyPhase LobbyPhase);
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UListView> PlayerListView;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_ReadyCount;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_PlayerCount;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_Ready;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> Button_Start;
	
	UPROPERTY(EditDefaultsOnly, Category="FallingApart|WidgetClass")
	TSubclassOf<UFALobbyPlayerEntryWidget> LobbyPlayerEntryWidgetClass;
	
private:
	
	UFUNCTION()
	void OnReadybuttonClicked();
	
	UFUNCTION()
	void OnStartbuttonClicked();
	
};
