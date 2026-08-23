// Fill out your copyright notice in the Description page of Project Settings.

#include "Interact/FAInteractionComponent.h"
#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "GAS/FAGameplayTags.h"
#include "GameFramework/PlayerController.h"
#include "Interact/FACabinet.h"
#include "Interact/FAInteractable.h"
#include "Player/FACharacter.h"
#include "TimerManager.h"

namespace
{
	/**
	 * 상호작용 디버그. 켜면 조준 대상 · 프롬프트 · 서버 거절 사유가 화면과 로그에 찍힌다.
	 */
	static TAutoConsoleVariable<int32> CVarInteractDebug(
		TEXT("FAInteract.Debug"),
		0,
		TEXT("1 이면 상호작용 조준 대상과 프롬프트를 화면에 표시한다."),
		ECVF_Cheat);

	/**
	 * 디버그 메시지 슬롯. 매 프레임 같은 줄을 덮어쓰기 위한 고정 키.
	 */
	constexpr uint64 InteractDebugKeyServer = 0xFA07;
	constexpr uint64 InteractDebugKeyClient = 0xFA08;

	bool IsInteractDebugOn()
	{
		return CVarInteractDebug.GetValueOnGameThread() != 0;
	}

	const TCHAR* RoleTag(const UActorComponent* Component)
	{
		return (Component && Component->GetOwnerRole() == ROLE_Authority) ? TEXT("서버") : TEXT("클라");
	}
}

UFAInteractionComponent::UFAInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	SetIsReplicatedByDefault(true);
}

void UFAInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	RefreshTickState();
}

void UFAInteractionComponent::RefreshTickState()
{
	// 탐지는 자기 화면에서만 한다. 서버 전용 인스턴스와 남의 캐릭터는 틱이 필요 없다.
	const AFACharacter* Owner = GetFACharacter();
	const bool bNeedsTick = Owner && Owner->IsLocallyControlled();

	if (bNeedsTick == IsComponentTickEnabled())
	{
		return;
	}

	SetComponentTickEnabled(bNeedsTick);

	if (IsInteractDebugOn())
	{
		UE_LOG(LogTemp, Log, TEXT("[Interact][%s] 탐지 %s"), RoleTag(this), bNeedsTick ? TEXT("켬") : TEXT("끔"));
	}

	// 틱이 꺼지면 마지막 대상이 화면에 남는다.
	if (!bNeedsTick)
	{
		SetCurrentTarget(nullptr);
	}
}

AFACharacter* UFAInteractionComponent::GetFACharacter() const
{
	return Cast<AFACharacter>(GetOwner());
}

void UFAInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AFACharacter* Character = GetFACharacter();
	if (!Character || !Character->IsLocallyControlled())
	{
		// 빙의가 풀린 것이다. 다시 빙의되면 RefreshTickState 가 켜준다 — 여기서 끄는 것은 영구가 아니다.
		RefreshTickState();
		return;
	}

	DetectTimer += DeltaTime;
	if (DetectInterval <= 0.0f || DetectTimer >= DetectInterval)
	{
		DetectTimer = 0.0f;
		SetCurrentTarget(CanCharacterInteract() ? FindBestTarget() : nullptr);
	}

	// 문구는 대상이 그대로여도 바뀐다 — 문 열기 ↔ 문 닫기.
	RefreshPrompt();

	if (bIsHolding)
	{
		// 붙들고 있는 도중에 대상이 사라지거나 조준이 벗어나면 접는다.
		if (!CurrentTarget.IsValid() || CurrentTarget.Get() != ServerHoldTarget.Get())
		{
			CancelHold();
		}
		else
		{
			// 완료 판정은 서버가 한다. 이 값은 게이지를 그리는 용도뿐이다.
			HoldElapsed = FMath::Min(HoldElapsed + DeltaTime, HoldDuration);
		}
	}

	if (CVarInteractDebug.GetValueOnGameThread() != 0 && GEngine)
	{
		const AActor* Target = CurrentTarget.Get();
		const FString HoldText = bIsHolding
			? FString::Printf(TEXT(" | 홀드 %.0f%%"), GetHoldProgress() * 100.0f)
			: FString();

		const bool bIsServer = Character->HasAuthority();
		GEngine->AddOnScreenDebugMessage(
			bIsServer ? InteractDebugKeyServer : InteractDebugKeyClient,
			0.0f,
			bIsServer ? FColor::Cyan : FColor::Green,
			FString::Printf(TEXT("[Interact][%s] %s | %s%s"),
				bIsServer ? TEXT("서버") : TEXT("클라"),
				Target ? *Target->GetName() : TEXT("(없음)"),
				*CurrentPrompt.ToString(),
				*HoldText));
	}
}

