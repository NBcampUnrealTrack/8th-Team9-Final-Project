// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FASessionCreateWidget.h"

#include "Components/Button.h"
#include "Components/EditableText.h"
#include "Components/SpinBox.h"
#include "GameFramework/PlayerState.h"

bool UFASessionCreateWidget::Initialize()
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
	
	return true;
}

void UFASessionCreateWidget::SubmitCreateRequest()
{
	FFASessionCreateRequest Request;
	Request.MaxConnections = GetMaxConnections();
	Request.ServerName = GetSessionName().TrimStartAndEnd();
	Request.Password = GetPasswordString();

	OnCreateSessionRequested.Broadcast(Request);
}

FString UFASessionCreateWidget::GetSessionName() const
{
	return SessionNameInput->GetText().ToString();
}
int32 UFASessionCreateWidget::GetMaxConnections() const
{
	return MaxConnectionSpinBox->GetValue();
}

FString UFASessionCreateWidget::GetPasswordString() const
{
	return PrivateSessionPass->GetText().ToString();
}

void UFASessionCreateWidget::NativeConstruct()
{
	Super::NativeConstruct();
}
