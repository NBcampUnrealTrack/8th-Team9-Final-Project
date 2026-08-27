// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoadingScreenWidget.generated.h"

class UCircularThrobber;
class UProgressBar;
class UTextBlock;

/**
 * 로딩 화면 위젯
 */
UCLASS(Blueprintable)
class LOADINGSCREENKIT_API ULSKLoadingScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 가운데 큰 글씨. */
	UFUNCTION(BlueprintCallable, Category = "LoadingScreenKit")
	void SetTitleText(const FText& InText);

	/** 아래쪽 한 줄. 비우면 그 칸을 숨긴다. */
	UFUNCTION(BlueprintCallable, Category = "LoadingScreenKit")
	void SetTipText(const FText& InText);

	/**
	 * 0~1 진행률.
	 */
	UFUNCTION(BlueprintCallable, Category = "LoadingScreenKit")
	void SetProgress(float InPercent);

	//~ 서브시스템이 부른다. 게임 코드가 직접 부를 일은 없다.
	void NotifyShown();
	void NotifyHidden();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	/** 로딩 화면이 화면에 올라온 직후. 여기서 애니메이션을 재생. */
	UFUNCTION(BlueprintImplementableEvent, Category = "LoadingScreenKit", meta = (DisplayName = "On Loading Screen Shown"))
	void BP_OnShown();

	/** 걷기 시작할 때(페이드 시작 시점)가 아니라 완전히 사라진 뒤. */
	UFUNCTION(BlueprintImplementableEvent, Category = "LoadingScreenKit", meta = (DisplayName = "On Loading Screen Hidden"))
	void BP_OnHidden();

	//~ ---------------- WBP 에서 맞출 이름표 ----------------
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Title;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Tip;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> Bar_Progress;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCircularThrobber> Throbber_Loading;

private:
	/** WBP 가 없을 때만 도는 임시 레이아웃 조립. LoadingScreenWidget_Fallback.cpp */
	void BuildFallbackLayout();

	/**
	 * SetTitleText 가 위젯 조립보다 먼저 불릴 수 있다
	 */
	FText PendingTitle;
	FText PendingTip;
	float PendingProgress = -1.f;

	void ApplyPendingValues();
};
