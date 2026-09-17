// Fill out your copyright notice in the Description page of Project Settings.

#include "Voice/FAVoiceTalker.h"
#include "Audio/FAHearingSubsystem.h" // [Z22 귀먹음 추가] 재사용 가능한 청취 배수 조회.

#include "Components/AudioComponent.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Voice/FAVoiceTypes.h"

TArray<TWeakObjectPtr<UFAVoiceTalker>> UFAVoiceTalker::TalkingTalkers;
TMap<FString, TWeakObjectPtr<UFAVoiceTalker>> UFAVoiceTalker::CurrentTalkerByPlayer;

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

void UFAVoiceTalker::RegisterForPlayer(APlayerState* OwningState)
{
	if (OwningState == nullptr || !OwningState->GetUniqueId().IsValid())
	{
		return;
	}

	RegisteredPlayerId = OwningState->GetUniqueId();

	// 엔진 맵에 나를 꽂는다. 여기까지는 기존과 같다.
	RegisterWithPlayerState(OwningState);

	// 그리고 "이 플레이어의 현재 화자는 나" 라고 우리 쪽에도 적는다.
	// 나중에 죽는 이전 화자가 이 값을 보고 자기 뒷정리 범위를 판단한다.
	CurrentTalkerByPlayer.Add(RegisteredPlayerId->ToString(), this);
}

bool UFAVoiceTalker::IsRegisteredWithVoiceEngine() const
{
	if (!RegisteredPlayerId.IsValid())
	{
		return false;
	}

	return UVOIPStatics::GetVOIPTalkerForPlayer(RegisteredPlayerId) == this;
}

void UFAVoiceTalker::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	// Super 가 VoiceTalkerMap 에서 PlayerId 칸을 통째로 지운다 — 그 칸이 내 것이 아니어도.
	Super::OnComponentDestroyed(bDestroyingHierarchy);

	if (!RegisteredPlayerId.IsValid())
	{
		return;
	}

	const FString Key = RegisteredPlayerId->ToString();
	const TWeakObjectPtr<UFAVoiceTalker>* Current = CurrentTalkerByPlayer.Find(Key);
	if (Current == nullptr)
	{
		return;
	}

	UFAVoiceTalker* Successor = Current->Get();

	// 아직 내가 현재 화자였다면(= 리스폰 없이 그냥 나가는 경우) 지운 게 맞다.
	if (Successor == nullptr || Successor == this)
	{
		CurrentTalkerByPlayer.Remove(Key);
		return;
	}

	// 다음 Pawn 의 화자가 이미 등록돼 있었는데 내가 죽으면서 같이 지워 버린 경우다. 도로 꽂는다.
	UVOIPStatics::SetVOIPTalkerForPlayer(RegisteredPlayerId, Successor);

	UE_LOG(LogFAVoice, Verbose, TEXT("[보이스] %s 의 화자 등록이 이전 Pawn 정리에 휩쓸려 지워져 복구했다."), *Key);
}

void UFAVoiceTalker::BeginDestroy()
{
	// 발화 중에 Pawn 이 파괴되면 OnTalkingEnd 가 안 온다 (라운드 리스폰이 매번 그렇다).
	TalkingTalkers.Remove(this);

	// OnComponentDestroyed 를 못 타고 GC 로 바로 오는 경로(월드 정리 등)를 위한 보루.
	ClearCurrentTalkerEntry();

	Super::BeginDestroy();
}

void UFAVoiceTalker::ClearCurrentTalkerEntry()
{
	if (!RegisteredPlayerId.IsValid())
	{
		return;
	}

	const FString Key = RegisteredPlayerId->ToString();
	if (const TWeakObjectPtr<UFAVoiceTalker>* Current = CurrentTalkerByPlayer.Find(Key))
	{
		if (Current->Get() == this)
		{
			CurrentTalkerByPlayer.Remove(Key);
		}
	}
}

void UFAVoiceTalker::SetPlaybackVolumeMultiplier(float NewMultiplier)
{
	PlaybackVolumeMultiplier = FMath::Max(0.f, NewMultiplier);
	ApplyPlaybackVolume();
}

float UFAVoiceTalker::GetEffectivePlaybackVolume() const
{
	const float UserVolume = FMath::Max(0.f, FAVoiceTalkerInternal::CVarVoiceOutputVolume.GetValueOnGameThread());
	// [Z22 귀먹음 변경 시작] 사용자/화자별 볼륨과 마이크 송신은 유지하고 출력 배수만 곱한다.
	const UFAHearingSubsystem* Hearing = UFAHearingSubsystem::Get(this);
	const float HearingMultiplier = Hearing ? Hearing->GetVoiceHearingMultiplier() : 1.0f;
	return PlaybackVolumeMultiplier * UserVolume * HearingMultiplier;
	// [Z22 귀먹음 변경 끝]
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

// [Z22 귀먹음 추가 시작] static 목록의 다른 PIE 월드에 영향을 주지 않도록 월드를 구분한다.
void UFAVoiceTalker::RefreshTalkingVolumesForWorld(const UWorld* World)
{
	if (!World)
	{
		return;
	}
	for (const TWeakObjectPtr<UFAVoiceTalker>& Entry : TalkingTalkers)
	{
		UFAVoiceTalker* Talker = Entry.Get();
		if (Talker && Talker->GetWorld() == World)
		{
			Talker->ApplyPlaybackVolume();
		}
	}
}
// [Z22 귀먹음 추가 끝]

void UFAVoiceTalker::SetTalkingState(bool bNewTalking)
{
	if (bIsTalking == bNewTalking)
	{
		return;
	}

	bIsTalking = bNewTalking;
	OnTalkerStateChanged.Broadcast(bIsTalking);
}
