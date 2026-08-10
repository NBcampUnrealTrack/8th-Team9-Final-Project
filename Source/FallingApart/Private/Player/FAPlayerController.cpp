// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/FAPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameMapsSettings.h"
#include "GAS/FAAbilitySystemComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/PackageName.h"
#include "Network/FAOnlineSessionSubsystem.h"
#include "Player/FACharacter.h"
#include "Rule/FARuleDraftSubsystem.h"
#include "UI/FADraftWidget.h"
#include "UI/FAHUD.h"
#include "UI/FAPauseMenuWidget.h"

AFAPlayerController::AFAPlayerController()
{
	bReplicates = true;

	PauseMenuWidgetClass = UFAPauseMenuWidget::StaticClass();
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
