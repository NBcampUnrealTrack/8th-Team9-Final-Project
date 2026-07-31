// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "FAVoiceTransport.generated.h"

class APlayerController;

/**
 * 음성 전송 계층 래퍼.
 *
 * 근접 보이스의 3D 정위·감쇠·발화 표시는 전부 UFAVoiceComponent 가 처리,
 * 마이크 데이터 보내는 방식 만.
 * 서버 완료 시 config(DefaultPlatformService=Steam) 바꿈
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class FALLINGAPART_API UFAVoiceTransport : public UObject
{
	GENERATED_BODY()

public:
	/** 지금 이 컨트롤러로 음성을 송신할 수 있는 상태인가. */
	virtual bool IsReadyToTalk(const APlayerController* OwningController) const;

	/** 로컬 마이크 송신을 켜고 끔. */
	virtual void SetLocalTalking(APlayerController* OwningController, bool bTalking);

	/** 로그/설정 UI 표시용 백엔드 이름 (NULL, Steam …). */
	virtual FName GetBackendName() const;

	/** 월드가 내려갈 때 전송 계층이 만들어 둔 것들을 정리한다. 라운드 리스폰에서는 호출하지 않는다. */
	virtual void ShutdownTransport(APlayerController* OwningController);
};

/**
 * 언리얼 내장 VoIP 전송. OnlineSubsystem 의 VoiceInterface 
 */
UCLASS()
class FALLINGAPART_API UFAEngineVoiceTransport : public UFAVoiceTransport
{
	GENERATED_BODY()

public:
	virtual bool IsReadyToTalk(const APlayerController* OwningController) const override;
	virtual void SetLocalTalking(APlayerController* OwningController, bool bTalking) override;
	virtual FName GetBackendName() const override;
	virtual void ShutdownTransport(APlayerController* OwningController) override;

private:
	void EnsureVoiceSessionExists(APlayerController* OwningController) const;
};
