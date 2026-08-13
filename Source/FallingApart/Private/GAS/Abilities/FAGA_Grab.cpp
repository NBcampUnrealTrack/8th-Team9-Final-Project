// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Abilities/FAGA_Grab.h"
#include "GAS/FAGameplayTags.h"
#include "AttributeSet/FAAttributeSet.h"
#include "Player/FACharacter.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"

namespace FAGrabAudio
{
	// 표의 RowName 과 같아야 한다 (DT_Sound_Character).
	// 이 어빌리티는 ServerOnly 라 소리는 반드시 AFACharacter 의 멀티캐스트를 거쳐야 한다.
	static const FName Attempt(TEXT("Grab_Attempt"));
	static const FName Success(TEXT("Grab_Success"));
	static const FName Miss(TEXT("Grab_Miss"));
	static const FName InfectComplete(TEXT("Infect_Complete"));
}

UFAGA_Grab::UFAGA_Grab()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	const FAGameplayTags& Tags = FAGameplayTags::Get();

	// 어빌리티 태그 등록
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(Tags.Ability_Grab);
	SetAssetTags(AssetTags);

	// 포획을 차단하는 모든 상태 태그 집약
	// 경직 · 줄다리기(QTE) 중 · 이미 대상을 잡고 있는 상태 · 자신이 잡힌 상태
	ActivationBlockedTags.AddTag(Tags.State_Stunned);
	ActivationBlockedTags.AddTag(Tags.State_Contesting);
	ActivationBlockedTags.AddTag(Tags.State_Grabbing);
	ActivationBlockedTags.AddTag(Tags.State_Grabbed);

	RemainingInfectTime = 0.0f;
}

void UFAGA_Grab::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AFACharacter* ZombieChar = Cast<AFACharacter>(ActorInfo->AvatarActor.Get());
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!ZombieChar || !SourceASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FAGameplayTags& Tags = FAGameplayTags::Get();

	float GrabRange = SourceASC->GetNumericAttribute(UFAAttributeSet::GetGrabRangeAttribute());
	if (GrabRange <= 0.0f) GrabRange = 200.0f;

	// 휘두르는 소리는 맞았든 빗나갔든 먼저 난다 — 주변 인간이 반응할 수 있어야 한다.
	ZombieChar->Multicast_PlayFASoundAtLocation(FAGrabAudio::Attempt);

	float SphereRadius = 50.0f;
	AFACharacter* TargetHuman = FindTargetInFront(ZombieChar, GrabRange, SphereRadius, Tags.State_Human);

	// 대상이 없거나 이미 다른 좀비에게 잡힌 상태인 경우
	if (!TargetHuman || TargetHuman->GetAbilitySystemComponent()->HasMatchingGameplayTag(Tags.State_Grabbed))
	{
		ZombieChar->Multicast_PlayFASoundAtLocation(FAGrabAudio::Miss);

		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// 포획 성공 처리
	GrabbedVictim = TargetHuman;
	UAbilitySystemComponent* TargetASC = TargetHuman->GetAbilitySystemComponent();

	if (TargetASC)
	{
		// 성공음은 실제로 붙잡는 처리 안에서 낸다 — 밖에서 내면 아래가 통째로 건너뛰었을 때
		// 아무 일도 안 일어났는데 소리만 나간다.
		ZombieChar->Multicast_PlayFASoundAtLocation(FAGrabAudio::Success);

		// Effect 적용
		if (GrabberGrabbingEffectClass)
		{
			FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(GrabberGrabbingEffectClass);
			GrabberEffectHandle = SourceASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}

		if (VictimGrabbedEffectClass)
		{
			FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(VictimGrabbedEffectClass);
			VictimEffectHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
		}

		float InfectDuration = SourceASC->GetNumericAttribute(UFAAttributeSet::GetInfectDurationAttribute());
		if (InfectDuration <= 0.0f) InfectDuration = 3.0f;

		ZombieChar->GetWorld()->GetTimerManager().SetTimer(
			InfectTimerHandle,
			this,
			&UFAGA_Grab::OnInfectTimerExpired,
			InfectDuration,
			false
		);

		// 거리 이탈 검사용 타이머
		ZombieChar->GetWorld()->GetTimerManager().SetTimer(
			DistanceCheckTimerHandle,
			this,
			&UFAGA_Grab::CheckDistanceDuringGrab,
			0.1f,
			true
		);
	}
}

