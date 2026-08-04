// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/FAPlayerState.h"

#include "Net/UnrealNetwork.h"
#include "Rule/FARuleDraftSubsystem.h"

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

	OnRep_Team();
	OnRep_RoundScore();
	OnRep_MatchScore();
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

void AFAPlayerState::Client_ShowDraftUI_Implementation(const TArray<FName>& CandidateRuleIds)
{
	UE_LOG(LogTemp, Log, TEXT("[Draft] Client_ShowDraftUI 수신 — 후보 %d장"), CandidateRuleIds.Num());

	for (const FName& RuleId : CandidateRuleIds)
	{
		UE_LOG(LogTemp, Log, TEXT("[Draft]   클라이언트가 받은 후보 - %s"), *RuleId.ToString());
	}

	// TODO: 실제 드래프트 UI 위젯을 여는 코드로 교체
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