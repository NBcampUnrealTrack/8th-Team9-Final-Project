// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FALobbyMainWidget.generated.h"


class UButton;
class UTextBlock;
class UListView;
class UFALobbyPlayerEntryWidget;
class AFALobbyGameState;
enum class EFALobbyPhase : uint8;

/**
 * 로비 레벨에서 사용할UI
 * 준비, 시작버튼과 시작 카운트다운 기능
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
	
	//TODO : 레벨 변경간에 레벨 지정 및 로딩화면 Async Loading Screen플러그인 또는 Persistent Level 확인해보기
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
	
	
	UPROPERTY(EditDefaultsOnly, Category="FallingApart|WidgetClass")
	TSubclassOf<UFALobbyPlayerEntryWidget> LobbyPlayerEntryWidgetClass;
	
private:
	UFUNCTION()
	void OnReadybuttonClicked();
	
	UFUNCTION()
	void OnStartbuttonClicked();
	
	void UpdateCountdownText();
	
	void StartCountdownUiTimer();
	
	void StopCountdownUiTimer();

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
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Countdown;

	UPROPERTY(Transient)
	TObjectPtr<AFALobbyGameState> BoundLobbyGameState;

	FTimerHandle CountdownUiTimerHandle;
	
};
