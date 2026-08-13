#include "GAS/Abilities/FAGA_RescueContest.h"
#include "GAS/Abilities/FAGA_Grab.h"
#include "GAS/FAGameplayTags.h"
#include "AttributeSet/FAAttributeSet.h"
#include "Game/FAGameMode.h"
#include "Player/FACharacter.h"
#include "Player/FAPlayerState.h"
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

	// InstancedPerActor 라 인스턴스가 QTE 사이에 재사용된다 — 안 지우면 두 번째 QTE 가 아예 판정되지 않는다.
	bOutcomeResolved = false;

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

	// 게이지를 양쪽 HUD 로 흘려보낸다. 초기화 뒤에 구독해야 50 을 두 번 쓰지 않는다.
	ContestGaugeChangedHandle = SourceASC->GetGameplayAttributeValueChangeDelegate(
		UFAAttributeSet::GetContestGaugeAttribute()).AddUObject(this, &UFAGA_RescueContest::HandleContestGaugeChanged);

	BroadcastResistRatio(0.5f);

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

void UFAGA_RescueContest::HandleContestGaugeChanged(const FOnAttributeChangeData& Data)
{
	// 게이지 0~100 을 그대로 0~1 로. 1 에 가까울수록 인간이 이기고 있다는 뜻이다.
	BroadcastResistRatio(Data.NewValue / 100.0f);
}

void UFAGA_RescueContest::BroadcastResistRatio(float Ratio)
{
	// 게이지가 이미 떠 있는 두 사람만 대상이다 — 구조자는 포획 상태가 아니라 값을 받아도 쓸 곳이 없다.
	if (TargetZombie.IsValid())
	{
		TargetZombie->SetCaptureResistRatio(Ratio);
	}
	if (GrabbedVictim.IsValid())
	{
		GrabbedVictim->SetCaptureResistRatio(Ratio);
	}
}

void UFAGA_RescueContest::ResolveContestOutcome(bool bHumanWon)
{
	// 먼저 도착한 판정만 유효하다.
	if (bOutcomeResolved)
	{
		return;
	}
	bOutcomeResolved = true;

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

		// 구출 성공 +1 은 게임모드가 매긴다. 위 CancelAbilities 가 GA_Grab 을 끝내면서
		// 포획 게이지까지 같이 내려가므로, 점수는 그 뒤에 붙여도 순서 문제가 없다.
		AFAPlayerState* RescuerState = Rescuer.IsValid() ? Rescuer->GetPlayerState<AFAPlayerState>() : nullptr;
		AFAPlayerState* VictimState = GrabbedVictim.IsValid() ? GrabbedVictim->GetPlayerState<AFAPlayerState>() : nullptr;

		if (const UWorld* World = GetWorld())
		{
			if (AFAGameMode* GameMode = World->GetAuthGameMode<AFAGameMode>())
			{
				GameMode->HandleRescueSuccess(RescuerState, VictimState);
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
	// 구독을 남겨두면 다음 QTE 에서 죽은 인스턴스가 또 불린다.
	if (ContestGaugeChangedHandle.IsValid())
	{
		if (UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo())
		{
			SourceASC->GetGameplayAttributeValueChangeDelegate(
				UFAAttributeSet::GetContestGaugeAttribute()).Remove(ContestGaugeChangedHandle);
		}
		ContestGaugeChangedHandle.Reset();
	}

	// 좀비가 이겨서 포획이 계속되는 경우 게이지는 그대로 남는다 — 줄다리기 표시만 지운다.
	BroadcastResistRatio(0.0f);

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
