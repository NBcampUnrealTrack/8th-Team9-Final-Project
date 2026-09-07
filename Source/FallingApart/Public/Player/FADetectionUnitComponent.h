// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "FADetectionUnitComponent.generated.h"

class AFACharacter;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class FALLINGAPART_API UFADetectionUnitComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UFADetectionUnitComponent();
	/**
	  * 지정한 팀 태그를 가진 캐릭터 중
	  * Radius 이내에 있는 모든 캐릭터를 반환한다.
	  *
	  * 소유 캐릭터 자신은 항상 제외된다.
	  *
	  * @param TeamTag
	  * 탐색 대상이 보유해야 하는 태그.
	  * 예: Rule.Team.Zombie
	  *
	  * @param Radius
	  * 탐색 반경. 언리얼 단위 기준:
	  * 300 = 3m
	  * 1500 = 15m
	  *
	  * @param OutCharacters
	  * 탐색 결과가 저장되는 배열.
	  */
	void GetCharactersInRange(
		 const FGameplayTag& TeamTag,
		 float Radius,
		 TArray<AFACharacter*>& OutCharacters) const;
	
	/**
	   * 지정한 팀 태그를 가진 캐릭터가
	   * 범위 안에 한 명이라도 있는지 확인한다.
	   *
	   * 대상 목록이 필요 없는 근접 룰에서 사용한다.
	   */
	bool HasCharacterInRange(
		const FGameplayTag& TeamTag,
		float Radius) const;

private:
	/** 이 컴포넌트를 소유한 AFACharacter를 반환한다. */
	AFACharacter* GetOwnerCharacter() const;

		
};
