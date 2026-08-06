#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Game/FAGameTypes.h"
#include "FARuleDraftSubsystem.generated.h"

class AFAPlayerState;

USTRUCT(BlueprintType)
struct FFADraftCandidate
{
	GENERATED_BODY()

	UPROPERTY()
	FName RuleId;
};

// 더미 룰 풀의 항목 1개 — 진영·티어 정보 포함. URuleCardData 나오면 이 구조체는 통째로 사라진다.
USTRUCT()
struct FFADummyRuleEntry
{
	GENERATED_BODY()

	UPROPERTY()
	FName RuleId;

	UPROPERTY()
	EFATeam Team = EFATeam::None;

	// 1~3. 숫자가 클수록 강한 룰
	UPROPERTY()
	int32 Tier = 1;
};

/*
 * 30초 주기 룰 드래프트와, 감염 시 연속 드래프트를 담당하는 서버 권한 서브시스템.
 * AFAGameMode 가 라운드 진행 중 이 서브시스템의 함수를 호출.
 */
UCLASS()
class FALLINGAPART_API UFARuleDraftSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// 30초 주기 정규 드래프트. GameMode::OnDraftTimer() 가 호출.	
	void StartDraftForAllPlayers(int32 DraftIndex);

	// 감염 시 연속 드래프트. GameMode::HandleInfection() 이 호출.
	void StartChainDraft(AFAPlayerState* Victim, int32 RemovedRuleCount);
	
	/**
	 * 클라이언트가 선택을 확정할 때 호출 (AFAPlayerState 의 Server RPC 가 이 함수를 부른다).
	 * @param ForPlayer 확정한 플레이어
	 * @param SelectedIndex 후보 배열 인덱스 (0~2)
	 */
	void ConfirmDraft(AFAPlayerState* ForPlayer, int32 SelectedIndex);
	
private:
	// 후보 3장을 뽑는다. 지금은 더미 룰 풀에서 무작위로 뽑기만 한다.
	TArray<FFADraftCandidate> GenerateCandidates(EFATeam ForTeam, int32 DraftIndex,int32 Count = 3) const;

	// 임시 룰 풀. GAS 담당자의 URuleCardData 가 나오면 이 함수를 통째로 교체한다.
	TArray<FFADummyRuleEntry> GetDummyRulePool() const;

	// 이번 드래프트 회차(1~6)의 후보 3장 각각의 티어 구성. GDD 4.2 티어 곡선(밸런싱 워크북 1_드래프트곡선 시트).
	TArray<int32> GetTierComposition(int32 DraftIndex) const;
	
	// 타임아웃 시 서버가 대신 무작위로 확정한다.
	void HandleDraftTimeout(TWeakObjectPtr<AFAPlayerState> ForPlayer);

	// 최종 확정 처리 — 정상 확정과 타임아웃 확정이 공유하는 공통 로직.
	void ApplyDraftResult(AFAPlayerState* ForPlayer, const FFADraftCandidate& Chosen);

	// 플레이어별로 "지금 보낸 후보가 뭐였는지" 기억해두는 표.
	TMap<TWeakObjectPtr<AFAPlayerState>, TArray<FFADraftCandidate>> PendingCandidates;

	// 플레이어별 10초 타임아웃 타이머.
	TMap<TWeakObjectPtr<AFAPlayerState>, FTimerHandle> PendingTimers;
};
