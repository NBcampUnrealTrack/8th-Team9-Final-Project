// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Net/VoiceConfig.h"
#include "FAVoiceTalker.generated.h"

class UAudioComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FFAOnTalkerStateChanged, bool /*bIsTalking*/);

/**
 * 엔진의 UVOIPTalker 에 발화 시작/종료 알림과 재생 음량 제어를 얹은 얇은 파생 클래스.
 *
 * UVOIPTalker 는 "이 플레이어의 목소리를 어디서 어떤 감쇠로 재생할지" 를 들고 있는
 * 엔진 쪽 창구.
 */
UCLASS()
class FALLINGAPART_API UFAVoiceTalker : public UVOIPTalker
{
	GENERATED_BODY()

public:
	UFAVoiceTalker(const FObjectInitializer& ObjectInitializer);

	virtual void OnTalkingBegin(UAudioComponent* AudioComponent) override;
	virtual void OnTalkingEnd() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	virtual void BeginDestroy() override;

	/**
	 * UVOIPTalker::RegisterWithPlayerState 대신 반드시 이것을 쓴다.
	 */
	void RegisterForPlayer(APlayerState* OwningState);

	/** 엔진 맵이 지금 이 화자를 가리키고 있는가. 풀렸으면 소유 컴포넌트가 다시 등록한다. */
	bool IsRegisteredWithVoiceEngine() const;

	bool IsTalking() const { return bIsTalking; }

	/**
	 * 이 화자의 목소리를 재생할 배수. 소유 UFAVoiceComponent 가 자기 설정값으로 넣어준다.
	 * 발화 중이면 즉시 반영된다.
	 */
	void SetPlaybackVolumeMultiplier(float NewMultiplier);

	float GetPlaybackVolumeMultiplier() const { return PlaybackVolumeMultiplier; }

	/**
	 * 사용자 출력 볼륨(FAVoice.OutputVolume)이 바뀌었을 때 지금 말하고 있는 모든 화자에 다시 적용.
	 * 말하고 있지 않은 화자는 다음 발화 시작 때 새 값을 집는다.
	 */
	static void RefreshAllTalkingVolumes();

	/** 실제로 오디오 컴포넌트에 곱해질 최종 배수 (화자별 배수 × 사용자 출력 볼륨). */
	float GetEffectivePlaybackVolume() const;

	/** 발화 상태가 바뀔 때 브로드캐스트. 소유 UFAVoiceComponent 가 구독. */
	FFAOnTalkerStateChanged OnTalkerStateChanged;

private:
	void SetTalkingState(bool bNewTalking);

	/** 내가 이 플레이어의 '현재 화자' 로 기록돼 있으면 지운다. 남의 것이면 건드리지 않는다. */
	void ClearCurrentTalkerEntry();

	/** 캐시해 둔 재생 컴포넌트에 최종 배수를 밀어넣는다. 재생 중이 아니면 아무 일도 안 한다. */
	void ApplyPlaybackVolume();

	bool bIsTalking = false;

	/**
	 * 화자별 배수. 기본 1.0 이고 실제 값은 UFAVoiceComponent::VoicePlaybackVolume 이 넣는다.
	 * (여기 기본값을 올리면 컴포넌트가 값을 넣기 전 한 프레임 동안만 적용돼서 의미가 없다.)
	 */
	float PlaybackVolumeMultiplier = 1.f;

	/**
	 * 엔진이 OnTalkingBegin 으로 건네준 재생용 오디오 컴포넌트.
	 * VoipSynthComponent 가 소유하며 발화가 끝나면 재사용되므로 약참조로만 들고 있는다.
	 */
	TWeakObjectPtr<UAudioComponent> PlaybackAudioComponent;

	/** 지금 발화 중인 화자 목록. 출력 볼륨 슬라이더를 움직였을 때 실시간 반영용. */
	static TArray<TWeakObjectPtr<UFAVoiceTalker>> TalkingTalkers;

	/** RegisterForPlayer 로 잡아 둔 등록 키. 죽을 때 남의 칸을 지웠는지 판정하는 데 쓴다. */
	FUniqueNetIdRepl RegisteredPlayerId;

	/**
	 * UniqueNetId 문자열 -> 그 플레이어의 가장 최근 화자.
	 */
	static TMap<FString, TWeakObjectPtr<UFAVoiceTalker>> CurrentTalkerByPlayer;
};
