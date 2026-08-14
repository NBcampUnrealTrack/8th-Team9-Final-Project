// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FAMainMenuWidget.h"

#include "Audio/FAAudioSubsystem.h"
#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Network/FAOnlineSessionSubsystem.h"
#include "UI/FASessionCreateWidget.h"
#include "UI/FASessionJoinWidget.h"

#define LOCTEXT_NAMESPACE "PrototypeMainMenu"

namespace FAMainMenuAudio
{
	static const FName Click(TEXT("UI_Click"));
	static const FName Back(TEXT("UI_Back"));
	static const FName Hover(TEXT("UI_Hover"));
	static const FName Denied(TEXT("UI_Denied"));
	static const FName PopupOpen(TEXT("UI_PopupOpen"));
	static const FName Success(TEXT("UI_Success"));
	static const FName Error(TEXT("UI_Error"));
}

void UFAMainMenuWidget::NativeOnInitialized()
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
	if (ConfirmHostButton)
	{
		ConfirmHostButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ConfirmHostButtonClicked);
	}
	if (ReturnToMainMenuButton)
	{
		ReturnToMainMenuButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ReturnToMainMenuButtonClicked);
	}
	if (SettingButton)
	{
		SettingButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnSettingButtonClicked);
	}
	if (QuitButton)
	{
		QuitButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnQuitButtonClicked);
	}

	// 클릭 소리는 각 핸들러가 직접 낸다 — 버튼마다 성격(확정 · 뒤로 · 막힘)이 달라서다.
	// 호버는 전부 같으므로 여기서 한 번에 묶는다.
	BindButtonAudio(HostButton);
	BindButtonAudio(JoinButton);
	BindButtonAudio(ConfirmHostButton);
	BindButtonAudio(ReturnToMainMenuButton);
	BindButtonAudio(SettingButton);
	BindButtonAudio(QuitButton);

	if (SessionCreateWidget)
	{
		SessionCreateWidget->OnCreateSessionRequested.RemoveAll(this);
		SessionCreateWidget->OnCreateSessionRequested.AddUObject(this, &ThisClass::HandleCreateSessionRequested);
	}
	if (SessionJoinWidget)
	{
		SessionJoinWidget->OnSessionRefreshRequested.RemoveAll(this);
		SessionJoinWidget->OnSessionRefreshRequested.AddUObject(this, &ThisClass::HandleSessionRefreshRequested);
		SessionJoinWidget->OnSessionJoinRequested.RemoveAll(this);
		SessionJoinWidget->OnSessionJoinRequested.AddUObject(this, &ThisClass::HandleJoinSessionRequested);
	}
}

void UFAMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 메뉴 레벨에는 AFAGameState 가 없어 페이즈 기반 BGM 이 걸리지 않는다 — 여기서 직접 건다.
	// (로비 BGM 은 AFALobbyGameState 를 보고 UFAAudioSubsystem 이 알아서 켠다)
	if (UFAAudioSubsystem* Audio = UFAAudioSubsystem::Get(this))
	{
		Audio->PlayMusic(TEXT("BGM_MainMenu"));
	}

	// 서브시스템은 GameInstance로부터 가져올 수 있다.
	if (UGameInstance* GameInstance = GetGameInstance())
	{ 
		OnlineSessionSubsystem = GameInstance->GetSubsystem<UFAOnlineSessionSubsystem>();
	}
	if (OnlineSessionSubsystem)
	{
		OnlineSessionSubsystem->MultiplayerOnCreateSessionComplete.RemoveAll(this);
		OnlineSessionSubsystem->MultiplayerOnCreateSessionComplete.AddUObject(this, &ThisClass::OnCreateSessionComplete);
		OnlineSessionSubsystem->MultiplayerOnFindSessionsComplete.RemoveAll(this);
		OnlineSessionSubsystem->MultiplayerOnFindSessionsComplete.AddUObject(this, &ThisClass::OnFindSessionComplete);
		OnlineSessionSubsystem->MultiplayerOnJoinSessionComplete.RemoveAll(this);
		OnlineSessionSubsystem->MultiplayerOnJoinSessionComplete.AddUObject(this, &ThisClass::OnJoinSessionComplete);
	}
}
void UFAMainMenuWidget::NativeDestruct()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{ 
		OnlineSessionSubsystem = GameInstance->GetSubsystem<UFAOnlineSessionSubsystem>();
	}
	if (OnlineSessionSubsystem)
	{
		OnlineSessionSubsystem->MultiplayerOnCreateSessionComplete.RemoveAll(this);
		OnlineSessionSubsystem->MultiplayerOnFindSessionsComplete.RemoveAll(this);
		OnlineSessionSubsystem->MultiplayerOnJoinSessionComplete.RemoveAll(this);
	}
	RestoreGameInputMode();
	Super::NativeDestruct();
}
void UFAMainMenuWidget::MenuSetup()
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

