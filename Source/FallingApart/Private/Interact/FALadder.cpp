// Fill out your copyright notice in the Description page of Project Settings.

#include "Interact/FALadder.h"
#include "AbilitySystemComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GAS/FAGameplayTags.h"
#include "Player/FACharacter.h"

AFALadder::AFALadder()
{
	PrimaryActorTick.bCanEverTick = false;

	LadderMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LadderMesh"));
	LadderMesh->SetupAttachment(InteractRoot);
	LadderMesh->SetGenerateOverlapEvents(false);

	ClimbBottom = CreateDefaultSubobject<USceneComponent>(TEXT("ClimbBottom"));
	ClimbBottom->SetupAttachment(InteractRoot);

	ClimbTop = CreateDefaultSubobject<USceneComponent>(TEXT("ClimbTop"));
	ClimbTop->SetupAttachment(InteractRoot);
	// 기본값은 3m 사다리. 배치할 때 메시 길이에 맞춰 옮긴다.
	ClimbTop->SetRelativeLocation(FVector(0.0f, 0.0f, 300.0f));

	TopExit = CreateDefaultSubobject<USceneComponent>(TEXT("TopExit"));
	TopExit->SetupAttachment(InteractRoot);
	// 꼭대기보다 조금 위, 그리고 벽 반대쪽(+X)으로 나온 자리.
	TopExit->SetRelativeLocation(FVector(80.0f, 0.0f, 340.0f));

	// 한 번에 한 명만 오른다.
	bExclusiveInteraction = true;

	// 조준 기준점은 사다리 중간 높이.
	InteractFocusOffset = FVector(0.0f, 0.0f, 150.0f);

	// 사다리는 몸을 붙여야 하니 문보다 가까이서만 잡히게 한다.
	InteractRange = 180.0f;

	ClimbPromptText = NSLOCTEXT("FallingApart", "LadderClimb", "오르기");
	DismountPromptText = NSLOCTEXT("FallingApart", "LadderDismount", "내려오기");
	NoClimbPromptText = NSLOCTEXT("FallingApart", "LadderNoClimb", "오를 수 없다");
}

FVector AFALadder::GetClimbBottomLocation() const
{
	return ClimbBottom->GetComponentLocation();
}

FVector AFALadder::GetClimbTopLocation() const
{
	return ClimbTop->GetComponentLocation();
}

FVector AFALadder::GetClimbUpVector() const
{
	const FVector Delta = GetClimbTopLocation() - GetClimbBottomLocation();
	// 두 점이 겹쳐 있으면(배치 실수) 액터 위쪽으로 폴백해 최소한 오르기는 되게 한다.
	return Delta.IsNearlyZero() ? GetActorUpVector() : Delta.GetSafeNormal();
}

float AFALadder::GetClimbLength() const
{
	return FVector::Dist(GetClimbBottomLocation(), GetClimbTopLocation());
}

float AFALadder::GetClimbAlpha(const FVector& WorldLocation) const
{
	const float Length = GetClimbLength();
	if (Length <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	// 등반축에 투영한 길이 비율. 옆으로 벌어진 성분은 무시한다.
	const FVector Bottom = GetClimbBottomLocation();
	return FVector::DotProduct(WorldLocation - Bottom, GetClimbUpVector()) / Length;
}

FVector AFALadder::GetAttachLocation(const FVector& WorldLocation) const
{
	const FVector Bottom = GetClimbBottomLocation();
	const float Alpha = FMath::Clamp(GetClimbAlpha(WorldLocation), 0.0f, 1.0f);

	// 중심선 위의 같은 높이 지점에서 벽 반대쪽으로 캡슐 반지름만큼 띄운다.
	return Bottom + GetClimbUpVector() * (Alpha * GetClimbLength())
		+ GetActorForwardVector() * AttachForwardOffset;
}

FRotator AFALadder::GetClimbFacingRotation() const
{
	// 액터 정면(+X)이 사람이 서는 쪽이므로, 캐릭터는 그 반대를 본다.
	return (-GetActorForwardVector()).Rotation();
}

FVector AFALadder::GetTopExitLocation() const
{
	return TopExit->GetComponentLocation();
}

bool AFALadder::CanInteract_Implementation(AFACharacter* Interactor) const
{
	if (!Super::CanInteract_Implementation(Interactor))
	{
		return false;
	}

	if (!Interactor)
	{
		return false;
	}

	// 이미 이 사다리를 타고 있으면 "내려오기" 로 다시 쓸 수 있어야 한다.
	if (Interactor->GetClimbingLadder() == this)
	{
		return true;
	}

	// 남의 사다리를 타는 중이면 먼저 내려야 한다.
	if (Interactor->IsClimbing())
	{
		return false;
	}

	// Z13/H12 등반 불가 룰.
	if (const UAbilitySystemComponent* ASC = Interactor->GetAbilitySystemComponent())
	{
		if (ASC->HasMatchingGameplayTag(FAGameplayTags::Get().Rule_Move_NoMantle))
		{
			return false;
		}
	}

	return true;
}

FText AFALadder::GetInteractPrompt_Implementation(AFACharacter* Interactor) const
{
	if (Interactor && Interactor->GetClimbingLadder() == this)
	{
		return DismountPromptText;
	}

	if (Interactor)
	{
		if (const UAbilitySystemComponent* ASC = Interactor->GetAbilitySystemComponent())
		{
			if (ASC->HasMatchingGameplayTag(FAGameplayTags::Get().Rule_Move_NoMantle))
			{
				return NoClimbPromptText;
			}
		}
	}

	return ClimbPromptText;
}

void AFALadder::Interact_Implementation(AFACharacter* Interactor)
{
	if (!HasAuthority() || !Interactor)
	{
		return;
	}

	if (Interactor->GetClimbingLadder() == this)
	{
		// 같은 사다리를 다시 누르면 그 자리에서 내린다(위로 올라서지는 않는다).
		Interactor->ServerStopClimbing(/*bLaunchOverLedge=*/false);
	}
	else
	{
		Interactor->ServerStartClimbing(this);
	}

	Super::Interact_Implementation(Interactor);
}
