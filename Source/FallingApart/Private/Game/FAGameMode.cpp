// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/FAGameMode.h"

#include "EngineUtils.h"
#include "GAS/FAAbilitySystemComponent.h"
#include "GAS/FAGameplayTags.h"
#include "Game/FAGameState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Network/FAOnlineSessionSubsystem.h"
#include "Player/FAPlayerState.h"
#include "Player/FAPlayerController.h"
#include "Player/FACharacter.h"
#include "Player/FASpectatorPawn.h"
#include "Voice/FAVoiceComponent.h"
#include "Rule/FARuleDraftSubsystem.h"
#include "Rule/FADeathMatchSubsystem.h"
#include "UI/FAHUD.h"

AFAGameMode::AFAGameMode()
{
	GameStateClass = AFAGameState::StaticClass();
	PlayerControllerClass = AFAPlayerController::StaticClass();
	PlayerStateClass = AFAPlayerState::StaticClass();
	// BP_FAGameMode 에서 BP_FAHUD 로 덮어쓴다 — WBP 지정이 BP 쪽에 있기 때문.
	HUDClass = AFAHUD::StaticClass();
	DefaultPawnClass = AFACharacter::StaticClass();

	// 관전자 보이스 붙는 폰
	SpectatorClass = AFASpectatorPawn::StaticClass();

	// 인원 조건은 ReadyToStartMatch() 에서 직접 판정
	bDelayedStart = true;

	PrimaryActorTick.bCanEverTick = false;
}

void AFAGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (AFAGameState* FAGameState = GetFAGameState())
	{
		FAGameState->SetMaxRound(MaxRound);
		FAGameState->SetRoundPhase(EFARoundPhase::WaitingToStart);
	}
	
	// 매치 종료 및 세션 로비상태로 복구 후 돌아가는 이벤트 미리 등록.
	if (UGameInstance* GameInstance = GetGameInstance())
    {
     	if (UFAOnlineSessionSubsystem* SessionSubsystem = GameInstance->GetSubsystem<UFAOnlineSessionSubsystem>())
     	{
     		SessionReopenForlobbyDelegateHandle = SessionSubsystem->OnSessionReopenedForLobby.AddUObject(
     			this, 
     			&AFAGameMode::HandleSessionReopenForLobby);
     	}
    }
}

void AFAGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ReturnToLobbyTimerHandle);
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFAOnlineSessionSubsystem* SessionSubsystem = GameInstance->GetSubsystem<UFAOnlineSessionSubsystem>())
		{
			SessionSubsystem->OnSessionReopenedForLobby.Remove(SessionReopenForlobbyDelegateHandle);
		}
	}
	
	
	Super::EndPlay(EndPlayReason);
}

bool AFAGameMode::ReadyToStartMatch_Implementation()
{
	if (GetMatchState() != MatchState::WaitingToStart)
	{
		return false;
	}
	//로비에서 넘어온 경우
	if (bHasLobbyMatchContext)
	{
		return NumPlayers>= ExpectedPlayersToStart;
	}
	else
	{
		return (NumPlayers + NumBots) >= FMath::Max(1, MinPlayersToStart);
	}

	
}

void AFAGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();

	if (AFAGameState* FAGameState = GetFAGameState())
	{
		FAGameState->SetCurrentRound(0);
	}

	// 지난 매치의 데스매치 참가자가 남아 있으면 관전자 판정이 어긋남.
	DeathMatchContenders.Reset();

	// 새 매치의 1라운드는 전원이 시작 좀비 후보다.
	LastStartingZombies.Reset();

	// 매치 시작 점수 비움.
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (AFAPlayerState* FAPlayerState = Cast<AFAPlayerState>(PlayerState))
		{
			FAPlayerState->ResetForNewMatch();
		}
	}

	StartRound();
}

void AFAGameMode::StartRound()
{
	AFAGameState* FAGameState = GetFAGameState();
	if (!FAGameState)
	{
		return;
	}

	ClearRoundTimers();

	// 지난 라운드 결과 화면이 떠 있으면 걷는다.
	HideRoundResult();

	FAGameState->SetCurrentRound(FAGameState->GetCurrentRound() + 1);
	FAGameState->SetLastRoundResult(EFARoundResult::None);
	FAGameState->SetRoundPhase(EFARoundPhase::RoundStarting);

	AssignStartingTeams();
	RestartAllPlayers();

	// 라운드 사이에 관전 전환·난입 문제로 보이스 격리를 다시 맞춤.
	UpdateVoiceMuteMatrix();

	UE_LOG(LogTemp, Log, TEXT("[FA] Round %d/%d starting"), FAGameState->GetCurrentRound(), FAGameState->GetMaxRound());

	GetWorldTimerManager().SetTimer(RoundStartTimerHandle, this, &AFAGameMode::BeginRoundPlay,
		FMath::Max(0.1f, RoundStartCountdown), false);
}

