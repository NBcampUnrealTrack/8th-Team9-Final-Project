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

	/**
	 * 설정 창 위젯 클래스. 일시정지 메뉴(`UFAPauseMenuWidget`)와 같은 창을 띄운다.
	 * 비워두면 SettingsKit 플러그인의 C++ 임시 레이아웃이 뜬다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Menu")
	TSubclassOf<UUserWidget> SettingsWidgetClass;

	/** 메인 메뉴 위에 떠야 한다. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Menu")
	int32 SettingsWidgetZOrder = 120;

protected:
	// 버튼 이벤트 바인딩
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
	// 자식 위젯의 요청을 세션 서브시스템에 전달하는 코디네이터 콜백
	void HandleCreateSessionRequested(const FFASessionCreateRequest& Request);
	void HandleSessionRefreshRequested();
	void HandleJoinSessionRequested(const FOnlineSessionSearchResult& SessionResult, const FString& Password, const FString& Nickname);

	// 세션 서브시스템 완료 콜백
	void OnCreateSessionComplete(EFASessionRequestResult Result);
	void OnFindSessionComplete(const TArray<FOnlineSessionSearchResult>& SessionResults, EFASessionRequestResult Result);
	
	void OnJoinSessionComplete(EFASessionRequestResult Result, const FString& ConnectString);
	void OnTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);
	void OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void BindTravelFailureDelegates();
	void UnbindTravelFailureDelegates();
	void RestoreJoinUIAfterTravelFailure(const FString& ErrorString);

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

	/** 설정창이 닫히기 시작할 때. 닫히는 소리를 낸다. */
	UFUNCTION()
	void HandleSettingsClosed();
	
	UFUNCTION()
	void OnQuitButtonClicked();
	
	UFUNCTION()
	void ConfirmHostButtonClicked();
	
	UFUNCTION()
	void ReturnToMainMenuButtonClicked();

	/** 버튼 전체가 공유하는 호버 소리. 표의 쿨다운(0.04초)이 목록을 빠르게 훑을 때의 연사를 막는다. */
	UFUNCTION()
	void OnButtonHovered();

	/** 버튼 하나에 클릭·호버 소리를 한 번에 물린다. null 이면 아무것도 하지 않는다. */
	void BindButtonAudio(UButton* Button);

	void CloseMenu();
	
	void RestoreGameInputMode();
	
	// 온라인 세션을 제어하기위한 서브시스템 포인터
	// 주의사항, 싱글톤 서브시스템이기 때문에 다른 클래스 위젯등에서 작업시 Menu가 살아있으면 콜백 여기서 발생함.
	// 이점을 고려하여 작성해야함.
	UPROPERTY(Transient)
	TObjectPtr<UFAOnlineSessionSubsystem> OnlineSessionSubsystem;

	/** 지금 떠 있는 설정 창. 두 번 눌러도 겹쳐 뜨지 않게 들고 있는다. */
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> SettingsWidget;

	FDelegateHandle TravelFailureDelegateHandle;
	FDelegateHandle NetworkFailureDelegateHandle;
};
