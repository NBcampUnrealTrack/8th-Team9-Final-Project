// Fill out your copyright notice in the Description page of Project Settings.

#include "LoadingScreenSubsystem.h"

#include "LoadingScreenKitLog.h"
#include "LoadingScreenKitSettings.h"
#include "LoadingScreenWidget.h"

#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Widgets/SWidget.h"

#if !UE_BUILD_SHIPPING
namespace LSKConsole
{
	static void ShowCommand(const TArray<FString>& Args, UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		ULSKLoadingScreenSubsystem* Subsystem =
			GameInstance ? GameInstance->GetSubsystem<ULSKLoadingScreenSubsystem>() : nullptr;

		if (!Subsystem)
		{
			UE_LOG(LogLoadingScreenKit, Warning, TEXT("LoadingScreen.Show — 서브시스템을 찾지 못했다."));
			return;
		}

		FLSKLoadingScreenParams TestParams = GetDefault<ULSKPluginSettings>()->MakeDefaultParams();

		TestParams.bAutoHideWhenMapLoaded = false;
		TestParams.MaximumDisplaySeconds = (Args.Num() > 0) ? FCString::Atof(*Args[0]) : 10.f;

		Subsystem->ShowWithParams(TestParams);
	}

	static void HideCommand(UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		if (ULSKLoadingScreenSubsystem* Subsystem =
			GameInstance ? GameInstance->GetSubsystem<ULSKLoadingScreenSubsystem>() : nullptr)
		{
			Subsystem->Hide();
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs ShowCmd(
		TEXT("LoadingScreen.Show"),
		TEXT("로딩 화면을 띄워 본다. 인자로 표시 시간(초)을 준다. 기본 10초 뒤 자동으로 걷힌다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ShowCommand));

	static FAutoConsoleCommandWithWorld HideCmd(
		TEXT("LoadingScreen.Hide"),
		TEXT("띄워 둔 로딩 화면을 지금 걷는다."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&HideCommand));
}
#endif

ULSKLoadingScreenSubsystem* ULSKLoadingScreenSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;

	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;

	return GameInstance ? GameInstance->GetSubsystem<ULSKLoadingScreenSubsystem>() : nullptr;
}

bool ULSKLoadingScreenSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return Super::ShouldCreateSubsystem(Outer) && !IsRunningDedicatedServer();
}

void ULSKLoadingScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &ThisClass::HandlePreLoadMap);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);
	SeamlessTravelStartHandle = FWorldDelegates::OnSeamlessTravelStart.AddUObject(this, &ThisClass::HandleSeamlessTravelStart);
}

void ULSKLoadingScreenSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PreLoadMapWithContext.Remove(PreLoadMapHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	FWorldDelegates::OnSeamlessTravelStart.Remove(SeamlessTravelStartHandle);

	PreLoadMapHandle.Reset();
	PostLoadMapHandle.Reset();
	SeamlessTravelStartHandle.Reset();

	StopTicker();
	RemoveFromViewport();
	DestroyWidget();

	State = ELSKLoadingScreenState::Hidden;

	Super::Deinitialize();
}

// ============================================================
// 띄우고 걷기
// ============================================================

void ULSKLoadingScreenSubsystem::Show()
{
	ShowWithParams(GetDefault<ULSKPluginSettings>()->MakeDefaultParams());
}

void ULSKLoadingScreenSubsystem::ShowWithTitle(const FText& Title)
{
	FLSKLoadingScreenParams NewParams = GetDefault<ULSKPluginSettings>()->MakeDefaultParams();
	NewParams.Title = Title;

	ShowWithParams(NewParams);
}

