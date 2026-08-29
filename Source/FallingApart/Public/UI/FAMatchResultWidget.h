// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/FAGameTypes.h"
#include "FAMatchResultWidget.generated.h"

class AFAGameState;
class AFAPlayerState;
class UButton;
class UPanelWidget;
class UTextBlock;

/**
 * 최종 순위표 1행.
 */
USTRUCT(BlueprintType)
struct FFAMatchResultEntry
{
	GENERATED_BODY()

	/** 1부터 시작하는 순위. */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Result")
	int32 Rank = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Result")
	FString PlayerName;

	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Result")
	int32 TotalScore = 0;

	/** 누적 생존 시간(초). */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Result")
	float SurvivalTime = 0.0f;

	/** 이 화면 주인인가 — 자기 줄 강조 */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Result")
	bool bIsLocalPlayer = false;

	/** 데스매치를 거쳐 순위가 갈린 사람인가 (0 이 아니면 참가자). */
	UPROPERTY(BlueprintReadOnly, Category = "FallingApart|Result")
	int32 DeathMatchPlacement = 0;
};

/**
 * 최종 순위표의 한 줄. WBP_FAMatchResultRow 의 부모.
 */
UCLASS()
class FALLINGAPART_API UFAMatchResultRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 이 줄이 표시할 값을 넣는다. 순위표가 갱신될 때마다 호출. */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Result")
	void SetEntry(const FFAMatchResultEntry& InEntry);

	UFUNCTION(BlueprintPure, Category = "FallingApart|Result")
	const FFAMatchResultEntry& GetEntry() const { return Entry; }

protected:
	/** 위의 TextBlock 들이 채워진 뒤에 호출. 강조 색·아이콘 등 추가 표현은 여기서. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|Result")
	void OnEntrySet(const FFAMatchResultEntry& InEntry);

	//~ WBP 에 같은 이름의 TextBlock 이 있으면 자동으로 물린다. 없으면 그냥 넘어간다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Result")
	TObjectPtr<UTextBlock> Text_Rank;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Result")
	TObjectPtr<UTextBlock> Text_Name;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Result")
	TObjectPtr<UTextBlock> Text_Score;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Result")
	TObjectPtr<UTextBlock> Text_SurvivalTime;

private:
	UPROPERTY(Transient)
	FFAMatchResultEntry Entry;
};

/**
 * 매치 종료 후 최종 순위표 .
 *
 * 게임 루프의 마지막 화면
 */
UCLASS()
class FALLINGAPART_API UFAMatchResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 현재 순위 배열. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Result")
	const TArray<FFAMatchResultEntry>& GetResultEntries() const { return Entries; }

	/**
	 * 로비로 나가는 버튼 클릭시
	 */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Result")
	void OnReturnToLobbyClicked();

	UFUNCTION(BlueprintCallable, Category = "FallingApart|Result")
	void OnReturnToLobbyHovered();

	/**
	 * 서버가 로비 복귀에 실패했을 때 버튼을 되살려 다시 누를 수 있게 한다.
	 */
	void NotifyReturnToLobbyFailed();

	/**
	 * 순위가 확정되거나 바뀌었을 때. WBP 가 행 위젯을 직접 그린다면 여기서 그릴 것.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|Result")
	void OnResultsChanged(const TArray<FFAMatchResultEntry>& NewEntries);

	/**
	 * '로비로 나가기' 를 눌러 서버 응답을 기다리는 중.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|Result")
	void OnReturnToLobbyPending();

	/** 로비 복귀가 실패해 버튼이 다시 살아났다. WBP 가 안내 문구를 띄우고 싶으면 여기서. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|Result")
	void OnReturnToLobbyFailed();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	//~ WBP 에 같은 이름의 위젯이 있으면 자동으로 물린다. 없으면 임시 레이아웃이 채운다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Result")
	TObjectPtr<UButton> Btn_ReturnToLobby;

	/** 위 버튼의 글자. 있으면 서버 응답을 기다리는 동안 대기 문구로 바뀐다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Result")
	TObjectPtr<UTextBlock> Text_ReturnToLobby;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Result")
	TObjectPtr<UTextBlock> Text_Title;

	/** 행이 들어갈 컨테이너 (VerticalBox 등). 임시 레이아웃은 여기에 텍스트 한 줄씩 채운다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|Result")
	TObjectPtr<UPanelWidget> Panel_Rows;

	/**
	 * Panel_Rows 를 채울 행 위젯 (WBP_FAMatchResultRow 등).
	 * 지정하면 순위가 바뀔 때마다 이 클래스가 행을 만들고 SetEntry() 까지 불러준다.
	 * 비워두면 행 그리기는 OnResultsChanged() 를 구현한 WBP 쪽 책임이 된다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Result")
	TSubclassOf<UFAMatchResultRowWidget> RowWidgetClass;

	/**
	 * 순위표 갱신 주기(초).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Result")
	float RefreshInterval = 0.25f;

	/** 순위를 한 줄씩 훑는 간격(초). 전부 동시에 울리면 한 덩어리로 뭉개진다. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Result")
	float RankRevealInterval = 0.15f;

private:
	/** 순위표가 처음 채워졌을 때 한 줄씩 소리를 낸다. 갱신 때마다 다시 울리지는 않는다. */
	void StartRankReveal();
	void TickRankReveal();

	/** 순위를 다시 계산한다. 이전과 결과가 같으면 아무것도 하지 않는다. */
	void RefreshEntries();

	/** 임시 레이아웃일 때만 — Panel_Rows 를 텍스트 줄로 다시 채운다. */
	void RebuildFallbackRows();

	/** RowWidgetClass 가 지정됐을 때 — Panel_Rows 를 행 위젯으로 다시 채운다. */
	void RebuildRowWidgets();

	//~ WBP 가 없을 때 쓰는 임시 레이아웃.
	void BuildFallbackLayout();

	UPROPERTY(Transient)
	TArray<FFAMatchResultEntry> Entries;

	/** 임시 레이아웃으로 만들어졌는가 — 그렇다면 행 그리기까지 이 클래스가 책임진다. */
	bool bUsingFallbackLayout = false;

	float TimeSinceRefresh = 0.0f;

	FTimerHandle RankRevealTimerHandle;
	int32 RankRevealIndex = 0;
	bool bRankRevealStarted = false;
};
