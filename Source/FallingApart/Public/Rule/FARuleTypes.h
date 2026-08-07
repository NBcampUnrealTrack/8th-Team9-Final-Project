// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Game/FAGameTypes.h"
#include "FARuleTypes.generated.h"

class UGameplayEffect;
class UTexture2D;

/** 룰 티어. 드래프트 회차별 구성표(밸런싱 워크북 1_드래프트곡선)가 이 값으로 후보를 고른다. */
UENUM(BlueprintType)
enum class EFARuleTier : uint8
{
	None	= 0,
	/** T1 — 가벼운 페널티. 1~3회차 주력 */
	T1		= 1,
	/** T2 — 중간 페널티. 3~5회차 주력 */
	T2		= 2,
	/** T3 — 무거운 페널티. 6회차 '무법 라운드' */
	T3		= 3
};

/** 룰 성격. 도감 워크북의 '형' 열(P/T/G)과 1:1 대응. */
UENUM(BlueprintType)
enum class EFARuleType : uint8
{
	/** P — 순수 페널티 */
	Penalty		= 0,
	/** T — 거래 (장점 + 대가) */
	Trade		= 1,
	/** G — 도박 (무작위 결과) */
	Gamble		= 2
};

USTRUCT(BlueprintType)
struct FFARuleCardRow : public FTableRowBase
{
	GENERATED_BODY()

	//~ ---------------- 표시 ----------------

	/** 카드 제목. 8자 이내 (카드텍스트·UX 가이드) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "표시")
	FText DisplayName;

	/** 한 줄 설명. 드래프트 카드에 그대로 찍힌다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "표시")
	FText ShortDesc;

	/** 카드 아이콘. 비어 있으면 티어 기본 아이콘으로 대체. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "표시")
	TSoftObjectPtr<UTexture2D> Icon;

	//~ ---------------- 분류 ----------------

	/** 이 카드를 뽑을 수 있는 진영. 드래프트 후보 1차 필터. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "분류")
	EFATeam Team = EFATeam::None;

	/** 티어. 드래프트 후보 2차 필터. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "분류")
	EFARuleTier Tier = EFARuleTier::T1;

	/** 페널티 / 거래 / 도박 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "분류")
	EFARuleType Type = EFARuleType::Penalty;

	//~ ---------------- GAS ----------------

	/**
	 * 이 카드의 정체성 태그 (Rule.Move.NoJump 등).
	 * CMC·PostProcess·어빌리티가 "이 태그가 있는가"로만 룰을 판정한다.
	 * GE 애셋의 GrantedTags 에도 같은 태그가 들어 있어야 한다 — 여기 값은 조회·중복검사용 사본이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS")
	FGameplayTag RuleTag;

	/** 적용할 GameplayEffect. 대부분 Infinite 지속 1개로 끝난다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS")
	TSoftClassPtr<UGameplayEffect> RuleEffect;

	/**
	 * 동시 보유 불가 태그.
	 * 이 컨테이너의 태그를 이미 하나라도 들고 있으면 드래프트 후보에서 제외된다.
	 * (예: 자동 전진. 후진만 가능)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS")
	FGameplayTagContainer ConflictTags;

	/**
	 * GE 의 SetByCaller(Data.Rule.Magnitude) 로 주입되는 수치.
	 * 곱연산 Modifier 는 배수를 넣는다 — 이동속도 -15% = 0.85, 마찰 -70% = 0.3.
	 * 이 룰이 수치를 안 쓰면 0 으로 두면 된다 (GE 가 참조하지 않으면 무시된다).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS")
	float Magnitude = 0.0f;

	//~ ---------------- 드래프트 ----------------

	/** 같은 티어 안에서의 가중 랜덤 가중치. 0 이하면 후보에서 완전히 빠진다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "드래프트")
	float DraftWeight = 1.0f;

	/** MVP 20종 포함 여부. 초기 테스트에서 이 플래그로 풀을 좁힌다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "드래프트")
	bool bMVP = false;

	/** 후보로 뽑힐 자격이 있는가 — 미구현 카드(RuleEffect 비어 있음)는 자동 제외. */
	bool IsDraftable() const
	{
		return DraftWeight > 0.0f && !RuleEffect.IsNull();
	}
};
