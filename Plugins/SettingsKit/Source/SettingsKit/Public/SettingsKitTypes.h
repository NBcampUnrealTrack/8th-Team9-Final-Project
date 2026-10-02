// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SettingsKitTypes.generated.h"

class UUserWidget;

/** 설정창 탭. 위젯 스위처의 인덱스와 같은 순서다. */
UENUM(BlueprintType)
enum class ESKSettingsTab : uint8
{
	Audio    UMETA(DisplayName = "오디오"),
	Voice    UMETA(DisplayName = "보이스"),
	Video    UMETA(DisplayName = "화면"),
	Controls UMETA(DisplayName = "조작"),

	MAX      UMETA(Hidden)
};

/** 색약 보정 유형. 실제 보정 적용은 게임 쪽(포스트 프로세스)이 한다. */
UENUM(BlueprintType)
enum class ESKColorblindType : uint8
{
	None        UMETA(DisplayName = "없음"),
	Deuteranope UMETA(DisplayName = "녹색맹"),
	Protanope   UMETA(DisplayName = "적색맹"),
	Tritanope   UMETA(DisplayName = "청색맹")
};

UENUM(BlueprintType)
enum class ESKVoiceMicMode : uint8
{
	/** 항상 열려 있다. 말하면 바로 나간다. */
	OpenMic    UMETA(DisplayName = "오픈 마이크"),

	/** 지정한 키를 누르고 있는 동안에만 나간다. */
	PushToTalk UMETA(DisplayName = "푸시투토크")
};

/** 조작 탭의 키바인딩 한 행. WBP 가 목록을 그릴 때 쓴다. */
USTRUCT(BlueprintType)
struct SETTINGSKIT_API FSKKeybindRow
{
	GENERATED_BODY()

	/** Enhanced Input PlayerMappableKeySettings 의 Name. 리바인딩 식별자다. */
	UPROPERTY(BlueprintReadOnly, Category = "SettingsKit")
	FName MappingName;

	UPROPERTY(BlueprintReadOnly, Category = "SettingsKit")
	FText DisplayName;

	/** 현재 할당된 키의 표시 문자열. */
	UPROPERTY(BlueprintReadOnly, Category = "SettingsKit")
	FText KeyName;
};

/**
 * 저장되는 설정값 전부.
 *
 * 구조체 하나로 묶은 이유는 두 가지다 —
 *  1. `UPROPERTY(Config)` 한 줄로 통째 저장/로드된다.
 *  2. 취소(되돌리기) 스냅샷이 그냥 구조체 복사다.
 *
 * 해상도·화질·VSync·프레임 제한은 여기 없다. 그건 엔진의 `UGameUserSettings` 가
 * 이미 저장·적용까지 다 하고 있어서 중복으로 들고 있으면 두 값이 어긋난다.
 */
USTRUCT(BlueprintType)
struct SETTINGSKIT_API FSKSettingsValues
{
	GENERATED_BODY()

	//~ ---------------- 오디오 (0~1) ----------------

