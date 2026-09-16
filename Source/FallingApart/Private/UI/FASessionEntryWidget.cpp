// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FASessionEntryWidget.h"

#include "Audio/FAAudioSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Network/FAOnlineSessionSubsystem.h"
#include "Utils/FASessionTextCodec.h"

namespace FASessionEntryAudio
{
	static const FName Click(TEXT("UI_Click"));
	static const FName Hover(TEXT("UI_Hover"));
}

bool UFASessionEntryWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (Button_Join)
	{
		Button_Join->OnClicked.AddUniqueDynamic(this, &ThisClass::OnJoinButtonClicked);
		Button_Join->OnHovered.AddUniqueDynamic(this, &ThisClass::OnJoinButtonHovered);
	}


	return true;
}

void UFASessionEntryWidget::OnJoinButtonHovered()
{
	// 방 목록은 여러 줄이라 마우스가 빠르게 훑고 지나간다 — 표의 0.04초 쿨다운이 연사를 막는다.
	UFAAudioSubsystem::PlayFASound(this, FASessionEntryAudio::Hover);
}

void UFASessionEntryWidget::SetSessionInfo(const FOnlineSessionSearchResult& Result)
{
	EntryData = Result;
	
	FString StoredServerName;
	if (Result.Session.SessionSettings.Get(FName("SERVER_NAME"), StoredServerName)
		&& SessionTitleText)
	{
		SessionTitleText->SetText(
			FText::FromString(FFASessionTextCodec::Decode(StoredServerName)));
	}

	FString StoredHostNickname;
	if (Result.Session.SessionSettings.Get(FName("OwningUserNickname"), StoredHostNickname)
		&& SessionHostNicknameText)
	{
		SessionHostNicknameText->SetText(
			FText::FromString(FFASessionTextCodec::Decode(StoredHostNickname)));
	}
	
	// 이전 버전의 방처럼 커스텀 값이 없으면 기존 빈자리 계산으로 대체한다.
	const int32 MaxConnection = FMath::Max(0, Result.Session.SessionSettings.NumPublicConnections);
	int32 CurrentPlayers = MaxConnection - Result.Session.NumOpenPublicConnections;
	Result.Session.SessionSettings.Get(FName(TEXT("CURRENT_PLAYERS")), CurrentPlayers);
	CurrentPlayers = FMath::Clamp(CurrentPlayers, 0, MaxConnection);

	FText ConnectionTextFormat = FText::FromString(FString::Printf(TEXT("%d / %d"), CurrentPlayers, MaxConnection));
	CurrentConnectionText->SetText(ConnectionTextFormat);
	

	// 지연시간
	int32 Ping = Result.PingInMs;
	SessionPingText->SetText(FText::FromString(FString::Printf(TEXT("핑 : %d"), Ping)));
	
	
	//현재 로비 진행중 여부
	FString CurrentLobbyState;
	Result.Session.SessionSettings.Get(FName(TEXT("SESSION_STATE")), CurrentLobbyState);
	
	if (CurrentLobbyState == TEXT("Playing"))
	{
		//추후 진행중인 게임에 난입 관련 요소 있을경우 따로 처리, 현재로서는 Playing인 경우는 목록에서 제외함.
		//SessionStateText->SetText(FText::FromString("게임 진행 중"));
		SessionStateText->SetText(FText::FromString(FString::Printf(TEXT("게임 진행 중"))));
		Button_Join->SetIsEnabled(false);
	}
	else if (CurrentLobbyState ==  TEXT("Lobby"))
	{
		//SessionStateText->SetText(FText::FromString("대기 중"));
		SessionStateText->SetText(FText::FromString(FString::Printf(TEXT("대기 중"))));
	}else
	{
		UE_LOG(LogFAOSS, Warning, TEXT("FASessionEntryWidget SESSION_STATE를 가져올 수 없음."));
	}

	
	// 세션 비밀번호 아이콘
	bool bHasPassword = false;
	Result.Session.SessionSettings.Get(FName(TEXT("HAS_PASSWORD")), bHasPassword);
	Image_Lock->SetVisibility(
		bHasPassword == true ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	
}
void UFASessionEntryWidget::OnJoinButtonClicked()
{
	UFAAudioSubsystem::PlayFASound(this, FASessionEntryAudio::Click);

	if (EntryData.IsValid())
	{
		OnSessionEntrySelected.Broadcast(EntryData);
	}
}
