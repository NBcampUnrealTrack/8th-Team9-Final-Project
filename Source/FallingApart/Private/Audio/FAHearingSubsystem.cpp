// [Z22 귀먹음 추가] 이 파일은 청취 상태와 발소리 음소거만 담당한다.
#include "Audio/FAHearingSubsystem.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GAS/FAGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "Voice/FAVoiceTalker.h"

UFAHearingSubsystem* UFAHearingSubsystem::Get(const UObject* WorldContextObject)
{
	if (!GEngine || !WorldContextObject)
	{
		return nullptr;
	}
	UWorld* World = GEngine->GetWorldFromContextObject(
		WorldContextObject, EGetWorldErrorMode::ReturnNull);
	return World ? World->GetSubsystem<UFAHearingSubsystem>() : nullptr;
}

bool UFAHearingSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && World->GetNetMode() != NM_DedicatedServer;
}

void UFAHearingSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	UpdateHearingState();
}

bool UFAHearingSubsystem::IsFootstepSound(FName SoundId)
{
	return SoundId.ToString().
	StartsWith(TEXT("Foot_")) || SoundId == FName(TEXT("Ladder_Step"));
}

UAudioComponent* UFAHearingSubsystem::TrackFootstep(FName SoundId, UAudioComponent* Component)
{
	if (Component && IsFootstepSound(SoundId))
	{
		ActiveFootsteps.Add({SoundId, Component});
	}
	return Component;
}

bool UFAHearingSubsystem::IsZombieFootstepSound(FName SoundId)
{
	return SoundId == FName(TEXT("Foot_Zombie_Walk"))
		|| SoundId == FName(TEXT("Foot_Zombie_Run"));
}

void UFAHearingSubsystem::StopActiveFootsteps(bool bZombieOnly)
{
	for (const FActiveFootstep& Entry : ActiveFootsteps)
	{
		if (bZombieOnly && !IsZombieFootstepSound(Entry.SoundId))
		{
			continue;
		}
		if (UAudioComponent* Component = Entry.Component.Get())
		{
			Component->Stop();
		}
	}
	ActiveFootsteps.RemoveAll([bZombieOnly](const FActiveFootstep& Entry)
	{
		return !bZombieOnly || IsZombieFootstepSound(Entry.SoundId);
	});
}

bool UFAHearingSubsystem::IsDeaf() const
{
	return HasListenerTag(FAGameplayTags::Get().Rule_Audio_Deaf);
}

bool UFAHearingSubsystem::HasNoStepRule() const
{
	return HasListenerTag(FAGameplayTags::Get().Rule_Audio_NoStep);
}

bool UFAHearingSubsystem::ShouldMuteSound(FName SoundId) const
{
	return IsFootstepSound(SoundId)
		&& (IsDeaf() || (IsZombieFootstepSound(SoundId) && HasNoStepRule()));
}

bool UFAHearingSubsystem::HasListenerTag(const FGameplayTag& Tag) const
{
	// 화자가 아닌 청취자의 현재 Pawn을 조회하여 관전/리스폰 이후에도 갱신한다.
	const APlayerController* Controller = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	
	APawn* Pawn = Controller && Controller->IsLocalController() ? Controller->GetPawn() : nullptr;
	if (!IsValid(Pawn) || Pawn->IsActorBeingDestroyed())
	{
		return false;
	}
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
	return ASC && ASC->HasMatchingGameplayTag(Tag);
}

float UFAHearingSubsystem::GetVoiceHearingMultiplier() const
{
	return IsDeaf() ? 0.0f : 1.0f;
}

bool UFAHearingSubsystem::IsTickable() const
{
	const UWorld* World = GetWorld();
	return !IsTemplate() && World && World->HasBegunPlay()
		&& !World->bIsTearingDown && World->GetNetMode() != NM_DedicatedServer;
}

TStatId UFAHearingSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFAHearingSubsystem, STATGROUP_Tickables);
}

void UFAHearingSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// [Z22 귀먹음] 완료된 단발 소리의 참조가 계속 쌓이지 않도록 정리한다.
	ActiveFootsteps.RemoveAll([](const FActiveFootstep& Entry)
	{
		return !Entry.Component.IsValid() || !Entry.Component->IsPlaying();
	});
	// [Z22 귀먹음] BeginPlay 이후 소유권 확정, 관전/리스폰은 태그 이벤트가 없을 수 있다.
	// 이 경우만 보완하며, 일반적인 룰 적용·해제는 Status의 이벤트에서 실행한다.
	const APlayerController* Controller = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	APawn* Pawn = Controller && Controller->IsLocalController() ? Controller->GetPawn() : nullptr;
	if (!IsValid(Pawn) || Pawn->IsActorBeingDestroyed())
	{
		Pawn = nullptr;
	}
	if (ListenerPawn.Get() != Pawn || (ListenerPawn.IsStale() && !Pawn))
	{
		ListenerPawn = Pawn;
		UpdateHearingState();
	}
}

void UFAHearingSubsystem::UpdateHearingState()
{
	const bool bDeaf = IsDeaf();
	const bool bNoStep = HasNoStepRule();
	// Status 이벤트와 Pawn 교체 보정이 겹쳐도 실제 볼륨은 상태 변화 때만 갱신한다.
	if (bHasHearingState && bDeaf == bWasDeaf && bNoStep == bHadNoStepRule)
	{
		return;
	}
	const bool bVoiceChanged = !bHasHearingState || bDeaf != bWasDeaf;
	bHasHearingState = true;
	bWasDeaf = bDeaf;
	bHadNoStepRule = bNoStep;
	if (bDeaf)
	{
		StopActiveFootsteps();
	}
	else if (bNoStep)
	{
		StopActiveFootsteps(true);
	}
	if (bVoiceChanged)
	{
		UFAVoiceTalker::RefreshTalkingVolumesForWorld(GetWorld());
	}
}

void UFAHearingSubsystem::Deinitialize()
{
	StopActiveFootsteps();
	Super::Deinitialize();
}
