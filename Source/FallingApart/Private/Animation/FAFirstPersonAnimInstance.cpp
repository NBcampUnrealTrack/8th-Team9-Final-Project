#include "Animation/FAFirstPersonAnimInstance.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimMontage.h"
#include "AnimNodes/AnimNode_CopyPoseFromMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Camera/CameraComponent.h"
#include "Player/FACharacter.h"

namespace
{
	// 부모 본의 좌표계를 거쳐 메시 공간의 회전을 로컬 본 회전에 적용한다.
	void RotateInMeshSpace(FTransform& Bone, const FQuat& ParentRotation, float Angle)
	{
		const FQuat Correction(FVector::XAxisVector, Angle);
		Bone.SetRotation(ParentRotation.Inverse() * Correction * ParentRotation * Bone.GetRotation());
	}
}

/** 전신의 완성된 포즈만 복사한다. 애니메이션·몽타주·노티파이를 별도로 재생하지 않는다. */
struct FFAFirstPersonAnimProxy : FAnimInstanceProxy
{
	explicit FFAFirstPersonAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	FAnimNode_CopyPoseFromMesh CopyPose;
	bool bSourcePoseValid = false;
	FBoneReference Head;
	// 양팔 배열은 모두 왼팔 0, 오른팔 1 순서다.
	FBoneReference UpperArms[2];
	FBoneReference Forearms[2];
	FBoneReference Hands[2];
	FVector EyeOffsetFromHead = FVector::ZeroVector;
	FVector CameraInMeshSpace = FVector::ZeroVector;
	bool bStabilizeEyePosition = false;
	float UpperArmLift = 0.0f;
	float ForearmLift = 0.0f;
	FVector ActiveShoulderOffset = FVector::ZeroVector;
	float ForwardSwingLift = 0.0f;
	float MotionFramingWeight = 0.0f;
	float InteractionWeight = 0.0f;
	float InteractionLift = 0.0f;

	virtual void Initialize(UAnimInstance* Instance) override
	{
		FAnimInstanceProxy::Initialize(Instance);
		CopyPose = FAnimNode_CopyPoseFromMesh();
		CopyPose.Initialize_AnyThread(FAnimationInitializeContext(this));
		Head.BoneName = TEXT("Head");
		UpperArms[0].BoneName = TEXT("L_Upperarm");
		UpperArms[1].BoneName = TEXT("R_Upperarm");
		Forearms[0].BoneName = TEXT("L_Forearm");
		Forearms[1].BoneName = TEXT("R_Forearm");
		Hands[0].BoneName = TEXT("L_Hand");
		Hands[1].BoneName = TEXT("R_Hand");
		bSourcePoseValid = false;
	}

	virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
	{
		FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
		auto* Anim = CastChecked<UFAFirstPersonAnimInstance>(Instance);
		const ACharacter* Character = Cast<ACharacter>(Instance->TryGetPawnOwner());
		USkeletalMeshComponent* Body = Character && Character->IsLocallyControlled() ? Character->GetMesh() : nullptr;
		bSourcePoseValid = Body && Body != Instance->GetSkelMeshComponent() && Body->IsRegistered()
			&& Body->GetSkeletalMeshAsset() && !Body->GetComponentSpaceTransforms().IsEmpty();

		// 전신 평가 완료 뒤 실행되도록 캐릭터에서 틱 순서를 지정한다.
		// 여기서 소스를 먼저 지정해야 이번 프레임의 본 배열을 복사할 수 있다.
		CopyPose.SourceMeshComponent = bSourcePoseValid ? Body : nullptr;
		CopyPose.PreUpdate(Instance);
		Anim->bSourcePoseValid = bSourcePoseValid;
		Anim->SourceBoneFrame = bSourcePoseValid ? static_cast<int64>(Body->GetCurrentBoneTransformFrame()) : -1;
		Anim->SourceSnapshotFrame = bSourcePoseValid ? static_cast<int64>(static_cast<uint32>(GFrameCounter)) : -1;
		const AFACharacter* FACharacter = Cast<AFACharacter>(Character);
		// 카메라·설정·몽타주는 게임 스레드에서 읽고, 평가 스레드에는 아래 값만 전달한다.
		bStabilizeEyePosition = Anim->bStabilizeEyePosition && FACharacter && FACharacter->FollowCamera;
		if (bStabilizeEyePosition)
		{
			CameraInMeshSpace = Instance->GetSkelMeshComponent()->GetComponentTransform().InverseTransformPosition(
				FACharacter->FollowCamera->GetComponentLocation());
		}
		EyeOffsetFromHead = Anim->EyeOffsetFromHead;
		UpperArmLift = FMath::DegreesToRadians(Anim->UpperArmLift);
		ForearmLift = FMath::DegreesToRadians(Anim->ForearmLift);
		ActiveShoulderOffset = Anim->ActiveShoulderOffset;
		ForwardSwingLift = FMath::DegreesToRadians(Anim->ForwardSwingLift);
		// 이동 속도와 기존 몽타주의 실제 혼합 비중을 사용한다. 취소 후 잦아드는 구간도 포함한다.
		const double Speed = Character ? Character->GetVelocity().Size2D() : 0.0;
		MotionFramingWeight = FMath::Clamp(Speed / 120.0f, 0.0f, 1.0f);
		ForwardSwingLift *= FMath::Lerp(1.0f, Anim->RunningLiftScale,
			FMath::SmoothStep(200.0f, 450.0f, static_cast<float>(Speed)));
		InteractionWeight = InteractionLift = 0.0f;
		if (bSourcePoseValid && Body->GetAnimInstance())
		{
			for (const FAnimMontageInstance* Montage : Body->GetAnimInstance()->MontageInstances)
			{
				if (!Montage)
				{
					continue;
				}
				const float Weight = Montage->GetWeight();
				MotionFramingWeight = FMath::Max(MotionFramingWeight, Weight);
				if (const float* Lift = Anim->RightArmInteractionLifts.Find(Montage->Montage))
				{
					InteractionLift += FMath::DegreesToRadians(*Lift) * Weight;
					InteractionWeight += Weight;
				}
			}
		}
		if (InteractionWeight > UE_SMALL_NUMBER)
		{
			InteractionLift /= InteractionWeight;
			InteractionWeight = FMath::Clamp(InteractionWeight, 0.0f, 1.0f);
		}
	}

	virtual void CacheBones() override
	{
		FAnimInstanceProxy::CacheBones();
		CopyPose.CacheBones_AnyThread(FAnimationCacheBonesContext(this));
		Head.Initialize(GetRequiredBones());
		for (int32 Side = 0; Side < 2; ++Side)
		{
			UpperArms[Side].Initialize(GetRequiredBones());
			Forearms[Side].Initialize(GetRequiredBones());
			Hands[Side].Initialize(GetRequiredBones());
		}
	}

	virtual void UpdateAnimationNode(const FAnimationUpdateContext& Context) override
	{
		CopyPose.Update_AnyThread(Context);
	}

	virtual bool Evaluate(FPoseContext& Output) override
	{
		// 소스가 사라져도 복사 노드에는 이전 값이 남을 수 있으므로 기본 포즈로 되돌린다.
		if (!bSourcePoseValid)
		{
			Output.ResetToRefPose();
			return true;
		}
		CopyPose.Evaluate_AnyThread(Output);
		const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
		FCSPose<FCompactPose> SourcePose;
		SourcePose.InitPose(Output.Pose);
		// 보정 전 원본에서 전방으로 나온 정도를 구해야 좌우 교대 순서가 변하지 않는다.
		const float ForwardPhase[2] = { GetForwardSwingPhase(SourcePose, Bones, 0), GetForwardSwingPhase(SourcePose, Bones, 1) };
		if (bStabilizeEyePosition && Head.IsValidToEvaluate(Bones))
		{
			// 표시 포즈의 눈 위치만 카메라에 맞춘다. 캐릭터나 카메라 자체를 이동시키지 않는다.
			const FVector Eye = SourcePose.GetComponentSpaceTransform(Head.GetCompactPoseIndex(Bones)).TransformPosition(EyeOffsetFromHead);
			Output.Pose[FCompactPoseBoneIndex(0)].AddToTranslation(CameraInMeshSpace - Eye);
		}
		for (int32 Side = 0; Side < 2; ++Side)
		{
			CorrectArmPitch(Output.Pose, UpperArms[Side], Forearms[Side], UpperArmLift);
			CorrectArmPitch(Output.Pose, Forearms[Side], Hands[Side], ForearmLift);
			ApplyMotionFraming(Output.Pose, Side, ForwardPhase[Side]);
		}
		Output.Pose.NormalizeRotations();
		return true;
	}

private:
	float GetForwardSwingPhase(FCSPose<FCompactPose>& SourcePose, const FBoneContainer& Bones, int32 Side) const
	{
		if (!UpperArms[Side].IsValidToEvaluate(Bones) || !Hands[Side].IsValidToEvaluate(Bones))
		{
			return 0.0f;
		}
		const FVector Swing = SourcePose.GetComponentSpaceTransform(Hands[Side].GetCompactPoseIndex(Bones)).GetLocation()
			- SourcePose.GetComponentSpaceTransform(UpperArms[Side].GetCompactPoseIndex(Bones)).GetLocation();
		return FMath::SmoothStep(-0.25f, 0.35f, static_cast<float>(Swing.GetSafeNormal().Y));
	}

