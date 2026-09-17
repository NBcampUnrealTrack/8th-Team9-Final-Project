// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/FAUIMessageSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateUser.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "UI/FAGenericPopup.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	enum class EFAStoredInputMode : uint8
	{
		GameOnly,
		GameAndUI,
		UIOnly
	};
}

UFAUIMessageSubsystem::UFAUIMessageSubsystem()
{
	static ConstructorHelpers::FClassFinder<UFAGenericPopup> PopupFinder(
		TEXT("/Game/FallingApart/WBP/WBP_GenericPopup"));

	if (PopupFinder.Succeeded())
		UE_LOG(LogTemp, Log, TEXT("Successfully Get WBP"));
	PopupWidgetClass = PopupFinder.Succeeded()
		? PopupFinder.Class.Get()
		: UFAGenericPopup::StaticClass();
}

bool UFAUIMessageSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    return Super::ShouldCreateSubsystem(Outer);
}

void UFAUIMessageSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PostLoadMapDelegateHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this, &ThisClass::HandlePostLoadMap);
}

void UFAUIMessageSubsystem::Deinitialize()
{
	if (PostLoadMapDelegateHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapDelegateHandle);
		PostLoadMapDelegateHandle.Reset();
	}

	CloseMessageInternal();
	bHasPendingMessage = false;
	Super::Deinitialize();
}

UFAUIMessageSubsystem* UFAUIMessageSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UFAUIMessageSubsystem>() : nullptr;
}

void UFAUIMessageSubsystem::ShowMessage(const UObject* WorldContextObject, const FText& Title,
	const FText& Message, const FText& ConfirmText)
{
	if (UFAUIMessageSubsystem* Messages = Get(WorldContextObject))
	{
		Messages->ShowMessageInternal(Title, Message, ConfirmText);
	}
}

void UFAUIMessageSubsystem::ShowConfirmation(const UObject* WorldContextObject, const FText& Title,
	const FText& Message, FOnFAConfirmationResult ResultCallback, const FText& YesText, const FText& NoText)
{
	if (UFAUIMessageSubsystem* Messages = Get(WorldContextObject))
	{
		Messages->ShowConfirmationInternal(Title, Message, YesText, NoText, ResultCallback);
		return;
	}

	ResultCallback.ExecuteIfBound(false);
}

void UFAUIMessageSubsystem::ShowMessageAfterTravel(const UObject* WorldContextObject, const FText& Title,
	const FText& Message, const FText& ConfirmText)
{
	if (UFAUIMessageSubsystem* Messages = Get(WorldContextObject))
	{
		Messages->QueueMessage(Title, Message, ConfirmText);
	}
}

void UFAUIMessageSubsystem::CloseMessage(const UObject* WorldContextObject)
{
	if (UFAUIMessageSubsystem* Messages = Get(WorldContextObject))
	{
		Messages->CloseMessageInternal();
	}
}

bool UFAUIMessageSubsystem::ShowMessageInternal(const FText& Title, const FText& Message,
	const FText& ConfirmText)
{
	if (IsValid(ActivePopup) && ActivePopup->IsInViewport())
	{
		// 선택을 기다리는 확인창은 다른 알림으로 덮지 않는다.
		if (ActivePopup->IsConfirmationDialog())
		{
			UE_LOG(LogTemp, Verbose, TEXT("[FAUIMessage] 확인창 응답 대기 중이라 일반 메시지를 표시하지 않았다."));
			return false;
		}

		ActivePopup->SetupMessage(Title, Message, ConfirmText);
		return true;
	}

	ActivePopup = nullptr;
	UGameInstance* GameInstance = GetGameInstance();
	APlayerController* PlayerController = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;
	if (!PlayerController)
	{
		QueueMessage(Title, Message, ConfirmText);
		return false;
	}

	TSubclassOf<UFAGenericPopup> WidgetClass = PopupWidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = UFAGenericPopup::StaticClass();
	}

	ActivePopup = CreateWidget<UFAGenericPopup>(PlayerController, WidgetClass);
	if (!ActivePopup)
	{
		UE_LOG(LogTemp, Error, TEXT("[FAUIMessage] 메시지 위젯 생성에 실패했다."));
		return false;
	}

	ActivePopup->OnClosed.AddUObject(this, &ThisClass::HandlePopupClosed);
	ActivePopup->SetupMessage(Title, Message, ConfirmText);
	ActivePopup->AddToViewport(PopupZOrder);
	SaveAndApplyPopupInputMode(PlayerController, ActivePopup);
	return true;
}

