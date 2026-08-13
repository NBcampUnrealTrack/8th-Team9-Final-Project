// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FASoundTypes.generated.h"

class USoundAttenuation;
class USoundBase;
class USoundConcurrency;

/**
 * 사운드가 속한 갈래. Sound Class 계층과 1:1 은 아니고, 표를 나누는 기준과
 * 감쇠 기본값을 고르는 데 쓴다 (실제 믹싱 계층은 Sound Cue 애셋의 Sound Class 가 정한다).
 */
UENUM(BlueprintType)
enum class EFASoundCategory : uint8
{
	/** 메뉴 · 버튼 · 팝업. 2D */
	UI			= 0,
	/** 드래프트 창. 2D */
	Draft		= 1,
	/** 발소리 · 점프 · 물. 3D */
	Move		= 2,
	/** 포획 · 감염 · 구출 QTE */
	Combat		= 3,
	/** 라운드 · 점수 · 데스매치. 2D */
	Round		= 4,
	/** BGM · 스팅어. 2D */
	Music		= 5,
	/** 구역 루프 */
	Ambience	= 6,
	/** 룰 적용 · 해제 */
	Rule		= 7,
	/** 근접 보이스 상태 알림 */
	Voice		= 8
};

/**
 * 사운드 표 한 행. DT_Sound_UI / _Character / _Game / _Rule / _Music 다섯 장이 이 구조를 공유한다.
 *
 * RowName 이 곧 SoundId 다 — 코드·블루프린트는 이 문자열 하나만 알면 소리를 낼 수 있다.
 * 표를 다섯 장으로 쪼갠 이유는 .uasset 이 머지가 안 되기 때문이다. 담당자별로 자기 표만 건드리면
 * 사운드 작업이 남의 진도를 막지 않는다.
 */
USTRUCT(BlueprintType)
struct FALLINGAPART_API FFASoundRow : public FTableRowBase
{
	GENERATED_BODY()

	/**
	 * SoundCue 또는 MetaSound.
	 * 소프트 참조라 표를 읽는 것만으로는 음원이 로드되지 않는다 — 메뉴만 켠 상태에서
	 * 인게임 사운드 전부가 메모리에 올라오면 안 된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TSoftObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	EFASoundCategory Category = EFASoundCategory::UI;

	/** true = 화면 소리(UI·BGM), false = 월드 3D. 3D 인데 위치를 안 주고 부르면 듣는 사람 위치에서 난다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	bool bIs2D = true;

	/** 3D 일 때만. 비워두면 프로젝트 설정의 카테고리 기본값을 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound|3D")
	TSoftObjectPtr<USoundAttenuation> Attenuation;

	/** 발소리·QTE 연타처럼 겹치는 소리의 동시 재생 상한. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TSoftObjectPtr<USoundConcurrency> Concurrency;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound|Mix")
	float VolumeMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound|Mix")
	float PitchMultiplier = 1.0f;

	/** ±. 발소리·타격음은 0.05~0.15 를 줘야 같은 파일이 반복돼도 기계음처럼 안 들린다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound|Mix", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PitchRandomRange = 0.0f;

	/** 초. 같은 ID 를 이 간격 안에 다시 부르면 무시한다 (연타 스팸 방지). 0 이면 제한 없음. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound|Mix", meta = (ClampMin = "0.0"))
	float RetriggerCooldown = 0.0f;

	/** 앰비언스·게이지·숨소리. 루프는 반드시 반환된 AudioComponent 를 들고 있다가 직접 멈춰야 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	bool bLooping = false;

	/** 붙여서 재생할 때, 붙은 액터가 사라지면 소리도 멈출지. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	bool bStopOnOwnerDestroy = true;
};
