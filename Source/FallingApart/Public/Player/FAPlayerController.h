// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "FAPlayerController.generated.h"

class UInputMappingContext;

UCLASS()
class FALLINGAPART_API AFAPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AFAPlayerController();

	/**
	 * 30초 주기를 기다리지 않고 드래프트를 즉시 한 번 돌린다. 콘솔에 `FADraft 3` 처럼 입력.
	 * 서버(PIE 단독·리슨 서버 호스트)에서만 동작한다 — 룰 적용은 서버 권한이다.
	 * @param DraftIndex 회차(1~6). 회차에 따라 후보 티어 구성이 달라진다 (1회차 T1T1T1 ~ 6회차 T3T3T3)
	 */
	UFUNCTION(Exec)
	void FADraft(int32 DraftIndex = 1);

	/** 내 캐릭터 ASC 의 보유 룰·게임플레이 태그를 콘솔에 찍는다. 룰이 실제로 붙었는지 확인용. */
	UFUNCTION(Exec)
	void FATags();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;
};
