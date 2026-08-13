#include "GAS/Abilities/FAGA_ContestMash.h"
#include "GAS/FAGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Player/FACharacter.h"

UFAGA_ContestMash::UFAGA_ContestMash()
{
	// 연타 처리는 서버에서 직접 게이지를 조작
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UFAGA_ContestMash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AFACharacter* AvatarChar = Cast<AFACharacter>(ActorInfo->AvatarActor.Get());
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();

	if (!AvatarChar || !SourceASC || !MashEffectClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FAGameplayTags& Tags = FAGameplayTags::Get();
	float ScoreDelta = 0.0f;

	// 진영별 QTE 가중치 계산
	if (SourceASC->HasMatchingGameplayTag(Tags.State_Zombie))
	{
		ScoreDelta = -4.0f; // 좀비는 누를 때마다 게이지 감소
	}
	else if (SourceASC->HasMatchingGameplayTag(Tags.State_Human))
	{
		if (SourceASC->HasMatchingGameplayTag(Tags.State_Grabbed))
		{
			ScoreDelta = 1.0f; // 잡힌 인간
		}
		else
		{
			ScoreDelta = 2.0f; // 구출하러 온 인간
		}
	}

	// 호스트 좀비를 찾아 GE(MashEffect)를 통해 ContestGauge 를 가감한다.
	AFACharacter* TargetZombie = FindTargetZombie(AvatarChar);
	if (TargetZombie)
	{
		UAbilitySystemComponent* TargetASC = TargetZombie->GetAbilitySystemComponent();
		if (TargetASC)
		{
			FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(MashEffectClass);
			SpecHandle.Data->SetSetByCallerMagnitude(Tags.Data_Rule_Magnitude, ScoreDelta);
			TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

AFACharacter* UFAGA_ContestMash::FindTargetZombie(AFACharacter* Avatar)
{
	UAbilitySystemComponent* ASC = Avatar->GetAbilitySystemComponent();
	const FAGameplayTags& Tags = FAGameplayTags::Get();

	if (ASC->HasMatchingGameplayTag(Tags.State_Zombie))
	{
		return Avatar;
	}

	// 인간인 경우, 주변에서 줄다리기(State.Contesting) 중인 좀비를 탐색
	FVector Center = Avatar->GetActorLocation();
	TArray<FHitResult> HitResults;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Avatar);

	bool bHit = Avatar->GetWorld()->SweepMultiByChannel(
		HitResults, Center, Center + FVector(0, 0, 1), FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeSphere(250.0f), QueryParams
	);

	if (bHit)
	{
		for (const FHitResult& Hit : HitResults)
		{
			AFACharacter* Target = Cast<AFACharacter>(Hit.GetActor());
			if (Target && Target->GetAbilitySystemComponent()->HasMatchingGameplayTag(Tags.State_Zombie) &&
				Target->GetAbilitySystemComponent()->HasMatchingGameplayTag(Tags.State_Contesting))
			{
				return Target;
			}
		}
	}
	return nullptr;
}
