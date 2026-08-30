// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/FAPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameMapsSettings.h"
#include "Audio/FAAudioSettings.h"
#include "Audio/FAAudioSubsystem.h"
#include "GAS/FAAbilitySystemComponent.h"
#include "Game/FAGameMode.h"
#include "HAL/IConsoleManager.h"
#include "Game/FAGameState.h"
#include "Kismet/KismetSystemLibrary.h"
#include "LoadingScreenSubsystem.h"
#include "Misc/PackageName.h"
#include "UObject/ConstructorHelpers.h"
#include "Network/FAOnlineSessionSubsystem.h"
#include "Player/FACharacter.h"
#include "Player/FAPlayerState.h"
#include "Rule/FADeathMatchSubsystem.h"
#include "Rule/FARuleDraftSubsystem.h"
#include "Voice/FAVoiceComponent.h"
#include "Voice/FAVoiceStatics.h"
#include "UI/FADraftWidget.h"
#include "UI/FAHUD.h"
#include "UI/FAMatchResultWidget.h"
#include "UI/FARoundResultWidget.h"
#include "UI/FAPauseMenuWidget.h"

AFAPlayerController::AFAPlayerController()
{
	bReplicates = true;

	// 로비 · 인게임 어느 컨트롤러로 들어와도 WBP 가 뜨게 여기서 기본값을 잡는다.
	// 못 찾으면 C++ 임시 레이아웃으로 떨어진다.
	static ConstructorHelpers::FClassFinder<UFAPauseMenuWidget> PauseMenuWBP(
		TEXT("/Game/FallingApart/WBP/WBP_FAPauseMenu"));
	PauseMenuWidgetClass = UFAPauseMenuWidget::StaticClass();
	if (PauseMenuWBP.Succeeded())
	{
		PauseMenuWidgetClass = PauseMenuWBP.Class;
	}

	MatchResultWidgetClass = UFAMatchResultWidget::StaticClass();

	// WBP 가 없어도 임시 레이아웃으로 뜬다 — UI 담당이 WBP 를 만들면 BP_FAPlayerController 에서 갈아끼울 것.
	RoundResultWidgetClass = UFARoundResultWidget::StaticClass();
	PauseMenuKeys.Add(EKeys::Escape);
	PushToTalkKeys.Add(EKeys::Tilde);
}

void AFAPlayerController::FADraft(int32 DraftIndex)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Draft] FADraft 는 서버에서만 쓸 수 있다 (PIE 는 Play As Listen Server 로 호스트에서 입력할 것)"));
		return;
	}

	if (UFARuleDraftSubsystem* Draft = GetWorld()->GetSubsystem<UFARuleDraftSubsystem>())
	{
		// 회차 카운터(GameState::DraftsCompleted)는 건드리지 않는다 — 정규 30초 루프와 섞이지 않게.
		Draft->StartDraftForAllPlayers(FMath::Clamp(DraftIndex, 1, 6));
	}
}

void AFAPlayerController::FATags()
{
	const AFACharacter* FACharacter = Cast<AFACharacter>(GetPawn());
	const UFAAbilitySystemComponent* ASC = FACharacter ? FACharacter->GetFAAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		ClientMessage(TEXT("[FATags] ASC 를 찾을 수 없다 (폰 없음/관전 중?)"));
		return;
	}

	FGameplayTagContainer OwnedTags;
	ASC->GetOwnedGameplayTags(OwnedTags);

	ClientMessage(FString::Printf(TEXT("[FATags] 보유 룰: %s"),
		*FString::JoinBy(ASC->GetActiveRuleIds(), TEXT(", "), [](const FName& Id) { return Id.ToString(); })));
	ClientMessage(FString::Printf(TEXT("[FATags] 태그: %s"), *OwnedTags.ToStringSimple()));
	ClientMessage(FString::Printf(TEXT("[FATags] 점프 가능: %s"),
		FACharacter->CanJump() ? TEXT("true") : TEXT("false")));
}

