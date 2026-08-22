// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/FALobbyReadyZone.h"

#include "Components/BoxComponent.h"
#include "Game/FALobbyGameMode.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

AFALobbyReadyZone::AFALobbyReadyZone()
{
	PrimaryActorTick.bCanEverTick = false;

	// 복제할 상태가 없다. 레벨 배치 액터라 클라이언트에도 이미 존재하고,
	// 판정은 서버 오버랩만 쓴다.
	bReplicates = false;

	ZoneVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("ZoneVolume"));
	SetRootComponent(ZoneVolume);

	ZoneVolume->SetBoxExtent(FVector(300.f, 300.f, 150.f));
	ZoneVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ZoneVolume->SetCollisionObjectType(ECC_WorldStatic);
	ZoneVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	ZoneVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ZoneVolume->SetGenerateOverlapEvents(true);
}

void AFALobbyReadyZone::BeginPlay()
{
	Super::BeginPlay();

	// 클라이언트에서는 오버랩을 아예 듣지 않는다. Ready 판정은 서버만 한다.
	if (!HasAuthority())
	{
		return;
	}

	ZoneVolume->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleBeginOverlap);
	ZoneVolume->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleEndOverlap);

	// BeginPlay 이전에 이미 겹쳐 있던 Pawn 은 BeginOverlap 이 안 온다.
	// 레벨 시작 위치가 구역 안이거나, 매치에서 로비로 돌아온 직후가 이 경우다.
	TArray<AActor*> AlreadyInside;
	ZoneVolume->GetOverlappingActors(AlreadyInside, APawn::StaticClass());
	for (AActor* Actor : AlreadyInside)
	{
		HandleBeginOverlap(ZoneVolume, Actor, nullptr, 0, false, FHitResult());
	}
}

void AFALobbyReadyZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		for (TPair<TWeakObjectPtr<APawn>, FZoneOccupant>& Pair : Occupants)
		{
			World->GetTimerManager().ClearTimer(Pair.Value.DwellTimer);
		}
	}
	Occupants.Empty();

	Super::EndPlay(EndPlayReason);
}

void AFALobbyReadyZone::HandleBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	APlayerController* PlayerController = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PlayerController)
	{
		return;
	}

	// 캡슐과 메시가 각각 오버랩을 쏘는 등 같은 Pawn 이 두 번 들어오는 경우를 접는다.
	if (Occupants.Contains(Pawn))
	{
		return;
	}

	FZoneOccupant& Occupant = Occupants.Add(Pawn);
	Occupant.Controller = PlayerController;

	if (ReadyDwellSeconds <= 0.f)
	{
		ApplyReady(PlayerController, true);
		return;
	}

	GetWorldTimerManager().SetTimer(
		Occupant.DwellTimer,
		FTimerDelegate::CreateUObject(this, &ThisClass::HandleDwellElapsed, TWeakObjectPtr<APawn>(Pawn)),
		ReadyDwellSeconds,
		false);
}

void AFALobbyReadyZone::HandleEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn)
	{
		return;
	}

	FZoneOccupant Occupant;
	if (!Occupants.RemoveAndCopyValue(Pawn, Occupant))
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(Occupant.DwellTimer);

	// 여기서 GetController() 를 다시 묻지 않는다 — Pawn 이 파괴되는 중이면 이미 null 이다.
	ApplyReady(Occupant.Controller.Get(), false);
}

void AFALobbyReadyZone::HandleDwellElapsed(TWeakObjectPtr<APawn> WeakPawn)
{
	const FZoneOccupant* Occupant = Occupants.Find(WeakPawn);
	if (!Occupant)
	{
		return;
	}

	ApplyReady(Occupant->Controller.Get(), true);
}

void AFALobbyReadyZone::ApplyReady(APlayerController* PlayerController, bool bReady)
{
	if (!HasAuthority() || !PlayerController)
	{
		return;
	}

	UWorld* World = GetWorld();
	AFALobbyGameMode* LobbyGameMode = World ? World->GetAuthGameMode<AFALobbyGameMode>() : nullptr;
	if (!LobbyGameMode)
	{
		return;
	}

	// 인원·관전자·시작 잠금 검증은 전부 GameMode 안에 있다. 여기서 다시 하지 않는다.
	LobbyGameMode->SetPlayerReady(PlayerController, bReady);
}
