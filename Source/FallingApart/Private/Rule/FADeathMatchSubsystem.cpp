#include "Rule/FADeathMatchSubsystem.h"

#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GAS/FAAbilitySystemComponent.h"
#include "Game/FAGameMode.h"
#include "Game/FAGameState.h"
#include "GameFramework/PlayerStart.h"
#include "Player/FACharacter.h"
#include "Player/FAPlayerState.h"
#include "TimerManager.h"

namespace FADeathMatch
{
	static constexpr float TimeLimit = 60.0f;

	// 맵 담당자와의 규약. 자세한 설명은 FADeathMatchSubsystem.h 클래스 주석 참고.
	static const FName BlockerActorTag(TEXT("FADeathMatchBlocker"));
	static const FName ArenaPlayerStartTag(TEXT("DeathMatch"));

	// 데스매치 스폰의 진영 지정(선택). 일반 라운드 스폰의 Human / Zombie 태그와 겹치지 않게 접두어를 붙인다.
	static const FName HumanRoleTag(TEXT("DM_Human"));
	static const FName ZombieRoleTag(TEXT("DM_Zombie"));
}

void UFADeathMatchSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// 맵에 배치된 봉쇄 액터는 데스매치 전까지 없는 것처럼 보여야 한다.
	ApplyArenaBlockers(&InWorld, false);
}

void UFADeathMatchSubsystem::StartDeathMatch(const TArray<AFAPlayerState*>& Contenders)
{
	UE_LOG(LogTemp, Log, TEXT("[DeathMatch] StartDeathMatch called — 참가자 %d명"), Contenders.Num());

	for (AFAPlayerState* Contender : Contenders)
	{
		if (Contender)
		{
			UE_LOG(LogTemp, Log, TEXT("[DeathMatch]   - %s"), *Contender->GetPlayerName());
		}
	}

	if (Contenders.Num() != 2 && Contenders.Num() != 3)
	{
		AbortDeathMatch(FString::Printf(TEXT("참가자가 2~3명이 아니다(%d명)"), Contenders.Num()));
		return;
	}

	TArray<AFAPlayerState*> ValidContenders;
	for (AFAPlayerState* Contender : Contenders)
	{
		if (Contender)
		{
			ValidContenders.Add(Contender);
		}
	}

	if (ValidContenders.Num() != Contenders.Num())
	{
		// 빈 포인터가 섞여 있으면 인원수 전제(2~3명)가 깨진다 — 안전하게 중단.
		AbortDeathMatch(TEXT("참가자 PlayerState 가 유효하지 않다"));
		return;
	}

	// 진영 랜덤 배정 — 좀비 1명, 나머지 전원 인간 (GDD "좀비1 vs 인간2", 3인은 권장안 · 확정 필요).
	TArray<AFAPlayerState*> Remaining = ValidContenders;
	const int32 ZombieIndex = FMath::RandRange(0, Remaining.Num() - 1);
	AFAPlayerState* Zombie = Remaining[ZombieIndex];
	Remaining.RemoveAtSwap(ZombieIndex);

	Zombie->SetTeam(EFATeam::Zombie);
	ActiveZombie = Zombie;

	ActiveHumans.Reset();
	for (AFAPlayerState* Human : Remaining)
	{
		Human->SetTeam(EFATeam::Human);
		ActiveHumans.Add(Human);
	}

	UE_LOG(LogTemp, Log, TEXT("[DeathMatch] 진영 배정 — 좀비: %s / 인간 %d명"),
		*Zombie->GetPlayerName(), ActiveHumans.Num());

	// 맵 1층 제한 · 보유 룰 전부 제거 (GDD 5.4). 룰 드래프트는 매치가 이미 끝나 타이머가 없다.
	EnterArena(Zombie, Remaining);

	GetWorld()->GetTimerManager().SetTimer(DeathMatchTimerHandle, this,
		&UFADeathMatchSubsystem::HandleTimeExpired, FADeathMatch::TimeLimit, false);
}

