// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FAVoiceTypes.generated.h"

FALLINGAPART_API DECLARE_LOG_CATEGORY_EXTERN(LogFAVoice, Log, All);

/**
 * 목소리가 어떤 방식으로 들리는지.
 *
 * 엔진 내장 VoIP 는 채널이 하나뿐이라 이 값이 "채널 분리"를 하지는 않음.
 * 실제 격리(관전자 ↔ 생존 플레이어)는 AFAGameMode::UpdateVoiceMuteMatrix() 가 담당.
 */
UENUM(BlueprintType)
enum class EFAVoiceChannel : uint8
{
	/**
	 * 근접 — 듣는 쪽 머신에 UVOIPTalker 를 등록해 캐릭터 위치에서 3D 로 재생하고 거리에 따라 감쇠.
	 * 살아있는 플레이어용.
	 */
	Proximity	= 0,

	/**
	 * 전역 — 거리·위치 무관하게 또렷하게 들린다. 관전자용.
	 */
	Global		= 1
};
