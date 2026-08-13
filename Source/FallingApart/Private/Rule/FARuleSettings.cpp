// Fill out your copyright notice in the Description page of Project Settings.

#include "Rule/FARuleSettings.h"

#include "Engine/DataTable.h"
#include "Rule/FARuleTypes.h"

UFARuleSettings::UFARuleSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("Falling Apart | Rule");
}

const UDataTable* UFARuleSettings::GetRuleCardTable()
{
	const UFARuleSettings* Settings = GetDefault<UFARuleSettings>();
	if (!Settings || Settings->RuleCardTable.IsNull())
	{
		// 표를 아직 안 만들었거나 프로젝트 설정에 안 물린 상태. 드래프트는 더미 풀로 폴백한다.
		static bool bWarnedOnce = false;
		if (!bWarnedOnce)
		{
			bWarnedOnce = true;
			UE_LOG(LogTemp, Warning,
				TEXT("[Rule] 룰 카드 DataTable 이 지정되지 않았다. Project Settings → Game → Falling Apart | Rule 에서 DT_RuleCards 를 지정할 것."));
		}
		return nullptr;
	}

	// 표 1개짜리 동기 로드. 룰 조회는 드래프트 시점(30초 주기)에만 일어나므로 히칭 부담이 없다.
	return Settings->RuleCardTable.LoadSynchronous();
}

const FFARuleCardRow* UFARuleSettings::FindRuleCard(FName RuleId)
{
	const UDataTable* Table = GetRuleCardTable();
	if (!Table || RuleId.IsNone())
	{
		return nullptr;
	}

	// bWarnIfRowMissing = false — 더미 룰 ID 로 호출되는 경우가 있어 로그를 직접 남긴다.
	return Table->FindRow<FFARuleCardRow>(RuleId, TEXT("FAFindRuleCard"), /*bWarnIfRowMissing=*/false);
}

int32 UFARuleSettings::GetMaxActiveRules()
{
	const UFARuleSettings* Settings = GetDefault<UFARuleSettings>();
	return Settings ? FMath::Max(1, Settings->MaxActiveRules) : 4;
}

float UFARuleSettings::GetSelectionTimeLimit()
{
	const UFARuleSettings* Settings = GetDefault<UFARuleSettings>();
	return Settings ? FMath::Max(1.0f, Settings->SelectionTimeLimit) : 10.0f;
}

float UFARuleSettings::GetChainDraftInterval()
{
	const UFARuleSettings* Settings = GetDefault<UFARuleSettings>();
	return Settings ? FMath::Max(0.5f, Settings->ChainDraftInterval) : 6.0f;
}
