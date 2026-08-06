// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FASessionCreateWidget.h"

#include "Components/Button.h"
#include "Components/EditableText.h"
#include "GameFramework/PlayerState.h"
#include "Network/FAOnlineSessionSubsystem.h"


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
	
	if (MaxConnectionInput)
	{
		MaxConnectionInput->OnTextChanged.AddDynamic(this, &ThisClass::OnNumberTextChanged);
		LastValidText = FText::GetEmpty();
	}
	
	return true;
}

void UFASessionCreateWidget::ConfirmButtonClicked()
{
	
}

void UFASessionCreateWidget::ReturnButtonClicked()
{
	
}

TArray<FString> UFASessionCreateWidget::GetSessionSettings()
{
	return TArray<FString>{};
}

FString UFASessionCreateWidget::GetNickName()
{
	return NicknameInput->GetText().ToString();
}

FString UFASessionCreateWidget::GetSessionName()
{
	return SessionNameInput->GetText().ToString();
}

int32 UFASessionCreateWidget::GetMaxConnection()
{
	// FString to int32
	return FCString::Atoi(*MaxConnectionInput->GetText().ToString());
}

FString UFASessionCreateWidget::GetPasswordString()
{
	return PrivateSessionPass->GetText().ToString();
}

void UFASessionCreateWidget::OnNumberTextChanged(const FText& Text)
{
	// 숫자만 허용하도록 함
	FString Str = Text.ToString();
	
	if (Str.IsEmpty()) 
	{
		LastValidText = Text;
		return;
	}
	
	// 숫자인지 확인 
	if (Str.IsNumeric())
	{
		LastValidText = Text; // 유효한 숫자이므로 저장
	}
	else
	{
		// 숫자가 아니면 직전의 유효한 텍스트로 복구
		MaxConnectionInput->SetText(LastValidText);
	}
	
}