bool UFAInteractionComponent::CanCharacterInteract() const
{
	const AFACharacter* Character = GetFACharacter();
	if (!Character)
	{
		return false;
	}

	// 잡혀 있거나 구출 줄다리기 중이면 손이 비어 있지 않다.
	if (Character->GetCaptureState().IsActive() || Character->GetContestState().IsActive())
	{
		return false;
	}

	if (const UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
	{
		const FAGameplayTags& Tags = FAGameplayTags::Get();
		if (ASC->HasMatchingGameplayTag(Tags.State_Stunned))
		{
			return false;
		}

		// 드래프트 창이 열려 있는 동안은 입력이 카드 선택으로 간다.
		if (ASC->HasMatchingGameplayTag(Tags.State_Drafting))
		{
			return false;
		}
	}

	return true;
}

void UFAInteractionComponent::GetViewPoint(FVector& OutLocation, FVector& OutDirection) const
{
	const AFACharacter* Character = GetFACharacter();
	if (!Character)
	{
		OutLocation = FVector::ZeroVector;
		OutDirection = FVector::ForwardVector;
		return;
	}

	// 3인칭이어도 조준은 카메라 기준이다 — 화면에서 보이는 것과 잡히는 것이 어긋나면 안 된다.
	if (const APlayerController* PC = Cast<APlayerController>(Character->GetController()))
	{
		FRotator ViewRotation;
		PC->GetPlayerViewPoint(OutLocation, ViewRotation);
		OutDirection = ViewRotation.Vector();
		return;
	}

	OutLocation = Character->GetActorLocation();
	OutDirection = Character->GetActorForwardVector();
}

AActor* UFAInteractionComponent::FindBestTarget() const
{
	AFACharacter* Character = GetFACharacter();
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}

	if (AFACabinet* HidingCabinet = Character->GetHidingCabinet())
	{
		return HidingCabinet;
	}

	FVector ViewLocation;
	FVector ViewDirection;
	GetViewPoint(ViewLocation, ViewDirection);

	// 거리는 카메라가 아니라 캐릭터에서 잰다 — 3인칭에서 카메라가 뒤로 빠진 만큼 손해 보면 안 된다.
	const FVector Origin = Character->GetActorLocation();

	// 물건마다 사거리가 다를 수 있으므로 넉넉히 한 번 훑고, 개별 사거리는 뒤에서 다시 본다.
	const float SearchRadius = FMath::Max(DetectRange, 600.0f);

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FAInteractDetect), false, Character);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, ObjectParams,
		FCollisionShape::MakeSphere(SearchRadius), QueryParams);

	const float CosLimit = FMath::Cos(FMath::DegreesToRadians(DetectHalfAngleDegrees));

	AActor* BestActor = nullptr;
	float BestScore = -1.0f;

	// 같은 액터가 컴포넌트 수만큼 중복해서 올라오므로 한 번 본 것은 건너뛴다.
	TSet<AActor*> Seen;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.OverlapObjectHandle.FetchActor<AActor>();
		if (!Actor || Seen.Contains(Actor))
		{
			continue;
		}
		Seen.Add(Actor);

		if (!Actor->GetClass()->ImplementsInterface(UFAInteractable::StaticClass()))
		{
			continue;
		}

		if (!IFAInteractable::Execute_CanInteract(Actor, Character))
		{
			continue;
		}

		FVector Focus = IFAInteractable::Execute_GetInteractFocusLocation(Actor);
		if (Focus.IsNearlyZero())
		{
			Focus = Actor->GetActorLocation();
		}

		const float Range = GetTargetRange(Actor);
		const float Distance = FVector::Dist(Origin, Focus);
		if (Distance > Range)
		{
			continue;
		}

		// 시야 중심에 가까울수록 높은 점수. 같은 각도면 가까운 쪽이 이긴다.
		const FVector ToFocus = (Focus - ViewLocation).GetSafeNormal();
		const float Alignment = FVector::DotProduct(ViewDirection, ToFocus);
		if (Alignment < CosLimit)
		{
			continue;
		}

		const float Score = Alignment - (Distance / FMath::Max(Range, 1.0f)) * 0.15f;
		if (Score <= BestScore)
		{
			continue;
		}

		if (bRequireLineOfSight)
		{
			FCollisionQueryParams LosParams(SCENE_QUERY_STAT(FAInteractLoS), true, Character);
			LosParams.AddIgnoredActor(Actor);

			FHitResult Blocker;
			if (World->LineTraceSingleByChannel(Blocker, ViewLocation, Focus, ECC_Visibility, LosParams))
			{
				continue;
			}
		}

		BestScore = Score;
		BestActor = Actor;
	}

	return BestActor;
}

