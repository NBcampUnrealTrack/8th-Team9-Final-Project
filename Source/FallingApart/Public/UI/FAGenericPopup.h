// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FAGenericPopup.generated.h"

class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFAGenericPopupConfirmed);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnFAConfirmationResult, bool, bConfirmed);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnFAGenericPopupClosed, class UFAGenericPopup*);

/**
 * 제목, 본문, 확인 버튼만 갖는 공용 메시지 창.
 *
 * 생성과 수명 관리는 UFAUIMessageSubsystem이 담당한다. 이 위젯은 전달받은 내용을
 * 화면에 반영하고, 확인 버튼을 누르면 자신을 닫아 관리자에게 알리는 역할만 한다.
 */
UCLASS()
class FALLINGAPART_API UFAGenericPopup : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 표시할 문구를 설정한다. 이미 떠 있는 창의 내용을 교체할 때도 사용한다. */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|UI|Message")
	void SetupMessage(const FText& Title, const FText& Message,
		const FText& ConfirmText = FText());

	/** 예/아니오 확인창으로 설정하고 선택 결과를 bool 콜백으로 전달한다. */
	void SetupConfirmation(const FText& Title, const FText& Message,
		const FText& YesText, const FText& NoText,
		const FOnFAConfirmationResult& ResultCallback);

	bool IsConfirmationDialog() const { return bConfirmationDialog; }
	bool SupportsConfirmationLayout() const { return NoButton != nullptr && NoButtonText != nullptr; }

	/** 확인 버튼 클릭과 동일하게 창을 닫고 뷰포트에서 제거한다. */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|UI|Message")
	void ClosePopup();

	/** 확인 버튼으로 닫혔을 때 외부 블루프린트 로직이 필요하면 여기에 바인딩한다. */
	UPROPERTY(BlueprintAssignable, Category = "FallingApart|UI|Message")
	FOnFAGenericPopupConfirmed OnConfirmed;

	/** 서브시스템이 강한 참조를 해제하기 위한 C++ 전용 알림. */
	FOnFAGenericPopupClosed OnClosed;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleNoClicked();

	void ResolveConfirmation(bool bConfirmed);

	/** WBP 없이 네이티브 클래스만 생성해도 동작하는 최소 레이아웃을 만든다. */
	void BuildFallbackLayout();
	void BroadcastClosedOnce();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MessageText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ConfirmButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ConfirmButtonText;

	/** 기존 WBP에는 없어도 된다. 확인창 디자인을 적용할 때 같은 이름으로 추가한다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> NoButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NoButtonText;

	UPROPERTY()
	FOnFAConfirmationResult ConfirmationResultCallback;

	bool bClosedBroadcast = false;
	bool bConfirmationDialog = false;
};
