#include "GAS/Abilities/FAGA_ContestMash.h"
#include "GAS/FAGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Player/FACharacter.h"
#include "GAS/Abilities/FAGA_RescueContest.h"

namespace FAContestAudio
{
	/** 잡힌 인간이 몸부림치는 소리 — 누른 사람이 인질일 때만. */
	static const FName Struggle(TEXT("Grab_Struggle"));

	/**
	 * 몸부림 소리를 멀티캐스트하는 최소 간격(초).
	 * 표의 Grab_Struggle 쿨다운(0.20)과 같은 값으로 둔다.
	 */
	static constexpr float StruggleSendInterval = 0.20f;
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

	if (!HasAuthority(&ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FAGameplayTags& Tags = FAGameplayTags::Get();
	float ScoreDelta = 0.0f;

	// 역할은 서버가 QTE 를 열 때 지목해 둔 값을 그대로 쓴다.
	// 태그(State.Grabbed)로 인질과 구조자를 가르면 GE 애셋이 비어 있을 때 둘 다 구조자로 잡힌다.
	EFAContestRole Role = AvatarChar->GetContestState().Role;
	if (Role == EFAContestRole::None)
	{
		// QTE 상태가 아직 안 왔거나 예전 경로로 들어온 경우의 폴백.
		if (SourceASC->HasMatchingGameplayTag(Tags.State_Zombie))
		{
			Role = EFAContestRole::Zombie;
		}
		else if (SourceASC->HasMatchingGameplayTag(Tags.State_Human))
		{
			Role = SourceASC->HasMatchingGameplayTag(Tags.State_Grabbed)
				? EFAContestRole::Victim
				: EFAContestRole::Rescuer;
		}
	}

	// 역할별 QTE 가중치 계산
	switch (Role)
	{
	case EFAContestRole::Zombie:
		ScoreDelta = ZombieMashDelta; // 좀비는 누를 때마다 게이지 감소
		break;

	case EFAContestRole::Victim:
		ScoreDelta = VictimMashDelta; // 잡힌 인간 — 감염 중이라 기여는 미미하다

		// 인질만 몸부림 소리를 낸다 — 구조자 쪽에서 나면 누가 잡혀 있는지 헷갈린다.
		// 연타 주기보다 훨씬 성기게 내보낸다.
		{
			const UWorld* World = GetWorld();
			const float Now = World ? World->GetTimeSeconds() : 0.0f;
			if (Now - LastStruggleSendTime >= FAContestAudio::StruggleSendInterval)
			{
				LastStruggleSendTime = Now;
				AvatarChar->Multicast_PlayFASoundAtLocation(FAContestAudio::Struggle);
			}
		}
		break;

	case EFAContestRole::Rescuer:
		ScoreDelta = RescuerMashDelta; // 구출하러 온 인간
		break;

	default:
		break;
	}

	// 연타 타격감(QTE_Press)은 여기서 내지 않는다.

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

			if (UFAGA_RescueContest* Contest = UFAGA_RescueContest::FindActiveInstance(TargetASC))
			{
				Contest->RecordMash(Role);
			}
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

AFACharacter* UFAGA_ContestMash::FindTargetZombie(AFACharacter* Avatar)
{
	// 서버가 QTE 를 열 때 게이지 주인을 직접 지목해 뒀다면 그걸 그대로 쓴다.
	// 아래 주변 탐색은 태그와 거리에 둘 다 의존해서, 셋 중 하나만 어긋나도 연타가 통째로 먹통이 된다.
	if (AFACharacter* HostZombie = Cast<AFACharacter>(Avatar->GetContestState().HostZombie))
	{
		return HostZombie;
	}

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
