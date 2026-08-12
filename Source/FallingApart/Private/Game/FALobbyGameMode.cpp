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
	AFALobbyPlayerState* LobbyPlayerState = NewPlayer ? NewPlayer->GetPlayerState<AFALobbyPlayerState>() : nullptr;
	
	if (!LobbyPlayerState)
		return;
	
	LobbyPlayerState->ResetLobbyState();

	RefreshLobbyState();
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

	RefreshLobbyState();
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
	RefreshLobbyState();
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
	//최소 시장에 필요한 수보다 적으면 WaitingForPlayer, 넘겼으면 WaitingForReady
	EFALobbyPhase Phase =
		PlayerCount < MinPlayersToStart
			? EFALobbyPhase::WaitingForPlayers
			: EFALobbyPhase::WaitingForReady;

	LobbyGameState->SetLobbySnapshot(
		PlayerCount,
		ReadyCount,
		MinPlayersToStart,
		Phase);
}

void AFALobbyGameMode::EvaluateLobby()
{
	if (bGameStartCommitted)
	{
		return;
	}

	int32 PlayerCount;
	int32 ReadyCount;
	bool CanStart = CanStartGame(PlayerCount, ReadyCount);
	if (CanStart)
	{
		CommitGameStart();
	}
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

	if (!CanStartGame(PlayerCount, ReadyCount))
	{
		return;
	}

	bGameStartCommitted = true;

	AFALobbyGameState* LobbyState = GetLobbyGameState();
	LobbyState->SetLobbySnapshot(
		PlayerCount,
		ReadyCount,
		MinPlayersToStart,
		EFALobbyPhase::LockingSession);

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

	if (!CanStartGame(PlayerCount, ReadyCount))
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
			EFALobbyPhase::Traveling);
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