bool UFAUIMessageSubsystem::ShowConfirmationInternal(const FText& Title, const FText& Message,
	const FText& YesText, const FText& NoText, const FOnFAConfirmationResult& ResultCallback)
{
	if (IsValid(ActivePopup) && ActivePopup->IsInViewport())
	{
		if (ActivePopup->IsConfirmationDialog())
		{
			// 기존 질문의 결과를 잃지 않도록 새 확인 요청은 실패(false)로 즉시 돌려준다.
			FOnFAConfirmationResult RejectedCallback = ResultCallback;
			RejectedCallback.ExecuteIfBound(false);
			return false;
		}

		if (ActivePopup->SupportsConfirmationLayout())
		{
			ActivePopup->SetupConfirmation(Title, Message, YesText, NoText, ResultCallback);
			return true;
		}

		// 구버전 WBP가 이미 화면에 올라간 상태라면 제거 후, 추가되기 전에 폴백 트리를 구성한다.
		CloseMessageInternal();
	}

	UGameInstance* GameInstance = GetGameInstance();
	APlayerController* PlayerController = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;
	if (!PlayerController)
	{
		FOnFAConfirmationResult FailedCallback = ResultCallback;
		FailedCallback.ExecuteIfBound(false);
		return false;
	}

	TSubclassOf<UFAGenericPopup> WidgetClass = PopupWidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = UFAGenericPopup::StaticClass();
	}

	ActivePopup = CreateWidget<UFAGenericPopup>(PlayerController, WidgetClass);
	if (!ActivePopup)
	{
		UE_LOG(LogTemp, Error, TEXT("[FAUIMessage] 확인창 위젯 생성에 실패했다."));
		FOnFAConfirmationResult FailedCallback = ResultCallback;
		FailedCallback.ExecuteIfBound(false);
		return false;
	}

	ActivePopup->OnClosed.AddUObject(this, &ThisClass::HandlePopupClosed);
	ActivePopup->SetupConfirmation(Title, Message, YesText, NoText, ResultCallback);
	ActivePopup->AddToViewport(PopupZOrder);
	SaveAndApplyPopupInputMode(PlayerController, ActivePopup);
	return true;
}

void UFAUIMessageSubsystem::QueueMessage(const FText& Title, const FText& Message, const FText& ConfirmText)
{
	PendingTitle = Title;
	PendingMessage = Message;
	PendingConfirmText = ConfirmText;
	bHasPendingMessage = true;
}

void UFAUIMessageSubsystem::CloseMessageInternal()
{
	if (IsValid(ActivePopup))
	{
		UFAGenericPopup* PopupToClose = ActivePopup;
		ActivePopup = nullptr;
		PopupToClose->OnClosed.RemoveAll(this);
		PopupToClose->ClosePopup();
	}

	RestorePreviousInputMode();
}

void UFAUIMessageSubsystem::HandlePopupClosed(UFAGenericPopup* ClosedPopup)
{
	if (ActivePopup != ClosedPopup)
	{
		return;
	}

	ActivePopup = nullptr;
	RestorePreviousInputMode();
}

void UFAUIMessageSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!bHasPendingMessage || !LoadedWorld || LoadedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	// PostLoadMap 시점에는 로컬 PlayerController 생성이 한 프레임 늦을 수 있다.
	LoadedWorld->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &ThisClass::TryShowPendingMessage));
}

void UFAUIMessageSubsystem::TryShowPendingMessage()
{
	if (!bHasPendingMessage)
	{
		return;
	}

	const FText Title = PendingTitle;
	const FText Message = PendingMessage;
	const FText ConfirmText = PendingConfirmText;
	bHasPendingMessage = false;

	ShowMessageInternal(Title, Message, ConfirmText);
}

