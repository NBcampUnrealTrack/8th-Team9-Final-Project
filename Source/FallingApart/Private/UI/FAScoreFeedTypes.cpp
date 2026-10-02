#include "UI/FAScoreFeedTypes.h"

#define LOCTEXT_NAMESPACE "FAHUDDetails"

FText FFAScoreFeedEntry::GetDescription() const
{
	const FText Name = PlayerName.IsEmpty() ? LOCTEXT("UnknownPlayer", "플레이어") : FText::FromString(PlayerName);
	switch (Reason)
	{
	case EFAScoreReason::Survival:
		return FText::Format(LOCTEXT("Survival", "{0}초 생존"), FText::AsNumber(IntervalSeconds));
	case EFAScoreReason::Rescue:
		return FText::Format(LOCTEXT("Rescue", "{0} 구출"), Name);
	case EFAScoreReason::Infection:
		return FText::Format(LOCTEXT("Infection", "{0} 감염"), Name);
	case EFAScoreReason::RoundComplete:
		return LOCTEXT("RoundComplete", "인간 팀 라운드 완주");
	case EFAScoreReason::WipeOut:
		return LOCTEXT("WipeOut", "좀비 팀 전원 감염");
	case EFAScoreReason::StartingZombie:
		return LOCTEXT("StartingZombie", "시작 좀비 보너스");
	default:
		return FText::GetEmpty();
	}
}

#undef LOCTEXT_NAMESPACE
