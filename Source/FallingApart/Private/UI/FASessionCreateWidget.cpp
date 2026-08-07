// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FASessionCreateWidget.h"

#include "Components/Button.h"
#include "Components/EditableText.h"
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
	
	if (MaxConnectionInput)
	{
		MaxConnectionInput->OnTextChanged.AddDynamic(this, &ThisClass::OnNumberTextChanged);
		LastValidText = FText::GetEmpty();
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
	// FString to int32
	return FCString::Atoi(*MaxConnectionInput->GetText().ToString());
}

FString UFASessionCreateWidget::GetPasswordString() const
{
	return PrivateSessionPass->GetText().ToString();
}

void UFASessionCreateWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (MaxConnectionInput && MaxConnectionInput->GetText().IsEmpty())
	{
		MaxConnectionInput->SetText(FText::AsNumber(8));
	}
}

void UFASessionCreateWidget::OnNumberTextChanged(const FText& Text)
{
	// 숫자만 입력하도록 제한함.
	FString Str = Text.ToString();
	
	if (Str.IsEmpty()) 
	{
		LastValidText = Text;
		return;
	}
	
	// 숫자인지 확인 + 최소최대인원수 제한
	const int32 ConnectionCount = FCString::Atoi(*Str);
	if (Str.IsNumeric() && ConnectionCount >= 6 && ConnectionCount <= 12)
	{
		LastValidText = Text; // 유효한 숫자이므로 저장
	}
	else
	{
		// 숫자가 아니면 직전의 유효한 텍스트로 복구
		MaxConnectionInput->SetText(LastValidText);
	}
	
}
