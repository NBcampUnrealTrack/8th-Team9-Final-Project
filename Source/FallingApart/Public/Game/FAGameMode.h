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

	/** Social.LeakVoice 등 음성 룰 변경 뒤 수신자별 GameplayMute 목록을 다시 계산한다. */
	void RefreshVoiceMuteMatrix();

	/** 이번 매치에서 시작 좀비 몇 명으로 시작하는지 (GDD 9부: 6~8인 1명 / 9~12인 2명) */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Game")
	int32 GetStartingZombieCount(int32 InPlayerCount) const;

	/**
	 * 최종 순위표의 '로비로 나가기' 가 부르는 즉시 복귀.
	 */
	void RequestReturnToLobbyNow();

protected:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool ReadyToStartMatch_Implementation() override;
	virtual void HandleMatchHasStarted() override;
	virtual void HandleMatchHasEnded() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

	/** 트래블로 넘어온 플레이어를 세션에 다시 등록한다 — 안 하면 서버가 보이스를 못 받는다. */
	virtual void HandleSeamlessTravelPlayer(AController*& C) override;

	virtual void Logout(AController* Exiting) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/** 데스매치 진행 중 접속한 플레이어는 관전만 시킨다. */
	virtual bool MustSpectate_Implementation(APlayerController* NewPlayerController) const override;

	/**
	 * 첫 스폰 자리에 고정되지 않게 함
	 */
	virtual bool ShouldSpawnAtStartSpot(AController* Player) override { return false; }
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

	/** 생존 틱 타이머 — 그 시점에 인간인 사람에게만 점수를 붙인다. */
	void OnSurvivalTick();

	/** 라운드 점수 정산 (GDD 5.1 / 5.2 · 기술문서 4.2) */
	void SettleRoundScore(EFARoundResult Result);

	/**
	 * 결과 화면이 읽을 라운드 스냅샷을 GameState 에 떠 둔다 (GDD 8.4).
	 * 반드시 CommitRoundScore() '전에' 불러야 한다 — 커밋이 라운드 점수를 0 으로 지운다.
	 */
	void BuildRoundResultSnapshot(EFARoundResult Result, const TArray<AFAPlayerState*>& AllPlayerStates);

	/** 라운드 결과 화면을 전 클라이언트에 띄우고 내린다. */
	void ShowRoundResult();
	void HideRoundResult();

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
	int32 InfectionScore = 3;

	/** 구출 QTE 승리 (개인 · 인간) */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Score")
	int32 RescueScore = 2;

	/**
	 * 생존 틱 (개인 · 인간). 인간으로 살아 있는 동안 SurvivalTickInterval 마다 지급.
	 *
	 * 라운드 안에서 실시간으로 오르는 유일한 인간 수입
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Score")
	int32 SurvivalTickScore = 1;

	/**
	 * 생존 틱 주기(초). DraftInterval 과 같은 값으로 두면 "카드 받는 순간 점수도 오른다"는
	 * 리듬이 생겨서 라운드 구조가 플레이어에게 체감된다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Score")
	float SurvivalTickInterval = 30.0f;

	/**
	 * 라운드 완주 (팀 · 인간). 감염 이력 없이 끝까지 남은 인간에게 고정으로 준다.
	 *
	 * 예전의 '생존자 수 × 1' 은 혼자 살아남은 가장 어려운 판이 가장 적게 받는 역전이 있었다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Score")
	int32 RoundCompleteScore = 3;

	/**
	 * 인간 전멸 상한 (팀 · 좀비 전원). 실제 지급액은 남은 시간 비율만큼 깎인다.
	 *
	 * 고정 지급이면 종료 직전에 감염당한 사람도 만점을 챙기므로, 인간이 던지는 게 이득이 된다.
	 * 늦은 전멸일수록 값이 떨어져야 그 유인이 사라지고 좀비 쪽엔 '빨리 쓸어담아라'가 남는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Score")
	int32 WipeOutScore = 6;

	/**
	 * 시작 좀비 수당 (개인 · 좀비). 라운드 배정 즉시 지급.
	 *
	 * 시작 좀비는 생존 틱·완주 같은 인간 수입이 원천 봉쇄된 자리다. 출발선을 맞춰 주지 않으면
	 * 뽑히는 것 자체가 벌칙이 된다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Score")
	int32 StartingZombieBonus = 3;

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

	/**
	 * 직전 라운드의 시작 좀비. 다음 라운드 추첨에서 후보 밖으로 뺀다.
	 *
	 * AssignStartingTeams 는 매 라운드 순수 랜덤이라 5라운드 중 같은 사람이 두 번 뽑히는 일이
	 * 드물지 않은데, 시작 좀비 자리는 인간 수입이 없으므로 연속으로 걸린 사람만 손해를 본다.
	 */
	TArray<TWeakObjectPtr<AFAPlayerState>> LastStartingZombies;

	FTimerHandle RoundStartTimerHandle;
	FTimerHandle RoundTimerHandle;
	FTimerHandle DraftTimerHandle;
	FTimerHandle RoundEndTimerHandle;
	FTimerHandle SurvivalTickTimerHandle;
	
	
	// 박지석 추가, 로비에서 전환했는지 구분을 위한 값 추가. InitGame 함수에서 설정함.
	int32 ExpectedPlayersToStart = 0;

	bool bHasLobbyMatchContext = false;
	
	// [박지석] - 최종 결과창 출력 10초후 로비로 다같이 돌아가는 시스템 관련
private:
	void BeginReturnToLobby();
	void HandleSessionReopenForLobby(bool bSuccess);
	void TravelToLobby();

	/** 로비 복귀가 무산됐을 때 전원에게 알린다 — 결과창의 '로비로 나가기' 버튼을 되살린다. */
	void NotifyReturnToLobbyFailedToAll();
	
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Lobby")
	float ReturnToLobbyDelaySeconds = 10.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Lobby")
	TSoftObjectPtr<UWorld> LobbyLevel;
	
	FTimerHandle ReturnToLobbyTimerHandle;
	FDelegateHandle SessionReopenForlobbyDelegateHandle;
	
	bool bReturningToLobby = false;
	
};
