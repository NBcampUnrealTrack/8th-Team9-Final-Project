// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FAVoiceStatics.generated.h"

class UFAVoiceComponent;
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
	 * 마이크 방식. 설정창의 '마이크 방식'(오픈 마이크 / 푸시투토크)이 여기로 들어온다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Voice", meta = (WorldContext = "WorldContextObject"))
	static void SetPushToTalkEnabled(const UObject* WorldContextObject, bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "FallingApart|Voice")
	static bool IsPushToTalkEnabled();

	/**
	 * 지금 연결된 마이크 장치 이름들(윈도우 전용, 다른 플랫폼은 빈 배열).
	 * 부를 때마다 새로 읽는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Voice")
	static TArray<FString> GetCaptureDevices();

	/**
	 * 마이크 입력 장치. 빈 문자열 = 시스템 기본. 설정창의 '입력 장치'가 여기로 들어온다.
	 *
	 * 값은 CVar FAVoice.CaptureDevice 에 두고, 엔진 캡처가 이미 있으면 바로 바꾼다.
	 * 캡처는 처음 말할 때(RegisterLocalTalker) 생기므로 메인 메뉴에서 고른 값은 그때 ApplyCaptureDevice 가 넣는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Voice", meta = (WorldContext = "WorldContextObject"))
	static void SetCaptureDevice(const UObject* WorldContextObject, const FString& DeviceName);

	UFUNCTION(BlueprintPure, Category = "FallingApart|Voice")
	static FString GetCaptureDevice();

	/**
	 * CVar 에 적힌 장치를 엔진 캡처에 넣는다. 이미 이 캡처에 같은 장치를 넣었으면 아무것도 안 한다.
	 * 실패하면(장치를 뽑았거나 게임 실행 뒤에 꽂은 장치) 기본 장치로 되돌린다. 저장값은 그대로 둔다.
	 */
	static void ApplyCaptureDevice(const UObject* WorldContextObject);

	/** 내가 조종 중인 Pawn 의 보이스 컴포넌트. 관전 Pawn 도 같은 컴포넌트를 단다. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Voice", meta = (WorldContext = "WorldContextObject"))
	static UFAVoiceComponent* GetLocalVoiceComponent(const UObject* WorldContextObject);

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