void AFAGameMode::BeginRoundPlay()
{
	AFAGameState* FAGameState = GetFAGameState();
	if (!FAGameState)
	{
		return;
	}

	const float Now = GetServerTime();
	FAGameState->StartRoundTimer(Now, RoundDuration, DraftInterval, DraftsPerRound);
	FAGameState->SetRoundPhase(EFARoundPhase::InProgress);

	// 생존 시간 계측은 실제 라운드 시작 시각을 기준
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		AFAPlayerState* FAPlayerState = Cast<AFAPlayerState>(PlayerState);
		if (FAPlayerState && FAPlayerState->IsHuman())
		{
			FAPlayerState->StartSurvivalClock(Now);
		}
	}

	GetWorldTimerManager().SetTimer(RoundTimerHandle, this, &AFAGameMode::OnRoundTimeExpired, RoundDuration, false);

	if (DraftsPerRound > 0)
	{
		// 첫 드래프트는 0:30, 이후 30초마다 반복.
		GetWorldTimerManager().SetTimer(DraftTimerHandle, this, &AFAGameMode::OnDraftTimer, DraftInterval, true, DraftInterval);
	}

	// 생존 틱도 같은 주기로 돈다. 감염되면 IsHuman() 이 false 가 되므로 틱 쪽에 해제 처리가 따로 필요 없다.
	if (SurvivalTickInterval > 0.0f && SurvivalTickScore != 0)
	{
		GetWorldTimerManager().SetTimer(SurvivalTickTimerHandle, this, &AFAGameMode::OnSurvivalTick,
			SurvivalTickInterval, true, SurvivalTickInterval);
	}
}

void AFAGameMode::OnSurvivalTick()
{
	const AFAGameState* FAGameState = GetFAGameState();
	if (!FAGameState || !FAGameState->IsRoundInProgress())
	{
		return;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		AFAPlayerState* FAPlayerState = Cast<AFAPlayerState>(PlayerState);
		if (FAPlayerState && FAPlayerState->IsHuman())
		{
			FAPlayerState->AddHumanPersonalScore(SurvivalTickScore);
		}
	}
}

void AFAGameMode::OnRoundTimeExpired()
{
	// 제한 시간까지 인간이 남아 있으면 인간 승 (GDD 2.3).
	EndRound(EFARoundResult::HumanWin);
}

void AFAGameMode::OnDraftTimer()
{
	AFAGameState* FAGameState = GetFAGameState();
	if (!FAGameState || !FAGameState->IsRoundInProgress())
	{
		return;
	}

	const int32 DraftIndex = FAGameState->GetDraftsCompleted() + 1;
	FAGameState->SetDraftsCompleted(DraftIndex);
	FAGameState->NotifyDraftRoundStarted(DraftIndex);

	if (DraftIndex >= DraftsPerRound)
	{
		// 6회차(3:00, 무법 라운드)가 마지막
		GetWorldTimerManager().ClearTimer(DraftTimerHandle);
	}

	if (UFARuleDraftSubsystem* Draft = GetWorld()->GetSubsystem<UFARuleDraftSubsystem>())
	{
		Draft->StartDraftForAllPlayers(DraftIndex);
	}
}

void AFAGameMode::EndRound(EFARoundResult Result)
{
	AFAGameState* FAGameState = GetFAGameState();
	if (!FAGameState || !FAGameState->IsRoundInProgress() || Result == EFARoundResult::None)
	{
		return;
	}

	ClearRoundTimers();

	// 선택 중이던 드래프트를 여기서 끊는다. 안 끊으면 타임아웃 타이머가 살아남아
	// 다음 라운드에서 룰이 붙고, 클라이언트 선택창도 라운드를 넘어 계속 떠 있는다.
	if (UFARuleDraftSubsystem* Draft = GetWorld()->GetSubsystem<UFARuleDraftSubsystem>())
	{
		Draft->CancelAllPendingDrafts();
	}

	SettleRoundScore(Result);

	FAGameState->SetRoundPhase(EFARoundPhase::RoundEnd);
	FAGameState->SetLastRoundResult(Result);

	UE_LOG(LogTemp, Log, TEXT("[FA] Round %d ended (%s)"), FAGameState->GetCurrentRound(),
		Result == EFARoundResult::HumanWin ? TEXT("HumanWin") : TEXT("ZombieWin"));

	// 결과 화면은 다음 라운드가 시작되거나(StartRound) 최종 순위표가 뜰 때 내려간다.
	ShowRoundResult();

	GetWorldTimerManager().SetTimer(RoundEndTimerHandle, this, &AFAGameMode::StartNextRoundOrEndMatch,
		FMath::Max(0.1f, RoundEndDelay), false);
}