void UFAInteractionComponent::SetCurrentTarget(AActor* NewTarget)
{
	if (CurrentTarget.Get() == NewTarget)
	{
		return;
	}

	// 대상이 바뀌면 붙들고 있던 것은 무효다.
	if (bIsHolding)
	{
		CancelHold();
	}

	CurrentTarget = NewTarget;
	RefreshPrompt();

	OnTargetChanged.Broadcast(NewTarget, CurrentPrompt);
}

void UFAInteractionComponent::RefreshPrompt()
{
	AActor* Target = CurrentTarget.Get();
	if (!Target)
	{
		CurrentPrompt = FText::GetEmpty();
		return;
	}

	CurrentPrompt = IFAInteractable::Execute_GetInteractPrompt(Target, GetFACharacter());
}

float UFAInteractionComponent::GetHoldProgress() const
{
	if (!bIsHolding || HoldDuration <= 0.0f)
	{
		return 0.0f;
	}

	return FMath::Clamp(HoldElapsed / HoldDuration, 0.0f, 1.0f);
}

void UFAInteractionComponent::OnInteractPressed()
{
	AActor* Target = CurrentTarget.Get();

	if (IsInteractDebugOn())
	{
		UE_LOG(LogTemp, Log, TEXT("[Interact][%s] 키 입력 — 대상 %s"), RoleTag(this), Target ? *Target->GetName() : TEXT("(없음)"));
	}

	if (!Target)
	{
		return;
	}

	if (!CanCharacterInteract())
	{
		if (IsInteractDebugOn())
		{
			UE_LOG(LogTemp, Log, TEXT("[Interact][%s] 무시 — 지금 상호작용할 수 있는 상태가 아니다(포획/QTE/경직/드래프트)"), RoleTag(this));
		}
		return;
	}

	const float HoldSeconds = IFAInteractable::Execute_GetInteractHoldSeconds(Target, GetFACharacter());
	if (HoldSeconds <= 0.0f)
	{
		Server_RequestInteract(Target);
		return;
	}

	bIsHolding = true;
	HoldElapsed = 0.0f;
	HoldDuration = HoldSeconds;
	ServerHoldTarget = Target;

	Server_BeginHold(Target);
}

void UFAInteractionComponent::OnInteractReleased()
{
	if (bIsHolding)
	{
		CancelHold();
	}
}

void UFAInteractionComponent::CancelHold()
{
	bIsHolding = false;
	HoldElapsed = 0.0f;
	HoldDuration = 0.0f;
	ServerHoldTarget.Reset();

	Server_CancelHold();
}

float UFAInteractionComponent::GetTargetRange(const AActor* Target) const
{
	if (!Target)
	{
		return DetectRange;
	}

	const float Custom = IFAInteractable::Execute_GetInteractRange(Target);
	return Custom > 0.0f ? Custom : DetectRange;
}

