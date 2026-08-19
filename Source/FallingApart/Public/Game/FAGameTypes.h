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
 * 포획(감염) 진행 상태. 서버가 채우고 전 클라이언트에 복제된다.
 *
 * 감염은 고정 길이 타이머가 아니다 — 잡힌 사람이 WASD 로 저항하면 차는 속도가 느려진다
 * (저항 0 이면 5초, 최대로 버티면 15초).
 */
USTRUCT(BlueprintType)
struct FFACaptureState
{
	GENERATED_BODY()

	/** 서버가 마지막으로 보낸 감염 진행도 0~1. 1 에 닿으면 감염이 확정된다. */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Capture")
	float InfectProgress = 0.0f;

	/**
	 * 그 시점의 초당 진행 속도. 저항이 셀수록 작아진다.
	 * 0 은 '멈춤' 이다 — 구출 QTE 중에는 이 값이 0 이라 게이지가 그 자리에 선다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Capture")
	float InfectRatePerSecond = 0.0f;

	/** InfectProgress 를 찍은 서버 월드 시각(초). 클라이언트는 이 시각부터 속도로 외삽한다. */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Capture")
	float ProgressServerTime = 0.0f;

	/** 포획 중인가. 진행도는 시작 직후 0 이라 활성 여부를 겸할 수 없어 따로 둔다. */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Capture")
	bool bActive = false;

	/** 이 캐릭터가 잡힌 쪽인가. false 면 잡고 있는 좀비. */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Capture")
	bool bIsVictim = false;

	/** 구출 QTE 줄다리기 진행도 0~1 (1 = 인간 승리 직전). QTE 중이 아니면 0. */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Capture")
	float ResistRatio = 0.0f;

	bool IsActive() const { return bActive; }
};

/** 구출 QTE(줄다리기)에서 이 캐릭터가 맡은 역할. */
UENUM(BlueprintType)
enum class EFAContestRole : uint8
{
	/** QTE 중이 아님 */
	None	= 0,
	/** 붙잡고 있는 좀비 — ContestGauge 를 들고 있는 호스트 */
	Zombie	= 1,
	/** 붙잡힌 인질 */
	Victim	= 2,
	/** 구하러 온 인간 */
	Rescuer	= 3
};

/**
 * 구출 QTE 진행 상태. 참가자 3명(좀비 · 인질 · 구조자) 전원의 캐릭터에 서버가 채우고 복제한다.
 */
USTRUCT(BlueprintType)
struct FFAContestState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Contest")
	EFAContestRole Role = EFAContestRole::None;

	/** 줄다리기 게이지 0~1. 1 이면 인간 승리(구출 성공) 직전, 0 이면 좀비 승리 직전. */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Contest")
	float Gauge = 0.5f;

	/** QTE 가 끝나는 서버 월드 시각(초). 프롬프트의 남은 시간 표시용. */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Contest")
	float EndServerTime = 0.0f;

	/**
	 * 게이지를 들고 있는 좀비.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Contest")
	TObjectPtr<AActor> HostZombie = nullptr;

	bool IsActive() const { return Role != EFAContestRole::None; }
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

/**
 * 라운드 결과 화면 한 줄 (GDD 8.4).
 *
 * 왜 스냅샷인가 — AFAPlayerState::CommitRoundScore() 가 라운드 점수를 매치 누적으로 넘기면서
 * RoundScore 를 0 으로 지운다. 그런데 결과 화면은 그 뒤 RoundEndDelay(8초) 동안 떠 있어야 하므로,
 * PlayerState 를 직접 읽으면 전부 0 이 찍힌다. 그래서 서버가 정산 직후 여기에 떠서 복제한다.
 */
USTRUCT(BlueprintType)
struct FFARoundResultRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Result")
	FString PlayerName;

	/** 자기 줄 강조용. 도중에 나간 사람은 null 이 될 수 있으니 이름은 위에 따로 들고 있는다. */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Result")
	TObjectPtr<class AFAPlayerState> PlayerState = nullptr;

	/** 이번 라운드 점수 — 팀/개인 4분할 그대로 (GDD 8.4 '팀/개인 분리 표시'). */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Result")
	FFAPlayerScore Score;

	/** 이번 라운드에 인간으로 버틴 시간(초). */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Result")
	float SurvivalTime = 0.0f;

	/** 감염되지 않고 끝까지 인간으로 남았는가. */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Result")
	bool bSurvived = false;

	/** 라운드가 끝난 시점의 진영. */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Result")
	EFATeam TeamAtRoundEnd = EFATeam::None;
};

/**
 * 한 라운드의 결과 전체. 서버가 채워 전 클라이언트로 복제한다.
 */
USTRUCT(BlueprintType)
struct FFARoundResultSnapshot
{
	GENERATED_BODY()

	/** 1부터 시작. 0 이면 아직 채워진 적이 없다는 뜻이라 유효성 판정을 겸한다. */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Result")
	int32 RoundIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Result")
	EFARoundResult Result = EFARoundResult::None;

	/** 생존 시간 내림차순으로 서버가 미리 정렬해 보낸다. */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Result")
	TArray<FFARoundResultRow> Rows;

	bool IsValid() const { return RoundIndex > 0; }
};
