#include "Player/FATrailComponent.h"

#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GAS/FAGameplayTags.h"
#include "Math/RotationMatrix.h"
#include "Net/UnrealNetwork.h"
#include "Player/FACharacter.h"
#include "TimerManager.h"

UFATrailComponent::UFATrailComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UFATrailComponent::BeginPlay()
{
	Super::BeginPlay();
	// 룰 제거 후에도 기존 기록 만료와 로컬 진영 전환을 처리한다.
	GetWorld()->GetTimerManager().SetTimer(UpdateTimer, this,
		&ThisClass::UpdateTrail, 0.1f, true);
}

void UFATrailComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(UpdateTimer);
	ClearTrailVisuals();
	Super::EndPlay(EndPlayReason);
}

void UFATrailComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UFATrailComponent, TrailPoints);
}

void UFATrailComponent::StartTrail()
{
	if (GetOwner()->HasAuthority() && !bRecording)
	{
		bRecording = true;
		bHasLastLocation = false;
	}
}

void UFATrailComponent::StopTrail()
{
	// 새 기록만 중지한다. 기존 기록/효과는 UpdateTrail에서 각자의 20초 수명이 끝나면 제거한다.
	if (GetOwner()->HasAuthority())
	{
		bRecording = false;
		bHasLastLocation = false;
	}
}

void UFATrailComponent::UpdateTrail()
{
	// 기록의 생성과 삭제는 서버만 결정한다. 룰이 꺼져 있어도 오래된 기록은 계속 정리한다.
	if (GetOwner()->HasAuthority())
	{
		const double Now = GetWorld()->GetTimeSeconds();
		const int32 Removed = TrailPoints.RemoveAll([Now](const FATrailPoint& Point)
		{
			return Now - Point.SpawnTime >= TrailLifetime;
		});
		if (Removed > 0)
		{
			GetOwner()->ForceNetUpdate();
		}
		if (bRecording)
		{
			RecordTrailPoint(Now);
		}
	}
	// 전용 서버는 화면 효과가 필요 없다. 리슨 서버 호스트는 자신의 화면 표시가 필요하다.
	if (GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		RefreshTrailVisuals();
	}
}

void UFATrailComponent::RecordTrailPoint(double Now)
{
	const AFACharacter* Character = Cast<AFACharacter>(GetOwner());
	const UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	const FAGameplayTags& Tags = FAGameplayTags::Get();
	if (!ASC || !Movement || !ASC->HasMatchingGameplayTag(Tags.Rule_State_Trail)
		|| !ASC->HasMatchingGameplayTag(Tags.State_Human))
	{
		bHasLastLocation = false;
		return;
	}
	// 공중/정지 상태에서는 생성하지 않는다. 수평 속도 3cm/s 미만도 정지로 취급한다.
	if (!Movement->IsMovingOnGround() || Character->GetVelocity().SizeSquared2D() < 9.0f)
	{
		return;
	}
	// 0.1초 검사 시 간격을 넘었으면 한 개를 기록한다. 이동 구간을 75cm마다 보간하지는 않는다.
	const FVector Location = Character->GetActorLocation();
	if (bHasLastLocation && FVector::DistSquared2D(Location, LastLocation)
		< FMath::Square(FMath::Max(1.0f, PointSpacing)))
	{
		return;
	}
	// 발 소켓 대신 캐릭터 중심에서 아래로 검사한다. 바닥은 Visibility 채널에 응답해야 한다.
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Character);
	FHitResult Hit;
	const double TraceDepth = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 100.0;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Location,
		Location - FVector(0.0, 0.0, TraceDepth), ECC_Visibility, Params))
	{
		return;
	}
	FATrailPoint Point;
	Point.Id = NextPointId++;
	// 표면과 겹침을 줄이기 위해 법선 방향으로 1cm 띄운다.
	Point.Location = Hit.ImpactPoint + Hit.ImpactNormal;
	// Z축은 바닥 법선, X축은 실제 이동 방향으로 정한다. 옆걸음/뒷걸음도 이동 경로를 따른다.
	// 이동 방향을 바닥 평면에 투영해 경사면에서도 방향을 유지한다.
	FVector TrailForward = FVector::VectorPlaneProject(Character->GetVelocity(), Hit.ImpactNormal).GetSafeNormal();
	if (TrailForward.IsNearlyZero())
	{
		TrailForward = Character->GetActorForwardVector();
	}
	Point.Rotation = FRotationMatrix::MakeFromZX(Hit.ImpactNormal, TrailForward).Rotator();
	Point.SpawnTime = Now;
	TrailPoints.Add(Point);
	LastLocation = Location;
	bHasLastLocation = true;
	GetOwner()->ForceNetUpdate();
}