void UFADeathMatchSubsystem::NotifyContenderCaught(AFAPlayerState* CaughtHuman)
{
	if (!ActiveZombie.IsValid() || !CaughtHuman)
	{
		return;
	}

	const TWeakObjectPtr<AFAPlayerState> WeakCaught(CaughtHuman);
	if (!ActiveHumans.Contains(WeakCaught))
	{
		// 데스매치 중이 아니거나, 지금 추적 중인 인간이 아니면 무시.
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[DeathMatch] %s 포획 성공 — 좀비 승"), *ActiveZombie->GetPlayerName());

	// 포획 안 당한 나머지 인간(들)은 좀비보단 위, 포획당한 인간보단 아래 — 중간 순위.
	AFAPlayerState* MidPlacementPlayer = nullptr;
	for (const TWeakObjectPtr<AFAPlayerState>& Human : ActiveHumans)
	{
		if (Human.IsValid() && Human.Get() != CaughtHuman)
		{
			// 3인 케이스는 최대 1명만 남으므로 이걸로 충분 (2명 이상 남는 경우는 없음 — 좀비 1 vs 인간 N-1).
			MidPlacementPlayer = Human.Get();
			break;
		}
	}

	EndDeathMatch({ ActiveZombie.Get() }, MidPlacementPlayer, { CaughtHuman });
}

void UFADeathMatchSubsystem::HandleTimeExpired()
{
	if (!ActiveZombie.IsValid() || ActiveHumans.Num() == 0)
	{
		return;
	}

	TArray<AFAPlayerState*> Survivors;
	for (const TWeakObjectPtr<AFAPlayerState>& Human : ActiveHumans)
	{
		if (Human.IsValid())
		{
			Survivors.Add(Human.Get());
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[DeathMatch] 1분 생존 — 인간 %d명 승"), Survivors.Num());

	// 끝까지 버틴 인간들은 전원 동일 순위(최상위) — 그들 사이 순서는 원래 점수 타이브레이크에 맡긴다.
	EndDeathMatch(Survivors, nullptr, { ActiveZombie.Get() });
}

void UFADeathMatchSubsystem::EndDeathMatch(const TArray<AFAPlayerState*>& TopPlacement, AFAPlayerState* MidPlacement,
	const TArray<AFAPlayerState*>& BottomPlacement)
{
	// 순위표는 AFAGameState::CompareRank() 4차(완전 동점 단계)에서 이 값을 읽는다 — 낮을수록 상위(1등=1).
	// 여기서 붙이지 않으면 완전 동점 그대로 남는다. 점수(MatchScore)는 전혀 건드리지 않는다.
	for (AFAPlayerState* PlayerState : TopPlacement)
	{
		if (PlayerState)
		{
			PlayerState->SetDeathMatchPlacement(1);
			UE_LOG(LogTemp, Log, TEXT("[DeathMatch] 최종 순위 — 1등: %s"), *PlayerState->GetPlayerName());
		}
	}

	if (MidPlacement)
	{
		MidPlacement->SetDeathMatchPlacement(2);
		UE_LOG(LogTemp, Log, TEXT("[DeathMatch] 최종 순위 — 2등: %s"), *MidPlacement->GetPlayerName());
	}

	const int32 BottomRank = MidPlacement ? 3 : 2;
	for (AFAPlayerState* PlayerState : BottomPlacement)
	{
		if (PlayerState)
		{
			PlayerState->SetDeathMatchPlacement(BottomRank);
			UE_LOG(LogTemp, Log, TEXT("[DeathMatch] 최종 순위 — %d등: %s"), BottomRank, *PlayerState->GetPlayerName());
		}
	}

	LeaveArena();

	// 데스매치가 끝나면 매치도 끝이다 — 관전 해제 · 최종 순위표는 GameMode 가 한 곳에서 처리한다.
	if (AFAGameMode* GameMode = GetWorld()->GetAuthGameMode<AFAGameMode>())
	{
		GameMode->FinishMatchAfterDeathMatch();
	}
}

void UFADeathMatchSubsystem::AbortDeathMatch(const FString& Reason)
{
	// GameMode 는 이 함수를 부르기 전에 이미 DeathMatch 페이즈를 켜고 나머지를 관전자로 돌려놨다.
	// 여기서 매치를 정상 종료로 되돌리지 않으면 게임이 그 상태로 영원히 멈춘다.
	UE_LOG(LogTemp, Warning, TEXT("[DeathMatch] 데스매치를 열지 못했다 — %s. 공동 1등으로 매치를 끝낸다."), *Reason);

	LeaveArena();

	if (AFAGameMode* GameMode = GetWorld()->GetAuthGameMode<AFAGameMode>())
	{
		GameMode->FinishMatchAfterDeathMatch();
	}
}

void UFADeathMatchSubsystem::EnterArena(AFAPlayerState* Zombie, const TArray<AFAPlayerState*>& Humans)
{
	TArray<AFAPlayerState*> Contenders;
	Contenders.Add(Zombie);
	Contenders.Append(Humans);

	// 보유 룰 전부 제거 — 5라운드 동안 쌓은 룰을 그대로 들고 붙으면 데스매치가 성립하지 않는다 (GDD 5.4).
	for (AFAPlayerState* Contender : Contenders)
	{
		AFACharacter* Character = Contender ? Cast<AFACharacter>(Contender->GetPawn()) : nullptr;
		if (UFAAbilitySystemComponent* ASC = Character ? Character->GetFAAbilitySystemComponent() : nullptr)
		{
			ASC->RemoveAllRules();
		}
	}

	// 1층 전용 스폰으로 이동. 매치 종료 시점의 위치가 그대로 유불리로 이어지면 안 되고,
	// 봉쇄가 켜질 때 2층에 있으면 갇힌다.
	TArray<AActor*> ZombieStarts;
	TArray<AActor*> HumanStarts;
	TArray<AActor*> AnyStarts;
	GatherArenaStarts(ZombieStarts, HumanStarts, AnyStarts);

	// 진영 태그가 붙은 스폰을 우선 쓰고, 없으면 진영 지정이 없는 자리에서 가져온다. 뽑은 자리는 바로 제거해
	// 다음 참가자와 겹치지 않게 한다.
	auto TakeRandom = [](TArray<AActor*>& Starts) -> AActor*
	{
		if (Starts.Num() == 0)
		{
			return nullptr;
		}
		const int32 Index = FMath::RandRange(0, Starts.Num() - 1);
		AActor* Picked = Starts[Index];
		Starts.RemoveAtSwap(Index);
		return Picked;
	};

	AActor* ZombieStart = TakeRandom(ZombieStarts);
	if (!ZombieStart)
	{
		ZombieStart = TakeRandom(AnyStarts);
	}

	TArray<AActor*> AssignedHumanStarts;
	for (int32 Index = 0; Index < Humans.Num(); ++Index)
	{
		AActor* Start = TakeRandom(HumanStarts);
		if (!Start)
		{
			Start = TakeRandom(AnyStarts);
		}
		AssignedHumanStarts.Add(Start);
	}

	bool bAllAssigned = (ZombieStart != nullptr);
	for (AActor* Start : AssignedHumanStarts)
	{
		bAllAssigned &= (Start != nullptr);
	}

	if (bAllAssigned)
	{
		auto MoveTo = [](APawn* Pawn, AActor* Start)
		{
			if (Pawn && Start)
			{
				Pawn->TeleportTo(Start->GetActorLocation(), Start->GetActorRotation());

				if (AController* Controller = Pawn->GetController())
				{
					Controller->SetControlRotation(Start->GetActorRotation());
				}
			}
		};

		MoveTo(Zombie ? Zombie->GetPawn() : nullptr, ZombieStart);
		for (int32 Index = 0; Index < Humans.Num(); ++Index)
		{
			MoveTo(Humans[Index] ? Humans[Index]->GetPawn() : nullptr, AssignedHumanStarts[Index]);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[DeathMatch] '%s' 스폰이 부족해 참가자를 옮기지 않았다 (좀비용 %d · 인간용 %d · 진영무관 %d — 참가자 %d명 필요)"),
			*FADeathMatch::ArenaPlayerStartTag.ToString(), ZombieStarts.Num(), HumanStarts.Num(), AnyStarts.Num(),
			Contenders.Num());
	}

	// 수직 연결 봉쇄.
	if (AFAGameState* FAGameState = GetWorld()->GetGameState<AFAGameState>())
	{
		FAGameState->SetDeathMatchArenaActive(true);
	}
}

void UFADeathMatchSubsystem::LeaveArena()
{
	GetWorld()->GetTimerManager().ClearTimer(DeathMatchTimerHandle);

	if (AFAGameState* FAGameState = GetWorld()->GetGameState<AFAGameState>())
	{
		FAGameState->SetDeathMatchArenaActive(false);
	}

	ActiveZombie.Reset();
	ActiveHumans.Reset();
}

bool UFADeathMatchSubsystem::PlayerStartHasTag(const APlayerStart* PlayerStart, const FName& Tag)
{
	// Player Start Tag 프로퍼티와 액터 태그 둘 다 인정 — 레벨마다 다는 방식이 다르다.
	return PlayerStart != nullptr && (PlayerStart->PlayerStartTag == Tag || PlayerStart->ActorHasTag(Tag));
}

bool UFADeathMatchSubsystem::IsArenaPlayerStart(const APlayerStart* PlayerStart)
{
	return PlayerStartHasTag(PlayerStart, FADeathMatch::ArenaPlayerStartTag);
}

void UFADeathMatchSubsystem::GatherArenaStarts(TArray<AActor*>& OutZombieStarts, TArray<AActor*>& OutHumanStarts,
	TArray<AActor*>& OutAnyStarts) const
{
	OutZombieStarts.Reset();
	OutHumanStarts.Reset();
	OutAnyStarts.Reset();

	for (TActorIterator<APlayerStart> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		APlayerStart* PlayerStart = *Iterator;
		if (!IsArenaPlayerStart(PlayerStart))
		{
			continue;
		}

		if (PlayerStartHasTag(PlayerStart, FADeathMatch::ZombieRoleTag))
		{
			OutZombieStarts.Add(PlayerStart);
		}
		else if (PlayerStartHasTag(PlayerStart, FADeathMatch::HumanRoleTag))
		{
			OutHumanStarts.Add(PlayerStart);
		}
		else
		{
			OutAnyStarts.Add(PlayerStart);
		}
	}
}

void UFADeathMatchSubsystem::ApplyArenaBlockers(UWorld* World, bool bEnabled)
{
	if (!World)
	{
		return;
	}

	int32 BlockerCount = 0;
	for (TActorIterator<AActor> Iterator(World); Iterator; ++Iterator)
	{
		AActor* Actor = *Iterator;
		if (!Actor || !Actor->ActorHasTag(FADeathMatch::BlockerActorTag))
		{
			continue;
		}

		Actor->SetActorHiddenInGame(!bEnabled);
		Actor->SetActorEnableCollision(bEnabled);

		if (bEnabled)
		{
			Actor->ForEachComponent<UPrimitiveComponent>(false, [](UPrimitiveComponent* Primitive)
			{
				if (Primitive->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
				{
					Primitive->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
				}

				Primitive->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
			});
		}

		++BlockerCount;
	}

	if (BlockerCount > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[DeathMatch] 1층 봉쇄 %s — 액터 %d개"), bEnabled ? TEXT("ON") : TEXT("OFF"), BlockerCount);
	}
	else if (bEnabled)
	{
		// 끌 때는 조용히 넘어간다 — 메인 메뉴처럼 봉쇄가 필요 없는 레벨에서도 이 함수가 한 번 돈다.
		UE_LOG(LogTemp, Warning,
			TEXT("[DeathMatch] 액터 태그가 '%s' 인 봉쇄 액터가 레벨에 없다 — 1층 제한이 걸리지 않는다 (맵 담당자 확인)"),
			*FADeathMatch::BlockerActorTag.ToString());
	}
}
