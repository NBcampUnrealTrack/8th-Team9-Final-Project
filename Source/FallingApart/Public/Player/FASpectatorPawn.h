// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpectatorPawn.h"
#include "FASpectatorPawn.generated.h"

class UFAVoiceComponent;

/**
 * 관전자 Pawn.
 *
 * 관전자 보이스는 세 조각으로 나뉘고, 이 클래스는 그중 마이크 제어만 담당.
 *   1) 관전자끼리 들리게
 *   2) 관전자 <-> 생존 플레이어 격리 — AFAGameMode::UpdateVoiceMuteMatrix() 담당.
 *   3) 마이크 송신 제어(푸시투토크 등) — 이 Pawn 의 UFAVoiceComponent 담당.
 */
UCLASS()
class FALLINGAPART_API AFASpectatorPawn : public ASpectatorPawn
{
	GENERATED_BODY()

public:
	AFASpectatorPawn();

	UFAVoiceComponent* GetVoiceComponent() const { return VoiceComponent; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FallingApart|Voice", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFAVoiceComponent> VoiceComponent;
};
