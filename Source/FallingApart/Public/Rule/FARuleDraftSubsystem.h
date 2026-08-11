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
	/**
	 * 후보 3장을 뽑는다. 지금은 티어 구성만 맞추고 가중치·ConflictTags 는 아직 안 본다.
	 * @param ExcludeRuleIds 후보에서 뺄 카드 — 이미 보유 중인 룰. 뽑아봐야 적용이 거부돼 빈 선택이 된다.
	 */
	TArray<FFADraftCandidate> GenerateCandidates(EFATeam ForTeam, int32 DraftIndex, const TArray<FName>& ExcludeRuleIds) const;

	/**
	 * 후보를 뽑을 룰 풀.
	 * 프로젝트 설정에 DT_RuleCards 가 물려 있으면 그 표를 쓰고, 없으면 아래 더미 풀로 폴백한다.
	 * (표가 채워지는 속도와 무관하게 드래프트 루프를 계속 테스트할 수 있어야 한다)
	 */
	TArray<FFADummyRuleEntry> GetRulePool() const;

	// DT 가 아직 없을 때 쓰는 임시 풀. 표가 14종을 채우면 지워도 된다.
	TArray<FFADummyRuleEntry> GetDummyRulePool() const;

	// 이번 드래프트 회차(1~6)의 후보 3장 각각의 티어 구성. GDD 4.2 티어 곡선(밸런싱 워크북 1_드래프트곡선 시트).
	TArray<int32> GetTierComposition(int32 DraftIndex) const;
	
	// 타임아웃 시 서버가 대신 무작위로 확정한다.
	void HandleDraftTimeout(TWeakObjectPtr<AFAPlayerState> ForPlayer);

	// 최종 확정 처리 — 정상 확정과 타임아웃 확정이 공유하는 공통 로직.
	void ApplyDraftResult(AFAPlayerState* ForPlayer, const FFADraftCandidate& Chosen);
	
	// 연속 드래프트 1장을 즉시 적용하고, 남은 장수가 있으면 6초 뒤 자신을 다시 예약한다.
	void ProcessNextChainDraft(TWeakObjectPtr<AFAPlayerState> ForPlayer, int32 RemainingCount);

	// 플레이어별로 "지금 보낸 후보가 뭐였는지" 기억해두는 표.
	TMap<TWeakObjectPtr<AFAPlayerState>, TArray<FFADraftCandidate>> PendingCandidates;

	// 플레이어별 10초 타임아웃 타이머.
	TMap<TWeakObjectPtr<AFAPlayerState>, FTimerHandle> PendingTimers;
};
