// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/FACaptureMarkerWidget.h"

void UFACaptureMarkerWidget::UpdateMarker(float InfectionRatio, EFAContestRole ContestRole, float ContestGauge)
{
	const float ClampedInfection = FMath::Clamp(InfectionRatio, 0.0f, 1.0f);
	if (!FMath::IsNearlyEqual(ClampedInfection, LastInfectionRatio, 0.001f))
	{
		LastInfectionRatio = ClampedInfection;
		OnInfectionRatioChanged(ClampedInfection);
	}

	const bool bInContest = ContestRole != EFAContestRole::None;
	const float ClampedGauge = FMath::Clamp(ContestGauge, 0.0f, 1.0f);

	// QTE 가 붙고 떨어지는 순간은 게이지 값이 그대로여도 알려야 한다 — 바를 켜고 끄는 신호를 겸한다.
	if (!bHasContestBaseline
		|| bInContest != bLastInContest
		|| !FMath::IsNearlyEqual(ClampedGauge, LastContestGauge, 0.001f))
	{
		bHasContestBaseline = true;
		bLastInContest = bInContest;
		LastContestGauge = ClampedGauge;

		OnContestChanged(bInContest, ClampedGauge);
	}
}