void UFAGA_Grab::PauseInfectTimer()
{
	if (UWorld* World = GetWorld())
	{
		RemainingInfectTime = World->GetTimerManager().GetTimerRemaining(InfectTimerHandle);
		World->GetTimerManager().ClearTimer(InfectTimerHandle);
	}
}

void UFAGA_Grab::ResumeInfectTimer()
{
	if (UWorld* World = GetWorld())
	{
		if (RemainingInfectTime > 0.0f)
		{
			World->GetTimerManager().SetTimer(
				InfectTimerHandle,
				this,
				&UFAGA_Grab::OnInfectTimerExpired,
				RemainingInfectTime,
				false
			);
		}
	}
}

void UFAGA_Grab::CheckDistanceDuringGrab()
{
	AFACharacter* ZombieChar = Cast<AFACharacter>(GetAvatarActorFromActorInfo());
	if (!ZombieChar || !GrabbedVictim.IsValid())
	{
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
		return;
	}

	float Distance = FVector::Dist(ZombieChar->GetActorLocation(), GrabbedVictim->GetActorLocation());
	if (Distance > 300.0f)
	{
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
	}
}

void UFAGA_Grab::OnInfectTimerExpired()
{
	if (GrabbedVictim.IsValid())
	{
		// 감염이 성립한 지점의 소리 — 주변 사람이 "저기서 한 명 넘어갔다"를 듣는 용도다.
		// 당사자에게는 UFAHUDWidget 이 진영 복제를 보고 Infect_Blackout + Sting_Infected 라는
		// 전용 2D 소리를 내주므로, 본인 화면에서는 이걸 빼야 같은 순간에 세 겹으로 겹치지 않는다.
		GrabbedVictim->Multicast_PlayFASoundAtLocation(FAGrabAudio::InfectComplete, /*bSkipLocalOwner=*/true);

		// TODO: 진영 전환(Rule.Team.Zombie) 및 개인 점수 +2 지급
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFAGA_Grab::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InfectTimerHandle);
		World->GetTimerManager().ClearTimer(DistanceCheckTimerHandle);
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (SourceASC)
	{
		if (GrabberEffectHandle.IsValid())
		{
			SourceASC->RemoveActiveGameplayEffect(GrabberEffectHandle);
		}
		if (GrabbedVictim.IsValid() && VictimEffectHandle.IsValid())
		{
			if (UAbilitySystemComponent* TargetASC = GrabbedVictim->GetAbilitySystemComponent())
			{
				TargetASC->RemoveActiveGameplayEffect(VictimEffectHandle);
			}
		}
	}

	GrabbedVictim.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

AFACharacter* UFAGA_Grab::FindTargetInFront(AFACharacter* Avatar, float Range, float Radius, FGameplayTag RequiredTag)
{
	if (!Avatar) return nullptr;

	FVector StartLoc = Avatar->GetActorLocation();
	FVector EndLoc = StartLoc + (Avatar->GetActorForwardVector() * Range);

	TArray<FHitResult> HitResults;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Avatar);

	bool bHit = Avatar->GetWorld()->SweepMultiByChannel(
		HitResults, StartLoc, EndLoc, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeSphere(Radius), QueryParams
	);

	if (bHit)
	{
		for (const FHitResult& Hit : HitResults)
		{
			AFACharacter* Target = Cast<AFACharacter>(Hit.GetActor());
			if (Target && Target->GetAbilitySystemComponent()->HasMatchingGameplayTag(RequiredTag))
			{
				return Target;
			}
		}
	}
	return nullptr;
}
