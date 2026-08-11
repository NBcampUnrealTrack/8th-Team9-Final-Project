// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FASessionEntryWidget.h"

#include "OnlineSessionSettings.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

bool UFASessionEntryWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	
	if (Button_Join)
	{
		Button_Join->OnClicked.AddUniqueDynamic(this, &ThisClass::OnJoinButtonClicked);
	}
	
	
	return true;
}

void UFASessionEntryWidget::SetSessionInfo(const FOnlineSessionSearchResult& Result)
{
	EntryData = Result;
	// 방 이름
	FString ServerName;
	Result.Session.SessionSettings.Get(FName("SERVER_NAME"), ServerName);
	SessionTitleText->SetText(FText::FromString(ServerName));
	
	// 지연시간
	int32 Ping = Result.PingInMs;
	SessionPingText->SetText(FText::AsNumber(Ping));
	
	// 참가자 수
	int32 MaxConnection = Result.Session.SessionSettings.NumPublicConnections;
	int32 CurrentConnection = MaxConnection - Result.Session.NumOpenPublicConnections;
	FText ConnectionTextFormat = FText::FromString(FString::Printf(TEXT("%d / %d"), CurrentConnection, MaxConnection));
	CurrentConnectionText->SetText(ConnectionTextFormat);
	
	//TODO : 현재 게임 상태를 가져오는방법? 대기중 - 진행중
	
	
}
void UFASessionEntryWidget::OnJoinButtonClicked()
{
	if (EntryData.IsValid())
	{
		OnSessionEntrySelected.Broadcast(EntryData);
	}
}
