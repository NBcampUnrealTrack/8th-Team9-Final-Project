#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Player/FAPlayerState.h"
#include "UI/FAScoreFeedTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFAHUDScoreHistoryTest, "FallingApart.UI.HUDDetails.ScoreHistory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFAHUDScoreHistoryTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Test world"), World)) { return false; }
	AFAPlayerState* PS = World->SpawnActor<AFAPlayerState>();
	if (TestNotNull(TEXT("Player state"), PS))
	{
		PS->AddHumanPersonalScore(3);
		PS->RecordScoreAward(EFAScoreReason::Rescue, 3, TEXT("테스터 {0}"));
		TestEqual(TEXT("Actual awarded amount"), PS->GetScoreHistory()[0].Points, 3);
		TestTrue(TEXT("Player names are literal arguments"), PS->GetScoreHistory()[0].GetDescription().ToString().Contains(TEXT("테스터 {0}")));
		PS->RecordScoreAward(EFAScoreReason::Infection, 0);
		TestEqual(TEXT("Zero awards do not produce phantom events"), PS->GetScoreHistory().Num(), 1);
		PS->CommitRoundScore();
		TestEqual(TEXT("Settlement does not create duplicate awards"), PS->GetScoreHistory().Num(), 1);
		PS->ResetForNewRound();
		TestEqual(TEXT("History survives round changes"), PS->GetScoreHistory().Num(), 1);
		for (int32 Index = 0; Index < 40; ++Index)
		{
			PS->RecordScoreAward(EFAScoreReason::Survival, 1, FString(), 30.f);
		}
		TestEqual(TEXT("History is bounded"), PS->GetScoreHistory().Num(), 32);
		TestEqual(TEXT("Newest event retained"), PS->GetScoreHistory().Last().Sequence, 41);
		TestTrue(TEXT("Survival uses configured seconds"), PS->GetScoreHistory().Last().GetDescription().ToString().Contains(TEXT("30")));
		TestFalse(TEXT("Format argument replaced"), PS->GetScoreHistory().Last().GetDescription().ToString().Contains(TEXT("{0}")));
		AFAPlayerState* Replacement = World->SpawnActor<AFAPlayerState>();
		PS->CopyProperties(Replacement);
		TestEqual(TEXT("Travel/reconnection preserves history"), Replacement->GetScoreHistory().Num(), 32);
		Replacement->RecordScoreAward(EFAScoreReason::Infection, 3, TEXT("Player"));
		TestEqual(TEXT("Sequence survives travel"), Replacement->GetScoreHistory().Last().Sequence, 42);
		PS->ResetForNewMatch();
		TestTrue(TEXT("New match clears old history"), PS->GetScoreHistory().IsEmpty());
	}
	World->DestroyWorld(false);
	return true;
}

#endif
