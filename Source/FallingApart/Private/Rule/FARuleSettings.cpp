// Fill out your copyright notice in the Description page of Project Settings.

#include "Rule/FARuleSettings.h"

#include "AbilitySystemInterface.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GAS/FAAbilitySystemComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
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

FFARuleDisplayInfo UFARuleSettings::MakeRuleDisplayInfo(FName RuleId)
{
	FFARuleDisplayInfo Info;
	Info.RuleId = RuleId;

	const FFARuleCardRow* Row = FindRuleCard(RuleId);
	if (!Row)
	{
		// DT 에 없는 ID(더미 풀 폴백 등). 빈 칸으로 두면 무엇이 걸렸는지 알 길이 없으므로 ID 를 그대로 보여준다.
		Info.DisplayName = FText::FromName(RuleId);
		Info.CategoryColor = GetRuleCategoryColor(EFARuleCategory::None);
		return Info;
	}

	Info.DisplayName = Row->DisplayName.IsEmpty() ? FText::FromName(RuleId) : Row->DisplayName;
	Info.ShortDesc = Row->ShortDesc;
	Info.Icon = Row->Icon;
	Info.BackgroundImage = Row->BackgroundImage;
	Info.Team = Row->Team;
	Info.Tier = Row->Tier;
	Info.Type = Row->Type;
	Info.RuleTag = Row->RuleTag;
	Info.Category = GetRuleCategory(Row->RuleTag, Row->Type);
	Info.CategoryColor = GetRuleCategoryColor(Info.Category);
	Info.bAnimatedColor = (Info.Category == EFARuleCategory::Gamble);
	Info.bFoundInTable = true;

	return Info;
}

EFARuleCategory UFARuleSettings::GetRuleCategory(const FGameplayTag& RuleTag, EFARuleType Type)
{
	// 이득이 있는 카드인지가 계열보다 먼저 읽혀야 한다 — 색을 먼저 가져간다.
	if (Type == EFARuleType::Gamble)
	{
		return EFARuleCategory::Gamble;
	}

	if (Type == EFARuleType::Trade)
	{
		return EFARuleCategory::Trade;
	}

	if (!RuleTag.IsValid())
	{
		return EFARuleCategory::None;
	}

	// 태그 계열은 문자열로 판정한다.
	const FString TagName = RuleTag.ToString();

	static const TCHAR* MovePrefixes[] = { TEXT("Rule.Move."), TEXT("Rule.Input.") };
	for (const TCHAR* Prefix : MovePrefixes)
	{
		if (TagName.StartsWith(Prefix))
		{
			return EFARuleCategory::Move;
		}
	}

	if (TagName.StartsWith(TEXT("Rule.Vision.")))
	{
		return EFARuleCategory::Vision;
	}

	// H36~H38처럼 Rule.State.* 태그를 사용하는 상태 제한 카드.
	if (TagName.StartsWith(TEXT("Rule.State.")))
	{
		return EFARuleCategory::State;
	}

	static const TCHAR* AudioPrefixes[] = { TEXT("Rule.Audio."), TEXT("Rule.Social.") };
	for (const TCHAR* Prefix : AudioPrefixes)
	{
		if (TagName.StartsWith(Prefix))
		{
			return EFARuleCategory::Audio;
		}
	}

	static const TCHAR* RescuePrefixes[] = { TEXT("Rule.Rescue."), TEXT("Rule.Bite.") };
	for (const TCHAR* Prefix : RescuePrefixes)
	{
		if (TagName.StartsWith(Prefix))
		{
			return EFARuleCategory::Rescue;
		}
	}

	return EFARuleCategory::None;
}

FLinearColor UFARuleSettings::GetRuleCategoryColor(EFARuleCategory Category)
{
	// 어두운 화면 위에 얹히는 색이라 채도를 높게 잡는다. 아이콘이 없는 칸은 이 색만으로 계열을 읽는다.
	switch (Category)
	{
	case EFARuleCategory::Move:		return FLinearColor(0.90f, 0.18f, 0.16f, 1.0f);	// 빨강
	case EFARuleCategory::Vision:	return FLinearColor(0.16f, 0.45f, 0.95f, 1.0f);	// 파랑
	case EFARuleCategory::Audio:	return FLinearColor(0.62f, 0.28f, 0.90f, 1.0f);	// 보라
	case EFARuleCategory::Rescue:	return FLinearColor(0.16f, 0.75f, 0.35f, 1.0f);	// 초록
	case EFARuleCategory::Trade:	return FLinearColor(0.95f, 0.78f, 0.15f, 1.0f);	// 노랑
	case EFARuleCategory::Gamble:	return FLinearColor(0.85f, 0.85f, 0.85f, 1.0f);	// 무지개의 기준색
	case EFARuleCategory::State:	return FLinearColor(1.00f, 0.48f, 0.10f, 1.0f);	// 주황
	default:						return FLinearColor(0.55f, 0.55f, 0.58f, 1.0f);	// 계열 미상 — 회색
	}
}

