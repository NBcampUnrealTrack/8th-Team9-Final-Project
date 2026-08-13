// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FASessionJoinWidget.h"

#include "Audio/FAAudioSubsystem.h"
#include "Components/Button.h"
#include "Components/EditableText.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerState.h"
#include "UI/FASessionEntryWidget.h"

namespace FASessionJoinAudio
{
	static const FName Click(TEXT("UI_Click"));
	static const FName Hover(TEXT("UI_Hover"));

	/** 목록이 채워졌을 때 / 한 방도 못 찾았을 때. */
	static const FName Success(TEXT("UI_Success"));
	static const FName Denied(TEXT("UI_Denied"));
}

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
		RefreshButton->OnHovered.AddUniqueDynamic(this, &ThisClass::HandleButtonHovered);
	}

	return true;
}

void UFASessionJoinWidget::HandleButtonHovered()
{
	UFAAudioSubsystem::PlayFASound(this, FASessionJoinAudio::Hover);
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

	// 검색은 몇 초씩 걸린다 — 목록을 계속 쳐다보지 않아도 끝났다는 걸 알아야 한다.
	UFAAudioSubsystem::PlayFASound(this, SessionResults.Num() > 0
		? FASessionJoinAudio::Success
		: FASessionJoinAudio::Denied);
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
	UFAAudioSubsystem::PlayFASound(this, FASessionJoinAudio::Click);

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
