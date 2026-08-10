// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/FAGameTypes.h"
#include "Rule/FARuleTypes.h"
#include "FADraftWidget.generated.h"

class UBorder;
class UButton;
class UHorizontalBox;
class UTextBlock;
class UTexture2D;

/**
 * 후보 카드 1장을 UI 가 쓰기 좋게 펼쳐놓은 사본.
 * 서버는 RuleId(=DT RowName) 만 보내고, 표시 정보는 클라이언트가 DT 에서 직접 찾는다.
 */
USTRUCT(BlueprintType)
struct FFADraftCardInfo
{
	GENERATED_BODY()

	/** 서버가 보낸 카드 ID. Server_ConfirmDraft 는 인덱스로 보내므로 이 값은 표시·로그용이다. */
	UPROPERTY(BlueprintReadOnly, Category = "Draft")
	FName RuleId;

	UPROPERTY(BlueprintReadOnly, Category = "Draft")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Draft")
	FText ShortDesc;

	UPROPERTY(BlueprintReadOnly, Category = "Draft")
	EFATeam Team = EFATeam::None;

	UPROPERTY(BlueprintReadOnly, Category = "Draft")
	EFARuleTier Tier = EFARuleTier::None;

	UPROPERTY(BlueprintReadOnly, Category = "Draft")
	EFARuleType Type = EFARuleType::Penalty;

	UPROPERTY(BlueprintReadOnly, Category = "Draft")
	TSoftObjectPtr<UTexture2D> Icon;

	/** false 면 DT 에 없는 ID(더미 풀 폴백). 카드에는 RuleId 가 그대로 찍힌다. */
	UPROPERTY(BlueprintReadOnly, Category = "Draft")
	bool bFoundInTable = false;
};

/**
 * 30초 주기 드래프트 선택 UI.
 *
 * 서버 → AFAPlayerState::Client_ShowDraftUI → AFAHUD::ShowDraftUI → 이 위젯.
 * 선택은 AFAPlayerState::Server_ConfirmDraft 로 되돌아가고, 실제 룰 적용은 서버가 한다.
 *
 * WBP 없이도 동작한다 — 디자이너 트리가 비어 있으면 RebuildWidget() 이 임시 레이아웃을 직접 조립한다.
 * UI 담당이 이 클래스를 상속한 WBP_FADraft 를 만들어 BP_FAHUD 에 물리면 그쪽이 우선한다.
 */
UCLASS()
class FALLINGAPART_API UFADraftWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 한 번에 보여줄 수 있는 최대 카드 수. Server_ConfirmDraft_Validate 의 상한과 같아야 한다. */
	static constexpr int32 MaxCards = 3;

	/**
	 * 후보를 받아 UI 를 연다. AFAHUD 가 호출.
	 * @param CandidateRuleIds 서버가 보낸 후보 ID (0~3장)
	 * @param SelectionTime    선택 제한 시간(초). 넘기면 서버가 무작위로 확정한다
	 */
	void ShowCandidates(const TArray<FName>& CandidateRuleIds, float SelectionTime);

	/** 카드 1장을 확정. 인덱스는 서버가 보낸 후보 배열 기준. */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Draft")
	void ConfirmSelection(int32 Index);

	/** 아무것도 고르지 않고 닫는다 (제한 시간 초과). 서버는 이미 무작위로 확정한 상태다. */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Draft")
	void CloseWithoutSelection();

	/** 후보를 받아 열려 있는 상태인가. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Draft")
	bool IsOpen() const { return bIsOpen; }

	/**
	 * 현재 열림 상태에 맞는 입력 모드를 다시 적용
	 * 드래프트 위에 떴던 일시정지 메뉴가 닫힐 때 AFAPlayerController 가 호출
	 */
	void RestoreInputMode();

	UFUNCTION(BlueprintPure, Category = "FallingApart|Draft")
	const TArray<FFADraftCardInfo>& GetCards() const { return Cards; }

	UFUNCTION(BlueprintPure, Category = "FallingApart|Draft")
	int32 GetCardCount() const { return Cards.Num(); }

	/** 선택까지 남은 시간(초). 이미 닫혔으면 0 */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Draft")
	float GetTimeRemaining() const;

	/** 남은 시간 정수 초 표기 — UMG Text 바인딩용 */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Draft")
	FText GetTimeRemainingText() const;

	/** 이번 라운드의 드래프트 회차(1~6). GameState 가 아직 없으면 0 */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Draft")
	int32 GetDraftIndex() const;

	//~ UI 담당이 WBP 에서 구현할 것.
	/** 후보가 도착해 UI 가 열렸을 때. 카드 배열은 0~3장이다 — 3장이라고 가정하지 말 것. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|Draft")
	void OnDraftShown(const TArray<FFADraftCardInfo>& InCards, float SelectionTime);

	/** 표시 초가 바뀔 때만 호출. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|Draft")
	void OnSelectionTimeChanged(float SecondsRemaining, int32 WholeSeconds);

	/** bConfirmed 가 false 면 제한 시간 초과로 닫힌 것 (SelectedIndex = -1). */
	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|Draft")
	void OnDraftClosed(bool bConfirmed, int32 SelectedIndex);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 드래프트 중에는 마우스 커서를 띄워 카드를 클릭할 수 있게 한다. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Draft")
	bool bTakeMouseCursorWhileOpen = true;

private:
	void Close(bool bConfirmed, int32 SelectedIndex);

	/** RuleId 를 DT 에서 찾아 표시 정보를 채운다. 없으면 RuleId 를 그대로 이름에 넣는다. */
	static FFADraftCardInfo MakeCardInfo(FName RuleId);

	void ApplyInputMode(bool bOpen);

	//~ WBP 가 없을 때만 쓰는 임시 레이아웃.
	void BuildFallbackLayout();
	void RefreshFallbackCards();
	void RefreshFallbackTimer(int32 WholeSeconds);

	UFUNCTION()
	void HandleCard0Clicked();

	UFUNCTION()
	void HandleCard1Clicked();

	UFUNCTION()
	void HandleCard2Clicked();

	UPROPERTY(Transient)
	TArray<FFADraftCardInfo> Cards;

	/** 클라이언트 월드 시간 기준 마감 시각. 음수면 열려 있지 않다. */
	float SelectionDeadline = -1.0f;

	float SelectionDuration = 0.0f;

	/** 마지막으로 이벤트를 내보낸 표시 초. -1 이면 아직 한 번도 안 냈다. */
	int32 LastDisplayedSeconds = -1;

	bool bIsOpen = false;

	//~ 임시 레이아웃 위젯 참조. WBP 를 쓰면 전부 null 이다.
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FallbackTitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FallbackTimerText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> FallbackCardSlots;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> FallbackCardButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> FallbackNameTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> FallbackDescTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> FallbackMetaTexts;
};