bool UFAInteractionComponent::ValidateServerInteract(AActor* Target) const
{
	AFACharacter* Character = GetFACharacter();
	if (!Character || !IsValid(Target))
	{
		if (IsInteractDebugOn())
		{
			UE_LOG(LogTemp, Log, TEXT("[Interact][%s] 거절 — 대상이 없거나 이미 파괴됨"), RoleTag(this));
		}
		return false;
	}

	if (!Target->GetClass()->ImplementsInterface(UFAInteractable::StaticClass()))
	{
		if (IsInteractDebugOn())
		{
			UE_LOG(LogTemp, Log, TEXT("[Interact][%s] 거절 — %s 가 IFAInteractable 을 구현하지 않는다"), RoleTag(this), *Target->GetName());
		}
		return false;
	}

	if (!CanCharacterInteract())
	{
		if (IsInteractDebugOn())
		{
			UE_LOG(LogTemp, Log, TEXT("[Interact][%s] 거절 — 캐릭터 상태(포획/QTE/경직/드래프트)"), RoleTag(this));
		}
		return false;
	}

	if (!IFAInteractable::Execute_CanInteract(Target, Character))
	{
		if (IsInteractDebugOn())
		{
			UE_LOG(LogTemp, Log, TEXT("[Interact][%s] 거절 — %s 의 CanInteract 가 false (잠김/점유/진영 제한/등반 불가 룰)"), RoleTag(this), *Target->GetName());
		}
		return false;
	}

	FVector Focus = IFAInteractable::Execute_GetInteractFocusLocation(Target);
	if (Focus.IsNearlyZero())
	{
		Focus = Target->GetActorLocation();
	}

	// 서버가 보는 클라 위치는 한 프레임 뒤처져 있다 — 딱 맞게 자르면 문 앞에서 안 열리는 일이 생긴다.
	const float AllowedRange = GetTargetRange(Target) * ServerRangeTolerance;
	const float Distance = FVector::Dist(Character->GetActorLocation(), Focus);
	if (Distance > AllowedRange)
	{
		if (IsInteractDebugOn())
		{
			UE_LOG(LogTemp, Log, TEXT("[Interact][%s] 거절 — 사거리 초과 (%s, %.0f > 허용 %.0f)"), RoleTag(this), *Target->GetName(), Distance, AllowedRange);
		}
		return false;
	}

	return true;
}

void UFAInteractionComponent::Server_RequestInteract_Implementation(AActor* Target)
{
	if (IsInteractDebugOn())
	{
		UE_LOG(LogTemp, Log, TEXT("[Interact][%s] 요청 도착 — %s"), RoleTag(this), Target ? *Target->GetName() : TEXT("(없음)"));
	}

	if (!ValidateServerInteract(Target))
	{
		return;
	}

	AFACharacter* Character = GetFACharacter();

	// 점유형(사다리 · 캐비넷)은 자리를 잡지 못하면 실행 자체를 하지 않는다.
	if (IFAInteractable::Execute_IsInteractionExclusive(Target))
	{
		if (!IFAInteractable::Execute_TryOccupyInteraction(Target, Character))
		{
			if (IsInteractDebugOn())
			{
				UE_LOG(LogTemp, Log, TEXT("[Interact][%s] 거절 — %s 는 이미 다른 사람이 쓰고 있다"), RoleTag(this), *Target->GetName());
			}
			return;
		}
	}

	if (IsInteractDebugOn())
	{
		UE_LOG(LogTemp, Log, TEXT("[Interact][%s] 실행 — %s"), RoleTag(this), *Target->GetName());
	}
	IFAInteractable::Execute_Interact(Target, Character);
}

void UFAInteractionComponent::Server_BeginHold_Implementation(AActor* Target)
{
	GetWorld()->GetTimerManager().ClearTimer(ServerHoldTimerHandle);

	if (!ValidateServerInteract(Target))
	{
		return;
	}

	const float HoldSeconds = IFAInteractable::Execute_GetInteractHoldSeconds(Target, GetFACharacter());
	if (HoldSeconds <= 0.0f)
	{
		// 클라는 홀드형이라고 봤는데 서버는 즉시형이다 — 서버 쪽 값을 따른다.
		Server_RequestInteract_Implementation(Target);
		return;
	}

	ServerHoldTarget = Target;
	ServerHoldDuration = HoldSeconds;

	GetWorld()->GetTimerManager().SetTimer(ServerHoldTimerHandle, this,
		&UFAInteractionComponent::ServerCompleteHold, HoldSeconds, false);
}

void UFAInteractionComponent::Server_CancelHold_Implementation()
{
	GetWorld()->GetTimerManager().ClearTimer(ServerHoldTimerHandle);
	ServerHoldTarget.Reset();
	ServerHoldDuration = 0.0f;
}

void UFAInteractionComponent::ServerCompleteHold()
{
	AActor* Target = ServerHoldTarget.Get();
	ServerHoldTarget.Reset();
	ServerHoldDuration = 0.0f;

	if (!Target)
	{
		return;
	}

	// 붙들고 있는 동안 문이 잠겼거나 멀어졌을 수 있다 — 완료 시점에 한 번 더 본다.
	Server_RequestInteract_Implementation(Target);
}
