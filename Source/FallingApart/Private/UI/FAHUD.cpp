// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FAHUD.h"

#include "Blueprint/UserWidget.h"
#include "UI/FAHUDWidget.h"

AFAHUD::AFAHUD()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AFAHUD::BeginPlay()
{
	Super::BeginPlay();

	if (!PlayerOwner || !PlayerOwner->IsLocalController())
	{
		return;
	}

	if (!HUDWidgetClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[FA] AFAHUD: HUDWidgetClass 가 비어 있어 HUD 를 만들지 않았다. BP_FAHUD 에서 WBP_FAHUD 를 지정할 것."));
		return;
	}

	HUDWidget = CreateWidget<UFAHUDWidget>(PlayerOwner, HUDWidgetClass);
	if (HUDWidget)
	{
		HUDWidget->AddToViewport(HUDWidgetZOrder);
	}
}

void AFAHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HUDWidget)
	{
		HUDWidget->RemoveFromParent();
		HUDWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

UFAHUDWidget* AFAHUD::GetHUDWidgetFor(APlayerController* PlayerController)
{
	const AFAHUD* FAHUD = PlayerController ? Cast<AFAHUD>(PlayerController->GetHUD()) : nullptr;
	return FAHUD ? FAHUD->GetHUDWidget() : nullptr;
}
