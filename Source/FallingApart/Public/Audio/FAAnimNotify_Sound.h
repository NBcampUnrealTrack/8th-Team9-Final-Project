// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "FAAnimNotify_Sound.generated.h"

/**
 * 애니메이션에서 사운드 표의 ID 를 그대로 재생하는 노티파이.
 */
UCLASS(meta = (DisplayName = "FA Sound"))
class FALLINGAPART_API UFAAnimNotify_Sound : public UAnimNotify
{
	GENERATED_BODY()

public:
	UFAAnimNotify_Sound();

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

protected:
	/** DT_Sound_* 의 RowName. 예: Jump_Land · Mantle · Grab_Attempt */
	UPROPERTY(EditAnywhere, Category = "FallingApart|Audio")
	FName SoundId;

	/**
	 * 소리가 날 소켓. 비우면 액터 위치에서 난다.
	 * 발소리는 foot_l / foot_r 처럼 실제 발 본을 지정하는 편이 방향감이 산다.
	 */
	UPROPERTY(EditAnywhere, Category = "FallingApart|Audio")
	FName Socket;
};

/**
 * 발소리 전용 노티파이 — 진영(인간/좀비)을 런타임에 보고 ID 를 고른다.
 *
 * 인간과 좀비가 같은 캐릭터·같은 애니메이션을 쓰기 때문에 필요하다. 엔진 기본 노티파이로는
 * 애니메이션 애셋을 진영별로 복제해야 소리를 나눌 수 있는데, 그러면 애니메이션 담당이
 * 같은 동작을 두 벌 관리하게 된다.
 *
 * 바닥재별 분기는 하지 않는다 (확정 사항) — 배리에이션과 피치 랜덤은 Sound Cue 와 DT 가 맡는다.
 */
UENUM(BlueprintType)
enum class EFAFootstepGait : uint8
{
	Walk	= 0,
	Run		= 1,
	/** 웅크림. 좀비용 행이 없으면 걷기로 폴백한다. */
	Crouch	= 2
};

UCLASS(meta = (DisplayName = "FA Footstep"))
class FALLINGAPART_API UFAAnimNotify_Footstep : public UAnimNotify
{
	GENERATED_BODY()

public:
	UFAAnimNotify_Footstep();

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

protected:
	/** 이 노티파이가 걷기 애니에 있는지 뛰기 애니에 있는지. 애니메이션이 알고 있는 정보다. */
	UPROPERTY(EditAnywhere, Category = "FallingApart|Audio")
	EFAFootstepGait Gait = EFAFootstepGait::Walk;

	/** foot_l / foot_r. 비우면 액터 위치. */
	UPROPERTY(EditAnywhere, Category = "FallingApart|Audio")
	FName Socket;
};
