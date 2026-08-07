// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FASessionCreateWidget.generated.h"

class UEditableText;
class UButton;
/**
 * 서버 생성 선택시 세션설정 및 생성을 진행하는 위젯.
 */
UCLASS()
class FALLINGAPART_API UFASessionCreateWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual bool Initialize() override;
	
	FString GetSessionName();
	
	int32 GetMaxConnections();
	
	FString GetPasswordString();
	
protected:
	virtual void NativeConstruct() override;
	
private:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UEditableText> NicknameInput;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UEditableText> SessionNameInput;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UEditableText> MaxConnectionInput;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UEditableText> PrivateSessionPass;
	
	UFUNCTION()
	void OnNumberTextChanged(const FText& Text);
	
	FText LastValidText;
};