	void CorrectArmPitch(FCompactPose& Pose, const FBoneReference& Bone, const FBoneReference& Tip, float Limit) const
	{
		const FBoneContainer& Bones = Pose.GetBoneContainer();
		if (!Bone.IsValidToEvaluate(Bones) || !Tip.IsValidToEvaluate(Bones) || FMath::IsNearlyZero(Limit))
		{
			return;
		}
		// 상완 보정이 전완에도 반영되도록, 호출할 때마다 최신 포즈에서 방향을 계산한다.
		FCSPose<FCompactPose> CurrentPose;
		CurrentPose.InitPose(Pose);
		const FCompactPoseBoneIndex Index = Bone.GetCompactPoseIndex(Bones);
		const FVector Direction = CurrentPose.GetComponentSpaceTransform(Tip.GetCompactPoseIndex(Bones)).GetLocation()
			- CurrentPose.GetComponentSpaceTransform(Index).GetLocation();
		// 내려간 팔과 머리 위로 올라간 팔을 각각 전방 쪽으로 보정하되 회전량을 제한한다.
		const float Pitch = FMath::Atan2(Direction.Z, FMath::Max(Direction.Y, Direction.Size() * 0.3f));
		const float Angle = FMath::Clamp(-Pitch * 0.65f, -Limit, Limit);
		const FQuat ParentRotation = CurrentPose.GetComponentSpaceTransform(Pose.GetParentBoneIndex(Index)).GetRotation();
		RotateInMeshSpace(Pose[Index], ParentRotation, Angle);
	}

	void ApplyMotionFraming(FCompactPose& Pose, int32 Side, float ForwardPhase) const
	{
		const FBoneContainer& Bones = Pose.GetBoneContainer();
		if (!UpperArms[Side].IsValidToEvaluate(Bones) || MotionFramingWeight <= 0.0f)
		{
			return;
		}
		FCSPose<FCompactPose> FramedPose;
		FramedPose.InitPose(Pose);
		const FCompactPoseBoneIndex Index = UpperArms[Side].GetCompactPoseIndex(Bones);
		const FTransform Parent = FramedPose.GetComponentSpaceTransform(Pose.GetParentBoneIndex(Index));
		// 어깨 표시 위치만 보정한다. 전완·손의 길이와 손목의 원본 로컬 회전은 유지한다.
		Pose[Index].AddToTranslation(Parent.InverseTransformVector(ActiveShoulderOffset * MotionFramingWeight));
		float Lift = ForwardSwingLift * MotionFramingWeight * ForwardPhase;
		if (Side == 1)
		{
			// 문 상태를 다시 판정하지 않고 기존 오른팔 몽타주의 시작·종료 혼합을 따른다.
			Lift = FMath::Lerp(Lift, InteractionLift, InteractionWeight);
		}
		RotateInMeshSpace(Pose[Index], Parent.GetRotation(), Lift);
	}
};

UFAFirstPersonAnimInstance::UFAFirstPersonAnimInstance()
{
	RootMotionMode = ERootMotionMode::IgnoreRootMotion;
}

void UFAFirstPersonAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	bSourcePoseValid = false;
	SourceBoneFrame = SourceSnapshotFrame = -1;
}

FAnimInstanceProxy* UFAFirstPersonAnimInstance::CreateAnimInstanceProxy()
{
	return new FFAFirstPersonAnimProxy(this);
}

void UFAFirstPersonAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}
