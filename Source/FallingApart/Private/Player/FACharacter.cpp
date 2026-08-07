// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/FACharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/InputComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Voice/FAVoiceComponent.h"

AFACharacter::AFACharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	bIsFirstPerson = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 500.f, 0.f);
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
	GetCharacterMovement()->MaxWalkSpeedCrouched = 300.f;

	ThirdPersonArmLength = 300.f;
	CameraInterpSpeed = 12.f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = ThirdPersonArmLength;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = CameraInterpSpeed;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// 3인칭이어도 오디오 리스너는 카메라가 아니라 캐릭터 헤드에 붙는다(기획 확정).
	// 카메라/스프링암 쪽에서 SetAudioListenerOverride 를 호출하면 이 동작이 깨진다.
	VoiceComponent = CreateDefaultSubobject<UFAVoiceComponent>(TEXT("VoiceComponent"));
}

void AFACharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsFirstPerson && !GetWorldTimerManager().IsTimerActive(PerspectiveTimerHandle))
	{
		UpdateCameraCollision(DeltaTime);
	}
}

void AFACharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AFACharacter::Move);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AFACharacter::Look);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AFACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AFACharacter::StopJumping);
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &AFACharacter::ToggleCrouch);
		EnhancedInputComponent->BindAction(PerspectiveAction, ETriggerEvent::Started, this, &AFACharacter::TogglePerspective);
		EnhancedInputComponent->BindAction(PrimaryAction, ETriggerEvent::Triggered, this, &AFACharacter::TriggerPrimary);
	}
}

void AFACharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	CameraBoom->TargetOffset.Z += HalfHeightAdjust;

	GetWorldTimerManager().ClearTimer(CrouchTimerHandle);

	if (!GetWorldTimerManager().IsTimerActive(CrouchTimerHandle))
	{
		GetWorldTimerManager().SetTimer(CrouchTimerHandle, this, &AFACharacter::UpdateCrouchCamera, GetWorld()->GetDeltaSeconds(), true);
	}
}

void AFACharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	CameraBoom->TargetOffset.Z -= HalfHeightAdjust;

	GetWorldTimerManager().ClearTimer(CrouchTimerHandle);

	if (!GetWorldTimerManager().IsTimerActive(CrouchTimerHandle))
	{
		GetWorldTimerManager().SetTimer(CrouchTimerHandle, this, &AFACharacter::UpdateCrouchCamera, GetWorld()->GetDeltaSeconds(), true);
	}
}

void AFACharacter::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AFACharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void AFACharacter::ToggleCrouch()
{
	if (GetCharacterMovement()->IsFalling())
		return;

	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

void AFACharacter::UpdateCrouchCamera()
{
	CameraBoom->TargetOffset.Z = FMath::FInterpTo(CameraBoom->TargetOffset.Z, 0.f, GetWorld()->GetDeltaSeconds(), CameraInterpSpeed);

	if (FMath::IsNearlyEqual(CameraBoom->TargetOffset.Z, 0.f, 1.f))
	{
		CameraBoom->TargetOffset.Z = 0.f;
		GetWorldTimerManager().ClearTimer(CrouchTimerHandle);
	}
}

void AFACharacter::TogglePerspective()
{
	bIsFirstPerson = !bIsFirstPerson;
	GetWorldTimerManager().ClearTimer(PerspectiveTimerHandle);

	if (!GetWorldTimerManager().IsTimerActive(PerspectiveTimerHandle))
	{
		GetWorldTimerManager().SetTimer(PerspectiveTimerHandle, this, &AFACharacter::UpdatePerspectiveCamera, GetWorld()->GetDeltaSeconds(), true);
	}
}

void AFACharacter::UpdatePerspectiveCamera()
{
	float DeltaTime = GetWorld()->GetDeltaSeconds();
	float TargetArmLength = bIsFirstPerson ? 0.f : ThirdPersonArmLength;
	float TargetSocketOffsetZ = bIsFirstPerson ? BaseEyeHeight : 0.f;

	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetArmLength, DeltaTime, CameraInterpSpeed);
	CameraBoom->SocketOffset.Z = FMath::FInterpTo(CameraBoom->SocketOffset.Z, TargetSocketOffsetZ, DeltaTime, CameraInterpSpeed);

	GetMesh()->SetOwnerNoSee(CameraBoom->TargetArmLength < 40.f);

	if (FMath::IsNearlyEqual(CameraBoom->TargetArmLength, TargetArmLength, 1.f))
	{
		CameraBoom->TargetArmLength = TargetArmLength;
		CameraBoom->SocketOffset.Z = TargetSocketOffsetZ;
		GetWorldTimerManager().ClearTimer(PerspectiveTimerHandle);
	}
}

void AFACharacter::UpdateCameraCollision(float DeltaTime)
{
	float TargetArmLength = ThirdPersonArmLength;
	FVector Start = CameraBoom->GetComponentLocation();
	FVector Direction = (FollowCamera->GetComponentLocation() - Start).GetSafeNormal();

	if (Direction.IsNearlyZero())
	{
		Direction = -CameraBoom->GetForwardVector();
	}

	FVector End = Start + (Direction * ThirdPersonArmLength);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams(FName("CameraCollision"), false, this);
	QueryParams.AddIgnoredActor(this);


	bool bHit = GetWorld()->SweepSingleByChannel(
		HitResult,
		Start,
		End,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(CameraBoom->ProbeSize),
		QueryParams
	);

	if (bHit)
	{
		TargetArmLength = FMath::Clamp(HitResult.Distance - CameraBoom->ProbeSize, 30.f, ThirdPersonArmLength);
	}

	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetArmLength, DeltaTime, CameraInterpSpeed);
}

void AFACharacter::TriggerPrimary()
{

}
