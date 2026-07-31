// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/FASpectatorPawn.h"

#include "Voice/FAVoiceComponent.h"

AFASpectatorPawn::AFASpectatorPawn()
{
	VoiceComponent = CreateDefaultSubobject<UFAVoiceComponent>(TEXT("VoiceComponent"));
	VoiceComponent->SetVoiceChannel(EFAVoiceChannel::Global);
}
