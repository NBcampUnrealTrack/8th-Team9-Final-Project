#include "Rule/FADeathMatchSubsystem.h"

#include "Engine/World.h"
#include "Game/FAGameState.h"
#include "Player/FAPlayerState.h"
#include "TimerManager.h"

namespace FADeathMatch
{
	static constexpr float TimeLimit = 60.0f;
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
		UE_LOG(LogTemp, Warning, TEXT("[DeathMatch] 참가자가 2~3명이 아니라(%d명) 처리하지 않는다"), Contenders.Num());
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

	// TODO: 맵 1층 제한(수직 연결 봉쇄), 룰 드래프트 비활성화, 보유 룰 전부 Remove — 맵/GAS 담당자와 연동 필요

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

	GetWorld()->GetTimerManager().ClearTimer(DeathMatchTimerHandle);

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
	for (AFAPlayerState* PlayerState : TopPlacement)
	{
		if (PlayerState)
		{
			LastPlacements.Add(TWeakObjectPtr<AFAPlayerState>(PlayerState), 3);
		}
	}

	if (MidPlacement)
	{
		LastPlacements.Add(TWeakObjectPtr<AFAPlayerState>(MidPlacement), 2);
	}

	for (AFAPlayerState* PlayerState : BottomPlacement)
	{
		if (PlayerState)
		{
			LastPlacements.Add(TWeakObjectPtr<AFAPlayerState>(PlayerState), 1);
		}
	}

	// 순위 화면이 DeathMatch 페이즈 동안 잠겨 있었다면, 여기서 되돌려야 다시 그려진다.
	if (UWorld* World = GetWorld())
	{
		if (AFAGameState* FAGameState = World->GetGameState<AFAGameState>())
		{
			FAGameState->SetRoundPhase(EFARoundPhase::MatchEnd);
		}
	}

	// TODO: 나머지 관전자 → 원래 상태 복귀 등 데스매치 종료 후처리는 미확정.

	ActiveZombie.Reset();
	ActiveHumans.Reset();
}

int32 UFADeathMatchSubsystem::GetDeathMatchPlacement(const AFAPlayerState* PlayerState) const
{
	if (!PlayerState)
	{
		return 0;
	}

	const TWeakObjectPtr<AFAPlayerState> Key(const_cast<AFAPlayerState*>(PlayerState));
	const int32* Placement = LastPlacements.Find(Key);
	return Placement ? *Placement : 0;
}