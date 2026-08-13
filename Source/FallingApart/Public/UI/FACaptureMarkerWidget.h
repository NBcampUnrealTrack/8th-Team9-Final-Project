// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Game/FAGameTypes.h"
#include "FACaptureMarkerWidget.generated.h"

/**
 * 잡힌 인간 머리 위에 뜨는 감염 게이지 (3D 위젯 베이스).
 */
UCLASS(Abstract)
class FALLINGAPART_API UFACaptureMarkerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 표시값 갱신. AFACharacter 가 매 프레임 부르지만 값이 실제로 바뀔 때만 이벤트가 나간다.
	 *
	 * @param InfectionRatio 감염 진행도 0~1. 1 이면 감염 확정
	 * @param ContestRole    이 캐릭터의 QTE 참가 역할. None 이면 줄다리기 중이 아니다
	 * @param ContestGauge   줄다리기 게이지 0~1 (1 이면 구출 성공 직전)
	 */
	void UpdateMarker(float InfectionRatio, EFAContestRole ContestRole, float ContestGauge);

	/** 감염 진행도가 바뀔 때 */
	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|Capture")
	void OnInfectionRatioChanged(float InfectionRatio);

	/**
	 * 줄다리기가 붙거나 떨어질 때, 그리고 게이지가 움직일 때.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "FallingApart|Capture")
	void OnContestChanged(bool bInContest, float ContestGauge);

private:
	//~ 직전 값. 음수면 아직 기준이 없다는 뜻이라 첫 갱신은 무조건 통과시킨다.
	float LastInfectionRatio = -1.0f;
	float LastContestGauge = -1.0f;

	bool bLastInContest = false;

	/** bLastInContest 는 false 가 기본값이라 '아직 안 정해짐'과 구분되지 않는다. */
	bool bHasContestBaseline = false;
};
