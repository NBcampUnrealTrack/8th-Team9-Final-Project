// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/PrototypeMainMenu.h"

#include "OnlineSessionSettings.h"
#include "OnlineSubsystemUtils.h"
#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "Network/FAOnlineSessionSubsystem.h"
#include "UI/FASessionCreateWidget.h"
#include "UI/FASessionJoinWidget.h"

void UPrototypeMainMenu::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
	//각 버튼에 이벤트 바인딩.
	if (HostButton)
	{
		HostButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnHostButtonClicked);
	}
	if (JoinButton)
	{
		JoinButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnJoinButtonClicked);
	}
	// if (ConfirmJoinButton)
	// {
	// 	ConfirmJoinButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ConfirmJoinButtonClicked);
	// }
	if (ConfirmHostButton)
	{
		ConfirmHostButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ConfirmHostButtonClicked);
	}
	if (ReturnToMainMenuButton)
	{
		ReturnToMainMenuButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ReturnToMainMenuButtonClicked);
	}
}

void UPrototypeMainMenu::NativeConstruct()
{
	Super::NativeConstruct();
	// 서브시스템은 GameInstance로부터 가져올 수 있다.
	if (UGameInstance* GameInstance = GetGameInstance())
	{ 
		OnlineSessionSubsystem = GameInstance->GetSubsystem<UFAOnlineSessionSubsystem>();
	}
	if (OnlineSessionSubsystem)
	{
		OnlineSessionSubsystem->MultiplayerOnCreateSessionComplete.AddUniqueDynamic(this, &ThisClass::OnCreateSession);
		OnlineSessionSubsystem->MultiplayerOnFindSessionsComplete.AddUObject(this, &ThisClass::OnFindSessionComplete);
		OnlineSessionSubsystem->MultiplayerOnJoinSessionComplete.AddUObject(this, &ThisClass::OnJoinSession);
		OnlineSessionSubsystem->MultiplayerOnDestroySessionComplete.AddUniqueDynamic(this, &ThisClass::OnDestroySession);
		OnlineSessionSubsystem->MultiplayerOnStartSessionComplete.AddUniqueDynamic(this, &ThisClass::OnStartSession);
	}
}
void UPrototypeMainMenu::NativeDestruct()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{ 
		OnlineSessionSubsystem = GameInstance->GetSubsystem<UFAOnlineSessionSubsystem>();
	}
	if (OnlineSessionSubsystem)
	{
		OnlineSessionSubsystem->MultiplayerOnCreateSessionComplete.RemoveDynamic(this, &ThisClass::OnCreateSession);
		OnlineSessionSubsystem->MultiplayerOnFindSessionsComplete.RemoveAll(this);
		OnlineSessionSubsystem->MultiplayerOnJoinSessionComplete.RemoveAll(this);
		OnlineSessionSubsystem->MultiplayerOnDestroySessionComplete.RemoveDynamic(this, &ThisClass::OnDestroySession);
		OnlineSessionSubsystem->MultiplayerOnStartSessionComplete.RemoveDynamic(this, &ThisClass::OnStartSession);
	}
	
	//view에서 내릴때, MenuTearDown호출해 InputMode등 되돌림.
	MenuTearDown();
	Super::NativeDestruct();
}
void UPrototypeMainMenu::MenuSetup()
{
	AddToViewport();
	SetVisibility(ESlateVisibility::Visible);
	//bIsFocusable = true; //deprecated
	SetIsFocusable(true);
	UWorld* World = GetWorld();
	if (World)
	{
		APlayerController* PlayerController = GetOwningPlayer();
		if (PlayerController)
		{
			FInputModeUIOnly InputMode;
			// 이 위젯에 포커스 설정
			InputMode.SetWidgetToFocus(TakeWidget());
			// 마우스를 화면 락 설정, 커서가 화면 밖으로 나가는것 등을 막을때 사용
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			// InputMode적용
			PlayerController->SetInputMode(InputMode);
			PlayerController->SetShowMouseCursor(true);
		}
	}
}

void UPrototypeMainMenu::ShowPage(EPrototypeMenuPage Page)
{
	WidgetSwitcher_Page->SetActiveWidgetIndex(static_cast<int32>(Page));
}

