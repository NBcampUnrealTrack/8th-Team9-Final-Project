// Fill out your copyright notice in the Description page of Project Settings.

#include "Interact/FACabinet.h"
#include "AbilitySystemComponent.h"
#include "Components/SceneComponent.h"
#include "GAS/FAGameplayTags.h"
#include "Player/FACharacter.h"
#include "TimerManager.h"

AFACabinet::AFACabinet()
{
	// 안에 사람이 서는 자리.
	HideSpot = CreateDefaultSubobject<USceneComponent>(TEXT("HideSpot"));
	HideSpot->SetupAttachment(InteractRoot);
	HideSpot->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));

	// 숨는 동안 카메라가 올라앉을 자리. 캐비넷 천장보다 조금 위.
	HideCameraSpot = CreateDefaultSubobject<USceneComponent>(TEXT("HideCameraSpot"));
	HideCameraSpot->SetupAttachment(InteractRoot);
	HideCameraSpot->SetRelativeLocation(FVector(0.0f, 0.0f, 230.0f));

	// 나올 자리는 캐비넷 앞(+X).
	ExitSpot = CreateDefaultSubobject<USceneComponent>(TEXT("ExitSpot"));
	ExitSpot->SetupAttachment(InteractRoot);
	ExitSpot->SetRelativeLocation(FVector(120.0f, 0.0f, 90.0f));

	// 한 번에 한 명만 들어간다.
	bExclusiveInteraction = true;

	// 캐비넷은 벽에 붙어 있어 여는 방향이 정해져 있다. 미는 사람 위치로 방향을 뒤집으면
	// 벽 쪽으로 열리는 경우가 생긴다.
	bOpenAwayFromInteractor = false;

	// 열림 방향은 BeginPlay 에서 실제 배치를 보고 정한다(ComputeOutwardOpenSign).
	// 스케일 부호로 짐작하는 문의 보정은 그 계산과 두 번 겹쳐 서로를 상쇄
	bAutoFlipForMirroredScale = false;

	// 몸을 넣는 물건이라 문보다 가까이서만 잡히게 한다.
	InteractRange = 200.0f;
	InteractFocusOffset = FVector(0.0f, 0.0f, 90.0f);

	// 사물함은 문과 다른 소리를 낸다. 잠금 경로를 안 타므로 LockedSoundId 는 비워둔다.
	OpenSoundId = TEXT("Cabinet_Open");
	CloseSoundId = TEXT("Cabinet_Close");
	LockedSoundId = NAME_None;

	OpenPromptText = NSLOCTEXT("FallingApart", "CabinetOpen", "열어보기");
	ClosePromptText = NSLOCTEXT("FallingApart", "CabinetClose", "닫기");
	HidePromptText = NSLOCTEXT("FallingApart", "CabinetHide", "숨기");
	ExitPromptText = NSLOCTEXT("FallingApart", "CabinetExit", "나오기");
}

int32 AFACabinet::ComputeOutwardOpenSign() const
{
	if (!HingePivot || !DoorMesh)
	{
		return 1;
	}

	// 문짝 중심을 경첩 로컬 좌표로 바꿔둔다.
	const FVector PanelInHinge = HingePivot->GetComponentTransform().InverseTransformPosition(DoorMesh->Bounds.Origin);
	if (PanelInHinge.SizeSquared() < 1.0f)
	{
		// 경첩과 문짝 중심이 겹친다.
		return 1;
	}

	const USceneComponent* HingeParent = HingePivot->GetAttachParent();
	const FTransform ParentToWorld = HingeParent ? HingeParent->GetComponentTransform() : GetActorTransform();

	const FRotator ClosedRelative = HingePivot->GetRelativeRotation();
	const FVector Forward = GetActorForwardVector();
	const FVector Origin = GetActorLocation();

	int32 BestSign = 1;
	float BestReach = -UE_MAX_FLT;

	for (const int32 Sign : { 1, -1 })
	{
		const FTransform Candidate(
			ClosedRelative + FRotator(0.0f, OpenAngleDegrees * Sign, 0.0f),
			HingePivot->GetRelativeLocation(),
			HingePivot->GetRelativeScale3D());

		const FVector PanelWorld = (Candidate * ParentToWorld).TransformPosition(PanelInHinge);
		const float Reach = FVector::DotProduct(PanelWorld - Origin, Forward);

		if (Reach > BestReach)
		{
			BestReach = Reach;
			BestSign = Sign;
		}
	}

	return BestSign;
}

void AFACabinet::BeginPlay()
{
	if (bAutoOpenOutward)
	{
		OpenDirectionSign = ComputeOutwardOpenSign();
	}

	Super::BeginPlay();

	if (HasAuthority() && ExitSpot && ExitSpot->GetRelativeLocation().IsNearlyZero())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Cabinet] %s 의 ExitSpot 이 액터 원점에 있다 — 나올 때 캐비넷 안에 그대로 서게 된다."),
			*GetName());
	}
}

void AFACabinet::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(AutoCloseTimerHandle);

	Super::EndPlay(EndPlayReason);
}

bool AFACabinet::IsZombie(const AFACharacter* Character) const
{
	if (!Character)
	{
		return false;
	}

	const UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	return ASC && ASC->HasMatchingGameplayTag(FAGameplayTags::Get().State_Zombie);
}

FVector AFACabinet::GetHideLocation() const
{
	return HideSpot ? HideSpot->GetComponentLocation() : GetActorLocation();
}

