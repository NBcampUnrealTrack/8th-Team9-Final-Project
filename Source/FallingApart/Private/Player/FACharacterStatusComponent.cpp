// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/FACharacterStatusComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AttributeSet/FAAttributeSet.h"
#include "Audio/FAAudioSubsystem.h"
#include "Audio/FAPhantomAudioComponent.h"
#include "Audio/FAHearingSubsystem.h" // [Z22 귀먹음] 실제 오디오 처리는 청취 클래스에 맡긴다.
#include "GameFramework/Character.h"
#include "GameplayEffect.h"
#include "GAS/FAGameplayTags.h"
#include "TimerManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimMontage.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Rule/FADeathMatchSubsystem.h"
#include "Player/FACharacter.h"
#include "Player/FADetectionUnitComponent.h"
#include "Player/FATrailComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"

// Sets default values for this component's properties
UFACharacterStatusComponent::UFACharacterStatusComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// 지속 상태인 GameplayTag는 기존처럼 ASC가 복제한다.
	SetIsReplicatedByDefault(true);
}


// Called when the game starts
void UFACharacterStatusComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerCharacter = Cast<ACharacter>(GetOwner());

	if (!OwnerCharacter)
	{
		UE_LOG(LogTemp,Warning,TEXT("FACharacterStatusComponent는 ""ACharacter에 부착해야 합니다."));
		return;
	}
	OwnerCharacter->ReceiveControllerChangedDelegate.AddDynamic(this, &ThisClass::HandleDarkControllerChanged);
	OwnerCharacter->LandedDelegate.AddDynamic(this,&ThisClass::HandleLanded);

	if (UAbilitySystemComponent* ASC = GetOwnerAbilitySystem())
	{
		const FGameplayTag DarkRuleTag = FAGameplayTags::Get().Rule_Vision_Dark;
		ASC->RegisterGameplayTagEvent(DarkRuleTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleDarkRuleChanged);
		HandleDarkRuleChanged(DarkRuleTag, ASC->GetTagCount(DarkRuleTag));

		const FGameplayTag PhantomRuleTag = FAGameplayTags::Get().Rule_Audio_Phantom;
		ASC->RegisterGameplayTagEvent(PhantomRuleTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandlePhantomRuleChanged);
		// BeginPlay 전에 적용된 환청도 초기 상태에 반영한다.
		HandlePhantomRuleChanged(PhantomRuleTag, ASC->GetTagCount(PhantomRuleTag));

		const FGameplayTag FreezeRuleTag = FAGameplayTags::Get().Rule_Move_Freeze;
		ASC->RegisterGameplayTagEvent(FreezeRuleTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleFreezeRuleChanged);
		HandleFreezeRuleChanged(FreezeRuleTag, ASC->GetTagCount(FreezeRuleTag));

		const FGameplayTag KnockbackTag = FAGameplayTags::Get().Rule_State_Knockback;
		ASC->RegisterGameplayTagEvent(KnockbackTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleKnockbackRuleChanged);
		HandleKnockbackRuleChanged(KnockbackTag, ASC->GetTagCount(KnockbackTag));

		const FGameplayTag FlipRuleTag = FAGameplayTags::Get().Rule_Input_FlipLR;
		ASC->RegisterGameplayTagEvent(FlipRuleTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleInputFlipRuleChanged);

		// BeginPlay 전에 룰이 이미 적용된 경우도 처리한다.
		HandleInputFlipRuleChanged(FlipRuleTag, ASC->GetTagCount(FlipRuleTag));

		const FGameplayTag BackwardOnlyRuleTag = FAGameplayTags::Get().Rule_Move_BackwardOnly;
		ASC->RegisterGameplayTagEvent(BackwardOnlyRuleTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleBackwardOnlyRuleChanged);
		HandleBackwardOnlyRuleChanged(BackwardOnlyRuleTag, ASC->GetTagCount(BackwardOnlyRuleTag));

		// [ 포복 ]
		const FGameplayTag CrouchOnlyRuleTag = FAGameplayTags::Get().Rule_Move_CrouchOnly;
		ASC->RegisterGameplayTagEvent(CrouchOnlyRuleTag, EGameplayTagEventType::NewOrRemoved)
		 	.AddUObject(this, &ThisClass::HandleCrouchOnlyRuleChanged);

		// BeginPlay 전에 이미 룰이 적용된 경우에도 즉시 포복시킨다.
		 HandleCrouchOnlyRuleChanged(CrouchOnlyRuleTag,ASC->GetTagCount(CrouchOnlyRuleTag));

		const FGameplayTag GhostRuleTag =FAGameplayTags::Get().Rule_Trade_Ghost;

		// Ghost
		ASC->RegisterGameplayTagEvent(GhostRuleTag,EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this,&ThisClass::HandleGhostRuleChanged);
		// BeginPlay 전에 Z44가 적용된 경우도 즉시 반영한다.
		HandleGhostRuleChanged(GhostRuleTag,ASC->GetTagCount(GhostRuleTag));

		const FGameplayTag FatigueRuleTag = FAGameplayTags::Get().Rule_Move_NoSprint;
		ASC->RegisterGameplayTagEvent(FatigueRuleTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleFatigueRuleChanged);
		// BeginPlay 전에 적용된 H03/Z03도 즉시 반영한다.
		HandleFatigueRuleChanged(FatigueRuleTag, ASC->GetTagCount(FatigueRuleTag));

		const FGameplayTag WallPhaseRuleTag =FAGameplayTags::Get().Rule_Trade_WallPhase;
		ASC->RegisterGameplayTagEvent(WallPhaseRuleTag,EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this,&ThisClass::HandleWallPhaseRuleChanged);
		// BeginPlay 전에 Z46이 적용된 경우도 처리한다.
		HandleWallPhaseRuleChanged(WallPhaseRuleTag,ASC->GetTagCount(WallPhaseRuleTag));

		const FGameplayTag SecondLifeRuleTag = FAGameplayTags::Get().Rule_Trade_SecondLife;
		ASC->RegisterGameplayTagEvent(SecondLifeRuleTag,EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this,&ThisClass::HandleSecondLifeRuleChanged);
		HandleSecondLifeRuleChanged(SecondLifeRuleTag,ASC->GetTagCount(SecondLifeRuleTag));

		const FGameplayTag GambleSpeedRuleTag = FAGameplayTags::Get().Rule_Gamble_Speed;
		ASC->RegisterGameplayTagEvent(GambleSpeedRuleTag,EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this,&ThisClass::HandleGambleSpeedRuleChanged);
		HandleGambleSpeedRuleChanged(GambleSpeedRuleTag,ASC->GetTagCount(GambleSpeedRuleTag));

		const FGameplayTag RecallRuleTag =FAGameplayTags::Get().Rule_State_Recall;

		// H43 / Z40 공용 귀환 태그의 추가·제거 이벤트를 받는다.
		ASC->RegisterGameplayTagEvent(RecallRuleTag,EGameplayTagEventType::NewOrRemoved).
		AddUObject(this,&ThisClass::HandleRecallRuleChanged);

		HandleRecallRuleChanged(RecallRuleTag,ASC->GetTagCount(RecallRuleTag));

		const FGameplayTag HeartbeatRuleTag =FAGameplayTags::Get().Rule_State_Heartbeat;

		ASC->RegisterGameplayTagEvent(HeartbeatRuleTag,EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this,&ThisClass::HandleHeartbeatRuleChanged);

		HandleHeartbeatRuleChanged(HeartbeatRuleTag,ASC->GetTagCount(HeartbeatRuleTag));

		const FGameplayTag TrailRuleTag = FAGameplayTags::Get().Rule_State_Trail;
		ASC->RegisterGameplayTagEvent(TrailRuleTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleTrailRuleChanged);
		HandleTrailRuleChanged(TrailRuleTag, ASC->GetTagCount(TrailRuleTag));

		const FGameplayTag ForceNoiseRuleTag = FAGameplayTags::Get().Rule_Social_ForceNoise;
		ASC->RegisterGameplayTagEvent(ForceNoiseRuleTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleForceNoiseRuleChanged);
		HandleForceNoiseRuleChanged(ForceNoiseRuleTag, ASC->GetTagCount(ForceNoiseRuleTag));

		const FGameplayTag DecayTag = FAGameplayTags::Get().Rule_State_Decay;
		ASC->RegisterGameplayTagEvent(DecayTag, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleDecayRuleChanged);

		HandleDecayRuleChanged(DecayTag, ASC->GetTagCount(DecayTag));

		// [Z22 귀먹음] 태그 변경을 등록하고 이미 적용된 룰도 초기 반영한다.
		const FGameplayTag DeafRuleTag = FAGameplayTags::Get().Rule_Audio_Deaf;
		ASC->RegisterGameplayTagEvent(DeafRuleTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleDeafRuleChanged);
		HandleDeafRuleChanged(DeafRuleTag, ASC->GetTagCount(DeafRuleTag));

		const FGameplayTag ForceScreamRuleTag = FAGameplayTags::Get().Rule_Social_ForceScream;
		ASC->RegisterGameplayTagEvent(ForceScreamRuleTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleForceScreamRuleChanged);
		HandleForceScreamRuleChanged(ForceScreamRuleTag, ASC->GetTagCount(ForceScreamRuleTag));
	}
}

void UFACharacterStatusComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UFAPhantomAudioComponent* Phantom = GetOwner()->FindComponentByClass<UFAPhantomAudioComponent>())
	{
		Phantom->SetRuleActive(false);
	}
	UpdateDarkWidget(false);
	SetHideActive(false);
	ClearFreeze();
	if (UAbilitySystemComponent* ASC = GetOwnerAbilitySystem())
	{
		const FAGameplayTags& Tags = FAGameplayTags::Get();
		ASC->RegisterGameplayTagEvent(Tags.Rule_Vision_Dark,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		ASC->RegisterGameplayTagEvent(Tags.Rule_Audio_Phantom,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		ASC->RegisterGameplayTagEvent(Tags.Rule_Audio_NoStep,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		// [Z22 귀먹음] 다른 룰과 동일하게 종료 시 태그 이벤트 연결을 해제한다.
		ASC->RegisterGameplayTagEvent(Tags.Rule_Audio_Deaf,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		ASC->RegisterGameplayTagEvent(Tags.Rule_State_Knockback,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		ASC->RegisterGameplayTagEvent(Tags.Rule_Input_FlipLR,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		ASC->RegisterGameplayTagEvent(Tags.Rule_Move_BackwardOnly,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		ASC->RegisterGameplayTagEvent(Tags.Rule_Move_CrouchOnly,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		ASC->RegisterGameplayTagEvent(Tags.Rule_Trade_Ghost,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		ASC->RegisterGameplayTagEvent(Tags.Rule_Move_NoSprint,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		ASC->RegisterGameplayTagEvent(Tags.Rule_Trade_WallPhase,
		 EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		ASC->RegisterGameplayTagEvent(Tags.Rule_Trade_SecondLife,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		ASC->RegisterGameplayTagEvent(Tags.Rule_Gamble_Speed,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		ASC->RegisterGameplayTagEvent(Tags.Rule_State_Recall,
		    EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		ASC->RegisterGameplayTagEvent(Tags.Rule_State_Heartbeat,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		ASC->RegisterGameplayTagEvent(Tags.Rule_State_Trail,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		ASC->RegisterGameplayTagEvent(Tags.Rule_Social_ForceNoise,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		ASC->RegisterGameplayTagEvent(Tags.Rule_State_Decay,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		ASC->RegisterGameplayTagEvent(Tags.Rule_Move_Freeze,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		ASC->RegisterGameplayTagEvent(Tags.Rule_Social_ForceScream,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
	}

	GetWorld()->GetTimerManager().ClearTimer(InputFlipTimerHandle);
	SetInputFlipActive(false);
	GetWorld()->GetTimerManager().ClearTimer(BackwardOnlyTimerHandle);
	SetBackwardOnlyActive(false);
	ClearGambleSpeed();
	ClearFatigue();
	ClearRecall();
	StopHeartbeatDetection();
	SetGhostVisualActive(false);
	GhostDynamicMaterials.Reset();

	if (OwnerCharacter)
	{
		OwnerCharacter->ReceiveControllerChangedDelegate.RemoveDynamic(this, &ThisClass::HandleDarkControllerChanged);
		OwnerCharacter->LandedDelegate.RemoveDynamic(this,&ThisClass::HandleLanded);
		OwnerCharacter->GetCapsuleComponent()->OnComponentHit.RemoveDynamic(this, &ThisClass::HandleKnockbackHit);
	}

	ClearForcedNoise();
	ClearForcedScream();
	ClearDecay();
	Super::EndPlay(EndPlayReason);
}

// [Z22 귀먹음] 룰 실행 진입점은 Status에 두고, 볼륨 적용은 공통 클래스를 재사용한다.
void UFACharacterStatusComponent::HandleDeafRuleChanged(const FGameplayTag Tag, int32 NewCount)
{
	(void)Tag;
	(void)NewCount;
	if (!OwnerCharacter || !OwnerCharacter->IsLocallyControlled())
	{
		return;
	}
	if (UFAHearingSubsystem* Hearing = UFAHearingSubsystem::Get(this))
	{
		// 콜백 인자 대신 현재 ASC 상태를 확인해 초기 적용과 중첩 해제를 함께 처리한다.
		Hearing->UpdateHearingState();
	}
}

void UFACharacterStatusComponent::HandleNoStepRuleChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (!OwnerCharacter || !OwnerCharacter->IsLocallyControlled())
	{
		return;
	}
	if (UFAHearingSubsystem* Hearing = UFAHearingSubsystem::Get(this))
	{
		Hearing->UpdateHearingState();
	}
}

void UFACharacterStatusComponent::HandleFreezeRuleChanged(FGameplayTag Tag, int32 NewCount)
{
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority())
	{
		return;
	}

	const UAbilitySystemComponent* ASC = GetOwnerAbilitySystem();
	const FAGameplayTags& Tags = FAGameplayTags::Get();
	const bool bEnabled = ASC && ASC->HasMatchingGameplayTag(Tags.Rule_Move_Freeze);
	if (!bEnabled)
	{
		ClearFreeze();
		return;
	}

	if (!GetWorld()->GetTimerManager().IsTimerActive(FreezeDetectionTimerHandle))
	{
		if (!FreezeStunEffect)
		{
			UE_LOG(LogTemp, Error, TEXT("[H15] FreezeStunEffect is not configured on %s"), *GetNameSafe(GetOwner()));
			return;
		}
		GetWorld()->GetTimerManager().SetTimer(FreezeDetectionTimerHandle, this,
			&ThisClass::ScanFreezeTargets, 0.1f, true);
		// Already nearby zombies count as entry when the rule becomes active.
		ScanFreezeTargets();
	}
}

void UFACharacterStatusComponent::ScanFreezeTargets()
{
	AFACharacter* Character = Cast<AFACharacter>(OwnerCharacter);
	UAbilitySystemComponent* ASC = GetOwnerAbilitySystem();
	if (!Character || !Character->HasAuthority() || !ASC)
	{
		return;
	}
	const FAGameplayTags& Tags = FAGameplayTags::Get();
	if (!ASC->HasMatchingGameplayTag(Tags.Rule_Move_Freeze))
	{
		ClearFreeze();
		return;
	}
	const UFADetectionUnitComponent* Detection = Character->GetDetectionUnitComponent();
	if (!Detection)
	{
		return;
	}

	TArray<AFACharacter*> NearbyZombies;
	Detection->GetCharactersInRange(Tags.State_Zombie, 500.0f, NearbyZombies);
	TSet<TWeakObjectPtr<AFACharacter>> CurrentTargets;
	bool bNewEntry = false;
	for (AFACharacter* Zombie : NearbyZombies)
	{
		CurrentTargets.Add(Zombie);
		if (!PreviousFreezeTargets.Contains(Zombie))
		{
			bNewEntry = true;
		}
	}

	PreviousFreezeTargets = MoveTemp(CurrentTargets);
	if (bNewEntry && FreezeStunEffect && !ASC->HasMatchingGameplayTag(Tags.State_Stunned))
	{
		FreezeEffectHandle = ASC->ApplyGameplayEffectToSelf(
			FreezeStunEffect->GetDefaultObject<UGameplayEffect>(),
			1.0f,
			ASC->MakeEffectContext());
	}
}

void UFACharacterStatusComponent::ClearFreeze()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FreezeDetectionTimerHandle);
	}
	PreviousFreezeTargets.Reset();
	if (FreezeEffectHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = GetOwnerAbilitySystem())
		{
			ASC->RemoveActiveGameplayEffect(FreezeEffectHandle);
		}
		FreezeEffectHandle.Invalidate();
	}
}

void UFACharacterStatusComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime,TickType,ThisTickFunction);
	if (!OwnerCharacter)
	{
		return;
	}

	UAbilitySystemComponent* ASC =GetOwnerAbilitySystem();

	UCapsuleComponent* Capsule =OwnerCharacter->GetCapsuleComponent();

	if (!ASC || !Capsule)
	{
		return;
	}

	if (OwnerCharacter->HasAuthority())
	{
		UpdateHideRule(DeltaTime);
		UpdateCampRule(DeltaTime);
		UpdateFatigueRule(DeltaTime);
	}

	const bool bShouldPhase =ASC->HasMatchingGameplayTag(
			FAGameplayTags::Get().Rule_Trade_WallPhase);

	const ECollisionResponse DesiredResponse =bShouldPhase ? ECR_Ignore : ECR_Block;

	if (Capsule->GetCollisionResponseToChannel(FACollision::PhaseWall) != DesiredResponse)
	{
		SetWallPhaseActive(bShouldPhase);
	}
}

void UFACharacterStatusComponent::HandleFatigueRuleChanged(const FGameplayTag Tag, int32 NewCount)
{
	(void)Tag;
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority())
	{
		return;
	}

	ClearFatigue();
	bFatigueActive = NewCount > 0;
}

void UFACharacterStatusComponent::ClearFatigue()
{
	bFatigueActive = false;
	FatigueState = FFAFatigueState();
	if (UAbilitySystemComponent* ASC = GetOwnerAbilitySystem(); ASC && OwnerCharacter
		&& OwnerCharacter->HasAuthority())
	{
		ASC->SetNumericAttributeBase(UFAAttributeSet::GetFatigueSpeedMultiplierAttribute(), 1.0f);
	}
}

void UFACharacterStatusComponent::UpdateFatigueRule(float DeltaTime)
{
	UAbilitySystemComponent* ASC = GetOwnerAbilitySystem();
	if (!bFatigueActive || !ASC || !OwnerCharacter || !OwnerCharacter->HasAuthority())
	{
		return;
	}

	// 태그 이벤트가 활성화한 동안만 이동/회복 시간을 갱신한다.
	const bool bMoving = OwnerCharacter->GetVelocity().SizeSquared2D() > FMath::Square(3.0f);
	FatigueState.Update(DeltaTime, bMoving);

	const float Multiplier = FatigueState.GetSpeedMultiplier();
	const float PreviousMultiplier = ASC->GetNumericAttribute(UFAAttributeSet::GetFatigueSpeedMultiplierAttribute());
	if (!FMath::IsNearlyEqual(PreviousMultiplier, Multiplier))
	{
		ASC->SetNumericAttributeBase(UFAAttributeSet::GetFatigueSpeedMultiplierAttribute(), Multiplier);
	}
}

void UFACharacterStatusComponent::HandlePhantomRuleChanged(FGameplayTag Tag, int32 NewCount)
{
	const UAbilitySystemComponent* ASC = GetOwnerAbilitySystem();
	const bool bActive = ASC && ASC->HasMatchingGameplayTag(FAGameplayTags::Get().Rule_Audio_Phantom);
	if (UFAPhantomAudioComponent* Phantom = GetOwner()->FindComponentByClass<UFAPhantomAudioComponent>())
	{
		Phantom->SetRuleActive(bActive);
	}
}

void UFACharacterStatusComponent::HandleKnockbackRuleChanged(FGameplayTag Tag, int32 NewCount)
{
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority())
	{
		return;
	}

	UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
	if (NewCount > 0)
	{
		Capsule->OnComponentHit.AddUniqueDynamic(this, &ThisClass::HandleKnockbackHit);
	}
	else
	{
		Capsule->OnComponentHit.RemoveDynamic(this, &ThisClass::HandleKnockbackHit);
		NextKnockbackTime = 0.0;
	}
}

void UFACharacterStatusComponent::HandleKnockbackHit(UPrimitiveComponent* HitComponent,
	AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority())
	{
		return;
	}
	// 충돌 상대의 본체 캡슐인지 확인
	AFACharacter* Human = Cast<AFACharacter>(OtherActor);
	if (!IsValid(Human) || Human == OwnerCharacter
		|| OtherComp != Human->GetCapsuleComponent())
	{
		return;
	}

	// 양쪽 ASC 확인
	UAbilitySystemComponent* ASC = GetOwnerAbilitySystem();
	UAbilitySystemComponent* HumanASC = Human->GetAbilitySystemComponent();
	if (!ASC || !HumanASC)
	{
		return;
	}

	const FAGameplayTags& Tags = FAGameplayTags::Get();

	// 반발 룰을 가진 좀비가 인간과 충돌한 경우만 처리
	if (!ASC->HasMatchingGameplayTag(Tags.Rule_State_Knockback)
		|| !ASC->HasMatchingGameplayTag(Tags.State_Zombie)
		|| !HumanASC->HasMatchingGameplayTag(Tags.State_Human))
	{
		return;
	}

	// 연속 반발 방지
	if (GetWorld()->GetTimeSeconds() < NextKnockbackTime)
	{
		return;
	}

	// 포획이나 구출 대결 중에는 반발하지 않음
	if (ASC->HasMatchingGameplayTag(Tags.State_Grabbing)
		|| ASC->HasMatchingGameplayTag(Tags.State_Grabbed)
		|| ASC->HasMatchingGameplayTag(Tags.State_Contesting))
	{
		return;
	}

	// 명중 전 물기 몽타주도 보호한다. State.Grabbing은 포획 성공 후에 붙는다.
	TArray<FGameplayAbilitySpec*> GrabSpecs;
	ASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(
		FGameplayTagContainer(Tags.Ability_Grab), GrabSpecs, false);
	for (const FGameplayAbilitySpec* Spec : GrabSpecs)
	{
		if (Spec && Spec->IsActive())
		{
			return;
		}
	}

	FVector Direction = (OwnerCharacter->GetActorLocation() - Human->GetActorLocation()).GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = -OwnerCharacter->GetActorForwardVector().GetSafeNormal2D();
	}
	// 하차 중 발생할 수 있는 중첩 Hit도 재발동 간격으로 막는다.
	NextKnockbackTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.05f, KnockbackInterval);
	if (AFACharacter* Zombie = Cast<AFACharacter>(OwnerCharacter))
	{
		Zombie->ServerStopClimbing(false);
	}
	OwnerCharacter->LaunchCharacter(Direction * KnockbackSpeed + FVector(0.0f, 0.0f, KnockbackUpSpeed), true, true);
	MulticastPlayKnockbackMontage();
}

void UFACharacterStatusComponent::MulticastPlayKnockbackMontage_Implementation()
{
	if (!OwnerCharacter || !KnockbackMontage)
	{
		return;
	}

	const float PlayedLength = OwnerCharacter->PlayAnimMontage(KnockbackMontage);
	if (PlayedLength <= 0.0f)
	{
		return;
	}
}

void UFACharacterStatusComponent::HandleInputFlipRuleChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(InputFlipTimerHandle);
	SetInputFlipActive(false);

	if (NewCount <= 0)
	{
		return;
	}

	// 룰이 생긴 직후 5초는 정상, 이후 반전/정상을 5초마다 반복한다.
	GetWorld()->GetTimerManager().SetTimer(
		InputFlipTimerHandle,
		this,
		&ThisClass::ToggleInputFlip,
		InputFlipInterval,
		true);
}

void UFACharacterStatusComponent::HandleBackwardOnlyRuleChanged(
	const FGameplayTag Tag, int32 NewCount)
{
	(void)Tag;

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(BackwardOnlyTimerHandle);
	SetBackwardOnlyActive(false);

	if (NewCount <= 0)
	{
		return;
	}

	// 룰을 얻는 즉시 첫 5초는 후진 전용으로 시작하고,
	// 이후 정상 이동과 후진 전용을 5초마다 반복한다.
	SetBackwardOnlyActive(true);
	GetWorld()->GetTimerManager().SetTimer(
		BackwardOnlyTimerHandle,
		this,
		&ThisClass::ToggleBackwardOnly,
		BackwardOnlyInterval,
		true);
}

void UFACharacterStatusComponent::HandleCrouchOnlyRuleChanged(const FGameplayTag Tag,int32 NewCount)
 {
 	if (!OwnerCharacter || !OwnerCharacter->HasAuthority())
 	{
 		return;
 	}

 	if (NewCount > 0)
 	{
 		OwnerCharacter->Crouch();
 	}
 	else
 	{
 		OwnerCharacter->UnCrouch();
 	}
 }

void UFACharacterStatusComponent::HandleGhostRuleChanged(const FGameplayTag Tag, int32 NewCount)
{
	// 델리게이트 시그니처를 맞추기 위한 인자다. ??
	(void)Tag;


	RefreshGhostVisual();
}

  void UFACharacterStatusComponent::SetGhostVisualActive(
      bool bActive)
  {
      if (!OwnerCharacter)
      {
          return;
      }

      USkeletalMeshComponent* MeshComponent =OwnerCharacter->GetMesh();
      if (!MeshComponent)
      {
          return;
      }
      const int32 SlotCount =MeshComponent->GetNumMaterials();
      if (SlotCount <= 0)
      {
          return;
      }

      if (bActive)
      {
          GhostDynamicMaterials.Reset();
          GhostDynamicMaterials.SetNum(SlotCount);

          for (int32 SlotIndex = 0;SlotIndex < SlotCount;++SlotIndex)
          {
              UMaterialInterface* OriginalMaterial =MeshComponent->GetMaterial(SlotIndex);

              UMaterialInstanceDynamic* DynamicMaterial =MeshComponent->CreateDynamicMaterialInstance(SlotIndex);

              if (!DynamicMaterial)
              {
                  continue;
              }

              GhostDynamicMaterials[SlotIndex] =DynamicMaterial;

              DynamicMaterial->SetScalarParameterValue(TEXT("GhostOpacity"),GhostActiveOpacity);
              UMaterialInterface* AppliedMaterial =MeshComponent->GetMaterial(SlotIndex);
          }
          return;
      }

      if (GhostDynamicMaterials.Num() == 0)
      {
          return;
      }

      for (int32 SlotIndex = 0;SlotIndex < GhostDynamicMaterials.Num();++SlotIndex)
      {
	      UMaterialInstanceDynamic* DynamicMaterial =GhostDynamicMaterials[SlotIndex];

      	if (!DynamicMaterial)
      	{
      		continue;
      	}

      	DynamicMaterial->SetScalarParameterValue(TEXT("GhostOpacity"),1.0f);
      }
  }

void UFACharacterStatusComponent::HandleWallPhaseRuleChanged(const FGameplayTag Tag, int32 NewCount)
{
	(void)Tag;

	SetWallPhaseActive(NewCount > 0);
}

void UFACharacterStatusComponent::SetWallPhaseActive(bool bActive)
{
	if (!OwnerCharacter)
	{
		return;
	}

	UCapsuleComponent* Capsule =OwnerCharacter->GetCapsuleComponent();

	if (!Capsule)
	{
		return;
	}

	Capsule->SetCollisionResponseToChannel(FACollision::PhaseWall,
		bActive ? ECR_Ignore : ECR_Block);
	UE_LOG(LogTemp,Warning,
	 TEXT("[Z46] Character=%s, Active=%s, PhaseWallResponse=%d"),
	 *GetNameSafe(OwnerCharacter),
	 bActive ? TEXT("true") : TEXT("false"),
	 static_cast<int32>(
		 Capsule->GetCollisionResponseToChannel(
			 FACollision::PhaseWall)));
}

void UFACharacterStatusComponent::ToggleInputFlip()
{
	UAbilitySystemComponent* ASC = GetOwnerAbilitySystem();
	if (!ASC || !ASC->HasMatchingGameplayTag(FAGameplayTags::Get().Rule_Input_FlipLR))
	{
		GetWorld()->GetTimerManager().ClearTimer(InputFlipTimerHandle);
		SetInputFlipActive(false);
		return;
	}

	SetInputFlipActive(!bInputFlipActive);
}

void UFACharacterStatusComponent::SetInputFlipActive(bool bActive)
{
	if (bInputFlipActive == bActive)
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetOwnerAbilitySystem();
	if (!ASC)
	{
		return;
	}

	bInputFlipActive = bActive;
	const FGameplayTag StateTag = FAGameplayTags::Get().State_Input_Flipped;

	if (bInputFlipActive)
	{
		ASC->AddLooseGameplayTag(StateTag,1,EGameplayTagReplicationState::CountToOwner);
	}
	else
	{
		ASC->RemoveLooseGameplayTag(StateTag,1,EGameplayTagReplicationState::CountToOwner);
	}
}

void UFACharacterStatusComponent::ToggleBackwardOnly()
{
	UAbilitySystemComponent* ASC = GetOwnerAbilitySystem();
	if (!ASC || !ASC->HasMatchingGameplayTag(
		FAGameplayTags::Get().Rule_Move_BackwardOnly))
	{
		GetWorld()->GetTimerManager().ClearTimer(BackwardOnlyTimerHandle);
		SetBackwardOnlyActive(false);
		return;
	}

	SetBackwardOnlyActive(!bBackwardOnlyActive);
}

void UFACharacterStatusComponent::SetBackwardOnlyActive(bool bActive)
{
	if (bBackwardOnlyActive == bActive)
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetOwnerAbilitySystem();
	if (!ASC)
	{
		return;
	}

	bBackwardOnlyActive = bActive;
	const FGameplayTag StateTag = FAGameplayTags::Get().State_BackwardOnly;

	if (bBackwardOnlyActive)
	{
		ASC->AddLooseGameplayTag(
			StateTag, 1, EGameplayTagReplicationState::CountToOwner);
	}
	else
	{
		ASC->RemoveLooseGameplayTag(
			StateTag, 1, EGameplayTagReplicationState::CountToOwner);
	}
}

void UFACharacterStatusComponent::StartHeartbeatDetection()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	ScanHeartbeatTargets();
	World->GetTimerManager().SetTimer(
		HeartbeatDetectionTimerHandle, this, &ThisClass::ScanHeartbeatTargets,
		FMath::Max(0.05f, HeartbeatDetectionInterval), true);
}

void UFACharacterStatusComponent::StopHeartbeatDetection()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HeartbeatDetectionTimerHandle);
	}
	HeartbeatRevealStartTime = -1.0;
	if (AFACharacter* Character = Cast<AFACharacter>(OwnerCharacter))
	{
		Character->SetHeartbeatRevealActive(false);
	}
}

void UFACharacterStatusComponent::ScanHeartbeatTargets()
{
	AFACharacter* Character = Cast<AFACharacter>(OwnerCharacter);
	UWorld* World = GetWorld();
	if (!Character || !World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// 표시 대상은 H41 보유자, 관찰자는 이 클라이언트의 좀비 플레이어다.
	const UFADetectionUnitComponent* Detection = Character->GetDetectionUnitComponent();
	UAbilitySystemComponent* ASC = GetOwnerAbilitySystem();
	const FAGameplayTags& Tags = FAGameplayTags::Get();
	const bool bInRange = Detection && ASC
		&& ASC->HasMatchingGameplayTag(Tags.Rule_State_Heartbeat)
		&& ASC->HasMatchingGameplayTag(Tags.State_Human)
		&& Detection->IsLocalPlayerInRange(Tags.State_Zombie, HeartbeatDetectionRadius);

	if (!bInRange)
	{
		HeartbeatRevealStartTime = -1.0;
		Character->SetHeartbeatRevealActive(false);
		return;
	}

	const double Now = World->GetTimeSeconds();
	if (HeartbeatRevealStartTime < 0.0)
	{
		HeartbeatRevealStartTime = Now;
	}
	const double Period = FMath::Max(0.1f, HeartbeatBlinkPeriod);
	const double VisibleDuration = FMath::Clamp(HeartbeatBlinkVisibleDuration, 0.0f, float(Period));
	Character->SetHeartbeatRevealActive(
		FMath::Fmod(Now - HeartbeatRevealStartTime, Period) < VisibleDuration);
}

void UFACharacterStatusComponent::HandleDecayRuleChanged(const FGameplayTag Tag, int32 NewCount)
{
	(void)Tag;

	// 배율 계산과 타이머는 서버에서 관리
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority())
	{
		return;
	}

	// 시작전 정리
	ClearDecay();

	UWorld* World = GetWorld();
	if (NewCount <= 0 || !World)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		DecayTimerHandle,
		this,
		&ThisClass::UpdateDecay,
		FMath::Max(0.05f, DecayUpdateInterval),
		true);

	// 룰 획득 즉시 현재 거리 반영
	UpdateDecay();
}

