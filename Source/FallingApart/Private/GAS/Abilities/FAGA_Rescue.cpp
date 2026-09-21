// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Abilities/FAGA_Rescue.h"
#include "GAS/Abilities/FAGA_Grab.h"
#include "GAS/Abilities/FAGA_RescueContest.h"
#include "GAS/FAGameplayTags.h"
#include "AttributeSet/FAAttributeSet.h"
#include "Player/FACharacter.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace FARescueAudio
{
	// ServerOnly 어빌리티라 소리는 AFACharacter 의 멀티캐스트를 거친다.
	static const FName Start(TEXT("Rescue_Start"));
	static const FName QTEStart(TEXT("QTE_Start"));
}

UFAGA_Rescue::UFAGA_Rescue()
{
	const FAGameplayTags& Tags = FAGameplayTags::Get();

	// 어빌리티 태그 등록
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(Tags.Ability_Rescue);
	SetAssetTags(AssetTags);

	// 구출을 차단하는 모든 태그 집약
	// H32 겁쟁이(구조 불가) · 경직 · 줄다리기(QTE) 중 · 자신이 잡힌 상태
	//ActivationBlockedTags.AddTag(Tags.Rule_Rescue_Disabled); // TODO: Tag 추가
	ActivationBlockedTags.AddTag(Tags.State_Stunned);
	ActivationBlockedTags.AddTag(Tags.State_Contesting);
	ActivationBlockedTags.AddTag(Tags.State_Grabbed);
}

bool UFAGA_Rescue::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// CDO 로 불릴 때도 있다 — 그때는 인스턴스를 찾아서 본다.
	const UFAGA_Rescue* Instance = this;
	if (!IsInstantiated() && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		if (const FGameplayAbilitySpec* Spec = ActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(Handle))
		{
			const TArray<UGameplayAbility*> Instances = Spec->GetAbilityInstances();
			if (Instances.Num() > 0)
			{
				Instance = Cast<UFAGA_Rescue>(Instances[0]);
			}
		}
	}

	const UWorld* World = ActorInfo && ActorInfo->AvatarActor.IsValid() ? ActorInfo->AvatarActor->GetWorld() : nullptr;
	return !(Instance && World && World->GetTimeSeconds() < Instance->NextRescueAllowedTime);
}

void UFAGA_Rescue::StartRescueCooldown(UAbilitySystemComponent* RescuerASC)
{
	if (!RescuerASC)
	{
		return;
	}

	for (const FGameplayAbilitySpec& Spec : RescuerASC->GetActivatableAbilities())
	{
		for (UGameplayAbility* Ability : Spec.GetAbilityInstances())
		{
			if (UFAGA_Rescue* Rescue = Cast<UFAGA_Rescue>(Ability))
			{
				if (const UWorld* World = Rescue->GetWorld())
				{
					Rescue->NextRescueAllowedTime = World->GetTimeSeconds() + Rescue->RescueCooldown;
				}
				return;
			}
		}
	}
}

void UFAGA_Rescue::ActivateAbility(
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

	AFACharacter* Rescuer = Cast<AFACharacter>(ActorInfo->AvatarActor.Get());
	UAbilitySystemComponent* RescuerASC = GetAbilitySystemComponentFromActorInfo();
	if (!Rescuer || !RescuerASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!HasAuthority(&ActivationInfo))
	{
		return;
	}

	const FAGameplayTags& Tags = FAGameplayTags::Get();

	// 내 주변 내에 잡혀있는 아군('State.Grabbed') 탐색
	float RescueRadius = 150.0f;
	AFACharacter* GrabbedAlly = FindTargetInRadius(Rescuer, RescueRadius, Tags.State_Grabbed);

	// 잡힌 아군이 없는 경우
	if (!GrabbedAlly)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	UAbilitySystemComponent* VictimASC = GrabbedAlly->GetAbilitySystemComponent();
	if (VictimASC && VictimASC->HasMatchingGameplayTag(Tags.Rule_Trade_Hide))
	{
		EndAbility(Handle,ActorInfo,ActivationInfo,true,false);
		return;
	}

	// 구출 시도가 시작됐다는 신호. 선딜레이가 있으면 이 소리가 그 대기 시간을 메운다.
	Rescuer->Multicast_PlayFASoundAtLocation(FARescueAudio::Start);

	float RescueDelay = RescuerASC->GetNumericAttribute(UFAAttributeSet::GetRescueDurationAttribute());
	if (RescueDelay > 0.0f)
	{
		// 선딜레이가 있는 경우 타이머 후 실행
		FTimerDelegate TimerDel;
		TimerDel.BindUObject(this, &UFAGA_Rescue::StartRescueContest, Rescuer, GrabbedAlly);
		Rescuer->GetWorld()->GetTimerManager().SetTimer(RescueDelayTimerHandle, TimerDel, RescueDelay, false);
	}
	else
	{
		// 딜레이 없이 즉시 줄다리기 개시
		StartRescueContest(Rescuer, GrabbedAlly);
	}
}

