#include "GAS/Abilities/FAGA_RescueContest.h"
#include "GAS/Abilities/FAGA_Grab.h"
#include "GAS/Abilities/FAGA_Rescue.h"
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
	const FAGameplayTags& Tags = FAGameplayTags::Get();

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(Tags.Ability_RescueContest);
	SetAssetTags(AssetTags);

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

	if (!HasAuthority(&ActivationInfo))
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();

	// InstancedPerActor 라 인스턴스가 QTE 사이에 재사용된다 — 안 지우면 두 번째 QTE 가 아예 판정되지 않는다.
	bOutcomeResolved = false;
	ContestStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	ZombieMashCount = 0;
	VictimMashCount = 0;
	RescuerMashCount = 0;

	// 페이로드에서 참가자 3명 정보 파싱
	Rescuer = Cast<AFACharacter>(const_cast<AActor*>(TriggerEventData->Instigator.Get()));
	TargetZombie = Cast<AFACharacter>(const_cast<AActor*>(TriggerEventData->Target.Get()));
	GrabbedVictim = Cast<AFACharacter>(const_cast<UObject*>(TriggerEventData->OptionalObject.Get()));

	if (!TargetZombie.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 같은 인질을 잡았다가 구출당하기를 반복하면 좀비 연타를 한 번당 RepeatRescueMashStep 만큼 무겁게 한다.
	// 상대가 바뀌면 누적을 버리고 −3.5 대 3 에서 다시 시작한다.
	if (GrabbedVictim.IsValid() && GrabbedVictim.Get() == RepeatRescueVictim.Get())
	{
		ZombieMashBonus = -RepeatRescueMashStep * RepeatRescueCount;
	}
	else
	{
		RepeatRescueVictim = GrabbedVictim;
		RepeatRescueCount = 0;
		ZombieMashBonus = 0.0f;
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

	// QTE 참가 상태를 3명 각자에게 심는다.
	if (TargetZombie.IsValid())
	{
		TargetZombie->BeginContestState(EFAContestRole::Zombie, TargetZombie.Get(), ContestDuration);
	}
	if (GrabbedVictim.IsValid())
	{
		GrabbedVictim->BeginContestState(EFAContestRole::Victim, TargetZombie.Get(), ContestDuration);
	}
	if (Rescuer.IsValid())
	{
		Rescuer->BeginContestState(EFAContestRole::Rescuer, TargetZombie.Get(), ContestDuration);
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

	// [제한 시간 타임아웃]
	UAbilityTask_WaitDelay* TaskDelay = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(0.1f, ContestDuration));
	TaskDelay->OnFinish.AddDynamic(this, &UFAGA_RescueContest::OnTimeOut);
	TaskDelay->ReadyForActivation();
}

UFAGA_RescueContest* UFAGA_RescueContest::FindActiveInstance(UAbilitySystemComponent* ASC)
{
	if (!ASC)
	{
		return nullptr;
	}

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.IsActive())
		{
			continue;
		}

		for (UGameplayAbility* Instance : Spec.GetAbilityInstances())
		{
			if (UFAGA_RescueContest* Contest = Cast<UFAGA_RescueContest>(Instance))
			{
				return Contest;
			}
		}
	}
	return nullptr;
}

