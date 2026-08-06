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

	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|HUD|Capture")
	void OnCaptureGaugeShown(bool bLocalPlayerIsVictim);

	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|HUD|Capture")
	void OnCaptureProgressChanged(float InfectionRatio, float ResistRatio);

	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|HUD|Capture")
	void OnCaptureGaugeHidden();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 남은 시간이 이 값 이하면 bIsFinalCountdown 이 true 로. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|HUD")
	float FinalCountdownSeconds = 30.0f;

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
	void UpdatePolledValues();

	TWeakObjectPtr<AFAGameState> CachedGameState;
	TWeakObjectPtr<AFAPlayerState> CachedPlayerState;

	/** 마지막으로 이벤트를 내보낸 표시 초. -1 이면 아직 한 번도 안 냈다. */
	int32 LastDisplayedSeconds = -1;

	int32 LastCurrentRound = -1;
	int32 LastMaxRound = -1;

	bool bCaptureGaugeVisible = false;
	float LastInfectionRatio = -1.0f;
	float LastResistRatio = -1.0f;
};
