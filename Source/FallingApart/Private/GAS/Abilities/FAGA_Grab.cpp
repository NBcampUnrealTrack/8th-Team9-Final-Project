// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Abilities/FAGA_Grab.h"
#include "GAS/FAGameplayTags.h"
#include "AttributeSet/FAAttributeSet.h"
#include "Game/FAGameMode.h"
#include "Player/FACharacter.h"
#include "Player/FAPlayerState.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"

namespace FAGrabAudio
{
	// 표의 RowName 과 같아야 한다 (DT_Sound_Character).
	// 이 어빌리티는 ServerOnly 라 소리는 반드시 AFACharacter 의 멀티캐스트를 거쳐야 한다.
	static const FName Attempt(TEXT("Grab_Attempt"));
	static const FName Success(TEXT("Grab_Success"));
	static const FName Miss(TEXT("Grab_Miss"));
	static const FName InfectComplete(TEXT("Infect_Complete"));
}

UFAGA_Grab::UFAGA_Grab()
{
	const FAGameplayTags& Tags = FAGameplayTags::Get();

	// 공격자 본인은 입력 즉시 몽타주를 예측 재생하고, 실제 포획 판정은 서버만 수행한다.
	// 서버에서 재생한 몽타주는 다른 클라이언트의 시뮬레이티드 프록시에 복제된다.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 어빌리티 태그 등록
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(Tags.Ability_Grab);
	SetAssetTags(AssetTags);

	// 포획을 차단하는 모든 상태 태그 집약
	// 경직 · 줄다리기(QTE) 중 · 이미 대상을 잡고 있는 상태 · 자신이 잡힌 상태
	ActivationBlockedTags.AddTag(Tags.State_Stunned);
	ActivationBlockedTags.AddTag(Tags.State_Contesting);
	ActivationBlockedTags.AddTag(Tags.State_Grabbing);
	ActivationBlockedTags.AddTag(Tags.State_Grabbed);

	InfectProgress = 0.0f;
}

void UFAGA_Grab::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AFACharacter* ZombieChar = Cast<AFACharacter>(ActorInfo->AvatarActor.Get());
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!ZombieChar || !SourceASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bIsAuthority = HasAuthority(&ActivationInfo);

	// InstancedPerActor 라 인스턴스가 포획 사이에 재사용된다 — 지난 포획의 진행도가 남아 있으면 안 된다.
	InfectProgress = 0.0f;
	bInfectPaused = false;

	bGrabImpactResolved = false;
	GrabbedVictim.Reset();
	GrabberEffectHandle.Invalidate();
	VictimEffectHandle.Invalidate();

	// 멀티캐스트는 서버에서 한 번만 호출한다. 예측 클라이언트까지 호출하면 소리가 중복된다.
	if (bIsAuthority)
	{
		// 휘두르는 소리는 맞았든 빗나갔든 먼저 난다 — 주변 인간이 반응할 수 있어야 한다.
		ZombieChar->Multicast_PlayFASoundAtLocation(FAGrabAudio::Attempt);
	}

	if (!GrabAttemptMontage)
	{
		// BP 에 몽타주를 아직 지정하지 않은 동안에도 기존 즉시 판정 동작은 유지한다.
		if (bIsAuthority)
		{
			ResolveGrabAttempt();
		}
		return;
	}

	UAbilityTask_PlayMontageAndWait* MontageTask =
		UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			TEXT("GrabAttemptMontage"),
			GrabAttemptMontage,
			1.0f,
			NAME_None,
			/*bStopWhenAbilityEnds=*/true);

	if (!MontageTask)
	{
		if (bIsAuthority)
		{
			ResolveGrabAttempt();
		}
		return;
	}

	MontageTask->OnCancelled.AddDynamic(this, &UFAGA_Grab::HandleGrabAttemptInterrupted);
	MontageTask->OnInterrupted.AddDynamic(this, &UFAGA_Grab::HandleGrabAttemptInterrupted);
	MontageTask->ReadyForActivation();

	// 소유 클라이언트는 여기까지 실행하여 몽타주만 예측 재생한다.
	// 실제 대상 탐색과 포획 판정 타이머는 서버에만 존재해야 한다.
	if (!bIsAuthority)
	{
		return;
	}

	// 서버의 월드 타이머를 판정 기준으로 삼는다. Dedicated Server 의 Mesh Tick/Notify 설정과 무관하다.
	const float ImpactDelay = FMath::Clamp(GrabImpactDelay, 0.0f, GrabAttemptMontage->GetPlayLength());
	if (ImpactDelay <= 0.0f)
	{
		ResolveGrabAttempt();
		return;
	}

	ZombieChar->GetWorldTimerManager().SetTimer(
		GrabImpactTimerHandle,
		this,
		&UFAGA_Grab::ResolveGrabAttempt,
		ImpactDelay,
		false);
}