void UFACharacterStatusComponent::UpdateDecay()
{
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority())
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetOwnerAbilitySystem();
	const FAGameplayTags& Tags = FAGameplayTags::Get();

	if (!ASC || !ASC->HasMatchingGameplayTag(Tags.Rule_State_Decay))
	{
		ClearDecay();
		return;
	}

	float Multiplier = 1.0f;

	const AFACharacter* Character = Cast<AFACharacter>(OwnerCharacter);
	const UFADetectionUnitComponent* Detection = Character ? Character->GetDetectionUnitComponent() : nullptr;

	if (Detection && ASC->HasMatchingGameplayTag(Tags.State_Zombie))
	{
		const float Radius = FMath::Max(1.0f, DecayRadius);

		// 기존 탐지 컴포넌트 재사용
		TArray<AFACharacter*> Humans;
		Detection->GetCharactersInRange(
			Tags.State_Human, Radius, Humans);

		float NearestDistanceSquared = FMath::Square(Radius);
		const FVector OwnerLocation = OwnerCharacter->GetActorLocation();

		for (const AFACharacter* Human : Humans)
		{
			if (!IsValid(Human))
			{
				continue;
			}

			NearestDistanceSquared = FMath::Min(
				NearestDistanceSquared,
				FVector::DistSquared(
					OwnerLocation, Human->GetActorLocation()));
		}

		// 가장 가까운 인간 기준: 10m → 1.0배, 0m → 0.65배
		const float Distance = FMath::Sqrt(NearestDistanceSquared);
		const float Alpha =1.0f - FMath::Clamp(Distance / Radius, 0.0f, 1.0f);
		Multiplier = 1.0f- FMath::Clamp(DecayMaxSlowRatio, 0.0f, 1.0f) * Alpha;
	}

	// 인간이 없거나 소유자가 좀비가 아니면 1.0배
	ASC->SetNumericAttributeBase(UFAAttributeSet::GetDecaySpeedMultiplierAttribute(),Multiplier);
}

