// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FASessionJoinWidget.h"

#include "Audio/FAAudioSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/EditableText.h"
#include "Components/EditableTextBox.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerState.h"
#include "Misc/Base64.h"
#include "UI/FASessionEntryWidget.h"
#include "UI/FAUIMessageSubsystem.h"

namespace FASessionJoinAudio
{
	static const FName Click(TEXT("UI_Click"));
	static const FName Hover(TEXT("UI_Hover"));

	/** 목록이 채워졌을 때 / 한 방도 못 찾았을 때. */
	static const FName Success(TEXT("UI_Success"));
	static const FName Denied(TEXT("UI_Denied"));
}

namespace
{
	const FName SessionNameSettingKey(TEXT("SERVER_NAME"));
	const FString SessionTextPrefix(TEXT("B64:"));

	FString DecodeSessionText(const FString& StoredText)
	{
		if (!StoredText.StartsWith(SessionTextPrefix))
		{
			return StoredText;
		}

		FString DecodedText;
		return FBase64::Decode(StoredText.RightChop(SessionTextPrefix.Len()), DecodedText)
			? DecodedText
			: StoredText;
	}
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

	if (SessionSearchInput)
	{
		SessionSearchInput->OnTextChanged.AddUniqueDynamic(this, &ThisClass::HandleSessionSearchTextChanged);
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
	CachedSessionResults = SessionResults;
	RebuildFilteredSessionList();

	// 검색은 몇 초씩 걸린다 — 목록을 계속 쳐다보지 않아도 끝났다는 걸 알아야 한다.
	UFAAudioSubsystem::PlayFASound(this, SessionResults.Num() > 0
		? FASessionJoinAudio::Success
		: FASessionJoinAudio::Denied);
}

bool UFASessionJoinWidget::DoesSessionTitleMatchSearch(
	const FOnlineSessionSearchResult& SessionResult, const FString& SearchText)
{
	const FString NormalizedSearchText = SearchText.TrimStartAndEnd();
	if (NormalizedSearchText.IsEmpty())
	{
		return true;
	}

	FString StoredSessionName;
	if (!SessionResult.Session.SessionSettings.Get(SessionNameSettingKey, StoredSessionName))
	{
		return false;
	}

	return DecodeSessionText(StoredSessionName).Contains(NormalizedSearchText, ESearchCase::IgnoreCase);
}

void UFASessionJoinWidget::HandleSessionSearchTextChanged(const FText& Text)
{
	RebuildFilteredSessionList();
}

void UFASessionJoinWidget::RebuildFilteredSessionList()
{
	ClearList();

	const FString SearchText = SessionSearchInput
		? SessionSearchInput->GetText().ToString()
		: FString();
	int32 VisibleResultCount = 0;
	for (const FOnlineSessionSearchResult& SessionResult : CachedSessionResults)
	{
		if (DoesSessionTitleMatchSearch(SessionResult, SearchText))
		{
			AddSessionSearchResult(SessionResult);
			++VisibleResultCount;
		}
	}

	const bool bHasActiveFilter = !SearchText.TrimStartAndEnd().IsEmpty();
	ShowStatusMessage(bHasActiveFilter && VisibleResultCount == 0 && CachedSessionResults.Num() > 0
		? NSLOCTEXT("FASessionJoinWidget", "NoMatchingSessions", "검색 조건과 일치하는 방이 없습니다.")
		: FText::GetEmpty());
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

void UFASessionJoinWidget::ShowInvitedSession(const FOnlineSessionSearchResult& InviteResult)
{
	CachedSessionResults.Reset();
	CachedSessionResults.Add(InviteResult);
	if (SessionSearchInput)
	{
		SessionSearchInput->SetText(FText::GetEmpty());
	}
	RebuildFilteredSessionList();
	
	//임시 메세지 출력
	ShowStatusMessage(
		NSLOCTEXT(
			"SessionJoinWidget",
			"ShowPlatformInvitedSession",
			"친구 초대로 참가할 방입니다."));
}

FString UFASessionJoinWidget::GetNickname() const
{
	return NicknameInput->GetText().ToString();
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
	FString Nickname = GetNickname();
	if (Nickname.IsEmpty() || Nickname.TrimStartAndEnd().Len()<2)
	{
		UFAUIMessageSubsystem::ShowMessage(
			this,
			NSLOCTEXT("FASessionJoinWidget","InvalidNicknameTitle", "닉네임 오류"),
			NSLOCTEXT("FASessionJoinWidget","NicknameTooShort", "닉네임은 2자 이상 입력해주세요."));
		return;
	}
	const FString Password = PasswordInput ? PasswordInput->GetText().ToString() : FString();
	OnSessionJoinRequested.Broadcast(SessionResult, Password, GetNickname());
}