void UFAGA_Grab::ResolveGrabAttempt()
{
	// 예측 클라이언트에서 이 함수가 호출되더라도 게임 판정은 절대 실행하지 않는다.
	if (!HasAuthority(&CurrentActivationInfo))
	{
		return;
	}

	if (bGrabImpactResolved || !IsActive())
	{
		return;
	}
	bGrabImpactResolved = true;

	AFACharacter* ZombieChar = Cast<AFACharacter>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!ZombieChar || !SourceASC)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	const FAGameplayTags& Tags = FAGameplayTags::Get();

	float GrabRange = SourceASC->GetNumericAttribute(UFAAttributeSet::GetGrabRangeAttribute());
	if (GrabRange <= 0.0f) GrabRange = 200.0f;

	float SphereRadius = 50.0f;
	AFACharacter* TargetHuman = FindTargetInFront(ZombieChar, GrabRange, SphereRadius, Tags.State_Human);

	// 대상이 없거나 이미 다른 좀비에게 잡힌 상태인 경우
	if (!TargetHuman || TargetHuman->GetAbilitySystemComponent()->HasMatchingGameplayTag(Tags.State_Grabbed))
	{
		ZombieChar->Multicast_PlayFASoundAtLocation(FAGrabAudio::Miss);

		if (SourceASC->HasMatchingGameplayTag(Tags.Rule_Bite_FailStun) && FailStunEffectClass)
		{
			FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(FailStunEffectClass);

			SpecHandle.Data->SetSetByCallerMagnitude(Tags.Data_Rule_Magnitude, 5.f);
			SourceASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}

		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// 포획 성공 처리
	GrabbedVictim = TargetHuman;
	UAbilitySystemComponent* TargetASC = TargetHuman->GetAbilitySystemComponent();
	if (!TargetASC)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// 성공음은 실제로 붙잡는 처리 안에서 낸다 — 밖에서 내면 아래가 통째로 건너뛰었을 때
	// 아무 일도 안 일어났는데 소리만 나간다.
	ZombieChar->Multicast_PlayFASoundAtLocation(FAGrabAudio::Success);

	// Effect 적용
	if (GrabberGrabbingEffectClass)
	{
		FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(GrabberGrabbingEffectClass);
		GrabberEffectHandle = SourceASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}

	if (VictimGrabbedEffectClass)
	{
		FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(VictimGrabbedEffectClass);
		VictimEffectHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	}

	float InfectDuration = SourceASC->GetNumericAttribute(UFAAttributeSet::GetInfectDurationAttribute());
	// Attribute 가 아직 초기화 전이면 쓰는 폴백. UFAAttributeSet 의 기본값과 같이 움직여야 한다.
	if (InfectDuration <= 0.0f) InfectDuration = 5.0f;

	BaseInfectDuration = InfectDuration;
	CurrentInfectRate = 1.0f / BaseInfectDuration;

	ZombieChar->GetWorld()->GetTimerManager().SetTimer(
		GrabTickHandle,
		this,
		&UFAGA_Grab::TickGrab,
		GrabTickInterval,
		true
	);

	// HUD 감염 게이지 상태는 서버 판정 결과를 캐릭터 복제로 각 화면에 전달한다.
	TargetHuman->BeginCaptureState(/*bInIsVictim=*/true);
	ZombieChar->BeginCaptureState(/*bInIsVictim=*/false);
	PushCaptureProgress();
}

void UFAGA_Grab::HandleGrabAttemptInterrupted()
{
	if (!bGrabImpactResolved && IsActive())
	{
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
	}
}

UFAGA_Grab* UFAGA_Grab::FindActiveInstance(UAbilitySystemComponent* ASC)
{
	if (!ASC)
	{
		return nullptr;
	}

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.IsActive())
		{
			continue;
		}

		for (UGameplayAbility* Instance : Spec.GetAbilityInstances())
		{
			if (UFAGA_Grab* GrabAbility = Cast<UFAGA_Grab>(Instance))
			{
				return GrabAbility;
			}
		}
	}

	return nullptr;
}

void UFAGA_Grab::PauseInfectTimer()
{
	// 틱은 계속 돌려둔다 — 거리 이탈 검사는 QTE 중에도 살아 있어야 한다.
	bInfectPaused = true;

	// 게이지도 같이 멈춘다 — 안 멈추면 QTE 로 겨루는 동안 감염이 혼자 채워지는 것처럼 보인다.
	// 속도 0 을 내보내면 클라이언트가 외삽을 멈춰 진행도가 그 자리에 선다.
	PushCaptureProgress();
}

void UFAGA_Grab::ResumeInfectTimer()
{
	// 처음부터 다시 도는 게 아니라 쌓아둔 진행도에서 이어진다.
	bInfectPaused = false;

	PushCaptureProgress();
}

void UFAGA_Grab::PushCaptureProgress()
{
	const float Rate = bInfectPaused ? 0.0f : CurrentInfectRate;

	if (AFACharacter* ZombieChar = Cast<AFACharacter>(GetAvatarActorFromActorInfo()))
	{
		ZombieChar->UpdateCaptureProgress(InfectProgress, Rate);
	}
	if (GrabbedVictim.IsValid())
	{
		GrabbedVictim->UpdateCaptureProgress(InfectProgress, Rate);
	}
}