void UFACharacterStatusComponent::ClearDecay()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DecayTimerHandle);
	}

	if (!OwnerCharacter || !OwnerCharacter->HasAuthority())
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = GetOwnerAbilitySystem())
	{
		// 룰 제거 또는 종료 시 감속 해제
		ASC->SetNumericAttributeBase(UFAAttributeSet::GetDecaySpeedMultiplierAttribute(),1.0f);
	}
}

void UFACharacterStatusComponent::HandleHeartbeatRuleChanged(
	const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0)
	{
		// H41 보유 중에는 로컬 폰과 진영 태그가 늦게 준비되어도 반복 판정한다.
		StartHeartbeatDetection();
	}
	else
	{
		// 룰이 제거되면 타이머와 H41 아웃라인을 즉시 해제한다.
		StopHeartbeatDetection();
	}
}

void UFACharacterStatusComponent::HandleTrailRuleChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (UFATrailComponent* Trail = GetOwner()->FindComponentByClass<UFATrailComponent>())
	{
		if (NewCount > 0)
		{
			Trail->StartTrail();
		}
		else
		{
			Trail->StopTrail();
		}
	}
}

void UFACharacterStatusComponent::HandleLanded(const FHitResult& Hit)
{
	AActor* OwnerActor = GetOwner();

	// GE는 서버에서 한 번만 적용한다.
	if (!OwnerActor ||!OwnerActor->HasAuthority() ||!LandStunEffect)
	{
		return;
	}

	UAbilitySystemComponent* ASC =GetOwnerAbilitySystem();

	if (!ASC)
	{
		return;
	}

	const FAGameplayTags& Tags =FAGameplayTags::Get();

	// 착지 경직 룰을 가진 캐릭터에게만 적용한다.
	if (!ASC->HasMatchingGameplayTag(Tags.Rule_Move_LandStun))
	{
		return;
	}

	FGameplayEffectContextHandle Context =ASC->MakeEffectContext();

	Context.AddSourceObject(OwnerActor);

	// 지속시간이 아니라 GE 레벨
	const FGameplayEffectSpecHandle SpecHandle =ASC->MakeOutgoingSpec(LandStunEffect,1.0f,Context);

	if (!SpecHandle.IsValid() ||!SpecHandle.Data.IsValid())
	{
		return;
	}

	const FActiveGameplayEffectHandle ActiveEffectHandle =ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

	if (ActiveEffectHandle.IsValid())
	{
		//  성공 추가코드
	}
	else
	{
		// 실패 로그
		UE_LOG(LogTemp,Warning,TEXT("[LandStun] Apply failed: Owner=%s, Effect=%s"),*GetNameSafe(OwnerActor),
			*GetNameSafe(LandStunEffect.Get()));
	}
}