void AFAGameMode::SettleRoundScore(EFARoundResult Result)
{
	const float Now = GetServerTime();

	TArray<AFAPlayerState*> AllPlayerStates;
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (AFAPlayerState* FAPlayerState = Cast<AFAPlayerState>(PlayerState))
		{
			AllPlayerStates.Add(FAPlayerState);
		}
	}

	// 라운드 종료까지 인간으로 남은 플레이어의 생존 시간을 확정.
	for (AFAPlayerState* FAPlayerState : AllPlayerStates)
	{
		if (FAPlayerState->IsHuman())
		{
			FAPlayerState->FinalizeSurvivalTime(Now);
		}
	}

	if (Result == EFARoundResult::HumanWin)
	{
		// 라운드 완주 — 감염 이력 없이 끝까지 인간으로 남은 사람에게 고정 지급.
		for (AFAPlayerState* FAPlayerState : AllPlayerStates)
		{
			if (FAPlayerState->IsHuman() && !FAPlayerState->WasInfectedThisRound())
			{
				FAPlayerState->AddHumanTeamScore(RoundCompleteScore);
			}
		}
	}
	else if (Result == EFARoundResult::ZombieWin)
	{
		// 전멸 보너스는 남은 시간에 비례한다 — 올림이라 종료 직전 전멸도 최소 1점은 남는다.
		// EndRound 는 페이즈를 RoundEnd 로 바꾸기 '전에' 여기를 부르므로 아직 남은 시간을 읽을 수 있다.
		const AFAGameState* FAGameState = GetFAGameState();
		const float RemainingRatio = FAGameState
			? FMath::Clamp(FAGameState->GetRoundTimeRemaining() / FMath::Max(RoundDuration, 1.0f), 0.0f, 1.0f)
			: 0.0f;

		const int32 WipeOutAward = FMath::Max(1, FMath::CeilToInt(WipeOutScore * RemainingRatio));

		for (AFAPlayerState* FAPlayerState : AllPlayerStates)
		{
			if (FAPlayerState->IsZombie())
			{
				FAPlayerState->AddZombieTeamScore(WipeOutAward);
			}
		}
	}

	// 최장 생존 보너스는 생존 틱이 대신한다. 예전 방식은 생존자 전원의 SurvivalTime 이 같은 Now 로
	// 확정돼 소수점까지 동률이라, 사실상 '생존자 전원 +2' 로 동작하고 있었다.
	// SurvivalTime 계측 자체는 최종 순위 타이브레이크 3차 기준이라 그대로 남는다.

	// 결과 화면용 스냅샷은 반드시 커밋 '전에' 떠야 한다 — 커밋이 라운드 점수를 0 으로 지운다.
	BuildRoundResultSnapshot(Result, AllPlayerStates);

	// 정산이 끝난 라운드 점수를 매치 누적으로 넘긴다.
	for (AFAPlayerState* FAPlayerState : AllPlayerStates)
	{
		FAPlayerState->CommitRoundScore();
	}
}

void AFAGameMode::BuildRoundResultSnapshot(EFARoundResult Result, const TArray<AFAPlayerState*>& AllPlayerStates)
{
	AFAGameState* FAGameState = GetFAGameState();
	if (!FAGameState)
	{
		return;
	}

	FFARoundResultSnapshot Snapshot;
	Snapshot.RoundIndex = FAGameState->GetCurrentRound();
	Snapshot.Result = Result;
	Snapshot.Rows.Reserve(AllPlayerStates.Num());

	for (const AFAPlayerState* FAPlayerState : AllPlayerStates)
	{
		if (!FAPlayerState)
		{
			continue;
		}

		FFARoundResultRow Row;
		Row.PlayerName = FAPlayerState->GetPlayerName();
		Row.PlayerState = const_cast<AFAPlayerState*>(FAPlayerState);
		Row.Score = FAPlayerState->GetRoundScore();
		Row.SurvivalTime = FAPlayerState->GetRoundScore().SurvivalTime;
		Row.bSurvived = FAPlayerState->IsHuman() && !FAPlayerState->WasInfectedThisRound();
		Row.TeamAtRoundEnd = FAPlayerState->GetTeam();

		Snapshot.Rows.Add(Row);
	}

	// GDD 8.4 는 '생존 시간 순위' 다. 동률이면 이번 라운드 점수로 가른다.
	Snapshot.Rows.Sort([](const FFARoundResultRow& A, const FFARoundResultRow& B)
		{
			if (!FMath::IsNearlyEqual(A.SurvivalTime, B.SurvivalTime, 0.01f))
			{
				return A.SurvivalTime > B.SurvivalTime;
			}
			return A.Score.GetTotal() > B.Score.GetTotal();
		});

	FAGameState->SetRoundResultSnapshot(Snapshot);
}

