// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SettingsKitTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SettingsKitSubsystem.generated.h"

class ULocalPlayer;
class UEnhancedInputUserSettings;
class UInputMappingContext;

/** 어느 카테고리가 바뀌었는지 알린다. 받는 쪽은 GetValues() 로 현재 값을 다시 읽는다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSKOnSettingsChanged, ESKSettingsTab, Category);

/** 설정창이 소리를 내달라고 요청한다(클릭·호버·볼륨 미리듣기). 게임이 자기 사운드 시스템에 연결한다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSKOnUISoundRequested, FName, SoundId);

/**
 * 설정값의 보관·저장·되돌리기를 맡는 곳. 플러그인의 심장.
 *
 * 이 서브시스템은 오디오를 직접 만지지 않음. 볼륨을 SoundMix 에 밀어넣거나 마이크 게인
 * CVar 을 건드리는 건 전부 게임 쪽 몫이고, 여기서는 `OnSettingsChanged` 로 "바뀌었다"만 알림.
 * 그래야 이미 오디오 시스템을 가진 프로젝트에 얹어도 두 시스템이 볼륨을 놓고 싸우지 않음.
 */
UCLASS(Config = GameUserSettings)
class SETTINGSKIT_API USKSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** 월드 컨텍스트 아무거나로 가져온다. GameInstance 가 없으면 nullptr. */
	UFUNCTION(BlueprintPure, Category = "SettingsKit", meta = (WorldContext = "WorldContextObject"))
	static USKSettingsSubsystem* Get(const UObject* WorldContextObject);

	//~ ---------------- 값 읽기 ----------------

	/** 지금 편집 중인 값 전부. 아직 저장되지 않았을 수 있다. */
	const FSKSettingsValues& GetValues() const { return Values; }

	/** 블루프린트용 사본. 참조 반환은 UFUNCTION 이 못 받아서 값으로 준다. */
	UFUNCTION(BlueprintPure, Category = "SettingsKit", meta = (DisplayName = "Get Values"))
	FSKSettingsValues GetValuesCopy() const { return Values; }

	/** 적용 버튼을 눌러야 할 변경이 남아 있는가. */
	UFUNCTION(BlueprintPure, Category = "SettingsKit")
	bool IsDirty() const { return bDirty; }

	//~ ---------------- 오디오 ----------------

	UFUNCTION(BlueprintCallable, Category = "SettingsKit|Audio")
	void SetMasterVolume(float NewVolume);

	UFUNCTION(BlueprintCallable, Category = "SettingsKit|Audio")
	void SetMusicVolume(float NewVolume);

	UFUNCTION(BlueprintCallable, Category = "SettingsKit|Audio")
	void SetSFXVolume(float NewVolume);

	UFUNCTION(BlueprintCallable, Category = "SettingsKit|Audio")
	void SetUIVolume(float NewVolume);

	UFUNCTION(BlueprintCallable, Category = "SettingsKit|Audio")
	void SetMuted(bool bNewMuted);

	//~ ---------------- 보이스 ----------------

	UFUNCTION(BlueprintCallable, Category = "SettingsKit|Voice")
	void SetVoiceOutputVolume(float NewVolume);

	UFUNCTION(BlueprintCallable, Category = "SettingsKit|Voice")
	void SetVoiceInputGain(float NewGain);

	UFUNCTION(BlueprintCallable, Category = "SettingsKit|Voice")
	void SetVoiceGateThreshold(float NewThreshold);

	UFUNCTION(BlueprintCallable, Category = "SettingsKit|Voice")
	void SetVoiceMuted(bool bNewMuted);

	/** 오픈 마이크 / 푸시투토크. 실제 마이크 제어는 게임이 OnSettingsChanged 를 받아서 한다. */
	UFUNCTION(BlueprintCallable, Category = "SettingsKit|Voice")
	void SetVoiceMicMode(ESKVoiceMicMode NewMode);

	//~ ---------------- 화면 ----------------

	UFUNCTION(BlueprintCallable, Category = "SettingsKit|Video")
	void SetGamma(float NewGamma);

	UFUNCTION(BlueprintCallable, Category = "SettingsKit|Video")
	void SetFieldOfView(float NewFOV);

	UFUNCTION(BlueprintCallable, Category = "SettingsKit|Video")
	void SetColorblindType(ESKColorblindType NewType);

	UFUNCTION(BlueprintCallable, Category = "SettingsKit|Video")
	void SetSubtitlesEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "SettingsKit|Video")
	void SetSubtitleScale(float NewScale);

	/** 컬처 코드. 빈 문자열이면 시스템 기본으로 되돌린다. 즉시 반영된다. */
	UFUNCTION(BlueprintCallable, Category = "SettingsKit|Video")
	void SetLanguageCulture(const FString& NewCulture);

	//~ ---------------- 조작 ----------------

	UFUNCTION(BlueprintCallable, Category = "SettingsKit|Controls")
	void SetMouseSensitivity(float NewSensitivity);

	UFUNCTION(BlueprintCallable, Category = "SettingsKit|Controls")
	void SetInvertYAxis(bool bInvert);

	//~ ---------------- 저장 · 되돌리기 ----------------

	/** 지금 값을 디스크에 확정한다. 엔진 GameUserSettings 도 함께 저장·적용된다. */
	UFUNCTION(BlueprintCallable, Category = "SettingsKit")
	void ApplyAndSave();

	/**
	 * "저장 안 한 변경이 있다"고만 표시
	 */
	UFUNCTION(BlueprintCallable, Category = "SettingsKit")
	void MarkDirty() { bDirty = true; }

	/** 되돌리기 기준점을 지금 값으로 잡는다. 설정창을 열 때 호출한다. */
	UFUNCTION(BlueprintCallable, Category = "SettingsKit")
	void TakeSnapshot();

	/** 스냅샷 시점으로 되돌린다. 실시간 미리보기로 이미 적용된 것들도 원복된다. */
	UFUNCTION(BlueprintCallable, Category = "SettingsKit")
	void RevertToSnapshot();

	/** 한 탭만 기본값으로. 다른 탭 값은 그대로 둔다. */
	UFUNCTION(BlueprintCallable, Category = "SettingsKit")
	void ResetCategoryToDefaults(ESKSettingsTab Category);

	UFUNCTION(BlueprintCallable, Category = "SettingsKit")
	void ResetAllToDefaults();

	/** 전 카테고리를 다시 브로드캐스트한다. 리스너가 늦게 붙었을 때 초기값을 받는 통로. */
	UFUNCTION(BlueprintCallable, Category = "SettingsKit")
	void BroadcastAll();

	//~ ---------------- 키 리바인딩 (Enhanced Input) ----------------

	/**
	 * 리바인딩 가능한 매핑 목록. 비어 있으면 아직 준비가 안 된 것이다 —
	 * Project Settings → Enhanced Input → Enable User Settings 를 켜고,
	 * IMC 의 각 매핑에 Player Mappable Key Settings를 붙여야 목록에 올라온다.
	 */
	UFUNCTION(BlueprintCallable, Category = "SettingsKit|Controls")
	TArray<FSKKeybindRow> GetKeybindRows(ULocalPlayer* LocalPlayer) const;

	/**
	 * IMC 하나를 리바인딩 대상으로 등록한다. 그 안의 매핑들이 키 프로필에 올라온다.
	 *
	 * 보통은 Project Settings → Plugins → Settings Kit 의 목록으로 충분하고, 이건 런타임에
	 * 만들어지는 IMC 처럼 미리 적어둘 수 없는 경우를 위한 문이다. 두 번 불러도 안전하다.
	 */
	UFUNCTION(BlueprintCallable, Category = "SettingsKit|Controls")
	bool RegisterMappingContextForRebinding(ULocalPlayer* LocalPlayer, const UInputMappingContext* MappingContext);

	/**
	 * 매핑에 새 키를 건다. 다른 매핑이 이미 그 키를 쓰고 있으면 거부하고 OutError 를 채운다.
	 * @return 실제로 바뀌었으면 true.
	 */
	UFUNCTION(BlueprintCallable, Category = "SettingsKit|Controls")
	bool TryRebindKey(ULocalPlayer* LocalPlayer, FName MappingName, const FKey& NewKey, FText& OutError);

	/** 키 배치를 전부 기본값으로. */
	UFUNCTION(BlueprintCallable, Category = "SettingsKit|Controls")
	void ResetKeybindsToDefaults(ULocalPlayer* LocalPlayer);

	//~ ---------------- 델리게이트 ----------------

	/** 값이 바뀔 때마다. 게임 쪽 적용 코드가 여기에 붙는다. */
	UPROPERTY(BlueprintAssignable, Category = "SettingsKit")
	FSKOnSettingsChanged OnSettingsChanged;

	/** 설정창이 소리를 요청할 때. 게임이 자기 오디오 시스템에 연결한다. */
	UPROPERTY(BlueprintAssignable, Category = "SettingsKit")
	FSKOnUISoundRequested OnUISoundRequested;

	/** 설정창이 부른다. 아무도 안 듣고 있으면 조용히 무시된다. */
	UFUNCTION(BlueprintCallable, Category = "SettingsKit")
	void RequestUISound(FName SoundId);

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	/** 값 변경 후 공통 처리 — dirty 표시 + 브로드캐스트. */
	void NotifyChanged(ESKSettingsTab Category);

	/** 감마를 엔진에 반영. 이 플러그인이 직접 적용하는 유일한 항목이다. */
	void ApplyGamma() const;

	/** 로컬 플레이어의 Enhanced Input 사용자 설정. 기능이 꺼져 있으면 nullptr. */
	static UEnhancedInputUserSettings* GetInputUserSettings(ULocalPlayer* LocalPlayer);

	/**
	 * 프로젝트 설정에 적어둔 IMC 들을 사용자 설정에 등록시킨다. 키 목록을 읽기 전에 부른다.
	 * 이미 등록된 것은 알아서 건너뛰므로 매번 불러도 된다.
	 */
	static void EnsureRebindableContextsRegistered(ULocalPlayer* LocalPlayer);

	/**
	 * 저장되는 값 전부. 구조체 한 덩어리라 ini 에도 한 줄로 들어간다.
	 * 필드를 나중에 추가해도 기존 ini 는 그대로 읽힌다(없는 필드는 기본값 유지).
	 */
	UPROPERTY(Config)
	FSKSettingsValues Values;

	/** 취소 시 돌아갈 지점. 설정창을 열 때 잡는다. */
	FSKSettingsValues Snapshot;

	bool bDirty = false;
};
