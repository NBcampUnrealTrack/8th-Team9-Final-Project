// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Abilities/FAGA_Jump.h"

#include "GAS/FAGameplayTags.h"
#include "Player/FACharacter.h"

UFAGA_Jump::UFAGA_Jump()
{
	const FAGameplayTags& Tags = FAGameplayTags::Get();

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(Tags.Ability_Jump);
	SetAssetTags(AssetTags);

	//  점프를 막는 모든 사유를 여기 한 곳에 모은다.
	//  Z01/H01 점프 불가 · 경련/착지 경직 · 구출 QTE 중 · 포획당한 상태.
	ActivationBlockedTags.AddTag(Tags.Rule_Move_NoJump);
	ActivationBlockedTags.AddTag(Tags.State_Stunned);
	ActivationBlockedTags.AddTag(Tags.State_Contesting);
	ActivationBlockedTags.AddTag(Tags.State_Grabbed);
}

bool UFAGA_Jump::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// 공중에서 또 누르는 경우 등 캐릭터 자체의 점프 조건도 확인
	const ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	return Character && Character->CanJump();
}

void UFAGA_Jump::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	if (ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get()))
	{
		Character->Jump();
	}

	// bPressedJump 는 캐릭터가 키를 뗄 때까지 유지
	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}
