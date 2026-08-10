// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "FAGameplayAbility.generated.h"

class AFACharacter;
class UFAAbilitySystemComponent;

/**
 * 프로젝트 어빌리티 공통 베이스.
 *
 * 인스턴싱·네트워크 정책의 기본값을 여기서 한 번 정해두고, 파생 클래스는 태그만 신경 쓰게 한다.
 * 정책을 바꿔야 하는 어빌리티(포획·QTE 는 서버 권한, 설계문서 §3.4)는 자기 생성자에서 덮어쓸 것.
 */
UCLASS(Abstract)
class FALLINGAPART_API UFAGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UFAGameplayAbility();

protected:
	/** 아바타 캐릭터. 관전 폰 등 캐릭터가 아닌 아바타에서는 nullptr. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Ability")
	AFACharacter* GetFACharacter() const;

	UFUNCTION(BlueprintPure, Category = "FallingApart|Ability")
	UFAAbilitySystemComponent* GetFAAbilitySystemComponent() const;
};