void UPrototypeMainMenu::OnCreateSession(bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		// 성공했다면, 로비레벨을 리슨서버로 오픈
		UWorld* World = GetWorld();
		if (World)
		{
			// UWorld 소프트 참조에서 파일 경로 문자열 추출
			FString LevelPath = TargetLevelPath.ToSoftObjectPath().GetLongPackageName();

			// 리슨 서버 옵션 설정 (?listen)
			FString TravelURL = LevelPath + TEXT("?listen");
			World->ServerTravel(TravelURL);
		}
	}else
	{
		//실패 로직
	}
}

void UPrototypeMainMenu::OnFindSessionComplete(const TArray<FOnlineSessionSearchResult>& SessionResult, bool bWasSuccessful)
{
	if (!OnlineSessionSubsystem)
		return;
	
	if (!bWasSuccessful)
		return;
	
	for (FOnlineSessionSearchResult Result : SessionResult)
	{
		// STEAM AppID480에서 구분을 위한 고유 키값
		FString ProjectUniqueValue;
		Result.Session.SessionSettings.Get(FName("SPARTA_PROJECT"), ProjectUniqueValue);
		if (ProjectUniqueValue != FString("FALLING_APART"))
			continue;
		Result.Session.SessionSettings.bUseLobbiesIfAvailable = true;
		Result.Session.SessionSettings.bUsesPresence = true;
		
		SessionJoinWidget->AddSessionSearchResult(Result);
	}
}

void UPrototypeMainMenu::OnJoinSession(EOnJoinSessionCompleteResult::Type Result)
{
	//접속
	if (!OnlineSessionSubsystem)
		return;
	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		//실패시 원인 확인 후 UI피드백
		UE_LOG(LogTemp, Error, TEXT("JoinSession failed: %d"), static_cast<int32>(Result));
		return;
	}
	
	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(UObject::GetWorld());
	if (OnlineSubsystem!=nullptr)
	{
		IOnlineSessionPtr SessionInterface = OnlineSubsystem->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			// Address는 GetResolvedConnectString함수로부터 세션의 주소를 저장함.
			FString Address;
			if (SessionInterface->GetResolvedConnectString(NAME_GameSession, Address))
			{
				// PlayerController의 ClientTravel을 이용해 서버의 주소로 이동.
				APlayerController* PlayerController = GetOwningPlayer();
				if (PlayerController)
				{
					PlayerController->ClientTravel(Address, TRAVEL_Absolute);
				}
			}
		}
	}
}

void UPrototypeMainMenu::OnDestroySession(bool bWasSuccessful)
{
	
}

void UPrototypeMainMenu::OnStartSession(bool bWasSuccessful)
{
	
}


void UPrototypeMainMenu::OnHostButtonClicked() 
{
	ShowPage(EPrototypeMenuPage::CreateSession);
}

void UPrototypeMainMenu::OnJoinButtonClicked()
{
	ShowPage(EPrototypeMenuPage::JoinSession);
	// 페이지를 열떄 검색 시도.
	SessionJoinWidget->RefreshSessionList();
}

void UPrototypeMainMenu::ConfirmHostButtonClicked()
{
	if (OnlineSessionSubsystem)
		OnlineSessionSubsystem->CreateSession(
			SessionCreateWidget->GetMaxConnections(), 
			SessionCreateWidget->GetSessionName(), 
			SessionCreateWidget->GetPasswordString());
}

void UPrototypeMainMenu::ReturnToMainMenuButtonClicked()
{
	ShowPage(EPrototypeMenuPage::Main);
}

void UPrototypeMainMenu::MenuTearDown()
{
	RemoveFromParent();
	UWorld* World = GetWorld();
	if (World)
	{
		APlayerController* PlayerController = GetOwningPlayer();
		if (PlayerController)
		{
			// 메뉴가 열릴때 변경된 인풋모드를 다시 기본으로
			FInputModeGameOnly InputMode;
			PlayerController->SetInputMode(InputMode);
			PlayerController->SetShowMouseCursor(false);
		}
	}
}