void UFACharacterStatusComponent::HandleSecondLifeRuleChanged(const FGameplayTag Tag, int32 NewCount)
{
	(void)Tag;
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority() || NewCount > 0)
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = GetOwnerAbilitySystem())
	{
		const FGameplayTag ConsumedTag = FAGameplayTags::Get().State_SecondLife_Consumed;
		if (ASC->HasMatchingGameplayTag(ConsumedTag))
		{
			ASC->RemoveLooseGameplayTag(ConsumedTag,1,EGameplayTagReplicationState::TagAndCountToAll);
		}
	}
}

void UFACharacterStatusComponent::UpdateHideRule(float DeltaTime)
{
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority())
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetOwnerAbilitySystem();
	if (!ASC)
	{
		return;
	}

	if (!ASC->HasMatchingGameplayTag(FAGameplayTags::Get().Rule_Trade_Hide))
	{
		HideStationaryElapsed = 0.0f;
		SetHideActive(false);
		return;
	}

	const UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	const bool bGrounded = Movement && !Movement->IsFalling();
	const bool bStationary = OwnerCharacter->GetVelocity().SizeSquared2D()
		<= FMath::Square(HideStationarySpeedTolerance);

	if (!bGrounded || !bStationary)
	{
		HideStationaryElapsed = 0.0f;
		SetHideActive(false);
		return;
	}

	HideStationaryElapsed += DeltaTime;
	if (HideStationaryElapsed >= HideDelay)
	{
		SetHideActive(true);
	}
}

