#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/FAGameMode.h"
#include "Game/FAGameState.h"
#include "Game/FALobbyGameMode.h"
#include "Game/FALobbyGameState.h"
#include "GameFramework/PlayerController.h"
#include "Player/FALobbyPlayerState.h"
#include "TimerManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFALobbyPlayerCountTest, "FallingApart.Lobby.PlayerCountWithoutController",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFALobbyPlayerCountTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("World"), World)) { return false; }
	AFALobbyGameMode* Mode = World->SpawnActor<AFALobbyGameMode>();
	AFALobbyGameState* State = World->SpawnActor<AFALobbyGameState>();
	AFALobbyPlayerState* Orphan = World->SpawnActor<AFALobbyPlayerState>();
	AFALobbyPlayerState* Connected = World->SpawnActor<AFALobbyPlayerState>();
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	if (TestNotNull(TEXT("Mode"), Mode) && TestNotNull(TEXT("State"), State)
		&& TestNotNull(TEXT("Orphan"), Orphan) && TestNotNull(TEXT("Connected"), Connected)
		&& TestNotNull(TEXT("Controller"), Controller))
	{
		Mode->GameState = State;
		Mode->MinPlayersToStart = 1;
		Mode->RequiredPlayersToStart = 0;
		Orphan->SetLobbyReady(true);
		Connected->SetOwner(Controller);
		Controller->SetPlayerState(Connected);
		Connected->SetLobbyReady(true);
		State->PlayerArray = { nullptr, Orphan, Connected };

		int32 PlayerCount = -1;
		int32 ReadyCount = -1;
		TestTrue(TEXT("Connected ready player can start; orphan is skipped"), Mode->CanStartGame(PlayerCount, ReadyCount));
		TestEqual(TEXT("Only connected player is counted"), PlayerCount, 1);
		TestEqual(TEXT("Orphan readiness is ignored"), ReadyCount, 1);

		Connected->SetOwner(nullptr);
		Controller->SetPlayerState(nullptr);
		TestFalse(TEXT("Departed controller cannot keep lobby ready"), Mode->CanStartGame(PlayerCount, ReadyCount));
		TestEqual(TEXT("No connected players remain"), PlayerCount, 0);
		TestEqual(TEXT("No ready players remain"), ReadyCount, 0);
	}
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFAMatchReturnGuardTest, "FallingApart.Lobby.ReturnOnlyAfterMatchEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFAMatchReturnGuardTest::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("Engine"), GEngine)) { return false; }
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("World"), World)) { return false; }
	// ServerTravel queries the engine's world context even if this world never ticks.
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	AFAGameMode* Mode = World->SpawnActor<AFAGameMode>();
	AFAGameState* State = World->SpawnActor<AFAGameState>();
	if (TestNotNull(TEXT("Mode"), Mode) && TestNotNull(TEXT("State"), State))
	{
		Mode->GameState = nullptr;
		Mode->RequestReturnToLobbyNow();
		TestFalse(TEXT("Missing game state cannot return"), Mode->bReturningToLobby);
		TestTrue(TEXT("Missing game state leaves travel alone"), World->NextURL.IsEmpty());
		Mode->GameState = State;
		Mode->LobbyLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/FallingApart/Level/L_LobbyLevel.L_LobbyLevel")));
		World->GetTimerManager().SetTimer(Mode->ReturnToLobbyTimerHandle, [] {}, 10.0f, false);

		for (EFARoundPhase Phase : { EFARoundPhase::WaitingToStart, EFARoundPhase::RoundStarting,
			EFARoundPhase::InProgress, EFARoundPhase::RoundEnd, EFARoundPhase::DeathMatch })
		{
			State->SetRoundPhase(Phase);
			Mode->RequestReturnToLobbyNow();
			Mode->BeginReturnToLobby();
			TestFalse(TEXT("Unfinished match cannot begin return"), Mode->bReturningToLobby);
			TestTrue(TEXT("Rejected request does not schedule travel"), World->NextURL.IsEmpty());
			TestTrue(TEXT("Rejected request preserves pending timer"), World->GetTimerManager().IsTimerActive(Mode->ReturnToLobbyTimerHandle));
		}

		State->SetRoundPhase(EFARoundPhase::MatchEnd);
		Mode->RequestReturnToLobbyNow();
		TestTrue(TEXT("Final match end begins return"), Mode->bReturningToLobby);
		TestFalse(TEXT("Immediate return clears delayed return timer"), World->GetTimerManager().IsTimerActive(Mode->ReturnToLobbyTimerHandle));
		TestEqual(TEXT("Return requests the configured lobby"), World->NextURL, FString(TEXT("/Game/FallingApart/Level/L_LobbyLevel")));
		TestFalse(TEXT("Duplicate return is rejected"), Mode->CanReturnToLobby());
		// This world has no game instance and is never ticked: no real map/session travel occurs.
		World->NextURL.Empty();
	}
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
