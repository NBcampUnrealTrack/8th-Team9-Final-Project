// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

/**
 * 이 플러그인의 전용 로그 카테고리.
 *
 * LogTemp 를 쓰면 안 된다 — 플러그인을 얹은 프로젝트에서 자기 로그와 우리 로그가 섞여
 * 필터링이 안 된다. `Log LogSettingsKit Verbose` 로 이 플러그인 것만 켜고 끌 수 있어야 한다.
 */
SETTINGSKIT_API DECLARE_LOG_CATEGORY_EXTERN(LogSettingsKit, Log, All);
