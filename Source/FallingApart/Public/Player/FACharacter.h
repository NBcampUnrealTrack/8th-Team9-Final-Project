// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "FACharacter.generated.h"

class UInputAction;
class UFAVoiceComponent;

UCLASS()
class FALLINGAPART_API AFACharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AFACharacter();

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** 근접 보이스. 감쇠·리스너·발화 아이콘은 전부 컴포넌트 안에서 처리된다. */
	UFAVoiceComponent* GetVoiceComponent() const { return VoiceComponent; }

protected:
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void ToggleCrouch();
	void TogglePerspective();
	void UpdatePerspectiveCamera();
	void TriggerPrimary();

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class UCameraComponent* FollowCamera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float ThirdPersonArmLength;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float PerspectiveInterpSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* CrouchAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* PerspectiveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* PrimaryAction;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FallingApart|Voice", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFAVoiceComponent> VoiceComponent;

	bool bIsFirstPerson;
	FTimerHandle PerspectiveTimerHandle;
};