void UFAMainMenuWidget::OnButtonHovered()
{
	UFAAudioSubsystem::PlayFASound(this, FAMainMenuAudio::Hover);
}

void UFAMainMenuWidget::BindButtonAudio(UButton* Button)
{
	if (Button)
	{
		Button->OnHovered.AddUniqueDynamic(this, &ThisClass::OnButtonHovered);
	}
}

void UFAMainMenuWidget::ShowPage(EPrototypeMenuPage Page)
{
	if (!WidgetSwitcher_Page)
	{
		return;
	}

	// 하위 페이지로 들어가는 것만 창 열리는 소리로 낸다. 메인으로 돌아가는 쪽은 뒤로 가기 버튼이
	// 이미 UI_Back 을 내므로 여기서 또 내면 두 소리가 겹친다.
	const int32 PageIndex = static_cast<int32>(Page);
	if (Page != EPrototypeMenuPage::Main && WidgetSwitcher_Page->GetActiveWidgetIndex() != PageIndex)
	{
		UFAAudioSubsystem::PlayFASound(this, FAMainMenuAudio::PopupOpen);
	}

	WidgetSwitcher_Page->SetActiveWidgetIndex(PageIndex);
	if (Page == EPrototypeMenuPage::JoinSession && SessionJoinWidget)
	{
		SessionJoinWidget->ActivatePage();
	}
}

void UFAMainMenuWidget::HandleCreateSessionRequested(const FFASessionCreateRequest& Request)
{
	if (OnlineSessionSubsystem)
	{
		OnlineSessionSubsystem->CreateSession(Request.MaxConnections, Request.ServerName, Request.Password);
	}
}

void UFAMainMenuWidget::HandleSessionRefreshRequested()
{
	if (!OnlineSessionSubsystem || !SessionJoinWidget)
	{
		if (SessionJoinWidget)
		{
			SessionJoinWidget->SetSearchInProgress(false);
			SessionJoinWidget->ShowStatusMessage(LOCTEXT("SessionSubsystemUnavailable", "Session service is unavailable."));
		}
		return;
	}

	SessionJoinWidget->SetSearchInProgress(true);
	OnlineSessionSubsystem->FindSession(100);
}

void UFAMainMenuWidget::HandleJoinSessionRequested(const FOnlineSessionSearchResult& SessionResult, const FString& Password)
{
	if (!OnlineSessionSubsystem || !SessionJoinWidget)
	{
		return;
	}

	SessionJoinWidget->SetJoinInProgress(true);
	OnlineSessionSubsystem->JoinSession(SessionResult, Password);
}

void UFAMainMenuWidget::OnCreateSessionComplete(EFASessionRequestResult Result)
{
	if (Result == EFASessionRequestResult::Success)
	{
		UFAAudioSubsystem::PlayFASound(this, FAMainMenuAudio::Success);

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
		return;
	}

	// 방 만들기 실패는 화면에 아직 표시가 없다 — 최소한 소리로는 알려야 한다.
	UFAAudioSubsystem::PlayFASound(this, FAMainMenuAudio::Error);

	UE_LOG(LogFAOSS, Warning, TEXT("Create session failed: %s"), *UEnum::GetValueAsString(Result));
}

