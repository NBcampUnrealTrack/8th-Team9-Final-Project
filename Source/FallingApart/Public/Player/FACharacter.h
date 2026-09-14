// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Game/FAGameTypes.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "Rule/FARuleTypes.h"
#include "FACharacter.generated.h"

class UFAOverHeadWidget;
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
class UFAInteractionComponent;
class AFACabinet;
class AFALadder;
class UAnimMontage;
class UFADetectionUnitComponent;
class UFATrailComponent;

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

	/** 상호작용 탐지·프롬프트·서버 요청 창구. HUD 가 프롬프트를 여기서 읽는다. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Interact")
	UFAInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }

	//~ ---------------- 사다리 등반 ----------------

	/** 지금 타고 있는 사다리. 안 타고 있으면 nullptr. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Interact")
	AFALadder* GetClimbingLadder() const { return ClimbingLadder; }

	UFUNCTION(BlueprintPure, Category = "FallingApart|Interact")
	bool IsClimbing() const { return ClimbingLadder != nullptr; }

	/** 서버 전용. 사다리에 붙는다 — AFALadder::Interact 만 호출한다. */
	void ServerStartClimbing(AFALadder* Ladder);

	/**
	 * 서버 전용. 사다리에서 내린다.
	 * @param bLaunchOverLedge 꼭대기에서 난간 위로 올라설 때 true. 그 자리에서 뛰어내리면 false.
	 */
	void ServerStopClimbing(bool bLaunchOverLedge);

	/**
	 * 내려달라는 클라이언트 요청. 점프 키와 꼭대기 자동 하차 시.
	 */
	UFUNCTION(Server, Reliable)
	void Server_RequestStopClimbing(bool bLaunchOverLedge);

	//~ ---------------- 캐비넷 숨기 ----------------

	/** 지금 숨어 있는 캐비넷. 안 숨었으면 nullptr. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Interact")
	AFACabinet* GetHidingCabinet() const { return HidingCabinet; }

	UFUNCTION(BlueprintPure, Category = "FallingApart|Interact")
	bool IsHiding() const { return HidingCabinet != nullptr; }

	/** 서버 전용. 캐비넷 안에 들어간다 — AFACabinet::Interact 만 호출한다. */
	void ServerStartHiding(AFACabinet* Cabinet);

	/**
	 * 서버 전용. 캐비넷에서 나온다. 스스로 나오는 것과 좀비가 열어 배출되는 것이 같은 길이다.
	 * 점유 해제와 위치 복구가 전부 여기서 일어난다.
	 */
	void ServerStopHiding();

	/** 나가겠다는 클라이언트 요청. 점프 키. */
	UFUNCTION(Server, Reliable)
	void Server_RequestStopHiding();

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

	virtual void UnPossessed() override;

	/** 빙의 상태가 바뀔 때마다 상호작용 탐지 틱을 다시 판단하게 한다. */
	void RefreshInteractionTick();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * 점프·착지 소리의 창구.
	 */
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode) override;

	/** ASC 에 아바타 정보를 물리고, 서버라면 기본 어빌리티를 지급한다. */
	void InitializeAbilitySystem();

	/** 진영에 딸린 연출(외형 · 그르렁 루프 · 감염 전환)을 현재 진영에 맞춘다. */
	UFUNCTION()
	void HandleTeamChanged(EFATeam NewTeam);

	/** 감염 전환 연출을 시작한다. 진영이 Human -> Zombie 로 바뀐 순간에만 불린다. */
	void StartInfectionSequence();

	/**
	 * 연출의 화면 효과 값을 지금 시각 기준으로 다시 계산한다. 다 끝났으면 스스로 꺼진다.
	 */
	void UpdateInfectionSequence();

	/**
	 * 진영에 맞는 머티리얼을 메시 전 슬롯에 적용한다.
	 */
	void ApplyTeamMaterial(EFATeam NewTeam);

	/** 좀비인 동안만 도는 그르렁 루프를 켜고 끈다. */
	void UpdateGrowlLoop(EFATeam NewTeam);

