// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/FACharacterStatusComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AttributeSet/FAAttributeSet.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"
#include "GAS/FAGameplayTags.h"
#include "TimerManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Rule/FADeathMatchSubsystem.h"
#include "Player/FACharacter.h"
#include "Player/FADetectionUnitComponent.h"
#include "Player/FATrailComponent.h"

// Sets default values for this component's properties
UFACharacterStatusComponent::UFACharacterStatusComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// [H45 RPC 수정] 컴포넌트의 Multicast RPC를 전달하도록 복제를 활성화한다.
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
	OwnerCharacter->LandedDelegate.AddDynamic(this,&ThisClass::HandleLanded);

	if (UAbilitySystemComponent* ASC = GetOwnerAbilitySystem())
	{
		const FGameplayTag FlipRuleTag = FAGameplayTags::Get().Rule_Input_FlipLR;
		ASC->RegisterGameplayTagEvent(FlipRuleTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleInputFlipRuleChanged);

		// BeginPlay 전에 룰이 이미 적용된 경우도 처리한다.
		HandleInputFlipRuleChanged(FlipRuleTag, ASC->GetTagCount(FlipRuleTag));

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

		const FGameplayTag WallPhaseRuleTag =FAGameplayTags::Get().Rule_Trade_WallPhase;
		ASC->RegisterGameplayTagEvent(WallPhaseRuleTag,EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this,&ThisClass::HandleWallPhaseRuleChanged);
		// BeginPlay 전에 Z46이 적용된 경우도 처리한다.
		HandleWallPhaseRuleChanged(WallPhaseRuleTag,ASC->GetTagCount(WallPhaseRuleTag));
		const FGameplayTag HiddenStateTag = FAGameplayTags::Get().State_Hidden;
		ASC->RegisterGameplayTagEvent(HiddenStateTag,EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this,&ThisClass::HandleHiddenStateChanged);
		HandleHiddenStateChanged(HiddenStateTag,ASC->GetTagCount(HiddenStateTag));

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


		const FGameplayTag DecayTag = FAGameplayTags::Get().Rule_State_Decay;
		ASC->RegisterGameplayTagEvent(DecayTag, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleDecayRuleChanged);

		HandleDecayRuleChanged(DecayTag, ASC->GetTagCount(DecayTag));
	}
}

void UFACharacterStatusComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAbilitySystemComponent* ASC = GetOwnerAbilitySystem())
	{
		ASC->RegisterGameplayTagEvent(
			FAGameplayTags::Get().Rule_Input_FlipLR,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);

		ASC->RegisterGameplayTagEvent(FAGameplayTags::Get().Rule_Move_CrouchOnly,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);

		ASC->RegisterGameplayTagEvent(FAGameplayTags::Get().Rule_Trade_Ghost,
				EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		ASC->RegisterGameplayTagEvent(FAGameplayTags::Get().Rule_Trade_WallPhase,
		 EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		ASC->RegisterGameplayTagEvent(FAGameplayTags::Get().State_Hidden,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		ASC->RegisterGameplayTagEvent(FAGameplayTags::Get().Rule_Trade_SecondLife,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		ASC->RegisterGameplayTagEvent(FAGameplayTags::Get().Rule_Gamble_Speed,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		ASC->RegisterGameplayTagEvent(FAGameplayTags::Get().Rule_State_Recall,
		  EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		ASC->RegisterGameplayTagEvent(FAGameplayTags::Get().Rule_State_Heartbeat,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		ASC->RegisterGameplayTagEvent(FAGameplayTags::Get().Rule_State_Trail,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);

		ASC->RegisterGameplayTagEvent(FAGameplayTags::Get().Rule_State_Decay,
	  EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
	}

	GetWorld()->GetTimerManager().ClearTimer(InputFlipTimerHandle);
	SetInputFlipActive(false);
	ClearGambleSpeed();
	ClearRecall();
	StopHeartbeatDetection();
	// 저게서
	SetGhostVisualActive(false);
	GhostDynamicMaterials.Reset();

	if (OwnerCharacter)
	{
		OwnerCharacter->LandedDelegate.RemoveDynamic(this,&ThisClass::HandleLanded);
	}
	ClearDecay();
	Super::EndPlay(EndPlayReason);
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
	}

	const bool bShouldPhase =ASC->HasMatchingGameplayTag(
			FAGameplayTags::Get().Rule_Trade_WallPhase);

	const ECollisionResponse DesiredResponse =bShouldPhase ? ECR_Ignore : ECR_Block;

	if (Capsule->GetCollisionResponseToChannel(FACollision::PhaseWall) != DesiredResponse)
	{
		SetWallPhaseActive(bShouldPhase);
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

	/*
	 * NewCount > 0:
	 * Rule.Trade.Ghost 태그 적용 상태
	 * → 반투명 처리
	 *
	 * NewCount == 0:
	 * 태그 미적용 또는 제거 상태
	 * → 완전히 보이는 상태로 복구
	 */
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
	const UFADetectionUnitComponent* Detection =
		Character ? Character->GetDetectionUnitComponent() : nullptr;

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

	const FGameplayTag HiddenTag = FAGameplayTags::Get().State_Hidden;
	if (ASC->HasMatchingGameplayTag(HiddenTag) == bActive)
	{
		return;
	}

	if (bActive)
	{
		ASC->AddLooseGameplayTag(HiddenTag,1,EGameplayTagReplicationState::TagAndCountToAll);
	}
	else
	{
		ASC->RemoveLooseGameplayTag(HiddenTag,1,EGameplayTagReplicationState::TagAndCountToAll);
	}

	// [H45 RPC 추가] 태그 도착 순서에 의존하지 않고 현재 은둔 상태를 전달한다.
	MulticastSetHideVisual(bActive);
}

void UFACharacterStatusComponent::MulticastSetHideVisual_Implementation(bool bActive)
{
	// [H45 RPC 추가] 전용 서버는 렌더링하지 않는다.
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!OwnerCharacter)
	{
		OwnerCharacter = Cast<ACharacter>(GetOwner());
	}

	const UAbilitySystemComponent* ASC = GetOwnerAbilitySystem();
	const bool bGhostRuleActive = ASC
		&& ASC->HasMatchingGameplayTag(FAGameplayTags::Get().Rule_Trade_Ghost);

	// 은둔이 해제돼도 별도 Ghost 룰의 반투명 효과는 유지한다.
	SetGhostVisualActive(bActive || bGhostRuleActive);

	UE_LOG(LogTemp, Log, TEXT("[H45 RPC] Character=%s | NetMode=%d | Hidden=%d"),
		*GetNameSafe(OwnerCharacter), static_cast<int32>(GetNetMode()), bActive);
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
void UFACharacterStatusComponent::HandleHiddenStateChanged(const FGameplayTag Tag, int32 NewCount)
{
	(void)Tag;
	(void)NewCount;
	RefreshGhostVisual();
}

void UFACharacterStatusComponent::RefreshGhostVisual()
{
	if (UAbilitySystemComponent* ASC = GetOwnerAbilitySystem())
	{
		const FAGameplayTags& Tags = FAGameplayTags::Get();
		SetGhostVisualActive(
			ASC->HasMatchingGameplayTag(Tags.Rule_Trade_Ghost)
			|| ASC->HasMatchingGameplayTag(Tags.State_Hidden));
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

	const float BaseMoveSpeed = ASC->GetNumericAttribute(UFAAttributeSet::GetMoveSpeedAttribute());

	UE_LOG(LogTemp,Warning,TEXT("[H50] 속도 갱신 | Character=%s | "
		  "BaseSpeed=%.1f | Multiplier=%.3f | FinalSpeed=%.1f"
	  ),*GetNameSafe(OwnerCharacter),BaseMoveSpeed,SpeedMultiplier,BaseMoveSpeed * SpeedMultiplier);
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

UAbilitySystemComponent* UFACharacterStatusComponent::GetOwnerAbilitySystem() const
{
	const IAbilitySystemInterface* AbilityInterface =Cast<IAbilitySystemInterface>(GetOwner());

	return AbilityInterface? AbilityInterface->GetAbilitySystemComponent(): nullptr;
}