void ULSKLoadingScreenSubsystem::ShowWithParams(const FLSKLoadingScreenParams& InParams)
{
	const ULSKPluginSettings* Settings = GetDefault<ULSKPluginSettings>();

	// 음수 = "설정값 따라감".
	FLSKLoadingScreenParams Resolved = InParams;
	if (Resolved.MinimumDisplaySeconds < 0.f) { Resolved.MinimumDisplaySeconds = Settings->MinimumDisplaySeconds; }
	if (Resolved.MaximumDisplaySeconds < 0.f) { Resolved.MaximumDisplaySeconds = Settings->MaximumDisplaySeconds; }
	if (Resolved.FadeOutSeconds < 0.f) { Resolved.FadeOutSeconds = Settings->FadeOutSeconds; }
	if (Resolved.Title.IsEmpty()) { Resolved.Title = Settings->DefaultTitle; }

	const bool bWasHidden = (State == ELSKLoadingScreenState::Hidden);

	Params = Resolved;

	if (!EnsureWidget())
	{
		return;
	}

	Widget->SetTitleText(Params.Title);
	Widget->SetProgress(-1.f);

	if (Params.Tip.IsEmpty())
	{
		// 팁을 안 넘겼으면 설정의 목록을 돌린다.
		PickNextTip(/*bResetTimer=*/true);
	}
	else
	{
		Widget->SetTipText(Params.Tip);
	}

	if (!bWasHidden)
	{
		if (State == ELSKLoadingScreenState::FadingOut)
		{
			// 걷는 중에 새 로딩이 시작된 경우. 되살린다.
			State = ELSKLoadingScreenState::Visible;
			FadeAlpha = 1.f;
			Widget->SetRenderOpacity(1.f);
			bHideRequested = false;
			bMapLoaded = false;
		}

		ApplyBlockInput();
		EnsureAddedToViewport();
		return;
	}

	State = ELSKLoadingScreenState::Visible;
	FadeAlpha = 1.f;
	bHideRequested = false;
	bMapLoaded = false;
	ShownAtSeconds = FPlatformTime::Seconds();

	Widget->SetRenderOpacity(1.f);
	ApplyBlockInput();

	EnsureAddedToViewport();
	StartTicker();

	Widget->NotifyShown();
	OnVisibilityChanged.Broadcast(true);

	UE_LOG(LogLoadingScreenKit, Log, TEXT("로딩 화면 표시 — 최소 %.2fs / 최대 %.2fs"),
		Params.MinimumDisplaySeconds, Params.MaximumDisplaySeconds);
}

void ULSKLoadingScreenSubsystem::Hide(bool bImmediate)
{
	if (State == ELSKLoadingScreenState::Hidden)
	{
		return;
	}

	if (bImmediate)
	{
		FinishHide();
		return;
	}

	bHideRequested = true;

	// 최소 표시 시간이 남아 있으면 티커가 마저 채우고 걷는다.
	const double Elapsed = FPlatformTime::Seconds() - ShownAtSeconds;
	if (State == ELSKLoadingScreenState::Visible && Elapsed >= Params.MinimumDisplaySeconds)
	{
		BeginFadeOut();
	}
}

