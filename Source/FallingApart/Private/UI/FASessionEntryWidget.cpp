// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FASessionEntryWidget.h"

#include "Audio/FAAudioSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

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
	UFAAudioSubsystem::PlayFASound(this, FASessionEntryAudio::Click);

	if (EntryData.IsValid())
	{
		OnSessionEntrySelected.Broadcast(EntryData);
	}
}