void UFATrailComponent::RefreshTrailVisuals()
{
	// 발자국 주인이 아니라 이 화면을 보는 플레이어의 진영을 확인한다. 화면 분할은 지원하지 않는다.
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const AFACharacter* Viewer = PC && PC->IsLocalController() ? Cast<AFACharacter>(PC->GetPawn()) : nullptr;
	const UAbilitySystemComponent* ASC = Viewer ? Viewer->GetAbilitySystemComponent() : nullptr;
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	
	//  확인여부 
	if (!ASC || !ASC->HasMatchingGameplayTag(FAGameplayTags::Get().State_Zombie)
		|| !GameState || !TrailSystem)
	{
		ClearTrailVisuals();
		return;
	}
	// 서버와 동기화된 시각을 사용한다. 15초 된 기록을 늦게 받으면 5초만 더 표시한다.
	const double Now = GameState->GetServerWorldTimeSeconds();
	TSet<int32> ActiveIds;
	for (const FATrailPoint& Point : TrailPoints)
	{
		const double Remaining = TrailLifetime - FMath::Max(0.0, Now - Point.SpawnTime);
		if (Remaining <= 0.0)
		{
			continue;
		}
		ActiveIds.Add(Point.Id);
		UNiagaraComponent* Effect = LocalEffects.FindRef(Point.Id);
		// 이미 생성한 ID는 재생성하지 않고 시간만 갱신한다.
		// Niagara에서 같은 이름의 Float User Parameter를 만들고 페이드 등에 연결해야 시각적으로 반영된다.
		if (IsValid(Effect))
		{
			Effect->SetVariableFloat(TEXT("User.TrailAge"), static_cast<float>(TrailLifetime - Remaining));
			Effect->SetVariableFloat(TEXT("User.TrailRemainingLifetime"), static_cast<float>(Remaining));
			continue;
		}
		// 자동 삭제=false: 수명은 아래 C++ 삭제 로직이 관리한다.
		// 자동 활성화=false: 시간 파라미터를 먼저 설정한 뒤 재생한다.
		// 풀링=None, 사전 컬링=false: 기록별 인스턴스를 직접 보관/제거하고 생성 전 컬링을 생략한다.
		// Niagara 자체가 먼저 끝나면 다시 재생하지 않으므로 에셋은 삭제 시점까지 유지되도록 구성한다.
		const AFACharacter* Character = Cast<AFACharacter>(GetOwner());
		USkeletalMeshComponent* SourceMesh = Character ? Character->GetMesh() : nullptr;
		if (!SourceMesh)
		{
			continue;
		}
		// 클라이언트가 지금 보고 있는 메시의 변환/포즈를 사용한다. 과거 포즈는 기록하지 않는다.
		// 따라서 늦게 받은 기록도 현재 위치/포즈로 생성된다. 정확한 과거 잔상에는 별도 기록이 필요하다.
		// Niagara의 Local Space는 끄고 Particle Spawn에서만 메시를 샘플링해야 잔상이 따라오지 않는다.
		Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			TrailSystem,
			SourceMesh->GetComponentLocation(),
			SourceMesh->GetComponentRotation(), SourceMesh->GetComponentScale() * EffectScale,
			false, false, ENCPoolMethod::None, false);
		if (Effect)
		{
			// 활성화 전에 실제 애니메이션 메시를 연결한다. 에셋의 Mesh 입력도 이 파라미터에 바인딩해야 한다.
			UNiagaraFunctionLibrary::OverrideSystemUserVariableSkeletalMeshComponent(
				Effect, TrailMeshParameterName.ToString(), SourceMesh);
			// Lifetime=전체 20초, Age=서버 생성 후 경과 시간, RemainingLifetime=남은 시간.
			// 이 값 전달이 Niagara 시뮬레이션을 과거부터 재생하는 것은 아니다.
			// 페이드 예: Niagara에서 투명도에 1 - clamp(TrailAge / TrailLifetime, 0, 1)을 사용한다.
			Effect->SetVariableFloat(TEXT("User.TrailLifetime"), static_cast<float>(TrailLifetime));
			Effect->SetVariableFloat(TEXT("User.TrailAge"), static_cast<float>(TrailLifetime - Remaining));
			Effect->SetVariableFloat(TEXT("User.TrailRemainingLifetime"), static_cast<float>(Remaining));
			LocalEffects.Add(Point.Id, Effect);
			Effect->Activate(true);
		}
	}
	// 만료되거나 복제 배열에서 사라진 기록의 효과를 즉시 제거한다.
	for (auto It = LocalEffects.CreateIterator(); It; ++It)
	{
		if (!ActiveIds.Contains(It.Key()))
		{
			if (IsValid(It.Value()))
			{
				It.Value()->DestroyComponent();
			}
			It.RemoveCurrent();
		}
	}
}

void UFATrailComponent::ClearTrailVisuals()
{
	// 인간으로 전환하거나 캐릭터가 종료될 때 호출한다. 화면 효과만 지우며 서버 기록은 건드리지 않는다.
	for (const auto& Pair : LocalEffects)
	{
		if (IsValid(Pair.Value))
		{
			Pair.Value->DestroyComponent();
		}
	}
	LocalEffects.Reset();
}
