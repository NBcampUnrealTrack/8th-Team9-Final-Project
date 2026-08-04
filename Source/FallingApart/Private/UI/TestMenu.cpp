// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/TestMenu.h"

#include "OnlineSessionSettings.h"
#include "OnlineSubsystemUtils.h"
#include "Components/Button.h"
#include "Network/FAOnlineSessionSubsystem.h"

void UTestMenu::MenuSetup(int32 NumberOfPublicConnections, FString TypeOfMatch)
{
	NumPublicConnections = NumberOfPublicConnections;
	MatchType = TypeOfMatch;
	// 화면에 추가
	AddToViewport();
	SetVisibility(ESlateVisibility::Visible);
	//bIsFocusable = true; //deprecated
	SetIsFocusable(true);
	UWorld* World = GetWorld();
	if (World)
	{
		APlayerController* PlayerController = World->GetFirstPlayerController();
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
	// 서브시스템은 GameInstance로부터 가져올 수 있다.
	if (UGameInstance* GameInstance = GetGameInstance())
	{ 
		OnlineSessionSubsystem = GameInstance->GetSubsystem<UFAOnlineSessionSubsystem>();
	}
	if (OnlineSessionSubsystem)
	{
		OnlineSessionSubsystem->MultiplayerOnCreateSessionComplete.AddDynamic(this, &ThisClass::OnCreateSession);
		OnlineSessionSubsystem->MultiplayerOnFindSessionsComplete.AddUObject(this, &ThisClass::OnFindSession);
		OnlineSessionSubsystem->MultiplayerOnJoinSessionComplete.AddUObject(this, &ThisClass::OnJoinSession);
		OnlineSessionSubsystem->MultiplayerOnDestroySessionComplete.AddDynamic(this, &ThisClass::OnDestroySession);
		OnlineSessionSubsystem->MultiplayerOnStartSessionComplete.AddDynamic(this, &ThisClass::OnStartSession);
	}
}

bool UTestMenu::Initialize()
{
	// Initialize는 생성시 단 한번 호출됨
	// NativeConstruct는 View에 추가될때마다 호출됨.
	if (!Super::Initialize())
		return false;
	//각 버튼에 이벤트 바인딩.
	if (HostButton)
	{
		HostButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HostbuttonClicked);
	}
	if (JoinButton)
	{
		JoinButton->OnClicked.AddUniqueDynamic(this, &ThisClass::JoinbuttonClicked);
	}
	
	return true;
}

void UTestMenu::NativeDestruct()
{
	//view에서 내릴때, MenuTearDown호출해 InputMode등 되돌림.
	MenuTearDown();
	Super::NativeDestruct();
}

void UTestMenu::OnCreateSession(bool bWasSuccessful)
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

void UTestMenu::OnFindSession(const TArray<FOnlineSessionSearchResult>& SessionResult, bool bWasSuccessful)
{
	if (!OnlineSessionSubsystem)
		return;
	
	for (FOnlineSessionSearchResult Result : SessionResult)
	{
		// 찾은 세션의 설정에서 키로 값을 가져올수있음
		FString SettingsValue;
		Result.Session.SessionSettings.Get(FName("MatchType"), SettingsValue);
		
		// 이유는 알수없지만 참가 전에 세션세팅을 주입해주어야 한다고... ue5.5이상의 버그?
		Result.Session.SessionSettings.bUseLobbiesIfAvailable = true;
		Result.Session.SessionSettings.bUsesPresence = true;
		
		// 매치타입이 검색설정과 같은경우
		if (SettingsValue == MatchType)
		{
			// 서브시스템을 통해 Join
			OnlineSessionSubsystem->JoinSession(Result);
			return;
		}
	}
}

void UTestMenu::OnJoinSession(EOnJoinSessionCompleteResult::Type Result)
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
				APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
				if (PlayerController)
				{
					PlayerController->ClientTravel(Address, TRAVEL_Absolute);
				}
			}
		}
	}
}

void UTestMenu::OnDestroySession(bool bWasSuccessful)
{
	
}

void UTestMenu::OnStartSession(bool bWasSuccessful)
{
	
}


void UTestMenu::HostbuttonClicked() 
{
	if (OnlineSessionSubsystem)
	{
		OnlineSessionSubsystem->CreateSession(NumPublicConnections, MatchType);;
	}
}

void UTestMenu::JoinbuttonClicked()
{
	if (OnlineSessionSubsystem)
	{
		OnlineSessionSubsystem->FindSession(10000);
	}
}

void UTestMenu::MenuTearDown()
{
	RemoveFromParent();
	
	UWorld* World = GetWorld();
	if (World)
	{
		APlayerController* PlayerController = World->GetFirstPlayerController();
		if (PlayerController)
		{
			// 메뉴가 열릴때 변경된 인풋모드를 다시 기본으로
			FInputModeGameOnly InputMode;
			PlayerController->SetInputMode(InputMode);
			PlayerController->SetShowMouseCursor(false);
		}
	}
	
}
