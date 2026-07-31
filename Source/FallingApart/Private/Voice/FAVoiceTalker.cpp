// Fill out your copyright notice in the Description page of Project Settings.

#include "Voice/FAVoiceTalker.h"

UFAVoiceTalker::UFAVoiceTalker(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UFAVoiceTalker::OnTalkingBegin(UAudioComponent* AudioComponent)
{
	Super::OnTalkingBegin(AudioComponent);

	SetTalkingState(true);
}

void UFAVoiceTalker::OnTalkingEnd()
{
	Super::OnTalkingEnd();

	SetTalkingState(false);
}

void UFAVoiceTalker::SetTalkingState(bool bNewTalking)
{
	if (bIsTalking == bNewTalking)
	{
		return;
	}

	bIsTalking = bNewTalking;
	OnTalkerStateChanged.Broadcast(bIsTalking);
}