void AFAGameMode::ShowRoundResult()
{
	// PIE 를 끄거나 레벨을 옮기면 플레이어가 빠지면서 '전멸' 로 라운드가 끝난다.
	// 그 흐름에서 위젯을 만들면 UMG 가 ensure 로 잡는다 (UserWidget.cpp 의 !bIsTearingDown).
	const UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown)
	{
		return;
	}

	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		if (AFAPlayerController* FAPlayerController = Cast<AFAPlayerController>(Iterator->Get()))
		{
			FAPlayerController->Client_ShowRoundResult();
		}
	}
}

void AFAGameMode::HideRoundResult()
{
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		if (AFAPlayerController* FAPlayerController = Cast<AFAPlayerController>(Iterator->Get()))
		{
			FAPlayerController->Client_HideRoundResult();
		}
	}
}

void AFAGameMode::StartNextRoundOrEndMatch()
{
	const AFAGameState* FAGameState = GetFAGameState();
	if (!FAGameState)
	{
		return;
	}

	if (FAGameState->GetCurrentRound() >= FAGameState->GetMaxRound())
	{
		EndMatch();
	}
	else
	{
		StartRound();
	}
}

void AFAGameMode::HandleMatchHasEnded()
{
	Super::HandleMatchHasEnded();

	ClearRoundTimers();

	AFAGameState* FAGameState = GetFAGameState();
	if (!FAGameState)
	{
		return;
	}

	FAGameState->SetRoundPhase(EFARoundPhase::MatchEnd);

	TArray<AFAPlayerState*> Ranked;
	FAGameState->GetRankedPlayerStates(Ranked);

	// 1등이 타이브레이크 3차까지 완전 동점이면 데스매치 대상 (GDD 5.4 / 5.5).
	int32 TiedFirstPlaceCount = 0;
	if (Ranked.Num() > 0)
	{
		TiedFirstPlaceCount = 1;
		for (int32 Index = 1; Index < Ranked.Num(); ++Index)
		{
			if (!AFAGameState::AreFullyTied(*Ranked[0], *Ranked[Index]))
			{
				break;
			}
			++TiedFirstPlaceCount;
		}
	}

	if (TiedFirstPlaceCount >= 2 && TiedFirstPlaceCount <= 3)
	{
		UE_LOG(LogTemp, Log, TEXT("[FA] %d-way tie for 1st — 데스매치 진입"), TiedFirstPlaceCount);

		// SetRoundPhase 를 먼저 바꾸고 SetDeathMatchContenders 를 호출해야 관전자 판정이 맞는다.
		FAGameState->SetRoundPhase(EFARoundPhase::DeathMatch);
		TArray<AFAPlayerState*> Contenders(Ranked.GetData(), TiedFirstPlaceCount);
		SetDeathMatchContenders(Contenders);   // 나머지는 여기서 관전자로 전환되고 보이스도 분리된다

		if (UFADeathMatchSubsystem* DeathMatch = GetWorld()->GetSubsystem<UFADeathMatchSubsystem>())
		{
			// 참가자가 2명이 아니는 등 데스매치를 열 수 없으면 이 안에서 FinishMatchAfterDeathMatch() 로 되돌린다.
			DeathMatch->StartDeathMatch(Contenders);
		}
		else
		{
			FinishMatchAfterDeathMatch();
		}

		return;
	}

	ShowFinalRanking();
}

void AFAGameMode::FinishMatchAfterDeathMatch()
{
	AFAGameState* FAGameState = GetFAGameState();
	if (!FAGameState)
	{
		return;
	}

	// 페이즈를 먼저 되돌려야 IsVoiceSpectator() 가 전원 비관전으로 답한다.
	FAGameState->SetRoundPhase(EFARoundPhase::MatchEnd);

	// 관전 해제 + 보이스 원복. 빈 배열이라 전원이 다시 근접 채널로 돌아온다.
	SetDeathMatchContenders(TArray<AFAPlayerState*>());

	ShowFinalRanking();
}

void AFAGameMode::ShowFinalRanking()
{
	const AFAGameState* FAGameState = GetFAGameState();
	if (!FAGameState)
	{
		return;
	}

	// 최종 순위표가 라운드 결과 화면 위에 겹치면 안 된다.
	HideRoundResult();

	TArray<AFAPlayerState*> Ranked;
	FAGameState->GetRankedPlayerStates(Ranked);

	for (int32 Index = 0; Index < Ranked.Num(); ++Index)
	{
		UE_LOG(LogTemp, Log, TEXT("[FA] Rank %d: %s (%d pts)"), Index + 1, *Ranked[Index]->GetPlayerName(),
			Ranked[Index]->GetMatchTotalScore());
	}

	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		if (AFAPlayerController* FAPlayerController = Cast<AFAPlayerController>(Iterator->Get()))
		{
			FAPlayerController->Client_ShowMatchResult();
		}
	}
	
	if (!bReturningToLobby && !GetWorldTimerManager().IsTimerActive(ReturnToLobbyTimerHandle))
	{
		GetWorldTimerManager().SetTimer(
			ReturnToLobbyTimerHandle,
			this, 
			&ThisClass::BeginReturnToLobby,
			ReturnToLobbyDelaySeconds,
			false
		);
	}
}

