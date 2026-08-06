// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FASessionJoinWidget.h"

#include "Components/Button.h"
#include "Components/EditableText.h"
#include "Components/ScrollBox.h"
#include "GameFramework/PlayerState.h"
#include "Network/FAOnlineSessionSubsystem.h"
#include "UI/FASessionEntryWidget.h"


bool UFASessionJoinWidget::Initialize()
{
	if (!Super::Initialize())
		return false;
	
	if (NicknameInput)
	{
		APlayerState* PlayerState = GetOwningPlayerState();
		if (PlayerState)
		{
			NicknameInput->SetText(FText::FromString(PlayerState->GetPlayerName()));
		}
	}
	if (RefreshButton)
	{
		RefreshButton->OnClicked.AddUniqueDynamic(this, &ThisClass::RefreshButtonClicked);
	}
	
	OnVisibilityChanged.AddDynamic(this, &ThisClass::VisibilityChanged);
	return true;
}

void UFASessionJoinWidget::VisibilityChanged(ESlateVisibility NewVisibility)
{
	if (NewVisibility == ESlateVisibility::Visible)
	{
		RefreshSessionList();
	}
}


void UFASessionJoinWidget::AddSessionSearchResult(const FOnlineSessionSearchResult& Result)
{
	UFASessionEntryWidget* EntryInstance = CreateWidget<UFASessionEntryWidget>(this, UFASessionEntryWidgetClass);
	if (EntryInstance)
	{
		EntryInstance->SetSessionInfo(Result);
		SessionScrollBox->AddChild(EntryInstance);
	}
}

void UFASessionJoinWidget::RefreshSessionList()
{
	//요청은 여기 버튼을 통해서 하지만, 작업은 MainUI에 콜백에서
	ClearList();
	if (UGameInstance* GameInstance = GetGameInstance())
	{ 
		UFAOnlineSessionSubsystem* OnlineSessionSubsystem = GameInstance->GetSubsystem<UFAOnlineSessionSubsystem>();
		OnlineSessionSubsystem->FindSession(100);
	}
}

void UFASessionJoinWidget::RefreshButtonClicked()
{
	RefreshSessionList();

}

void UFASessionJoinWidget::ClearList()
{
	SessionScrollBox->ClearChildren();
}