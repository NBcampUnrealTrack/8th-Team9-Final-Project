// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FALobbyPlayerEntryWidget.h"
#include "Components/CheckBox.h"
#include "Player/FALobbyPlayerState.h"

#define LOCTEXT_NAMESPACE "FALobbyPlayerEntryWidget"

void UFALobbyPlayerEntryWidget::Setup(AFALobbyPlayerState* InPlayerState)
{
	UnBindPlayerState();
	LobbyPlayerState = InPlayerState;
	
	if (!LobbyPlayerState)
		return;
	// 각 엔트리는 한명의 플레이어 등록
	LobbyPlayerState->OnLobbyReadyChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleReadyChanged);
	
	//이름 등록
	Text_Nickname->SetText(FText::FromString(InPlayerState->GetPlayerName()));
	
	//처음에 명시적 호출하여 초기값등록
	HandleReadyChanged(LobbyPlayerState->IsLobbyReady());
}

void UFALobbyPlayerEntryWidget::UnBindPlayerState()
{
	if (LobbyPlayerState)
	{
		LobbyPlayerState->OnLobbyReadyChanged.RemoveAll(this);
		LobbyPlayerState = nullptr;
	}
}

void UFALobbyPlayerEntryWidget::HandleReadyChanged(bool bIsReady)
{
	Text_Ready->SetText(
		bIsReady ? LOCTEXT("ReadyState", "준비") : LOCTEXT("WaitingState", "대기"));
	
	Check_Ready->SetIsChecked(bIsReady);
}

void UFALobbyPlayerEntryWidget::NativeDestruct()
{
	UnBindPlayerState();
	Super::NativeDestruct();
}

void UFALobbyPlayerEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
	AFALobbyPlayerState* InLobbyGameState = Cast<AFALobbyPlayerState>(ListItemObject);
	Setup(InLobbyGameState);
}

#undef LOCTEXT_NAMESPACE