void UFAGA_Rescue::StartRescueContest(AFACharacter* Rescuer, AFACharacter* GrabbedAlly)
{
	if (!Rescuer || !GrabbedAlly)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	UAbilitySystemComponent* VictimASC = GrabbedAlly->GetAbilitySystemComponent();
	if (!VictimASC || VictimASC->HasMatchingGameplayTag(FAGameplayTags::Get().Rule_Trade_Hide))
	{
		EndAbility(CurrentSpecHandle,CurrentActorInfo,CurrentActivationInfo,true,false);
		return;
	}

	const FAGameplayTags& Tags = FAGameplayTags::Get();

	// 잡힌 아군의 주변에서 'State.Grabbing' 태그를 가진 좀비를 탐색
	float SearchGrabberRadius = 250.0f;
	AFACharacter* ZombieGrabber = FindTargetInRadius(GrabbedAlly, SearchGrabberRadius, Tags.State_Grabbing);

	if (!ZombieGrabber)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	UAbilitySystemComponent* ZombieASC = ZombieGrabber->GetAbilitySystemComponent();
	if (!ZombieASC)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// 좀비의 활성화된 포획 어빌리티를 찾아 감염 타이머를 멈춘다 — 줄다리기 하는 동안
	// 감염 게이지가 혼자 차면 QTE 를 이겨도 이미 늦은 상황이 나온다.
	UFAGA_Grab* GrabAbility = UFAGA_Grab::FindActiveInstance(ZombieASC);
	if (GrabAbility)
	{
		GrabAbility->PauseInfectTimer();
	}

	// 줄다리기 개시 — 세 사람(구조자 · 좀비 · 인질) 주변에 다 들려야 하므로 좀비 위치에서 낸다.
	ZombieGrabber->Multicast_PlayFASoundAtLocation(FARescueAudio::QTEStart);

	// 줄다리기(QTE)는 좀비 쪽에서 인스턴스 하나만 돈다 — ContestGauge Attribute 의 주인이 좀비다.
	// 참가자 3명은 페이로드로 넘긴다.
	FGameplayEventData Payload;
	Payload.EventTag = Tags.Ability_RescueContest;
	Payload.Instigator = Rescuer;         // 구조자 (인간)
	Payload.Target = ZombieGrabber;       // 대결 상대 (좀비)
	Payload.OptionalObject = GrabbedAlly; // 인질 (잡힌 아군)

	// 예전에는 이 페이로드를 만들어 놓고 아무 데도 보내지 않은 채, 대신 GA_Grab 을 취소하고 끝냈다.
	// 그래서 QTE 가 한 번도 열리지 않았고 좌클릭 한 번에 판정 없이 인질이 풀려났다.
	//
	// HandleGameplayEvent 대신 스펙을 직접 찾아 발동시킨다. 이벤트 경로를 쓰려면 GA_RescueContest
	// 애셋의 AbilityTriggers 에 GameplayEvent 항목이 있어야 하는데, 그건 BP 에서 빠뜨리기 쉽고
	// 빠졌을 때 아무 일도 안 일어난 채 조용히 넘어간다 — 이 어빌리티가 죽은 이유가 바로 그런 종류였다.
	bool bContestStarted = false;

	for (const FGameplayAbilitySpec& Spec : ZombieASC->GetActivatableAbilities())
	{
		// 클래스로 먼저 본다 — 태그는 BP 의 Asset Tags 칸에 의존하고, 그 칸은 비어 있어도 경고가 없다.
		const bool bIsContestAbility = Spec.Ability
			&& (Spec.Ability->IsA<UFAGA_RescueContest>()
				|| Spec.Ability->GetAssetTags().HasTagExact(Tags.Ability_RescueContest));

		if (!bIsContestAbility)
		{
			continue;
		}

		bContestStarted = ZombieASC->TriggerAbilityFromGameplayEvent(
			Spec.Handle, ZombieASC->AbilityActorInfo.Get(), Tags.Ability_RescueContest, &Payload, *ZombieASC);
		break;
	}

	if (!bContestStarted)
	{
		// 좀비가 GA_RescueContest 를 아예 지급받지 못한 경우(DefaultAbilities 누락).
		// 감염 타이머를 멈춘 채로 빠져나가면 인질이 영원히 잡힌 상태로 남으므로 반드시 되돌린다.
		if (GrabAbility)
		{
			GrabAbility->ResumeInfectTimer();
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFAGA_Rescue::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// 선딜레이 대기 중 취소되었을 경우 타이머 정리
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RescueDelayTimerHandle);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

AFACharacter* UFAGA_Rescue::FindTargetInRadius(AFACharacter* Avatar, float Radius, FGameplayTag RequiredTag)
{
	if (!Avatar) return nullptr;

	FVector Center = Avatar->GetActorLocation();
	TArray<FHitResult> HitResults;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Avatar);

	bool bHit = Avatar->GetWorld()->SweepMultiByChannel(
		HitResults, Center, Center + FVector(0.0f, 0.0f, 1.0f), FQuat::Identity, ECC_Pawn,
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