void AFAGameMode::HandleInfection(AFAPlayerState* Victim, AFAPlayerState* Infector)
{
	AFAGameState* FAGameState = GetFAGameState();
	if (!FAGameState || !Victim)
	{
		return;
	}

	// 데스매치 중 포획은 점수 정산·연속 드래프트 없이 승패 판정만 한다 — 일반 감염과 완전히 다른 경로.
	if (FAGameState->GetRoundPhase() == EFARoundPhase::DeathMatch)
	{
		if (UFADeathMatchSubsystem* DeathMatch = GetWorld()->GetSubsystem<UFADeathMatchSubsystem>())
		{
			DeathMatch->NotifyContenderCaught(Victim);
		}
		return;
	}

	if (!FAGameState->IsRoundInProgress())
	{
		return;
	}

	if (!Victim->IsHuman())
	{
		return;
	}

	// 감염 성공 — 개인 점수 (시작 좀비 · 전환 좀비 동일)
	if (Infector && Infector != Victim && Infector->IsZombie())
	{
		Infector->AddZombiePersonalScore(InfectionScore);
	}

	// 생존 시간 확정 → 인간 팀 점수 자격 상실 → 좀비로 전환
	Victim->MarkInfected(GetServerTime());

	UE_LOG(LogTemp, Log, TEXT("[FA] %s infected by %s"), *Victim->GetPlayerName(),
		Infector ? *Infector->GetPlayerName() : TEXT("None"));

	if (AFACharacter* VictimCharacter = Cast<AFACharacter>(Victim->GetPawn()))
	{
		int32 RemovedRuleCount = 0;

		if (UFAAbilitySystemComponent* ASC = VictimCharacter->GetFAAbilitySystemComponent())
		{
			// 1) 인간 룰 전부 제거 — 제거된 장수 N 이 곧 연속 드래프트 횟수
			RemovedRuleCount = ASC->RemoveRulesOfTeam(FAGameplayTags::Get().Rule_Team_Human);
		}

		// 2) 감염 직후 2초 무적
		VictimCharacter->ApplyInvulnerability(InfectionInvulnerabilityTime);

		// 3) 제거된 장수만큼 좀비 카드 연속 드래프트 (1장당 6초)
		if (UFARuleDraftSubsystem* Draft = GetWorld()->GetSubsystem<UFARuleDraftSubsystem>())
		{
			Draft->StartChainDraft(Victim, RemovedRuleCount);
		}

		// TODO: 암전 · 시야 필터 전환 연출(PlayInfectionSequence)은 캐릭터 담당자 작업 후 연결.
	}

	UpdateTeamCounts();
}

void AFAGameMode::HandleRescueSuccess(AFAPlayerState* Rescuer, AFAPlayerState* Victim)
{
	const AFAGameState* FAGameState = GetFAGameState();
	if (!FAGameState || !FAGameState->IsRoundInProgress())
	{
		return;
	}

	// 구출 성공 — 개인 점수. (GDD 11.3).
	if (Rescuer && Rescuer->IsHuman())
	{
		Rescuer->AddHumanPersonalScore(RescueScore);
	}

	UE_LOG(LogTemp, Log, TEXT("[FA] %s rescued %s"), Rescuer ? *Rescuer->GetPlayerName() : TEXT("None"),
		Victim ? *Victim->GetPlayerName() : TEXT("None"));

	// 포획 해제 · 좀비 경직 2초(Z30 보유 시 기절 8초)는 GA_RescueContest 가 처리 (기술문서 3.3).
}

int32 AFAGameMode::GetStartingZombieCount(int32 InPlayerCount) const
{
	if (StartingZombieCountOverride > 0)
	{
		// 방장 지정값 — 인원의 절반을 넘지 않게
		return FMath::Clamp(StartingZombieCountOverride, 1, FMath::Max(1, FMath::Min(MaxStartingZombies, InPlayerCount - 1)));
	}

	if (InPlayerCount < 2)
	{
		// 단독 테스트 — 좀비 없이 맵과 타이머만 확인하는 상황
		return 0;
	}

	return (InPlayerCount >= PlayerCountForTwoZombies) ? 2 : 1;
}

void AFAGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	// 다른 액터가 초기화되기 전에 가장 먼저 실행되는 Gamemode 초기화 가상함수. 규칙설정에 적절함.
	Super::InitGame(MapName, Options, ErrorMessage);
	
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	UFAOnlineSessionSubsystem* SessionSubsystem =
		GameInstance->GetSubsystem<UFAOnlineSessionSubsystem>();

	if (!SessionSubsystem)
	{
		return;
	}

	FFAPendingMatchContext MatchContext;
	if (!SessionSubsystem->GetPendingMatchContext(MatchContext))
	{
		return;
	}

	ExpectedPlayersToStart =
		FMath::Max(1, MatchContext.ExpectedPlayerCount);

	bHasLobbyMatchContext = true;
}

