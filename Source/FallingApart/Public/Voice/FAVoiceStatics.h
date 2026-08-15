// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FAVoiceStatics.generated.h"

class UWorld;

/**
 * 보이스 설정 UI 에서 쓰는 헬퍼 모음.
 * 엔진의 UVOIPStatics / config 를 직접 만지지 않고 여기를 통하게 해서
 * 백엔드 교체 시 UI 쪽 블루프린트를 손대지 않도록 함.
 */
UCLASS()
class FALLINGAPART_API UFAVoiceStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 마이크 무음 판정 임계값(0.0 ~ 1.0).
	 */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Voice")
	static void SetMicThreshold(float Threshold);

	/**
	 * 보이스 재생 볼륨(사용자 설정). 설정창의 '출력 볼륨' 슬라이더가 여기로 들어온다.
	 * 각 화자의 UFAVoiceComponent::VoicePlaybackVolume 과 곱해지므로 1.0 이 기본값이다.
	 * 지금 말하고 있는 사람에게도 즉시 반영된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Voice")
	static void SetVoiceOutputVolume(float Volume);

	UFUNCTION(BlueprintPure, Category = "FallingApart|Voice")
	static float GetVoiceOutputVolume();

	/**
	 * 지금 실제로 올라와 있는 보이스 백엔드 이름(NULL, STEAM …). 설정 UI 표시와 디버그용.
	 */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Voice")
	static FName GetVoiceBackendName();

	/** config 가 요구하는 백엔드([OnlineSubsystem] DefaultPlatformService). 실제 동작값이 아니다. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Voice")
	static FName GetConfiguredVoiceBackendName();

	/** config 상 보이스가 켜져 있는가([Voice] bEnabled + [OnlineSubsystem] bHasVoiceEnabled). */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Voice")
	static bool IsVoiceEnabledInConfig();

	/** config 상 푸시투토크가 요구되는가([/Script/Engine.GameSession] bRequiresPushToTalk, DefaultGame.ini). */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Voice")
	static bool DoesConfigRequirePushToTalk();

	/**
	 * 월드가 내려가기 전에 그 월드에 등록된 VOIP 재생 컴포넌트를 전부 떼어낸다.
	 */
	static void ShutdownVoicePlaybackForWorld(UWorld* World);
};