void UFAGA_Grab::TickGrab()
{
	AFACharacter* ZombieChar = Cast<AFACharacter>(GetAvatarActorFromActorInfo());
	if (!ZombieChar || !GrabbedVictim.IsValid())
	{
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
		return;
	}

	// 거리 판정이 먼저다 — 이미 놓친 포획으로 감염이 한 틱 더 차면 안 된다.
	float Distance = FVector::Dist(ZombieChar->GetActorLocation(), GrabbedVictim->GetActorLocation());
	if (Distance > 300.0f)
	{
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
		return;
	}

	if (bInfectPaused)
	{
		return;
	}

	// 저항이 셀수록 감염이 느려진다: 저항 0 이면 BaseInfectDuration, 최대면 × MaxResistTimeMultiplier.
	// 저항 강도는 여기서 감쇠시킨다 — 손을 놓으면 알아서 0 으로 돌아온다.
	const float ResistLevel = GrabbedVictim->UpdateResistLevel(GrabTickInterval);
	const float CurrentDuration = FMath::Max(0.1f, BaseInfectDuration * FMath::Lerp(1.0f, MaxResistTimeMultiplier, ResistLevel));

	CurrentInfectRate = 1.0f / CurrentDuration;
	InfectProgress += GrabTickInterval * CurrentInfectRate;

	PushCaptureProgress();

	if (InfectProgress >= 1.0f)
	{
		CompleteInfection();
	}
}

void UFAGA_Grab::CompleteInfection()
{
	if (GrabbedVictim.IsValid())
	{
		// 감염이 성립한 지점의 소리 — 주변 사람이 "저기서 한 명 넘어갔다"를 듣는 용도다.
		// 당사자에게는 UFAHUDWidget 이 진영 복제를 보고 Infect_Blackout + Sting_Infected 라는
		// 전용 2D 소리를 내주므로, 본인 화면에서는 이걸 빼야 같은 순간에 세 겹으로 겹치지 않는다.
		GrabbedVictim->Multicast_PlayFASoundAtLocation(FAGrabAudio::InfectComplete, /*bSkipLocalOwner=*/true);

		// 진영 전환 · 감염 점수 +2 · 인간 룰 제거 · 연속 드래프트 · 생존자 수 갱신 ·
		AFACharacter* ZombieChar = Cast<AFACharacter>(GetAvatarActorFromActorInfo());

		AFAPlayerState* VictimState = GrabbedVictim->GetPlayerState<AFAPlayerState>();
		AFAPlayerState* InfectorState = ZombieChar ? ZombieChar->GetPlayerState<AFAPlayerState>() : nullptr;

		if (const UWorld* World = GetWorld())
		{
			if (AFAGameMode* GameMode = World->GetAuthGameMode<AFAGameMode>())
			{
				GameMode->HandleInfection(VictimState, InfectorState);
			}
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFAGA_Grab::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GrabImpactTimerHandle);
		World->GetTimerManager().ClearTimer(GrabTickHandle);
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (SourceASC)
	{
		if (GrabberEffectHandle.IsValid())
		{
			SourceASC->RemoveActiveGameplayEffect(GrabberEffectHandle);
		}
		if (GrabbedVictim.IsValid() && VictimEffectHandle.IsValid())
		{
			if (UAbilitySystemComponent* TargetASC = GrabbedVictim->GetAbilitySystemComponent())
			{
				TargetASC->RemoveActiveGameplayEffect(VictimEffectHandle);
			}
		}
	}

	// 게이지는 감염 성공·거리 이탈·QTE 패배 어느 쪽으로 끝나도 반드시 내려가야 한다.
	if (AFACharacter* ZombieChar = Cast<AFACharacter>(GetAvatarActorFromActorInfo()))
	{
		ZombieChar->EndCaptureState();
	}
	if (GrabbedVictim.IsValid())
	{
		GrabbedVictim->EndCaptureState();
	}

	GrabbedVictim.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

AFACharacter* UFAGA_Grab::FindTargetInFront(AFACharacter* Avatar, float Range, float Radius, FGameplayTag RequiredTag)
{
	if (!Avatar) return nullptr;

	FVector StartLoc = Avatar->GetActorLocation();
	FVector EndLoc = StartLoc + (Avatar->GetActorForwardVector() * Range);

	TArray<FHitResult> HitResults;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Avatar);

	bool bHit = Avatar->GetWorld()->SweepMultiByChannel(
		HitResults, StartLoc, EndLoc, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeSphere(Radius), QueryParams
	);

	if (bHit)
	{
		for (const FHitResult& Hit : HitResults)
		{
			AFACharacter* Target = Cast<AFACharacter>(Hit.GetActor());
			if (Target && Target->GetAbilitySystemComponent()->HasMatchingGameplayTag(RequiredTag))
			{
				return Target;
			}
		}
	}
	return nullptr;
}