void AFAPlayerController::FADeathMatch()
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[DeathMatch] FADeathMatch 는 서버에서만 쓸 수 있다 (PIE 는 Play As Listen Server 로 호스트에서 입력할 것)"));
		return;
	}

	AFAGameMode* GameMode = GetWorld()->GetAuthGameMode<AFAGameMode>();
	AFAGameState* FAGameState = GetWorld()->GetGameState<AFAGameState>();
	if (!GameMode || !FAGameState)
	{
		return;
	}

	TArray<AFAPlayerState*> Ranked;
	FAGameState->GetRankedPlayerStates(Ranked);
	if (Ranked.Num() < 2)
	{
		ClientMessage(TEXT("[FADeathMatch] 데스매치에는 2명이 필요하다 (현재 인원 부족)"));
		return;
	}

	// 남아 있는 라운드·드래프트 타이머는 페이즈가 InProgress 가 아니게 되는 순간 전부 no-op 이 된다.
	const TArray<AFAPlayerState*> Contenders(Ranked.GetData(), 2);

	FAGameState->SetRoundPhase(EFARoundPhase::DeathMatch);
	GameMode->SetDeathMatchContenders(Contenders);

	if (UFADeathMatchSubsystem* DeathMatch = GetWorld()->GetSubsystem<UFADeathMatchSubsystem>())
	{
		DeathMatch->StartDeathMatch(Contenders);
	}
}

void AFAPlayerController::FAPlaySound(const FString& SoundId)
{
	if (SoundId.IsEmpty())
	{
		// 인자 없이 부르면 지금 표에 실제로 들어 있는 ID 를 전부 보여준다.
		TArray<FName> AllIds;
		UFAAudioSettings::GetAllSoundIds(AllIds);

		if (AllIds.Num() == 0)
		{
			ClientMessage(TEXT("[FAPlaySound] 표가 비어 있다 — Project Settings → Game → Falling Apart | Audio 에서 DT_Sound_* 를 지정할 것"));
			return;
		}

		ClientMessage(FString::Printf(TEXT("[FAPlaySound] 등록된 ID %d개:"), AllIds.Num()));
		for (const FName& Id : AllIds)
		{
			const FFASoundRow* Row = UFAAudioSettings::FindSound(Id);
			ClientMessage(FString::Printf(TEXT("  %s%s"), *Id.ToString(),
				(Row && Row->Sound.IsNull()) ? TEXT("   (음원 미지정)") : TEXT("")));
		}
		return;
	}

	const FName Id(*SoundId);
	const FFASoundRow* Row = UFAAudioSettings::FindSound(Id);
	if (!Row)
	{
		ClientMessage(FString::Printf(TEXT("[FAPlaySound] '%s' 행을 다섯 표 어디에서도 찾지 못했다"), *SoundId));
		return;
	}

	UFAAudioSubsystem* Audio = UFAAudioSubsystem::Get(this);
	if (!Audio)
	{
		return;
	}

	// 루프 행을 PlaySound 로 내면 끌 방법이 없다 — 컴포넌트를 안 돌려받는 데다 bAutoDestroy 도 꺼져 있다.
	// 들어보려고 켠 BGM 이 레벨을 옮길 때까지 안 멎으면 곤란하니 음악 채널로 태운다.
	if (Row->bLooping)
	{
		Audio->PlayMusic(Id, /*FadeTime=*/0.0f);
		ClientMessage(FString::Printf(TEXT("[FAPlaySound] '%s' 재생 (루프 — FAStopSound 로 정지)"), *SoundId));
		return;
	}

	Audio->PlaySound(Id);
	ClientMessage(FString::Printf(TEXT("[FAPlaySound] '%s' 재생"), *SoundId));
}

void AFAPlayerController::FAStopSound()
{
	if (UFAAudioSubsystem* Audio = UFAAudioSubsystem::Get(this))
	{
		Audio->StopMusic(/*FadeTime=*/0.25f);
		ClientMessage(TEXT("[FAStopSound] 루프 정지"));
	}
}

