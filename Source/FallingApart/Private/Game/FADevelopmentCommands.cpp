#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"

namespace FADevelopmentCommands
{
	static void TestMap(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() != 1)
		{
			UE_LOG(LogTemp, Display, TEXT("[FATestMap] Usage: fa.TestMap Mall|Gym|Container|Lobby (solo host only)"));
			return;
		}

		FString MapName;
		if (Args[0].Equals(TEXT("Mall"), ESearchCase::IgnoreCase)) { MapName = TEXT("L_Map02_Mall"); }
		else if (Args[0].Equals(TEXT("Gym"), ESearchCase::IgnoreCase)) { MapName = TEXT("L_Map01_Gym"); }
		else if (Args[0].Equals(TEXT("Container"), ESearchCase::IgnoreCase)) { MapName = TEXT("L_Map03_Container"); }
		else if (Args[0].Equals(TEXT("Lobby"), ESearchCase::IgnoreCase)) { MapName = TEXT("L_LobbyLevel"); }
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[FATestMap] Unknown map. Use Mall, Gym, Container, or Lobby."));
			return;
		}

		if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogTemp, Warning, TEXT("[FATestMap] Run in a standalone game or on the local host."));
			return;
		}
		int32 PlayerCount = 0;
		APlayerController* LocalHost = nullptr;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* PC = It->Get())
			{
				++PlayerCount;
				if (PC->IsLocalController()) { LocalHost = PC; }
			}
		}
		if (PlayerCount != 1 || !LocalHost)
		{
			UE_LOG(LogTemp, Warning, TEXT("[FATestMap] Requires exactly one player and a local host; found %d."), PlayerCount);
			return;
		}

		const FString Package = TEXT("/Game/FallingApart/Level/") + MapName;
		if (!FPackageName::DoesPackageExist(Package))
		{
			UE_LOG(LogTemp, Error, TEXT("[FATestMap] Map is missing from this build: %s"), *Package);
			return;
		}
		// Absolute travel clears the solo option when returning to the normal lobby.
		const FString URL = Package + (MapName == TEXT("L_LobbyLevel") ? TEXT("") : TEXT("?FASoloTest=1"));
		UE_LOG(LogTemp, Display, TEXT("[FATestMap] Traveling to %s"), *URL);
		if (!World->ServerTravel(URL, true))
		{
			UE_LOG(LogTemp, Error, TEXT("[FATestMap] Travel request failed."));
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs TestMapCommand(
		TEXT("fa.TestMap"),
		TEXT("fa.TestMap Mall|Gym|Container|Lobby - solo Development map test; gameplay maps use a one-hour round without drafts."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TestMap));
}

#endif
