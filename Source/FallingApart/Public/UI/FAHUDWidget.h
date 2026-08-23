// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/FAGameTypes.h"
#include "FAHUDWidget.generated.h"

class AFAGameState;
class AFAPlayerState;

/**
 * 인게임 HUD C++ 베이스.
 */
UCLASS(Abstract)
class FALLINGAPART_API UFAHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 라운드 남은 시간(초). 라운드 진행 중이 아니면 0 */
	UFUNCTION(BlueprintPure, Category = "FallingApart|HUD")
	float GetRoundTimeRemaining() const;

	/** 라운드 남은 시간 "M:SS" 표기 */
	UFUNCTION(BlueprintPure, Category = "FallingApart|HUD")
	FText GetRoundTimeText() const;

	/** "1 / 5" 형태의 라운드 번호 표기. 매치 시작 전에는 비어 있다. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|HUD")
	FText GetRoundText() const;

	UFUNCTION(BlueprintPure, Category = "FallingApart|HUD")
	int32 GetSurvivorCount() const;

	UFUNCTION(BlueprintPure, Category = "FallingApart|HUD")
	int32 GetZombieCount() const;

	//~ Text 버전 — UMG 의 Text 바인딩 드롭다운에는 FText 반환 함수만 올라옴.
	//~ 디자이너가 변환 노드를 끼우지 않고 바로 묶을 수 있도록 같이 열어둠.
	UFUNCTION(BlueprintPure, Category = "FallingApart|HUD")
	FText GetSurvivorCountText() const;

	UFUNCTION(BlueprintPure, Category = "FallingApart|HUD")
	FText GetZombieCountText() const;

	UFUNCTION(BlueprintPure, Category = "FallingApart|HUD")
	FText GetLocalScoreText() const;

	/** "인간" / "좀비". 진영 미배정이면 "-" */
	UFUNCTION(BlueprintPure, Category = "FallingApart|HUD")
	FText GetLocalTeamText() const;

	UFUNCTION(BlueprintPure, Category = "FallingApart|HUD")
	EFARoundPhase GetRoundPhase() const;

	/** 이 화면 주인의 진영. PlayerState 가 아직 없으면 None */
	UFUNCTION(BlueprintPure, Category = "FallingApart|HUD")
	EFATeam GetLocalTeam() const;

	UFUNCTION(BlueprintPure, Category = "FallingApart|HUD")
	bool IsLocalPlayerZombie() const;

	/** 매치 누적 + 아직 정산되지 않은 진행 중 라운드 점수 */
	UFUNCTION(BlueprintPure, Category = "FallingApart|HUD")
	int32 GetLocalDisplayScore() const;

	UFUNCTION(BlueprintPure, Category = "FallingApart|HUD")
	AFAGameState* GetFAGameState() const;

	UFUNCTION(BlueprintPure, Category = "FallingApart|HUD")
	AFAPlayerState* GetLocalFAPlayerState() const;

	//~ 포획 감염 게이지 — 감염 담당(GA_Grab)이 로컬에서 호출하는 진입점.
	/**
	 * 포획 게이지를 띄움.
	 */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|HUD|Capture")
	void ShowCaptureGauge(bool bLocalPlayerIsVictim);

	/**
	 * 게이지 갱신. 매 프레임 불러도 됨 — 값이 실제로 바뀔 때만 이벤트가 나감.
	 * InfectionRatio 감염 진행도 0~1. 1 이면 감염 확정
	 * ResistRatio    저항 입력 누적 0~1. 저항 입력이 없으면 0
	 */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|HUD|Capture")
	void SetCaptureProgress(float InfectionRatio, float ResistRatio);

	UFUNCTION(BlueprintCallable, Category = "FallingApart|HUD|Capture")
	void HideCaptureGauge();

	UFUNCTION(BlueprintPure, Category = "FallingApart|HUD|Capture")
	bool IsCaptureGaugeVisible() const { return bCaptureGaugeVisible; }

	//~ ---------------- 구출 QTE 프롬프트 ----------------
	//~ 참가자 3명(좀비 · 인질 · 구조자) 전원

	UFUNCTION(BlueprintPure, Category = "FallingApart|HUD|Contest")
	bool IsContestPromptVisible() const { return bContestPromptVisible; }

	/** 이 화면 주인의 QTE 역할. QTE 중이 아니면 None. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|HUD|Contest")
	EFAContestRole GetLocalContestRole() const;

	/** QTE 남은 시간(초). QTE 중이 아니면 0. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|HUD|Contest")
	float GetContestTimeRemaining() const;

	//~ UI 담당이 WBP 에서 구현할 것 — 여기부터가 실제 그리기.
	/** 표시 초가 바뀔 때만 호출. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|HUD")
	void OnRoundTimeChanged(const FText& TimeText, float SecondsRemaining, bool bIsFinalCountdown);

	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|HUD")
	void OnRoundChanged(int32 CurrentRound, int32 MaxRound);

	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|HUD")
	void OnTeamCountsChanged(int32 SurvivorCount, int32 ZombieCount);

	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|HUD")
	void OnRoundPhaseChanged(EFARoundPhase NewPhase);

	/** 라운드 승패 확정. 결과 배너 · 정산 화면 트리거 */
	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|HUD")
	void OnRoundEnded(int32 RoundIndex, EFARoundResult Result);

	/** 내가 감염됐을 때도 여기로 온다 — 시야 필터 · 아이콘 전환 트리거 */
	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|HUD")
	void OnLocalTeamChanged(EFATeam NewTeam);

	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|HUD")
	void OnLocalScoreChanged(int32 DisplayTotalScore);

	/**
	 * 점수가 오른 순간 — 점수 표시 위로 "+N" 을 띄우라는 신호.
	 *
	 * @param Delta             이번에 오른 점수. 항상 1 이상
	 * @param bIsTeamScore      팀 점수(라운드 완주 · 전멸)면 true, 개인 점수(생존 틱 · 감염 · 구출)면 false
	 * @param DisplayTotalScore 오른 뒤의 표시 총점
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|HUD")
	void OnScorePopup(int32 Delta, bool bIsTeamScore, int32 DisplayTotalScore);

	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|HUD|Capture")
	void OnCaptureGaugeShown(bool bLocalPlayerIsVictim);

	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|HUD|Capture")
	void OnCaptureProgressChanged(float InfectionRatio, float ResistRatio);

	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|HUD|Capture")
	void OnCaptureGaugeHidden();

	/**
	 * 줄다리기 시작 — "SPACE 연타" 프롬프트를 띄우라는 신호.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|HUD|Contest")
	void OnContestPromptShown(EFAContestRole Role);

	/** 줄다리기 게이지 0~1. 1 이면 구출 성공 직전, 0 이면 감염 직전. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|HUD|Contest")
	void OnContestGaugeChanged(float Gauge);

	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|HUD|Contest")
	void OnContestPromptHidden();

	//~ ---------------- 상호작용 프롬프트 ----------------
	//~ 실제 판정은 UFAInteractionComponent 가 로컬에서 한다. 여기서는 그 값을 읽어 그리기만 한다.

	UFUNCTION(BlueprintPure, Category = "FallingApart|HUD|Interact")
	bool IsInteractPromptVisible() const { return bInteractPromptVisible; }

	/** "문 열기" 같은 문구. 대상이 없으면 비어 있다. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|HUD|Interact")
	FText GetInteractPromptText() const { return LastInteractPrompt; }

	/** 길게 누르기 진행도 0~1. 즉시형이면 항상 0. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|HUD|Interact")
	float GetInteractHoldProgress() const;

	/** 조준 대상이 생긴 순간. 프롬프트를 띄우라는 신호. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|HUD|Interact")
	void OnInteractPromptShown(const FText& PromptText);

	/** 대상은 그대로인데 문구만 바뀐 경우(문 열기 → 문 닫기). */
	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|HUD|Interact")
	void OnInteractPromptTextChanged(const FText& PromptText);

	/** 길게 누르기 게이지 0~1. 값이 실제로 바뀔 때만 온다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|HUD|Interact")
	void OnInteractHoldProgressChanged(float Progress);

	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|HUD|Interact")
	void OnInteractPromptHidden();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/**
	 * 남은 시간이 이 값 이하면 bIsFinalCountdown 이 true 로.
	 * 초침 간격이 좁아지기 시작하는 지점도 겸한다
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|HUD")
	float FinalCountdownSeconds = 30.0f;

	//~ ---------------- 라운드 시계 소리 ----------------
	//~ 짧은 tick 단발을 간격만 좁혀가며 반복.

	/** 초침이 들리기 시작하는 남은 시간(초). 이 순간 Round_LastMinute 이 한 번 울린다. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|HUD|Audio")
	float TickStartSeconds = 60.0f;

	/** FinalCountdownSeconds 위에서 쓰는 초침 간격(초). */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|HUD|Audio")
	float TickIntervalSlow = 1.0f;

	/** 남은 시간 0 에서의 초침 간격(초). FinalCountdownSeconds 부터 여기까지 선형으로 좁아진다. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|HUD|Audio")
	float TickIntervalFast = 0.25f;

	/**
	 * 점수 무효화 소리를 감염 순간에서 얼마나 뒤로 미룰지(초).
	 * 0 이면 Infect_Blackout · Sting_Infected 와 같은 프레임에 겹친다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|HUD|Audio")
	float ScoreVoidedDelay = 0.8f;

private:
	//~ 델리게이트 핸들러 — AddDynamic 대상.
	UFUNCTION()
	void HandleRoundPhaseChanged(EFARoundPhase NewPhase);

	UFUNCTION()
	void HandleTeamCountsChanged();

	UFUNCTION()
	void HandleRoundEnded(int32 RoundIndex, EFARoundResult Result);

	UFUNCTION()
	void HandleLocalTeamChanged(EFATeam NewTeam);

	UFUNCTION()
	void HandleLocalScoreChanged();

	/** 늦게 도착했거나 교체된 GameState / PlayerState 를 다시 묶음. 변화가 없으면 아무것도 하지 않음. */
	void RefreshBindings();

	void BindGameState(AFAGameState* NewGameState);
	void UnbindGameState();
	void BindPlayerState(AFAPlayerState* NewPlayerState);
	void UnbindPlayerState();

	/**
	 * 라운드 번호와 남은 시간은 폴링해서 변화분만 이벤트로.
	 * RoundPhase 와 CurrentRound 는 별개 복제 프로퍼티라 OnRep 순서가 보장되지 않음.
	 * 폴링이면 그 순서에 의존하지 않음.
	 */
	void UpdatePolledValues(float DeltaTime);

	/**
	 * 내 캐릭터의 복제된 포획 상태를 읽어 게이지를 띄우고 · 갱신하고 · 내린다.
	 * 포획 어빌리티는 ServerOnly 라 화면을 직접 못 건드리므로 이쪽에서 폴링한다.
	 */
	void UpdateCaptureGauge();

	/**
	 * 내 캐릭터의 복제된 QTE 상태를 읽어 프롬프트를 띄우고 · 갱신하고 · 내린다.
	 */
	void UpdateContestPrompt();

	/** 내 캐릭터의 상호작용 컴포넌트를 읽어 프롬프트를 띄우고 · 갱신하고 · 내린다. */
	void UpdateInteractPrompt();

	/** 로컬 폰의 상호작용 컴포넌트. 아직 빙의 전이면 nullptr. */
	class UFAInteractionComponent* GetLocalInteractionComponent() const;

	/** 초침을 울릴 때가 됐는지 누산기로 판정한다. 표시 초와 분리돼 있어야 1초보다 촘촘해질 수 있다. */
	void UpdateRoundTimerAudio(float SecondsRemaining, bool bRoundInProgress, float DeltaTime);

	/** 남은 시간에 대응하는 초침 간격(초). */
	float GetRoundTickInterval(float SecondsRemaining) const;

	/** 직전 라운드 점수와 비교해 종류별 소리와 "+N" 팝업을 낸다. */
	void UpdateScoreFeedback();

	TWeakObjectPtr<AFAGameState> CachedGameState;
	TWeakObjectPtr<AFAPlayerState> CachedPlayerState;

	/** 마지막으로 이벤트를 내보낸 표시 초. -1 이면 아직 한 번도 안 냈다. */
	int32 LastDisplayedSeconds = -1;

	/** 마지막 초침 이후 흐른 시간(초). */
	float TickAccumulator = 0.0f;

	/** Round_LastMinute 을 이번 라운드에 이미 울렸는가. 초침 구간을 벗어나면 다시 false 가 된다. */
	bool bLastMinuteAnnounced = false;

	/**
	 * 초침 구간(TickStartSeconds).
	 * 이게 false 인 채로 구간 안에 들어와 있다면 '넘어선' 게 아니라 '이미 지난 뒤에 붙은' 것이다.
	 */
	bool bSawAboveTickWindow = false;

	//~ 점수 소리 판정용 직전 값. -1 이면 아직 기준이 없다는 뜻이라 첫 갱신에서는 소리를 내지 않는다.
	int32 LastPersonalScore = -1;
	int32 LastTeamScore = -1;
	bool bLastWasInfectedThisRound = false;

	//~ 감염 소리 판정용. 기준이 없으면 첫 호출에서는 소리를 내지 않는다.
	EFATeam LastLocalTeam = EFATeam::None;
	bool bHasTeamBaseline = false;

	/** Score_Voided 를 감염 순간에서 떼어놓기 위한 타이머. */
	FTimerHandle ScoreVoidedTimerHandle;

	int32 LastCurrentRound = -1;
	int32 LastMaxRound = -1;

	bool bCaptureGaugeVisible = false;
	float LastInfectionRatio = -1.0f;
	float LastResistRatio = -1.0f;

	bool bContestPromptVisible = false;
	float LastContestGauge = -1.0f;

	//~ 상호작용 프롬프트 — 변화분만 이벤트로 내보내기 위한 직전 값.
	bool bInteractPromptVisible = false;
	FText LastInteractPrompt;
	float LastInteractHoldProgress = -1.0f;
};
