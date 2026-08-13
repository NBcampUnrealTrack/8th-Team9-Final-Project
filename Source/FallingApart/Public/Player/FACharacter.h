// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Game/FAGameTypes.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "FACharacter.generated.h"

class UFAAbilitySystemComponent;
class UFACharacterMovementComponent;
class UFAAttributeSet;
class UFAGameplayAbility;
class UGameplayEffect;
class UInputAction;
class UFAVoiceComponent;
class UAudioComponent;
class AFAPlayerState;
class UFACharacterStatusComponent;

UCLASS()
class FALLINGAPART_API AFACharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AFACharacter(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	//~ IAbilitySystemInterface — 룰 GE·태그·Attribute 의 주인.
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** 진영 전환·룰 부착은 전부 이 컴포넌트를 통한다 */
	UFAAbilitySystemComponent* GetFAAbilitySystemComponent() const { return AbilitySystemComponent; }

	/** 룰 Attribute 를 읽어 이동에 반영하는 CMC. */
	UFACharacterMovementComponent* GetFAMovementComponent() const;

	//~ 서버 권한 — 진영 배정 시 GameMode 가 호출.
	/** State.Human / State.Zombie 를 갈아끼운다. 시야 필터·포획 대상 판정이 이 태그를 본다. */
	void SetTeamStateTag(EFATeam NewTeam);

	/** 감염 직후 무적(GDD 3.4). 연속 드래프트를 뽑는 동안 즉사하지 않게 한다. */
	void ApplyInvulnerability(float Duration);

	/** 근접 보이스. 감쇠·리스너·발화 아이콘은 전부 컴포넌트 안에서 처리된다. */
	UFAVoiceComponent* GetVoiceComponent() const { return VoiceComponent; }

	/**
	 * 주인 화면에서는 재생하지 않는다.
	 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayFASoundAtLocation(FName SoundId, bool bSkipLocalOwner = false);

protected:
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	virtual void OnRep_Controller() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * 점프·착지 소리의 창구.
	 */
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode) override;

	/** ASC 에 아바타 정보를 물리고, 서버라면 기본 어빌리티를 지급한다. */
	void InitializeAbilitySystem();

	/** 진영에 딸린 연출(좀비 그르렁 루프)을 현재 진영에 맞춘다. */
	UFUNCTION()
	void HandleTeamChanged(EFATeam NewTeam);

	/**
	 * 소유 PlayerState 의 진영 변경 알림을 받아둔다.
	 * PlayerState 와 Pawn 은 복제 도착 순서가 보장되지 않아 빙의 시점과 OnRep 양쪽에서 부른다.
	 */
	void BindTeamNotify();

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

	/** GA_Jump 를 통해 점프한다 — 점프 불가 룰(Z01/H01)이 여기서 걸린다. */
	void Input_Jump();
	void Input_StopJumping();

	void ToggleCrouch();
	void UpdateCrouchCamera();
	void TogglePerspective();
	void UpdatePerspectiveCamera();
	void UpdateCameraCollision(float DeltaTime);

	/** 좌클릭: 진영에 따른 포획(좀비) / 구출(인간) 어빌리티 발동 */
	void TriggerPrimary();

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class UCameraComponent* FollowCamera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float ThirdPersonArmLength;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float CameraInterpSpeed;

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

	//~ ---------------- GAS ----------------

	/**
	 * 기본 지급 어빌리티. 서버가 빙의 시점에 1회 지급한다.
	 * 점프처럼 '룰로 봉인될 수 있는 행동'은 전부 어빌리티여야 한다 — 그래야 태그 하나로 막힌다.
	 */
	// 기본룰 ?..
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|GAS")
	TArray<TSubclassOf<UFAGameplayAbility>> DefaultAbilities;

	/**
	 * 감염 직후 무적에 쓰는 GE. State.Invulnerable 을 부여하는 Has Duration 타입으로 만들고,
	 * Duration 은 Set By Caller(Data.Rule.Magnitude)로 두면 호출부가 시간을 정할 수 있다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|GAS")
	TSubclassOf<UGameplayEffect> InvulnerabilityEffect;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="FallingApart|Status",meta=(AllowPrivateAccess="true"))
	TObjectPtr<UFACharacterStatusComponent> CharacterStatusComponent;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FallingApart|Voice", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFAVoiceComponent> VoiceComponent;

	/**
	 * ASC 는 PlayerState 가 아니라 캐릭터에 둔다.
	 * 이 게임은 라운드 중 리스폰이 없고(감염되면 같은 폰이 좀비가 된다),
	 * 룰 효과의 소비자가 전부 캐릭터(CMC·카메라·PostProcess)라 아바타 옆에 두는 편이 참조가 짧다.
	 */
	//UFAAbilitySystemComponent 언리얼 형태의 포인터 
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FallingApart|GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFAAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UFAAttributeSet> AttributeSet;

	/** 어빌리티 중복 지급 방지 — PossessedBy 와 OnRep_PlayerState 가 둘 다 초기화를 부른다. */
	bool bAbilitiesGranted = false;

	//~ ---------------- 사운드 ----------------

	/**
	 * 좀비인 동안 계속 도는 그르렁 루프.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> GrowlLoopComponent;

	/** 진영 알림을 이미 물린 PlayerState. 같은 대상에 두 번 묶지 않기 위한 것. */
	TWeakObjectPtr<AFAPlayerState> TeamNotifySource;

	/**
	 * 이번 낙하에서 기록한 최대 하강 속도(cm/s, 양수).
	 */
	float PeakFallSpeed = 0.0f;

	/** 이 속도 이상으로 떨어져 닿으면 Jump_Land 대신 Land_Heavy. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Audio")
	float HeavyLandSpeed = 900.0f;

	/**
	 * 공중에 뜨는 순간 위로 이만큼 이상 솟고 있으면 '스스로 점프한 것'으로 본다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Audio")
	float JumpDetectSpeed = 100.0f;

	bool bIsFirstPerson;
	FTimerHandle CrouchTimerHandle;
	FTimerHandle PerspectiveTimerHandle;
};
