// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Game/FAGameTypes.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "Rule/FARuleTypes.h"
#include "FACharacter.generated.h"

class UFAAbilitySystemComponent;
class UFACharacterMovementComponent;
class UFAAttributeSet;
class UFAGameplayAbility;
class UGameplayEffect;
class UInputAction;
class UFAVoiceComponent;
class UAudioComponent;
class UMaterialInterface;
class AFAPlayerState;
class UFACharacterStatusComponent;
class UFACaptureMarkerWidget;
class UFARuleMarkerWidget;
class UWidgetComponent;

UCLASS()
class FALLINGAPART_API AFACharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AFACharacter(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
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

	//~ ---------------- 포획 감염 게이지 ----------------
	//~ 상태만 복제하고 그리기는 UFAHUDWidget 이 폴링으로 한다.

	/** 포획 진행 상태. HUD 게이지가 읽는다. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Capture")
	const FFACaptureState& GetCaptureState() const { return CaptureState; }

	/**
	 * 감염 진행도 0~1. 1 이면 감염이 확정되는 시점.
	 * 서버 시각을 기준으로 계산하므로 서버와 클라이언트가 같은 값을 본다.
	 */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Capture")
	float GetCaptureInfectionRatio() const;

	//~ 서버 전용 — UFAGA_Grab · UFAGA_RescueContest 만 호출한다.
	/** 포획 시작. 진행도 0 에서 시작하고 UpdateCaptureProgress 가 채워 나간다. */
	void BeginCaptureState(bool bInIsVictim);

	/**
	 * 감염 진행도(0~1)와 지금의 초당 진행 속도를 갱신한다.
	 * 속도 0 이 곧 '멈춤' 이다 — 구출 QTE 중에는 0 을 넣어 게이지를 세운다.
	 */
	void UpdateCaptureProgress(float Progress, float RatePerSecond);

	/** 구출 QTE 줄다리기 진행도(0~1). 포획 중이 아니면 무시된다. */
	void SetCaptureResistRatio(float Ratio);

	/** 포획 종료 — 감염 성공·취소 어느 쪽이든 반드시 불러야 게이지가 내려간다. */
	void EndCaptureState();

	//~ ---------------- 감염 저항 (WASD 연타) ----------------
	//~ 저항은 감염을 취소하지 못한다. 감염 속도를 늦춰 구조자가 올 시간을 버는 것이 전부다.

	/**
	 * 저항 강도를 DeltaSeconds 만큼 감쇠시킨 뒤 현재 값(0~1)을 돌려준다. 서버 전용.
	 *
	 * 감쇠가 있어야 '계속 빠르게 눌러야 유지되는' 것이 된다
	 */
	float UpdateResistLevel(float DeltaSeconds);

	/** 현재 저항 강도 0~1. 서버에서만 의미가 있다. */
	float GetResistLevel() const { return ResistLevel; }

	//~ ---------------- 구출 QTE (줄다리기) ----------------
	//~ 포획과 달리 참가자가 셋(좀비 · 인질 · 구조자)이라 상태를 따로 들고 있다.

	/** QTE 참가 상태. 구조자도 여기 들어오므로 이 값이 곧 "지금 QTE 중인가" 다. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Contest")
	const FFAContestState& GetContestState() const { return ContestState; }

	/** QTE 남은 시간(초). QTE 중이 아니면 0. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Contest")
	float GetContestTimeRemaining() const;

	//~ 서버 전용 — UFAGA_RescueContest 만 호출한다.
	/** QTE 참가 시작. Duration 초 뒤에 타임아웃 판정이 난다. */
	void BeginContestState(EFAContestRole InRole, AActor* InHostZombie, float Duration);

	/** 줄다리기 게이지 0~1 갱신 (1 이면 인간 승리 직전). */
	void SetContestGauge(float Gauge01);

	/** QTE 종료 — 승패 어느 쪽이든 반드시 불러야 프롬프트가 내려간다. */
	void EndContestState();

protected:
	/** GameState 기준 서버 월드 시각. GameState 가 아직 없으면 로컬 시각으로 폴백. */
	float GetServerWorldTime() const;

	virtual void BeginPlay() override;

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

	/** 진영에 딸린 연출(외형 · 그르렁 루프)을 현재 진영에 맞춘다. */
	UFUNCTION()
	void HandleTeamChanged(EFATeam NewTeam);

	/**
	 * 진영에 맞는 머티리얼을 메시 전 슬롯에 적용한다.
	 */
	void ApplyTeamMaterial(EFATeam NewTeam);

	/** 좀비인 동안만 도는 그르렁 루프를 켜고 끈다. */
	void UpdateGrowlLoop(EFATeam NewTeam);

	/**
	 * 소유 PlayerState 의 진영 변경 알림을 받아둔다.
	 * PlayerState 와 Pawn 은 복제 도착 순서가 보장되지 않아 빙의 시점과 OnRep 양쪽에서 부른다.
	 */
	void BindTeamNotify();

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

	/**
	 * 이동 입력이 끊긴 순간. 저항 판정의 '마지막 방향' 을 지운다.
	 * 이게 없으면 W 를 뗐다 다시 누르는 연타가 방향 변화로 안 잡혀서 한 키만 두들기는 사람은
	 * 저항이 전혀 안 오른다.
	 */
	void Input_MoveReleased();

	/**
	 * 잡혀 있는 동안의 WASD 를 저항 입력으로 바꾼다. 방향이 바뀔 때마다 1회.
	 * 한 방향으로 꾹 누르고 있는 것은 저항이 아니다 — 번갈아 빠르게 눌러야 유지된다.
	 */
	void HandleResistInput(const FVector2D& MovementVector);

	/** 이동 벡터를 WASD 네 방향(1~4)으로 뭉친다. 0 이면 입력 없음(데드존). */
	static int32 QuantizeResistDirection(const FVector2D& MovementVector);

	/** 저항 입력 1회. 잡힌 본인의 클라이언트만 보낸다. */
	UFUNCTION(Server, Unreliable)
	void Server_ReportResistInput();

	/** 서버가 저항 입력 1회를 반영한다. 매크로 연타를 막는 레이트 제한이 여기 있다. */
	void RegisterResistInput();

	/** GA_Jump 를 통해 점프한다 — 점프 불가 룰(Z01/H01)이 여기서 걸린다. */
	void Input_Jump();
	void Input_StopJumping();

	void ToggleCrouch();
	void UpdateCrouchCamera();
	void TogglePerspective();
	
	/** 플레이어 입력(TogglePerspective)과 Vision.ForceFPS 룰 양쪽이 이걸 호출한다. */
	void ForceTogglePerspective();
	
	void UpdatePerspectiveCamera();
	void UpdateCameraCollision(float DeltaTime);
	
	/** 룰 Attribute·태그를 읽어 카메라 FOV·PostProcess 를 갱신한다. 로컬 컨트롤 캐릭터에서만 의미가 있다. */
	void UpdateVisionEffects();

	/** 좌클릭: 진영에 따른 포획(좀비) / 구출(인간) 어빌리티 발동 */
	void TriggerPrimary();

	/**
	 * 지금 줄다리기 중인가. 스페이스바를 점프가 아니라 연타로 보낼지, 좌클릭을 막을지 판정.
	 */
	bool IsInRescueContest() const;

	/** 머리 위 감염 게이지 위젯 컴포넌트를 만든다. BeginPlay 에서 1회. */
	void SetupCaptureMarker();

	/**
	 * 이 프레임에 머리 위 게이지를 그려야 하는가.
	 *
	 * 잡힌 쪽인지 · 보는 사람이 본인인지 · 거리가 컬링 범위 안인지를 전부 여기서 판정한다.
	 * 스크린 스페이스 위젯 컴포넌트는 SetOwnerNoSee() 도 SetCullDistance() 도 먹지 않는다.
	 */
	bool ShouldShowCaptureMarker() const;

	/** 복제된 포획 · QTE 상태를 머리 위 위젯으로 흘려보낸다. 매 프레임. */
	void UpdateCaptureMarker();

	/** 머리 위 보유 룰 아이콘 위젯 컴포넌트를 만든다. BeginPlay 에서 1회. */
	void SetupRuleMarker();

	/**
	 * 이 프레임에 머리 위 룰 아이콘을 그려야 하는가.
	 *
	 * 남의 것만 · 룰이 1장이라도 있을 때 · 컬링 거리 안에서만
	 */
	bool ShouldShowRuleMarker() const;

	/** 복제된 보유 룰을 머리 위 위젯으로 흘려보낸다. 매 프레임. */
	void UpdateRuleMarker(float DeltaTime);

	/** 시야 가림 트레이스 1회. 결과는 bRuleMarkerOccluded 에 캐시된다. */
	void UpdateRuleMarkerOcclusion();

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

	//~ ---------------- 진영 외형 ----------------
	//~ 둘 다 비워두면 메시에 원래 물려 있는 머티리얼이 그대로 쓰인다

	/**
	 * 인간(비감염) 외형. MI_CharacterSurvived.
	 * 진영 미배정 상태에도 이걸 쓴다
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Appearance")
	TObjectPtr<UMaterialInterface> HumanMaterial;

	/** 좀비(감염) 외형. MI_CharacterInfected. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Appearance")
	TObjectPtr<UMaterialInterface> ZombieMaterial;

	//~ ---------------- 머리 위 감염 게이지 ----------------

	/**
	 * 잡힌 사람 머리 위에 뜨는 감염 게이지 WBP.
	 * 비워두면 3D 표시 없이 당사자의 HUD 게이지만 돈다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Capture")
	TSubclassOf<UFACaptureMarkerWidget> CaptureMarkerWidgetClass;

	/** 캡슐 원점 기준 위젯 위치. 캐릭터 머리보다 조금 위. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Capture")
	FVector CaptureMarkerOffset = FVector(0.0f, 0.0f, 110.0f);

	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Capture")
	FVector2D CaptureMarkerDrawSize = FVector2D(180.0f, 44.0f);

	/** 이 거리 밖에서는 그리지 않는다. 0 이면 컬링하지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Capture")
	float CaptureMarkerCullDistance = 3000.0f;

	/** 저항 입력 1회가 올리는 저항 강도(0~1 기준). */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Capture", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float ResistGainPerInput = 0.4f;

	/**
	 * 저항 강도가 초당 떨어지는 양.
	 * Gain 과 짝을 이뤄 '최대 저항을 유지하는 데 필요한 연타 속도' 를 정한다 — 0.2 / 1.0 이면 초당 5회다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Capture", meta = (ClampMin = "0.0"))
	float ResistDecayPerSecond = 1.0f;

	/**
	 * 서버가 받아들이는 저항 입력의 최소 간격(초).
	 * 매크로가 초당 수백 번을 보내도 이 이상은 먹지 않는다 — 사람이 낼 수 있는 속도보다 살짝 위로 둔다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Capture", meta = (ClampMin = "0.0"))
	float ResistInputMinInterval = 0.08f;

	//~ ---------------- 머리 위 보유 룰 아이콘  ----------------

	/**
	 * 다른 플레이어 머리 위에 뜨는 보유 룰 아이콘 WBP.
	 * 비워두면 3D 표시 없이 본인 HUD 4칸만 돈다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Rule")
	TSubclassOf<UFARuleMarkerWidget> RuleMarkerWidgetClass;

	/** 캡슐 원점 기준 위치. 감염 게이지와 겹치지 않게 그보다 위에 둔다. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Rule")
	FVector RuleMarkerOffset = FVector(0.0f, 0.0f, 145.0f);

	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Rule")
	FVector2D RuleMarkerDrawSize = FVector2D(160.0f, 40.0f);

	/** 이 거리 밖에서는 그리지 않는다. 0 이면 컬링하지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Rule")
	float RuleMarkerCullDistance = 4000.0f;

	/** 이 거리부터 아이콘 대신 계열 색 점 3개로 축약한다. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Rule")
	float RuleMarkerAbbreviateDistance = 1800.0f;

	/** 거리 감쇠가 시작되는 거리. 이 안쪽에서는 원래 크기 그대로. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Rule")
	float RuleMarkerFullScaleDistance = 800.0f;

	/** 컬링 거리에서의 최소 배율. 0.4 면 원래 크기의 40%. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Rule", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float RuleMarkerMinScale = 0.45f;

	/**
	 * 벽에 가려지면 숨길지.
	 *
	 * 켜두면 '보이는 상대의 룰만 읽을 수 있다'
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Rule")
	bool bHideRuleMarkerWhenOccluded = true;

	/** 가림 판정 주기(초). 매 프레임 트레이스하면 8인 기준으로 낭비가 크다. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Rule", meta = (ClampMin = "0.05"))
	float RuleMarkerOcclusionInterval = 0.2f;

private:
	/**
	 * 포획 진행 상태.
	 */
	UPROPERTY(Replicated)
	FFACaptureState CaptureState;

	/** 구출 QTE 참가 상태. 참가자 3명 전원이 각자 자기 몫을 들고 있다. */
	UPROPERTY(Replicated)
	FFAContestState ContestState;

	/**
	 * 저항 강도 0~1. 서버 전용이라 복제하지 않는다 —
	 * 화면에는 감염 게이지가 느려지는 것으로만 드러난다.
	 */
	float ResistLevel = 0.0f;

	/** 마지막으로 받아들인 저항 입력의 서버 시각(초). 레이트 제한용. 음수면 아직 없음. */
	float LastResistInputTime = -1.0f;

	/** 로컬 클라이언트가 마지막으로 보고한 이동 방향(1~4). 0 이면 입력 없음. */
	int32 LastResistDirection = 0;

	/** 머리 위 감염 게이지. 서버 전용 인스턴스에서는 만들지 않는다. */
	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> CaptureMarker;

	/** 머리 위 보유 룰 아이콘. 서버 전용 인스턴스에서는 만들지 않는다. */
	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> RuleMarker;

	/** 룰 아이콘에 넘길 표시 사본. 매 프레임 새로 만들지 않도록 보유 목록이 바뀔 때만 다시 채운다. */
	UPROPERTY(Transient)
	TArray<FFARuleDisplayInfo> CachedRuleDisplayInfos;

	/** CachedRuleDisplayInfos 를 만든 기준 목록. */
	TArray<FName> CachedRuleIds;

	/** 마지막 가림 판정 이후 흐른 시간(초). */
	float RuleMarkerOcclusionTimer = 0.0f;

	bool bRuleMarkerOccluded = false;

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
	
	/** Vision.ForceFPS 룰이 강제로 1인칭 전환시켰는가 — 룰이 빠질 때 되돌릴지 판단하는 데 쓴다. */
	bool bVisionForcedFirstPerson = false;
};
