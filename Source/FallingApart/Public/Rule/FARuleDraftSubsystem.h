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

/** 대기열에 들어가는 드래프트 한 건의 종류. */
UENUM()
enum class EFADraftRequestType : uint8
{
	// 30초 주기 정규 드래프트. 회차(DraftIndex)에 따라 티어 구성이 달라진다.
	Regular,

	// 감염 직후 잃은 인간 룰을 좀비 룰로 메우는 연속 드래프트. 티어 곡선을 타지 않는다.
	Chain
};

/** 아직 선택창을 띄우지 못한 드래프트 한 건. 후보는 실제로 열리는 순간에 뽑는다. */
USTRUCT()
struct FFADraftRequest
{
	GENERATED_BODY()

	UPROPERTY()
	EFADraftRequestType Type = EFADraftRequestType::Regular;

	// 정규 드래프트에서만 쓴다(티어 구성).
	UPROPERTY()
	int32 DraftIndex = 1;
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
	 * 진행 중인 모든 드래프트를 즉시 취소한다. AFAGameMode::EndRound() 가 호출.
	 *
	 * 라운드가 끝나는 세 경우(마지막 1명 감염 · 선택 도중 전원 감염 · 라운드 시간 종료) 모두
	 */
	void CancelAllPendingDrafts();
	
	/**
	 * 클라이언트가 선택을 확정할 때 호출 (AFAPlayerState 의 Server RPC 가 이 함수를 부른다).
	 * @param ForPlayer 확정한 플레이어
	 * @param SelectedIndex 후보 배열 인덱스 (0~2)
	 */
	void ConfirmDraft(AFAPlayerState* ForPlayer, int32 SelectedIndex);
	
private:
	/**
	 * 지금 드래프트를 돌려도 되는 상태인가.
	 * 데스매치·매치 종료 후에는 룰을 새로 쥐여주면 안 된다.
	 */
	bool IsDraftAllowed() const;

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
	
	/**
	 * 드래프트 한 건을 이 플레이어의 대기열 뒤에 붙이고, 지금 열 수 있으면 바로 연다.
	 *
	 * 정규·연속 드래프트가 모두 이 문으로 들어온다 — 선택창을 직접 띄우는 경로를 하나로 모아야
	 * 한쪽이 다른 쪽을 덮어쓰는 일이 없다.
	 */
	void EnqueueDraft(AFAPlayerState* ForPlayer, const FFADraftRequest& Request);

	/**
	 * 대기열의 맨 앞 한 건을 연다. 선택창이 떠 있거나 카드 확인 간격이 남아 있으면 아무 것도 하지 않는다
	 *
	 * 후보는 여기서 뽑는다 — 대기열에 넣을 때가 아니라. 감염으로 진영이 바뀌었어도
	 * 열리는 시점의 진영·보유 룰 기준으로 뽑히게 하기 위함이다.
	 */
	void StartNextQueuedDraft(TWeakObjectPtr<AFAPlayerState> ForPlayer);

	/** 방금 고른 카드를 볼 수 있게 한 박자 둔 뒤 대기열의 다음 건을 연다. */
	void ScheduleNextQueuedDraft(AFAPlayerState* ForPlayer, float Delay);

	/**
	 * 위 간격 타이머의 콜백. QueueTimers 에서 자기 항목을 지운 뒤 다음 건을 연다.
	 *
	 * StartNextQueuedDraft 를 타이머에 직접 걸면 안 된다 — "간격 대기 중인가" 를 QueueTimers
	 * 항목 유무로 판정하기 때문에, 지우는 사람이 없으면 자기 콜백이 자기 가드에 막힌다.
	 */
	void HandleQueueGapElapsed(TWeakObjectPtr<AFAPlayerState> ForPlayer);

	/** 연속 드래프트 후보 풀 — 좀비 진영에서 보유 중이 아닌 카드 중 무작위 최대 3장. */
	TArray<FFADraftCandidate> GenerateChainCandidates(const TArray<FName>& HeldRuleIds) const;

	/** 후보를 보내고 타임아웃 타이머를 거는 공통 처리. 정규·연속 드래프트가 함께 쓴다. */
	void SendCandidatesToPlayer(AFAPlayerState* ForPlayer, const TArray<FFADraftCandidate>& Candidates, float TimeLimit);

	// 플레이어별로 "지금 보낸 후보가 뭐였는지" 기억해두는 표.
	TMap<TWeakObjectPtr<AFAPlayerState>, TArray<FFADraftCandidate>> PendingCandidates;

	// 플레이어별 선택 타임아웃 타이머.
	TMap<TWeakObjectPtr<AFAPlayerState>, FTimerHandle> PendingTimers;

	/**
	 * 아직 열지 못한 드래프트 대기열 (플레이어별 선입선출).
	 *
	 * 연속 드래프트 도중 30초 정기 드래프트가 들어와도 여기 쌓였다가 순서대로 열린다.
	 */
	TMap<TWeakObjectPtr<AFAPlayerState>, TArray<FFADraftRequest>> DraftQueues;

	/** 대기열의 다음 건을 여는 타이머(카드 확인용 간격). 플레이어당 하나. */
	TMap<TWeakObjectPtr<AFAPlayerState>, FTimerHandle> QueueTimers;
};
