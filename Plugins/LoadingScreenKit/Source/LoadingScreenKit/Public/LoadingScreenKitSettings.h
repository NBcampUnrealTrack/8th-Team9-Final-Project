// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "LoadingScreenKitTypes.h"
#include "LoadingScreenKitSettings.generated.h"

class ULSKLoadingScreenWidget;

/**
 * 프로젝트 설정 → Plugins → Loading Screen Kit.
 *
 * 게임 코드가 로딩 화면을 부를 때 매번 조건을 넘기지 않아도 되도록 기본값을 여기 모아둔다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Loading Screen Kit"))
class LOADINGSCREENKIT_API ULSKPluginSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	ULSKPluginSettings();

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/**
	 * 띄울 위젯. 비워두면 C++ 임시 레이아웃이 조립된다(검정 배경 + 글씨 + 회전자).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen", meta = (AllowAbstract = "false"))
	TSoftClassPtr<ULSKLoadingScreenWidget> WidgetClass;

	/**
	 * 뷰포트 Z 순서. 로딩 화면은 무조건 맨 위여야 한다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen")
	int32 ZOrder = 10000;

	/** FLSKLoadingScreenParams 의 같은 이름 항목이 음수일 때 쓰이는 값. */
	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen", meta = (ClampMin = "0.0", UIMax = "5.0"))
	float MinimumDisplaySeconds = 1.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen", meta = (ClampMin = "0.0", UIMax = "120.0"))
	float MaximumDisplaySeconds = 45.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen", meta = (ClampMin = "0.0", UIMax = "3.0"))
	float FadeOutSeconds = 0.35f;

	/**
	 * 맵 로드가 끝난 뒤에도 이만큼 더 붙잡는다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen", meta = (ClampMin = "0.0", UIMax = "5.0"))
	float HoldAfterMapLoadedSeconds = 0.5f;

	/**
	 * 맵이 바뀔 때 게임 코드가 부르지 않아도 알아서 띄운다.
	 *
	 * 끄면 ULSKLoadingScreenSubsystem::Show() 를 부른 곳에서만 뜬다. 켜두면 트래블 경로가
	 * 몇 개든(메인메뉴→로비, 로비→인게임, 인게임→메인메뉴) 전부 덮인다.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen")
	bool bAutoShowOnMapChange = true;

	/** 자동으로 띄울 때 쓸 제목. Show() 에 Title 을 안 넘겼을 때도 이 값이 쓰인다. */
	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen")
	FText DefaultTitle;

	/** 아래쪽에 돌아가며 뜨는 한 줄. 비어 있으면 아무것도 안 뜬다. */
	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen")
	TArray<FText> Tips;

	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen", meta = (ClampMin = "0.5", UIMax = "20.0"))
	float TipRotationSeconds = 4.0f;

	/** 로딩 화면이 마우스 입력을 먹을지. 자동 표시와 Show() 기본값 둘 다에 적용된다. */
	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen")
	bool bBlockInputWhileVisible = true;

	/** 설정값으로 채운 조건 한 벌. Show() 를 인자 없이 부를 때 쓰는 것. */
	FLSKLoadingScreenParams MakeDefaultParams() const;
};
