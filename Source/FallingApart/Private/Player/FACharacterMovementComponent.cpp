// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/FACharacterMovementComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AttributeSet/FAAttributeSet.h"
#include "GAS/FAGameplayTags.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"

UFACharacterMovementComponent::UFACharacterMovementComponent()
{
	// AttributeSet 기본값(MoveSpeed 600 / GroundFriction 8.0 / AirControl 0.35).
	MaxWalkSpeed = 600.0f;
	GroundFriction = 8.0f;
	AirControl = 0.35f;
}

void UFACharacterMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();

	DefaultGroundFriction = GroundFriction;
}

UAbilitySystemComponent* UFACharacterMovementComponent::GetOwnerASC() const
{
	if (CachedASC.IsValid())
	{
		return CachedASC.Get();
	}

	if (const IAbilitySystemInterface* AbilityInterface = Cast<IAbilitySystemInterface>(GetOwner()))
	{
		UAbilitySystemComponent* ASC = AbilityInterface->GetAbilitySystemComponent();
		CachedASC = ASC;
		return ASC;
	}

	return nullptr;
}

const UFAAttributeSet* UFACharacterMovementComponent::GetOwnerAttributes() const
{
	const UAbilitySystemComponent* ASC = GetOwnerASC();
	return ASC ? ASC->GetSet<UFAAttributeSet>() : nullptr;
}

bool UFACharacterMovementComponent::HasOwnerTag(const FGameplayTag& Tag) const
{
	const UAbilitySystemComponent* ASC = GetOwnerASC();
	return ASC && ASC->HasMatchingGameplayTag(Tag);
}

bool UFACharacterMovementComponent::IsMovementFrozen() const
{
	const UAbilitySystemComponent* ASC = GetOwnerASC();
	if (!ASC)
	{
		return false;
	}

	const FAGameplayTags& Tags = FAGameplayTags::Get();

	// State.Stunned 는 경련(Z07/H06)·착지 경직·QTE 패배 경직이 공유하는 창구
	// 새로운 경직 룰은 태그를 새로 만들지 말고 이 태그를 1.5초짜리 GE 로 붙일 것.
	return ASC->HasMatchingGameplayTag(Tags.State_Stunned)
		|| ASC->HasMatchingGameplayTag(Tags.State_Contesting);
}

float UFACharacterMovementComponent::GetMaxSpeed() const
{
	if (IsMovementFrozen())
	{
		return 0.0f;
	}

	const UFAAttributeSet* Attributes = GetOwnerAttributes();
	if (!Attributes)
	{
		return Super::GetMaxSpeed();
	}

	// 속도 계열 룰은 전부 이 Attribute 하나만 건드린다
	// -15% 와 -30% 를 동시에 들면 GAS 배수 합산 규칙에 따라 -45% 가 된다.
	const float RuleSpeed = FMath::Max(0.0f, Attributes->GetMoveSpeed());

	switch (MovementMode)
	{
	case MOVE_Walking:
	case MOVE_NavWalking:
		// 웅크리기 속도는 룰 속도를 넘지 않는 선에서 유지
		return IsCrouching() ? FMath::Min(MaxWalkSpeedCrouched, RuleSpeed) : RuleSpeed;

	case MOVE_Falling:
		return RuleSpeed;

	default:
		// 수영·비행은 이 게임에 없다. 생기면 여기서 분기할 것.
		return Super::GetMaxSpeed();
	}
}

bool UFACharacterMovementComponent::CanAttemptJump() const
{
	if (!Super::CanAttemptJump())
	{
		return false;
	}

	if (IsMovementFrozen())
	{
		return false;
	}

	return !HasOwnerTag(FAGameplayTags::Get().Rule_Move_NoJump);
}

void UFACharacterMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	if (IsMovementFrozen())
	{
		// 경련은 '천천히 멈추는' 게 아니라 '굳는' 것. 감속을 기다리지 않고 즉시 정지시킨다.
		Velocity = FVector::ZeroVector;
		Acceleration = FVector::ZeroVector;
		return;
	}

	if (IsMovingOnGround() && DefaultGroundFriction > UE_KINDA_SMALL_NUMBER)
	{
		if (const UFAAttributeSet* Attributes = GetOwnerAttributes())
		{
			const float RuleFriction = FMath::Max(0.0f, Attributes->GetGroundFriction());

			Friction = RuleFriction;
			BrakingDeceleration *= (RuleFriction / DefaultGroundFriction);
		}
	}

	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
}

FVector UFACharacterMovementComponent::GetAirControl(float DeltaTime, float TickAirControl, const FVector& FallAcceleration)
{
	if (const UFAAttributeSet* Attributes = GetOwnerAttributes())
	{
		// Z09/H08 공중 마비는 이 값을 0 으로 만든다.
		TickAirControl = FMath::Clamp(Attributes->GetAirControl(), 0.0f, 1.0f);
		
		const float AttributeAirControl = Attributes->GetAirControl();
		
	}
	return Super::GetAirControl(DeltaTime, TickAirControl, FallAcceleration);
}

FVector UFACharacterMovementComponent::ConstrainInputAcceleration(const FVector& InputAcceleration) const
{
	FVector Result = InputAcceleration;

	if (IsMovementFrozen())
	{
		return FVector::ZeroVector;
	}
	

	const FAGameplayTags& Tags = FAGameplayTags::Get();

	if (HasOwnerTag(Tags.State_Input_Flipped))
	{
		Result = ComputeFlippedLRAcceleration(Result);
	}

	if (HasOwnerTag(Tags.Rule_Move_ForceForward) && (IsMovingOnGround() || IsFalling()))
	{
		// 좌우 반전과 자동 전진이 함께 적용되면 반전된 조향값을 사용한다.
		Result = ComputeForceForwardAcceleration(Result);
	}

	return Super::ConstrainInputAcceleration(Result);
}

FVector UFACharacterMovementComponent::ComputeFlippedLRAcceleration(const FVector& InputAcceleration) const
{
	const AController* OwnerController = CharacterOwner ? CharacterOwner->GetController() : nullptr;

	const float Yaw = OwnerController
		? OwnerController->GetControlRotation().Yaw
		: (UpdatedComponent ? UpdatedComponent->GetComponentRotation().Yaw : 0.0f);

	const FRotationMatrix YawMatrix(FRotator(0.0f, Yaw, 0.0f));
	const FVector Forward = YawMatrix.GetUnitAxis(EAxis::X);
	const FVector Right = YawMatrix.GetUnitAxis(EAxis::Y);

	const float ForwardAmount = FVector::DotProduct(InputAcceleration, Forward);
	const float RightAmount = FVector::DotProduct(InputAcceleration, Right);
	const float VerticalAmount = FVector::DotProduct(InputAcceleration, FVector::UpVector);

	// 지면 이동 성분을 모두 반전한다. 수직 성분은 점프/중력에 영향을 주지 않도록 유지한다.
	return -Forward * ForwardAmount - Right * RightAmount + FVector::UpVector * VerticalAmount;
}

FVector UFACharacterMovementComponent::ComputeForceForwardAcceleration(const FVector& InputAcceleration) const
{
	const AController* OwnerController = CharacterOwner ? CharacterOwner->GetController() : nullptr;

	const float Yaw = OwnerController
		? OwnerController->GetControlRotation().Yaw
		: (UpdatedComponent ? UpdatedComponent->GetComponentRotation().Yaw : 0.0f);

	const FRotationMatrix YawMatrix(FRotator(0.0f, Yaw, 0.0f));
	const FVector Forward = YawMatrix.GetUnitAxis(EAxis::X);
	const FVector Right = YawMatrix.GetUnitAxis(EAxis::Y);

	const float SteerAmount = FMath::Clamp(FVector::DotProduct(InputAcceleration.GetSafeNormal(), Right), -1.0f, 1.0f);
	const FVector Direction = (Forward + Right * SteerAmount).GetSafeNormal();

	return Direction * GetMaxAcceleration();
}
