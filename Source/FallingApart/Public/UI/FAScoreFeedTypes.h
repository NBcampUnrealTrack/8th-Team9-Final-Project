#pragma once

#include "CoreMinimal.h"
#include "FAScoreFeedTypes.generated.h"

UENUM(BlueprintType)
enum class EFAScoreReason : uint8
{
	Survival, Rescue, Infection, RoundComplete, WipeOut, StartingZombie
};

USTRUCT(BlueprintType)
struct FALLINGAPART_API FFAScoreFeedEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) EFAScoreReason Reason = EFAScoreReason::Survival;
	UPROPERTY(BlueprintReadOnly) int32 Points = 0;
	UPROPERTY(BlueprintReadOnly) FString PlayerName;
	UPROPERTY(BlueprintReadOnly) float IntervalSeconds = 0.f;
	UPROPERTY(BlueprintReadOnly) int32 Round = 0;
	UPROPERTY(BlueprintReadOnly) int32 Sequence = 0;

	FText GetDescription() const;
};
