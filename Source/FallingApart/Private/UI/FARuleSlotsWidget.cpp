// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FARuleSlotsWidget.h"

#include "AbilitySystemInterface.h"
#include "Audio/FAAudioSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "GAS/FAAbilitySystemComponent.h"
#include "GameFramework/PlayerController.h"
#include "Rule/FARuleSettings.h"
#include "Styling/CoreStyle.h"

namespace FARuleSlotsAudio
{
	/** 룰이 상한에 밀려 사라졌을 때. 표의 RowName 과 같아야 한다 (DT_Sound_Game). */
	static const FName RuleExpired(TEXT("Draft_RuleExpired"));
}

namespace FARuleSlotsUI
{
	/** 임시 레이아웃 한 칸의 크기(픽셀). 화면을 가리지 않는 선. */
	static constexpr float SlotSize = 72.0f;
	static constexpr float IconSize = 40.0f;
}

TSharedRef<SWidget> UFARuleSlotsWidget::RebuildWidget()
{
	if (IsDesignTime())
	{
		return Super::RebuildWidget();
	}

	GatherSlotWidgets();
	ApplySlotsToWidgets();

	return Super::RebuildWidget();
}

void UFARuleSlotsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	GatherSlotWidgets();

	RefreshBinding();

	// 접속 직후 이미 들고 있던 룰은 '새로 붙은 것'이 아니다 — 기준선만 깔고 소리는 내지 않는다.
	RefreshSlots(/*bForceEvent=*/true);
}

void UFARuleSlotsWidget::NativeDestruct()
{
	if (UFAAbilitySystemComponent* ASC = BoundASC.Get())
	{
		ASC->OnActiveRulesChanged.RemoveDynamic(this, &UFARuleSlotsWidget::HandleActiveRulesChanged);
	}
	BoundASC.Reset();

	Super::NativeDestruct();
}

void UFARuleSlotsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 포인터 비교뿐이라 매 프레임 불러도 부담이 없다 (UFAHUDWidget 과 같은 방식).
	RefreshBinding();

	UpdateAnimatedVisuals(InDeltaTime);
}

//~ ---------------- ASC 바인딩 ----------------

void UFARuleSlotsWidget::RefreshBinding()
{
	const APlayerController* OwningController = GetOwningPlayer();
	const APawn* LocalPawn = OwningController ? OwningController->GetPawn() : nullptr;

	UFAAbilitySystemComponent* NewASC = nullptr;
	if (const IAbilitySystemInterface* AsInterface = Cast<IAbilitySystemInterface>(LocalPawn))
	{
		NewASC = Cast<UFAAbilitySystemComponent>(AsInterface->GetAbilitySystemComponent());
	}

	UFAAbilitySystemComponent* OldASC = BoundASC.Get();
	if (NewASC == OldASC)
	{
		return;
	}

	if (OldASC)
	{
		OldASC->OnActiveRulesChanged.RemoveDynamic(this, &UFARuleSlotsWidget::HandleActiveRulesChanged);
	}

	BoundASC = NewASC;

	if (NewASC)
	{
		NewASC->OnActiveRulesChanged.AddDynamic(this, &UFARuleSlotsWidget::HandleActiveRulesChanged);
	}

	// 폰이 갈리면 보유 목록도 통째로 바뀐다. 이건 '새 룰이 붙은 것'이 아니므로 소리는 내지 않는다.
	bHasBaseline = false;
	RefreshSlots(/*bForceEvent=*/true);
}

void UFARuleSlotsWidget::HandleActiveRulesChanged()
{
	RefreshSlots(/*bForceEvent=*/false);
}

void UFARuleSlotsWidget::RefreshSlots(bool bForceEvent)
{
	TArray<FName> CurrentIds;
	if (const UFAAbilitySystemComponent* ASC = BoundASC.Get())
	{
		CurrentIds = ASC->GetActiveRuleIds();
	}

	// 상한이 칸 수보다 크게 잡혀 있으면 앞(=오래된 것)부터 4장만 그린다.
	const int32 VisibleCount = FMath::Min3(CurrentIds.Num(), MaxSlots, UFARuleSettings::GetMaxActiveRules());

	if (!bForceEvent && CurrentIds == LastRuleIds)
	{
		return;
	}

	//~ 추가·제거를 구분한다. 감염으로 진영 룰이 통째로 빠지면 제거 이벤트가 여러 번 나간다.
	if (bHasBaseline)
	{
		for (const FName& OldId : LastRuleIds)
		{
			if (!CurrentIds.Contains(OldId))
			{
				OnRuleRemoved(UFARuleSettings::MakeRuleDisplayInfo(OldId));
			}
		}
	}

	Slots.Reset(VisibleCount);
	for (int32 Index = 0; Index < VisibleCount; ++Index)
	{
		Slots.Add(UFARuleSettings::MakeRuleDisplayInfo(CurrentIds[Index]));
	}

	// 상한에 닿았을 때만 예고한다. 아직 여유가 있으면 다음 카드는 아무것도 밀어내지 않는다.
	ExpiringSlotIndex = (CurrentIds.Num() >= UFARuleSettings::GetMaxActiveRules() && Slots.Num() > 0)
		? 0
		: INDEX_NONE;

	if (bHasBaseline)
	{
		for (const FName& NewId : CurrentIds)
		{
			if (!LastRuleIds.Contains(NewId))
			{
				const FFARuleDisplayInfo Info = UFARuleSettings::MakeRuleDisplayInfo(NewId);
				PlayRuleAddedSound(Info);
				OnRuleAdded(Info);
			}
		}

		// 제거 소리는 장수가 줄었을 때 한 번만. 감염 때 4번 겹쳐 나면 소리가 뭉개진다.
		if (CurrentIds.Num() < LastRuleIds.Num())
		{
			PlayRuleRemovedSound();
		}
	}

	LastRuleIds = MoveTemp(CurrentIds);
	bHasBaseline = true;

	ApplySlotsToWidgets();
	OnRuleSlotsChanged(Slots, ExpiringSlotIndex);
}

