// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/FADetectionUnitComponent.h"

#include "EngineUtils.h"
#include "GAS/FAAbilitySystemComponent.h"
#include "Player/FACharacter.h"

UFADetectionUnitComponent::UFADetectionUnitComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);

}

void UFADetectionUnitComponent::GetCharactersInRange(const FGameplayTag& TeamTag, float Radius,
	TArray<AFACharacter*>& OutCharacters) const
{
	// 호출자가 이전 호출에서 받은 결과를 제거한다.
	OutCharacters.Reset();

	AFACharacter* OwnerCharacter =GetOwnerCharacter();

	UWorld* World = GetWorld();

	if (!OwnerCharacter ||!World ||!TeamTag.IsValid() ||Radius <= 0.0f)
	{
		return;
	}

	const FVector OwnerLocation =OwnerCharacter->GetActorLocation();

	/**
	 * 실제 거리를 구하기 위한 제곱근 계산을 피하기 위해
	 * 거리와 탐색 반경을 모두 제곱 값으로 비교한다.
	 */
	const float RadiusSquared =FMath::Square(Radius);

	for (TActorIterator<AFACharacter> It(World);It;++It)
	{
		AFACharacter* Candidate = *It;

		/**
		 * 유효하지 않은 캐릭터와 자기 자신은 제외한다.
		 *
		 * 자신을 제외하지 않으면
		 * "다른 좀비가 3m 이내" 같은 룰이 항상 발동할 수 있다.
		 */
		if (!IsValid(Candidate) ||Candidate == OwnerCharacter)
		{
			continue;
		}

		UFAAbilitySystemComponent* CandidateASC =Candidate->GetFAAbilitySystemComponent();

		// 대상의 ASC가 아직 준비되지 않았다면 제외한다.
		if (!CandidateASC)
		{
			continue;
		}

		/**
		 * 요청받은 팀 태그를 가진 캐릭터만 탐색한다.
		 *
		 * H41에서는 Rule.Team.Zombie를 전달한다.
		 */
		if (!CandidateASC->HasMatchingGameplayTag(TeamTag))
		{
			continue;
		}

		const float DistanceSquared =FVector::DistSquared(OwnerLocation,Candidate->GetActorLocation());

		if (DistanceSquared <= RadiusSquared)
		{
			OutCharacters.Add(Candidate);
		}
	}
}

bool UFADetectionUnitComponent::HasCharacterInRange(const FGameplayTag& TeamTag, float Radius) const
{
	TArray<AFACharacter*> Characters;

	GetCharactersInRange(TeamTag,Radius,Characters);

	return !Characters.IsEmpty();
}

AFACharacter* UFADetectionUnitComponent::GetOwnerCharacter() const
{
	return Cast<AFACharacter>(GetOwner());
}