void ULSKLoadingScreenSubsystem::BeginFadeOut()
{
	if (State != ELSKLoadingScreenState::Visible)
	{
		return;
	}

	if (Params.FadeOutSeconds <= 0.f)
	{
		FinishHide();
		return;
	}

	State = ELSKLoadingScreenState::FadingOut;
	FadeAlpha = 1.f;

	if (Widget)
	{
		// 사라지는 중에는 입력을 먹으면 안 된다
		Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void ULSKLoadingScreenSubsystem::FinishHide()
{
	const bool bWasShowing = (State != ELSKLoadingScreenState::Hidden);

	State = ELSKLoadingScreenState::Hidden;
	bHideRequested = false;
	bMapLoaded = false;
	FadeAlpha = 0.f;

	StopTicker();
	RemoveFromViewport();

	if (Widget)
	{
		Widget->NotifyHidden();

		// 위젯은 살려둔다. (메인메뉴→로비→인게임)
		Widget->SetRenderOpacity(1.f);
	}

	if (bWasShowing)
	{
		OnVisibilityChanged.Broadcast(false);
		UE_LOG(LogLoadingScreenKit, Log, TEXT("로딩 화면 걷음"));
	}
}

// ============================================================
// 내용 바꾸기
// ============================================================

void ULSKLoadingScreenSubsystem::SetTitle(const FText& InTitle)
{
	Params.Title = InTitle;

	if (Widget)
	{
		Widget->SetTitleText(InTitle);
	}
}

void ULSKLoadingScreenSubsystem::SetTip(const FText& InTip)
{
	Params.Tip = InTip;

	if (Widget)
	{
		Widget->SetTipText(InTip);
	}
}

void ULSKLoadingScreenSubsystem::SetProgress(float InPercent)
{
	if (Widget)
	{
		Widget->SetProgress(InPercent);
	}
}

// ============================================================
// 위젯 · 뷰포트
// ============================================================

bool ULSKLoadingScreenSubsystem::EnsureWidget()
{
	if (Widget)
	{
		return true;
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return false;
	}

	const ULSKPluginSettings* Settings = GetDefault<ULSKPluginSettings>();

	// WBP 를 지정하지 않았으면 밑판 클래스를 그대로 쓴다 — C++ 임시 레이아웃이 조립된다.
	UClass* WidgetClass = Settings->WidgetClass.IsNull()
		? ULSKLoadingScreenWidget::StaticClass()
		: Settings->WidgetClass.LoadSynchronous();

	if (!WidgetClass)
	{
		UE_LOG(LogLoadingScreenKit, Warning,
			TEXT("로딩 화면 위젯 클래스를 로드하지 못했다 (%s). 프로젝트 설정 → Plugins → Loading Screen Kit 확인."),
			*Settings->WidgetClass.ToString());

		WidgetClass = ULSKLoadingScreenWidget::StaticClass();
	}

	Widget = CreateWidget<ULSKLoadingScreenWidget>(GameInstance, WidgetClass);

	if (!Widget)
	{
		UE_LOG(LogLoadingScreenKit, Error, TEXT("로딩 화면 위젯 생성에 실패했다."));
		return false;
	}

	return true;
}

void ULSKLoadingScreenSubsystem::EnsureAddedToViewport()
{
	if (!Widget || State == ELSKLoadingScreenState::Hidden)
	{
		return;
	}

	if (WidgetSlate.IsValid())
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UGameViewportClient* Viewport = GameInstance ? GameInstance->GetGameViewportClient() : nullptr;

	if (!Viewport)
	{
		// 엔진 초기화 중이라 아직 뷰포트가 없을 수 있다.
		return;
	}

	WidgetSlate = Widget->TakeWidget();

	Viewport->AddViewportWidgetContent(WidgetSlate.ToSharedRef(), GetDefault<ULSKPluginSettings>()->ZOrder);
}

void ULSKLoadingScreenSubsystem::RemoveFromViewport()
{
	if (!WidgetSlate.IsValid())
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (UGameViewportClient* Viewport = GameInstance ? GameInstance->GetGameViewportClient() : nullptr)
	{
		Viewport->RemoveViewportWidgetContent(WidgetSlate.ToSharedRef());
	}

	WidgetSlate.Reset();
}

void ULSKLoadingScreenSubsystem::DestroyWidget()
{
	if (Widget)
	{
		Widget->ReleaseSlateResources(/*bReleaseChildren=*/true);
		Widget = nullptr;
	}
}

void ULSKLoadingScreenSubsystem::ApplyBlockInput()
{
	if (!Widget)
	{
		return;
	}

	// Visible = 마우스 입력을 여기서 끊는다. HitTestInvisible = 뒤로 흘려보낸다.
	Widget->SetVisibility(Params.bBlockInput
		? ESlateVisibility::Visible
		: ESlateVisibility::HitTestInvisible);
}

// ============================================================
// 시간
// ============================================================

void ULSKLoadingScreenSubsystem::StartTicker()
{
	if (TickerHandle.IsValid())
	{
		return;
	}

	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::HandleTick));
}

void ULSKLoadingScreenSubsystem::StopTicker()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
}

