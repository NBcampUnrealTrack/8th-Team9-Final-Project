// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/FALobbyGameMode.h"

#include "Game/FALobbyGameState.h"
#include "Network/FAOnlineSessionSubsystem.h"
#include "Player/FACharacter.h"
#include "Player/FALobbyPlayerController.h"
#include "Player/FALobbyPlayerState.h"

AFALobbyGameMode::AFALobbyGameMode()
{
	GameStateClass = AFALobbyGameState::StaticClass();
	PlayerStateClass = AFALobbyPlayerState::StaticClass();
	PlayerControllerClass = AFALobbyPlayerController::StaticClass();
	DefaultPawnClass = AFACharacter::StaticClass();
	
	bUseSeamlessTravel = true;
	
	PrimaryActorTick.bCanEverTick = false;
}

void AFALobbyGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId,
                                FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty())
		return;
	
	if (bGameStartCommitted)
	{
		ErrorMessage = TEXT("게임이 이미 시작하였습니다.");
		return;
	}
	// Options나 입장 Address속성으로 추가작업
}

void AFALobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	//접속시 카운트다운 취소.
	if (IsCountdownActive())
		CancelGameCountdown();
	
	AFALobbyPlayerState* LobbyPlayerState = NewPlayer ? NewPlayer->GetPlayerState<AFALobbyPlayerState>() : nullptr;
	
	if (!LobbyPlayerState)
	{
		RefreshLobbyState();
		return;
	}
	
	LobbyPlayerState->ResetLobbyState();

	EvaluateLobby();
}

void AFALobbyGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	// 준비페이즈, 카운트다운페이즈, 이동페이즈에 대한 처리
	if (bGameStartCommitted)
	{
		RollbackGameStart();
		return;
	}
	//플레이어 이탈시 카운트다운 초기화, Evaluate 결과에 따라 재시작됨.
	if (IsCountdownActive())
		CancelGameCountdown();

	EvaluateLobby();
}

void AFALobbyGameMode::SetPlayerReady(APlayerController* PlayerController, bool bReady)
{
	// 서버검증 + 게임 시작중인지 확인
	if (!HasAuthority() || !PlayerController || bGameStartCommitted)
	{
		return;
	}
	
	AFALobbyGameState* LobbyGameState = GetLobbyGameState();
	AFALobbyPlayerState* LobbyplayerState = PlayerController->GetPlayerState<AFALobbyPlayerState>();
	if (!LobbyplayerState || !LobbyGameState)
		return;

	// 필요시 IsOnlyASpectator까지 확인(완전 관전자)
	if (!LobbyGameState->PlayerArray.Contains(LobbyplayerState))
		return;
	
	LobbyplayerState->SetLobbyReady(bReady);
	
	EvaluateLobby();
}

void AFALobbyGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (UGameInstance* GameInstance = GetGameInstance())
	{ 
		OnlineSessionSubsystem = GameInstance->GetSubsystem<UFAOnlineSessionSubsystem>();
		if (OnlineSessionSubsystem)
		{
			// 매치 준비와 로비 복구의 비동기 완료 결과를 각각 처리한다.
			PreparedForMatchDelegateHandle = OnlineSessionSubsystem->OnSessionPreparedForMatch.AddUObject(
				this, &ThisClass::HandleSessionPrepared);
			
			ReopenedForLobbyDelegateHandle = OnlineSessionSubsystem->OnSessionReopenedForLobby.AddUObject(
				this, &ThisClass::HandleSessionReopened);
		}
	}
	if (AFALobbyGameState* LobbyGameState = GetLobbyGameState())
	{
		LobbyGameState->SetSelectedMapId(DefaultMapId);
	}
	RefreshLobbyState();
}

void AFALobbyGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (OnlineSessionSubsystem)
	{
		// GameMode 종료 후 지연 콜백이 호출되지 않도록 두 이벤트를 모두 해제한다.
		OnlineSessionSubsystem->OnSessionPreparedForMatch.Remove(PreparedForMatchDelegateHandle);
		OnlineSessionSubsystem->OnSessionReopenedForLobby.Remove(ReopenedForLobbyDelegateHandle);
	}
	GetWorldTimerManager().ClearTimer(GameStartCountdownTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void AFALobbyGameMode::RefreshLobbyState()
{
	// 이미 시작을 눌러 bStartCommitted=true일때 처리 필요.
	AFALobbyGameState* LobbyGameState = GetLobbyGameState();
	if (!LobbyGameState)
		return;
	
	int32 PlayerCount = 0;
	int32 ReadyCount = 0;
	
	for (APlayerState* PlayerState : LobbyGameState->PlayerArray)
	{
		AFALobbyPlayerState* LobbyPlayerState = Cast<AFALobbyPlayerState>(PlayerState);
		
		if (!LobbyPlayerState || LobbyPlayerState->IsInactive() || LobbyPlayerState->IsOnlyASpectator())
			continue;
		PlayerCount++;
		
		if (LobbyPlayerState->IsLobbyReady())
			ReadyCount++;
	}
	float ReplicatedCountdownEndTime = 0.f;
	EFALobbyPhase Phase;
	if (IsCountdownActive())
	{
		Phase = EFALobbyPhase::Countdown;
		ReplicatedCountdownEndTime = CountdownEndServerTime;
	}
	else
	{
		//최소 시장에 필요한 수보다 적으면 WaitingForPlayer, 넘겼으면 WaitingForReady
		Phase =	PlayerCount < MinPlayersToStart
			? EFALobbyPhase::WaitingForPlayers
			: EFALobbyPhase::WaitingForReady;
	}

	const bool bCanStart =
		CanStartGame(PlayerCount, ReadyCount);
	LobbyGameState->SetLobbySnapshot(
		PlayerCount,
		ReadyCount,
		MinPlayersToStart,
		Phase,
		bCanStart,
		ReplicatedCountdownEndTime
		);
	
}

void AFALobbyGameMode::EvaluateLobby()
{
	if (bGameStartCommitted)
	{
		return;
	}

	int32 PlayerCount;
	int32 ReadyCount;
	bool bCanStart = CanStartGame(PlayerCount, ReadyCount);
	
	// Countdown 중 시작 조건이 깨지면 즉시 취소.
	if (!bCanStart && IsCountdownActive())
	{
		CancelGameCountdown();
	}

	// 전원 Ready여도 자동 시작하지 않고 UI 상태만 갱신.
	RefreshLobbyState();
}

bool AFALobbyGameMode::CanStartGame(int32& OutPlayerCount, int32& OutReadyCount) const
{
	OutPlayerCount = 0;
	OutReadyCount = 0;

	const AFALobbyGameState* LobbyState = GetGameState<AFALobbyGameState>();
	if (!LobbyState)
	{
		return false;
	}
	
	for (APlayerState* PlayerState : LobbyState->PlayerArray)
	{
		AFALobbyPlayerState* LobbyPlayerState = Cast<AFALobbyPlayerState>(PlayerState);
		
		if (!LobbyPlayerState || LobbyPlayerState->IsInactive() || LobbyPlayerState->IsOnlyASpectator())
			continue;
		
		OutPlayerCount++;
		
		if (LobbyPlayerState->IsLobbyReady())
			OutReadyCount++;
	}

	const bool bEnoughPlayers =
		RequiredPlayersToStart > 0
			? OutPlayerCount >= RequiredPlayersToStart
			: OutPlayerCount >= MinPlayersToStart;
	const bool bAllReady =
		OutPlayerCount > 0 &&
		OutReadyCount == OutPlayerCount;
	UE_LOG(LogFAOSS, Warning, TEXT("CanStartGame() | bEnoughPlayers %d,  bAllReady %d players"), bEnoughPlayers, bAllReady);

	return bEnoughPlayers && bAllReady;
}

void AFALobbyGameMode::CommitGameStart()
{
	if (bGameStartCommitted)
	{
		return;
	}

	int32 PlayerCount;
	int32 ReadyCount;
	bool bCanStart = CanStartGame(PlayerCount, ReadyCount);
	if (!bCanStart)
	{
		return;
	}

	bGameStartCommitted = true;

	AFALobbyGameState* LobbyState = GetLobbyGameState();
	LobbyState->SetLobbySnapshot(
		PlayerCount,
		ReadyCount,
		MinPlayersToStart,
		EFALobbyPhase::LockingSession,
		bCanStart,
		CountdownEndServerTime
		);

	const FName MapId = LobbyState->GetSelectedMapId();

	if (!OnlineSessionSubsystem)
	{
		RollbackGameStart();
		return;
	}

	OnlineSessionSubsystem->PrepareSessionForMatch(
		MapId,
		PlayerCount);
}

void AFALobbyGameMode::HandleSessionPrepared(bool bSuccess)
{		
	//OnlineSessionSubsystem->OnSessionPreparedForMatch.Remove(PreparedForMatchDelegateHandle);

	if (!bGameStartCommitted)
	{
		return;
	}

	if (!bSuccess)
	{
		RollbackGameStart();
		return;
	}

	int32 PlayerCount;
	int32 ReadyCount;
	bool bCanStart = CanStartGame(PlayerCount, ReadyCount);
	if (!bCanStart)
	{
		RollbackGameStart();
		return;
	}

	if (AFALobbyGameState* LobbyState = GetLobbyGameState())
	{
		LobbyState->SetLobbySnapshot(
			PlayerCount,
			ReadyCount,
			MinPlayersToStart,
			EFALobbyPhase::Traveling,
			bCanStart,
			CountdownEndServerTime);
	}

	TravelToSelectedMap();
}

void AFALobbyGameMode::HandleSessionReopened(bool bSuccess)
{
	// 현재 GameMode가 요청한 복구 결과만 처리한다.
	if (!bRollbackInProgress)
	{
		return;
	}

	bRollbackInProgress = false;

	if (!bSuccess)
	{
		// 세션 복구 실패 시 실제 세션과 UI 상태가 어긋나지 않도록 로비 잠금을 유지한다.
		UE_LOG(LogFAOSS, Error, TEXT("HandleSessionReopened() | Failed to reopen the lobby session"));
		return;
	}

	// 세션 설정 복구가 완료된 뒤에만 Ready 입력과 신규 접속을 다시 허용한다.
	bGameStartCommitted = false;
	RefreshLobbyState();
}

void AFALobbyGameMode::TravelToSelectedMap()
{
	const AFALobbyGameState* LobbyState = GetLobbyGameState();
	if (!LobbyState)
	{
		RollbackGameStart();
		return;
	}

	const TSoftObjectPtr<UWorld> Map =
		OnlineSessionSubsystem->ResolveMap(LobbyState->GetSelectedMapId());

	FString MapPath =
		Map.ToSoftObjectPath().GetLongPackageName();
	
	MapPath = MapPath+TEXT("?listen");
	
	// 이동 경로는 아래의 형태로 진행해야함.
	//FString TempMapPath = TEXT("/Game/FallingApart/Level/L_TestLevel_PJH_Greybox?listen");
	if (MapPath.IsEmpty() || !GetWorld()->ServerTravel(MapPath))
	{
		RollbackGameStart();
	}
}

void AFALobbyGameMode::RollbackGameStart()
{
	// 중복 이탈이나 실패 콜백으로 동일한 복구 요청이 여러 번 시작되는 것을 막는다.
	if (!bGameStartCommitted || bRollbackInProgress)
	{
		return;
	}

	if (!OnlineSessionSubsystem)
	{
		// 온라인 세션이 없는 로컬 실행에서는 복구할 외부 상태가 없으므로 즉시 잠금을 해제한다.
		bGameStartCommitted = false;
		RefreshLobbyState();
		return;
	}

	// 복구 완료 전까지 현재 잠금 Phase를 유지하고 완료 콜백에서만 로비를 다시 연다.
	bRollbackInProgress = true;
	OnlineSessionSubsystem->ReopenSessionForLobby();
}

AFALobbyGameState* AFALobbyGameMode::GetLobbyGameState() const
{
	return GetGameState<AFALobbyGameState>();
}

void AFALobbyGameMode::RequestStartCountDown(APlayerController* RequestingController)
{
	bool bHasAuthority = HasAuthority();
	bool bIsHostController = IsHostController(RequestingController);
	bool bIsCounddownActive = IsCountdownActive();
	if (  !bHasAuthority
		|| !bIsHostController// 호스트가 아니거나
		|| bGameStartCommitted// 이미 시작했거나
		|| bRollbackInProgress// 롤백중이거나
		|| bIsCounddownActive)// 카운트다운중이라면
	{
		return;
	}
	int32 PlayerCount = 0;
	int32 ReadyCount = 0;
	
	if (!CanStartGame(PlayerCount, ReadyCount))
	{
		RefreshLobbyState();
		return;
	}

	StartGameCountdown();
}



bool AFALobbyGameMode::IsHostController(APlayerController* Controller)
{
	return Controller //nullCkeck
	&& Controller->HasAuthority()//서버 권한 체크
	&& Controller->IsLocalController()//동시에 로컬 컨트롤러인지 체크
	&& GetNetMode() == NM_ListenServer;//넷모드 체크
}

void AFALobbyGameMode::StartGameCountdown()
{
	if (IsCountdownActive() || bGameStartCommitted)
		return;
	int32 PlayerCount = 0;
	int32 ReadyCount = 0;
	bool bCanStart = CanStartGame(PlayerCount, ReadyCount);
	if (!bCanStart)
	{
		return;
	}

	AFALobbyGameState* LobbyState = GetLobbyGameState();
	if (!LobbyState)
	{
		return;
	}
	// 설정된 시간만큼 타이머 설정함
	CountdownEndServerTime =
		LobbyState->GetServerWorldTimeSeconds()
		+ GameStartCountdownDelayInSeconds;
	
	// GameState에 전파
	LobbyState->SetLobbySnapshot(
		PlayerCount,
		ReadyCount,
		MinPlayersToStart,
		EFALobbyPhase::Countdown,
		bCanStart,
		CountdownEndServerTime);
	
	// 실제 타이머 시작
	GetWorldTimerManager().SetTimer(
		GameStartCountdownTimerHandle,
		this,
		&ThisClass::HandleGameCountdownFinished,
		GameStartCountdownDelayInSeconds,
		false);
	
}

void AFALobbyGameMode::CancelGameCountdown()
{
	//취소 타이머 핸들 해제
	GetWorldTimerManager().ClearTimer(GameStartCountdownTimerHandle);
	CountdownEndServerTime = 0.f;
}

void AFALobbyGameMode::HandleGameCountdownFinished()
{
	CountdownEndServerTime = 0.0f;

	int32 PlayerCount = 0;
	int32 ReadyCount = 0;

	// 카운트 다운 종료후 마지막으로 추가검증.
	if (!CanStartGame(PlayerCount, ReadyCount))
	{
		RefreshLobbyState();
		return;
	}
	
	//문제 없으면 시작.
	CommitGameStart();
}


bool AFALobbyGameMode::IsCountdownActive() const
{
	//핸들로 작동중 확인
	bool bIsCountdownActive = GetWorldTimerManager().IsTimerActive(GameStartCountdownTimerHandle);
	return bIsCountdownActive;
}
