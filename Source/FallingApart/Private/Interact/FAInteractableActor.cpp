// Fill out your copyright notice in the Description page of Project Settings.

#include "Interact/FAInteractableActor.h"
#include "AbilitySystemComponent.h"
#include "GAS/FAGameplayTags.h"
#include "Player/FACharacter.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace
{
	/** 점유자가 아직 살아 있는지 확인하는 주기(초). 사망·접속 끊김을 이 간격 안에 알아챈다. */
	constexpr float OccupantWatchdogInterval = 0.5f;
}

AFAInteractableActor::AFAInteractableActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// 메시를 자식으로 두면 배치된 뒤에 메시를 갈아끼워도 액터 원점(= 조준 기준점)이 안 움직인다.
	InteractRoot = CreateDefaultSubobject<USceneComponent>(TEXT("InteractRoot"));
	RootComponent = InteractRoot;
}

void AFAInteractableActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFAInteractableActor, InteractionOccupant);
}

void AFAInteractableActor::BeginPlay()
{
	Super::BeginPlay();
}

void AFAInteractableActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(OccupantWatchdogHandle);

	Super::EndPlay(EndPlayReason);
}

bool AFAInteractableActor::CanInteract_Implementation(AFACharacter* Interactor) const
{
	if (!bInteractEnabled || !Interactor)
	{
		return false;
	}

	// 점유 중이면 쓰고 있는 본인만 다시 부를 수 있다(사다리에서 내려오기 같은 것).
	if (bExclusiveInteraction && InteractionOccupant && InteractionOccupant != Interactor)
	{
		return false;
	}

	return IsFactionAllowed(Interactor);
}

bool AFAInteractableActor::IsFactionAllowed(const AFACharacter* Interactor) const
{
	if (!Interactor)
	{
		return false;
	}

	// 둘 다 켜져 있으면 볼 것도 없다. ASC 조회를 아끼는 흔한 경로다.
	if (bZombieCanUse && bHumanCanUse)
	{
		return true;
	}

	const UAbilitySystemComponent* ASC = Interactor->GetAbilitySystemComponent();
	if (!ASC)
	{
		return false;
	}

	const FAGameplayTags& FATags = FAGameplayTags::Get();
	if (!bZombieCanUse && ASC->HasMatchingGameplayTag(FATags.State_Zombie))
	{
		return false;
	}
	if (!bHumanCanUse && ASC->HasMatchingGameplayTag(FATags.State_Human))
	{
		return false;
	}

	return true;
}

FText AFAInteractableActor::GetInteractPrompt_Implementation(AFACharacter* Interactor) const
{
	if (!InteractPromptText.IsEmpty())
	{
		return InteractPromptText;
	}

	return IFAInteractable::GetInteractPrompt_Implementation(Interactor);
}

void AFAInteractableActor::Interact_Implementation(AFACharacter* Interactor)
{
	// 블루프린트 소품은 이 이벤트 하나로 끝난다.
	OnInteracted(Interactor);
}

FVector AFAInteractableActor::GetInteractFocusLocation_Implementation() const
{
	return GetActorTransform().TransformPosition(InteractFocusOffset);
}

bool AFAInteractableActor::TryOccupyInteraction_Implementation(AFACharacter* Interactor)
{
	if (!HasAuthority() || !Interactor)
	{
		return false;
	}

	if (!bExclusiveInteraction)
	{
		return true;
	}

	if (InteractionOccupant && InteractionOccupant != Interactor)
	{
		return false;
	}

	if (InteractionOccupant != Interactor)
	{
		InteractionOccupant = Interactor;
		OnRep_InteractionOccupant();

		// 점유자가 감염되거나 나가면 물건이 영원히 잠긴다 — 살아 있는지 주기적으로 확인한다.
		GetWorldTimerManager().SetTimer(OccupantWatchdogHandle, this,
			&AFAInteractableActor::CheckOccupantAlive, OccupantWatchdogInterval, true);
	}

	return true;
}

void AFAInteractableActor::ReleaseInteraction_Implementation(AFACharacter* Interactor)
{
	if (!HasAuthority())
	{
		return;
	}

	// Interactor 가 null 이면 강제 해제(감시 타이머가 부르는 경우).
	if (Interactor && InteractionOccupant != Interactor)
	{
		return;
	}

	if (InteractionOccupant)
	{
		InteractionOccupant = nullptr;
		OnRep_InteractionOccupant();
	}

	GetWorldTimerManager().ClearTimer(OccupantWatchdogHandle);
}

void AFAInteractableActor::CheckOccupantAlive()
{
	if (!HasAuthority())
	{
		return;
	}

	// IsValid 는 파괴 예약된 액터도 걸러낸다. 컨트롤러가 떨어져 나간 폰도 점유자로 두지 않는다.
	const bool bStillUsable = IsValid(InteractionOccupant) && InteractionOccupant->GetController() != nullptr;
	if (!bStillUsable)
	{
		// BP 오버라이드까지 태우려면 Execute_ 를 거쳐야 한다 — 인터페이스 함수는 직접 호출할 수 없다.
		IFAInteractable::Execute_ReleaseInteraction(this, nullptr);
	}
}

void AFAInteractableActor::OnRep_InteractionOccupant()
{
	OnOccupantChanged(InteractionOccupant);
}