void AFAGameMode::AssignStartingTeams()
{
	TArray<AFAPlayerState*> Candidates;
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		AFAPlayerState* FAPlayerState = Cast<AFAPlayerState>(PlayerState);
		if (!FAPlayerState || FAPlayerState->IsOnlyASpectator())
		{
			continue;
		}

		FAPlayerState->ResetForNewRound();
		// 여기는 '시작 좀비를 뽑기 전의 기본 진영' 이다 — Zombie 로 두면 아래에서 누굴 뽑든
		// 전원 좀비인 라운드가 되어 인간이 한 명도 없다.
		FAPlayerState->SetTeam(EFATeam::Human);
		Candidates.Add(FAPlayerState);
	}

	const int32 ZombieCount = GetStartingZombieCount(Candidates.Num());

	// PIE 는 마지막 클라이언트의 PostLogin 이 매치 시작보다 늦게 끝나는 경우가 있어,
	// 여기 후보 수가 실제 접속 인원보다 적게 잡히면 나머지가 전부 '난입 = 좀비' 로 들어온다.
	// 진영이 이상하면 이 줄부터 볼 것
	UE_LOG(LogTemp, Log,
		TEXT("[FA] 진영 배정 — 후보 %d명 / 시작 좀비 %d명 (NumPlayers=%d, MinPlayersToStart=%d, Override=%d, 로비컨텍스트=%s)"),
		Candidates.Num(), ZombieCount, NumPlayers, MinPlayersToStart, StartingZombieCountOverride,
		bHasLobbyMatchContext ? TEXT("있음") : TEXT("없음"));

	// 직전 라운드의 시작 좀비는 후보에서 뺀다. 빼고 나서 인원이 모자랄 때만 전원 풀로 되돌린다
	// (3인 테스트처럼 인원이 적으면 연속 배정이 불가피하다).
	TArray<AFAPlayerState*> Preferred = Candidates.FilterByPredicate(
		[this](const AFAPlayerState* Candidate)
		{
			return !LastStartingZombies.ContainsByPredicate(
				[Candidate](const TWeakObjectPtr<AFAPlayerState>& Previous)
				{
					return Previous.Get() == Candidate;
				});
		});

	TArray<AFAPlayerState*>& Pool = (Preferred.Num() >= ZombieCount) ? Preferred : Candidates;

	LastStartingZombies.Reset();

	// 시작 좀비는 서버 RNG 로 무작위 선정
	for (int32 Index = 0; Index < ZombieCount && Pool.Num() > 0; ++Index)
	{
		const int32 PickedIndex = FMath::RandRange(0, Pool.Num() - 1);
		AFAPlayerState* Picked = Pool[PickedIndex];
		Pool.RemoveAtSwap(PickedIndex);

		Picked->SetTeam(EFATeam::Zombie);

		// 수당은 배정 직후에 붙인다 — 위에서 ResetForNewRound() 로 라운드 점수를 이미 비웠으므로
		// 이 시점에 더하면 라운드 시작과 동시에 HUD 에 찍힌다.
		Picked->AddZombiePersonalScore(StartingZombieBonus);

		LastStartingZombies.Add(Picked);
	}

	UpdateTeamCounts(false);
}

void AFAGameMode::RestartAllPlayers()
{
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PlayerController = Cast<APlayerController>(Iterator->Get());
		if (!PlayerController || !PlayerController->PlayerState || MustSpectate(PlayerController))
		{
			continue;
		}

		// 라운드마다 진영에 맞는 스폰 지점에서 새로 시작
		if (APawn* OldPawn = PlayerController->GetPawn())
		{
			PlayerController->UnPossess();
			OldPawn->Destroy();
		}

		RestartPlayer(PlayerController);
	}
}

void AFAGameMode::UpdateTeamCounts(bool bCheckWipeOut)
{
	AFAGameState* FAGameState = GetFAGameState();
	if (!FAGameState)
	{
		return;
	}

	int32 HumanCount = 0;
	int32 ZombieCount = 0;

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		const AFAPlayerState* FAPlayerState = Cast<AFAPlayerState>(PlayerState);
		if (!FAPlayerState)
		{
			continue;
		}

		if (FAPlayerState->IsHuman())
		{
			++HumanCount;
		}
		else if (FAPlayerState->IsZombie())
		{
			++ZombieCount;
		}
	}

	FAGameState->SetTeamCounts(HumanCount, ZombieCount);

	if (bCheckWipeOut && HumanCount <= 0 && ZombieCount > 0 && FAGameState->IsRoundInProgress())
	{
		EndRound(EFARoundResult::ZombieWin);
	}
}

void AFAGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	AFAGameState* FAGameState = GetFAGameState();
	if (!FAGameState || !NewPlayer)
	{
		return;
	}
	// 라운드 중(카운트다운 포함) 난입은 좀비로 참전.
	// RoundStarting 단계는 AssignStartingTeams() 가 이미 실행된 뒤라, 여기서 놓치면 Team=None으로 남는다.
	// TODO: 이탈 / 난입 처리 규칙은 기획 미확정 (GDD 부록 C)
	const EFARoundPhase Phase = FAGameState->GetRoundPhase();
	if (Phase == EFARoundPhase::InProgress || Phase == EFARoundPhase::RoundStarting)
	{
		if (AFAPlayerState* FAPlayerState = Cast<AFAPlayerState>(NewPlayer->PlayerState))
		{
			if (FAPlayerState->GetTeam() == EFATeam::None)
			{
				FAPlayerState->ResetForNewRound();
				FAPlayerState->SetTeam(EFATeam::Zombie);
			}
		}
		UpdateTeamCounts(false);
	}
}

void AFAGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
	Super::HandleSeamlessTravelPlayer(C);
	APlayerController* PlayerController = Cast<APlayerController>(C);
	APlayerState* TravelledPlayerState = PlayerController ? PlayerController->PlayerState : nullptr;
	if (!TravelledPlayerState || !TravelledPlayerState->GetUniqueId().IsValid())
	{
		return;
	}

	TravelledPlayerState->RegisterPlayerWithSession(false);

	UE_LOG(LogFAOSS, Log, TEXT("[FA] 심리스 트래블 후 세션 재등록: %s"), *TravelledPlayerState->GetPlayerName());
}

void AFAGameMode::Logout(AController* Exiting)
{
	AFAPlayerState* FAPlayerState = Exiting ? Cast<AFAPlayerState>(Exiting->PlayerState) : nullptr;
	if (FAPlayerState)
	{
		// 나간 플레이어는 어느 진영에도 넣지 않음. 남은 인간이 0명이 되면 라운드가 끝남.
		FAPlayerState->FinalizeSurvivalTime(GetServerTime());
		FAPlayerState->SetTeam(EFATeam::None);
	}

	Super::Logout(Exiting);

	UpdateTeamCounts();
	UpdateVoiceMuteMatrix();
}

void AFAGameMode::UpdateGameplayMuteList(APlayerController* aPlayer)
{
	Super::UpdateGameplayMuteList(aPlayer);

	UpdateVoiceMuteMatrix();
}

void AFAGameMode::SetDeathMatchContenders(const TArray<AFAPlayerState*>& Contenders)
{
	DeathMatchContenders.Reset(Contenders.Num());
	for (AFAPlayerState* Contender : Contenders)
	{
		DeathMatchContenders.Add(Contender);
	}

	UWorld* World = GetWorld();
	if (World)
	{
		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			APlayerController* PlayerController = Iterator->Get();
			if (!PlayerController)
			{
				continue;
			}

			APawn* Pawn = PlayerController->GetPawn();
			UFAVoiceComponent* VoiceComponent = Pawn ? Pawn->FindComponentByClass<UFAVoiceComponent>() : nullptr;
			if (VoiceComponent)
			{
				VoiceComponent->SetVoiceChannel(IsVoiceSpectator(PlayerController)
					? EFAVoiceChannel::Global
					: EFAVoiceChannel::Proximity);
			}
		}
	}

	UpdateVoiceMuteMatrix();
}

bool AFAGameMode::IsVoiceSpectator(APlayerController* PlayerController) const
{
	if (!PlayerController)
	{
		return false;
	}

	// 개발 중 테스트용 관전 전용 접속(?SpectatorOnly=1)
	if (MustSpectate(PlayerController))
	{
		return true;
	}

	const AFAGameState* FAGameState = GetFAGameState();
	if (!FAGameState || FAGameState->GetRoundPhase() != EFARoundPhase::DeathMatch)
	{
		return false;
	}

	AFAPlayerState* FAPlayerState = Cast<AFAPlayerState>(PlayerController->PlayerState);
	return FAPlayerState != nullptr && !DeathMatchContenders.Contains(FAPlayerState);
}

