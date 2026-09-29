#include "Audio/FAPhantomAudioComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Audio/FAAudioSettings.h"
#include "Audio/FAAudioSubsystem.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Character.h"
#include "GAS/FAGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UFAPhantomAudioComponent::UFAPhantomAudioComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UFAPhantomAudioComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerCharacter = Cast<ACharacter>(GetOwner());
}

void UFAPhantomAudioComponent::SetRuleActive(bool bActive)
{
	bPhantomRuleActive = bActive;
	ResetPhantomAudio();
	// 활성 룰만 Tick한다. 소유권이 나중에 복제되는 경우는 기존 로컬 판정으로 보완한다.
	SetComponentTickEnabled(bActive && GetNetMode() != NM_DedicatedServer);
}

void UFAPhantomAudioComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdatePhantomAudio(DeltaTime);
}

void UFAPhantomAudioComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetRuleActive(false);
	Super::EndPlay(EndPlayReason);
}

void UFAPhantomAudioComponent::UpdatePhantomAudio(float DeltaTime)
{
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	if (!IsValid(OwnerCharacter) || OwnerCharacter->IsActorBeingDestroyed() || !OwnerCharacter->IsLocallyControlled()
		|| GetNetMode() == NM_DedicatedServer || !ASC || !bPhantomRuleActive)
	{
		ResetPhantomAudio();
		return;
	}

	const FAGameplayTags& Tags = FAGameplayTags::Get();
	if (ASC->HasMatchingGameplayTag(Tags.State_Human))
	{
		UpdateHumanPhantomAudio(DeltaTime);
	}
	else if (ASC->HasMatchingGameplayTag(Tags.State_Zombie))
	{
		UpdateZombiePhantomAudio(DeltaTime);
	}
	else
	{
		ResetPhantomAudio();
	}
}

// H26: 주변의 한 위치에서 비명을 단발 재생한다.
void UFAPhantomAudioComponent::UpdateHumanPhantomAudio(float DeltaTime)
{
	if (PhantomScreamSound.IsNull())
	{
		ResetPhantomAudio();
		return;
	}

	const float MinInterval = FMath::Max(0.1f, PhantomScreamMinInterval);
	const float MaxInterval = FMath::Max(MinInterval, PhantomScreamMaxInterval);
	if (UAudioComponent* Component = PhantomScreamComponent.Get())
	{
		PhantomScreamPlaybackRemaining -= DeltaTime;
		if (PhantomScreamPlaybackRemaining <= 0.0f)
		{
			Component->Stop();
			PhantomScreamComponent.Reset();
		}
	}
	if (PhantomTimeRemaining < 0.0f)
	{
		PhantomTimeRemaining = FMath::FRandRange(MinInterval, MaxInterval);
		return;
	}

	PhantomTimeRemaining -= DeltaTime;
	if (PhantomTimeRemaining > 0.0f)
	{
		return;
	}
	const float MinDistance = FMath::Max(0.0f, PhantomScreamMinDistance);
	const float MaxDistance = FMath::Max(MinDistance, PhantomScreamMaxDistance);
	if (UAudioComponent* Previous = PhantomScreamComponent.Get())
	{
		Previous->Stop();
	}
	if (USoundBase* Sound = PhantomScreamSound.LoadSynchronous())
	{
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		const FVector Location = OwnerCharacter->GetActorLocation()
			+ FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * FMath::FRandRange(MinDistance, MaxDistance);
		PhantomScreamComponent = UGameplayStatics::SpawnSoundAtLocation(
			GetWorld(), Sound, Location, FRotator::ZeroRotator, 1.0f, 1.0f, 0.0f,
			UFAAudioSettings::GetDefaultAttenuation(EFASoundCategory::Move));
		PhantomScreamPlaybackRemaining = FMath::Max(0.1f, PhantomScreamMaxDuration);
	}
	PhantomTimeRemaining = FMath::FRandRange(MinInterval, MaxInterval);
	return;
}