public:
	/** 감염 전환 연출이 도는 중인가. 화면이 아직 어두운 동안은 이동 입력을 받지 않는다. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Infection")
	bool IsPlayingInfectionSequence() const { return bInfectionSequenceActive; }

protected:

	/** 물에 잠겨 있는 동안만 도는 첨벙 루프를 켜고 끈다. */
	void UpdateWadeLoop(bool bInWater);

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

	//~ ---------------- 상호작용 ----------------

	void Input_InteractPressed();
	void Input_InteractReleased();

	//~ ---------------- 등반 ----------------

	UFUNCTION()
	void OnRep_ClimbingLadder();

	UFUNCTION()
	void OnRep_HidingCabinet();

	/**
	 * 숨은 상태를 실제 표시·충돌·이동으로 옮긴다.
	 */
	void ApplyHideState();

	/**
	 * 숨는 동안 카메라를 캐비넷 위로 올린다. 자기 화면에서만 도는 처리
	 */
	void ApplyHideCamera(bool bHiding);

	/**
	 * 등반 상태를 실제 이동 설정으로 옮긴다
	 * 붙는 순간의 위치·회전 스냅도 여기서
	 */
	void ApplyClimbState();

	/** 등반 중 중심선 유지와 끝단 자동 하차. 매 프레임. */
	void UpdateClimbing(float DeltaTime);

	/** 등반 입력 — Move() 가 사다리 위에서는 여기로 넘긴다. */
	void ClimbMove(const FVector2D& MovementVector);

	/**
	 * 사다리에서 오르내림과 뛰어내리기를 막아야 하는가.
	 *
	 * 포획·줄다리기가 걸린 동안은 좀비와 인질이 그 자리에 함께 묶여야 한다
	 */
	bool IsLadderMovementLocked() const;

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

	/**
	 * bShouldBeActive 에 맞춰 MID 를 FollowCamera 의 WeightedBlendables 배열에 넣거나 뺀다.
	 * 각 비전 이펙트(Fog·NarrowFOV)가 자기 자신의 세기(0)일 때는 완전히 배열에서 빠져야,
	 * 다른 이펙트가 PostProcessBlendWeight 를 켰을 때 얘도 같이 묻어 나오는 일이 없다.
	 */
	void SetVisionBlendableActive(UMaterialInstanceDynamic* MID, bool bShouldBeActive) const;

	/**
	 * FullDistance 보다 멀리 있는 상대를 로컬 화면에서만 숨긴다(렌더링 컬링). 서버·남의 화면엔 영향 없음.
	 * @param bHideAllies [H24 고독] true면 거리와 무관하게 다른 인간(State.Human)을 전부 숨긴다.
	 */
	void UpdateOpponentDistanceCulling(float FullDistance, bool bHideAllies);

	/**
	 * [Z25 심박 왜곡] 가장 가까운 인간까지의 거리(제곱)를 ClosestHumanDistanceSq 에 캐싱한다.
	 * 너무 잦은 전체 순회를 피하려고 ShakeCheckInterval 주기로만 갱신 — 흔들림 세기는 부드럽게
	 * 이어지므로 캐시가 살짝 늦어도 체감상 문제없다.
	 */
	void UpdateClosestHumanDistance();

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

	/** 머리 위 닉네임 위젯을 만들고 이 캐릭터를 표시 대상으로 주입한다. */
	void SetupNicknameMarker();

	/** PlayerState 도착 또는 빙의 변경 후 닉네임 위젯의 표시 대상을 다시 갱신한다. */
	void RefreshNicknameMarker();

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

	/** State.Exposed 태그 변경 시 X-Ray 렌더링 스위치 조작 */
	void OnExposedTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
	void OnMutualSightTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	/**
	 * 캐릭터의 윤곽선을 켤지 말지 최종 판단, 렌더링을 갱신.
	 * [H23 투명한 적] 로컬 뷰어가 NoZombie 룰을 갖고 있으면 좀비·인간 구분 없이 this 의 본체는 숨기고
	 * 아웃라인만 남긴다 — 좀비한테만 적용하면 실루엣 자체가 정체를 드러내므로 전원 대상으로 바꿨다.
	 */
	void RefreshCustomDepth();

	bool ShouldShowOverheadWidget() const;

	void UpdateGroupRules();

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class UCameraComponent* FollowCamera;

	/** 전신의 최종 포즈를 받는 표시용 팔. 로컬 플레이어의 일인칭에서만 보인다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera|First Person")
	TObjectPtr<USkeletalMeshComponent> FirstPersonArms;

	/** 카툰 캐릭터에서 측정한 서기 눈높이 보정. 머리 애니메이션의 흔들림은 전달하지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|First Person", meta = (Units = "cm"))
	float FirstPersonEyeHeightOffset = -20.0f;

	/** 실제 앉기 최종 포즈에서 측정한 눈높이. 정지와 이동 값을 속도에 따라 보간한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|First Person", meta = (Units = "cm"))
	float FirstPersonCrouchedIdleEyeHeightOffset = -11.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|First Person", meta = (Units = "cm"))
	float FirstPersonCrouchedMovingEyeHeightOffset = 8.7f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|First Person", meta = (ClampMin = "1.0", Units = "cm/s"))
	float FirstPersonCrouchedEyeReferenceSpeed = 120.0f;

private:
	/** 시점 변경을 로컬 카메라와 이동 방향 요청에 함께 반영한다. */
	void UpdateFirstPersonViewMode();
	/** 팔 표시·틱·전신 본 갱신 정책을 같은 상태로 맞춘다. */
	void SetFirstPersonArmsVisible(bool bVisible);
	void UpdateFirstPersonPoseSource(bool bShowArms);
	float GetFirstPersonEyeHeight() const;
	void UpdateFirstPersonCameraPresentation(bool bFirstPersonView);

	// 일인칭을 벗어날 때 복구할 기존 카메라 설정.
	bool bFirstPersonCameraPresentationActive = false;
	bool bSavedCameraTranslationLag = false;
	bool bSavedMotionBlurOverride = false;
	float SavedMotionBlurAmount = 0.0f;

	// 팔을 숨기면 원래 정책으로 되돌려 전신의 불필요한 본 갱신을 막는다.
	bool bFirstPersonSourceActive = false;
	uint8 SavedBodyVisibilityTickOption = 0;
	bool bSavedBodyUpdateRateOptimizations = false;

