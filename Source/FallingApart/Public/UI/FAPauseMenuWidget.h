// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FAPauseMenuWidget.generated.h"

class UButton;
class UTextBlock;

/**
 * ESC 로 여는 공용 일시정지 메뉴 — 로비 · 인게임.
 * 메인 메뉴 / 설정 / 게임 종료 세 가지.
 *
 * 실제 동작은 전부 AFAPlayerController 가 한다 — 이 위젯은 버튼만 들고 있다.
 */
UCLASS()
class FALLINGAPART_API UFAPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 메뉴를 닫는다. 컨트롤러를 거치므로 입력 모드 · 커서까지 원래대로. */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|PauseMenu")
	void CloseMenu();

	//~ 버튼 핸들러. 임시 레이아웃 · WBP 양쪽에서 그대로 바인딩.
	UFUNCTION(BlueprintCallable, Category = "FallingApart|PauseMenu")
	void OnMainMenuClicked();

	UFUNCTION(BlueprintCallable, Category = "FallingApart|PauseMenu")
	void OnSettingsClicked();

	/** 게임 종료*/
	UFUNCTION(BlueprintCallable, Category = "FallingApart|PauseMenu")
	void OnQuitClicked();

	/** 버튼 위에 마우스가 올라갔을 때. 세 버튼이 공유한다. */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|PauseMenu")
	void OnButtonHovered();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	//~ WBP 에 같은 이름의 버튼이 있으면 자동으로 물린다. 없으면 임시 레이아웃이 채운다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|PauseMenu")
	TObjectPtr<UButton> Btn_MainMenu;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|PauseMenu")
	TObjectPtr<UButton> Btn_Settings;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FallingApart|PauseMenu")
	TObjectPtr<UButton> Btn_Quit;

	/**
	 * 설정 창 위젯 클래스.
	 * 비워두면 SettingsKit 플러그인의 `USKSettingsMenuWidget`(C++ 임시 레이아웃)이 뜬다.
	 * WBP_FASettings 를 만들면 WBP_FAPauseMenu 클래스 디폴트에서 여기에 지정할 것.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|PauseMenu")
	TSubclassOf<UUserWidget> SettingsWidgetClass;

	/** 일시정지 메뉴 위에 떠야 한다. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|PauseMenu")
	int32 SettingsWidgetZOrder = 120;

	//~ UI 담당이 WBP 에서 구현할 것 (등장 · 퇴장 연출 등). 미구현이면 no-op.
	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|PauseMenu")
	void OnPauseMenuOpened();

	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|PauseMenu")
	void OnPauseMenuClosed();

private:
	/** 설정창이 닫히기 시작할 때. 닫히는 소리를 낸다. */
	UFUNCTION()
	void HandleSettingsClosed();

	//~ WBP 가 없을 때만 쓰는 임시 레이아웃.
	void BuildFallbackLayout();

	/** 임시 레이아웃 · WBP 공통 클릭 바인딩. */
	void BindButtons();

	/** 임시 레이아웃용 버튼 1개(텍스트 포함) 생성. */
	UButton* MakeFallbackButton(class UVerticalBox* Column, const FText& Label);

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> SettingsWidget;
};