// Z23: 세 위치에서 발소리 묶음을 이어서 재생한다.
void UFAPhantomAudioComponent::UpdateZombiePhantomAudio(float DeltaTime)
{
	if (PhantomFootstepSoundIds.IsEmpty())
	{
		ResetPhantomAudio();
		return;
	}

	const float MinInterval = FMath::Max(0.1f, PhantomMinInterval);
	const float MaxInterval = FMath::Max(MinInterval, PhantomMaxInterval);
	const float BurstMinDuration = FMath::Max(0.1f, PhantomBurstMinDuration);
	const float BurstMaxDuration = FMath::Max(BurstMinDuration, PhantomBurstMaxDuration);
	const float BurstMinStepInterval = FMath::Max(0.1f, PhantomBurstMinStepInterval);
	const float BurstMaxStepInterval = FMath::Max(BurstMinStepInterval, PhantomBurstMaxStepInterval);

	// 환청 묶음이 진행 중이면 세 위치에서 각자 랜덤 간격으로 발소리를 이어서 낸다.
	if (PhantomBurstRemainings.Num() > 0)
	{
		bool bAnyEmitterActive = false;
		for (int32 Index = 0; Index < PhantomBurstLocations.Num(); ++Index)
		{
			if (PhantomBurstStartDelays[Index] > 0.0f)
			{
				PhantomBurstStartDelays[Index] -= DeltaTime;
				bAnyEmitterActive = true;
				if (PhantomBurstStartDelays[Index] > 0.0f)
				{
					continue;
				}
				PhantomBurstRemainings[Index] = FMath::FRandRange(BurstMinDuration, BurstMaxDuration);
				PhantomBurstStepTimers[Index] = 0.0f;
			}
			const bool bWasActive = PhantomBurstRemainings[Index] > 0.0f;
			if (!bWasActive)
			{
				continue;
			}
			PhantomBurstRemainings[Index] -= DeltaTime;
			if (PhantomBurstRemainings[Index] <= 0.0f)
			{
				if (bWasActive)
				{
					// 다른 Emitter가 재생 중인 동안 즉시 새 발소리 묶음을 시작해 공백을 없앤다.
					const float MinDistance = FMath::Max(0.0f, PhantomMinDistance);
					const float MaxDistance = FMath::Max(MinDistance, PhantomMaxDistance);
					const float NewAngle = FMath::FRandRange(0.0f, 2.0f * PI);
					const float NewDistance = FMath::FRandRange(MinDistance, MaxDistance);
					PhantomBurstLocations[Index] = OwnerCharacter->GetActorLocation()
						+ FVector(FMath::Cos(NewAngle), FMath::Sin(NewAngle), 0.0f) * NewDistance;
					PhantomBurstRemainings[Index] = FMath::FRandRange(BurstMinDuration, BurstMaxDuration);
					PhantomBurstStepTimers[Index] = FMath::FRandRange(BurstMinStepInterval, BurstMaxStepInterval);
					bAnyEmitterActive = true;
				}
				continue;
			}
			bAnyEmitterActive = true;
			PhantomBurstStepTimers[Index] -= DeltaTime;
			if (PhantomBurstStepTimers[Index] <= 0.0f)
			{
				if (UFAAudioSubsystem* Audio = UFAAudioSubsystem::Get(this))
				{
					const FName SoundId = PhantomFootstepSoundIds[FMath::RandHelper(PhantomFootstepSoundIds.Num())];
					const float MinVolume = FMath::Max(0.0f, PhantomMinVolumeMultiplier);
					const float MaxVolume = FMath::Max(MinVolume, PhantomMaxVolumeMultiplier);
					const float Volume = FMath::FRandRange(MinVolume, MaxVolume);
					Audio->PlaySoundAtLocation(SoundId, PhantomBurstLocations[Index], Volume);
				}
			PhantomBurstStepTimers[Index] = FMath::FRandRange(BurstMinStepInterval, BurstMaxStepInterval);
			}
		}
		if (!bAnyEmitterActive)
		{
			PhantomBurstRemainings.Reset();
			PhantomBurstStepTimers.Reset();
			PhantomBurstStartDelays.Reset();
			PhantomBurstLocations.Reset();
			PhantomTimeRemaining = FMath::FRandRange(MinInterval, MaxInterval);
		}
		return;
	}

	if (PhantomTimeRemaining < 0.0f)
	{
		PhantomTimeRemaining = FMath::FRandRange(MinInterval, MaxInterval);
		return;
	}

	PhantomTimeRemaining -= DeltaTime;
	if (PhantomTimeRemaining > 0.0f)
	{
		return;
	}
	const float MinDistance = FMath::Max(0.0f, PhantomMinDistance);
	const float MaxDistance = FMath::Max(MinDistance, PhantomMaxDistance);
	PhantomBurstLocations.SetNum(3);
	PhantomBurstRemainings.SetNum(3);
	PhantomBurstStartDelays.SetNum(3);
	PhantomBurstStepTimers.SetNum(3);
	for (int32 Index = 0; Index < PhantomBurstLocations.Num(); ++Index)
	{
		const float EmitterAngle = FMath::FRandRange(0.0f, 2.0f * PI);
		const float EmitterDistance = FMath::FRandRange(MinDistance, MaxDistance);
		PhantomBurstLocations[Index] = OwnerCharacter->GetActorLocation()
			+ FVector(FMath::Cos(EmitterAngle), FMath::Sin(EmitterAngle), 0.0f) * EmitterDistance;
		PhantomBurstRemainings[Index] = 0.0f;
		// 세 명의 생성 시작 시점 자체를 분리한다.
		PhantomBurstStartDelays[Index] = FMath::FRandRange(0.1f, 0.5f);
		PhantomBurstStepTimers[Index] = 0.0f;
	}
	PhantomTimeRemaining = FMath::FRandRange(MinInterval, MaxInterval);
}

void UFAPhantomAudioComponent::ResetPhantomAudio()
{
	PhantomTimeRemaining = -1.0f;
	PhantomBurstRemainings.Reset();
	PhantomBurstStartDelays.Reset();
	PhantomBurstStepTimers.Reset();
	PhantomBurstLocations.Reset();
	PhantomScreamPlaybackRemaining = 0.0f;
	if (UAudioComponent* Component = PhantomScreamComponent.Get())
	{
		Component->Stop();
	}
	PhantomScreamComponent.Reset();
}

