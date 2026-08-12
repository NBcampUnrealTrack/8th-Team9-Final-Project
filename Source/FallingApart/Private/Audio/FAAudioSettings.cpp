// Fill out your copyright notice in the Description page of Project Settings.

#include "Audio/FAAudioSettings.h"

#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Sound/SoundAttenuation.h"

UFAAudioSettings::UFAAudioSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("Falling Apart | Audio");

	// 룰 계열 9종. 태그는 `Rule.<계열>.<이름>` 꼴이라 앞 두 마디가 계열을 정한다.
	// Move/Input 과 Rescue/Group 은 같은 소리를 쓴다 — 플레이어가 구분할 이유가 없는 짝이다.
	RuleSeriesSoundIds.Add(TEXT("Rule.Move"), TEXT("Rule_Apply_Move"));
	RuleSeriesSoundIds.Add(TEXT("Rule.Input"), TEXT("Rule_Apply_Move"));
	RuleSeriesSoundIds.Add(TEXT("Rule.Vision"), TEXT("Rule_Apply_Vision"));
	RuleSeriesSoundIds.Add(TEXT("Rule.Audio"), TEXT("Rule_Apply_Audio"));
	RuleSeriesSoundIds.Add(TEXT("Rule.Social"), TEXT("Rule_Apply_Social"));
	RuleSeriesSoundIds.Add(TEXT("Rule.Bite"), TEXT("Rule_Apply_Bite"));
	RuleSeriesSoundIds.Add(TEXT("Rule.Rescue"), TEXT("Rule_Apply_Rescue"));
	RuleSeriesSoundIds.Add(TEXT("Rule.Group"), TEXT("Rule_Apply_Rescue"));
	RuleSeriesSoundIds.Add(TEXT("Rule.State"), TEXT("Rule_Apply_State"));
	RuleSeriesSoundIds.Add(TEXT("Rule.Trade"), TEXT("Rule_Apply_Trade"));
	RuleSeriesSoundIds.Add(TEXT("Rule.Gamble"), TEXT("Rule_Apply_Gamble"));
}

const UFAAudioSettings* UFAAudioSettings::Get()
{
	return GetDefault<UFAAudioSettings>();
}

void UFAAudioSettings::GatherTables(TArray<const UDataTable*>& OutTables)
{
	OutTables.Reset();

	const UFAAudioSettings* Settings = Get();
	if (!Settings)
	{
		return;
	}

	// 표 자체는 작고 음원은 소프트 참조라, 여기서 로드되는 건 표뿐이다.
	const TSoftObjectPtr<UDataTable> SoftTables[] =
	{
		Settings->UISoundTable,
		Settings->CharacterSoundTable,
		Settings->GameSoundTable,
		Settings->RuleSoundTable,
		Settings->MusicSoundTable
	};

	for (const TSoftObjectPtr<UDataTable>& SoftTable : SoftTables)
	{
		if (SoftTable.IsNull())
		{
			continue;
		}

		if (const UDataTable* Table = SoftTable.LoadSynchronous())
		{
			OutTables.Add(Table);
		}
	}
}

const FFASoundRow* UFAAudioSettings::FindSound(FName SoundId)
{
	if (SoundId.IsNone())
	{
		return nullptr;
	}

	// 다섯 장 전부를 뒤진다. 호출부는 ID 가 어느 표에 있는지 몰라도 된다.
	TArray<const UDataTable*> Tables;
	GatherTables(Tables);

	for (const UDataTable* Table : Tables)
	{
		// bWarnIfRowMissing = false — 다섯 장을 순회하는 구조라 "없음"이 정상 경로다.
		if (const FFASoundRow* Row = Table->FindRow<FFASoundRow>(SoundId, TEXT("FAFindSound"), /*bWarnIfRowMissing=*/false))
		{
			return Row;
		}
	}

	return nullptr;
}

void UFAAudioSettings::GetAllSoundIds(TArray<FName>& OutSoundIds)
{
	OutSoundIds.Reset();

	TArray<const UDataTable*> Tables;
	GatherTables(Tables);

	for (const UDataTable* Table : Tables)
	{
		OutSoundIds.Append(Table->GetRowNames());
	}
}

USoundAttenuation* UFAAudioSettings::GetDefaultAttenuation(EFASoundCategory Category)
{
	const UFAAudioSettings* Settings = Get();
	if (!Settings)
	{
		return nullptr;
	}

	const TSoftObjectPtr<USoundAttenuation>* Found = Settings->DefaultAttenuationByCategory.Find(Category);
	return (Found && !Found->IsNull()) ? Found->LoadSynchronous() : nullptr;
}

FName UFAAudioSettings::FindRuleSeriesSoundId(const FGameplayTagContainer& RuleTags)
{
	const UFAAudioSettings* Settings = Get();
	if (!Settings || RuleTags.IsEmpty())
	{
		return NAME_None;
	}

	for (const FGameplayTag& Tag : RuleTags)
	{
		const FString TagString = Tag.ToString();

		for (const TPair<FName, FName>& Pair : Settings->RuleSeriesSoundIds)
		{
			const FString Prefix = Pair.Key.ToString();

			// `Rule.Move` 는 `Rule.Move.NoJump` 에만 걸려야 한다. `Rule.Movement` 에 걸리면 안 되므로
			// 정확히 같거나 뒤에 마디 구분점이 오는 경우만 인정한다.
			if (TagString == Prefix || TagString.StartsWith(Prefix + TEXT("."), ESearchCase::IgnoreCase))
			{
				return Pair.Value;
			}
		}
	}

	return NAME_None;
}

FName UFAAudioSettings::MakeRuleTierSoundId(int32 Tier)
{
	const UFAAudioSettings* Settings = Get();
	if (!Settings || Settings->RuleTierSoundIdFormat.IsEmpty())
	{
		return NAME_None;
	}

	const FString Formatted = Settings->RuleTierSoundIdFormat.Replace(
		TEXT("{0}"), *FString::FromInt(FMath::Clamp(Tier, 1, 3)));

	return FName(*Formatted);
}
