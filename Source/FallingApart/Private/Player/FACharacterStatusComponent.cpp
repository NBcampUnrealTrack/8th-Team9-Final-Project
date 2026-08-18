// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/FACharacterStatusComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"
#include "GAS/FAGameplayTags.h"
#include "TimerManager.h"

// Sets default values for this component's properties
UFACharacterStatusComponent::UFACharacterStatusComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	
	// 컴포넌트 자체 상태는 복제할 필요가 없다.
	// 적용된 GE와 GameplayTag는 ASC가 복제한다.
	SetIsReplicatedByDefault(false);
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

		// [H07 포복 수정안 — 설명용 주석이므로 현재 실행되지 않음]
		// CrouchOnly 룰이 붙거나 제거되는 순간을 감지한다.
		// 실제 적용할 때 아래 코드의 주석을 해제한다.
		// [ 포복 ]
		const FGameplayTag CrouchOnlyRuleTag = FAGameplayTags::Get().Rule_Move_CrouchOnly;
		ASC->RegisterGameplayTagEvent(CrouchOnlyRuleTag, EGameplayTagEventType::NewOrRemoved)
		 	.AddUObject(this, &ThisClass::HandleCrouchOnlyRuleChanged);
		
		// BeginPlay 전에 이미 룰이 적용된 경우에도 즉시 포복시킨다.
		 HandleCrouchOnlyRuleChanged(CrouchOnlyRuleTag,ASC->GetTagCount(CrouchOnlyRuleTag));
	}
	
	
}

void UFACharacterStatusComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAbilitySystemComponent* ASC = GetOwnerAbilitySystem())
	{
		ASC->RegisterGameplayTagEvent(
			FAGameplayTags::Get().Rule_Input_FlipLR,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);

		//[H07 포복 수정안 — 실제 적용할 때 아래 코드의 주석을 해제한다]
		ASC->RegisterGameplayTagEvent(FAGameplayTags::Get().Rule_Move_CrouchOnly,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
	}

	GetWorld()->GetTimerManager().ClearTimer(InputFlipTimerHandle);
	SetInputFlipActive(false);

	if (OwnerCharacter)
	{
		OwnerCharacter->LandedDelegate.RemoveDynamic(this,&ThisClass::HandleLanded);
	}

	Super::EndPlay(EndPlayReason);
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

// [H07 포복 수정안 — 설명용 주석이므로 현재 실행되지 않음]
// 아래 구현을 활성화하면 서버에서 CrouchOnly 태그가 붙는 즉시 포복하고,
// 태그가 제거되면 다시 일어선다. Crouch 상태는 ACharacter가 복제한다.
//
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

UAbilitySystemComponent* UFACharacterStatusComponent::GetOwnerAbilitySystem() const
{
	const IAbilitySystemInterface* AbilityInterface =Cast<IAbilitySystemInterface>(GetOwner());
	
	return AbilityInterface? AbilityInterface->GetAbilitySystemComponent(): nullptr;
}