void UFACharacterStatusComponent::SetHideActive(bool bActive)
{
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority())
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetOwnerAbilitySystem();
	if (!ASC)
	{
		return;
	}

	if (bHideGhostTagApplied == bActive)
	{
		return;
	}

	// 합산 태그 유무가 아닌 H45가 부여한 몫을 기준으로 중복 추가/제거를 방지한다.
	bHideGhostTagApplied = bActive;
	const FGameplayTag GhostTag = FAGameplayTags::Get().Rule_Trade_Ghost;
	if (bActive)
	{
		ASC->AddLooseGameplayTag(GhostTag,1,EGameplayTagReplicationState::TagAndCountToAll);
	}
	else
	{
		ASC->RemoveLooseGameplayTag(GhostTag,1,EGameplayTagReplicationState::TagAndCountToAll);
	}
	// 외형과 머리 위 UI는 복제되는 Ghost 태그를 공통으로 사용한다.
}

void UFACharacterStatusComponent::UpdateCampRule(float DeltaTime)
{
	if (!OwnerCharacter ||!OwnerCharacter->HasAuthority())
	{
		return;
	}

	UAbilitySystemComponent* ASC =GetOwnerAbilitySystem();

	if (!ASC)
	{
		return;
	}

	const FAGameplayTags& Tags =FAGameplayTags::Get();

	// H36 카드가 없으면 누적 시간과 노출 상태를 초기화한다.
	if (!ASC->HasMatchingGameplayTag(Tags.Rule_State_Camp))
	{
		CampStationaryElapsed = 0.0f;
		SetCampActive(false);
		return;
	}

	const UCharacterMovementComponent* Movement =OwnerCharacter->GetCharacterMovement();

	const bool bGrounded =Movement && !Movement->IsFalling();

	const bool bStationary = OwnerCharacter->GetVelocity().SizeSquared2D()
			<= FMath::Square(CampStationarySpeedTolerance);

	// 이동하거나 공중에 있으면 시간을 초기화하고 노출을 해제한다.
	if (!bGrounded || !bStationary)
	{
		CampStationaryElapsed = 0.0f;
		SetCampActive(false);
		return;
	}

	CampStationaryElapsed += DeltaTime;

	// 4초 이상 정지했을 때 State.Camp를 적용한다.
	SetCampActive(CampStationaryElapsed >= CampDelay);
}

