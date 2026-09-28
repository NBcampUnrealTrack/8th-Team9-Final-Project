#include "UI/FAHUDControlRowWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"

#define LOCTEXT_NAMESPACE "FAHUDDetails"

void UFAHUDKeycapWidget::SetKeyLabel(const FText& Label)
{
	if (Text_Key) { Text_Key->SetText(Label); }
}

void UFAHUDControlRowWidget::SetControl(const FText& Label, const TArray<FKey>& Keys)
{
	if (Text_Action) { Text_Action->SetText(Label); }
	if (!KeyList || !KeycapWidgetClass) { return; }
	KeyList->ClearChildren();
	const int32 Count = FMath::Max(1, Keys.Num());
	for (int32 Index = 0; Index < Count; ++Index)
	{
		UFAHUDKeycapWidget* Cap = CreateWidget<UFAHUDKeycapWidget>(this, KeycapWidgetClass);
		if (!Cap) { continue; }
		KeyList->AddChildToHorizontalBox(Cap)->SetPadding(FMargin(0.f, 0.f, Index + 1 < Count ? KeySpacing : 0.f, 0.f));
		Cap->SetKeyLabel(Keys.IsEmpty() ? LOCTEXT("Unbound", "미지정") : Keys[Index].GetDisplayName(false));
	}
}

#undef LOCTEXT_NAMESPACE