namespace FAVoiceTuning
{
	/** CVar 하나를 읽고, 유효한 새 값이 오면 덮어쓴다. 콘솔 출력용 문자열을 돌려준다. */
	static FString ApplyCVar(const TCHAR* Name, float NewValue)
	{
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name);
		if (!CVar)
		{
			return FString::Printf(TEXT("  %s — 찾을 수 없음"), Name);
		}

		if (NewValue > 0.0f)
		{
			CVar->Set(NewValue, ECVF_SetByConsole);
		}

		return FString::Printf(TEXT("  %s = %.4f"), Name, CVar->GetFloat());
	}
}

void AFAPlayerController::FAInfect(const FString& PlayerNameFragment)
{
	if (!HasAuthority())
	{
		ClientMessage(TEXT("[FAInfect] 서버에서만 쓸 수 있다 (PIE 는 호스트 창에서 입력할 것)"));
		return;
	}

	AFAGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AFAGameMode>() : nullptr;
	if (!GameMode)
	{
		return;
	}

	// 인자가 없으면 나를 감염시킨다
	AFAPlayerState* Victim = Cast<AFAPlayerState>(PlayerState);

	if (!PlayerNameFragment.IsEmpty())
	{
		Victim = nullptr;

		if (const AGameStateBase* GameState = GetWorld()->GetGameState())
		{
			for (APlayerState* Each : GameState->PlayerArray)
			{
				if (Each != nullptr && Each->GetPlayerName().Contains(PlayerNameFragment, ESearchCase::IgnoreCase))
				{
					Victim = Cast<AFAPlayerState>(Each);
					break;
				}
			}
		}
	}

	if (!Victim)
	{
		ClientMessage(FString::Printf(TEXT("[FAInfect] '%s' 를 찾지 못했다"), *PlayerNameFragment));
		return;
	}

	if (!Victim->IsHuman())
	{
		ClientMessage(FString::Printf(TEXT("[FAInfect] %s 는 이미 좀비다"), *Victim->GetPlayerName()));
		return;
	}

	// 감염시킨 쪽은 없는 것으로 넘긴다
	GameMode->HandleInfection(Victim, nullptr);

	ClientMessage(FString::Printf(TEXT("[FAInfect] %s 감염 — 전환 연출과 연속 드래프트가 시작된다"),
		*Victim->GetPlayerName()));
}

void AFAPlayerController::FAVoiceGate(float SilenceThreshold, float NoiseGateThreshold)
{
	ClientMessage(TEXT("[FAVoiceGate] 현재 값:"));
	ClientMessage(FAVoiceTuning::ApplyCVar(TEXT("voice.SilenceDetectionThreshold"), SilenceThreshold));
	ClientMessage(FAVoiceTuning::ApplyCVar(TEXT("voice.MicNoiseGateThreshold"), NoiseGateThreshold));
	ClientMessage(TEXT("  (노이즈 바닥을 보려면 voice.debug.PrintAmplitude 1)"));
}

void AFAPlayerController::FAPushToTalk(bool bEnabled)
{
	// 설정창과 같은 통로로 넣는다 — 컴포넌트에 직접 넣으면 리스폰 한 번에 되돌아간다.
	UFAVoiceStatics::SetPushToTalkEnabled(this, bEnabled);

	if (UFAVoiceStatics::GetLocalVoiceComponent(this) == nullptr)
	{
		ClientMessage(TEXT("[FAPushToTalk] 지금은 보이스 컴포넌트가 없다 (폰 없음?). 다음에 스폰될 폰부터 적용된다."));
	}

	ClientMessage(FString::Printf(TEXT("[FAPushToTalk] %s (키: %s)"),
		bEnabled ? TEXT("켜짐 — 누를 때만 전송") : TEXT("꺼짐 — 오픈 마이크"),
		PushToTalkKeys.IsEmpty() ? TEXT("없음") : *PushToTalkKeys[0].GetDisplayName().ToString()));
}

void AFAPlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameOnly InputModeData;
	SetInputMode(InputModeData);
	bShowMouseCursor = false;

	if (IsLocalPlayerController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			// 로비 등 캐릭터 입력이 필요 없는 레벨에서는 비어 있을 수 있다.
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}

void AFAPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!InputComponent)
	{
		return;
	}

	// Enhanced Input 과 별개로 키를 직접 잡는다
	for (const FKey& Key : PauseMenuKeys)
	{
		if (Key.IsValid())
		{
			InputComponent->BindKey(Key, IE_Pressed, this, &AFAPlayerController::TogglePauseMenu);
		}
	}

	for (const FKey& Key : PushToTalkKeys)
	{
		if (Key.IsValid())
		{
			InputComponent->BindKey(Key, IE_Pressed, this, &AFAPlayerController::StartPushToTalk);
			InputComponent->BindKey(Key, IE_Released, this, &AFAPlayerController::StopPushToTalk);
		}
	}
}

void AFAPlayerController::StartPushToTalk()
{
	// 오픈 마이크일 때는 손대지 않는다 — 특히 뗄 때. 안 그러면 이 키가 마이크를 꺼버린다.
	if (!UFAVoiceStatics::IsPushToTalkEnabled())
	{
		return;
	}

	if (UFAVoiceComponent* Voice = UFAVoiceStatics::GetLocalVoiceComponent(this))
	{
		Voice->StartTalking();
	}
}

void AFAPlayerController::StopPushToTalk()
{
	if (!UFAVoiceStatics::IsPushToTalkEnabled())
	{
		return;
	}

	if (UFAVoiceComponent* Voice = UFAVoiceStatics::GetLocalVoiceComponent(this))
	{
		Voice->StopTalking();
	}
}

void AFAPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (PauseMenuWidget)
	{
		PauseMenuWidget->RemoveFromParent();
		PauseMenuWidget = nullptr;
	}

	if (MatchResultWidget)
	{
		MatchResultWidget->RemoveFromParent();
		MatchResultWidget = nullptr;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFAOnlineSessionSubsystem* Sessions = GameInstance->GetSubsystem<UFAOnlineSessionSubsystem>())
		{
			// 서브시스템은 레벨 이동 후에도 살아남는다 — 죽은 컨트롤러가 콜백을 물고 있으면 안 된다.
			Sessions->MultiplayerOnDestroySessionComplete.RemoveAll(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

//~ ---------------- 일시정지 메뉴 ----------------

bool AFAPlayerController::IsPauseMenuOpen() const
{
	return PauseMenuWidget != nullptr && PauseMenuWidget->IsInViewport();
}

bool AFAPlayerController::IsPauseMenuKey(const FKey& Key) const
{
	return PauseMenuKeys.Contains(Key);
}

void AFAPlayerController::TogglePauseMenu()
{
	if (IsPauseMenuOpen())
	{
		HidePauseMenu();
	}
	else
	{
		ShowPauseMenu();
	}
}

void AFAPlayerController::ShowPauseMenu()
{
	if (!IsLocalController() || IsPauseMenuOpen())
	{
		return;
	}

	// 최종 순위표가 떠 있으면 게임은 이미 끝났다. ESC 메뉴가 그 위에 겹치면 입력 모드가 꼬인다.
	if (IsMatchResultOpen())
	{
		return;
	}

	if (!PauseMenuWidgetClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[FA] AFAPlayerController: PauseMenuWidgetClass 가 비어 있어 ESC 메뉴를 만들지 못했다."));
		return;
	}

	if (!PauseMenuWidget)
	{
		PauseMenuWidget = CreateWidget<UFAPauseMenuWidget>(this, PauseMenuWidgetClass);
		if (!PauseMenuWidget)
		{
			return;
		}
	}

	PauseMenuWidget->SetIsFocusable(true);
	PauseMenuWidget->AddToViewport(PauseMenuZOrder);

	FocusPauseMenu();
}

void AFAPlayerController::FocusPauseMenu()
{
	if (!IsLocalController() || !IsPauseMenuOpen())
	{
		return;
	}

	// 멀티플레이라 게임을 멈추지는 않는다. 대신 메뉴가 입력을 독점한다.
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(PauseMenuWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	SetShowMouseCursor(true);
}

void AFAPlayerController::HidePauseMenu()
{
	if (!PauseMenuWidget)
	{
		return;
	}

	PauseMenuWidget->RemoveFromParent();

	UFADraftWidget* DraftWidget = nullptr;
	if (const AFAHUD* FAHUD = Cast<AFAHUD>(GetHUD()))
	{
		DraftWidget = FAHUD->GetDraftWidget();
	}

	if (DraftWidget && DraftWidget->IsOpen())
	{
		DraftWidget->RestoreInputMode();
		return;
	}

	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);
}

//~ ---------------- 최종 순위표 ----------------

bool AFAPlayerController::IsMatchResultOpen() const
{
	return MatchResultWidget != nullptr && MatchResultWidget->IsInViewport();
}

void AFAPlayerController::Client_ShowMatchResult_Implementation()
{
	if (!IsLocalController() || IsMatchResultOpen())
	{
		return;
	}

	if (!MatchResultWidgetClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[FA] AFAPlayerController: MatchResultWidgetClass 가 비어 있어 최종 순위표를 만들지 못했다."));
		return;
	}

	// 순위표가 뜨는 순간 게임은 끝났다 — 드래프트 창이 남아 있으면 걷는다.
	if (AFAHUD* FAHUD = Cast<AFAHUD>(GetHUD()))
	{
		FAHUD->HideDraftUI();
	}

	// ESC 메뉴가 열려 있으면 닫는다. 순위표가 입력을 가져가야 한다.
	HidePauseMenu();

	if (!MatchResultWidget)
	{
		MatchResultWidget = CreateWidget<UFAMatchResultWidget>(this, MatchResultWidgetClass);
		if (!MatchResultWidget)
		{
			return;
		}
	}

	MatchResultWidget->SetIsFocusable(true);
	MatchResultWidget->AddToViewport(MatchResultZOrder);

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(MatchResultWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	SetShowMouseCursor(true);
}

//~ ---------------- 라운드 결과 화면 ----------------

bool AFAPlayerController::IsRoundResultOpen() const
{
	return RoundResultWidget != nullptr && RoundResultWidget->IsInViewport();
}

void AFAPlayerController::Client_ShowRoundResult_Implementation()
{
	if (!IsLocalController() || IsRoundResultOpen())
	{
		return;
	}

	// RPC 는 월드가 내려가기 시작한 뒤에도 도착할 수 있다 — 그때 위젯을 만들면 ensure 가 터진다.
	const UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown)
	{
		return;
	}

	// 매치가 끝나 최종 순위표가 뜬 뒤라면 라운드 결과를 덧씌우지 않는다.
	if (IsMatchResultOpen())
	{
		return;
	}

	if (!RoundResultWidgetClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[FA] AFAPlayerController: RoundResultWidgetClass 가 비어 있어 라운드 결과 화면을 만들지 못했다."));
		return;
	}

	if (!RoundResultWidget)
	{
		RoundResultWidget = CreateWidget<UFARoundResultWidget>(this, RoundResultWidgetClass);
		if (!RoundResultWidget)
		{
			return;
		}
	}

	// 입력 모드는 건드리지 않는다 — 결과를 보는 동안에도 다음 라운드로 그대로 이어져야 한다.
	RoundResultWidget->AddToViewport(RoundResultZOrder);
}

void AFAPlayerController::Client_HideRoundResult_Implementation()
{
	if (RoundResultWidget && RoundResultWidget->IsInViewport())
	{
		RoundResultWidget->RemoveFromParent();
	}
}

void AFAPlayerController::ReturnToLobby()
{
	Server_RequestReturnToLobby();
}

void AFAPlayerController::Server_RequestReturnToLobby_Implementation()
{
	if (UWorld* World = GetWorld())
	{
		if (AFAGameMode* FAGameMode = World->GetAuthGameMode<AFAGameMode>())
		{
			FAGameMode->RequestReturnToLobbyNow();
		}
	}
}

void AFAPlayerController::Client_NotifyReturnToLobbyFailed_Implementation()
{
	if (MatchResultWidget)
	{
		MatchResultWidget->NotifyReturnToLobbyFailed();
	}
}

void AFAPlayerController::ReturnToMainMenu()
{
	if (bReturningToMainMenu)
	{
		return;
	}
	bReturningToMainMenu = true;

	HidePauseMenu();

	UGameInstance* GameInstance = GetGameInstance();
	UFAOnlineSessionSubsystem* Sessions = GameInstance ? GameInstance->GetSubsystem<UFAOnlineSessionSubsystem>() : nullptr;
	if (!Sessions)
	{
		TravelToMainMenu();
		return;
	}

	Sessions->MultiplayerOnDestroySessionComplete.RemoveAll(this);
	Sessions->MultiplayerOnDestroySessionComplete.AddUObject(this, &AFAPlayerController::HandleDestroySessionComplete);
	Sessions->DestroySession();
}

void AFAPlayerController::HandleDestroySessionComplete(bool bWasSuccessful)
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFAOnlineSessionSubsystem* Sessions = GameInstance->GetSubsystem<UFAOnlineSessionSubsystem>())
		{
			Sessions->MultiplayerOnDestroySessionComplete.RemoveAll(this);
		}
	}

	if (!bWasSuccessful)
	{
		UE_LOG(LogFAOSS, Warning, TEXT("[FA] 메인 메뉴 복귀 — 세션 정리에 실패했다. 그대로 레벨만 옮긴다."));
	}

	TravelToMainMenu();
}

