// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Templates/SubclassOf.h"
#include "FAPlayerController.generated.h"

class UFAMatchResultWidget;
class UFARoundResultWidget;
class UFAPauseMenuWidget;
class UInputMappingContext;

UCLASS()
class FALLINGAPART_API AFAPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AFAPlayerController();

	/**
	 * 30초 주기를 기다리지 않고 드래프트를 즉시 한 번 돌린다. 콘솔에 `FADraft 3` 처럼 입력.
	 * 서버(PIE 단독·리슨 서버 호스트)에서만 동작한다 — 룰 적용은 서버 권한이다.
	 * @param DraftIndex 회차(1~6). 회차에 따라 후보 티어 구성이 달라진다 (1회차 T1T1T1 ~ 6회차 T3T3T3)
	 */
	UFUNCTION(Exec)
	void FADraft(int32 DraftIndex = 1);

	/** 내 캐릭터 ASC 의 보유 룰·게임플레이 태그를 콘솔에 찍는다. 룰이 실제로 붙었는지 확인용. */
	UFUNCTION(Exec)
	void FATags();

	/**
	 * 테스트 용
	 * 콘솔에 `FADeathMatch` 입력.
	 * 현재 순위 상위 2명이 참가자
	 */
	UFUNCTION(Exec)
	void FADeathMatch();

	/**
	 * 사운드 표에 걸어둔 ID 를 즉시 재생해 본다. 콘솔에 `FAPlaySound UI_Click` 처럼 입력.
	 * 인자를 비우면 다섯 표에 들어 있는 ID 를 전부 콘솔에 찍는다 — 애셋 연결 확인용.
	 */
	UFUNCTION(Exec)
	void FAPlaySound(const FString& SoundId);

	/** FAPlaySound 로 켠 루프(BGM · 앰비언스 · 게이지)를 끈다. 단발 소리에는 쓸 일이 없다. */
	UFUNCTION(Exec)
	void FAStopSound();

	/**
	 * 내 마이크의 노이즈 게이트를 즉시 바꿔본다. 콘솔에 `FAVoiceGate 0.05 0.045` 처럼 입력.
	 * 인자를 비우면 현재 값만 보여준다.
	 *
	 * 마이크·방 소음이 사람마다 달라 DefaultEngine.ini 값 하나로는 안 맞는다.
	 * `voice.debug.PrintAmplitude 1` 로 노이즈 바닥을 보면서 여기서 바로 조정할 것.
	 * @param SilenceThreshold 이 진폭 아래는 아예 전송하지 않는다. 0 이하를 주면 안 바꾼다.
	 * @param NoiseGateThreshold 게이트 임계. 0 이하를 주면 안 바꾼다.
	 */
	UFUNCTION(Exec)
	void FAVoiceGate(float SilenceThreshold = -1.0f, float NoiseGateThreshold = -1.0f);

	/**
	 * 감염을 즉시 한 번 일으킨다. 감염 전환 연출(암전 → 붉은 화면 → 기상)과 연속 드래프트
	 */
	UFUNCTION(Exec)
	void FAInfect(const FString& PlayerNameFragment);

	/**
	 * 푸시투토크를 켜고 끈다. 오픈 마이크가 시끄러울 때 즉시 쓰는 탈출구.
	 */
	UFUNCTION(Exec)
	void FAPushToTalk(bool bEnabled = true);

	//~ ---------------- 푸시투토크 ----------------

	/** 눌린 동안만 마이크를 연다. 오픈 마이크 상태에서는 아무 일도 하지 않는다. */
	void StartPushToTalk();
	void StopPushToTalk();

	//~ ---------------- 일시정지 메뉴 (ESC) ----------------
	// 로비 · 인게임 공통

	UFUNCTION(BlueprintCallable, Category = "FallingApart|PauseMenu")
	void TogglePauseMenu();

	UFUNCTION(BlueprintCallable, Category = "FallingApart|PauseMenu")
	void ShowPauseMenu();

	UFUNCTION(BlueprintCallable, Category = "FallingApart|PauseMenu")
	void HidePauseMenu();

	/**
	 * 키보드 포커스와 입력 모드를 일시정지 메뉴로 되돌린다.
	 *
	 * 설정창처럼 메뉴 위에 겹쳐 뜬 창이 닫힌 뒤에 부른다. ESC 로 메뉴를 닫는 것은
	 * UFAPauseMenuWidget::NativeOnKeyDown 이라 포커스가 메뉴에 없으면 키가 아무 데도 안 간다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|PauseMenu")
	void FocusPauseMenu();

	UFUNCTION(BlueprintPure, Category = "FallingApart|PauseMenu")
	bool IsPauseMenuOpen() const;

	UFUNCTION(BlueprintPure, Category = "FallingApart|PauseMenu")
	bool IsPauseMenuKey(const FKey& Key) const;

	/**
	 * 세션을 정리하고 메인 메뉴 레벨로
	 */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|PauseMenu")
	void ReturnToMainMenu();

	//~ ---------------- 최종 순위표 ----------------

	/** 매치 종료 후 호출 */
	UFUNCTION(Client, Reliable)
	void Client_ShowMatchResult();

	UFUNCTION(BlueprintPure, Category = "FallingApart|Result")
	bool IsMatchResultOpen() const;

	/**
	 * 최종 순위표의 '로비로 나가기'.
	 *
	 * ReturnToMainMenu() 와 달리 세션을 부수지 않는다. 혼자 빠져나가는 게 아니라
	 * 서버가 세션을 다시 열고 다 같이 로비 레벨로 ServerTravel 하도록 요청
	 */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Result")
	void ReturnToLobby();

	UFUNCTION(Server, Reliable)
	void Server_RequestReturnToLobby();

	/** 서버가 로비 복귀에 실패했다 — 결과창의 버튼을 되살린다. */
	UFUNCTION(Client, Reliable)
	void Client_NotifyReturnToLobbyFailed();

	//~ ---------------- 라운드 결과 화면 ----------------

	/**
	 * 라운드 종료 시 호출. RoundEndDelay 동안 떠 있다가 다음 라운드 시작 때 내려간다.
	 * 최종 순위표와 달리 입력을 가져가지 않는다 — 다음 라운드로 그대로 이어져야 한다.
	 */
	UFUNCTION(Client, Reliable)
	void Client_ShowRoundResult();

	UFUNCTION(Client, Reliable)
	void Client_HideRoundResult();

	UFUNCTION(BlueprintPure, Category = "FallingApart|Result")
	bool IsRoundResultOpen() const;

	/** 게임 프로세스 종료. */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|PauseMenu")
	void QuitGame();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * 일시정지 메뉴 위젯. 기본값은 C++ 기본 위젯이라 WBP 없이도 뜬다.
	 * UI 담당이 UFAPauseMenuWidget 을 상속한 WBP_FAPauseMenu 를 만들면 여기서 갈아끼울 것.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|PauseMenu")
	TSubclassOf<UFAPauseMenuWidget> PauseMenuWidgetClass;

	/** HUD · 드래프트 창보다 위에 떠야 한다. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|PauseMenu")
	int32 PauseMenuZOrder = 100;

	/**
	 * 최종 순위표 위젯. 기본값은 C++ 기본 위젯이라 WBP 없이도 뜬다.
	 * UI 담당이 UFAMatchResultWidget 을 상속한 WBP_FAMatchResult 를 만들면 여기서 갈아끼울 것.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Result")
	TSubclassOf<UFAMatchResultWidget> MatchResultWidgetClass;

	/** 게임이 끝난 화면이라 ESC 메뉴보다도 위에 둔다. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Result")
	int32 MatchResultZOrder = 150;

	/**
	 * 라운드 결과 화면. 기본값은 C++ 기본 위젯이라 WBP 없이도 뜬다.
	 * UI 담당이 UFARoundResultWidget 을 상속한 WBP 를 만들면 여기서 갈아끼울 것.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Result")
	TSubclassOf<UFARoundResultWidget> RoundResultWidgetClass;

	/** HUD 위에는 오되 ESC 메뉴·최종 순위표보다는 아래. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Result")
	int32 RoundResultZOrder = 90;

	/**
	 * 일시정지 메뉴 토글 키. 기본은 ESC 하나.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|PauseMenu")
	TArray<FKey> PauseMenuKeys;

	/**
	 * 푸시투토크 키. 기본은 ` (EKeys::Tilde).
	 *
	 * Enhanced Input 을 거치지 않고 직접 잡는다(ESC 와 같은 방식) — 마이크는 캐릭터를 조종할 수
	 * 없는 상태(관전·연출 중)에서도 열려야 하는데, IMC 는 그때 빠져 있을 수 있다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Voice")
	TArray<FKey> PushToTalkKeys;

	/**
	 * 돌아갈 메인 메뉴 레벨. 비워두면 프로젝트 설정의 Game Default Map
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|PauseMenu")
	TSoftObjectPtr<UWorld> MainMenuLevel;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	/** 세션 정리가 끝났을 때(성공·실패 모두) 실제로 레벨을 옮긴다. */
	void HandleDestroySessionComplete(bool bWasSuccessful);

	void TravelToMainMenu();

	UPROPERTY(Transient)
	TObjectPtr<UFAPauseMenuWidget> PauseMenuWidget;

	UPROPERTY(Transient)
	TObjectPtr<UFAMatchResultWidget> MatchResultWidget;

	UPROPERTY(Transient)
	TObjectPtr<UFARoundResultWidget> RoundResultWidget;

	/** 메인 메뉴 복귀를 두 번 태우지 않기 위한 가드. */
	bool bReturningToMainMenu = false;
};