void AFAGameMode::UpdateVoiceMuteMatrix()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<APlayerController*> Participants;
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PlayerController = Iterator->Get();

		// UniqueNetId 가 없으면 뮤트 대상으로 지목할 수단이 없음.
		if (PlayerController && PlayerController->PlayerState && PlayerController->PlayerState->GetUniqueId().IsValid())
		{
			Participants.Add(PlayerController);
		}
	}

	for (APlayerController* Listener : Participants)
	{
		const bool bListenerSpectating = IsVoiceSpectator(Listener);

		for (APlayerController* Speaker : Participants)
		{
			if (Listener == Speaker)
			{
				continue;
			}

			const FUniqueNetIdRepl& SpeakerId = Speaker->PlayerState->GetUniqueId();

			// 관전 여부가 서로 다르면 차단. 같으면(둘 다 관전 / 둘 다 인게임).
			if (bListenerSpectating != IsVoiceSpectator(Speaker))
			{
				Listener->GameplayMutePlayer(SpeakerId);
			}
			else
			{
				Listener->GameplayUnmutePlayer(SpeakerId);
			}
		}
	}
}

AActor* AFAGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	const AFAPlayerState* FAPlayerState = Player ? Cast<AFAPlayerState>(Player->PlayerState) : nullptr;
	if (FAPlayerState && FAPlayerState->GetTeam() != EFATeam::None)
	{
		const FName WantedTag = FAPlayerState->IsZombie() ? ZombiePlayerStartTag : HumanPlayerStartTag;

		TArray<AActor*> Candidates;
		for (TActorIterator<APlayerStart> Iterator(GetWorld()); Iterator; ++Iterator)
		{
			APlayerStart* PlayerStart = *Iterator;

			// 데스매치 전용 스폰은 1층에 몰려 있어 일반 라운드에 쓰면 안 된다.
			if (UFADeathMatchSubsystem::IsArenaPlayerStart(PlayerStart))
			{
				continue;
			}

			// Player Start Tag 든 액터 태그든 인정한다 — 레벨이 액터 태그로 진영을 구분해 두었다.
			if (UFADeathMatchSubsystem::PlayerStartHasTag(PlayerStart, WantedTag))
			{
				Candidates.Add(PlayerStart);
			}
		}

		if (Candidates.Num() > 0)
		{
			return Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
		}

		// 여기까지 오면 진영 스폰을 하나도 못 찾았다는 뜻이다. 엔진 기본값은 아무 PlayerStart 나
		// 고르므로 인간이 좀비 스폰에서 시작할 수 있다 — 조용히 넘어가면 안 된다.
		UE_LOG(LogTemp, Warning,
			TEXT("[FA] '%s' 태그가 붙은 PlayerStart 를 찾지 못했다 — 진영 스폰이 무시되고 아무 자리에서나 시작한다 (맵 확인)"),
			*WantedTag.ToString());
	}

	return Super::ChoosePlayerStart_Implementation(Player);
}

void AFAGameMode::ClearRoundTimers()
{
	FTimerManager& TimerManager = GetWorldTimerManager();
	TimerManager.ClearTimer(RoundStartTimerHandle);
	TimerManager.ClearTimer(RoundTimerHandle);
	TimerManager.ClearTimer(DraftTimerHandle);
	TimerManager.ClearTimer(RoundEndTimerHandle);
	TimerManager.ClearTimer(SurvivalTickTimerHandle);
}

float AFAGameMode::GetServerTime() const
{
	return GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

AFAGameState* AFAGameMode::GetFAGameState() const
{
	return GetGameState<AFAGameState>();
}

void AFAGameMode::BeginReturnToLobby()
{
	//한번만 발동 명시
	if (bReturningToLobby)
		return;
	bReturningToLobby = true;
	
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFAOnlineSessionSubsystem* SessionSubsystem = GameInstance->GetSubsystem<UFAOnlineSessionSubsystem>())
		{
			//handle은 BeginPlayer에서 단 한번 등록을 유지함.
			SessionSubsystem->ReopenSessionForLobby();
			return;
		}
	}
	//OSS가 안잡히는 상황일때 대비
	TravelToLobby();
	
}

void AFAGameMode::HandleSessionReopenForLobby(bool bSuccess)
{
	if (!bReturningToLobby)
		return;
	
	if (!bSuccess)
	{
		//세션 복구 실패?? 이 경우 저장된 Saved\Logs경로의 로그에서 OnlineSubsystem의 오류 메세지 확인할 필요가 있음.
		UE_LOG(LogFAOSS, Error, TEXT("세션 복구 실패, Log확인필요"));
		bReturningToLobby = false;
		return;
	}
	TravelToLobby();
}

void AFAGameMode::TravelToLobby()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		bReturningToLobby = false;
		return;
	}
	
	const FString LobbyLevelPath = LobbyLevel.ToSoftObjectPath().GetLongPackageName();
	
	if (LobbyLevelPath.IsEmpty())
	{
		UE_LOG(LogFAOSS, Error, TEXT("FAGameMode에 돌아갈 로비 레벨이 지정되지 않았습니다."));
		return;
	}
	
	if (!World->ServerTravel(LobbyLevelPath))
	{
		UE_LOG(
			LogFAOSS,
			Error,
			TEXT("로비 ServerTravel 요청에 실패했습니다."));

		bReturningToLobby = false;
	}
}