void UFAGA_RescueContest::RecordMash(EFAContestRole Role)
{
	switch (Role)
	{
	case EFAContestRole::Zombie:	++ZombieMashCount;	break;
	case EFAContestRole::Victim:	++VictimMashCount;	break;
	case EFAContestRole::Rescuer:	++RescuerMashCount;	break;
	default: break;
	}
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
	// 타임아웃 시 무승부는 없고, 50 초과면 인간 승리, 아니면 좀비 승리.
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
	// 포획 게이지 위의 줄다리기 표시는 이미 게이지가 떠 있는 두 사람 몫이다.
	if (TargetZombie.IsValid())
	{
		TargetZombie->SetCaptureResistRatio(Ratio);
	}
	if (GrabbedVictim.IsValid())
	{
		GrabbedVictim->SetCaptureResistRatio(Ratio);
	}

	// QTE 프롬프트는 구조자까지 셋 다 봐야 한다.
	AFACharacter* Participants[] = { TargetZombie.Get(), GrabbedVictim.Get(), Rescuer.Get() };
	for (AFACharacter* Participant : Participants)
	{
		if (Participant)
		{
			Participant->SetContestGauge(Ratio);
		}
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

	// QTE 밸런스 확인용 — 누가 몇 번 눌렀고 게이지가 어디서 끝났는지 한 줄로 남긴다.
	{
		const UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
		const float FinalGauge = SourceASC ? SourceASC->GetNumericAttribute(UFAAttributeSet::GetContestGaugeAttribute()) : -1.0f;
		const float Elapsed = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f) - ContestStartTime;
		auto NameOf = [](const TWeakObjectPtr<AFACharacter>& Char)
		{
			const APlayerState* PS = Char.IsValid() ? Char->GetPlayerState() : nullptr;
			return PS ? PS->GetPlayerName() : FString(TEXT("None"));
		};
		UE_LOG(LogTemp, Log, TEXT("[QTE] 결과=%s | 경과=%.2fs | 게이지=%.1f | 좀비(%s)=%d회 | 구조자(%s)=%d회 | 인질(%s)=%d회 | 좀비보정=%+.1f(같은 인질 구출 %d회)"),
			bHumanWon ? TEXT("인간승") : TEXT("좀비승"), Elapsed, FinalGauge,
			*NameOf(TargetZombie), ZombieMashCount,
			*NameOf(Rescuer), RescuerMashCount,
			*NameOf(GrabbedVictim), VictimMashCount,
			ZombieMashBonus, RepeatRescueCount);
	}

	if (bHumanWon)
	{
		// 이 인질은 또 구출됐다 — 같은 사람을 다시 잡으면 그만큼 좀비 연타가 무거워진다.
		if (GrabbedVictim.IsValid() && GrabbedVictim.Get() == RepeatRescueVictim.Get())
		{
			++RepeatRescueCount;
		}

		// [인간 승리] 좀비의 포획(Grab)을 강제 취소하고 기절시킨다.
		if (TargetZombie.IsValid())
		{
			UAbilitySystemComponent* ZombieASC = TargetZombie->GetAbilitySystemComponent();
			FGameplayTagContainer GrabTagContainer(Tags.Ability_Grab);
			ZombieASC->CancelAbilities(&GrabTagContainer);

			if (ZombieStunEffectClass)
			{
				FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(ZombieStunEffectClass);

				// [Z30 구조 트라우마] 룰 태그가 있으면 8초, 없으면 기본 기절 시간 적용
				float StunDuration = ZombieASC->HasMatchingGameplayTag(Tags.Rule_Bite_RescueStun) ? 8.f : 3.f;
				SpecHandle.Data->SetSetByCallerMagnitude(Tags.Data_Rule_Magnitude, StunDuration);
				ZombieASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			}
		}

		// 구출 성공 점수는 게임모드가 매긴다. 위 CancelAbilities 가 GA_Grab 을 끝내면서
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
			if (UFAGA_Grab* GrabAbility = UFAGA_Grab::FindActiveInstance(TargetZombie->GetAbilitySystemComponent()))
			{
				GrabAbility->ResumeInfectTimer();
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

	// 좀비가 이겨서 포획이 계속되는 경우 감염 게이지는 그대로 남는다 — 줄다리기 표시만 지운다.
	BroadcastResistRatio(0.0f);

	// QTE 참가 상태 해제. 이게 내려가야 세 사람의 스페이스바 프롬프트가 사라지고
	// 스페이스바가 다시 점프로, 좌클릭이 다시 포획·구출로 돌아간다.
	AFACharacter* Participants[] = { TargetZombie.Get(), GrabbedVictim.Get(), Rescuer.Get() };
	for (AFACharacter* Participant : Participants)
	{
		if (Participant)
		{
			Participant->EndContestState();
		}
	}

	// 어빌리티 종료 시 참가자 3명 전원의 Contesting 상태를 일괄 해제한다.
	for (auto& Pair : ContestingHandles)
	{
		if (Pair.Key && Pair.Value.IsValid())
		{
			Pair.Key->GetAbilitySystemComponent()->RemoveActiveGameplayEffect(Pair.Value);
		}
	}
	ContestingHandles.Empty();

	// 같은 구조자가 끝나자마자 또 구출에 들어가지 못하게 한다 (이기든 지든).
	if (Rescuer.IsValid())
	{
		UFAGA_Rescue::StartRescueCooldown(Rescuer->GetAbilitySystemComponent());
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
