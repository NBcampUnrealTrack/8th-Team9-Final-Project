// Fill out your copyright notice in the Description page of Project Settings.

#include "Rule/FARuleSettings.h"

#include "Engine/DataTable.h"
#include "GameplayEffect.h"
#include "HAL/IConsoleManager.h"
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

float UFARuleSettings::GetChainSelectionTimeLimit()
{
	const UFARuleSettings* Settings = GetDefault<UFARuleSettings>();
	return Settings ? FMath::Max(1.0f, Settings->ChainSelectionTimeLimit) : 5.0f;
}

float UFARuleSettings::GetChainDraftInterval()
{
	const UFARuleSettings* Settings = GetDefault<UFARuleSettings>();
	return Settings ? FMath::Max(0.5f, Settings->ChainDraftInterval) : 2.0f;
}

namespace FARuleDebug
{
	/**
	 * 표에 있는 모든 룰 카드의 GE 를 한 번에 검사한다.
	 *
	 * ApplyRuleCard 에도 같은 검사가 있지만 그건 '뽑힌 카드'만 걸린다 — 무작위 드래프트라
	 * 전수 확인을 하려면 몇 판을 돌려야 할지 모른다. 이 명령은 애셋을 전부 로드해서 한 번에 답을 준다.
	 */
	static void AuditRuleEffects()
	{
		const UDataTable* Table = UFARuleSettings::GetRuleCardTable();
		if (Table == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("FARule.Audit: 룰 카드 DataTable 이 지정되지 않았다."));
			return;
		}

		UE_LOG(LogTemp, Display, TEXT("========== 룰 카드 GE 감사 =========="));

		int32 Total = 0;
		int32 MissingAsset = 0;
		int32 WrongDuration = 0;
		int32 MissingTag = 0;

		for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
		{
			const FFARuleCardRow* Row = reinterpret_cast<const FFARuleCardRow*>(Pair.Value);
			if (Row == nullptr)
			{
				continue;
			}

			++Total;

			UClass* EffectClass = Row->RuleEffect.IsNull() ? nullptr : Row->RuleEffect.LoadSynchronous();
			if (EffectClass == nullptr)
			{
				++MissingAsset;
				UE_LOG(LogTemp, Warning, TEXT("  %-8s GE 미제작 (표에 행만 있음)"), *Pair.Key.ToString());
				continue;
			}

			const UGameplayEffect* EffectCDO = EffectClass->GetDefaultObject<UGameplayEffect>();
			if (EffectCDO == nullptr)
			{
				continue;
			}

			// 룰 GE 는 Infinite 여야 한다. Instant 는 Base Value 를 영구히 바꿔서
			// 감염 시 룰을 제거해도 디버프가 안 풀린다.
			if (EffectCDO->DurationPolicy != EGameplayEffectDurationType::Infinite)
			{
				++WrongDuration;
				UE_LOG(LogTemp, Error, TEXT("  %-8s %-40s Duration Policy = %s   <<< Infinite 로 고칠 것"),
					*Pair.Key.ToString(), *GetNameSafe(EffectClass),
					EffectCDO->DurationPolicy == EGameplayEffectDurationType::Instant ? TEXT("Instant") : TEXT("HasDuration"));
			}

			// 태그를 안 주는 GE 는 CMC·어빌리티가 못 읽어서 "붙었는데 아무 일도 안 일어난다"가 된다.
			if (Row->RuleTag.IsValid() && !EffectCDO->GetGrantedTags().HasTagExact(Row->RuleTag))
			{
				++MissingTag;
				UE_LOG(LogTemp, Warning, TEXT("  %-8s %-40s 태그 %s 를 부여하지 않는다"),
					*Pair.Key.ToString(), *GetNameSafe(EffectClass), *Row->RuleTag.ToString());
			}
		}

		UE_LOG(LogTemp, Display, TEXT("총 %d장 / GE 미제작 %d · Duration 잘못 %d · 태그 누락 %d"),
			Total, MissingAsset, WrongDuration, MissingTag);
		UE_LOG(LogTemp, Display, TEXT("===================================="));
	}

	static FAutoConsoleCommand AuditRuleEffectsCommand(
		TEXT("FARule.Audit"),
		TEXT("모든 룰 카드의 GE 를 검사한다 (Duration Policy · 태그 부여 · 애셋 유무)."),
		FConsoleCommandDelegate::CreateStatic(&AuditRuleEffects));
}
