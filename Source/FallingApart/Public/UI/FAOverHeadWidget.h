// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FAOverHeadWidget.generated.h"

class UTextBlock;
/**
 * 
 */
UCLASS()
class FALLINGAPART_API UFAOverHeadWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	
protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;
	
	UPROPERTY(BlueprintReadOnly, Category="FallingApart|Widget", meta=(OptionalWidget))
	TObjectPtr<UTextBlock> Img_TeamIcon;// 진영 아이콘?
	
	UPROPERTY(BlueprintReadOnly, Category="FallingApart|Widget", meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_Nickname;
	
	
	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Visibility", meta=(OptionalWidget))
	int NamePlateCullDistance = 4000;
	
	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Visibility", meta=(OptionalWidget))
	int VisibilityCheckFrequency = 0.1;
	
	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Visibility", meta=(OptionalWidget))
	int OcclusionCheckFrequency = 0.2;

private:
	TObjectPtr<APawn> OwnerCharacter;
	
	bool IsCharacterVisible = false;
	bool IsOccluded = false;
	
	float LastCharacterVisibleUpdateTime = 0.0f;
	float LastOcclusionUpdateTime = 0.0f;
};
