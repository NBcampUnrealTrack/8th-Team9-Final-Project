// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FAOverHeadWidget.h"


#include "Components/TextBlock.h"
#include "GameFramework/PlayerState.h"

void UFAOverHeadWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	OwnerCharacter = GetOwningPlayerPawn();
	if (APlayerState* PlayerState = OwnerCharacter->GetPlayerState())
	{
		Text_Nickname->SetText(FText::FromString(PlayerState->GetPlayerName()));
	}
}

void UFAOverHeadWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
}

void UFAOverHeadWidget::NativeDestruct()
{
	Super::NativeDestruct();
}