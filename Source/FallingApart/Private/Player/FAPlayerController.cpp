// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/FAPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"

AFAPlayerController::AFAPlayerController()
{
	bReplicates = true;
}

void AFAPlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameOnly InputModeData;
	SetInputMode(InputModeData);
	bShowMouseCursor = false;

	if (IsLocalPlayerController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}
