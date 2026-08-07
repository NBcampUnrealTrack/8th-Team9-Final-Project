// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FASessionJoinWidget.h"

#include "Components/Button.h"
#include "Components/EditableText.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerState.h"
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
	
	return true;
}

void UFASessionJoinWidget::AddSessionSearchResult(const FOnlineSessionSearchResult& Result)
{
	UFASessionEntryWidget* EntryInstance = CreateWidget<UFASessionEntryWidget>(this, UFASessionEntryWidgetClass);
	if (EntryInstance)
	{
		EntryInstance->OnSessionEntrySelected.AddUObject(this, &ThisClass::HandleSessionEntrySelected);
		EntryInstance->SetSessionInfo(Result);
		SessionScrollBox->AddChild(EntryInstance);
	}
}

void UFASessionJoinWidget::ActivatePage()
{
	RequestRefresh();
}

void UFASessionJoinWidget::SetSearchInProgress(bool bIsSearching)
{
	if (RefreshButton)
	{
		RefreshButton->SetIsEnabled(!bIsSearching);
	}

	if (bIsSearching)
	{
		ShowStatusMessage(NSLOCTEXT("FallingApart", "SearchingSessions", "Searching for sessions..."));
	}
}

void UFASessionJoinWidget::ShowSearchResults(const TArray<FOnlineSessionSearchResult>& SessionResults)
{
	ClearList();
	for (const FOnlineSessionSearchResult& SessionResult : SessionResults)
	{
		AddSessionSearchResult(SessionResult);
	}

	ShowStatusMessage(FText::GetEmpty());
}

void UFASessionJoinWidget::ShowStatusMessage(const FText& Message)
{
	if (SearchStatusText)
	{
		SearchStatusText->SetText(Message);
		SearchStatusText->SetVisibility(Message.IsEmpty() ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
	}
}

void UFASessionJoinWidget::SetJoinInProgress(bool bIsJoining)
{
	SetIsEnabled(!bIsJoining);
}

void UFASessionJoinWidget::RefreshButtonClicked()
{
	RequestRefresh();
}

void UFASessionJoinWidget::RequestRefresh()
{
	OnSessionRefreshRequested.Broadcast();
}

void UFASessionJoinWidget::ClearList()
{
	if (SessionScrollBox)
	{
		SessionScrollBox->ClearChildren();
	}
}

void UFASessionJoinWidget::HandleSessionEntrySelected(const FOnlineSessionSearchResult& SessionResult)
{
	const FString Password = PasswordInput ? PasswordInput->GetText().ToString() : FString();
	OnSessionJoinRequested.Broadcast(SessionResult, Password);
}
