// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Abilities/FAGA_Grab.h"
#include "GAS/FAGameplayTags.h"
#include "AttributeSet/FAAttributeSet.h"
#include "Game/FAGameMode.h"
#include "Interact/FALadder.h"
#include "Player/FACharacter.h"
#include "Player/FAPlayerState.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"

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

	// 실제 포획 성공 시 Attribute 값으로 다시 설정된다.
	CapturedMaxResistTimeMultiplier = 1.0f;

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

	// 사다리에 붙어 있으면 정면이 벽이라 스윕이 절대 사람을 못 문다 — 사다리 위·아래를 먼저 본다.
	// 못 찾으면 평소 판정으로 떨어진다(사다리 옆에 선 사람은 그쪽으로 걸린다).
	AFACharacter* TargetHuman = ZombieChar->IsClimbing()
		? FindTargetOnLadder(ZombieChar, GrabRange, Tags.State_Human)
		: nullptr;

	if (!TargetHuman)
	{
		TargetHuman = FindTargetInFront(ZombieChar, GrabRange, SphereRadius, Tags.State_Human);
	}

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

	UAbilitySystemComponent* TargetASC = TargetHuman->GetAbilitySystemComponent();
	if (!TargetASC)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	const bool bHasSecondLife = TargetASC->HasMatchingGameplayTag(Tags.Rule_Trade_SecondLife);
	const bool bSecondLifeConsumed = TargetASC->HasMatchingGameplayTag(Tags.State_SecondLife_Consumed);

	UE_LOG(LogTemp,Warning,
		TEXT("[H49] 포획 판정 | Grabber=%s | Target=%s | HasSecondLife=%s | Consumed=%s"),
		*GetNameSafe(ZombieChar),*GetNameSafe(TargetHuman),
		bHasSecondLife ? TEXT("True") : TEXT("False"),
		bSecondLifeConsumed ? TEXT("True") : TEXT("False"));

	if (bHasSecondLife && !bSecondLifeConsumed)
	{
		TargetASC->AddLooseGameplayTag(Tags.State_SecondLife_Consumed,1,
			EGameplayTagReplicationState::TagAndCountToAll);

		UE_LOG(LogTemp,Warning,
			TEXT("[H49] 첫 포획 무효 | Target=%s | ConsumedCount=%d"),
			*GetNameSafe(TargetHuman),
			TargetASC->GetTagCount(Tags.State_SecondLife_Consumed));

		EndAbility(CurrentSpecHandle,CurrentActorInfo,CurrentActivationInfo,true,false);
		return;
	}

	// 포획 성공 처리
	GrabbedVictim = TargetHuman;

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

	// H39 카드는 인간 진영 룰이므로 TargetASC(인간)의 속성을 읽어온다.
	float InfectDuration = TargetASC->GetNumericAttribute(UFAAttributeSet::GetInfectDurationAttribute());
	if (InfectDuration <= 0.01f) // H39 - 감염 소요 시간이 0초라면 줄다리기(QTE) 없이 즉시 감염
	{
		CompleteInfection();
		return;
	}

	BaseInfectDuration = InfectDuration;
	CurrentInfectRate = 1.0f / BaseInfectDuration;

	float ResistTimeMultiplier =SourceASC->GetNumericAttribute(UFAAttributeSet::GetMaxResistTimeMultiplierAttribute());

	if (ResistTimeMultiplier < 1.0f)
	{
		ResistTimeMultiplier = 3.0f;
	}

	// Z43 광폭: 저항 0에서 10초, 최대 저항에서 20초.
	if (SourceASC->HasMatchingGameplayTag(Tags.Rule_Trade_Frenzy))
	{
		BaseInfectDuration = 10.0f;
		ResistTimeMultiplier = 2.0f;
		CurrentInfectRate = 1.0f / BaseInfectDuration;

		UE_LOG(LogTemp,Log,
			TEXT("[Z43] 광폭 감염 시간 적용 | Base=%.1fs | Max=%.1fs"),
			BaseInfectDuration,BaseInfectDuration * ResistTimeMultiplier);
	}

	// 포획 도중 GE 값이 바뀌어도 이번 포획에는 시작 시점 값을 사용한다.
	CapturedMaxResistTimeMultiplier =ResistTimeMultiplier;

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
	const float CurrentDuration = FMath::Max(0.1f,BaseInfectDuration
		*FMath::Lerp(1.0f,CapturedMaxResistTimeMultiplier,ResistLevel));

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
			ResolvePlagueGamble();
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UFAGA_Grab::ResolvePlagueGamble()
{
	// Z50 판정과 추가 감염은 서버에서만 실행한다.
    if (!HasAuthority(&CurrentActivationInfo))
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // 현재 서버의 GameMode를 가져와 사용.
    AFAGameMode* GameMode =World->GetAuthGameMode<AFAGameMode>();
    if (!GameMode)
    {
        return;
    }
    // 현재 Grab 어빌리티를 사용한 캐릭터가 감염자다.
    AFACharacter* InfectorCharacter =Cast<AFACharacter>(GetAvatarActorFromActorInfo());
    if (!InfectorCharacter)
    {
        return;
    }

    // 감염자의 PlayerState를 가져온다.
    AFAPlayerState* InfectorState =InfectorCharacter->GetPlayerState<AFAPlayerState>();

    if (!InfectorState)
    {
        return;
    }
    // Z50 보유 여부와 실패 GE 적용을 위해
    // 감염자의 AbilitySystemComponent를 가져온다.
    UAbilitySystemComponent* InfectorASC =InfectorCharacter->GetAbilitySystemComponent();
    if (!InfectorASC)
    {
        return;
    }

    const FAGameplayTags& Tags = FAGameplayTags::Get();

    // Rule.Gamble.Plague를 가진 좀비만 Z50을 발동한다.
    if (!InfectorASC->HasMatchingGameplayTag(Tags.Rule_Gamble_Plague))
    {
        return;
    }

    // 이번 포획으로 이미 감염된 1차 피해자다.
    //
    // ResolvePlagueGamble()은 EndAbility() 전에 호출되므로
    // GrabbedVictim은 아직 유효한 상태다.
    AFACharacter* PrimaryVictimCharacter = GrabbedVictim.Get();

    AFAPlayerState* PrimaryVictimState = PrimaryVictimCharacter? PrimaryVictimCharacter
                ->GetPlayerState<AFAPlayerState>(): nullptr;

	// 성공 확률설정 . 매직 넘버 수정 생각 .
    constexpr float PlagueSuccessChance = 0.35f;

    const float RandomRoll = FMath::FRand();

    const bool bPlagueSucceeded = RandomRoll < PlagueSuccessChance;

    if (!bPlagueSucceeded)
    {
    	// 실패처리 / 20초 정지 Gameplay Effect를 적용한다.
        if (PlagueFailStunEffectClass)
        {
            FGameplayEffectSpecHandle StunSpec =MakeOutgoingGameplayEffectSpec(PlagueFailStunEffectClass);

            if (StunSpec.IsValid()&& StunSpec.Data.IsValid())
            {
                InfectorASC->ApplyGameplayEffectSpecToSelf(*StunSpec.Data.Get());
            }
        }
        else
        {
            // 블루프린트 Class Defaults에 실패 GE가
            // 지정되지 않았을 때 확인할 수 있는 오류 로그다.
            UE_LOG(LogTemp,Error,TEXT("[Z50] PlagueFailStunEffectClass가 ""설정되지 않았습니다."));
        }

        UE_LOG(LogTemp,Warning,TEXT("[Z50] 전염 도박 실패 | ""Infector=%s | Roll=%.3f | "
                "Chance=%.2f | Stun=20.0s"),*GetNameSafe(InfectorCharacter),RandomRoll,PlagueSuccessChance);

        return;
    }
	// 성공처리
    TArray<AFAPlayerState*> HumanCandidates;

    // 현재 맵에 존재하는 모든 캐릭터를 검사한다.
    for (TActorIterator<AFACharacter> Iterator(World);Iterator;++Iterator)
    {
        AFACharacter* CandidateCharacter =*Iterator;

        if (!IsValid(CandidateCharacter))
        {
            continue;
        }

        AFAPlayerState* CandidateState = CandidateCharacter->GetPlayerState<AFAPlayerState>();

        if (!CandidateState)
        {
            continue;
        }

        // 이번에 이미 감염된 1차 피해자를 제외한다.
        if (CandidateState == PrimaryVictimState)
        {
            continue;
        }

        // Z50을 발동시킨 좀비 자신을 제외한다.
        if (CandidateState == InfectorState)
        {
            continue;
        }

        // 현재 인간인 플레이어만 추가 감염 후보로 사용한다.
        if (!CandidateState->IsHuman())
        {
            continue;
        }

        HumanCandidates.Add(CandidateState);
    }

	// 추가 감염 인간이 없는 경우
    if (HumanCandidates.IsEmpty())
    {
        // 이 경우는 확률 판정 실패가 아니므로
        // 좀비에게 20초 정지를 적용하지 않는다.
        UE_LOG(LogTemp,Warning,TEXT("[Z50] 전염 도박 성공, 추가 대상 없음 | ""Infector=%s | Roll=%.3f"),
            *GetNameSafe(InfectorCharacter),RandomRoll);

        return;
    }

    // 인간 후보 중 한 명을 무작위로 선택한다.
    const int32 RandomTargetIndex =FMath::RandRange(0,HumanCandidates.Num() - 1);

    AFAPlayerState* AdditionalVictim =HumanCandidates[RandomTargetIndex];

	// 갱신
    GameMode->HandleInfection(AdditionalVictim,InfectorState);

    UE_LOG(LogTemp,Warning,TEXT("[Z50] 전염 도박 성공 | "
            "Infector=%s | AdditionalVictim=%s | "
            "Roll=%.3f | Chance=%.2f"),
        *GetNameSafe(InfectorCharacter),*GetNameSafe(AdditionalVictim),
        RandomRoll,PlagueSuccessChance);
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

AFACharacter* UFAGA_Grab::FindTargetOnLadder(AFACharacter* Avatar, float Range, FGameplayTag RequiredTag) const
{
	AFALadder* Ladder = Avatar ? Avatar->GetClimbingLadder() : nullptr;
	if (!Ladder)
	{
		return nullptr;
	}

	const UCapsuleComponent* MyCapsule = Avatar->GetCapsuleComponent();
	const float MyHalfHeight = MyCapsule ? MyCapsule->GetScaledCapsuleHalfHeight() : 0.0f;

	// 사다리에 겹쳐 붙은 두 사람은 캡슐이 서로를 막아 중심끼리 키만큼 벌어진다.
	// 중심 거리로 재면 사거리를 넘겨버리므로 몸 표면 사이의 간격으로 잰다 — 캡슐 크기와 무관해진다.
	const FVector Origin = Avatar->GetActorLocation();

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Avatar);

	Avatar->GetWorld()->OverlapMultiByChannel(
		Overlaps, Origin, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeSphere(Range + MyHalfHeight * 2.0f), QueryParams);

	AFACharacter* Nearest = nullptr;
	float NearestGap = TNumericLimits<float>::Max();

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AFACharacter* Candidate = Cast<AFACharacter>(Overlap.GetActor());

		// 같은 사다리에 붙은 사람만 본다. 꼭대기 바닥에 선 사람까지 걸리면 벽 너머로 잡는 꼴이 된다.
		if (!Candidate || Candidate->GetClimbingLadder() != Ladder)
		{
			continue;
		}

		const UAbilitySystemComponent* CandidateASC = Candidate->GetAbilitySystemComponent();
		if (!CandidateASC || !CandidateASC->HasMatchingGameplayTag(RequiredTag))
		{
			continue;
		}

		const UCapsuleComponent* TheirCapsule = Candidate->GetCapsuleComponent();
		const float TheirHalfHeight = TheirCapsule ? TheirCapsule->GetScaledCapsuleHalfHeight() : 0.0f;

		const float Gap = FVector::Dist(Origin, Candidate->GetActorLocation()) - MyHalfHeight - TheirHalfHeight;
		if (Gap <= Range && Gap < NearestGap)
		{
			NearestGap = Gap;
			Nearest = Candidate;
		}
	}

	return Nearest;
}
