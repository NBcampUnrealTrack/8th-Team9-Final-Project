// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UI/FAGenericPopup.h"
#include "FAUIMessageSubsystem.generated.h"

class APlayerController;
class SWidget;

/**
 * 모든 레벨과 위젯에서 공통으로 사용하는 단일 메시지 창 관리자.
 *
 * GameInstance와 수명을 같이하므로 맵 전환 전 예약한 메시지를 전환 후에 표시할 수 있다.
 * 활성 팝업은 항상 하나만 유지하며, 연속 호출 시 새 창을 쌓지 않고 내용을 교체한다.
 */
UCLASS(Config = Game, DefaultConfig, Blueprintable)
class FALLINGAPART_API UFAUIMessageSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFAUIMessageSubsystem();
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 현재 월드에 즉시 메시지를 표시한다. */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|UI|Message",
		meta = (WorldContext = "WorldContextObject", DisplayName = "Show FA Message"))
	static void ShowMessage(const UObject* WorldContextObject, const FText& Title, const FText& Message,
		const FText& ConfirmText = FText());

	/**
	 * 예/아니오 메시지창을 열고 버튼 선택 결과를 FOnFAConfirmationResult 콜백의 bool 값으로 반환한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|UI|Message",
		meta = (WorldContext = "WorldContextObject", DisplayName = "Show FA Confirmation"))
	static void ShowConfirmation(const UObject* WorldContextObject, const FText& Title, const FText& Message,
		FOnFAConfirmationResult ResultCallback, const FText& YesText = FText(), const FText& NoText = FText());

	/** 맵 전환이 끝난 뒤 다음 월드에 메시지를 표시한다. 호스트 이탈/접속 종료 처리용. */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|UI|Message",
		meta = (WorldContext = "WorldContextObject", DisplayName = "Show FA Message After Travel"))
	static void ShowMessageAfterTravel(const UObject* WorldContextObject, const FText& Title, const FText& Message,
		const FText& ConfirmText = FText());

	/** 현재 떠 있는 메시지 창이 있으면 닫고 참조를 정리한다. */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|UI|Message",
		meta = (WorldContext = "WorldContextObject", DisplayName = "Close FA Message"))
	static void CloseMessage(const UObject* WorldContextObject);

	/** 정적 함수를 통해 서브시스템 참조 과정을 간소화한다. */
	static UFAUIMessageSubsystem* Get(const UObject* WorldContextObject);

private:
	bool ShowMessageInternal(const FText& Title, const FText& Message, const FText& ConfirmText);
	bool ShowConfirmationInternal(const FText& Title, const FText& Message,
		const FText& YesText, const FText& NoText, const FOnFAConfirmationResult& ResultCallback);
	void QueueMessage(const FText& Title, const FText& Message, const FText& ConfirmText);
	void CloseMessageInternal();
	void HandlePopupClosed(UFAGenericPopup* ClosedPopup);
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void TryShowPendingMessage();

	/** UI창이 열릴때 InputMode를 저장후 메시지 창에 접근이 가능하도록 GameAndUI로 변경.
	 * UIOnly는 매치 중 조작을 멈출 가능성이 있어 GameAndUI를 사용함. */
	void SaveAndApplyPopupInputMode(APlayerController* PlayerController, UFAGenericPopup* Popup);
	void RestorePreviousInputMode();
	void ResetSavedInputMode();

	UPROPERTY(Config, EditAnywhere, Category = "FallingApart|UI|Message")
	TSubclassOf<UFAGenericPopup> PopupWidgetClass;

	UPROPERTY(Config, EditAnywhere, Category = "FallingApart|UI|Message", meta = (ClampMin = "0"))
	int32 PopupZOrder = 500;

	UPROPERTY(Transient)
	TObjectPtr<UFAGenericPopup> ActivePopup;

	FText PendingTitle;
	FText PendingMessage;
	FText PendingConfirmText;
	bool bHasPendingMessage = false;
	FDelegateHandle PostLoadMapDelegateHandle;

	/**
	 * UE에는 런타임용 InputMode getter가 없으므로, 모드를 판별하고 복원하는 데 필요한
	 * 실제 뷰포트/포커스 상태를 첫 팝업이 열릴 때 한 번만 저장한다.
	 */
	TWeakObjectPtr<APlayerController> SavedInputPlayerController;
	TWeakPtr<SWidget> SavedFocusedWidget;
	uint8 SavedInputMode = 0;
	uint8 SavedMouseCaptureMode = 0;
	uint8 SavedMouseLockMode = 0;
	bool bSavedShowMouseCursor = false;
	bool bSavedIgnoreInput = false;
	bool bSavedHideCursorDuringCapture = false;
	bool bHasSavedInputMode = false;
};
