#pragma once

#include "CoreMinimal.h"

/** H03/Z03: 서버에서 진행하는 이동 피로. 짧은 정지는 피로와 이동 누적을 지우지 않는다. */
struct FFAFatigueState
{
	float MovingTime = 0.0f;
	float RestTime = 0.0f;
	float Fatigue = 0.0f;
	float RecoveryStartFatigue = 0.0f;

	void Update(float DeltaTime, bool bMoving)
	{
		DeltaTime = FMath::Max(0.0f, DeltaTime);
		if (bMoving)
		{
			RestTime = 0.0f;
			const float PreviousTime = MovingTime;
			MovingTime += DeltaTime;
			const float FatigueTime = FMath::Max(0.0f, MovingTime - 6.0f)
				- FMath::Max(0.0f, PreviousTime - 6.0f);
			Fatigue = FMath::Clamp(Fatigue + FatigueTime / 4.0f, 0.0f, 1.0f);
			RecoveryStartFatigue = Fatigue;
		}
		else
		{
			RestTime += DeltaTime;
			if (RestTime >= 1.5f)
			{
				MovingTime = 0.0f;
				Fatigue = RecoveryStartFatigue * (1.0f - FMath::Clamp((RestTime - 1.5f) / 2.0f, 0.0f, 1.0f));
			}
		}
	}

	float GetSpeedMultiplier() const { return 1.0f - 0.25f * Fatigue; }
};
