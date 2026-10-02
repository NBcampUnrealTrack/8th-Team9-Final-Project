#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "FAHUDDetailsWidget.generated.h"

class UCanvasPanel;
class UTextBlock;
class UVerticalBox;
class UInputAction;
class UFAHUDControlRowWidget;

UCLASS()
class FALLINGAPART_API UFAHUDDetailsWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(EditDefaultsOnly, Category = "HUD Details")
	TSubclassOf<UFAHUDControlRowWidget> ControlRowClass;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCanvasPanel> DetailsCanvas;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UVerticalBox> Controls;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> ScoreHistoryTitle;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> ControlsTitle;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> EmptyScoreText;

private:
	void RefreshDetails();
	void RefreshControls();
	void AddControl(const FText& Label, const TArray<FKey>& Keys);
	UPROPERTY(Transient) TArray<TObjectPtr<UWidget>> RulePanels;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> RuleNames;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> RuleDescriptions;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> ScoreDescriptions;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> ScorePoints;
	FString ControlSignature;
	float RefreshCountdown = 0.f;
	bool bDetailsVisible = false;
};
