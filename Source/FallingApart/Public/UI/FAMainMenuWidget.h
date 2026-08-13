// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FAMainMenuWidget.generated.h"


class UFAOnlineSessionSubsystem;
class UFASessionCreateWidget;
class UFASessionJoinWidget;
class UWidgetSwitcher;
class UButton;
struct FFASessionCreateRequest;
class FOnlineSessionSearchResult;
enum class EFASessionRequestResult : uint8;


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
class FALLINGAPART_API UFAMainMenuWidget : public UUserWidget
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
	
	// 자식 위젯의 요청을 세션 서브시스템에 전달하는 코디네이터 콜백
	void HandleCreateSessionRequested(const FFASessionCreateRequest& Request);
	void HandleSessionRefreshRequested();
	void HandleJoinSessionRequested(const FOnlineSessionSearchResult& SessionResult, const FString& Password);

	// 세션 서브시스템 완료 콜백
	void OnCreateSessionComplete(EFASessionRequestResult Result);
	void OnFindSessionComplete(const TArray<FOnlineSessionSearchResult>& SessionResults, EFASessionRequestResult Result);
	
	void OnJoinSessionComplete(EFASessionRequestResult Result, const FString& ConnectString);
	
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> WidgetSwitcher_Page;
	
	UPROPERTY(BlueprintReadOnly,meta=(BindWidget))
	TObjectPtr<UFASessionCreateWidget> SessionCreateWidget;
	
	UPROPERTY(BlueprintReadOnly,meta=(BindWidget))
	TObjectPtr<UFASessionJoinWidget> SessionJoinWidget;
	
private:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> HostButton;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> JoinButton;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> ConfirmHostButton;
	
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
	void OnSettingButtonClicked();
	
	UFUNCTION()
	void OnQuitButtonClicked();
	
	UFUNCTION()
	void ConfirmHostButtonClicked();
	
	UFUNCTION()
	void ReturnToMainMenuButtonClicked();
	
	void CloseMenu();
	
	void RestoreGameInputMode();
	
	// 온라인 세션을 제어하기위한 서브시스템 포인터
	// 주의사항, 싱글톤 서브시스템이기 때문에 다른 클래스 위젯등에서 작업시 Menu가 살아있으면 콜백 여기서 발생함.
	// 이점을 고려하여 작성해야함.
	UPROPERTY(Transient)
	TObjectPtr<UFAOnlineSessionSubsystem> OnlineSessionSubsystem;
};
