// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "FAAbilitySystemComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFAOnActiveRulesChanged);

/** 서버가 들고 있는 '적용 중인 룰 1장'의 실체. 클라이언트에는 복제되지 않는다. */
USTRUCT()
struct FFAActiveRule
{
	GENERATED_BODY()

	/** 이 룰을 걸고 있는 GE 핸들. 제거할 때 필요. */
	FActiveGameplayEffectHandle Handle;

	/** 이 룰이 붙을 때 함께 부여한 진영 태그. 감염 시 진영 단위로 걷어내는 기준. */
	FGameplayTag TeamTag;
};

/**
 * 룰 카드의 부착·제거를 책임지는 ASC.
 *
 * 룰 조작(Apply/Remove)은 전부 서버 권한
 * 클라이언트는 ASC 의 태그·Attribute 복제로 결과만
 */
UCLASS(ClassGroup = (FallingApart), meta = (BlueprintSpawnableComponent))
class FALLINGAPART_API UFAAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UFAAbilitySystemComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 보유 룰 목록이 바뀔 때마다 (서버·클라 양쪽에서) 브로드캐스트. HUD 룰 슬롯 갱신용. */
	UPROPERTY(BlueprintAssignable, Category = "FallingApart|Rule")
	FFAOnActiveRulesChanged OnActiveRulesChanged;

	//~ ---------------- 서버 전용 ----------------

	/**
	 * 룰 카드 1장을 적용. 드래프트 확정(UFARuleDraftSubsystem::ApplyDraftResult)이 호출.
	 *
	 * 상한(기본 4장)을 넘기면 가장 먼저 뽑았던 룰이 자동으로 빠진다.
	 *
	 * @return 실제로 적용됐으면 true. 이미 보유 중이거나 DT/GE 가 비어 있으면 false.
	 */
	bool ApplyRuleCard(FName RuleId);

	/** 룰 1장을 이름으로 제거. */
	bool RemoveRuleCard(FName RuleId);

	/**
	 * 특정 진영 태그가 붙은 룰을 전부 제거
	 * 감염 시 Rule.Team.Human 을 넘겨 인간 룰을 걷어내고, 그 장수만큼 좀비 카드를 연속 드래프트
	 */
	int32 RemoveRulesOfTeam(const FGameplayTag& TeamTag);

	/** 라운드 종료·데스매치 진입 시 전부 초기화. */
	void RemoveAllRules();

	//~ ---------------- 조회 (서버·클라 공통) ----------------

	/** 보유 순서대로 정렬된 룰 ID 목록. HUD 가 이 순서로 카드를 그린다. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Rule")
	const TArray<FName>& GetActiveRuleIds() const { return ActiveRuleIds; }

	UFUNCTION(BlueprintPure, Category = "FallingApart|Rule")
	int32 GetActiveRuleCount() const { return ActiveRuleIds.Num(); }

	/** 룰 태그(Rule.Move.NoJump 등) 보유 여부. 부모 태그로 물어도 자식 태그가 걸린다. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Rule")
	bool HasRuleTag(FGameplayTag RuleTag) const { return HasMatchingGameplayTag(RuleTag); }

protected:
	UFUNCTION()
	void OnRep_ActiveRuleIds();

private:
	/** 서버 전용 실체 표. RuleId → GE 핸들 + 진영 태그. */
	TMap<FName, FFAActiveRule> ActiveRules;

	UPROPERTY(ReplicatedUsing = OnRep_ActiveRuleIds)
	TArray<FName> ActiveRuleIds;
};
