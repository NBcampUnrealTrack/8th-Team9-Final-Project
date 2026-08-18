// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FASessionCreateWidget.h"

#include "Audio/FAAudioSubsystem.h"
#include "Components/Button.h"
#include "Components/EditableText.h"
#include "Components/SpinBox.h"
#include "GameFramework/PlayerState.h"

namespace FASessionCreateAudio
{
	static const FName SliderTick(TEXT("UI_SliderTick"));
}

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
	if (MaxConnectionSpinBox)
	{
		MaxConnectionSpinBox->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleMaxConnectionChanged);
	}

	return true;
}

void UFASessionCreateWidget::HandleMaxConnectionChanged(float NewValue)
{
	UFAAudioSubsystem::PlayFASound(this, FASessionCreateAudio::SliderTick);
}

void UFASessionCreateWidget::SubmitCreateRequest()
{
	FFASessionCreateRequest Request;
	Request.MaxConnections = GetMaxConnections();
	Request.ServerName = GetSessionName().TrimStartAndEnd();
	Request.Password = GetPasswordString();
	Request.Nickname = GetNickname();
	
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

FString UFASessionCreateWidget::GetNickname() const
{
	return NicknameInput->GetText().ToString();
}

void UFASessionCreateWidget::NativeConstruct()
{
	Super::NativeConstruct();
}
