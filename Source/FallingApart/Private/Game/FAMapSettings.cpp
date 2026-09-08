// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/FAMapSettings.h"

#include "Engine/DataTable.h"

UFAMapSettings::UFAMapSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("Falling Apart | Maps");
}

const UDataTable* UFAMapSettings::GetMapDataTable()
{
	const UFAMapSettings* Settings = GetDefault<UFAMapSettings>();
	
	if (!Settings || Settings->MapDataTable.IsNull())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[Map] Project Settings에 MapDataTable이 지정되지 않았습니다.")
			);
		return nullptr;
	}
	return Settings->MapDataTable.LoadSynchronous();
}

const FFAMapDataRow* UFAMapSettings::FindMap(FName MapId)
{
	const UDataTable* Table = GetMapDataTable();
	
	if (!Table || MapId.IsNone())
		return nullptr;
	
	return Table->FindRow<FFAMapDataRow>(
		MapId,
		TEXT("FAFindMap"),
		false);
}

bool UFAMapSettings::IsSelectableMap(FName MapId)
{
	const FFAMapDataRow* Row = FindMap(MapId);

	return Row
		&& Row->bSelectable
		&& !Row->Level.IsNull();
}

TArray<FName> UFAMapSettings::GetSelectableMapIds()
{
	TArray<FName> Results;
	
	const UDataTable* Table = GetMapDataTable();
	if (!Table)
		return Results;
	
	for (const FName RowName : Table->GetRowNames())
	{
		if (IsSelectableMap(RowName))
			Results.Add(RowName);
	}
	
	Results.Sort([](const FName& Left, const FName& Right)
	{
		const FFAMapDataRow* LeftRow = FindMap(Left);
		const FFAMapDataRow* RightRow = FindMap(Right);

		const int32 LeftOrder = LeftRow ? LeftRow->SortOrder : MAX_int32;
		const int32 RightOrder = RightRow ? RightRow->SortOrder : MAX_int32;

		if (LeftOrder != RightOrder)
		{
			return LeftOrder < RightOrder;
		}

		return Left.LexicalLess(Right);
	});

	return Results;
}

FName UFAMapSettings::ResolveInitialMapId(FName PreferredMapId)
{
	if (IsSelectableMap(PreferredMapId))
		return PreferredMapId;
	
	const UFAMapSettings* Settings = GetDefault<UFAMapSettings>();

	if (Settings && IsSelectableMap(Settings->DefaultMapId))
	{
		return Settings->DefaultMapId;
	}
	
	const TArray<FName> InitialMapIds = GetSelectableMapIds();
	
	return InitialMapIds.IsEmpty() ? NAME_None : InitialMapIds[0];
}
