// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Rule/FARuleTypes.h"
#include "FARuleSettings.generated.h"

class UDataTable;

/**
 * 룰 카드 DataTable 의 위치를 프로젝트 설정에 한 번만 박아두는 곳.
 * (Project Settings → Game → Falling Apart | Rule)
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Falling Apart | Rule"))
class FALLINGAPART_API UFARuleSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UFARuleSettings();

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** RowStruct 가 FFARuleCardRow 인 DataTable (DT_RuleCards). */
	UPROPERTY(Config, EditAnywhere, Category = "Rule", meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> RuleCardTable;

	/**
	 * 동시에 보유할 수 있는 룰 최대 장수. 초과하면 가장 먼저 뽑은 룰이 사라진다.
	 * (밸런싱 워크북 0_기준 · 활성 룰 상한 4장)
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Rule", meta = (ClampMin = "1"))
	int32 MaxActiveRules = 4;

	/** 드래프트 후보 선택 제한 시간(초). 이 시간이 지나면 서버가 무작위로 확정한다. */
	UPROPERTY(Config, EditAnywhere, Category = "Draft", meta = (ClampMin = "1.0"))
	float SelectionTimeLimit = 10.0f;

	/**
	 * 연속 드래프트의 선택 제한 시간(초). 정규 드래프트(SelectionTimeLimit)보다 짧게 잡는다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Draft", meta = (ClampMin = "1.0"))
	float ChainSelectionTimeLimit = 5.0f;

	/**
	 * 연속 드래프트 1장당 간격(초).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Draft", meta = (ClampMin = "0.5"))
	float ChainDraftInterval = 2.0f;

	//~ ---------------- 조회 헬퍼 ----------------

	/** 표를 동기 로드해서 돌려준다. 미지정이면 nullptr (로그 1회). */
	static const UDataTable* GetRuleCardTable();

	/** RuleId(=RowName) 로 카드 1장을 찾는다. 없으면 nullptr. */
	static const FFARuleCardRow* FindRuleCard(FName RuleId);

	static int32 GetMaxActiveRules();

	/** 드래프트 후보 선택 제한 시간(초). */
	static float GetSelectionTimeLimit();

	/** 연속 드래프트의 선택 제한 시간(초). */
	static float GetChainSelectionTimeLimit();

	/** 연속 드래프트 1장당 간격(초). */
	static float GetChainDraftInterval();

	//~ ---------------- 표시 정보 (UI 공용) ----------------

	/**
	 * RuleId 로 UI 표시 사본을 만든다. DT 에 없으면 이름 자리에 RuleId 를 넣고 bFoundInTable = false.
	 */
	static FFARuleDisplayInfo MakeRuleDisplayInfo(FName RuleId);

	/**
	 * 계열 판정. 거래(T)·도박(G) 은 태그 계열보다 우선한다
	 */
	static EFARuleCategory GetRuleCategory(const FGameplayTag& RuleTag, EFARuleType Type);

	/** 계열 색. */
	static FLinearColor GetRuleCategoryColor(EFARuleCategory Category);

	/**
	 * 도박 카드용 무지개 색. 시간(초)을 넣으면 색상환을 한 바퀴 돈다.
	 * @param TimeSeconds 월드 시간 등 단조 증가하는 값
	 */
	static FLinearColor GetGambleColor(float TimeSeconds);
};
