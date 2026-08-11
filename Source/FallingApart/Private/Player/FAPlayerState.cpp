// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/FAPlayerState.h"

#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Player/FACharacter.h"
#include "Rule/FARuleDraftSubsystem.h"
#include "UI/FAHUD.h"

AFAPlayerState::AFAPlayerState()
{
	PrimaryActorTick.bCanEverTick = false;

	// 룰 태그·Attribute 를 다루는 ASC 는 캐릭터(AFACharacter) 에 위치
	// AbilitySystemComponent = CreateDefaultSubobject<UFAAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	// AbilitySystemComponent->SetIsReplicated(true);
	// AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	// AttributeSet = CreateDefaultSubobject<UFAAttributeSet>(TEXT("AttributeSet"));
}

void AFAPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFAPlayerState, Team);
	DOREPLIFETIME(AFAPlayerState, RoundScore);
	DOREPLIFETIME(AFAPlayerState, MatchScore);
	DOREPLIFETIME(AFAPlayerState, DeathMatchPlacement);
}

void AFAPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	// 심리스 트래블 / 재접속 시 매치 누적 점수를 잃지 않도록 넘겨줌
	if (AFAPlayerState* FAPlayerState = Cast<AFAPlayerState>(PlayerState))
	{
		FAPlayerState->Team = Team;
		FAPlayerState->RoundScore = RoundScore;
		FAPlayerState->MatchScore = MatchScore;
		FAPlayerState->DeathMatchPlacement = DeathMatchPlacement;
	}
}

EFATeam AFAPlayerState::GetMainSide() const
{
	return (MatchScore.GetZombieTotal() > MatchScore.GetHumanTotal()) ? EFATeam::Zombie : EFATeam::Human;
}

int32 AFAPlayerState::GetMainSideScore() const
{
	return FMath::Max(MatchScore.GetHumanTotal(), MatchScore.GetZombieTotal());
}

float AFAPlayerState::GetMainSideRatio() const
{
	const int32 Total = MatchScore.GetTotal();
	if (Total <= 0)
	{
		return 0.0f;
	}

	return static_cast<float>(GetMainSideScore()) / static_cast<float>(Total);
}

void AFAPlayerState::SetTeam(EFATeam NewTeam)
{
	if (!ensure(HasAuthority()) || Team == NewTeam)
	{
		return;
	}

	Team = NewTeam;

	// 진영은 GAS 쪽에서도 태그(State.Human / State.Zombie)로 표현.
	if (AFACharacter* FACharacter = Cast<AFACharacter>(GetPawn()))
	{
		FACharacter->SetTeamStateTag(NewTeam);
	}

	OnRep_Team();
}

void AFAPlayerState::ResetForNewMatch()
{
	if (!ensure(HasAuthority()))
	{
		return;
	}

	MatchScore.Reset();
	RoundScore.Reset();
	SurvivalClockStart = -1.0f;
	Team = EFATeam::None;
	DeathMatchPlacement = 0;

	OnRep_Team();
	OnRep_RoundScore();
	OnRep_MatchScore();
	OnRep_DeathMatchPlacement();
}

void AFAPlayerState::ResetForNewRound()
{
	if (!ensure(HasAuthority()))
	{
		return;
	}

	RoundScore.Reset();
	SurvivalClockStart = -1.0f;

	OnRep_RoundScore();
}

void AFAPlayerState::StartSurvivalClock(float ServerTimeNow)
{
	if (!ensure(HasAuthority()))
	{
		return;
	}

	SurvivalClockStart = ServerTimeNow;
}

void AFAPlayerState::FinalizeSurvivalTime(float ServerTimeNow)
{
	if (!ensure(HasAuthority()) || SurvivalClockStart < 0.0f)
	{
		return;
	}

	RoundScore.SurvivalTime += FMath::Max(0.0f, ServerTimeNow - SurvivalClockStart);
	SurvivalClockStart = -1.0f;

	OnRep_RoundScore();
}

void AFAPlayerState::MarkInfected(float ServerTimeNow)
{
	if (!ensure(HasAuthority()))
	{
		return;
	}

	FinalizeSurvivalTime(ServerTimeNow);

	// 인간 팀 점수 지급 대상에서 제외. 이미 받은 개인 점수는 유지 (GDD 5.2).
	RoundScore.bWasInfectedThisRound = true;
	RoundScore.HumanTeam = 0;

	OnRep_RoundScore();

	SetTeam(EFATeam::Zombie);
}

