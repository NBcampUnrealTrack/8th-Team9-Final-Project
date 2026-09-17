// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Game/FAGameTypes.h"
#include "FAPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFAOnTeamChanged, EFATeam, NewTeam);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFAOnScoreChanged);

/**
 * 플레이어 1인의 진영과 점수를 보관·복제
 *
 * 점수는 라운드 점수(RoundScore)와 매치 누적(MatchScore)
 * 라운드 종료 정산이 끝나면 GameMode 가 CommitRoundScore() 로 매치 누적에 합산
 * HUD 가 '라운드 누적 / 매치 누적'을 동시에 보여줘야함
 *
 * 점수 변경은 전부 서버 권한
 */
UCLASS()
class FALLINGAPART_API AFAPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AFAPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* PlayerState) override;

	/** 진영이 바뀔 때 브로드캐스트 (HUD 시야 필터·아이콘 갱신용) */
	UPROPERTY(BlueprintAssignable, Category = "FallingApart|Game")
	FFAOnTeamChanged OnTeamChanged;

	/** 점수가 바뀔 때 브로드캐스트 (HUD 우하단 점수·순위표 갱신용) */
	UPROPERTY(BlueprintAssignable, Category = "FallingApart|Score")
	FFAOnScoreChanged OnScoreChanged;

	//~ 조회 — 서버 / 클라이언트 공통
	UFUNCTION(BlueprintPure, Category = "FallingApart|Game")
	EFATeam GetTeam() const { return Team; }

	UFUNCTION(BlueprintPure, Category = "FallingApart|Game")
	bool IsHuman() const { return Team == EFATeam::Human; }

	UFUNCTION(BlueprintPure, Category = "FallingApart|Game")
	bool IsZombie() const { return Team == EFATeam::Zombie; }

	const FFAPlayerScore& GetRoundScore() const { return RoundScore; }
	const FFAPlayerScore& GetMatchScore() const { return MatchScore; }

	/** 이번 라운드에 획득한 점수 합 */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Score")
	int32 GetRoundTotalScore() const { return RoundScore.GetTotal(); }

	/** 정산이 끝난 매치 누적 점수 합 — 최종 순위 판정 기준 */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Score")
	int32 GetMatchTotalScore() const { return MatchScore.GetTotal(); }

	/** HUD 표기용 — 매치 누적 + 아직 정산되지 않은 진행 중 라운드 점수 */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Score")
	int32 GetDisplayTotalScore() const { return MatchScore.GetTotal() + RoundScore.GetTotal(); }

	/**
	 * HUD 표기용 팀 점수 — 위와 같은 기준(매치 누적 + 진행 중 라운드)에서 팀 계만.
	 * 인간 계와 좀비 계는 동시에 늘지 않으므로 한 칸에 합쳐 보여준다.
	 */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Score")
	int32 GetDisplayTeamScore() const
	{
		return MatchScore.HumanTeam + MatchScore.ZombieTeam + RoundScore.HumanTeam + RoundScore.ZombieTeam;
	}

	/** HUD 표기용 개인 점수 — 같은 기준에서 개인 계만. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Score")
	int32 GetDisplayPersonalScore() const
	{
		return MatchScore.HumanPersonal + MatchScore.ZombiePersonal + RoundScore.HumanPersonal + RoundScore.ZombiePersonal;
	}

	UFUNCTION(BlueprintPure, Category = "FallingApart|Score")
	float GetMatchSurvivalTime() const { return MatchScore.SurvivalTime; }

	UFUNCTION(BlueprintPure, Category = "FallingApart|Score")
	bool WasInfectedThisRound() const { return RoundScore.bWasInfectedThisRound; }

	/** 타이브레이크 2차용 주 진영 — 인간 계와 좀비 계 중 점수가 큰 쪽 (GDD 5.3) */
	EFATeam GetMainSide() const;

	/** 주 진영의 점수 절대값 */
	int32 GetMainSideScore() const;

	/** 주 진영 점수 ÷ 총점. 주 진영이 서로 다른 두 플레이어를 비교할 때 쓴다. */
	float GetMainSideRatio() const;

	/**
	 * 데스매치 최종 순위 — 1 이 승자, 2 가 패자. 0 이면 데스매치에 나가지 않았다는 뜻.
	 * 타이브레이크 4차 기준. 완전 동점자끼리만 값이 붙으므로 0 과 섞어 비교할 일은 없다.
	 */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Score")
	int32 GetDeathMatchPlacement() const { return DeathMatchPlacement; }

	//~ 서버 전용 — GameMode 만 호출
	/** 진영을 설정. 감염 처리는 MarkInfected() 를 쓸 것. */
	void SetTeam(EFATeam NewTeam);

	/** 매치 시작 시 점수·진영 초기화 */
	void ResetForNewMatch();

	/** 라운드 시작 시 라운드 점수와 생존 시간 계측을 초기화. */
	void ResetForNewRound();

	/** 인간 상태 진입 시각을 기록해 생존 시간 계측을 시작. */
	void StartSurvivalClock(float ServerTimeNow);

	/** 계측 중이던 생존 시간을 라운드 점수에 확정 반영 (감염 또는 라운드 종료 시). */
	void FinalizeSurvivalTime(float ServerTimeNow);

	/**
	 * 감염 확정 처리 — 생존 시간을 확정하고, 인간 팀 점수 자격을 잃고, 좀비로 전환.
	 * 이미 적립된 인간 개인 점수(구출 +1 등)는 그대로 유지된다 (GDD 5.2).
	 */
	void MarkInfected(float ServerTimeNow);

	void AddHumanTeamScore(int32 Amount);
	void AddHumanPersonalScore(int32 Amount);
	void AddZombieTeamScore(int32 Amount);
	void AddZombiePersonalScore(int32 Amount);

	/** 라운드 점수를 매치 누적에 합산하고 라운드 점수를 비운다. */
	void CommitRoundScore();

	/** 데스매치 결과 확정. UFADeathMatchSubsystem 만 호출한다. */
	void SetDeathMatchPlacement(int32 InPlacement);
	
	/**
	 * 드래프트 UI를 여는 신호. 서버가 후보 3장을 담아 호출한다.
	 * @param SelectionTime 선택 제한 시간(초). 이 시간이 지나면 서버가 무작위로 확정하므로
	 *                      클라이언트 카운트다운은 반드시 서버가 준 이 값을 써야 한다.
	 */
	UFUNCTION(Client, Reliable)
	void Client_ShowDraftUI(const TArray<FName>& CandidateRuleIds, float SelectionTime);
	
	/**
	 * 열려 있는 드래프트 UI 를 선택 없이 닫는 신호.
	 * 라운드가 끝나면 서버가 남은 드래프트를 전부 취소하고 이걸 보낸다 — 안 보내면
	 * 다음 라운드가 시작된 뒤에도 이전 라운드의 선택창이 화면에 남는다.
	 */
	UFUNCTION(Client, Reliable)
	void Client_HideDraftUI();

	/** 플레이어가 후보 중 하나를 골랐을 때 클라이언트가 호출. 서버가 검증 후 확정한다. */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_ConfirmDraft(int32 SelectedIndex);

