// Fill out your copyright notice in the Description page of Project Settings.

#include "Voice/FAVoiceTalker.h"

#include "Components/AudioComponent.h"
#include "HAL/IConsoleManager.h"

TArray<TWeakObjectPtr<UFAVoiceTalker>> UFAVoiceTalker::TalkingTalkers;

namespace FAVoiceTalkerInternal
{
	/**
	 * 사용자 출력 볼륨. 설정창 슬라이더가 UFAVoiceStatics::SetVoiceOutputVolume 으로 쓴다.
	 *
	 * 화자별 배수(UFAVoiceComponent::VoicePlaybackVolume)와 곱해진다 —
	 * 즉 여기 1.0 은 "설정 기본값", 실제 크기는 화자별 배수가 정한다.
	 * 콘솔에서 `FAVoice.OutputVolume 1.5` 처럼 바로 만져 가며 튜닝할 수 있다.
	 */
	static TAutoConsoleVariable<float> CVarVoiceOutputVolume(
		TEXT("FAVoice.OutputVolume"),
		1.f,
		TEXT("근접 보이스 재생 볼륨 배수 (사용자 설정). 화자별 배수와 곱해진다. 기본 1.0"),
		ECVF_Default);

	/** 슬라이더를 끌고 있는 동안에도 소리가 따라오게 한다. */
	static FAutoConsoleVariableSink CVarVoiceOutputVolumeSink(
		FConsoleCommandDelegate::CreateStatic(&UFAVoiceTalker::RefreshAllTalkingVolumes));
}

UFAVoiceTalker::UFAVoiceTalker(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UFAVoiceTalker::OnTalkingBegin(UAudioComponent* AudioComponent)
{
	Super::OnTalkingBegin(AudioComponent);

	// 엔진이 여기서 넘겨주는 것이 이 화자의 실제 재생 컴포넌트다 (VoiceEngineImpl.cpp).
	// 재생 볼륨을 만질 수 있는 지점은 사실상 여기 하나뿐이다.
	PlaybackAudioComponent = AudioComponent;
	ApplyPlaybackVolume();

	TalkingTalkers.AddUnique(this);

	SetTalkingState(true);
}

void UFAVoiceTalker::OnTalkingEnd()
{
	Super::OnTalkingEnd();

	TalkingTalkers.Remove(this);
	PlaybackAudioComponent.Reset();

	SetTalkingState(false);
}

void UFAVoiceTalker::BeginDestroy()
{
	// 발화 중에 Pawn 이 파괴되면 OnTalkingEnd 가 안 온다 (라운드 리스폰이 매번 그렇다).
	TalkingTalkers.Remove(this);

	Super::BeginDestroy();
}

void UFAVoiceTalker::SetPlaybackVolumeMultiplier(float NewMultiplier)
{
	PlaybackVolumeMultiplier = FMath::Max(0.f, NewMultiplier);
	ApplyPlaybackVolume();
}

float UFAVoiceTalker::GetEffectivePlaybackVolume() const
{
	const float UserVolume = FMath::Max(0.f, FAVoiceTalkerInternal::CVarVoiceOutputVolume.GetValueOnGameThread());
	return PlaybackVolumeMultiplier * UserVolume;
}

void UFAVoiceTalker::ApplyPlaybackVolume()
{
	if (UAudioComponent* AudioComponent = PlaybackAudioComponent.Get())
	{
		// 재생 중에도 즉시 반영된다 (UAudioComponent::SetVolumeMultiplier → SendCommandToActiveSounds).
		AudioComponent->SetVolumeMultiplier(GetEffectivePlaybackVolume());
	}
}

void UFAVoiceTalker::RefreshAllTalkingVolumes()
{
	for (int32 Index = TalkingTalkers.Num() - 1; Index >= 0; --Index)
	{
		if (UFAVoiceTalker* Talker = TalkingTalkers[Index].Get())
		{
			Talker->ApplyPlaybackVolume();
		}
		else
		{
			TalkingTalkers.RemoveAtSwap(Index);
		}
	}
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