void UFACharacterStatusComponent::SetCampActive(bool bActive)
{
	if (!OwnerCharacter ||!OwnerCharacter->HasAuthority())
	{
		return;
	}

	UAbilitySystemComponent* ASC =GetOwnerAbilitySystem();

	if (!ASC)
	{
		return;
	}

	const FGameplayTag CampTag =FAGameplayTags::Get().State_Camp;

	// 이미 원하는 상태라면 중복 처리하지 않는다.
	if (ASC->HasMatchingGameplayTag(CampTag) == bActive)
	{
		return;
	}

	if (bActive)
	{
		ASC->AddLooseGameplayTag(CampTag,1,EGameplayTagReplicationState::TagAndCountToAll);
	}
	else
	{
		ASC->RemoveLooseGameplayTag(CampTag,1,EGameplayTagReplicationState::TagAndCountToAll);
	}
}
void UFACharacterStatusComponent::RefreshGhostVisual()
{
	if (UAbilitySystemComponent* ASC = GetOwnerAbilitySystem())
	{
		const FAGameplayTags& Tags = FAGameplayTags::Get();
		SetGhostVisualActive(
			ASC->HasMatchingGameplayTag(Tags.Rule_Trade_Ghost));
	}
}

void UFACharacterStatusComponent::HandleGambleSpeedRuleChanged(const FGameplayTag Tag,int32 NewCount)
{
	(void)Tag;

	if (!OwnerCharacter || !OwnerCharacter->HasAuthority())
	{
		return;
	}

	ClearGambleSpeed();
	if (NewCount <= 0)
	{
		return;
	}

	// 룰 획득 즉시 한 번 적용하고, 이후 10초마다 새 배율로 교체한다.
	RollGambleSpeed();
	GetWorld()->GetTimerManager().SetTimer(GambleSpeedTimerHandle,
		this,&ThisClass::RollGambleSpeed,GambleSpeedInterval,true);
}

void UFACharacterStatusComponent::RollGambleSpeed()
{
	UAbilitySystemComponent* ASC = GetOwnerAbilitySystem();
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority() || !ASC
		|| !ASC->HasMatchingGameplayTag(FAGameplayTags::Get().Rule_Gamble_Speed))
	{
		ClearGambleSpeed();
		return;
	}

	const float MinMultiplier = FMath::Min(GambleSpeedMinMultiplier,GambleSpeedMaxMultiplier);
	const float MaxMultiplier = FMath::Max(GambleSpeedMinMultiplier,GambleSpeedMaxMultiplier);
	const float SpeedMultiplier = FMath::FRandRange(MinMultiplier,MaxMultiplier);
	ASC->SetNumericAttributeBase(UFAAttributeSet::GetGambleSpeedMultiplierAttribute(),SpeedMultiplier);
	MulticastPlayGambleSpeedEffect(SpeedMultiplier);
	const float BaseMoveSpeed = ASC->GetNumericAttribute(UFAAttributeSet::GetMoveSpeedAttribute());

	UE_LOG(LogTemp,Warning,TEXT("[H50] 속도 갱신 | Character=%s | "
		  "BaseSpeed=%.1f | Multiplier=%.3f | FinalSpeed=%.1f"
	  ),*GetNameSafe(OwnerCharacter),BaseMoveSpeed,SpeedMultiplier,BaseMoveSpeed * SpeedMultiplier);
}

void UFACharacterStatusComponent::MulticastPlayGambleSpeedEffect_Implementation(float SpeedMultiplier)
{
	if (GetNetMode() == NM_DedicatedServer || FMath::IsNearlyEqual(SpeedMultiplier, 1.0f))
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UNiagaraSystem* Effect = SpeedMultiplier > 1.0f ? GambleSpeedUpEffect.Get() : GambleSpeedDownEffect.Get();
	if (!Character || !Effect)
	{
		return;
	}

	UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	if (!Capsule)
	{
		return;
	}

	const FVector Location = GambleSpeedEffectOffset
		- FVector(0.0f, 0.0f, Capsule->GetUnscaledCapsuleHalfHeight());
	UNiagaraFunctionLibrary::SpawnSystemAttached(Effect, Capsule, NAME_None, Location,
		FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true);
}

void UFACharacterStatusComponent::ClearGambleSpeed()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GambleSpeedTimerHandle);
	}

	if (UAbilitySystemComponent* ASC = GetOwnerAbilitySystem(); ASC && OwnerCharacter
		&& OwnerCharacter->HasAuthority())
	{
		ASC->SetNumericAttributeBase(UFAAttributeSet::GetGambleSpeedMultiplierAttribute(),1.0f);
	}
}

void UFACharacterStatusComponent::HandleForceScreamRuleChanged(const FGameplayTag Tag, int32 NewCount)
{
	// 반복 타이머는 서버에서만 관리한다.
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority())
	{
		return;
	}

	// 기존 타이머가 있다면 먼저 제거.
	ClearForcedScream();

	if (NewCount <= 0)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 룰 획득 8초 후 첫 비명, 이후 8초마다 반복.
	World->GetTimerManager().SetTimer(ForceScreamTimerHandle, this, &ThisClass::PlayForcedScream, ForceScreamInterval, true);
}

void UFACharacterStatusComponent::PlayForcedScream()
{
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority())
	{
		ClearForcedScream();
		return;
	}

	UAbilitySystemComponent* ASC = GetOwnerAbilitySystem();
	if (!ASC || !ASC->HasMatchingGameplayTag(FAGameplayTags::Get().Rule_Social_ForceScream))
	{
		ClearForcedScream();
		return;
	}

	AFACharacter* FACharacter = Cast<AFACharacter>(OwnerCharacter);
	if (!FACharacter)
	{
		return;
	}

	FACharacter->Multicast_PlayFASoundAtLocation(ForceScreamSoundId);
}

void UFACharacterStatusComponent::ClearForcedScream()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ForceScreamTimerHandle);
	}
}

void UFACharacterStatusComponent::HandleRecallRuleChanged(const FGameplayTag Tag, int32 NewCount)
{
	// 이벤트 함수 형식을 맞추기 위한 인자
	(void)Tag;

	// 타이머와 순간이동 위치는 서버에서만 관리한다.
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority())
	{
		return;
	}

	// 중복 타이머 방지
	ClearRecall();

	// 공용 귀환 태그가 제거된 경우 타이머를 다시 시작하지 않는다.
	if (NewCount <= 0)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 즉시 순간이동하지 않고 40초 뒤 첫 순간이동을 실행한다.
	World->GetTimerManager().SetTimer(RecallTimerHandle,
		this,&ThisClass::RecallToRandomPlayerStart,
		RecallInterval,true);

	UE_LOG(LogTemp,Log,TEXT("[Recall H43/Z40] 귀환 시작 | Character=%s | Interval=%.1f"),
		*GetNameSafe(OwnerCharacter),RecallInterval);
}

