// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Net/VoiceConfig.h"
#include "FAVoiceTalker.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FFAOnTalkerStateChanged, bool /*bIsTalking*/);

/**
 * 엔진의 UVOIPTalker 에 발화 시작/종료 알림만 얹은 얇은 파생 클래스.
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

	bool IsTalking() const { return bIsTalking; }

	/** 발화 상태가 바뀔 때 브로드캐스트. 소유 UFAVoiceComponent 가 구독. */
	FFAOnTalkerStateChanged OnTalkerStateChanged;

private:
	void SetTalkingState(bool bNewTalking);

	bool bIsTalking = false;
};
