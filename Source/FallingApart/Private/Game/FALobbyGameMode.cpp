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
			PreparedForMatchDelegateHandle = OnlineSessionSubsystem->OnSessionPreparedForMatch.AddUObject(
				this, &ThisClass::HandleSessionPrepared);
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
		//콜백 해제
		OnlineSessionSubsystem->OnSessionPreparedForMatch.Remove(PreparedForMatchDelegateHandle);
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
		
		if (!LobbyPlayerState || LobbyPlayerState->IsInactive())
			return;
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
		
		if (!LobbyPlayerState || LobbyPlayerState->IsInactive())
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
	OnlineSessionSubsystem->OnSessionPreparedForMatch.Remove(PreparedForMatchDelegateHandle);

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
}

AFALobbyGameState* AFALobbyGameMode::GetLobbyGameState() const
{
	return GetGameState<AFALobbyGameState>();
}