	UPROPERTY(BlueprintReadOnly, Category = "SettingsKit|Audio")
	float MasterVolume = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "SettingsKit|Audio")
	float MusicVolume = 0.8f;

	UPROPERTY(BlueprintReadOnly, Category = "SettingsKit|Audio")
	float SFXVolume = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "SettingsKit|Audio")
	float UIVolume = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "SettingsKit|Audio")
	bool bMuted = false;

	//~ ---------------- 보이스 ----------------

	/**
	 * 보이스 수신 볼륨(0~1). 게임 쪽에서 배수로 환산해 쓴다 — 0.5 를 "지금 음량 그대로"로
	 * 잡는 게 보통이라 여기 기본값도 0.5 다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "SettingsKit|Voice")
	float VoiceOutputVolume = 0.5f;

	/**
	 * 내 마이크 입력 게인(0~1). 게임 쪽에서 실제 배수로 환산한다.
	 * 환산 상한을 2.0 배 넘게 잡으면 캡처 경로가 int16 에서 하드 클리핑한다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "SettingsKit|Voice")
	float VoiceInputGain = 0.5f;

	/**
	 * 마이크 게이트(무음 판정) 임계값(0~1). 높일수록 작은 소리를 잘라낸다.
	 * 마이크 감도가 사람마다 달라 한 세트 값으로 팀 전원을 맞출 수 없어서 슬라이더로 뺐다.
	 */
	//~ 0.5 = 슬라이더 한가운데. 브리지의 환산표가 가운데를 ini 튜닝값(0.02)에 맞춰 두었으므로
	//~ 여기 기본값도 0.5 여야 한다. 예전에 0.02 로 적어둬서, 새로 설치하면 게이트가
	//~ 0.0056 까지 내려가 사실상 꺼진 상태로 시작했다.
	UPROPERTY(BlueprintReadOnly, Category = "SettingsKit|Voice")
	float VoiceGateThreshold = 0.5f;

	/** 보이스 수신 전체 끄기. */
	UPROPERTY(BlueprintReadOnly, Category = "SettingsKit|Voice")
	bool bVoiceMuted = false;

	/**
	 * 마이크를 항상 열어 둘지(오픈 마이크), 키를 누르는 동안만 열지(푸시투토크).
	 * 기본은 오픈 마이크 — 푸시투토크는 키를 모르면 아예 말을 못 하는 상태가 된다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "SettingsKit|Voice")
	ESKVoiceMicMode MicMode = ESKVoiceMicMode::OpenMic;

	/**
	 * 마이크 입력 장치 이름. 비우면 시스템 기본 장치.
	 * 이름 목록을 주는 것도, 실제로 장치를 바꾸는 것도 게임 쪽이다(USKSettingsSubsystem::MicDeviceProvider).
	 * 장치를 뽑아도 값은 지우지 않는다 — 다시 꽂으면 그 장치로 돌아가야 한다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "SettingsKit|Voice")
	FString MicDeviceName;

	//~ ---------------- 화면 (엔진 GameUserSettings 에 없는 것만) ----------------

	/** 감마. 2.2 가 표준. */
	UPROPERTY(BlueprintReadOnly, Category = "SettingsKit|Video")
	float Gamma = 2.2f;

	/** 시야각(도). 적용은 게임 쪽 카메라가 한다. */
	UPROPERTY(BlueprintReadOnly, Category = "SettingsKit|Video")
	float FieldOfView = 90.f;

	UPROPERTY(BlueprintReadOnly, Category = "SettingsKit|Video")
	ESKColorblindType ColorblindType = ESKColorblindType::None;

	UPROPERTY(BlueprintReadOnly, Category = "SettingsKit|Video")
	bool bSubtitlesEnabled = true;

	UPROPERTY(BlueprintReadOnly, Category = "SettingsKit|Video")
	float SubtitleScale = 1.f;

	/** 컬처 코드("ko" · "en"). 비우면 시스템 기본. */
	UPROPERTY(BlueprintReadOnly, Category = "SettingsKit|Video")
	FString LanguageCulture;

	//~ ---------------- 조작 ----------------

	/** 마우스 감도 배율. 적용은 게임 쪽 캐릭터/컨트롤러가 한다. */
	UPROPERTY(BlueprintReadOnly, Category = "SettingsKit|Controls")
	float MouseSensitivity = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "SettingsKit|Controls")
	bool bInvertYAxis = false;
};

/**
 * 게임 전용 위젯을 설정창 탭 안에 끼워 넣는 지정.
 */
USTRUCT(BlueprintType)
struct FSKTabExtension
{
	GENERATED_BODY()

	/** 어느 탭에 넣을 것인가. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SettingsKit")
	ESKSettingsTab Tab = ESKSettingsTab::Audio;

	/** 넣을 위젯. WBP 여도 되고 C++ 위젯 클래스여도 된다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SettingsKit", meta = (MetaClass = "/Script/UMG.UserWidget"))
	TSoftClassPtr<UUserWidget> WidgetClass;
};
