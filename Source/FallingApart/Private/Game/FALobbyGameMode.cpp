// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/FALobbyGameMode.h"

#include "OnlineBeaconHost.h"
#include "Game/FALobbyGameState.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Kismet/GameplayStatics.h"
#include "Network/FAOnlineBeaconHostObject.h"
#include "Network/FAOnlineSessionSubsystem.h"
#include "Online/OnlineSessionNames.h"
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
	{
		UE_LOG(LogFAOSS, Warning, TEXT("[PreLoginAuth] Rejected by base PreLogin; Address=%s NetId=%s Error=%s"),
			*Address, *UniqueId.ToDebugString(), *ErrorMessage);
		return;
	}

	if (!OnlineSessionSubsystem)
	{
		OnlineSessionSubsystem = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UFAOnlineSessionSubsystem>()
			: nullptr;
	}
	
	if (!OnlineSessionSubsystem)
	{
		ErrorMessage = TEXT("FA_SESSION_UNAVAILABLE");
		UE_LOG(LogFAOSS, Error, TEXT("[PreLoginAuth] Rejected: session subsystem unavailable; Address=%s"),
			*Address);
		return;
	}
	
	if (bGameStartCommitted)
	{
		ErrorMessage = TEXT("MATCH_ALREADY_STARTED");
		UE_LOG(LogFAOSS, Warning, TEXT("[PreLoginAuth] Rejected: match already started; NetId=%s"),
			*UniqueId.ToDebugString());
		return;
	}
	
	if (!UniqueId.IsValid())
	{
		ErrorMessage = TEXT("INVALID_UNIQUE_NET_ID");
		UE_LOG(LogFAOSS, Warning, TEXT("[PreLoginAuth] Rejected: invalid NetId; Address=%s"), *Address);
		return;
	}
	//서버장
	if (IsSessionOwner(UniqueId))
	{
		UE_LOG(LogFAOSS, Verbose, TEXT("[PreLoginAuth] Accepted session owner; NetId=%s"),
			*UniqueId.ToDebugString());
		return;
	}
	//이미 접속한 유저(Lobby->Match->Lobby)이동시
	if (OnlineSessionSubsystem->IsAuthenticatedUserId(UniqueId))
	{
		UE_LOG(LogFAOSS, Verbose, TEXT("[PreLoginAuth] Accepted existing authenticated member; NetId=%s"),
			*UniqueId.ToDebugString());
		return;
	}
	
	const FString JoinToken = UGameplayStatics::ParseOption(Options, TEXT("JoinToken"));
	if (JoinToken.IsEmpty() || !OnlineSessionSubsystem->ConsumeJoinToken(JoinToken, UniqueId))
	{
		ErrorMessage = TEXT("INVALID_JOIN_TOKEN");
		UE_LOG(LogFAOSS, Warning, TEXT("[PreLoginAuth] Rejected invalid/expired token; NetId=%s HasToken=%s"),
			*UniqueId.ToDebugString(), JoinToken.IsEmpty() ? TEXT("false") : TEXT("true"));
		return;
	}

	UE_LOG(LogFAOSS, Log, TEXT("[PreLoginAuth] Accepted one-time token; NetId=%s"),
		*UniqueId.ToDebugString());
}

void AFALobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (OnlineSessionSubsystem && NewPlayer && NewPlayer->PlayerState)
	{
		const FUniqueNetIdRepl& PlayerNetId = NewPlayer->PlayerState->GetUniqueId();
		if (PlayerNetId.IsValid())
		{
			OnlineSessionSubsystem->SaveLoginUserId(PlayerNetId);
			UE_LOG(LogFAOSS, Log, TEXT("[PreLoginAuth] PostLogin authenticated member saved; NetId=%s"),
				*PlayerNetId.ToDebugString());
		}
	}

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
	const FUniqueNetIdRepl ExitingNetId = Exiting && Exiting->PlayerState
		? Exiting->PlayerState->GetUniqueId()
		: FUniqueNetIdRepl();

	Super::Logout(Exiting);
	
	if (OnlineSessionSubsystem && ExitingNetId.IsValid())
	{
		OnlineSessionSubsystem->RemoveLogoutUserId(ExitingNetId);
		UE_LOG(LogFAOSS, Log, TEXT("[PreLoginAuth] Logout removed authenticated member; NetId=%s"),
			*ExitingNetId.ToDebugString());
	}
	
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

FString AFALobbyGameMode::InitNewPlayer(
	APlayerController* NewPlayerController,
	const FUniqueNetIdRepl& UniqueId,
	const FString& Options, 
	const FString& Portal)
{
	FString ErrorMessage = Super::InitNewPlayer(
	   NewPlayerController,
	   UniqueId,
	   Options,
	   Portal);
	if (ErrorMessage.IsEmpty() == false)
		return ErrorMessage;
	
	// UGameplayStatics::ParseOption로 접속 URL의 파라미터 파싱이 가능함.
	FString NewPlayerNickname = FGenericPlatformHttp::UrlDecode(
		UGameplayStatics::ParseOption(Options, TEXT("Nickname")));
	NewPlayerNickname = SanitizeNickname(NewPlayerController, NewPlayerNickname);
	SetPlayerNickname(NewPlayerController, NewPlayerNickname);
	
	return FString();
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
	
	OnlineSessionSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UFAOnlineSessionSubsystem>() : nullptr;
	
	//OSS없을시 
	if (!OnlineSessionSubsystem)
	{
		UE_LOG(LogFAOSS, Error, TEXT("OnlineSessionSubsystem is nullptr"));
		return;
	}
	
	if (!StartPasswordBeacon())
	{
		UE_LOG(LogFAOSS, Error, TEXT("Password onlineBeacon is unavailable"));
		
		OnlineSessionSubsystem->DestroySession();

		return;
	}
	UE_LOG(LogFAOSS, Log, TEXT("[BeaconAuth] Password beacon listening on port %d"), DEFAULT_BEACON_PORT);

	// 매치 준비와 로비 복구의 비동기 완료 결과를 각각 처리한다.
	PreparedForMatchDelegateHandle = OnlineSessionSubsystem->OnSessionPreparedForMatch.AddUObject(
		this, &ThisClass::HandleSessionPrepared);
	
	ReopenedForLobbyDelegateHandle = OnlineSessionSubsystem->OnSessionReopenedForLobby.AddUObject(
		this, &ThisClass::HandleSessionReopened);

	
	if (AFALobbyGameState* LobbyGameState = GetLobbyGameState())
	{
		LobbyGameState->SetSelectedMapId(DefaultMapId);
	}
	RefreshLobbyState();
}

void AFALobbyGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopPasswordBeacon();
	if (OnlineSessionSubsystem)
	{
		// GameMode 종료 후 지연 콜백이 호출되지 않도록 두 이벤트를 모두 해제한다.
		OnlineSessionSubsystem->OnSessionPreparedForMatch.Remove(PreparedForMatchDelegateHandle);
		OnlineSessionSubsystem->OnSessionReopenedForLobby.Remove(ReopenedForLobbyDelegateHandle);
		PreparedForMatchDelegateHandle.Reset();
		ReopenedForLobbyDelegateHandle.Reset();
	}
	GetWorldTimerManager().ClearTimer(GameStartCountdownTimerHandle);
	Super::EndPlay(EndPlayReason);
}

bool AFALobbyGameMode::IsSessionOwner(FUniqueNetIdRepl UniqueNetId)
{
	return OnlineSessionSubsystem && UniqueNetId.IsValid()
		&& OnlineSessionSubsystem->GetLocalUniqueNetId() == UniqueNetId;
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
	// 준비 구역에서 한 명이라도 걸어 나오면 이 경로로 들어와 카운트다운이 풀린다.
	if (!bCanStart && IsCountdownActive())
	{
		CancelGameCountdown();
	}
	else if (bCanStart
		&& bAutoStartCountdownWhenAllReady
		&& !IsCountdownActive()
		&& !bRollbackInProgress)
	{
		// 전원이 준비 구역에 모인 순간이 곧 시작 신호다. 호스트가 따로 누를 버튼이 없다.
		StartGameCountdown();

		// StartGameCountdown 이 Countdown Phase 스냅샷까지 이미 전파했으므로 중복 갱신을 건너뛴다.
		if (IsCountdownActive())
		{
			return;
		}
	}

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

FString AFALobbyGameMode::SanitizeNickname(APlayerController* NewPlayerController, FString Nickname)
{
	TArray<FString> PlayerNicknames;
	for (auto ExistingPlayerState : GetLobbyGameState()->PlayerArray)
	{
		//자신 제외
		if (NewPlayerController->GetPlayerState<APlayerState>() == ExistingPlayerState)
			continue;
		PlayerNicknames.Add(ExistingPlayerState->GetPlayerName());
	}
	bool IsSanitized = false;
	int32 DupeCount = 2;
	FString SanitizedNickname = Nickname;
	while (IsSanitized == false)
	{
		if (PlayerNicknames.Contains(SanitizedNickname))
		{
			SanitizedNickname = Nickname + FString::Printf(TEXT("_%d"), DupeCount);
			DupeCount++;
			continue;
		}
		
		IsSanitized = true;
	}
	return SanitizedNickname;
}

void AFALobbyGameMode::SetPlayerNickname(APlayerController* NewPlayerController, FString NewNickname)
{
	APlayerState* NewPlayerState = NewPlayerController->GetPlayerState<APlayerState>();
	NewPlayerState->SetPlayerName(NewNickname);
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

bool AFALobbyGameMode::StartPasswordBeacon()
{
	UWorld* World = GetWorld();

	if (!HasAuthority() || !World)
	{
		return false;
	}

	StopPasswordBeacon();

	PasswordBeaconHostObject = World->SpawnActor<AFAOnlineBeaconHostObject>();

	PasswordBeaconHost = World->SpawnActor<AOnlineBeaconHost>();

	if (!PasswordBeaconHostObject || !PasswordBeaconHost)
	{
		StopPasswordBeacon();
		return false;
	}

	PasswordBeaconHost->ListenPort = DEFAULT_BEACON_PORT;

	// 엔진 테스트 코드와 같은 등록 순서다.
	PasswordBeaconHost->RegisterHost(
		PasswordBeaconHostObject);

	if (!PasswordBeaconHost->InitHost())
	{
		StopPasswordBeacon();
		return false;
	}

	// InitHost 이후 실제 요청을 허용한다.
	PasswordBeaconHost->PauseBeaconRequests(false);
	return true;
}

void AFALobbyGameMode::StopPasswordBeacon()
{
	if (IsValid(PasswordBeaconHost) && IsValid(PasswordBeaconHostObject))
	{
		PasswordBeaconHost->UnregisterHost(
			PasswordBeaconHostObject->GetBeaconType());
	}

	if (IsValid(PasswordBeaconHost))
	{
		PasswordBeaconHost->DestroyBeacon();
		PasswordBeaconHost = nullptr;
	}

	if (IsValid(PasswordBeaconHostObject))
	{
		PasswordBeaconHostObject->Destroy();
		PasswordBeaconHostObject = nullptr;
	}
}
