// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FASessionCreateWidget.generated.h"

class USpinBox;
class UEditableText;
class UButton;

struct FFASessionCreateRequest
{
	int32 MaxConnections = 8;
	FString ServerName;
	FString Password;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnCreateSessionRequested, const FFASessionCreateRequest& Request);

/**
 * 서버 생성 선택시 세션설정 및 생성을 진행하는 위젯.
 */
UCLASS()
class FALLINGAPART_API UFASessionCreateWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual bool Initialize() override;

	void SubmitCreateRequest();

	FOnCreateSessionRequested OnCreateSessionRequested;

protected:
	virtual void NativeConstruct() override;

	/** 최대 인원 스핀박스를 끌 때 나는 눈금 소리. 표의 쿨다운(0.05초)이 연사를 막는다. */
	UFUNCTION()
	void HandleMaxConnectionChanged(float NewValue);

private:

	FString GetSessionName() const;
	
	int32 GetMaxConnections() const;
	
	FString GetPasswordString() const;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UEditableText> NicknameInput;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UEditableText> SessionNameInput;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<USpinBox> MaxConnectionSpinBox;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UEditableText> PrivateSessionPass;
	
	FText LastValidText;
};
