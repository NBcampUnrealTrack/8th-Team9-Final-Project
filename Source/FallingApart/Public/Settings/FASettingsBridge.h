// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SettingsKitTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FASettingsBridge.generated.h"

class USKSettingsSubsystem;
class USoundClass;
class USoundMix;

/**
 * 설정 슬라이더(0~1) 와 실제 값 사이의 환산.
 */
namespace FASettingsMapping
{
	/** 0.5 를 가운데 값에 맞춰 접은 선형 보간. 슬라이더 한가운데가 ini 튜닝값이 된다. */
	FALLINGAPART_API float MapWithMidpoint(float T, float Min, float Mid, float Max);

	/** MapWithMidpoint 의 역. 실제 값을 슬라이더 위치로 되돌린다. */
	FALLINGAPART_API float UnmapWithMidpoint(float Value, float Min, float Mid, float Max);

	//~ 아래 숫자는 Config/DefaultEngine.ini 의 튜닝 결과다. 가운데(0.02)가 기본값.
	inline constexpr float GateMin = 0.005f;
	inline constexpr float GateMid = 0.02f;
	inline constexpr float GateMax = 0.08f;

	/** 슬라이더 -> voice.MicNoiseGateThreshold */
	inline float ToNoiseGate(float T) { return MapWithMidpoint(T, GateMin, GateMid, GateMax); }

	/** voice.MicNoiseGateThreshold -> 슬라이더 */
	inline float FromNoiseGate(float NoiseGate) { return UnmapWithMidpoint(NoiseGate, GateMin, GateMid, GateMax); }

	/**
	 * 무음 판정(전송 임계)은 게이트보다 살짝 아래에 둔다.
	 * 게이트가 눌러 놓은 구간까지는 보내지 않되, 말 시작의 여린 부분은 살린다.
	 */
	inline float ToSilenceThreshold(float NoiseGate) { return NoiseGate * 0.75f; }
}

/**
 * SettingsKit 이 보관한 값을 이 게임의 실제 시스템에 밀어넣는 곳.
 *
 * 플러그인은 값만 들고 있고 적용은 전부 여기서 한다.
 */
UCLASS()
class FALLINGAPART_API UFASettingsBridge : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** 지금 설정값 기준으로 전부 다시 적용한다. 레벨이 바뀐 뒤에도 불린다. */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Settings")
	void ApplyAll();

	/**
	 * 마우스 감도 배율. 캐릭터의 Look 이 여기서 값을 가져간다.
	 * 설정이 아직 없으면 1.0 을 돌려주므로 호출부에 분기가 필요 없다.
	 */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Settings", meta = (WorldContext = "WorldContextObject"))
	static float GetMouseSensitivity(const UObject* WorldContextObject);

	/** 마우스 Y축 반전 여부. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Settings", meta = (WorldContext = "WorldContextObject"))
	static bool IsYAxisInverted(const UObject* WorldContextObject);

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	UFUNCTION()
	void HandleSettingsChanged(ESKSettingsTab Category);

	UFUNCTION()
	void HandleUISoundRequested(FName SoundId);

	/** 레벨이 바뀌면 SoundMix 푸시가 날아간다 — 새 월드에 다시 밀어야 한다. */
	void HandlePostLoadMap(UWorld* LoadedWorld);

	void ApplyAudio();
	void ApplyVoice();
	void ApplyVideo();

	USKSettingsSubsystem* GetSettings() const;

	/**
	 * SoundClass 와 그 자손 전부를 오디오 디바이스에 등록시키고, 동시에 하드 참조로 붙든다.
	 * 둘 다 안 하면 안 된다 — 이유는 아래 ReferencedSoundClasses 주석 참고.
	 */
	void RegisterAndKeepSoundClassTree(class FAudioDeviceManager* DeviceManager, USoundClass* Root);

	/**
	 * 우리가 볼륨 오버라이드를 걸어 둔 SoundClass 전부
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundClass>> ReferencedSoundClasses;

	/** 같은 이유로 SoundMix 도 붙든다. 푸시된 믹스도 디바이스가 날 포인터로 들고 있다. */
	UPROPERTY(Transient)
	TObjectPtr<USoundMix> ActiveSoundMix;

	/** SoundClass 애셋이 아직 없을 때 로그를 도배하지 않으려고 한 번만 경고한다. */
	bool bWarnedMissingSoundMix = false;

	FDelegateHandle PostLoadMapHandle;
};
