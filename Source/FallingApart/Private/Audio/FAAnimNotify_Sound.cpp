// Fill out your copyright notice in the Description page of Project Settings.

#include "Audio/FAAnimNotify_Sound.h"

#include "Audio/FAAudioSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Player/FAPlayerState.h"

namespace FAFootstepAudio
{
	static const FName HumanWalk(TEXT("Foot_Human_Walk"));
	static const FName HumanRun(TEXT("Foot_Human_Run"));
	static const FName HumanCrouch(TEXT("Foot_Human_Crouch"));
	static const FName ZombieWalk(TEXT("Foot_Zombie_Walk"));
	static const FName ZombieRun(TEXT("Foot_Zombie_Run"));

	/** 소켓이 지정돼 있으면 그 위치, 아니면 액터 위치. */
	FVector ResolveLocation(const USkeletalMeshComponent* MeshComp, const FName& Socket)
	{
		if (!Socket.IsNone() && MeshComp->DoesSocketExist(Socket))
		{
			return MeshComp->GetSocketLocation(Socket);
		}

		const AActor* Owner = MeshComp->GetOwner();
		return Owner ? Owner->GetActorLocation() : MeshComp->GetComponentLocation();
	}
}

//~ ---------------- 범용 ----------------

UFAAnimNotify_Sound::UFAAnimNotify_Sound()
{
#if WITH_EDITORONLY_DATA
	// 노티파이 트랙에서 사운드 계열임을 색으로 구분.
	NotifyColor = FColor(120, 200, 210);
#endif
}

FString UFAAnimNotify_Sound::GetNotifyName_Implementation() const
{
	return SoundId.IsNone() ? TEXT("FA Sound (미지정)") : SoundId.ToString();
}

void UFAAnimNotify_Sound::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || SoundId.IsNone())
	{
		return;
	}

	// 전용 서버에서는 서브시스템이 아예 생성되지 않으므로 여기서 null 이 나온다.
	if (UFAAudioSubsystem* Audio = UFAAudioSubsystem::Get(MeshComp))
	{
		Audio->PlaySoundAtLocation(SoundId, FAFootstepAudio::ResolveLocation(MeshComp, Socket));
	}
}

//~ ---------------- 발소리 ----------------

UFAAnimNotify_Footstep::UFAAnimNotify_Footstep()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(200, 130, 120);
#endif
}

FString UFAAnimNotify_Footstep::GetNotifyName_Implementation() const
{
	switch (Gait)
	{
	case EFAFootstepGait::Run:    return TEXT("Footstep (Run)");
	case EFAFootstepGait::Crouch: return TEXT("Footstep (Crouch)");
	default:                      return TEXT("Footstep (Walk)");
	}
}

void UFAAnimNotify_Footstep::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	UFAAudioSubsystem* Audio = UFAAudioSubsystem::Get(MeshComp);
	if (!Audio)
	{
		return;
	}

	// 진영은 PlayerState 가 들고 있다. 시뮬레이티드 프록시에도 복제돼 있으므로 남의 캐릭터에서도 읽힌다.
	const ACharacter* Character = Cast<ACharacter>(MeshComp->GetOwner());
	const AFAPlayerState* FAPlayerState = Character ? Character->GetPlayerState<AFAPlayerState>() : nullptr;
	const bool bIsZombie = FAPlayerState && FAPlayerState->IsZombie();

	// 웅크림은 애니메이션이 아니라 실제 상태를 본다 — 같은 애니에 노티파이가 하나만 있어도 맞는다.
	const bool bCrouching = (Gait == EFAFootstepGait::Crouch) || (Character && Character->bIsCrouched);

	FName SoundId;
	if (bIsZombie)
	{
		// 좀비용 웅크림 행은 두지 않았다 (표 5행 확정) — 걷기로 폴백.
		SoundId = (Gait == EFAFootstepGait::Run && !bCrouching)
			? FAFootstepAudio::ZombieRun
			: FAFootstepAudio::ZombieWalk;
	}
	else if (bCrouching)
	{
		SoundId = FAFootstepAudio::HumanCrouch;
	}
	else
	{
		SoundId = (Gait == EFAFootstepGait::Run) ? FAFootstepAudio::HumanRun : FAFootstepAudio::HumanWalk;
	}

	Audio->PlaySoundAtLocation(SoundId, FAFootstepAudio::ResolveLocation(MeshComp, Socket));
}
