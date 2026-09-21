#include "Animation/FAAnimStatusPolicy.h"

#include "Animation/FAAnimInstance.h"
#include "GAS/FAGameplayTags.h"
#include "NativeGameplayTags.h"

namespace
{
	// GE_Rule_Seizure_Stun의 수명에 묶인 표현용 태그다. 카드 보유와 실제 발동을 구분한다.
	// Mixed 복제에서 원격 플레이어도 받는 태그를 사용하며, 효과 목록이나 로컬 타이머를 조회하지 않는다.
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Animation_State_Seizure, "Animation.State.Seizure");

	struct FStatusRule
	{
		EFAAnimStatusOverride Status;
		FGameplayTag RequiredState;
		FGameplayTag RequiredDetail;

		bool Matches(const FGameplayTagContainer& ActiveTags) const
		{
			return ActiveTags.HasTag(RequiredState)
				&& (!RequiredDetail.IsValid() || ActiveTags.HasTag(RequiredDetail));
		}
	};

	EFAAnimStatusOverride ResolveContestPose(const FGameplayTagContainer& ActiveTags)
	{
		const FAGameplayTags& Tags = FAGameplayTags::Get();
		const bool bHuman = ActiveTags.HasTag(Tags.State_Human);
		const bool bZombie = ActiveTags.HasTag(Tags.State_Zombie);
		if (bHuman != bZombie)
		{
			return bHuman ? EFAAnimStatusOverride::HumanContesting : EFAAnimStatusOverride::ZombieContesting;
		}

		// 진영 정보가 없거나 서로 충돌하는 동안에도 QTE 자세를 유지한다.
		// AnimBP의 Contesting 입력에는 기본 QTE 포즈가 연결되어 있어야 한다.
		return EFAAnimStatusOverride::Contesting;
	}
}

EFAAnimStatusOverride FAAnimStatusPolicy::Resolve(const FGameplayTagContainer& ActiveTags)
{
	const FAGameplayTags& Tags = FAGameplayTags::Get();

	// 위에서부터 우선한다. enum 번호, 태그 도착 순서, 하위 상태의 개수와 무관하다.
	// 포획 피해자는 QTE 중에도 붙잡힌 자세를, 나머지 참가자는 QTE 자세를 유지한다.
	const FStatusRule Rules[] = {
		{EFAAnimStatusOverride::Grabbed, Tags.State_Grabbed, {}},
		{EFAAnimStatusOverride::Contesting, Tags.State_Contesting, {}},
		{EFAAnimStatusOverride::Grabbing, Tags.State_Grabbing, {}},
		{EFAAnimStatusOverride::Seizure, Tags.State_Stunned, TAG_Animation_State_Seizure},
		{EFAAnimStatusOverride::Stunned, Tags.State_Stunned, {}},
	};

	for (const FStatusRule& Rule : Rules)
	{
		if (Rule.Matches(ActiveTags))
		{
			return Rule.Status == EFAAnimStatusOverride::Contesting
				? ResolveContestPose(ActiveTags) : Rule.Status;
		}
	}

	return EFAAnimStatusOverride::None;
}
