// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Game/FAGameTypes.h"
#include "FAGameState.generated.h"

class AFAPlayerState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFAOnRoundPhaseChanged, EFARoundPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFAOnRoundEnded, int32, RoundIndex, EFARoundResult, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFAOnDraftRoundStarted, int32, DraftIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFAOnTeamCountsChanged);

/**
 * 라운드 진행 상태를 전 클라이언트에 복제 (GDD 2.3 · 8.1).
 *
 * 타이머는 '남은 시간'을 매초 복제하지 않고 라운드 시작 시각만 복제한 뒤
 * GetServerWorldTimeSeconds() 로 각 클라이언트가 직접 계산
 *
 * 상태 변경은 전부 서버(AFAGameMode) 권한
 */
UCLASS()
class FALLINGAPART_API AFAGameState : public AGameState
{
	GENERATED_BODY()

public:
	AFAGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintAssignable, Category = "FallingApart|Game")
	FFAOnRoundPhaseChanged OnRoundPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "FallingApart|Game")
	FFAOnRoundEnded OnRoundEnded;

	/** 30초 주기 드래프트가 시작될 때 (드래프트 UI 오픈 트리거) */
	UPROPERTY(BlueprintAssignable, Category = "FallingApart|Game")
	FFAOnDraftRoundStarted OnDraftRoundStarted;

	UPROPERTY(BlueprintAssignable, Category = "FallingApart|Game")
	FFAOnTeamCountsChanged OnTeamCountsChanged;

	//~ 조회 — 서버 / 클라이언트 공통
	UFUNCTION(BlueprintPure, Category = "FallingApart|Game")
	EFARoundPhase GetRoundPhase() const { return RoundPhase; }

	UFUNCTION(BlueprintPure, Category = "FallingApart|Game")
	bool IsRoundInProgress() const { return RoundPhase == EFARoundPhase::InProgress; }

	/** 1부터 시작하는 현재 라운드 번호. 매치 시작 전에는 0 */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Game")
	int32 GetCurrentRound() const { return CurrentRound; }

	UFUNCTION(BlueprintPure, Category = "FallingApart|Game")
	int32 GetMaxRound() const { return MaxRound; }

	UFUNCTION(BlueprintPure, Category = "FallingApart|Game")
	EFARoundResult GetLastRoundResult() const { return LastRoundResult; }

	/** HUD 상단 중앙 라운드 타이머용 남은 시간(초) */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Game")
	float GetRoundTimeRemaining() const;

	UFUNCTION(BlueprintPure, Category = "FallingApart|Game")
	float GetRoundElapsedTime() const;

	/** 다음 드래프트까지 남은 시간(초). 이번 라운드 드래프트가 모두 끝났으면 -1 */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Game")
	float GetTimeToNextDraft() const;

	/** 이번 라운드에 완료된 드래프트 회차 (0~6).티어 곡선 조회에 쓴다 (GDD 4.2) */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Game")
	int32 GetDraftsCompleted() const { return DraftsCompleted; }

	UFUNCTION(BlueprintPure, Category = "FallingApart|Game")
	int32 GetSurvivorCount() const { return SurvivorCount; }

	UFUNCTION(BlueprintPure, Category = "FallingApart|Game")
	int32 GetZombieCount() const { return ZombieCount; }

	/** 지정 진영의 플레이어 목록을 채운다. */
	void GetPlayerStatesByTeam(EFATeam InTeam, TArray<AFAPlayerState*>& OutPlayerStates) const;

	/**
	 * 매치 누적 점수 기준 순위표 생성 (GDD 5.3 타이브레이크 체인).
	 * 총점 → 진영 기여도 → 누적 생존 시간 순으로 판정, 결과 UI 그대로 사용
	 */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Score")
	void GetRankedPlayerStates(TArray<AFAPlayerState*>& OutRanked) const;

	/** 타이브레이크 3차까지 완전 동점인가 — 데스매치 발동 판정용 (GDD 5.4) */
	static bool AreFullyTied(const AFAPlayerState& A, const AFAPlayerState& B);

	/** A 가 B 보다 상위 순위인가 */
	static bool CompareRank(const AFAPlayerState& A, const AFAPlayerState& B);

	//~ 서버 전용 — AFAGameMode 만 호출
	void SetRoundPhase(EFARoundPhase NewPhase);
	void SetCurrentRound(int32 InRound);
	void SetMaxRound(int32 InMaxRound);
	void SetLastRoundResult(EFARoundResult InResult);

	/** 라운드 실제 시작 시점에 타이머 기준값을 한 번에 설정. */
	void StartRoundTimer(float ServerStartTime, float InRoundDuration, float InDraftInterval, int32 InMaxDraftsPerRound);

	void SetDraftsCompleted(int32 InCount);
	void SetTeamCounts(int32 InSurvivorCount, int32 InZombieCount);

	/** 드래프트 회차 알림 (서버에서 호출 → 전 클라이언트 UI 오픈) */
	void NotifyDraftRoundStarted(int32 DraftIndex);

protected:
	UFUNCTION()
	void OnRep_RoundPhase();

	UFUNCTION()
	void OnRep_SurvivorCount();

	UFUNCTION()
	void OnRep_ZombieCount();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_DraftRoundStarted(int32 DraftIndex);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_RoundEnded(int32 RoundIndex, EFARoundResult Result);

private:
	UPROPERTY(ReplicatedUsing = OnRep_RoundPhase, VisibleAnywhere, Category = "FallingApart|Game")
	EFARoundPhase RoundPhase = EFARoundPhase::WaitingToStart;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "FallingApart|Game")
	int32 CurrentRound = 0;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "FallingApart|Game")
	int32 MaxRound = 5;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "FallingApart|Game")
	EFARoundResult LastRoundResult = EFARoundResult::None;

	/** 라운드가 InProgress 로 전환된 서버 월드 시간 */
	UPROPERTY(Replicated, VisibleAnywhere, Category = "FallingApart|Game")
	float RoundStartServerTime = 0.0f;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "FallingApart|Game")
	float RoundDuration = 210.0f;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "FallingApart|Game")
	float DraftInterval = 30.0f;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "FallingApart|Game")
	int32 MaxDraftsPerRound = 6;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "FallingApart|Game")
	int32 DraftsCompleted = 0;

	UPROPERTY(ReplicatedUsing = OnRep_SurvivorCount, VisibleAnywhere, Category = "FallingApart|Game")
	int32 SurvivorCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_ZombieCount, VisibleAnywhere, Category = "FallingApart|Game")
	int32 ZombieCount = 0;
};
