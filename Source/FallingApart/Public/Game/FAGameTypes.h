// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FAGameTypes.generated.h"

/** 플레이어 진영. 감염되면 Human -> Zombie 로 전환 역방향은 없음 */
UENUM(BlueprintType)
enum class EFATeam : uint8
{
	/** 미배정 / 관전 */
	None		= 0,
	Human		= 1,
	Zombie		= 2
};

/** 라운드 진행 단계. GameState 가 복제해 HUD·UI 가 참조 */
UENUM(BlueprintType)
enum class EFARoundPhase : uint8
{
	/** 매치 시작 대기 (최소 인원 충족 대기) */
	WaitingToStart	= 0,
	/** 진영 배정 후 시작 카운트다운 */
	RoundStarting	= 1,
	/** 술래잡기 진행 중 — 라운드 타이머 / 드래프트 타이머 동작 */
	InProgress		= 2,
	/** 라운드 정산 및 결과 표시 */
	RoundEnd		= 3,
	/** 5라운드 종료, 최종 순위표 */
	MatchEnd		= 4,
	/** 1등 동점 서든데스 데스매치 (GDD 5.4 · MVP 제외) */
	DeathMatch		= 5
};

/** 라운드 승패. 인간은 제한 시간 도달로, 좀비는 인간 전멸로 승리. */
UENUM(BlueprintType)
enum class EFARoundResult : uint8
{
	None		= 0,
	HumanWin	= 1,
	ZombieWin	= 2
};

/**
 * 포획(감염) 진행 상태. 서버가 채우고 잡은 좀비 · 잡힌 인간 양쪽 캐릭터에 복제된다.
 * 각 클라이언트의 UFAHUDWidget 이 이 상태를 폴링해서 한다.
 */
USTRUCT(BlueprintType)
struct FFACaptureState
{
	GENERATED_BODY()

	/** 감염이 완료되는 서버 월드 시각(초). TotalDuration 이 0 이면 의미 없음. */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Capture")
	float EndServerTime = 0.0f;

	/** 감염까지 걸리는 총 시간(초). 0 이면 포획 중이 아니다 — 이 값 하나가 활성 여부를 겸한다. */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Capture")
	float TotalDuration = 0.0f;

	/**
	 * 구출 QTE 로 감염 타이머가 멈춰 있는 동안의 남은 시간(초).
	 * 0 보다 크면 게이지는 이 값에서 멈춘다 — 서버 시각으로 계산하면 QTE 중에도 혼자 채워진다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Capture")
	float PausedRemaining = 0.0f;

	/** 이 캐릭터가 잡힌 쪽인가. false 면 잡고 있는 좀비. */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Capture")
	bool bIsVictim = false;

	/** 구출 QTE 줄다리기 진행도 0~1 (1 = 인간 승리 직전). QTE 중이 아니면 0. */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Capture")
	float ResistRatio = 0.0f;

	bool IsActive() const { return TotalDuration > 0.0f; }
};

/**
 * 플레이어 점수 집계 단위 (GDD 5부 · 기술문서 4.1).
 */
USTRUCT(BlueprintType)
struct FFAPlayerScore
{
	GENERATED_BODY()

	/** 인간 팀 점수 — 생존 +1 × 생존자 수 */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Score")
	int32 HumanTeam = 0;

	/** 인간 개인 점수 — 구출 +1, 최장 생존 +2 */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Score")
	int32 HumanPersonal = 0;

	/** 좀비 팀 점수 — 인간 전멸 +5 */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Score")
	int32 ZombieTeam = 0;

	/** 좀비 개인 점수 — 감염 성공 +2 */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Score")
	int32 ZombiePersonal = 0;

	/** 인간 상태를 유지한 시간(초). 타이브레이크 3차 기준 */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Score")
	float SurvivalTime = 0.0f;

	/** 이번 라운드에 진영 전환이 있었는가 — 인간 팀 점수 지급 대상에서 제외하는 플래그 */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Score")
	bool bWasInfectedThisRound = false;

	int32 GetTotal() const { return HumanTeam + HumanPersonal + ZombieTeam + ZombiePersonal; }
	int32 GetHumanTotal() const { return HumanTeam + HumanPersonal; }
	int32 GetZombieTotal() const { return ZombieTeam + ZombiePersonal; }

	/** 라운드 점수를 매치 누적에 합산한다. bWasInfectedThisRound 는 라운드 한정 플래그라 합산하지 않음. */
	void Add(const FFAPlayerScore& Other)
	{
		HumanTeam += Other.HumanTeam;
		HumanPersonal += Other.HumanPersonal;
		ZombieTeam += Other.ZombieTeam;
		ZombiePersonal += Other.ZombiePersonal;
		SurvivalTime += Other.SurvivalTime;
	}

	void Reset() { *this = FFAPlayerScore(); }
};
