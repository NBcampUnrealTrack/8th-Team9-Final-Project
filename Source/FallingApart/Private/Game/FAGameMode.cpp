// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/FAGameMode.h"

#include "EngineUtils.h"
#include "GAS/FAAbilitySystemComponent.h"
#include "GAS/FAGameplayTags.h"
#include "Game/FAGameState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
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
}

bool AFAGameMode::ReadyToStartMatch_Implementation()
{
	if (GetMatchState() != MatchState::WaitingToStart)
	{
		return false;
	}

	return (NumPlayers + NumBots) >= FMath::Max(1, MinPlayersToStart);
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

	SettleRoundScore(Result);

	FAGameState->SetRoundPhase(EFARoundPhase::RoundEnd);
	FAGameState->SetLastRoundResult(Result);

	UE_LOG(LogTemp, Log, TEXT("[FA] Round %d ended (%s)"), FAGameState->GetCurrentRound(),
		Result == EFARoundResult::HumanWin ? TEXT("HumanWin") : TEXT("ZombieWin"));

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
		// 생존 +1 × 생존자 수 — 감염 이력이 없는 인간 전원에게 지급 (GDD 5.1 / 5.2).
		int32 SurvivorCount = 0;
		for (const AFAPlayerState* FAPlayerState : AllPlayerStates)
		{
			if (FAPlayerState->IsHuman() && !FAPlayerState->WasInfectedThisRound())
			{
				++SurvivorCount;
			}
		}

		for (AFAPlayerState* FAPlayerState : AllPlayerStates)
		{
			if (FAPlayerState->IsHuman() && !FAPlayerState->WasInfectedThisRound())
			{
				FAPlayerState->AddHumanTeamScore(SurvivorCount * SurvivorScorePerHead);
			}
		}
	}
	else if (Result == EFARoundResult::ZombieWin)
	{
		// 인간 전멸 +5 — 좀비 진영 전원 (시작 좀비 · 전환 좀비 동일)
		for (AFAPlayerState* FAPlayerState : AllPlayerStates)
		{
			if (FAPlayerState->IsZombie())
			{
				FAPlayerState->AddZombieTeamScore(WipeOutScore);
			}
		}
	}

	// 최장 생존 +2 — 동률이면 전원에게 준다 (GDD 5.1 각주).
	float BestSurvivalTime = 0.0f;
	for (const AFAPlayerState* FAPlayerState : AllPlayerStates)
	{
		BestSurvivalTime = FMath::Max(BestSurvivalTime, FAPlayerState->GetRoundScore().SurvivalTime);
	}

	if (BestSurvivalTime > 0.01f)
	{
		for (AFAPlayerState* FAPlayerState : AllPlayerStates)
		{
			if (FMath::IsNearlyEqual(FAPlayerState->GetRoundScore().SurvivalTime, BestSurvivalTime, 0.01f))
			{
				FAPlayerState->AddHumanPersonalScore(LongestSurvivorScore);
			}
		}
	}

	// 정산이 끝난 라운드 점수를 매치 누적으로 넘긴다.
	for (AFAPlayerState* FAPlayerState : AllPlayerStates)
	{
		FAPlayerState->CommitRoundScore();
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

	// 감염 성공 +2 — 개인 점수 (시작 좀비 · 전환 좀비 동일)
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

	// 구출 성공 +1 — 개인 점수. (GDD 11.3).
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
		FAPlayerState->SetTeam(EFATeam::Human);
		Candidates.Add(FAPlayerState);
	}

	const int32 ZombieCount = GetStartingZombieCount(Candidates.Num());

	// 시작 좀비는 서버 RNG 로 무작위 선정
	for (int32 Index = 0; Index < ZombieCount && Candidates.Num() > 0; ++Index)
	{
		const int32 PickedIndex = FMath::RandRange(0, Candidates.Num() - 1);
		Candidates[PickedIndex]->SetTeam(EFATeam::Zombie);
		Candidates.RemoveAtSwap(PickedIndex);
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
			FAPlayerState->ResetForNewRound();
			FAPlayerState->SetTeam(EFATeam::Zombie);
		}
		UpdateTeamCounts(false);
	}
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
}

float AFAGameMode::GetServerTime() const
{
	return GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

AFAGameState* AFAGameMode::GetFAGameState() const
{
	return GetGameState<AFAGameState>();
}
