// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LoadingScreenKitTypes.generated.h"

/**
 * 로딩 화면 한 번을 띄울 때의 조건.
 */
USTRUCT(BlueprintType)
struct LOADINGSCREENKIT_API FLSKLoadingScreenParams
{
	GENERATED_BODY()

	/** 화면 가운데 큰 글씨. 비우면 설정의 DefaultTitle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LoadingScreenKit")
	FText Title;

	/** 아래쪽 한 줄. 비우면 설정의 Tips 에서 골라 돌린다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LoadingScreenKit")
	FText Tip;

	/**
	 * 아무리 빨리 끝나도 이 시간만큼은 떠 있는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LoadingScreenKit")
	float MinimumDisplaySeconds = -1.f;

	/**
	 * 이 시간이 지나면 아무 신호가 없어도 강제로 걷는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LoadingScreenKit")
	float MaximumDisplaySeconds = -1.f;

	/** 걷을 때 서서히 사라지는 시간. 0 이면 즉시. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LoadingScreenKit")
	float FadeOutSeconds = -1.f;

	/** 켜면 로딩 화면이 마우스 입력을 먹는다. 뒤에 있는 UI 가 눌리면 안 되므로 기본은 켬. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LoadingScreenKit")
	bool bBlockInput = true;

	/**
	 * 새 맵 로드가 끝나면 알아서 걷는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LoadingScreenKit")
	bool bAutoHideWhenMapLoaded = true;
};
