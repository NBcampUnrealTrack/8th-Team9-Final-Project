// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Templates/SubclassOf.h"
#include "FAHUD.generated.h"

class UFADraftWidget;
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

	//~ 드래프트 UI — AFAPlayerState::Client_ShowDraftUI 가 로컬에서 호출한다.
	/** 후보를 받아 드래프트 위젯을 띄운다. 이미 떠 있으면 새 후보로 갈아끼운다. */
	void ShowDraftUI(const TArray<FName>& CandidateRuleIds, float SelectionTime);

	/** 라운드 종료 등으로 드래프트를 강제로 접을 때. */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|HUD|Draft")
	void HideDraftUI();

	UFUNCTION(BlueprintPure, Category = "FallingApart|HUD|Draft")
	UFADraftWidget* GetDraftWidget() const { return DraftWidget; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** WBP_FAHUD 를 지정. 비어 있으면 HUD 가 뜨지 않고 경고 로그만. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|HUD")
	TSubclassOf<UFAHUDWidget> HUDWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|HUD")
	int32 HUDWidgetZOrder = 0;

	/**
	 * 드래프트 선택 창. 기본값은 C++ 기본 위젯이라 WBP 없이도 뜬다.
	 * UI 담당이 UFADraftWidget 을 상속한 WBP_FADraft 를 만들면 여기서 갈아끼울 것.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|HUD")
	TSubclassOf<UFADraftWidget> DraftWidgetClass;

	/** HUD 위에 떠야 한다. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|HUD")
	int32 DraftWidgetZOrder = 10;

private:
	UPROPERTY(Transient)
	TObjectPtr<UFAHUDWidget> HUDWidget;

	UPROPERTY(Transient)
	TObjectPtr<UFADraftWidget> DraftWidget;
};