FLinearColor UFARuleSettings::GetGambleColor(float TimeSeconds)
{
	// 2초에 한 바퀴. 이보다 빠르면 눈이 아프고, 느리면 '점멸'로 안 읽힌다.
	const float Hue = FMath::Fmod(TimeSeconds * 180.0f, 360.0f);
	return FLinearColor(Hue, 0.85f, 1.0f).HSVToLinearRGB();
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

	//~ ---------------- 표시 확인용 ----------------
	//~ 드래프트는 30초 주기라 HUD 4칸·머리 위 아이콘을 보려면 몇 분을 기다려야 한다.

	/** 대상 ASC 를 모은다. bAll 이면 접속 중인 전원, 아니면 이 창의 로컬 폰 하나. */
	static TArray<UFAAbilitySystemComponent*> GatherTargetASCs(UWorld* World, bool bAll)
	{
		TArray<UFAAbilitySystemComponent*> Result;
		if (!World)
		{
			return Result;
		}

		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* Controller = It->Get();
			if (!Controller || (!bAll && !Controller->IsLocalController()))
			{
				continue;
			}

			const IAbilitySystemInterface* AsInterface = Cast<IAbilitySystemInterface>(Controller->GetPawn());
			if (!AsInterface)
			{
				continue;
			}

			if (UFAAbilitySystemComponent* ASC = Cast<UFAAbilitySystemComponent>(AsInterface->GetAbilitySystemComponent()))
			{
				Result.Add(ASC);
			}
		}

		return Result;
	}

	static bool WarnIfNotAuthority(const UWorld* World)
	{
		if (World && World->GetNetMode() != NM_Client)
		{
			return false;
		}

		UE_LOG(LogTemp, Warning,
			TEXT("FARule: 룰 적용은 서버 권한이다 — 호스트(리슨 서버) 창의 콘솔에서 실행할 것."));
		return true;
	}

	static void GiveRule(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("FARule.Give <RuleId> [all] — 예: FARule.Give Z01"));
			return;
		}

		if (WarnIfNotAuthority(World))
		{
			return;
		}

		const FName RuleId(*Args[0]);
		const bool bAll = Args.Num() > 1 && Args[1].Equals(TEXT("all"), ESearchCase::IgnoreCase);

		if (!UFARuleSettings::FindRuleCard(RuleId))
		{
			UE_LOG(LogTemp, Warning, TEXT("FARule.Give: DT 에 없는 카드 %s"), *RuleId.ToString());
			return;
		}

		int32 Applied = 0;
		for (UFAAbilitySystemComponent* ASC : GatherTargetASCs(World, bAll))
		{
			Applied += ASC->ApplyRuleCard(RuleId) ? 1 : 0;
		}

		UE_LOG(LogTemp, Display, TEXT("FARule.Give: %s 를 %d명에게 적용"), *RuleId.ToString(), Applied);
	}

	static void ClearRules(const TArray<FString>& Args, UWorld* World)
	{
		if (WarnIfNotAuthority(World))
		{
			return;
		}

		const bool bAll = Args.Num() > 0 && Args[0].Equals(TEXT("all"), ESearchCase::IgnoreCase);

		int32 Cleared = 0;
		for (UFAAbilitySystemComponent* ASC : GatherTargetASCs(World, bAll))
		{
			ASC->RemoveAllRules();
			++Cleared;
		}

		UE_LOG(LogTemp, Display, TEXT("FARule.Clear: %d명의 룰을 전부 제거"), Cleared);
	}

	static FAutoConsoleCommandWithWorldAndArgs GiveRuleCommand(
		TEXT("FARule.Give"),
		TEXT("룰 카드 1장을 즉시 적용한다. FARule.Give <RuleId> [all] (서버 창 전용)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&GiveRule));

	static FAutoConsoleCommandWithWorldAndArgs ClearRulesCommand(
		TEXT("FARule.Clear"),
		TEXT("보유 룰을 전부 제거한다. FARule.Clear [all] (서버 창 전용)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ClearRules));
}
