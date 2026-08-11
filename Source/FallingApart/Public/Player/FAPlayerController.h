// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Templates/SubclassOf.h"
#include "FAPlayerController.generated.h"

class UFAMatchResultWidget;
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

	//~ ---------------- 일시정지 메뉴 (ESC) ----------------
	// 로비 · 인게임 공통

	UFUNCTION(BlueprintCallable, Category = "FallingApart|PauseMenu")
	void TogglePauseMenu();

	UFUNCTION(BlueprintCallable, Category = "FallingApart|PauseMenu")
	void ShowPauseMenu();

	UFUNCTION(BlueprintCallable, Category = "FallingApart|PauseMenu")
	void HidePauseMenu();

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
	 * 일시정지 메뉴 토글 키. 기본은 ESC 하나.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|PauseMenu")
	TArray<FKey> PauseMenuKeys;

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

	/** 메인 메뉴 복귀를 두 번 태우지 않기 위한 가드. */
	bool bReturningToMainMenu = false;
};
