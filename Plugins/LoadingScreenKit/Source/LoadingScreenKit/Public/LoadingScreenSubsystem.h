// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "LoadingScreenKitTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LoadingScreenSubsystem.generated.h"

class SWidget;
class ULSKLoadingScreenWidget;
struct FWorldContext;

/** 로딩 화면이 완전히 뜨거나 완전히 사라진 순간. 페이드 도중에는 안 온다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLSKOnLoadingScreenVisibilityChanged, bool, bIsVisible);

UENUM()
enum class ELSKLoadingScreenState : uint8
{
	Hidden,
	Visible,
	FadingOut,
};

UCLASS()
class LOADINGSCREENKIT_API ULSKLoadingScreenSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** 월드 컨텍스트 아무거나로 가져온다. GameInstance 가 없으면 nullptr. */
	UFUNCTION(BlueprintPure, Category = "LoadingScreenKit", meta = (WorldContext = "WorldContextObject"))
	static ULSKLoadingScreenSubsystem* Get(const UObject* WorldContextObject);

	//~ ---------------- 띄우고 걷기 ----------------

	/** 플러그인 설정값 그대로 띄운다. */
	UFUNCTION(BlueprintCallable, Category = "LoadingScreenKit")
	void Show();

	/** 제목만 바꿔 띄운다. 나머지는 설정값. */
	UFUNCTION(BlueprintCallable, Category = "LoadingScreenKit", meta = (DisplayName = "Show With Title"))
	void ShowWithTitle(const FText& Title);

	/** 조건을 직접 정해 띄운다. 이미 떠 있으면 글씨만 갈아끼운다(다시 띄우지 않는다). */
	void ShowWithParams(const FLSKLoadingScreenParams& InParams);

	/**
	 * 걷는다. 최소 표시 시간이 남아 있으면 그 시간을 채운 뒤에 걷힌다.
	 * @param bImmediate 켜면 최소 표시 시간도 페이드도 무시하고 그 자리에서 없앤다.
	 */
	UFUNCTION(BlueprintCallable, Category = "LoadingScreenKit")
	void Hide(bool bImmediate = false);

	/** 페이드 중인 것도 "떠 있다" 로 본다. */
	UFUNCTION(BlueprintPure, Category = "LoadingScreenKit")
	bool IsShowing() const { return State != ELSKLoadingScreenState::Hidden; }

	UFUNCTION(BlueprintPure, Category = "LoadingScreenKit")
	ELSKLoadingScreenState GetState() const { return State; }

	//~ ---------------- 떠 있는 동안 내용 바꾸기 ----------------

	UFUNCTION(BlueprintCallable, Category = "LoadingScreenKit")
	void SetTitle(const FText& InTitle);

	UFUNCTION(BlueprintCallable, Category = "LoadingScreenKit")
	void SetTip(const FText& InTip);

	/** 0~1. 음수면 "모름" 이라 막대를 숨긴다. */
	UFUNCTION(BlueprintCallable, Category = "LoadingScreenKit")
	void SetProgress(float InPercent);

	UPROPERTY(BlueprintAssignable, Category = "LoadingScreenKit")
	FLSKOnLoadingScreenVisibilityChanged OnVisibilityChanged;

	//~ ---------------- UGameInstanceSubsystem ----------------

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	//~ 위젯 · 뷰포트
	bool EnsureWidget();
	void EnsureAddedToViewport();
	void RemoveFromViewport();
	void DestroyWidget();
	void ApplyBlockInput();

	//~ 시간
	bool HandleTick(float DeltaSeconds);
	void StartTicker();
	void StopTicker();
	void BeginFadeOut();
	void FinishHide();

	//~ 팁
	void PickNextTip(bool bResetTimer);

	//~ 엔진 델리게이트
	void HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName);
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void HandleSeamlessTravelStart(UWorld* CurrentWorld, const FString& LevelName);

	/** 자동 표시(설정의 bAutoShowOnMapChange)가 트래블을 보고 띄우는 경로. */
	void AutoShowForTravel(const TCHAR* Reason, const FString& MapName);

	UPROPERTY(Transient)
	TObjectPtr<ULSKLoadingScreenWidget> Widget;

	/**
	 * 뷰포트에 건 슬레이트 위젯.
	 */
	TSharedPtr<SWidget> WidgetSlate;

	FTSTicker::FDelegateHandle TickerHandle;

	FDelegateHandle PreLoadMapHandle;
	FDelegateHandle PostLoadMapHandle;
	FDelegateHandle SeamlessTravelStartHandle;

	ELSKLoadingScreenState State = ELSKLoadingScreenState::Hidden;

	/** 지금 떠 있는 화면의 조건. 설정값이 이미 반영된(음수가 없는) 상태로 보관한다. */
	FLSKLoadingScreenParams Params;

	/** 전부 FPlatformTime::Seconds() 기준 실시간. 월드 시간이 아니다. */
	double ShownAtSeconds = 0.0;
	double MapLoadedAtSeconds = 0.0;
	double NextTipAtSeconds = 0.0;

	float FadeAlpha = 1.f;

	/** Hide() 가 불렸지만 최소 표시 시간이 남아 대기 중. */
	bool bHideRequested = false;

	/** 이번 표시 구간에서 맵 로드 완료를 봤는가. */
	bool bMapLoaded = false;

	int32 LastTipIndex = INDEX_NONE;
};
