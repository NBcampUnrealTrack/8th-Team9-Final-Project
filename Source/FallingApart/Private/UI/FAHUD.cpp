// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FAHUD.h"

#include "Blueprint/UserWidget.h"
#include "LoadingScreenSubsystem.h"
#include "UI/FADraftWidget.h"
#include "UI/FAHUDWidget.h"
#include "UI/FAHUDDetailsWidget.h"
#include "UI/FARuleSlotsWidget.h"

AFAHUD::AFAHUD()
{
	PrimaryActorTick.bCanEverTick = false;

	// WBP_FADraft 가 만들어지기 전에도 드래프트를 테스트할 수 있어야 한다.
	DraftWidgetClass = UFADraftWidget::StaticClass();

	// 룰 칸도 같은 이유로 C++ 기본 위젯을 기본값으로 둔다.
	RuleSlotsWidgetClass = UFARuleSlotsWidget::StaticClass();
}

void AFAHUD::BeginPlay()
{
	Super::BeginPlay();

	if (!PlayerOwner || !PlayerOwner->IsLocalController())
	{
		return;
	}

	// 로비에서 넘어오며 켜둔 로딩 화면을 여기서 걷는다.
	if (ULSKLoadingScreenSubsystem* LoadingScreen = ULSKLoadingScreenSubsystem::Get(this))
	{
		LoadingScreen->Hide();
	}

	if (!HUDWidgetClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[FA] AFAHUD: HUDWidgetClass 가 비어 있어 HUD 를 만들지 않았다. BP_FAHUD 에서 WBP_FAHUD 를 지정할 것."));
		return;
	}

	HUDWidget = CreateWidget<UFAHUDWidget>(PlayerOwner, HUDWidgetClass);
	if (HUDWidget)
	{
		HUDWidget->AddToViewport(HUDWidgetZOrder);
	}

	CreateRuleSlotsWidget();
	if (DetailsWidgetClass)
	{
		DetailsWidget = CreateWidget<UFAHUDDetailsWidget>(PlayerOwner, DetailsWidgetClass);
		if (DetailsWidget)
		{
			DetailsWidget->AddToPlayerScreen(DetailsWidgetZOrder);
		}
	}
}

void AFAHUD::CreateRuleSlotsWidget()
{
	// 비워두는 것도 정상이다 — WBP_FAHUD 안에 룰 칸을 직접 넣은 구성.
	if (!RuleSlotsWidgetClass)
	{
		return;
	}

	RuleSlotsWidget = CreateWidget<UFARuleSlotsWidget>(PlayerOwner, RuleSlotsWidgetClass);
	if (RuleSlotsWidget)
	{
		RuleSlotsWidget->AddToViewport(RuleSlotsZOrder);
	}
}

void AFAHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (DetailsWidget)
	{
		DetailsWidget->RemoveFromParent();
		DetailsWidget = nullptr;
	}
	if (HUDWidget)
	{
		HUDWidget->RemoveFromParent();
		HUDWidget = nullptr;
	}

	if (DraftWidget)
	{
		DraftWidget->RemoveFromParent();
		DraftWidget = nullptr;
	}

	if (RuleSlotsWidget)
	{
		RuleSlotsWidget->RemoveFromParent();
		RuleSlotsWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void AFAHUD::ShowDraftUI(const TArray<FName>& CandidateRuleIds, float SelectionTime)
{
	if (!PlayerOwner || !PlayerOwner->IsLocalController())
	{
		return;
	}

	if (CandidateRuleIds.Num() == 0)
	{
		// 해당 진영·티어에 뽑을 카드가 없었던 경우. 빈 창을 띄우지 않는다.
		UE_LOG(LogTemp, Warning, TEXT("[Draft] 후보가 0장이라 드래프트 UI 를 열지 않는다"));
		return;
	}

	if (!DraftWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Draft] AFAHUD: DraftWidgetClass 가 비어 있어 드래프트 UI 를 만들지 못했다"));
		return;
	}

	if (!DraftWidget)
	{
		DraftWidget = CreateWidget<UFADraftWidget>(PlayerOwner, DraftWidgetClass);
		if (!DraftWidget)
		{
			return;
		}
	}

	// 후보를 넘기기 전에 뷰포트에 올린다 — 위젯 트리는 AddToViewport 시점에 만들어진다.
	if (!DraftWidget->IsInViewport())
	{
		DraftWidget->AddToViewport(DraftWidgetZOrder);
	}

	DraftWidget->ShowCandidates(CandidateRuleIds, SelectionTime);
}

void AFAHUD::HideDraftUI()
{
	if (DraftWidget && DraftWidget->IsInViewport())
	{
		DraftWidget->CloseWithoutSelection();
	}
}

UFAHUDWidget* AFAHUD::GetHUDWidgetFor(APlayerController* PlayerController)
{
	const AFAHUD* FAHUD = PlayerController ? Cast<AFAHUD>(PlayerController->GetHUD()) : nullptr;
	return FAHUD ? FAHUD->GetHUDWidget() : nullptr;
}
