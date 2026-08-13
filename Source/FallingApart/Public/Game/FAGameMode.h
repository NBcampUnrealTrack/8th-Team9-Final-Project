// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Game/FAGameTypes.h"
#include "FAGameMode.generated.h"

class AFAGameState;
class AFAPlayerState;

/**
 * 매치 · 라운드 루프의 서버 권한 주체 (GDD 2.3 / 2.4 · 기술문서 4.2).
 *
 * 라운드 루프
 *   진영 배정 → 시작 카운트다운 → 3분 30초 진행(30초마다 드래프트 6회)
 *   → 전원 감염(좀비 승) 또는 시간 만료(인간 승) → 점수 정산 → 다음 라운드
 * 매치 루프
 *   5라운드 누적 개인 점수로 최종 순위표 산출.
 *
 * 감염 / 구출은 어빌리티(GA_Grab · GA_RescueContest)가 판정하고, 결과만
 * HandleInfection() / HandleRescueSuccess() 로 이 클래스에 넘김
 */
UCLASS()
class FALLINGAPART_API AFAGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	AFAGameMode();

	/**
	 * 감염 게이지 완주 또는 QTE 좀비 승리로 감염이 확정됐을 때 호출
	 * @param Victim   감염된 인간
	 * @param Infector 감염시킨 좀비 (없으면 nullptr — 룰에 의한 즉시 감염 등)
	 */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Game")
	void HandleInfection(AFAPlayerState* Victim, AFAPlayerState* Infector);

	/** 구출 QTE 에서 인간이 승리했을 때 호출한다 (구조자 개인 점수 +1). */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Game")
	void HandleRescueSuccess(AFAPlayerState* Rescuer, AFAPlayerState* Victim);

	/** 라운드를 즉시 종료한다. 승패가 이미 확정된 경우에만 호출할 것. */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Game")
	void EndRound(EFARoundResult Result);
	
	UFUNCTION(Exec)
	void ForceEndRound();
	/**
	 * 데스매치 참가자 확정 (GDD 5.4 · 1등 완전 동점자 2~3명).
	 *
	 * 관전자 보이스 관전자 채널로 분리.
	 * 데스매치를 켤 때 반드시 호출할 것. 빈 배열을 넘기면 관전 상태 해제
	 */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Game")
	void SetDeathMatchContenders(const TArray<AFAPlayerState*>& Contenders);

	/**
	 * 데스매치가 끝났으니(또는 열지 못했으니) 매치를 최종 종료 상태로 되돌린다.
	 * UFADeathMatchSubsystem 만 호출. 관전 해제 · 최종 순위표 표시가 여기서 일어난다.
	 */
	void FinishMatchAfterDeathMatch();

	/**
	 * 보이스 기준으로 관전자인가.
	 */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Game")
	bool IsVoiceSpectator(APlayerController* PlayerController) const;

	/** 이번 매치에서 시작 좀비 몇 명으로 시작하는지 (GDD 9부: 6~8인 1명 / 9~12인 2명) */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Game")
	int32 GetStartingZombieCount(int32 InPlayerCount) const;