//~ ---------------- 그리기 ----------------

void UFARuleSlotsWidget::GatherSlotWidgets()
{ 
	SlotBackgrounds = { Img_Slot0Background, Img_Slot1Background, Img_Slot2Background, Img_Slot3Background };
	SlotIcons = { Img_Slot0Icon, Img_Slot1Icon, Img_Slot2Icon, Img_Slot3Icon };
}

void UFARuleSlotsWidget::ApplySlotsToWidgets()
{
	for (int32 Index = 0; Index < MaxSlots; ++Index)
	{
		const bool bFilled = Slots.IsValidIndex(Index);
		
		UImage* BgImage = SlotBackgrounds.IsValidIndex(Index) ? SlotBackgrounds[Index].Get() : nullptr;
		UImage* IconImage = SlotIcons.IsValidIndex(Index) ? SlotIcons[Index].Get() : nullptr;

		if (!bFilled)
		{
			if (BgImage)
			{
				BgImage->SetVisibility(ESlateVisibility::Collapsed);
			}
			
			if (IconImage)
			{
				IconImage->SetVisibility(ESlateVisibility::Collapsed);
			}
			
			continue;
		}

		const FFARuleDisplayInfo& Info = Slots[Index];

		if (BgImage)
		{
			if (UTexture2D* BgTexture = Info.BackgroundImage.IsNull() ? nullptr : Info.BackgroundImage.LoadSynchronous())
			{
				BgImage->SetBrushFromTexture(BgTexture);
				BgImage->SetColorAndOpacity(FLinearColor::White);
				BgImage->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				BgImage->SetVisibility(ESlateVisibility::Collapsed);
			}
			
			BgImage->SetRenderOpacity(1.0f);
		}

		if (IconImage)
		{
			if (UTexture2D* IconTexture = Info.Icon.IsNull() ? nullptr : Info.Icon.LoadSynchronous())
			{
				IconImage->SetBrushFromTexture(IconTexture);
				IconImage->SetColorAndOpacity(FLinearColor::White);
				IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				IconImage->SetVisibility(ESlateVisibility::Collapsed);
			}
			
			IconImage->SetRenderOpacity(1.0f);
		}
	}
}

void UFARuleSlotsWidget::UpdateAnimatedVisuals(float InDeltaTime)
{
	AnimTime += InDeltaTime;

	// 소멸 예고 — 가장 오래된 칸이 깜빡인다 (GDD 8.1).
	if (Slots.Num() > 0 && ExpiringBlinkPeriod > 0.0f)
	{
		const float Phase = FMath::Sin(AnimTime * 2.0f * PI / ExpiringBlinkPeriod) * 0.5f + 0.5f;
		const float Opacity = FMath::Lerp(ExpiringBlinkMinOpacity, 1.0f, Phase);

		for (int32 Index = 0; Index < MaxSlots; ++Index)
		{
			if (!Slots.IsValidIndex(Index))
			{
				continue;
			}
			
			const float TargetOpacity = (Index == ExpiringSlotIndex) ? Opacity : 1.0f;
			
			if (UImage* BgImage = SlotBackgrounds.IsValidIndex(Index) ? SlotBackgrounds[Index].Get() : nullptr)
			{
				BgImage->SetRenderOpacity(TargetOpacity);
			}
		}
	}

	// 도박(G) 카드는 색 자체가 불확실성이다 (UX 가이드 3.1).
		for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (!Slots[Index].bAnimatedColor)
		{
			continue;
		}

		if (UImage* BgImage = SlotBackgrounds.IsValidIndex(Index) ? SlotBackgrounds[Index].Get() : nullptr)
		{
			BgImage->SetColorAndOpacity(UFARuleSettings::GetGambleColor(AnimTime));
		}
	}
}

//~ ---------------- 소리 ----------------

void UFARuleSlotsWidget::PlayRuleAddedSound(const FFARuleDisplayInfo& Rule)
{
	if (!bPlayRuleSounds)
	{
		return;
	}

	UFAAudioSubsystem* Audio = UFAAudioSubsystem::Get(this);
	if (!Audio)
	{
		return;
	}

	// 전용 소리 → 계열 소리 → 티어 기본음 3단 폴백은 오디오 쪽이 알아서 한다.
	FGameplayTagContainer RuleTags;
	if (Rule.RuleTag.IsValid())
	{
		RuleTags.AddTag(Rule.RuleTag);
	}

	Audio->PlayRuleSound(Rule.RuleId, RuleTags, static_cast<int32>(Rule.Tier));
}

void UFARuleSlotsWidget::PlayRuleRemovedSound()
{
	if (!bPlayRuleSounds)
	{
		return;
	}

	UFAAudioSubsystem::PlayFASound(this, FARuleSlotsAudio::RuleExpired);
}
