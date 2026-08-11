// Fill out your copyright notice in the Description page of Project Settings.

#include "Audio/FAAmbienceSource.h"

#include "Audio/FAAudioSubsystem.h"
#include "Components/AudioComponent.h"
#include "Components/BillboardComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

AFAAmbienceSource::AFAAmbienceSource()
{
	PrimaryActorTick.bCanEverTick = false;

	// 소리만 내는 액터라 복제할 것이 없다 — 각 클라이언트가 자기 월드에서 로컬로 재생한다.
	bReplicates = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

#if WITH_EDITORONLY_DATA
	// 에디터에서 눈에 보여야 배치·이동이 가능하다.
	if (UBillboardComponent* Sprite = CreateDefaultSubobject<UBillboardComponent>(TEXT("Sprite")))
	{
		Sprite->SetupAttachment(Root);
		Sprite->bIsScreenSizeScaled = true;
	}
#endif
}

void AFAAmbienceSource::BeginPlay()
{
	Super::BeginPlay();

	if (StartDelay > 0.0f)
	{
		GetWorldTimerManager().SetTimer(StartTimerHandle, this, &AFAAmbienceSource::StartAmbience, StartDelay, false);
		return;
	}

	StartAmbience();
}

void AFAAmbienceSource::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(StartTimerHandle);

	// bAutoDestroy 를 꺼둔 컴포넌트라 직접 멈춰야 한다.
	if (AmbienceComponent)
	{
		AmbienceComponent->Stop();
		AmbienceComponent = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void AFAAmbienceSource::StartAmbience()
{
	if (SoundId.IsNone())
	{
		return;
	}

	// 전용 서버에서는 서브시스템이 없다.
	UFAAudioSubsystem* Audio = UFAAudioSubsystem::Get(this);
	if (!Audio)
	{
		return;
	}

	AmbienceComponent = Audio->PlayLoopAtLocation(SoundId, GetActorLocation());
}

void AFAAmbienceSource::SetAmbienceSound(FName NewSoundId)
{
	if (SoundId == NewSoundId)
	{
		return;
	}

	if (AmbienceComponent)
	{
		AmbienceComponent->Stop();
		AmbienceComponent = nullptr;
	}

	SoundId = NewSoundId;
	StartAmbience();
}