protected:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool ReadyToStartMatch_Implementation() override;
	virtual void HandleMatchHasStarted() override;
	virtual void HandleMatchHasEnded() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void UpdateGameplayMuteList(APlayerController* aPlayer) override;

	void UpdateVoiceMuteMatrix();

	/** 진영 배정 + 리스폰 후 시작 카운트다운을 건다. */
	void StartRound();

	/** 카운트다운이 끝나고 실제로 라운드 타이머·드래프트 타이머가 도는 시점. */
	void BeginRoundPlay();

	/** 라운드 제한 시간 도달 — 인간 승 */
	void OnRoundTimeExpired();

	/** 30초 주기 드래프트 타이머 */
	void OnDraftTimer();

	/** 라운드 점수 정산 (GDD 5.1 / 5.2 · 기술문서 4.2) */
	void SettleRoundScore(EFARoundResult Result);

	void StartNextRoundOrEndMatch();

	/** 라운드 시작 시 시작 좀비를 무작위로 뽑고 나머지를 인간으로 배정한다. */
	void AssignStartingTeams();

	void RestartAllPlayers();

	/** GameState 의 생존자 / 좀비 수를 다시 세고, 전원 감염이면 라운드를 끝낸다. */
	void UpdateTeamCounts(bool bCheckWipeOut = true);

	void ClearRoundTimers();

	/** 최종 순위를 로그로 찍고 전 클라이언트에 순위표 UI 를 띄운다. */
	void ShowFinalRanking();

	float GetServerTime() const;

	AFAGameState* GetFAGameState() const;

	//~ 라운드 / 매치 설정
	/** 매치를 시작하기 위한 최소 접속 인원. 개발 중 단독 테스트를 위해 기본 1 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Game")
	int32 MinPlayersToStart = 1;

	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Game")
	int32 MaxRound = 5;

	/** 1라운드 길이(초) — 3분 30초 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Game")
	float RoundDuration = 210.0f;

	/** 진영 배정 후 라운드가 실제로 시작되기까지의 카운트다운(초) */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Game")
	float RoundStartCountdown = 5.0f;

	/** 라운드 결과 화면을 보여주는 시간(초) */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Game")
	float RoundEndDelay = 8.0f;

	/** 드래프트 주기(초) */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Draft")
	float DraftInterval = 30.0f;

	/** 라운드당 드래프트 횟수 — 0:30 부터 3:00 까지 6회 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Draft")
	int32 DraftsPerRound = 6;

	/** 감염 직후 무적 시간(초). 연속 드래프트를 뽑는 동안 즉사하지 않게 한다 (GDD 3.4). */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Draft")
	float InfectionInvulnerabilityTime = 2.0f;

	/** 0 이면 인원수에 따라 자동 결정, 1 이상이면 방장 지정값으로 취급한다. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Game")
	int32 StartingZombieCountOverride = 0;

	/** 이 인원 이상이면 시작 좀비 2명 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Game")
	int32 PlayerCountForTwoZombies = 9;

	/** 방장이 지정할 수 있는 시작 좀비 상한 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Game")
	int32 MaxStartingZombies = 3;

	//~ 점수 배점 — GDD 5.1
	/** 감염 성공 (개인 · 좀비) */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Score")
	int32 InfectionScore = 2;

	/** 구출 QTE 승리 (개인 · 인간) */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Score")
	int32 RescueScore = 1;

	/** 인간 생존 — 생존자 1인당 (팀 · 인간 전원) */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Score")
	int32 SurvivorScorePerHead = 1;

	/** 인간 전멸 (팀 · 좀비 전원) */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Score")
	int32 WipeOutScore = 5;

	/** 최장 생존 (개인 · 인간, 동률 시 전원) */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Score")
	int32 LongestSurvivorScore = 2;

	//~ 스폰
	/** 인간 스폰 지점으로 쓸 PlayerStart 의 Player Start Tag */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Spawn")
	FName HumanPlayerStartTag = FName(TEXT("Human"));

	/** 좀비 스폰 지점으로 쓸 PlayerStart 의 Player Start Tag */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Spawn")
	FName ZombiePlayerStartTag = FName(TEXT("Zombie"));

private:
	/** 데스매치 참가자. 서버 전용. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AFAPlayerState>> DeathMatchContenders;

	FTimerHandle RoundStartTimerHandle;
	FTimerHandle RoundTimerHandle;
	FTimerHandle DraftTimerHandle;
	FTimerHandle RoundEndTimerHandle;
	
	
	// 박지석 추가, 로비에서 전환했는지 구분을 위한 값 추가. InitGame 함수에서 설정함.
	int32 ExpectedPlayersToStart = 0;

	bool bHasLobbyMatchContext = false;
	
	// [박지석] - 최종 결과창 출력 10초후 로비로 다같이 돌아가는 시스템 관련
private:
	void BeginReturnToLobby();
	void HandleSessionReopenForLobby(bool bSuccess);
	void TravelToLobby();
	
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Lobby")
	float ReturnToLobbyDelaySeconds = 10.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Lobby")
	TSoftObjectPtr<UWorld> LobbyLevel;
	
	FTimerHandle ReturnToLobbyTimerHandle;
	FDelegateHandle SessionReopenForlobbyDelegateHandle;
	
	bool bReturningToLobby = false;
	
};