void UFACharacterStatusComponent::RecallToRandomPlayerStart()
{
	 UAbilitySystemComponent* ASC = GetOwnerAbilitySystem();

      // 서버가 아니거나 귀환 태그가 제거된 경우 타이머를 종료한다.
      if (!OwnerCharacter|| !OwnerCharacter->HasAuthority()
          || !ASC|| !ASC->HasMatchingGameplayTag(FAGameplayTags::Get().Rule_State_Recall))
      {
          ClearRecall();
          return;
      }

      UWorld* World = GetWorld();
      if (!World)
      {
          return;
      }

      // 인간과 좀비가 공유하는 스폰 지점의 태그.
      static const FName HumanPlayerStartTag(TEXT("Human"));

      TArray<APlayerStart*> Candidates;

      // 현재 맵의 모든 PlayerStart를 순회한다.
      for (TActorIterator<APlayerStart> Iterator(World);Iterator;++Iterator)
      {
          APlayerStart* PlayerStart = *Iterator;

          if (!IsValid(PlayerStart))
          {
              continue;
          }

          // 일반 라운드에서 DeathMatch 전용 스폰을 사용하지 않는다.
          if (UFADeathMatchSubsystem::IsArenaPlayerStart(
                  PlayerStart))
          {
              continue;
          }

          // PlayerStartTag 또는 ActorTag가 Human인 지점만 사용한다.
          if (!UFADeathMatchSubsystem::PlayerStartHasTag(PlayerStart,HumanPlayerStartTag))
          {
              continue;
          }

          Candidates.Add(PlayerStart);
      }

      if (Candidates.IsEmpty())
      {
          // 순간이동 실패
          return;
      }

      // 막힌 지점이 있을 수 있으므로 성공하거나 후보가 소진될 때까지 시도한다.
      while (!Candidates.IsEmpty())
      {
          const int32 RandomIndex =
              FMath::RandRange(0, Candidates.Num() - 1);

          APlayerStart* SelectedStart =
              Candidates[RandomIndex];

          // 같은 지점을 다시 시도하지 않도록 제거한다.
          Candidates.RemoveAtSwap(RandomIndex);

          const FVector Destination =SelectedStart->GetActorLocation();
          const FRotator DestinationRotation =SelectedStart->GetActorRotation();

          // 마지막 false:
          // 충돌 검사를 무시하지 않는다.
          const bool bTeleported = OwnerCharacter->TeleportTo(
              Destination,DestinationRotation,false,false);

          if (!bTeleported)
          {
              // 막힌 지점이면 다른 후보를 시도한다.
              continue;
          }

          UE_LOG(LogTemp,Warning,TEXT("[Recall H43/Z40] 순간이동 성공 | Character=%s | "
                  "PlayerStart=%s | Location=%s"
              ),
              *GetNameSafe(OwnerCharacter),
              *GetNameSafe(SelectedStart),
              *Destination.ToCompactString()
          );

          return;
      }

}

void UFACharacterStatusComponent::ClearRecall()
{
	if (UWorld* World = GetWorld())
	{
		// 귀환 태그가 제거되거나 캐릭터가 종료될 때 반복 타이머를 정리한다.
		World->GetTimerManager().ClearTimer(RecallTimerHandle);
	}
}

void UFACharacterStatusComponent::HandleForceNoiseRuleChanged(const FGameplayTag Tag, int32 NewCount)
{
	// 반복 소음은 서버에서만 관리한다.
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority())
	{
		return;
	}

	ClearForcedNoise();

	if (NewCount <= 0)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 첫 소음은 3초 뒤, 이후 3초마다 반복.
	World->GetTimerManager().SetTimer(ForceNoiseTimerHandle, this, &ThisClass::PlayForcedNoise, ForceNoiseInterval, true);
}

void UFACharacterStatusComponent::PlayForcedNoise()
{
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority())
	{
		ClearForcedNoise();
		return;
	}

	UAbilitySystemComponent* ASC = GetOwnerAbilitySystem();

	if (!ASC || !ASC->HasMatchingGameplayTag(FAGameplayTags::Get().Rule_Social_ForceNoise))
	{
		ClearForcedNoise();
		return;
	}

	AFACharacter* FACharacter = Cast<AFACharacter>(OwnerCharacter);
	if (!FACharacter)
	{
		return;
	}

	FACharacter->Multicast_PlayFASoundAtLocation(ForceNoiseSoundId);
}

void UFACharacterStatusComponent::ClearForcedNoise()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ForceNoiseTimerHandle);
	}
}

UAbilitySystemComponent* UFACharacterStatusComponent::GetOwnerAbilitySystem() const
{
	const IAbilitySystemInterface* AbilityInterface =Cast<IAbilitySystemInterface>(GetOwner());

	return AbilityInterface? AbilityInterface->GetAbilitySystemComponent(): nullptr;
}

void UFACharacterStatusComponent::HandleDarkRuleChanged(FGameplayTag Tag, int32 NewCount)
{
	UpdateDarkWidget(NewCount > 0);
}

void UFACharacterStatusComponent::HandleDarkControllerChanged(APawn* Pawn, AController* OldController, AController* NewController)
{
	// 태그가 컨트롤러보다 먼저 도착한 경우와 소유권 상실도 처리한다.
	const UAbilitySystemComponent* ASC = GetOwnerAbilitySystem();
	UpdateDarkWidget(ASC && ASC->HasMatchingGameplayTag(FAGameplayTags::Get().Rule_Vision_Dark));
}

void UFACharacterStatusComponent::UpdateDarkWidget(bool bActive)
{
	APlayerController* LocalController = OwnerCharacter ? Cast<APlayerController>(OwnerCharacter->GetController()) : nullptr;
	if (!bActive || !LocalController || !LocalController->IsLocalController())
	{
		if (VisionDarkWidget)
		{
			VisionDarkWidget->RemoveFromParent();
			VisionDarkWidget = nullptr;
		}
		return;
	}

	// 소유권이 바뀌면 이전 플레이어의 화면에서 먼저 제거한다.
	if (VisionDarkWidget && VisionDarkWidget->GetOwningPlayer() != LocalController)
	{
		UpdateDarkWidget(false);
	}
	if (!VisionDarkWidget && VisionDarkWidgetClass)
	{
		VisionDarkWidget = CreateWidget<UUserWidget>(LocalController, VisionDarkWidgetClass);
		if (VisionDarkWidget)
		{
			VisionDarkWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
			VisionDarkWidget->AddToViewport(VisionDarkWidgetZOrder);
		}
	}
}
