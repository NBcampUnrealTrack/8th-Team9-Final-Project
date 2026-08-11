#include "Rule/FADeathMatchSubsystem.h"

#include "Engine/World.h"
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

	if (Contenders.Num() != 2)
	{
		// 3인 동점(좀비1 vs 인간2)은 기획 확정 전이라 아직 처리하지 않는다 (GDD "확정 필요" 항목).
		UE_LOG(LogTemp, Warning, TEXT("[DeathMatch] 참가자가 2명이 아니라(%d명) 아직 처리하지 않는다 (3인 데스매치 규칙 미확정)"),
			Contenders.Num());
		return;
	}

	AFAPlayerState* First = Contenders[0];
	AFAPlayerState* Second = Contenders[1];
	if (!First || !Second)
	{
		return;
	}

	// 진영 랜덤 배정 — 1명 인간, 1명 좀비.
	const bool bFirstIsZombie = FMath::RandBool();
	AFAPlayerState* Zombie = bFirstIsZombie ? First : Second;
	AFAPlayerState* Human = bFirstIsZombie ? Second : First;

	Zombie->SetTeam(EFATeam::Zombie);
	Human->SetTeam(EFATeam::Human);

	ActiveZombie = Zombie;
	ActiveHuman = Human;

	UE_LOG(LogTemp, Log, TEXT("[DeathMatch] 진영 배정 — 좀비: %s / 인간: %s"),
		*Zombie->GetPlayerName(), *Human->GetPlayerName());

	// TODO: 맵 1층 제한(수직 연결 봉쇄), 룰 드래프트 비활성화, 보유 룰 전부 Remove — 맵/GAS 담당자와 연동 필요

	GetWorld()->GetTimerManager().SetTimer(DeathMatchTimerHandle, this,
		&UFADeathMatchSubsystem::HandleTimeExpired, FADeathMatch::TimeLimit, false);
}

void UFADeathMatchSubsystem::NotifyContenderCaught(AFAPlayerState* CaughtHuman)
{
	if (!ActiveHuman.IsValid() || !ActiveZombie.IsValid() || ActiveHuman.Get() != CaughtHuman)
	{
		// 데스매치 중이 아니거나, 지금 추적 중인 인간이 아니면 무시.
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(DeathMatchTimerHandle);

	UE_LOG(LogTemp, Log, TEXT("[DeathMatch] %s 포획 성공 — 좀비 승"), *ActiveZombie->GetPlayerName());

	EndDeathMatch(ActiveZombie.Get(), ActiveHuman.Get());
}

void UFADeathMatchSubsystem::HandleTimeExpired()
{
	if (!ActiveHuman.IsValid() || !ActiveZombie.IsValid())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[DeathMatch] 1분 생존 — %s 승"), *ActiveHuman->GetPlayerName());

	EndDeathMatch(ActiveHuman.Get(), ActiveZombie.Get());
}

void UFADeathMatchSubsystem::EndDeathMatch(AFAPlayerState* Winner, AFAPlayerState* Loser)
{
	if (Winner)
	{
		UE_LOG(LogTemp, Log, TEXT("[DeathMatch] 최종 순위 — 1등: %s"), *Winner->GetPlayerName());
	}
	if (Loser)
	{
		UE_LOG(LogTemp, Log, TEXT("[DeathMatch] 최종 순위 — 2등: %s"), *Loser->GetPlayerName());
	}

	// TODO: 실제 최종 순위표(UI)에 이 결과를 반영하는 방법은 GameMode/UI 담당자와 상의 필요.
	// TODO: 나머지 관전자 → 원래 상태 복귀 등 데스매치 종료 후처리는 미확정.

	ActiveHuman.Reset();
	ActiveZombie.Reset();
}