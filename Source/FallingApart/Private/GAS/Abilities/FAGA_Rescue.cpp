// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Abilities/FAGA_Rescue.h"
#include "GAS/Abilities/FAGA_Grab.h"
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
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

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

	const FAGameplayTags& Tags = FAGameplayTags::Get();

	// 잡힌 아군의 주변에서 'State.Grabbing' 태그를 가진 좀비를 탐색
	float SearchGrabberRadius = 250.0f;
	AFACharacter* ZombieGrabber = FindTargetInRadius(GrabbedAlly, SearchGrabberRadius, Tags.State_Grabbing);

	if (!ZombieGrabber)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// 좀비의 활성화된 UFAGA_Grab 어빌리티 인스턴스를 찾아 감염 타이머를 일시정지시킴
	UAbilitySystemComponent* ZombieASC = ZombieGrabber->GetAbilitySystemComponent();
	if (ZombieASC)
	{
		TArray<FGameplayAbilitySpec> ActivatableAbilities = ZombieASC->GetActivatableAbilities();
		for (const FGameplayAbilitySpec& Spec : ActivatableAbilities)
		{
			if (Spec.IsActive())
			{
				for (UGameplayAbility* ActiveAbilityInstance : Spec.GetAbilityInstances())
				{
					if (UFAGA_Grab* GrabAbility = Cast<UFAGA_Grab>(ActiveAbilityInstance))
					{
						GrabAbility->PauseInfectTimer();
						break;
					}
				}
			}
		}
	}

	// 줄다리기 개시 — 세 사람(구조자 · 좀비 · 인질) 주변에 다 들려야 하므로 좀비 위치에서 낸다.
	ZombieGrabber->Multicast_PlayFASoundAtLocation(FARescueAudio::QTEStart);

	// 줄다리기(QTE) 어빌리티 발동을 위한 GameplayEvent 데이터 세팅
	FGameplayEventData Payload;
	Payload.Instigator = Rescuer;         // 구조자 (인간)
	Payload.Target = ZombieGrabber;       // 대결 상대 (좀비)
	Payload.OptionalObject = GrabbedAlly; // 인질 (잡힌 아군)

	// 구조자와 좀비 양측의 줄다리기 어빌리티(Ability.RescueContest) 트리거
	UAbilitySystemComponent* RescuerASC = GetAbilitySystemComponentFromActorInfo();
	if (ZombieASC)
	{
		FGameplayTagContainer GrabTagContainer(Tags.Ability_Grab);
		ZombieASC->CancelAbilities(&GrabTagContainer);
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
