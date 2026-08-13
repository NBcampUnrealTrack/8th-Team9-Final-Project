// Fill out your copyright notice in the Description page of Project Settings.

#include "Rule/FADeathMatchSettings.h"

UFADeathMatchSettings::UFADeathMatchSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("Falling Apart | DeathMatch");
}

float UFADeathMatchSettings::GetTimeLimit()
{
	const UFADeathMatchSettings* Settings = GetDefault<UFADeathMatchSettings>();
	return Settings ? FMath::Max(5.0f, Settings->TimeLimit) : 60.0f;
}
