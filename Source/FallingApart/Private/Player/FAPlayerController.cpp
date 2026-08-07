// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/FAPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GAS/FAAbilitySystemComponent.h"
#include "Player/FACharacter.h"
#include "Rule/FARuleDraftSubsystem.h"

AFAPlayerController::AFAPlayerController()
{
	bReplicates = true;
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
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}
