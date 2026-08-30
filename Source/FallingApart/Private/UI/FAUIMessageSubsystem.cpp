// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/FAUIMessageSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UI/FAGenericPopup.h"
#include "UObject/UObjectGlobals.h"

UFAUIMessageSubsystem::UFAUIMessageSubsystem()
{
	PopupWidgetClass = UFAGenericPopup::StaticClass();
}

bool UFAUIMessageSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// 상위 클래스의 기본 동작을 유지합니다.
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	// 기본 C++ 클래스 대신 블루프린트 클래스를 쓰기 위해 우회할 수 있습니다.
	return true; 
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
}

void UFAUIMessageSubsystem::HandlePopupClosed(UFAGenericPopup* ClosedPopup)
{
	if (ActivePopup == ClosedPopup)
	{
		ActivePopup = nullptr;
	}
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
