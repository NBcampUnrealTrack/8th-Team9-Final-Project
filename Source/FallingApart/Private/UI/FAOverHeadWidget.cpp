// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FAOverHeadWidget.h"

#include "Components/TextBlock.h"
#include "GameFramework/PlayerState.h"
#include "Player/FACharacter.h"

void UFAOverHeadWidget::SetTargetCharacter(AFACharacter* InTargetCharacter)
{
	TargetCharacter = InTargetCharacter;
	RefreshNickname();
}

void UFAOverHeadWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// SetTargetCharacter()가 NativeConstruct보다 먼저 호출되는 구성도 안전하게 지원한다.
	RefreshNickname();
}

void UFAOverHeadWidget::RefreshNickname()
{
	if (!Text_Nickname)
	{
		return;
	}

	const AFACharacter* Character = TargetCharacter.Get();
	const APlayerState* PlayerState = Character ? Character->GetPlayerState() : nullptr;
	Text_Nickname->SetText(PlayerState
		? FText::FromString(PlayerState->GetPlayerName())
		: FText::GetEmpty());
}

void UFAOverHeadWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
}

void UFAOverHeadWidget::NativeDestruct()
{
	Super::NativeDestruct();
}