protected:
	UFUNCTION()
	void OnRep_Team();

	UFUNCTION()
	void OnRep_RoundScore();

	UFUNCTION()
	void OnRep_MatchScore();

	UFUNCTION()
	void OnRep_DeathMatchPlacement();


private:
	UPROPERTY(ReplicatedUsing = OnRep_Team, VisibleAnywhere, Category = "FallingApart|Game")
	EFATeam Team = EFATeam::None;

	UPROPERTY(ReplicatedUsing = OnRep_RoundScore, VisibleAnywhere, Category = "FallingApart|Score")
	FFAPlayerScore RoundScore;

	UPROPERTY(ReplicatedUsing = OnRep_MatchScore, VisibleAnywhere, Category = "FallingApart|Score")
	FFAPlayerScore MatchScore;

	/** 0 = 데스매치 미참가, 1 = 승, 2 = 패. 최종 순위표가 이 값으로 완전 동점을 가른다. */
	UPROPERTY(ReplicatedUsing = OnRep_DeathMatchPlacement, VisibleAnywhere, Category = "FallingApart|Score")
	int32 DeathMatchPlacement = 0;

	/**
	 * 인간 상태에 진입한 서버 월드 시간. 음수면 계측 중이 아님
	 * 매 프레임 누적하지 않고 진입 시각만 들고 있다가 감염·라운드 종료 시 한 번에 계산
	 */
	float SurvivalClockStart = -1.0f;
};
