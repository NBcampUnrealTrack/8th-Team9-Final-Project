// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "PrototypeMainMenu.generated.h"


class UFAOnlineSessionSubsystem;
class UFASessionCreateWidget;
class UFASessionJoinWidget;
class UWidgetSwitcher;
class UButton;


enum class EPrototypeMenuPage : uint8
{
	Main = 0,
	CreateSession = 1,
	JoinSession = 2
};
/**
 * 
 */
UCLASS()
class FALLINGAPART_API UPrototypeMainMenu : public UUserWidget
{
	GENERATED_BODY()
public:

	/**메뉴 열기*/
	UFUNCTION(BlueprintCallable)
	void MenuSetup();
	
	void ShowPage(EPrototypeMenuPage Page);
	
	//* 로비 레벨의 경로 입력 선택 */
	UPROPERTY(EditDefaultsOnly,Category = "FallingApart|Level")
	TSoftObjectPtr<UWorld> TargetLevelPath;
protected:
	// 버튼 이벤트 바인딩
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
	// 서브시스템의 대리자에 등록할 콜백함수
	UFUNCTION()
	void OnCreateSession(bool bWasSuccessful);

	// 아래 두 함수는 다이나믹 델리게이트(블루프린트 공개)가 아니므로 UFUNCTION을 사용하지않음)
	void OnFindSessionComplete(const TArray<FOnlineSessionSearchResult>& SessionResult, bool bWasSuccessful);
	
	void OnJoinSession(EOnJoinSessionCompleteResult::Type Result);
	
	UFUNCTION()
	void OnDestroySession(bool bWasSuccessful);
	
	UFUNCTION()
	void OnStartSession(bool bWasSuccessful);
	
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> WidgetSwitcher_Page;
	
	UPROPERTY(BlueprintReadOnly,meta=(BindWidget))
	UFASessionCreateWidget* SessionCreateWidget;
	
	UPROPERTY(BlueprintReadOnly,meta=(BindWidget))
	UFASessionJoinWidget* SessionJoinWidget;
	
private:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> HostButton;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> JoinButton;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> ConfirmHostButton;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> ConfirmJoinButton;
	
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> SettingButton;
 
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> QuitButton;
	
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> ReturnToMainMenuButton;
	
	UFUNCTION()
	void OnHostButtonClicked();
	
	UFUNCTION()
	void OnJoinButtonClicked();
	
	UFUNCTION()
	void ConfirmHostButtonClicked();
	
	UFUNCTION()
	void ReturnToMainMenuButtonClicked();
	
	void MenuTearDown();
	
	// 온라인 세션을 제어하기위한 서브시스템 포인터
	// 주의사항, 싱글톤 서브시스템이기 때문에 다른 클래스 위젯등에서 작업시 Menu가 살아있으면 콜백 여기서 발생함.
	// 이점을 고려하여 작성해야함.
	// TODO : 위젯을 분리하였으니 코드 책임도 분리하기
	UFAOnlineSessionSubsystem* OnlineSessionSubsystem;
};