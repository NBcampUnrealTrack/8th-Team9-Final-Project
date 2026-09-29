#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Animation/FAAnimInstance.h"
#include "Animation/FAAnimStatusPolicy.h"
#include "AnimNodes/AnimNode_BlendListByEnum.h"
#include "GAS/FAGameplayTags.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFAAnimStatusOverlapTest,
	"FallingApart.Animation.StatusPolicy.OverlapMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFAAnimStatusOverlapTest::RunTest(const FString& Parameters)
{
	using Status = EFAAnimStatusOverride;
	const FAGameplayTags& Tags = FAGameplayTags::Get();
	const FGameplayTag Seizure = FGameplayTag::RequestGameplayTag(TEXT("Animation.State.Seizure"));
	const FGameplayTag StateBits[] = {
		Tags.State_Stunned, Tags.State_Grabbing, Tags.State_Contesting, Tags.State_Grabbed
	};
	// 상태 비트의 16가지 조합에 대한 표시 계약. 경직보다 포획/QTE 역할을 우선한다.
	const Status ExpectedByMask[] = {
		Status::None, Status::Stunned, Status::Grabbing, Status::Grabbing,
		Status::Contesting, Status::Contesting, Status::Contesting, Status::Contesting,
		Status::Grabbed, Status::Grabbed, Status::Grabbed, Status::Grabbed,
		Status::Grabbed, Status::Grabbed, Status::Grabbed, Status::Grabbed
	};

	for (int32 StateMask = 0; StateMask < 16; ++StateMask)
	for (int32 TeamMask = 0; TeamMask < 4; ++TeamMask)
	for (int32 bSeizure = 0; bSeizure < 2; ++bSeizure)
	for (int32 bHasRule = 0; bHasRule < 2; ++bHasRule)
	{
		FGameplayTagContainer ActiveTags;
		for (int32 Bit = 0; Bit < 4; ++Bit)
		{
			if (StateMask & (1 << Bit)) ActiveTags.AddTag(StateBits[Bit]);
		}
		if (TeamMask & 1) ActiveTags.AddTag(Tags.State_Human);
		if (TeamMask & 2) ActiveTags.AddTag(Tags.State_Zombie);
		if (bSeizure) ActiveTags.AddTag(Seizure);
		if (bHasRule) ActiveTags.AddTag(Tags.Rule_Move_Seizure);

		Status Expected = ExpectedByMask[StateMask];
		if (Expected == Status::Stunned && bSeizure) Expected = Status::Seizure;
		if (Expected == Status::Contesting && TeamMask == 1) Expected = Status::HumanContesting;
		if (Expected == Status::Contesting && TeamMask == 2) Expected = Status::ZombieContesting;

		const FString Case = FString::Printf(TEXT("State=%d Team=%d ActiveSeizure=%d Rule=%d"),
			StateMask, TeamMask, bSeizure, bHasRule);
		TestEqual(Case, FAAnimStatusPolicy::Resolve(ActiveTags), Expected);

		// 태그가 도착한 순서나 컨테이너 삽입 순서로 우선순위가 달라지면 안 된다.
		FGameplayTagContainer ReverseTags;
		const TArray<FGameplayTag>& OrderedTags = ActiveTags.GetGameplayTagArray();
		for (int32 Index = OrderedTags.Num() - 1; Index >= 0; --Index) ReverseTags.AddTag(OrderedTags[Index]);
		TestEqual(Case + TEXT(" Reversed"), FAAnimStatusPolicy::Resolve(ReverseTags), Expected);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFAAnimStatusRecoveryTest,
	"FallingApart.Animation.StatusPolicy.InterruptionAndRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFAAnimStatusRecoveryTest::RunTest(const FString& Parameters)
{
	using Status = EFAAnimStatusOverride;
	const FAGameplayTags& Tags = FAGameplayTags::Get();
	const FGameplayTag Seizure = FGameplayTag::RequestGameplayTag(TEXT("Animation.State.Seizure"));
	FGameplayTagContainer ActiveTags;
	ActiveTags.AddTag(Tags.State_Zombie);
	ActiveTags.AddTag(Tags.State_Grabbing);
	TestEqual(TEXT("포획 유지"), FAAnimStatusPolicy::Resolve(ActiveTags), Status::Grabbing);
	ActiveTags.AddTag(Tags.State_Stunned);
	ActiveTags.AddTag(Seizure);
	TestEqual(TEXT("하위 경련이 포획 자세를 재시작하지 않음"), FAAnimStatusPolicy::Resolve(ActiveTags), Status::Grabbing);
	ActiveTags.AddTag(Tags.State_Contesting);
	TestEqual(TEXT("QTE 진입"), FAAnimStatusPolicy::Resolve(ActiveTags), Status::ZombieContesting);
	ActiveTags.RemoveTag(Tags.State_Zombie);
	TestEqual(TEXT("진영 정보 대기 중 QTE 유지"), FAAnimStatusPolicy::Resolve(ActiveTags), Status::Contesting);
	ActiveTags.AddTag(Tags.State_Zombie);
	ActiveTags.RemoveTag(Tags.State_Contesting);
	TestEqual(TEXT("QTE 종료 후 포획 복귀"), FAAnimStatusPolicy::Resolve(ActiveTags), Status::Grabbing);
	ActiveTags.RemoveTag(Tags.State_Grabbing);
	TestEqual(TEXT("포획 취소 후 남은 경련 표시"), FAAnimStatusPolicy::Resolve(ActiveTags), Status::Seizure);
	ActiveTags.RemoveTag(Seizure);
	TestEqual(TEXT("경련 종료 후 남은 경직 표시"), FAAnimStatusPolicy::Resolve(ActiveTags), Status::Stunned);
	ActiveTags.RemoveTag(Tags.State_Stunned);
	TestEqual(TEXT("모든 행동 종료 후 일반 이동 복귀"), FAAnimStatusPolicy::Resolve(ActiveTags), Status::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFAAnimStatusPoseCoverageTest,
	"FallingApart.Animation.StatusPolicy.PoseCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFAAnimStatusPoseCoverageTest::RunTest(const FString& Parameters)
{
	// 정책이 올바른 값을 반환해도 AnimBP 연결이 빠지면 이동/기준 자세로 돌아가는 회귀를 검사한다.
	UClass* AnimClass = LoadClass<UFAAnimInstance>(nullptr,
		TEXT("/Game/FallingApart/Character/Animation/ABP_Character.ABP_Character_C"));
	if (!TestNotNull(TEXT("전신 AnimBP"), AnimClass)) return false;

	const UEnum* StatusEnum = StaticEnum<EFAAnimStatusOverride>();
	const FArrayProperty* PosesProperty = FindFProperty<FArrayProperty>(
		FAnimNode_BlendListBase::StaticStruct(), TEXT("BlendPose"));
	if (!TestNotNull(TEXT("포즈 입력 배열"), PosesProperty)) return false;

	int32 CheckedNodes = 0;
	for (TFieldIterator<FStructProperty> Property(AnimClass); Property; ++Property)
	{
		if (Property->Struct != FAnimNode_BlendListByEnum::StaticStruct()) continue;
		const FAnimNode_BlendListByEnum* Node = Property->ContainerPtrToValuePtr<FAnimNode_BlendListByEnum>(AnimClass->GetDefaultObject());
		const TArray<int32>& Mapping = Node->GetEnumToPoseIndex();
		const TArray<FPoseLink>& Poses = *PosesProperty->ContainerPtrToValuePtr<TArray<FPoseLink>>(Node);
		++CheckedNodes;
		for (int32 Index = 0; Index < StatusEnum->NumEnums() - 1; ++Index)
		{
			const int32 Value = static_cast<int32>(StatusEnum->GetValueByIndex(Index));
			const FString Name = StatusEnum->GetNameStringByIndex(Index);
			if (!TestTrue(Name + TEXT(" 매핑 존재"), Mapping.IsValidIndex(Value))) continue;
			const int32 PoseIndex = Mapping[Value];
			if (Value != static_cast<int32>(EFAAnimStatusOverride::None))
			{
				TestTrue(Name + TEXT(" 일반 이동과 분리"), PoseIndex > 0);
			}
			if (!TestTrue(Name + TEXT(" 입력 존재"), Poses.IsValidIndex(PoseIndex))) continue;
			TestTrue(Name + TEXT(" 포즈 연결"), Poses[PoseIndex].LinkID != INDEX_NONE);
		}
	}
	TestEqual(TEXT("특수 상태의 최종 선택 노드 수"), CheckedNodes, 1);
	return true;
}

#endif