void UFAUIMessageSubsystem::SaveAndApplyPopupInputMode(APlayerController* PlayerController, UFAGenericPopup* Popup)
{
	if (!PlayerController || !Popup)
	{
		return;
	}

	// 팝업 내용을 교체하는 동안 원래 상태가 UIOnly로 덮어써지지 않도록 최초 한 번만 저장한다.
	if (!bHasSavedInputMode)
	{
		UGameViewportClient* ViewportClient = PlayerController->GetWorld()
			? PlayerController->GetWorld()->GetGameViewport()
			: nullptr;
		if (!ViewportClient)
		{
			return;
		}

		SavedInputPlayerController = PlayerController;
		SavedMouseCaptureMode = static_cast<uint8>(ViewportClient->GetMouseCaptureMode());
		SavedMouseLockMode = static_cast<uint8>(ViewportClient->GetMouseLockMode());
		bSavedShowMouseCursor = PlayerController->ShouldShowMouseCursor();
		bSavedIgnoreInput = ViewportClient->IgnoreInput();
		bSavedHideCursorDuringCapture = ViewportClient->HideCursorDuringCapture();

		const EMouseCaptureMode MouseCaptureMode = ViewportClient->GetMouseCaptureMode();
		if (bSavedIgnoreInput)
		{
			SavedInputMode = static_cast<uint8>(EFAStoredInputMode::UIOnly);
		}
		else if (MouseCaptureMode == EMouseCaptureMode::CaptureDuringMouseDown
			|| MouseCaptureMode == EMouseCaptureMode::NoCapture)
		{
			SavedInputMode = static_cast<uint8>(EFAStoredInputMode::GameAndUI);
		}
		else
		{
			SavedInputMode = static_cast<uint8>(EFAStoredInputMode::GameOnly);
		}

		if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
		{
			if (TSharedPtr<FSlateUser> SlateUser = LocalPlayer->GetSlateUser())
			{
				SavedFocusedWidget = SlateUser->GetFocusedWidget();
			}
		}

		bHasSavedInputMode = true;
	}

	//FInputModeUIOnly PopupInputMode;
	FInputModeGameAndUI PopupInputMode;
	PopupInputMode.SetWidgetToFocus(Popup->TakeWidget());
	PopupInputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController->SetInputMode(PopupInputMode);
	PlayerController->SetShowMouseCursor(true);
}

void UFAUIMessageSubsystem::RestorePreviousInputMode()
{
	if (!bHasSavedInputMode)
	{
		return;
	}

	APlayerController* PlayerController = SavedInputPlayerController.Get();
	UGameViewportClient* ViewportClient = PlayerController && PlayerController->GetWorld()
		? PlayerController->GetWorld()->GetGameViewport()
		: nullptr;

	if (PlayerController && ViewportClient)
	{
		const EFAStoredInputMode InputMode = static_cast<EFAStoredInputMode>(SavedInputMode);
		const EMouseLockMode MouseLockMode = static_cast<EMouseLockMode>(SavedMouseLockMode);
		const TSharedPtr<SWidget> FocusedWidget = SavedFocusedWidget.Pin();

		switch (InputMode)
		{
		case EFAStoredInputMode::UIOnly:
		{
			FInputModeUIOnly RestoredInputMode;
			RestoredInputMode.SetLockMouseToViewportBehavior(MouseLockMode);
			if (FocusedWidget.IsValid())
			{
				RestoredInputMode.SetWidgetToFocus(FocusedWidget);
			}
			PlayerController->SetInputMode(RestoredInputMode);
			break;
		}
		case EFAStoredInputMode::GameAndUI:
		{
			FInputModeGameAndUI RestoredInputMode;
			RestoredInputMode.SetLockMouseToViewportBehavior(MouseLockMode);
			RestoredInputMode.SetHideCursorDuringCapture(bSavedHideCursorDuringCapture);
			if (FocusedWidget.IsValid())
			{
				RestoredInputMode.SetWidgetToFocus(FocusedWidget);
			}
			PlayerController->SetInputMode(RestoredInputMode);
			break;
		}
		case EFAStoredInputMode::GameOnly:
		default:
		{
			FInputModeGameOnly RestoredInputMode;
			RestoredInputMode.SetConsumeCaptureMouseDown(
				static_cast<EMouseCaptureMode>(SavedMouseCaptureMode) == EMouseCaptureMode::CapturePermanently);
			PlayerController->SetInputMode(RestoredInputMode);
			break;
		}
		}

		// 표준 InputMode 적용 후 커스텀 설정까지 포함한 실제 뷰포트 값을 정확히 되돌린다.
		ViewportClient->SetIgnoreInput(bSavedIgnoreInput);
		ViewportClient->SetMouseCaptureMode(static_cast<EMouseCaptureMode>(SavedMouseCaptureMode));
		ViewportClient->SetMouseLockMode(MouseLockMode);
		ViewportClient->SetHideCursorDuringCapture(bSavedHideCursorDuringCapture);
		PlayerController->SetShowMouseCursor(bSavedShowMouseCursor);
	}

	ResetSavedInputMode();
}

void UFAUIMessageSubsystem::ResetSavedInputMode()
{
	SavedInputPlayerController.Reset();
	SavedFocusedWidget.Reset();
	SavedInputMode = 0;
	SavedMouseCaptureMode = 0;
	SavedMouseLockMode = 0;
	bSavedShowMouseCursor = false;
	bSavedIgnoreInput = false;
	bSavedHideCursorDuringCapture = false;
	bHasSavedInputMode = false;
}
