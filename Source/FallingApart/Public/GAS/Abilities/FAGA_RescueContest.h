#pragma once

#include "CoreMinimal.h"
#include "GAS/FAGameplayAbility.h"
#include "Game/FAGameTypes.h"
#include "FAGA_RescueContest.generated.h"

class AFACharacter;
class UGameplayEffect;

/**
 * 좀비 서버에서 1개만 실행되어 3명(좀비, 인질, 구조자)의 줄다리기를 통제하는 어빌리티.
 * 게이지 0/100 도달 또는 3초 타임아웃 시 승패를 판정한다 (설계문서 §3.3).
 */
UCLASS()
class FALLINGAPART_API UFAGA_RescueContest : public UFAGameplayAbility
{
	GENERATED_BODY()

public:
	UFAGA_RescueContest();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** 좀비 ASC 에서 지금 돌고 있는 줄다리기 인스턴스. 없으면 nullptr. */
	static UFAGA_RescueContest* FindActiveInstance(UAbilitySystemComponent* ASC);

	/** 연타 1회 집계 — 밸런스 확인용 로그에만 쓴다. */
	void RecordMash(EFAContestRole Role);

	/**
	 * 이번 QTE 에서 좀비 연타 1회에 얹을 추가 가중치. 0 이거나 음수다.
	 *
	 * 같은 인질을 잡았다가 구출당하기를 반복하면 한 번당 RepeatRescueMashStep 만큼 무거워진다.
	 * 상대가 바뀌면 0 으로 돌아간다 — 그래서 첫 대결은 언제나 −2.5 대 3 이다.
	 */
	float GetZombieMashBonus() const { return ZombieMashBonus; }

protected:
	/** QTE 중 부여할 상태 (State.Contesting + 이동/행동 불가) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Effects")
	TSubclassOf<UGameplayEffect> ContestingEffectClass;

	/** 인간 승리 시 좀비에게 적용할 기절 GE (기본 2초 / Z30 룰 적용 시 8초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Effects")
	TSubclassOf<UGameplayEffect> ZombieStunEffectClass;

	/** 줄다리기 제한 시간(초). 이 시간이 지나면 게이지가 50 을 넘었는지로 승패를 가른다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Contest")
	float ContestDuration = 3.f;

	/**
	 * 같은 인질을 한 번 더 구출당할 때마다 좀비 연타에 더 얹는 양(절댓값).
	 *
	 * −2.5 대 3 으로 시작해서 그 인질이 구출될 때마다 −3, −3.5 … 로 좀비가 유리해진다.
	 * 같은 사람을 붙잡고 늘어지는 무한 구출 고리를 끊기 위한 장치다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Contest", meta = (ClampMin = "0"))
	float RepeatRescueMashStep = 0.5f;

private:
	//~ GAS Tasks 콜백
	UFUNCTION()
	void OnHumanWin();

	UFUNCTION()
	void OnZombieWin();

	UFUNCTION()
	void OnTimeOut();

	/** 대결 결과 확정 및 처리 */
	void ResolveContestOutcome(bool bHumanWon);

	/**
	 * 줄다리기 게이지가 움직일 때마다 참가자 3명의 화면으로 흘려보낸다.
	 */
	void HandleContestGaugeChanged(const struct FOnAttributeChangeData& Data);

	/**
	 * 참가자 전원에게 줄다리기 진행도를 써 넣는다.
	 *
	 * 포획 게이지(잡힌 인간 · 좀비)와 QTE 상태(3명 전원) 양쪽에 같이 넣는다.
	 * 구조자는 포획 상태가 아예 없어서 QTE 상태 쪽에만 값이 들어간다.
	 */
	void BroadcastResistRatio(float Ratio);

private:
	UPROPERTY()
	TWeakObjectPtr<AFACharacter> TargetZombie;

	UPROPERTY()
	TWeakObjectPtr<AFACharacter> Rescuer;

	UPROPERTY()
	TWeakObjectPtr<AFACharacter> GrabbedVictim;

	/** 어빌리티 종료 시 상태 일괄 해제를 위한 GE 핸들 맵 */
	TMap<AFACharacter*, FActiveGameplayEffectHandle> ContestingHandles;

	/** ContestGauge 변경 구독. EndAbility 에서 반드시 해제한다. */
	FDelegateHandle ContestGaugeChangedHandle;

	/**
	 * 승패를 이미 확정했는가.
	 * 완료 경로가 게이지 100 · 게이지 0 · 3초 타임아웃 세 갈래라 같은 프레임에 둘이 겹칠 수 있고,
	 * 그러면 구출 점수가 두 번 붙는다.
	 */
	bool bOutcomeResolved = false;

	/**
	 * 연속 구출 누적을 세고 있는 인질.
	 *
	 * InstancedPerActor 라 이 좀비 몫으로 QTE 사이에도 값이 남는다.
	 * 다른 사람을 잡으면 여기가 갈리면서 누적도 0 부터 다시 센다.
	 */
	TWeakObjectPtr<AFACharacter> RepeatRescueVictim;

	/** 위 인질이 이 좀비에게서 구출된 횟수. */
	int32 RepeatRescueCount = 0;

	/** 이번 QTE 에 적용 중인 좀비 연타 보정(0 이하). ActivateAbility 에서 한 번만 계산한다. */
	float ZombieMashBonus = 0.0f;

	//~ 밸런스 확인용 집계. 판정이 끝날 때 한 줄로 로그에 남긴다.
	float ContestStartTime = 0.0f;
	int32 ZombieMashCount = 0;
	int32 VictimMashCount = 0;
	int32 RescuerMashCount = 0;
};
