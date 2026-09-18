// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FASessionCreateWidget.h"

#include "Audio/FAAudioSubsystem.h"
#include "Components/Button.h"
#include "Components/EditableText.h"
#include "Components/Image.h"
#include "Components/SpinBox.h"
#include "GameFramework/PlayerState.h"
#include "UI/FAUIMessageSubsystem.h"

#define LOCTEXT_NAMESPACE "FASessionCreateWidget"

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

	if (PasswordVisibilityButton)
	{
		PasswordVisibilityButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePasswordVisibilityClicked);
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
	Request.ServerName = GetSessionName();
	Request.Password = GetPasswordString();
	Request.Nickname = GetNickname();

	FText ValidationError;
	if (!ValidateCreateRequest(Request, ValidationError))
	{
		UFAUIMessageSubsystem::ShowMessage(
			this,
			LOCTEXT("InvalidCreateRequestTitle", "입력 오류"),
			ValidationError);
		return;
	}
	
	OnCreateSessionRequested.Broadcast(Request);
}

bool UFASessionCreateWidget::ValidateCreateRequest(FFASessionCreateRequest& Request, FText& OutErrorMessage)
{
	Request.ServerName = Request.ServerName.TrimStartAndEnd();
	Request.Nickname = Request.Nickname.TrimStartAndEnd();

	if (Request.ServerName.IsEmpty())
	{
		OutErrorMessage = LOCTEXT("SessionNameRequired", "방 제목을 입력해주세요.");
		return false;
	}

	if (Request.Password.Len() > 0 && Request.Password.Len() < 4)
	{
		OutErrorMessage = LOCTEXT("PasswordTooShort", "비밀번호는 4자 이상 입력해주세요.");
		return false;
	}

	if (Request.Nickname.TrimStartAndEnd().Len() < 2)
	{
		OutErrorMessage = LOCTEXT("NicknameTooShort", "닉네임은 2자 이상 입력해주세요.");
		return false;
	}

	OutErrorMessage = FText::GetEmpty();
	return true;
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
	ActivatePage();
}

void UFASessionCreateWidget::ActivatePage()
{
	SetPasswordVisible(false);
}

void UFASessionCreateWidget::HandlePasswordVisibilityClicked()
{
	if (PrivateSessionPass)
	{
		SetPasswordVisible(PrivateSessionPass->GetIsPassword());
		UFAAudioSubsystem::PlayFASound(this, TEXT("UI_Click"));
	}
}

void UFASessionCreateWidget::SetPasswordVisible(bool bVisible)
{
	if (PrivateSessionPass)
	{
		PrivateSessionPass->SetIsPassword(!bVisible);
	}
	if (PasswordVisibilityIcon)
	{
		PasswordVisibilityIcon->SetBrushFromTexture(bVisible ? PasswordVisibleIcon : PasswordHiddenIcon);
	}
	if (PasswordVisibilityButton)
	{
		PasswordVisibilityButton->SetToolTipText(bVisible
			? NSLOCTEXT("FAPasswordVisibilityToggle", "HidePassword", "비밀번호 숨기기")
			: NSLOCTEXT("FAPasswordVisibilityToggle", "ShowPassword", "비밀번호 보기"));
	}
}

#undef LOCTEXT_NAMESPACE
