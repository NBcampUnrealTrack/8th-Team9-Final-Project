// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FADeathMatchSettings.generated.h"

/**
 * 데스매치(GDD 5.4 / 5.5) 관련 수치를 프로젝트 설정에 한 번만 박아두는 곳.
 * (Project Settings → Game → Falling Apart | DeathMatch)
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Falling Apart | DeathMatch"))
class FALLINGAPART_API UFADeathMatchSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UFADeathMatchSettings();

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** 데스매치 제한 시간(초). 이 시간까지 인간이 안 잡히면 인간 승 (GDD 5.4 사양표 "제한 시간: 1분"). */
	UPROPERTY(Config, EditAnywhere, Category = "DeathMatch", meta = (ClampMin = "5.0"))
	float TimeLimit = 60.0f;

	/** 데스매치 제한 시간(초). */
	static float GetTimeLimit();
};
