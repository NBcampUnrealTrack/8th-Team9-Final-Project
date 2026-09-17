#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FAHearingSubsystem.generated.h"

class UAudioComponent;
class APawn;

/** [Z22 귀먹음 추가] 재생 코드와 분리한 로컬 청취 제어. 로컬 플레이어 1명 기준. */
UCLASS()
class FALLINGAPART_API UFAHearingSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UFAHearingSubsystem* Get(const UObject* WorldContextObject);
	bool IsDeaf() const;
	// [Z22 귀먹음] 별도 에셋 없이 발소리 분류와 재생 컴포넌트 추적을 재사용한다.
	static bool IsFootstepSound(FName SoundId);
	UAudioComponent* TrackFootstep(FName SoundId, UAudioComponent* Component);
	// 음성 코드는 청취 배수만 사용하도록 분리한다.
	float GetVoiceHearingMultiplier() const;
	// [Z22 귀먹음] Status의 HandleDeafRuleChanged에서 호출하는 오디오 적용 함수.
	void UpdateHearingState();
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

protected:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

private:
	void StopActiveFootsteps();
	// 태그 변경은 Status 이벤트로 처리하고, Tick은 Pawn 교체와 완료된 소리 정리를 맡는다.
	TWeakObjectPtr<APawn> ListenerPawn;

	// 단발 사운드의 자동 파괴를 방해하지 않도록 약한 참조로 기록한다.
	TArray<TWeakObjectPtr<UAudioComponent>> ActiveFootsteps;

	bool bHasHearingState = false;
	bool bWasDeaf = false;
};
