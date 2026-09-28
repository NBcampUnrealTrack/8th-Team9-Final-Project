#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "FAHUDControlRowWidget.generated.h"

class UTextBlock;
class UHorizontalBox;

UCLASS(Abstract)
class FALLINGAPART_API UFAHUDKeycapWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetKeyLabel(const FText& Label);
protected:
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Key;
};

UCLASS(Abstract)
class FALLINGAPART_API UFAHUDControlRowWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetControl(const FText& Label, const TArray<FKey>& Keys);
protected:
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UHorizontalBox> KeyList;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_Action;
	UPROPERTY(EditDefaultsOnly, Category = "HUD Details")
	TSubclassOf<UFAHUDKeycapWidget> KeycapWidgetClass;
	UPROPERTY(EditDefaultsOnly, Category = "HUD Details", meta = (ClampMin = "0"))
	float KeySpacing = 6.f;
};