void AFAPlayerState::AddHumanTeamScore(int32 Amount)
{
	if (!ensure(HasAuthority()) || Amount == 0)
	{
		return;
	}

	RoundScore.HumanTeam += Amount;
	OnRep_RoundScore();
}

void AFAPlayerState::AddHumanPersonalScore(int32 Amount)
{
	if (!ensure(HasAuthority()) || Amount == 0)
	{
		return;
	}

	RoundScore.HumanPersonal += Amount;
	OnRep_RoundScore();
}

void AFAPlayerState::AddZombieTeamScore(int32 Amount)
{
	if (!ensure(HasAuthority()) || Amount == 0)
	{
		return;
	}

	RoundScore.ZombieTeam += Amount;
	OnRep_RoundScore();
}

void AFAPlayerState::AddZombiePersonalScore(int32 Amount)
{
	if (!ensure(HasAuthority()) || Amount == 0)
	{
		return;
	}

	RoundScore.ZombiePersonal += Amount;
	OnRep_RoundScore();
}

void AFAPlayerState::CommitRoundScore()
{
	if (!ensure(HasAuthority()))
	{
		return;
	}

	MatchScore.Add(RoundScore);
	RoundScore.Reset();

	OnRep_RoundScore();
	OnRep_MatchScore();
}

void AFAPlayerState::SetDeathMatchPlacement(int32 InPlacement)
{
	if (!ensure(HasAuthority()) || DeathMatchPlacement == InPlacement)
	{
		return;
	}

	DeathMatchPlacement = InPlacement;

	OnRep_DeathMatchPlacement();
}

void AFAPlayerState::OnRep_Team()
{
	OnTeamChanged.Broadcast(Team);
}

void AFAPlayerState::OnRep_RoundScore()
{
	OnScoreChanged.Broadcast();
}

void AFAPlayerState::OnRep_MatchScore()
{
	OnScoreChanged.Broadcast();
}

void AFAPlayerState::OnRep_DeathMatchPlacement()
{
	// 순위가 바뀌는 값이라 순위표를 다시 그리게 해야 한다.
	OnScoreChanged.Broadcast();
}

void AFAPlayerState::Client_ShowDraftUI_Implementation(const TArray<FName>& CandidateRuleIds, float SelectionTime)
{
	UE_LOG(LogTemp, Log, TEXT("[Draft] Client_ShowDraftUI 수신 — 후보 %d장 (제한 %.0f초)"),
		CandidateRuleIds.Num(), SelectionTime);

	for (const FName& RuleId : CandidateRuleIds)
	{
		UE_LOG(LogTemp, Log, TEXT("[Draft]   클라이언트가 받은 후보 - %s"), *RuleId.ToString());
	}

	// 리슨 서버에서는 APlayerState::GetPlayerController() 가 로컬 플레이어가 아닌 PS 에도 답하므로
	// 반드시 이 PS 의 주인 컨트롤러로만 UI 를 연다.
	APlayerController* OwningController = GetPlayerController();
	if (!OwningController || !OwningController->IsLocalController())
	{
		return;
	}

	if (AFAHUD* FAHUD = Cast<AFAHUD>(OwningController->GetHUD()))
	{
		FAHUD->ShowDraftUI(CandidateRuleIds, SelectionTime);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Draft] HUD 가 AFAHUD 가 아니라 드래프트 UI 를 열지 못했다 — GameMode 의 HUD Class 를 확인할 것"));
	}
}

void AFAPlayerState::Server_ConfirmDraft_Implementation(int32 SelectedIndex)
{
	if (UFARuleDraftSubsystem* Draft = GetWorld()->GetSubsystem<UFARuleDraftSubsystem>())
	{
		Draft->ConfirmDraft(this, SelectedIndex);
	}
}

bool AFAPlayerState::Server_ConfirmDraft_Validate(int32 SelectedIndex)
{
	// 후보는 항상 3장(0~2)이므로, 이 범위를 벗어나면 애초에 조작된 요청으로 간주해 거부.
	return SelectedIndex >= 0 && SelectedIndex < 3;
}