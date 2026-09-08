// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FAMapTypes.h"
#include "Engine/DeveloperSettings.h"
#include "FAMapSettings.generated.h"


class UDataTable;

UCLASS(
	Config = Game,
	DefaultConfig,
	meta = (DisplayName = "Falling Apart | Maps")
)
class FALLINGAPART_API UFAMapSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UFAMapSettings();
	
	virtual FName GetCategoryName() const override
	{
		return TEXT("Game");
	}
	
	UPROPERTY(
		Config, 
		EditAnywhere, 
		Category="FallingApart|Maps",
		meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> MapDataTable;
	
	
	//* 기본 또는 오류시 선택될 맵 */
	UPROPERTY(Config, EditAnywhere, Category="FallingApart|Maps")
	FName DefaultMapId;
	
	static const UDataTable* GetMapDataTable();
	
	static const FFAMapDataRow* FindMap(FName MapId);
	
	//* MapId로 맵이 존재하는지, 사용 가능한지 확인. */
	static bool IsSelectableMap(FName MapId);

	/** SortOrder 순서로 정렬된 선택 가능 MapId */
	static TArray<FName> GetSelectableMapIds();

	/** 이전 선택값 → 기본값 → 첫 유효 맵 순서로 결정 */
	static FName ResolveInitialMapId(FName PreferredMapId);
};