FRotator AFACabinet::GetHideRotation() const
{
	// 캐비넷 정면(+X) 바깥을 본다
	return GetActorForwardVector().Rotation();
}

FVector AFACabinet::GetExitLocation() const
{
	return ExitSpot ? ExitSpot->GetComponentLocation() : GetActorLocation() + GetActorForwardVector() * 120.0f;
}

FRotator AFACabinet::GetExitRotation() const
{
	return GetActorForwardVector().Rotation();
}

bool AFACabinet::CanInteract_Implementation(AFACharacter* Interactor) const
{
	if (!bInteractEnabled || !Interactor)
	{
		return false;
	}

	if (!IsFactionAllowed(Interactor))
	{
		return false;
	}

	// 안에 있는 본인은 언제나 나올 수 있다.
	if (Interactor->GetHidingCabinet() == this)
	{
		return true;
	}

	// 다른 캐비넷에 들어가 있거나 사다리에 붙어 있으면 먼저 그쪽을 정리
	if (Interactor->IsHiding() || Interactor->IsClimbing())
	{
		return false;
	}

	if (IsZombie(Interactor))
	{
		// 좀비가 할 수 있는 것은 '여는 것' 하나
		return !bIsOpen;
	}

	// 인간은 빈 캐비넷에만 들어간다.
	return InteractionOccupant == nullptr;
}

FText AFACabinet::GetInteractPrompt_Implementation(AFACharacter* Interactor) const
{
	if (!Interactor)
	{
		return HidePromptText;
	}

	if (Interactor->GetHidingCabinet() == this)
	{
		return ExitPromptText;
	}

	return IsZombie(Interactor) ? OpenPromptText : HidePromptText;
}

float AFACabinet::GetInteractHoldSeconds_Implementation(AFACharacter* Interactor) const
{
	if (!Interactor)
	{
		return 0.0f;
	}

	// 나오는 것은 언제나 즉시
	if (Interactor->GetHidingCabinet() == this)
	{
		return 0.0f;
	}

	return IsZombie(Interactor) ? ZombieOpenHoldSeconds : HideHoldSeconds;
}

bool AFACabinet::TryOccupyInteraction_Implementation(AFACharacter* Interactor)
{
	if (!HasAuthority() || !Interactor)
	{
		return false;
	}

	// 좀비는 문만 열고 지나간다.
	if (IsZombie(Interactor))
	{
		return true;
	}

	return Super::TryOccupyInteraction_Implementation(Interactor);
}

void AFACabinet::ReleaseInteraction_Implementation(AFACharacter* Interactor)
{
	AFACharacter* PreviousOccupant = InteractionOccupant;

	Super::ReleaseInteraction_Implementation(Interactor);

	if (!HasAuthority())
	{
		return;
	}

	// 밑판이 실제로 자리를 비웠을 때만 손댄다.
	if (InteractionOccupant == nullptr && IsValid(PreviousOccupant) && PreviousOccupant->GetHidingCabinet() == this)
	{
		PreviousOccupant->ServerStopHiding();
	}

	UpdateAutoCloseTimer();
}

void AFACabinet::Interact_Implementation(AFACharacter* Interactor)
{
	if (!HasAuthority() || !Interactor)
	{
		return;
	}

	// 1) 안에 있는 본인 -> 나오기
	if (Interactor->GetHidingCabinet() == this)
	{
		ServerEjectHider();
		AFAInteractableActor::Interact_Implementation(Interactor);
		return;
	}

	if (IsZombie(Interactor))
	{
		// 2) 좀비 -> 열어서 노출
		SetDoorOpen(true, Interactor);
		ServerEjectHider();
		UpdateAutoCloseTimer();
	}
	else
	{
		// 3) 인간 -> 들어가면서 문을 닫는다.
		if (InteractionOccupant && InteractionOccupant != Interactor)
		{
			return;
		}

		Interactor->ServerStartHiding(this);
		SetDoorOpen(false, Interactor);
		UpdateAutoCloseTimer();
	}

	AFAInteractableActor::Interact_Implementation(Interactor);
}

void AFACabinet::ServerEjectHider()
{
	if (!HasAuthority())
	{
		return;
	}

	AFACharacter* Hider = InteractionOccupant;
	if (!IsValid(Hider))
	{
		return;
	}

	// 문이 닫힌 채로 사람만 튀어나오는 그림은 없다.
	SetDoorOpen(true, nullptr);

	// 위치 복구와 점유 해제는 캐릭터 쪽에서 한다(사다리 ServerStopClimbing 과 같은 구조).
	Hider->ServerStopHiding();

	UpdateAutoCloseTimer();
}

void AFACabinet::UpdateAutoCloseTimer()
{
	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(AutoCloseTimerHandle);

	// 안에 사람이 있는 동안에는 닫히지 않는다 — 숨는 쪽이 문을 닫는 주체다.
	if (!bIsOpen || AutoCloseSeconds <= 0.0f || InteractionOccupant)
	{
		return;
	}

	GetWorldTimerManager().SetTimer(AutoCloseTimerHandle, this,
		&AFACabinet::HandleAutoClose, AutoCloseSeconds, false);
}

void AFACabinet::HandleAutoClose()
{
	if (!HasAuthority() || !bIsOpen || InteractionOccupant)
	{
		return;
	}

	SetDoorOpen(false, nullptr);
}
