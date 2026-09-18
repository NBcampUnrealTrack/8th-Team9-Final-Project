// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FAMainMenuWidget.h"

#include "Audio/FAAudioSubsystem.h"
#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Engine.h"
#include "Kismet/KismetSystemLibrary.h"
#include "LoadingScreenSubsystem.h"
#include "Network/FAOnlineSessionSubsystem.h"
#include "SettingsKitMenuWidget.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "UI/FASessionCreateWidget.h"
#include "UI/FASessionJoinWidget.h"
#include "UI/FAUIMessageSubsystem.h"

#define LOCTEXT_NAMESPACE "MainMenu"

namespace FAMainMenuAudio
{
	static const FName Click(TEXT("UI_Click"));
	static const FName Back(TEXT("UI_Back"));
	static const FName Hover(TEXT("UI_Hover"));
	static const FName PopupOpen(TEXT("UI_PopupOpen"));
	static const FName PopupClose(TEXT("UI_PopupClose"));
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
		
		OnlineSessionSubsystem->OnPendingInviteJoinAvailable.RemoveAll(this);
		OnlineSessionSubsystem->OnPendingInviteJoinAvailable.AddUObject(this, &ThisClass::HandlePendingInviteJoin);
		
		//만약 이 단계에서 PendingSession이 있다면(게임 중 참가 / 참가를 통한 게임 시작) 바로 적용하도록 명시
		HandlePendingInviteJoin();
	}
	
	
}
void UFAMainMenuWidget::NativeDestruct()
{
	UnbindTravelFailureDelegates();

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
	if (Page == EPrototypeMenuPage::CreateSession && SessionCreateWidget)
	{
		SessionCreateWidget->ActivatePage();
	}
	if (Page == EPrototypeMenuPage::JoinSession && SessionJoinWidget)
	{
		SessionJoinWidget->ActivatePage();
	}
}

void UFAMainMenuWidget::ShowPageByInvite(const FOnlineSessionSearchResult& InviteResult)
{
	if (!WidgetSwitcher_Page || !SessionJoinWidget)
	{
		return;
	}
	
	WidgetSwitcher_Page->SetActiveWidgetIndex(
		static_cast<int32>(EPrototypeMenuPage::JoinSession));

	//ActivatePage는 검색 갱신있어 사용하면 초기화됨.
	SessionJoinWidget->ShowInvitedSession(InviteResult);
}

