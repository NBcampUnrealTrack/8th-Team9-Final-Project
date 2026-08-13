// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Audio/FASoundTypes.h"
#include "Engine/DeveloperSettings.h"
#include "FAAudioSettings.generated.h"

class UDataTable;
class USoundAttenuation;

/**
 * 사운드 표 다섯 장의 위치를 프로젝트 설정에 한 번만 박아두는 곳.
 * (Project Settings → Game → Falling Apart | Audio)
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Falling Apart | Audio"))
class FALLINGAPART_API UFAAudioSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UFAAudioSettings();

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	//~ ---------------- 표 다섯 장 (RowStruct = FFASoundRow) ----------------

	/** 메뉴·버튼·드래프트 창·팝업. UI 담당. */
	UPROPERTY(Config, EditAnywhere, Category = "Audio|Tables", meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> UISoundTable;

	/** 발소리·점프·포획·감염·QTE. 캐릭터 담당. */
	UPROPERTY(Config, EditAnywhere, Category = "Audio|Tables", meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> CharacterSoundTable;

	/** 라운드·점수·데스매치·시스템 알림. 게임모드 담당. */
	UPROPERTY(Config, EditAnywhere, Category = "Audio|Tables", meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> GameSoundTable;

	/** 룰 ID 별 효과음 + 계열 폴백. 룰 담당. 룰이 GE 까지 만들어진 뒤에 행을 채운다. */
	UPROPERTY(Config, EditAnywhere, Category = "Audio|Tables", meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> RuleSoundTable;

	/** BGM·앰비언스·스팅어. 사운드 담당. */
	UPROPERTY(Config, EditAnywhere, Category = "Audio|Tables", meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> MusicSoundTable;

	//~ ---------------- 기본값 ----------------

	/**
	 * 행의 Attenuation 이 비어 있을 때 카테고리별로 대신 쓸 감쇠.
	 * 발소리 하나하나에 감쇠를 지정하지 않아도 되게 하는 장치다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Audio|Defaults")
	TMap<EFASoundCategory, TSoftObjectPtr<USoundAttenuation>> DefaultAttenuationByCategory;

	//~ ---------------- 룰 사운드 3단 폴백 ----------------

	/**
	 * 룰 태그 접두어 → 계열 SoundId. 룰 ID 로 행을 못 찾았을 때 2단계로 쓴다.
	 *
	 * 룰이 100종까지 늘어도 사운드 표는 이 9개 계열 + 티어 3개로 고정된다 — 이게 룰 담당과
	 * 충돌하지 않는 핵심이다. 태그는 `Rule.<계열>.<이름>` 꼴이라 앞 두 마디로 계열이 갈린다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Audio|Rule")
	TMap<FName, FName> RuleSeriesSoundIds;

	/**
	 * 계열도 못 찾았을 때 마지막으로 쓸 티어 기본음. `{0}` 자리에 티어(1~3)가 들어간다.
	 * 룰 사운드가 하나도 없어도 드래프트가 소리 없이 지나가지는 않게 하는 바닥.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Audio|Rule")
	FString RuleTierSoundIdFormat = TEXT("Draft_RuleApplied_T{0}");

	//~ ---------------- 조회 헬퍼 ----------------

	static const UFAAudioSettings* Get();

	/**
	 * SoundId 로 행을 찾는다. 다섯 장을 순서대로 뒤지고 첫 히트를 돌려준다.
	 * 표가 아직 없거나 행이 없으면 nullptr — 애셋이 0개인 단계에서도 호출부가 안전해야 한다.
	 */
	static const FFASoundRow* FindSound(FName SoundId);

	/** 행이 감쇠를 안 지정했을 때 쓸 카테고리 기본 감쇠. 없으면 nullptr. */
	static USoundAttenuation* GetDefaultAttenuation(EFASoundCategory Category);

	/** 지정된 다섯 표를 로드해 담는다. 비어 있거나 로드 실패한 표는 건너뛴다. */
	static void GatherTables(TArray<const UDataTable*>& OutTables);

	/** 다섯 표에 들어 있는 모든 SoundId. 애셋 연결 확인용. */
	static void GetAllSoundIds(TArray<FName>& OutSoundIds);

	/** 룰 태그 묶음에서 계열 SoundId 를 고른다. 못 찾으면 NAME_None. */
	static FName FindRuleSeriesSoundId(const struct FGameplayTagContainer& RuleTags);

	/** 티어(1~3) 기본음 SoundId. */
	static FName MakeRuleTierSoundId(int32 Tier);
};
