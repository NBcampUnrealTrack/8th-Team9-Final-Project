// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/FAPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameMapsSettings.h"
#include "Audio/FAAudioSettings.h"
#include "Audio/FAAudioSubsystem.h"
#include "GAS/FAAbilitySystemComponent.h"
#include "Game/FAGameMode.h"
#include "Game/FAGameState.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/PackageName.h"
#include "Network/FAOnlineSessionSubsystem.h"
#include "Player/FACharacter.h"
#include "Player/FAPlayerState.h"
#include "Rule/FADeathMatchSubsystem.h"
#include "Rule/FARuleDraftSubsystem.h"
#include "UI/FADraftWidget.h"
#include "UI/FAHUD.h"
#include "UI/FAMatchResultWidget.h"
#include "UI/FAPauseMenuWidget.h"

AFAPlayerController::AFAPlayerController()
{
	bReplicates = true;

	PauseMenuWidgetClass = UFAPauseMenuWidget::StaticClass();
	MatchResultWidgetClass = UFAMatchResultWidget::StaticClass();
	PauseMenuKeys.Add(EKeys::Escape);
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
	if (!UFAAudioSettings::FindSound(Id))
	{
		ClientMessage(FString::Printf(TEXT("[FAPlaySound] '%s' 행을 다섯 표 어디에서도 찾지 못했다"), *SoundId));
		return;
	}

	if (UFAAudioSubsystem* Audio = UFAAudioSubsystem::Get(this))
	{
		Audio->PlaySound(Id);
		ClientMessage(FString::Printf(TEXT("[FAPlaySound] '%s' 재생"), *SoundId));
	}
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

	ClientTravel(MapPath, TRAVEL_Absolute);
}

void AFAPlayerController::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}