void UFAMainMenuWidget::OnFindSessionComplete(const TArray<FOnlineSessionSearchResult>& SessionResults, EFASessionRequestResult Result)
{
	if (!SessionJoinWidget)
	{
		return;
	}

	SessionJoinWidget->SetSearchInProgress(false);
	switch (Result)
	{
	case EFASessionRequestResult::Success:
		SessionJoinWidget->ShowSearchResults(SessionResults);
		break;
	case EFASessionRequestResult::NoResults:
		SessionJoinWidget->ShowSearchResults(TArray<FOnlineSessionSearchResult>());
		SessionJoinWidget->ShowStatusMessage(LOCTEXT("NoSessionsFound", "No sessions found."));
		break;
	case EFASessionRequestResult::Busy:
		SessionJoinWidget->ShowStatusMessage(LOCTEXT("SessionOperationBusy", "Another session operation is already in progress."));
		break;
	default:
		SessionJoinWidget->ShowStatusMessage(LOCTEXT("SessionSearchFailed", "Failed to search for sessions."));
		UE_LOG(LogFAOSS, Warning, TEXT("Find sessions failed: %s"), *UEnum::GetValueAsString(Result));
		break;
	}
}

void UFAMainMenuWidget::OnJoinSessionComplete(EFASessionRequestResult Result, const FString& ConnectString)
{
	if (SessionJoinWidget)
	{
		SessionJoinWidget->SetJoinInProgress(false);
	}

	if (Result != EFASessionRequestResult::Success)
	{
		if (SessionJoinWidget)
		{
			const FText ErrorMessage = Result == EFASessionRequestResult::InvalidPassword
				? LOCTEXT("InvalidSessionPassword", "The session password is incorrect.")
				: LOCTEXT("JoinSessionFailed", "Failed to join the selected session.");
			SessionJoinWidget->ShowStatusMessage(ErrorMessage);
		}
		// 비밀번호 오류가 제일 흔하다 — 메시지를 안 보고 있어도 실패했다는 건 알아야 한다.
		UFAAudioSubsystem::PlayFASound(this, FAMainMenuAudio::Error);

		UE_LOG(LogFAOSS, Error, TEXT("Join session failed: %s"), *UEnum::GetValueAsString(Result));
		return;
	}

	UFAAudioSubsystem::PlayFASound(this, FAMainMenuAudio::Success);

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->ClientTravel(ConnectString, TRAVEL_Absolute);
	}
}


void UFAMainMenuWidget::OnHostButtonClicked()
{
	// 클릭 소리는 ShowPage() 의 열림 소리와 겹치므로 여기서 따로 내지 않는다.
	ShowPage(EPrototypeMenuPage::CreateSession);
}

void UFAMainMenuWidget::OnJoinButtonClicked()
{
	ShowPage(EPrototypeMenuPage::JoinSession);
}

void UFAMainMenuWidget::OnSettingButtonClicked()
{
	// 눌러도 아무 일 없는 버튼이라 최소한 "안 된다"는 건 들려야 한다 (일시정지 메뉴와 같은 처리).
	UFAAudioSubsystem::PlayFASound(this, FAMainMenuAudio::Denied);

	//TODO : 메뉴 UI추가 및 설정 창 이동
}

void UFAMainMenuWidget::OnQuitButtonClicked()
{
	UFAAudioSubsystem::PlayFASound(this, FAMainMenuAudio::Click);

	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	UKismetSystemLibrary::QuitGame(
		GetWorld(),
		PlayerController,
		EQuitPreference::Quit,
		false
		);
}

void UFAMainMenuWidget::ConfirmHostButtonClicked()
{
	UFAAudioSubsystem::PlayFASound(this, FAMainMenuAudio::Click);

	if (SessionCreateWidget)
	{
		SessionCreateWidget->SubmitCreateRequest();
	}
}


void UFAMainMenuWidget::ReturnToMainMenuButtonClicked()
{
	UFAAudioSubsystem::PlayFASound(this, FAMainMenuAudio::Back);

	ShowPage(EPrototypeMenuPage::Main);
}

void UFAMainMenuWidget::CloseMenu()
{
	RemoveFromParent();
}

void UFAMainMenuWidget::RestoreGameInputMode()
{
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

#undef LOCTEXT_NAMESPACE
