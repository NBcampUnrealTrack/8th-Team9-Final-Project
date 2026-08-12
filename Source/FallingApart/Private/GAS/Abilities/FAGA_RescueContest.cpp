#include "GAS/Abilities/FAGA_RescueContest.h"
#include "GAS/Abilities/FAGA_Grab.h"
#include "GAS/FAGameplayTags.h"
#include "AttributeSet/FAAttributeSet.h"
#include "Player/FACharacter.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitAttributeChange.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"

UFAGA_RescueContest::UFAGA_RescueContest()
{
	// QTE 판정 및 게이지 동기화는 서버 권한으로 일원화한다.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UFAGA_RescueContest::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo) || !TriggerEventData)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();

	// 페이로드에서 참가자 3명 정보 파싱
	Rescuer = Cast<AFACharacter>(const_cast<AActor*>(TriggerEventData->Instigator.Get()));
	TargetZombie = Cast<AFACharacter>(const_cast<AActor*>(TriggerEventData->Target.Get()));
	GrabbedVictim = Cast<AFACharacter>(const_cast<UObject*>(TriggerEventData->OptionalObject.Get()));

	if (!TargetZombie.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 3명 전원에게 Contesting(이동/행동 불가) GE를 적용하고 핸들을 보관한다.
	if (ContestingEffectClass)
	{
		AFACharacter* Participants[] = { Rescuer.Get(), TargetZombie.Get(), GrabbedVictim.Get() };
		for (AFACharacter* Participant : Participants)
		{
			if (Participant)
			{
				UAbilitySystemComponent* TargetASC = Participant->GetAbilitySystemComponent();
				FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(ContestingEffectClass);
				FActiveGameplayEffectHandle ActiveHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
				ContestingHandles.Add(Participant, ActiveHandle);
			}
		}
	}

	// QTE 게이지 50(중립)으로 초기화
	SourceASC->SetNumericAttributeBase(UFAAttributeSet::GetContestGaugeAttribute(), 50.0f);

	//~ GAS Tasks: 게이지 임계치 및 제한 시간 감시

	// [인간 승리: 게이지 100 도달]
	UAbilityTask_WaitAttributeChange* TaskHumanWin = UAbilityTask_WaitAttributeChange::WaitForAttributeChangeWithComparison(
		this, UFAAttributeSet::GetContestGaugeAttribute(), FGameplayTag(), FGameplayTag(), EWaitAttributeChangeComparison::GreaterThanOrEqualTo, 100.0f);
	TaskHumanWin->OnChange.AddDynamic(this, &UFAGA_RescueContest::OnHumanWin);
	TaskHumanWin->ReadyForActivation();

	// [좀비 승리: 게이지 0 도달]
	UAbilityTask_WaitAttributeChange* TaskZombieWin = UAbilityTask_WaitAttributeChange::WaitForAttributeChangeWithComparison(
		this, UFAAttributeSet::GetContestGaugeAttribute(), FGameplayTag(), FGameplayTag(), EWaitAttributeChangeComparison::LessThanOrEqualTo, 0.0f);
	TaskZombieWin->OnChange.AddDynamic(this, &UFAGA_RescueContest::OnZombieWin);
	TaskZombieWin->ReadyForActivation();

	// [3초 타임아웃]
	UAbilityTask_WaitDelay* TaskDelay = UAbilityTask_WaitDelay::WaitDelay(this, 3.0f);
	TaskDelay->OnFinish.AddDynamic(this, &UFAGA_RescueContest::OnTimeOut);
	TaskDelay->ReadyForActivation();
}

void UFAGA_RescueContest::OnHumanWin()
{
	ResolveContestOutcome(true);
}

void UFAGA_RescueContest::OnZombieWin()
{
	ResolveContestOutcome(false);
}

void UFAGA_RescueContest::OnTimeOut()
{
	// 3초 타임아웃 시 무승부는 없고, 50 초과면 인간 승리, 아니면 좀비 승리.
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	float FinalGauge = SourceASC->GetNumericAttribute(UFAAttributeSet::GetContestGaugeAttribute());
	ResolveContestOutcome(FinalGauge > 50.0f);
}

void UFAGA_RescueContest::ResolveContestOutcome(bool bHumanWon)
{
	const FAGameplayTags& Tags = FAGameplayTags::Get();

	if (bHumanWon)
	{
		// [인간 승리] 좀비의 포획(Grab)을 강제 취소하고 기절시킨다.
		if (TargetZombie.IsValid())
		{
			UAbilitySystemComponent* ZombieASC = TargetZombie->GetAbilitySystemComponent();
			FGameplayTagContainer GrabTagContainer(Tags.Ability_Grab);
			ZombieASC->CancelAbilities(&GrabTagContainer);

			if (ZombieStunEffectClass)
			{
				FGameplayEffectSpecHandle StunSpec = MakeOutgoingGameplayEffectSpec(ZombieStunEffectClass);
				ZombieASC->ApplyGameplayEffectSpecToSelf(*StunSpec.Data.Get());
			}
		}
	}
	else
	{
		// [좀비 승리] 정지해 두었던 감염 타이머를 재개한다.
		if (TargetZombie.IsValid())
		{
			UAbilitySystemComponent* ZombieASC = TargetZombie->GetAbilitySystemComponent();
			for (const FGameplayAbilitySpec& Spec : ZombieASC->GetActivatableAbilities())
			{
				if (Spec.IsActive())
				{
					for (UGameplayAbility* ActiveInstance : Spec.GetAbilityInstances())
					{
						if (UFAGA_Grab* GrabAbility = Cast<UFAGA_Grab>(ActiveInstance))
						{
							GrabAbility->ResumeInfectTimer();
							break;
						}
					}
				}
			}
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFAGA_RescueContest::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 어빌리티 종료 시 참가자 3명 전원의 Contesting 상태를 일괄 해제한다.
	for (auto& Pair : ContestingHandles)
	{
		if (Pair.Key && Pair.Value.IsValid())
		{
			Pair.Key->GetAbilitySystemComponent()->RemoveActiveGameplayEffect(Pair.Value);
		}
	}
	ContestingHandles.Empty();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