public:

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	class UMaterialInterface* VisionFogMaterial = nullptr;

	UPROPERTY(Transient)
	class UMaterialInstanceDynamic* VisionFogMID = nullptr;

	// 평상시(FogAmount=0)엔 맵보다 훨씬 큰 값으로 둬서 사실상 안개·컬링이 발동하지 않게 한다.
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Vision Fog")
	float VisionFogStartDistanceMax = 20000.0f;   // FogAmount = 0 일 때 시작 거리

	UPROPERTY(EditDefaultsOnly, Category = "Camera|Vision Fog")
	float VisionFogStartDistanceMin = 300.0f;     // FogAmount = 1 일 때 시작 거리

	UPROPERTY(EditDefaultsOnly, Category = "Camera|Vision Fog")
	float VisionFogFullDistanceMax = 25000.0f;    // FogAmount = 0 일 때 완전히 안 보이는 거리

	UPROPERTY(EditDefaultsOnly, Category = "Camera|Vision Fog")
	float VisionFogFullDistanceMin = 1200.0f;     // FogAmount = 1 일 때 완전히 안 보이는 거리

	/** 상대 컬링 검사 주기(초). 매 프레임 전체 캐릭터를 순회하지 않도록 주기적으로만 수행한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Vision Fog")
	float OpponentCullCheckInterval = 0.2f;

	float TimeSinceLastOpponentCullCheck = 0.0f;

	//~ ---------------- 암전 명멸 연출 (Vision.Blackout) ----------------

	/** 암전 명멸 주기(초). 이 주기마다 한 번씩 화면이 완전히 어두워진다. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Vision Blackout")
	float BlackoutPeriod = 5.0f;

	/** 주기 안에서 완전 암전 상태가 유지되는 시간(초). BlackoutPeriod 보다 작아야 한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Vision Blackout")
	float BlackoutFlashDuration = 1.0f;

	//~ ---------------- 시야각 축소 연출 (Vision.NarrowFOV) ----------------
	//~ 실제 카메라 FOV는 더 이상 건드리지 않는다 — 화면 테두리를 검게 좁히는 별도 PP 머티리얼로 대체.
	//~ Vision.Fog(VisionFogMID)와는 완전히 분리된 블렌더블 슬롯 · 파라미터를 쓴다.

	UPROPERTY(EditDefaultsOnly, Category = "Camera|Vision Narrow")
	class UMaterialInterface* VisionNarrowFOVMaterial = nullptr;

	UPROPERTY(Transient)
	class UMaterialInstanceDynamic* VisionNarrowFOVMID = nullptr;

	/**
	 * FOV 축소 비율(NarrowAmount)에 곱하는 배율. 1.0보다 크면 실제 FOV가 덜 줄어도 화면 마스크는 더 세게 닫힌다.
	 * GE가 FOV를 살짝만 줄이는데도 "거의 다 가려지게" 하고 싶으면 이 값을 키운다. (예: 2.0 ~ 3.0)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Vision Narrow", meta = (ClampMin = "1.0"))
	float NarrowAmountIntensity = 2.0f;

	//~ ---------------- 심박 왜곡 연출 (Vision.Shake, Z25) ----------------
	//~ 카메라 셰이크 애셋 없이 FollowCamera 의 RelativeRotation 을 매 프레임 직접 흔든다 —
	//~ 이 룰이 꺼져 있으면 항상 ZeroRotator 로 되돌리므로 다른 카메라 로직과 안 겹친다.

	/** 이 거리 이하로 가까워지면 흔들림이 최대치가 된다(uu). */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Vision Shake")
	float ShakeMinDistance = 150.0f;

	/** 이 거리보다 멀면 흔들림이 없다(uu). */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Vision Shake")
	float ShakeMaxDistance = 800.0f;

	/** 흔들림 최대 각도(도). 최대치일 때 카메라가 좌우/상하로 이 각도만큼 떨린다. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Vision Shake")
	float ShakeMaxAngle = 1.5f;

	/** 흔들림 파형 속도. 값이 클수록 더 빠르게 떨린다. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Vision Shake")
	float ShakeFrequency = 18.0f;

	/** 가장 가까운 인간 탐색 주기(초). 매 프레임 전체 캐릭터를 순회하지 않도록 주기적으로만 수행한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Vision Shake")
	float ShakeCheckInterval = 0.2f;

	float TimeSinceLastShakeCheck = 0.0f;

	/** UpdateClosestHumanDistance() 가 채우는 캐시. 사람이 하나도 없으면 아주 큰 값(=흔들림 0)으로 유지. */
	float ClosestHumanDistanceSq = TNumericLimits<float>::Max();

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

	/**
	 * 상호작용(문 · 사다리).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* InteractAction;

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

	//~ ---------------- 감염 전환 연출 ----------------
	//~ 인간 -> 좀비 로 진영이 바뀐 순간 한 번 돈다. 암전 → 빨강에서 정상으로 → 기상.

	/**
	 * 쓰러진 자세에서 일어나는 몽타주. 비워두면 화면 효과만 돌고 캐릭터는 그대로 서 있는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Infection")
	TObjectPtr<UAnimMontage> InfectionGetUpMontage;

	/** 화면이 완전히 검은 채로 머무는 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Infection", meta = (ClampMin = "0", ForceUnits = "s"))
	float InfectionBlackoutHoldSeconds = 1.0f;

	/** 암전이 풀리며 밝아지는 데 걸리는 시간(초). 이 시간이 끝나야 이동 입력이 돌아온다. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Infection", meta = (ClampMin = "0", ForceUnits = "s"))
	float InfectionDarkenReleaseSeconds = 0.6f;

	/**
	 * 빨간 화면이 정상 색으로 돌아오는 데 걸리는 시간(초).
	 * 암전 해제와 같은 시점에 시작해서 더 길게 끈다 — 밝아지고 나서도 한동안 붉게 남는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Infection", meta = (ClampMin = "0", ForceUnits = "s"))
	float InfectionRedFadeSeconds = 2.5f;

	/** 빨강의 최대 강도(0~1). 1 이면 초록·파랑이 완전히 죽어 순수 빨강만 남는다. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Infection", meta = (ClampMin = "0", ClampMax = "1"))
	float InfectionRedTintStrength = 0.85f;

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
	float ResistInputMinInterval = 0.2f;

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

	/**
	 *	닉네임 컴포넌트
	 *	Rule과 마찬가지로 플레이어가 보일때만 가시성 활성.
	 *	여건이 되면 위의 Rulemarker와 통합하는게 좋을듯?
	*/
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Rule")
	TSubclassOf<UFAOverHeadWidget> OverHeadWidgetClass;

	void SetHeartbeatRevealActive(bool bActive);

	UFADetectionUnitComponent* GetDetectionUnitComponent() const
	{
		return DetectionUnitComponent;
	}

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

	/** 머리 닉네임 텍스트. 서버 전용 인스턴스에서는 만들지 않는다. */
	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> NicknameMarker;

	/** 머리 위 닉네임 텍스트 거림 검사 시간 확인용. */
	float NicknameMarkerOcclusionTimer = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Rule")
	FVector NicknameMarkerOffset = FVector(0.0f, 0.0f, 80.0f);

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FallingApart|Interact", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFAInteractionComponent> InteractionComponent;

	//~ ---------------- 등반 상태 ----------------

	/**
	 * 타고 있는 사다리.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_ClimbingLadder)
	TObjectPtr<AFALadder> ClimbingLadder;

	/**
	 * 들어가 있는 캐비넷.
	 *
	 * 사다리와 같은 방식
	 */
	UPROPERTY(ReplicatedUsing = OnRep_HidingCabinet)
	TObjectPtr<AFACabinet> HidingCabinet;

	/**
	 * 마지막으로 적용한 숨김 상태.
	 */
	bool bHideStateApplied = false;

	/** 카메라 붐을 캐비넷에 매달아 둔 상태인가. 되돌릴 때 한 번만 풀기 위한 값이다. */
	bool bHideCameraApplied = false;

	/** 붐을 캐비넷으로 옮기기 직전의 상대 트랜스폼. 나올 때 그대로 되돌린다. */
	FTransform SavedBoomRelativeTransform;

	/** 등반 진입 전 CMC 값. 내릴 때 그대로 되돌린다. */
	float SavedMaxFlySpeed = 0.0f;
	float SavedBrakingDecelerationFlying = 0.0f;
	bool bSavedOrientRotationToMovement = true;
	bool bHasSavedClimbMovementSettings = false;

	/** 사다리에 붙은 서버 시각. 붙자마자 바닥 판정으로 튕겨나가는 것을 막는 유예에 쓴다. */
	float ClimbStartTime = 0.0f;

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

	//~ 감염 전환 연출의 진행 상태. 전부 로컬 표현이라 복제하지 않는다.

	bool bInfectionSequenceActive = false;

	/** 연출이 시작된 월드 시각. 연출이 안 도는 동안에는 의미 없는 값이다. */
	float InfectionSequenceStartTime = 0.0f;

	/** 이번 프레임의 연출 화면 값. Tick 이 계산하고 UpdateVisionEffects 가 읽어 간다. */
	float InfectionDarkenAmount = 0.0f;
	float InfectionRedAmount = 0.0f;

	/**
	 * 마지막으로 본 진영. Human -> Zombie 로 '바뀌는 순간' 만 잡아내려고 들고 있는다.
	 * 첫 호출은 기준만 잡는다
	 */
	EFATeam LastKnownTeam = EFATeam::None;
	bool bHasTeamBaseline = false;

	/**
	 * 물에 잠겨 있는 동안 도는 첨벙 루프. MOVE_Swimming 진입/이탈로만 켜고 끈다.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> WadeLoopComponent;

	/** 사다리 발소리 누산기(cm). 등반축으로 실제 움직인 거리다. */
	float ClimbStepAccumulator = 0.0f;

	/** 사다리를 이만큼(cm) 오르내릴 때마다 Ladder_Step 을 낸다. 0 이면 안 낸다. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Audio")
	float ClimbStepDistance = 60.0f;

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

	/**
	 * 카메라 원래 FOV. BeginPlay 에서 한 번만 캐싱한다.
	 * Vision.NarrowFOV 가 더 이상 실제 FOV를 바꾸지 않으므로, "얼마나 좁아졌는가" 비율을 계산하는 기준값으로만 쓴다.
	 */
	float DefaultCameraFOV = 90.0f;

	/** Vision.ForceFPS 룰이 강제로 1인칭 전환시켰는가 — 룰이 빠질 때 되돌릴지 판단하는 데 쓴다. */
	bool bVisionForcedFirstPerson = false;

	bool bHeartbeatRevealActive = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FallingApart|Trail", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UFATrailComponent> TrailComponent;
	UPROPERTY(VisibleAnywhere, Category = "FallingApart|Detection")
	TObjectPtr<UFADetectionUnitComponent> DetectionUnitComponent;

	// 거리 기반 룰(무리 강박, 대인 기피) 검사용 타이머
	FTimerHandle GroupRuleTimerHandle;
};