void AFAPlayerController::TravelToMainMenu()
{
	FString MapPath = MainMenuLevel.IsNull()
		? UGameMapsSettings::GetGameDefaultMap()
		: MainMenuLevel.ToSoftObjectPath().ToString();

	if (MapPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[FA] 메인 메뉴 복귀 — 돌아갈 레벨을 찾지 못했다. BP_FAPlayerController 의 MainMenuLevel 을 지정할 것."));
		bReturningToMainMenu = false;
		return;
	}

	MapPath = FPackageName::ObjectPathToPackageName(MapPath);

	// 세션 정리까지 끝난 뒤라 게임 화면은 이미 의미가 없다. 메뉴가 뜰 때까지 덮는다.
	if (ULSKLoadingScreenSubsystem* LoadingScreen = ULSKLoadingScreenSubsystem::Get(this))
	{
		LoadingScreen->ShowWithTitle(
			NSLOCTEXT("FAPauseMenu", "ReturningToMainMenu", "메인 메뉴로 돌아가는 중..."));
	}

	ClientTravel(MapPath, TRAVEL_Absolute);
}

void AFAPlayerController::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

// ============================================================
// 콘솔에서 ESC 메뉴 열기
// ============================================================
namespace FAPauseMenuDebug
{
	static void TogglePauseMenuCommand(UWorld* World)
	{
		APlayerController* PlayerController = GEngine != nullptr ? GEngine->GetFirstLocalPlayerController(World) : nullptr;
		AFAPlayerController* FAPlayerController = Cast<AFAPlayerController>(PlayerController);

		if (FAPlayerController == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("FA.PauseMenu: 로컬 플레이어 컨트롤러가 없다. 아직 맵에 들어오지 않았을 수 있다."));
			return;
		}

		FAPlayerController->TogglePauseMenu();
	}

	static FAutoConsoleCommandWithWorld TogglePauseMenuConsoleCommand(
		TEXT("FA.PauseMenu"),
		TEXT("ESC 메뉴를 연다/닫는다. PIE 에서 Esc 가 플레이를 멈춰 버릴 때 쓴다."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&TogglePauseMenuCommand));
}