bool ULSKLoadingScreenSubsystem::HandleTick(float DeltaSeconds)
{
	if (State == ELSKLoadingScreenState::Hidden)
	{
		return true;
	}

	// 뷰포트가 늦게 생겼거나 무언가가 오버레이를 비웠을 때를 대비해 매 프레임 확인한다.
	EnsureAddedToViewport();

	const double Now = FPlatformTime::Seconds();

	if (State == ELSKLoadingScreenState::FadingOut)
	{
		FadeAlpha -= DeltaSeconds / FMath::Max(Params.FadeOutSeconds, KINDA_SMALL_NUMBER);

		if (FadeAlpha <= 0.f)
		{
			FinishHide();
			return true;
		}

		if (Widget)
		{
			Widget->SetRenderOpacity(FadeAlpha);
		}

		return true;
	}

	//~ 여기부터 State == Visible

	if (Params.Tip.IsEmpty() && Now >= NextTipAtSeconds)
	{
		PickNextTip(/*bResetTimer=*/true);
	}

	const double Elapsed = Now - ShownAtSeconds;

	// 안전장치가 먼저다 — 최소 표시 시간보다 우선한다.
	if (Params.MaximumDisplaySeconds > 0.f && Elapsed >= Params.MaximumDisplaySeconds)
	{
		UE_LOG(LogLoadingScreenKit, Warning,
			TEXT("로딩 화면이 %.1f초를 넘겨 강제로 걷는다. 걷을 신호(맵 로드 완료 · Hide 호출)가 오지 않았다."),
			Params.MaximumDisplaySeconds);

		BeginFadeOut();
		return true;
	}

	if (Elapsed < Params.MinimumDisplaySeconds)
	{
		return true;
	}

	if (bHideRequested)
	{
		BeginFadeOut();
		return true;
	}

	if (Params.bAutoHideWhenMapLoaded && bMapLoaded)
	{
		const float Hold = GetDefault<ULSKPluginSettings>()->HoldAfterMapLoadedSeconds;
		if (Now - MapLoadedAtSeconds >= Hold)
		{
			BeginFadeOut();
		}
	}

	return true;
}

void ULSKLoadingScreenSubsystem::PickNextTip(bool bResetTimer)
{
	const ULSKPluginSettings* Settings = GetDefault<ULSKPluginSettings>();

	if (bResetTimer)
	{
		NextTipAtSeconds = FPlatformTime::Seconds() + FMath::Max(Settings->TipRotationSeconds, 0.5f);
	}

	if (!Widget)
	{
		return;
	}

	const int32 Count = Settings->Tips.Num();
	if (Count == 0)
	{
		Widget->SetTipText(FText::GetEmpty());
		return;
	}

	// 같은 팁이 연달아 두 번 나오면 회전이 멈춘 것처럼 보인다.
	int32 Index = FMath::RandRange(0, Count - 1);
	if (Count > 1 && Index == LastTipIndex)
	{
		Index = (Index + 1) % Count;
	}

	LastTipIndex = Index;
	Widget->SetTipText(Settings->Tips[Index]);
}

// ============================================================
// 엔진 델리게이트
// ============================================================

void ULSKLoadingScreenSubsystem::HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName)
{
	if (WorldContext.OwningGameInstance != GetGameInstance())
	{
		return;
	}

	AutoShowForTravel(TEXT("맵 로드"), MapName);
}

void ULSKLoadingScreenSubsystem::HandleSeamlessTravelStart(UWorld* CurrentWorld, const FString& LevelName)
{
	const UGameInstance* GameInstance = GetGameInstance();
	if (CurrentWorld && GameInstance && CurrentWorld->GetGameInstance() != GameInstance)
	{
		return;
	}

	AutoShowForTravel(TEXT("심리스 트래블"), LevelName);
}

void ULSKLoadingScreenSubsystem::AutoShowForTravel(const TCHAR* Reason, const FString& MapName)
{
	if (!GetDefault<ULSKPluginSettings>()->bAutoShowOnMapChange)
	{
		return;
	}

	if (State != ELSKLoadingScreenState::Hidden)
	{
		return;
	}

	UE_LOG(LogLoadingScreenKit, Verbose, TEXT("자동 표시 (%s → %s)"), Reason, *MapName);

	Show();
}

void ULSKLoadingScreenSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (State == ELSKLoadingScreenState::Hidden)
	{
		return;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	if (LoadedWorld && GameInstance && LoadedWorld->GetGameInstance() != GameInstance)
	{
		return;
	}

	bMapLoaded = true;
	MapLoadedAtSeconds = FPlatformTime::Seconds();

	EnsureAddedToViewport();
}
