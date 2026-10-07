#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Interact/FADoor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFADoorIdleTickTest, "FallingApart.Interaction.Door.IdleTick",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFADoorIdleTickTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("World"), World)) { return false; }
	AFADoor* Door = World->SpawnActor<AFADoor>();
	if (TestNotNull(TEXT("Door"), Door))
	{
		Door->PawnCollisionRestoreGraceSeconds = 0.0f;
		Door->OpenSoundId = NAME_None;
		Door->CloseSoundId = NAME_None;
		Door->DispatchBeginPlay();
		TestFalse(TEXT("Closed door sleeps"), Door->IsActorTickEnabled());
		const FRotator Closed = Door->HingePivot->GetRelativeRotation();
		const ECollisionResponse OriginalResponse = Door->DoorMesh->GetCollisionResponseToChannel(ECC_Pawn);
		Door->SetDoorOpen(true, nullptr);
		TestTrue(TEXT("Opening wakes door"), Door->IsActorTickEnabled());
		Door->Tick(0.1f);
		TestEqual(TEXT("Moving door does not push pawns"), Door->DoorMesh->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
		Door->SetDoorOpen(false, nullptr);
		TestTrue(TEXT("Mid-motion reversal stays awake"), Door->IsActorTickEnabled());
		for (int32 Step = 0; Step < 20 && Door->IsActorTickEnabled(); ++Step) { Door->Tick(0.1f); }
		TestTrue(TEXT("Reversal reaches closed pose"), Door->HingePivot->GetRelativeRotation().Equals(Closed, 0.05f));
		TestEqual(TEXT("Collision restored before sleeping"), Door->DoorMesh->GetCollisionResponseToChannel(ECC_Pawn), OriginalResponse);
		TestFalse(TEXT("Settled door sleeps again"), Door->IsActorTickEnabled());
		Door->SetDoorOpen(true, nullptr);
		for (int32 Step = 0; Step < 20 && Door->IsActorTickEnabled(); ++Step) { Door->Tick(0.1f); }
		TestFalse(TEXT("Open door sleeps"), Door->IsActorTickEnabled());
		TestFalse(TEXT("Open pose differs from closed"), Door->HingePivot->GetRelativeRotation().Equals(Closed, 0.05f));
	}
	World->DestroyWorld(false);
	return true;
}

#endif