void UFAMainMenuWidget::HandleCreateSessionRequested(const FFASessionCreateRequest& Request)
{
	if (OnlineSessionSubsystem)
	{
		OnlineSessionSubsystem->SetPendingNickname(Request.Nickname);
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

void UFAMainMenuWidget::HandleJoinSessionRequested(const FOnlineSessionSearchResult& SessionResult, const FString& Password, const FString& Nickname)
{
	if (!OnlineSessionSubsystem || !SessionJoinWidget)
	{
		return;
	}

	SessionJoinWidget->SetJoinInProgress(true);
	OnlineSessionSubsystem->SetPendingNickname(Nickname);
	OnlineSessionSubsystem->VerifyPasswordAndJoin(SessionResult, Password);
	//VerifyPasswordAndJoin완료 성공시 JoinSession 요청됨.
	//OnlineSessionSubsystem->JoinSession(SessionResult);
}

void UFAMainMenuWidget::HandlePendingInviteJoin()
{
	if (!OnlineSessionSubsystem || !SessionJoinWidget)
	{
		return;
	}
	
	// 기존의 세션 상태를 확인하고, 정리되지 않았다면 소비하지않는다.
	if (OnlineSessionSubsystem->GetCurrentSession()
		|| OnlineSessionSubsystem->GetCurrentOperationState() != ESessionOperationState::Idle)
	{
		return;
	}
	
	FOnlineSessionSearchResult InviteResult;
	if (!OnlineSessionSubsystem->ConsumePendingInviteJoin(InviteResult))
	{
		return;
	}
	ShowPageByInvite(InviteResult);
	
	
}

void UFAMainMenuWidget::OnCreateSessionComplete(EFASessionRequestResult Result)
{
	if (!OnlineSessionSubsystem)
		return;
	
	if (Result == EFASessionRequestResult::Success)
	{
		UFAAudioSubsystem::PlayFASound(this, FAMainMenuAudio::Success);

		// 성공했다면, 로비레벨을 리슨서버로 오픈
		UWorld* World = GetWorld();
		if (World)
		{
			// UWorld 소프트 참조에서 파일 경로 문자열 추출
			FString LevelPath = TargetLevelPath.ToSoftObjectPath().GetLongPackageName();
			FString PendingNickname = OnlineSessionSubsystem->GetPendingNickname();
			// 만약 한글이 깨진다면, 아래 함수로 인코딩 및 서버에서 디코딩 필요. HTTP모듈 필요함.
			//FString EncodedNickname = FGenericPlatformHttp::UrlEncode(OnlineSessionSubsystem->GetPendingNickname());
			
			// 서버 URL구성(호스트는 ?listen 필수)
			const FString TravelURL = FString::Printf(
					TEXT("%s?listen?Nickname=%s"),
					*LevelPath,
					*PendingNickname);
			// 레벨을 여는 동안 메인 메뉴가 그대로 보이면 멈춘 것처럼 느껴진다.
			if (ULSKLoadingScreenSubsystem* LoadingScreen = ULSKLoadingScreenSubsystem::Get(this))
			{
				LoadingScreen->ShowWithTitle(
					NSLOCTEXT("FAMainMenu", "OpeningLobby", "방을 여는 중..."));
			}

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

	if (!OnlineSessionSubsystem)
	{
		RestoreJoinUIAfterTravelFailure(TEXT("SESSION_SUBSYSTEM_UNAVAILABLE"));
		return;
	}

	if (Result != EFASessionRequestResult::Success)
	{
		const FText ErrorMessage = Result == EFASessionRequestResult::InvalidPassword
			? LOCTEXT("InvalidSessionPassword", "비밀번호가 올바르지 않습니다.")
			: LOCTEXT("JoinSessionFailed", "선택한 방에 참가하지 못했습니다.");
		if (SessionJoinWidget)
		{
			SessionJoinWidget->ShowStatusMessage(ErrorMessage);
		}
		UFAUIMessageSubsystem::ShowMessage(
			this,
			LOCTEXT("JoinSessionErrorTitle", "접속 실패"),
			ErrorMessage);
		// 비밀번호 오류가 제일 흔하다 — 메시지를 안 보고 있어도 실패했다는 건 알아야 한다.
		UFAAudioSubsystem::PlayFASound(this, FAMainMenuAudio::Error);

		UE_LOG(LogFAOSS, Error, TEXT("Join session failed: %s"), *UEnum::GetValueAsString(Result));
		return;
	}

	const FString PendingNickname = FGenericPlatformHttp::UrlEncode(
		OnlineSessionSubsystem->GetPendingNickname());
	const FString PendingJoinToken = OnlineSessionSubsystem->GetPendingJoinToken();
	APlayerController* PlayerController = GetOwningPlayer();
	if (!PlayerController || ConnectString.IsEmpty() || PendingJoinToken.IsEmpty())
	{
		UE_LOG(LogFAOSS, Error,
			TEXT("[ClientTravelAuth] Refused travel; PlayerController=%s ConnectString=%s JoinToken=%s"),
			PlayerController ? TEXT("valid") : TEXT("null"),
			ConnectString.IsEmpty() ? TEXT("empty") : TEXT("valid"),
			PendingJoinToken.IsEmpty() ? TEXT("empty") : TEXT("valid"));
		RestoreJoinUIAfterTravelFailure(TEXT("INVALID_TRAVEL_AUTH_STATE"));
		return;
	}
	
	//URL 작성
	const FString TravelURL = FString::Printf(
		TEXT("%s?Nickname=%s?JoinToken=%s"),
		*ConnectString,
		*PendingNickname,
		*PendingJoinToken);

	BindTravelFailureDelegates();
	OnlineSessionSubsystem->ClearClientJoinAuthorization();
	UFAAudioSubsystem::PlayFASound(this, FAMainMenuAudio::Success);
	UE_LOG(LogFAOSS, Log, TEXT("[ClientTravelAuth] ClientTravel with one-time JoinToken; TokenId=%s"),
		*PendingJoinToken.Left(8));
	PlayerController->ClientTravel(TravelURL, TRAVEL_Absolute);
}

void UFAMainMenuWidget::OnTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	UE_LOG(LogFAOSS, Error, TEXT("[ClientTravelAuth] Travel failure: Type=%d Error=%s"),
		static_cast<int32>(FailureType), *ErrorString);
	RestoreJoinUIAfterTravelFailure(ErrorString);
}

void UFAMainMenuWidget::OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType,
                                         const FString& ErrorString)
{
	UE_LOG(LogFAOSS, Error, TEXT("[ClientTravelAuth] Network failure: Type=%s Error=%s"),
		ENetworkFailure::ToString(FailureType), *ErrorString);
	RestoreJoinUIAfterTravelFailure(ErrorString);
}

void UFAMainMenuWidget::BindTravelFailureDelegates()
{
	UnbindTravelFailureDelegates();
	if (!GEngine)
	{
		return;
	}

	TravelFailureDelegateHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::OnTravelFailure);
	NetworkFailureDelegateHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::OnNetworkFailure);
}

