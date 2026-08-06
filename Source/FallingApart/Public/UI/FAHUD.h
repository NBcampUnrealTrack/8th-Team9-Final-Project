// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Templates/SubclassOf.h"
#include "FAHUD.generated.h"

class UFAHUDWidget;

/**
 * 인게임 HUD 위젯의 소유자.
 */
UCLASS()
class FALLINGAPART_API AFAHUD : public AHUD
{
	GENERATED_BODY()

public:
	AFAHUD();

	UFUNCTION(BlueprintPure, Category = "FallingApart|HUD")
	UFAHUDWidget* GetHUDWidget() const { return HUDWidget; }

	/**
	 * 해당 컨트롤러 화면의 HUD 위젯. 서버·원격 클라이언트에서는 null
	 * 감염 게이지처럼 화면에만 보이는 것은 반드시 로컬에서 호출할 것.
	 */
	UFUNCTION(BlueprintPure, Category = "FallingApart|HUD")
	static UFAHUDWidget* GetHUDWidgetFor(APlayerController* PlayerController);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** WBP_FAHUD 를 지정. 비어 있으면 HUD 가 뜨지 않고 경고 로그만. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|HUD")
	TSubclassOf<UFAHUDWidget> HUDWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|HUD")
	int32 HUDWidgetZOrder = 0;

private:
	UPROPERTY(Transient)
	TObjectPtr<UFAHUDWidget> HUDWidget;
};
