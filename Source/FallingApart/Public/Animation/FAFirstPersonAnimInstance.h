#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "FAFirstPersonAnimInstance.generated.h"

/** 소유 캐릭터의 최종 전신 포즈를 복사하는 표시용 애니메이션. 게임플레이 처리는 전신에 맡긴다. */
UCLASS(Transient, Blueprintable)
class FALLINGAPART_API UFAFirstPersonAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UFAFirstPersonAnimInstance();
	virtual void NativeInitializeAnimation() override;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

	/** 표시 포즈의 눈 위치를 카메라에 맞춰 머리 애니메이션으로 팔 배치가 흔들리는 것을 막는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "First Person|Framing")
	bool bStabilizeEyePosition = true;

	/** 공유 스켈레톤의 Head 본 좌표계에서 측정한 두 눈의 중심 위치. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "First Person|Framing")
	FVector EyeOffsetFromHead = FVector(0.806666, -9.962656, 12.141244);

	/** 원본 팔의 길이·움직임 순서를 유지하면서 적용할 상완의 최대 각도 보정. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "First Person|Framing", meta = (ClampMin = "0", ClampMax = "70", Units = "deg"))
	float UpperArmLift = 45.0f;

	/** 상완 보정 후 전완에 적용할 최대 각도 보정. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "First Person|Framing", meta = (ClampMin = "0", ClampMax = "45", Units = "deg"))
	float ForearmLift = 25.0f;

	/** 메시 공간에서의 어깨 표시 위치 보정(모델 cm). 정지하면 보정이 사라진다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "First Person|Framing")
	FVector ActiveShoulderOffset = FVector(0.0f, 4.0f, 4.0f);

	/** 원본 팔이 앞으로 나오는 구간에만 더하는 각도. 별도 동작을 재생하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "First Person|Framing", meta = (ClampMin = "0", ClampMax = "30", Units = "deg"))
	float ForwardSwingLift = 20.0f;

	/** 원본 달리기는 팔꿈치를 충분히 들기 때문에 걷기보다 추가 보정을 줄인다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "First Person|Framing", meta = (ClampMin = "0", ClampMax = "1"))
	float RunningLiftScale = 0.4f;

	/** 기존 문 몽타주별 오른팔 보정 각도. 전신 몽타주의 실제 혼합 비중을 따른다. */
	UPROPERTY(EditDefaultsOnly, Category = "First Person|Framing")
	TMap<TObjectPtr<UAnimMontage>, float> RightArmInteractionLifts;

	/** 검사 정보: 이번 프레임에 복사할 유효한 로컬 전신 메시가 있는지 여부. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "First Person|Pose Source")
	bool bSourcePoseValid = false;

	/** 검사 정보: 전신이 본 변환을 갱신한 엔진 프레임. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "First Person|Pose Source")
	int64 SourceBoneFrame = -1;

	/** 검사 정보: 본 변환을 복사한 프레임. SourceBoneFrame과 비교해 평가 순서를 확인한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "First Person|Pose Source")
	int64 SourceSnapshotFrame = -1;
};
