// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SettingsKitTypes.h"
#include "SettingsKitPluginSettings.generated.h"

class UInputMappingContext;

/**
 * 이 플러그인의 프로젝트 단위 설정.
 * (Project Settings → Plugins → Settings Kit)
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Settings Kit"))
class SETTINGSKIT_API USKPluginSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/**
	 * 조작 탭에서 키를 바꿀 수 있게 할 Input Mapping Context 목록.
	 *
	 * 이걸 채우지 않으면 조작 탭이 빈 채로 뜬다.
	 * Enhanced Input 은 "사용자 설정에 등록된 IMC" 의 매핑만 키 프로필에 올린다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Controls", meta = (AllowedClasses = "/Script/EnhancedInput.InputMappingContext"))
	TArray<TSoftObjectPtr<UInputMappingContext>> RebindableMappingContexts;

	/**
	 * 게임 전용 위젯을 탭 안에 끼워 넣을 목록.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Extensions")
	TArray<FSKTabExtension> TabExtensions;

	static const USKPluginSettings* Get();
};