void UFAMainMenuWidget::UnbindTravelFailureDelegates()
{
	if (GEngine)
	{
		if (TravelFailureDelegateHandle.IsValid())
		{
			GEngine->OnTravelFailure().Remove(TravelFailureDelegateHandle);
		}
		if (NetworkFailureDelegateHandle.IsValid())
		{
			GEngine->OnNetworkFailure().Remove(NetworkFailureDelegateHandle);
		}
	}

	TravelFailureDelegateHandle.Reset();
	NetworkFailureDelegateHandle.Reset();
}

void UFAMainMenuWidget::RestoreJoinUIAfterTravelFailure(const FString& ErrorString)
{
	UnbindTravelFailureDelegates();

	if (SessionJoinWidget)
	{
		SessionJoinWidget->SetJoinInProgress(false);
		const bool bJoinAuthorizationRejected = ErrorString.Contains(TEXT("INVALID_JOIN_TOKEN"));
		SessionJoinWidget->ShowStatusMessage(bJoinAuthorizationRejected
			? LOCTEXT("JoinAuthorizationExpired", "Join authorization expired or was already used. Please try again.")
			: LOCTEXT("JoinTravelFailed", "Could not connect to the lobby. Please try again."));
	}
	UFAAudioSubsystem::PlayFASound(this, FAMainMenuAudio::Error);

	if (OnlineSessionSubsystem)
	{
		OnlineSessionSubsystem->ClearClientJoinAuthorization();
		if (OnlineSessionSubsystem->GetCurrentOperationState() == ESessionOperationState::Idle
			&& OnlineSessionSubsystem->GetCurrentSession())
		{
			// JoinSession 성공 뒤 ClientTravel/PreLogin이 실패하면 로컬 named session을 제거해야 재시도가 가능하다.
			OnlineSessionSubsystem->DestroySession();
		}
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
	UFAAudioSubsystem::PlayFASound(this, FAMainMenuAudio::Click);

	// 이미 떠 있으면 중복 생성하지 않는다.
	if (SettingsWidget && SettingsWidget->IsInViewport())
	{
		return;
	}

	// 일시정지 메뉴와 같은 창을 띄운다 — 설정은 한 곳에서만 관리한다.
	// WBP 를 안 지정했으면 플러그인의 C++ 임시 레이아웃이 뜬다.
	const TSubclassOf<UUserWidget> WidgetClass =
		SettingsWidgetClass ? SettingsWidgetClass : TSubclassOf<UUserWidget>(USKSettingsMenuWidget::StaticClass());

	SettingsWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), WidgetClass);
	if (SettingsWidget)
	{
		// 설정창은 자기가 닫히는 시점을 알려준다 — 자기 닫기 버튼 · ESC · 바깥 클릭이 다 이 한 줄로 모인다.
		if (USKSettingsMenuWidget* SettingsMenu = Cast<USKSettingsMenuWidget>(SettingsWidget))
		{
			SettingsMenu->OnSettingsClosed.AddUniqueDynamic(this, &ThisClass::HandleSettingsClosed);
		}

		SettingsWidget->AddToViewport(SettingsWidgetZOrder);
		UFAAudioSubsystem::PlayFASound(this, FAMainMenuAudio::PopupOpen);
	}
}

void UFAMainMenuWidget::HandleSettingsClosed()
{
	UFAAudioSubsystem::PlayFASound(this, FAMainMenuAudio::PopupClose);
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
