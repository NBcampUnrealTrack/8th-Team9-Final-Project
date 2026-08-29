// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FAVoiceUserSettings.generated.h"

class APlayerState;

/** (플레이어 키, 최종 배수). 배수는 뮤트를 이미 반영한 값이다. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FFAOnPlayerVoiceSettingsChanged, const FString& /*PlayerKey*/, float /*NewScale*/);

UCLASS()
class FALLINGAPART_API UFAVoiceUserSettings : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "FallingApart|Voice", meta = (WorldContext = "WorldContextObject"))
	static UFAVoiceUserSettings* Get(const UObject* WorldContextObject);

	/** 이 PlayerState 의 저장 키. UniqueNetId 가 아직 안 왔으면 빈 문자열이다. */
	static FString MakePlayerKey(const APlayerState* PlayerState);

	static constexpr float MinPlayerVolume = 0.f;
	static constexpr float MaxPlayerVolume = 2.f;
	static constexpr float DefaultPlayerVolume = 1.f;

	/** 저장된 배수(0.0 ~ 2.0). 없으면 1.0. 뮤트는 반영되지 않은 원래 값이다. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Voice")
	float GetPlayerVolume(const FString& PlayerKey) const;

	UFUNCTION(BlueprintCallable, Category = "FallingApart|Voice")
	void SetPlayerVolume(const FString& PlayerKey, float NewVolume);

	UFUNCTION(BlueprintPure, Category = "FallingApart|Voice")
	bool IsPlayerMuted(const FString& PlayerKey) const;

	/** 뮤트해도 볼륨값은 그대로 남는다 — 풀면 원래 크기로 돌아온다. */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Voice")
	void SetPlayerMuted(const FString& PlayerKey, bool bMuted);

	/** 실제로 재생에 곱해질 배수. 뮤트면 0. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Voice")
	float GetPlayerVolumeScale(const FString& PlayerKey) const;

	UFUNCTION(BlueprintCallable, Category = "FallingApart|Voice")
	void ResetPlayer(const FString& PlayerKey);

	UFUNCTION(BlueprintCallable, Category = "FallingApart|Voice")
	void ResetAll();

	/** 지금 저장돼 있는 플레이어 키 전부. 디버그 명령용. */
	void GetConfiguredPlayers(TArray<FString>& OutKeys) const;

	/**
	 * 값이 바뀔 때마다. UFAVoiceComponent 가 구독해서 자기 화자면 즉시 반영한다.
	 * 설정창을 닫지 않아도, 말하는 도중에도 바로 적용된다.
	 */
	FFAOnPlayerVoiceSettingsChanged OnPlayerVoiceSettingsChanged;

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	struct FEntry
	{
		float Volume = UFAVoiceUserSettings::DefaultPlayerVolume;
		bool bMuted = false;

		bool IsDefault() const { return bMuted == false && FMath::IsNearlyEqual(Volume, UFAVoiceUserSettings::DefaultPlayerVolume); }
	};

	FEntry& FindOrAddEntry(const FString& PlayerKey);
	void NotifyChanged(const FString& PlayerKey);

	void LoadFromConfig();
	void SaveToConfig();

	/** 슬라이더를 끄는 동안 매 프레임 ini 를 쓰지 않도록 잠깐 모아서 한 번에 쓴다. */
	void RequestSave();

	/** ini 키로 쓸 수 있게 영숫자만 남긴다. 스팀 ID 는 숫자라 그대로 남는다. */
	static FString SanitizeKey(const FString& PlayerKey);

	TMap<FString, FEntry> Entries;

	FTimerHandle SaveTimerHandle;
	bool bDirty = false;
};
