// Fill out your copyright notice in the Description page of Project Settings.

#include "Interact/FADoor.h"
#include "Audio/FAAudioSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Player/FACharacter.h"
#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

AFADoor::AFADoor()
{
	PrimaryActorTick.bCanEverTick = true;

	// 문틀은 InteractRoot 밑에 둔다. 문틀 메시를 비워도 액터 원점이 흔들리지 않는다.
	DoorFrame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorFrame"));
	DoorFrame->SetupAttachment(InteractRoot);
	DoorFrame->SetGenerateOverlapEvents(false);

	// 경첩. 여기를 문 회전축 위치로 옮겨두고, 이 컴포넌트를 돌린다.
	HingePivot = CreateDefaultSubobject<USceneComponent>(TEXT("HingePivot"));
	HingePivot->SetupAttachment(InteractRoot);
	HingePivot->SetMobility(EComponentMobility::Movable);

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(HingePivot);
	DoorMesh->SetGenerateOverlapEvents(false);
	// Static 이면 런타임에 아무리 회전을 넣어도 안 움직인다. 레벨에 배치된 벽 메시가 전부 Static 이라
	// 그걸 그대로 문에 쓰면 이 함정을 밟는다.
	DoorMesh->SetMobility(EComponentMobility::Movable);

	// 문은 여러 명이 동시에 여닫아도 상관없다.
	bExclusiveInteraction = false;

	// 손잡이 쪽을 조준 기준점으로 잡는 것이 자연스럽다. 배치에 맞춰 에디터에서 조정할 것.
	InteractFocusOffset = FVector(0.0f, 0.0f, 100.0f);

	OpenPromptText = NSLOCTEXT("FallingApart", "DoorOpen", "문 열기");
	ClosePromptText = NSLOCTEXT("FallingApart", "DoorClose", "문 닫기");
	LockedPromptText = NSLOCTEXT("FallingApart", "DoorLocked", "잠겨 있다");
}

void AFADoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFADoor, bIsOpen);
	DOREPLIFETIME(AFADoor, bIsLocked);
	DOREPLIFETIME(AFADoor, OpenDirectionSign);
}

void AFADoor::BeginPlay()
{
	Super::BeginPlay();

	//   배치된 그대로의 회전을 기준으로 삼는다.
	ClosedPivotRotation = HingePivot->GetRelativeRotation();
	TargetPivotRotation = ClosedPivotRotation;
	bRotationInitialized = true;

	if (HasAuthority())
	{
		bIsOpen = bStartOpen;
		bIsLocked = bStartLocked;
	}

	if (bIsOpen)
	{
		// 시작부터 열린 문은 애니메이션 없이 그 자리에 둔다.
		TargetPivotRotation = GetTargetPivotRotation();
		HingePivot->SetRelativeRotation(TargetPivotRotation);
	}

	bLastSoundedOpenState = bIsOpen;
	SavedPawnCollisionResponse = DoorMesh->GetCollisionResponseToChannel(ECC_Pawn);
}

void AFADoor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bRotationInitialized)
	{
		return;
	}

	const FRotator Current = HingePivot->GetRelativeRotation();
	const bool bMoving = !Current.Equals(TargetPivotRotation, 0.05f);

	if (bMoving)
	{
		// 각속도 고정 보간.
		const FRotator Next = FMath::RInterpConstantTo(Current, TargetPivotRotation, DeltaTime, OpenSpeedDegreesPerSecond);
		HingePivot->SetRelativeRotation(Next);
	}

	// 멈춘 뒤에도 충돌을 되살릴 때까지는 계속 봐야 하므로 위에서 일찍 빠져나가지 않는다.
	UpdatePawnCollisionForMotion(DeltaTime, bMoving);
}

void AFADoor::UpdatePawnCollisionForMotion(float DeltaTime, bool bMoving)
{
	if (!bDisablePawnCollisionWhileMoving)
	{
		// 도중에 옵션을 꺼도 문이 통과 상태로 남지 않게 한다.
		SetPawnCollisionSuppressed(false);
		return;
	}

	if (bMoving)
	{
		SetPawnCollisionSuppressed(true);
		PawnCollisionSuppressedSeconds = 0.0f;
		return;
	}

	if (!bPawnCollisionSuppressed)
	{
		return;
	}

	PawnCollisionSuppressedSeconds += DeltaTime;
	if (PawnCollisionSuppressedSeconds < PawnCollisionRestoreGraceSeconds && IsPawnOverlappingDoorMesh())
	{
		return;
	}

	SetPawnCollisionSuppressed(false);
}

void AFADoor::SetPawnCollisionSuppressed(bool bSuppress)
{
	if (bPawnCollisionSuppressed == bSuppress || !DoorMesh)
	{
		return;
	}

	bPawnCollisionSuppressed = bSuppress;
	PawnCollisionSuppressedSeconds = 0.0f;

	// Pawn 채널만 손댄다.
	DoorMesh->SetCollisionResponseToChannel(ECC_Pawn, bSuppress ? ECR_Ignore : SavedPawnCollisionResponse.GetValue());
}

