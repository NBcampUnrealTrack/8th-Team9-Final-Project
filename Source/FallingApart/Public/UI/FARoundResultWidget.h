// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/FAGameTypes.h"
#include "FARoundResultWidget.generated.h"

class UPanelWidget;
class UTextBlock;

/**
 * 라운드 결과 화면 한 줄.
 *
 * 서버가 떠 준 스냅샷(FFARoundResultRow)에 화면에서만 필요한 값(순위·자기 줄 여부)을 얹은 것이다.
 */
USTRUCT(BlueprintType)
struct FFARoundResultEntry
{
	GENERATED_BODY()

	/** 1부터 시작하는 생존 시간 순위. */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Result")
	int32 Rank = 0;

	/** 서버가 보낸 줄 원본 — 이름 · 점수 4분할 · 생존 시간 · 진영. */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Result")
	FFARoundResultRow Row;

	/** 이 화면 주인인가 — 자기 줄 강조 */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Result")
	bool bIsLocalPlayer = false;
};

/**
 * 라운드 결과의 한 줄. WBP 로 만들 행 위젯의 부모.
 */
UCLASS()
class FALLINGAPART_API UFARoundResultRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 이 줄이 표시할 값을 넣는다. */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Result")
	void SetEntry(const FFARoundResultEntry& InEntry);

	UFUNCTION(BlueprintPure, Category = "FallingApart|Result")
	const FFARoundResultEntry& GetEntry() const { return Entry; }

protected:
	/** TextBlock 들이 채워진 뒤에 호출. 강조 색·진영 아이콘 등은 여기서. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|Result")
	void OnEntrySet(const FFARoundResultEntry& InEntry);

	//~ WBP 에 같은 이름의 TextBlock 이 있으면 자동으로 물린다. 없으면 그냥 넘어간다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Result")
	TObjectPtr<UTextBlock> Text_Rank;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Result")
	TObjectPtr<UTextBlock> Text_Name;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Result")
	TObjectPtr<UTextBlock> Text_SurvivalTime;

	/** 팀 점수 — 인간 팀 + 좀비 팀 (GDD 8.4 의 '팀' 칸). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Result")
	TObjectPtr<UTextBlock> Text_TeamScore;

	/** 개인 점수 — 인간 개인 + 좀비 개인 (GDD 8.4 의 '개인' 칸). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Result")
	TObjectPtr<UTextBlock> Text_PersonalScore;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Result")
	TObjectPtr<UTextBlock> Text_Total;

	/** 생존 / 감염. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Result")
	TObjectPtr<UTextBlock> Text_Status;

	/**
	 * 칸 안에 항목 이름을 같이 넣을지 (`팀 +6` 대 `+6`).
	 *
	 * 기본은 켜둔다 — 숫자만 여섯 개 나열되면 어느 칸이 무엇인지 읽을 수가 없다.
	 * WBP 에 머리글 행(순위 · 이름 · 생존 · 팀 · 개인 · 합계)을 따로 두면 여기서 꺼서 중복을 없앤다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Result")
	bool bShowInlineLabels = true;

private:
	UPROPERTY(Transient)
	FFARoundResultEntry Entry;
};

/**
 * 라운드 결과 화면 (GDD 8.4).
 *
 * 라운드가 끝나면 RoundEndDelay 동안 떠 있다가 다음 라운드 시작과 함께 내려간다.
 * 값은 PlayerState 가 아니라 AFAGameState 의 라운드 스냅샷에서 읽는다 —
 * 라운드 점수는 정산 직후 매치 누적으로 넘어가며 0 으로 지워지기 때문이다.
 */
UCLASS()
class FALLINGAPART_API UFARoundResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "FallingApart|Result")
	const TArray<FFARoundResultEntry>& GetResultEntries() const { return Entries; }

	/** 결과가 채워지거나 바뀌었을 때. WBP 가 행을 직접 그린다면 여기서 그릴 것. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|Result")
	void OnResultsChanged(const TArray<FFARoundResultEntry>& NewEntries);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	//~ WBP 에 같은 이름의 위젯이 있으면 자동으로 물린다. 없으면 임시 레이아웃이 채운다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Result")
	TObjectPtr<UTextBlock> Text_Title;

	/** '인간 승리' / '좀비 승리' 한 줄. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Result")
	TObjectPtr<UTextBlock> Text_Outcome;

	/** 행이 들어갈 컨테이너 (VerticalBox 등). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Result")
	TObjectPtr<UPanelWidget> Panel_Rows;

	/**
	 * Panel_Rows 를 채울 행 위젯.
	 * 지정하면 이 클래스가 행을 만들고 SetEntry() 까지 불러준다.
	 * 비워두면 행 그리기는 OnResultsChanged() 를 구현한 WBP 쪽 책임이 된다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Result")
	TSubclassOf<UFARoundResultRowWidget> RowWidgetClass;

	/**
	 * 갱신 주기(초).
	 * 결과 스냅샷은 위젯을 띄우라는 RPC 보다 늦게 도착할 수 있어, 뜬 뒤에도 잠깐 지켜봐야 한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Result")
	float RefreshInterval = 0.25f;

private:
	/** GameState 스냅샷을 다시 읽는다. 내용이 그대로면 아무것도 하지 않는다. */
	void RefreshEntries();

	/** 임시 레이아웃일 때만 — Panel_Rows 를 텍스트 줄로 다시 채운다. */
	void RebuildFallbackRows();

	/** RowWidgetClass 가 지정됐을 때 — Panel_Rows 를 행 위젯으로 다시 채운다. */
	void RebuildRowWidgets();

	/** WBP 가 없을 때 쓰는 임시 레이아웃. */
	void BuildFallbackLayout();

	/** 제목 · 승패 줄을 현재 스냅샷에 맞춘다. */
	void UpdateHeaderTexts();

	UPROPERTY(Transient)
	TArray<FFARoundResultEntry> Entries;

	/** 마지막으로 그린 스냅샷의 라운드 번호. 0 이면 아직 아무것도 못 받았다. */
	int32 DisplayedRoundIndex = 0;

	EFARoundResult DisplayedResult = EFARoundResult::None;

	bool bUsingFallbackLayout = false;

	float TimeSinceRefresh = 0.0f;
};