bool AFADoor::IsPawnOverlappingDoorMesh() const
{
	const UWorld* World = GetWorld();
	if (!World || !DoorMesh)
	{
		return false;
	}

	const FBoxSphereBounds MeshBounds = DoorMesh->Bounds;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FADoorPawnClearance), /*bTraceComplex=*/false, this);
	FCollisionObjectQueryParams ObjectParams(ECC_Pawn);

	TArray<FOverlapResult> Overlaps;
	return World->OverlapMultiByObjectType(
		Overlaps,
		MeshBounds.Origin,
		FQuat::Identity,
		ObjectParams,
		FCollisionShape::MakeBox(MeshBounds.BoxExtent),
		Params);
}

float AFADoor::GetMirrorSign() const
{
	if (!bAutoFlipForMirroredScale)
	{
		return 1.0f;
	}

	// 거울 배치는 스케일 성분 하나가 음수다. 세 축의 곱이 음수면 뒤집힌 배치로 본다.
	const FVector Scale = GetActorScale3D();
	return (Scale.X * Scale.Y * Scale.Z) < 0.0f ? -1.0f : 1.0f;
}

FRotator AFADoor::GetTargetPivotRotation() const
{
	if (!bIsOpen)
	{
		return ClosedPivotRotation;
	}

	const float Sign = static_cast<float>(OpenDirectionSign) * GetMirrorSign() * (bInvertOpenDirection ? -1.0f : 1.0f);
	return ClosedPivotRotation + FRotator(0.0f, OpenAngleDegrees * Sign, 0.0f);
}

FText AFADoor::GetInteractPrompt_Implementation(AFACharacter* Interactor) const
{
	if (bIsLocked)
	{
		return LockedPromptText;
	}

	return bIsOpen ? ClosePromptText : OpenPromptText;
}

void AFADoor::Interact_Implementation(AFACharacter* Interactor)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bIsLocked)
	{
		if (!LockedSoundId.IsNone())
		{
			if (UFAAudioSubsystem* Audio = UFAAudioSubsystem::Get(this))
			{
				Audio->PlaySoundAtLocation(LockedSoundId, GetActorLocation());
			}
		}
		return;
	}

	const bool bOpening = !bIsOpen;
	MulticastPlayDoorInteractionMontage(Interactor, bOpening);
	ToggleDoor(Interactor);

	// BP 훅(OnInteracted)까지 살려둔다.
	Super::Interact_Implementation(Interactor);
}

void AFADoor::MulticastPlayDoorInteractionMontage_Implementation(AFACharacter* Interactor, bool bOpening)
{
	if (!IsValid(Interactor))
	{
		return;
	}

	UAnimMontage* MontageToPlay = bOpening ? DoorOpenMontage : DoorCloseMontage;
	if (MontageToPlay)
	{
		Interactor->PlayAnimMontage(MontageToPlay);
	}
}

void AFADoor::ToggleDoor(AFACharacter* ByCharacter)
{
	SetDoorOpen(!bIsOpen, ByCharacter);
}

void AFADoor::SetDoorOpen(bool bOpen, AFACharacter* ByCharacter)
{
	if (!HasAuthority() || bIsOpen == bOpen)
	{
		return;
	}

	if (bOpen && bOpenAwayFromInteractor && ByCharacter)
	{
		// 미는 사람이 문 앞쪽(액터 +X)에 있으면 뒤로, 뒤쪽에 있으면 앞으로 연다.
		const FVector ToInstigator = ByCharacter->GetActorLocation() - GetActorLocation();
		const float Side = FVector::DotProduct(GetActorForwardVector(), ToInstigator);
		OpenDirectionSign = (Side >= 0.0f) ? 1 : -1;
	}

	bIsOpen = bOpen;

	// 서버는 OnRep 이 안 불리므로 직접 같은 경로를 탄다.
	ApplyOpenState(/*bPlaySound=*/true);
}

void AFADoor::SetLocked(bool bLock)
{
	if (!HasAuthority() || bIsLocked == bLock)
	{
		return;
	}

	bIsLocked = bLock;
	OnRep_IsLocked();
}

void AFADoor::OnRep_IsOpen()
{
	ApplyOpenState(/*bPlaySound=*/true);
}

void AFADoor::OnRep_IsLocked()
{
	// 지금은 표시 문구만 바뀐다. 잠금 표시 연출이 붙으면 여기서 한다.
}

void AFADoor::ApplyOpenState(bool bPlaySound)
{
	if (!bRotationInitialized)
	{
		// BeginPlay 보다 복제가 먼저 도착할 수 있다. 기준 회전이 잡힌 뒤에 BeginPlay 가 맞춰준다.
		return;
	}

	TargetPivotRotation = GetTargetPivotRotation();

	if (!bPlaySound || bIsOpen == bLastSoundedOpenState)
	{
		return;
	}

	bLastSoundedOpenState = bIsOpen;

	// 접속 직후 초기 복제로 상태만 맞춰지는 경우에는 소리를 내지 않는다.
	if (GetGameTimeSinceCreation() < 1.0f)
	{
		return;
	}

	const FName SoundId = bIsOpen ? OpenSoundId : CloseSoundId;
	if (SoundId.IsNone())
	{
		return;
	}

	if (UFAAudioSubsystem* Audio = UFAAudioSubsystem::Get(this))
	{
		Audio->PlaySoundAtLocation(SoundId, GetActorLocation());
	}
}
