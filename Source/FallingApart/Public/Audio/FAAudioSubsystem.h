// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Audio/FASoundTypes.h"
#include "Game/FAGameTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "FAAudioSubsystem.generated.h"

class AFAGameState;
class UAudioComponent;
class USceneComponent;
struct FGameplayTagContainer;

/**
 * 사운드 재생 창구. 프로젝트에서 소리를 내는 길은 여기 하나뿐이다.
 *
 */
UCLASS()
class FALLINGAPART_API UFAAudioSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * 행 설정대로 재생한다. 2D 면 화면 소리로, 3D 면 듣는 사람 위치에서 난다.
	 * 위치가 있는 소리는 PlaySoundAtLocation() / PlaySoundAttached() 를 쓸 것.
	 */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Audio")
	void PlaySound(FName SoundId);

	/** 월드 특정 지점에서 재생. 행이 2D 로 되어 있어도 여기서는 3D 로 낸다. */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Audio")
	void PlaySoundAtLocation(FName SoundId, FVector Location);

	/**
	 * 컴포넌트에 붙여서 재생 — 움직이는 소리(발소리·좀비 그르렁 루프)용.
	 * @return 재생된 AudioComponent. 루프는 이걸 들고 있다가 직접 Stop() 해야 한다. 실패 시 nullptr.
	 */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Audio")
	UAudioComponent* PlaySoundAttached(FName SoundId, USceneComponent* AttachTo);

	/**
	 * 룰 사운드 3단 폴백 — 룰 ID → 계열 → 티어 기본음.
	 *
	 * 룰마다 전용 소리를 만들지 않아도 계열 9개만 있으면 "무슨 종류가 걸렸는지"는 전달된다.
	 * 룰이 100종으로 늘어도 사운드 표는 그대로다.
	 *
	 * @param RuleId   룰 카드 ID (Z23 · H31 …). DT_Sound_Rule 의 RowName 과 같은 키.
	 * @param RuleTags 그 룰의 게임플레이 태그. 계열 판정에 쓴다.
	 * @param Tier     1~3. 계열도 못 찾았을 때 쓸 마지막 폴백.
	 */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Audio")
	void PlayRuleSound(FName RuleId, const FGameplayTagContainer& RuleTags, int32 Tier = 1);

	/** 쿨다운에 걸려 지금은 안 나는 상태인가. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Audio")
	bool IsOnCooldown(FName SoundId) const;

	//~ ---------------- 음악 ----------------
	// BGM 은 한 번에 한 곡만 돈다. 같은 곡을 다시 요청하면 무시하므로 매 프레임 불러도 안전하다.

	/**
	 * BGM 을 바꾼다. 돌고 있던 곡은 페이드 아웃되고 새 곡이 페이드 인된다.
	 * @param SoundId  비우면(NAME_None) 음악을 끈다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Audio|Music")
	void PlayMusic(FName SoundId, float FadeTime = 1.5f);

	UFUNCTION(BlueprintCallable, Category = "FallingApart|Audio|Music")
	void StopMusic(float FadeTime = 1.5f);

	/**
	 * 라운드 막판 구간에 들어섰다는 신호 — BGM 을 최종 곡(BGM_Round_Final)으로 갈아끼운다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Audio|Music")
	void EnterFinalCountdown();

	UFUNCTION(BlueprintPure, Category = "FallingApart|Audio|Music")
	FName GetCurrentMusicId() const { return CurrentMusicId; }

	/**
	 * 베이스 BGM 위에 겹쳐 깔리는 긴장 레이어. 생존자 비율이 떨어질수록 커진다.
	 * 곡을 갈아끼우는 게 아니라 볼륨만 움직이므로 박자가 어긋나지 않는다.
	 * @param TargetVolume 0 이면 사실상 꺼진 상태. 1 이면 표에 적힌 볼륨 그대로.
	 */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Audio|Music")
	void SetMusicLayer(FName SoundId, float TargetVolume, float FadeTime = 2.0f);

	UFUNCTION(BlueprintCallable, Category = "FallingApart|Audio|Music")
	void StopMusicLayer(float FadeTime = 2.0f);

	/** 구역 앰비언스 등 루프를 이 위치에 깐다. AFAAmbienceSource 가 쓴다. */
	UAudioComponent* PlayLoopAtLocation(FName SoundId, FVector Location);

	/** 월드 어디서든 한 줄로 부르기 위한 진입점. 위젯·어빌리티 어디서 불러도 된다. */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Audio", meta = (WorldContext = "WorldContextObject"))
	static void PlayFASound(const UObject* WorldContextObject, FName SoundId);

	static UFAAudioSubsystem* Get(const UObject* WorldContextObject);

protected:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	//~ 라운드 사운드는 GameState 델리게이트에 물려서 난다. GameState 는 클라이언트에 늦게 도착하므로
	//~ 도착할 때까지만 틱을 돌며 기다린다 — 한 번 묶이면 틱이 멈춘다.
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

private:
	/** GameState 가 준비됐으면 델리게이트를 묶는다. 이미 묶였으면 아무것도 하지 않는다. */
	void TryBindGameState();

	UFUNCTION()
	void HandleRoundPhaseChanged(EFARoundPhase NewPhase);

	UFUNCTION()
	void HandleRoundEnded(int32 RoundIndex, EFARoundResult Result);

	UFUNCTION()
	void HandleTeamCountsChanged();

	/** 페이즈에 맞는 BGM 으로 갈아끼운다. */
	void UpdateMusicForPhase(EFARoundPhase Phase);

	/** 생존자 비율로 긴장 레이어 볼륨을 잡는다. */
	void UpdateTensionLayer();

	TWeakObjectPtr<AFAGameState> BoundGameState;

	/**
	 * 위 GameState 에 델리게이트를 걸어둔 상태인가.
	 */
	bool bBoundGameState = false;

	/**
	 * 이 월드에는 AFAGameState 가 없다고 판단한 상태.
	 * 메인 메뉴·로비는 각자 다른 GameState 를 쓰므로, 없는 걸 계속 기다리며 틱을 돌면 안 된다.
	 */
	bool bGaveUpBindingGameState = false;

	/** 이 시간(초)까지 AFAGameState 가 안 나타나면 포기한다. 클라이언트 복제 지연을 넉넉히 감안한 값. */
	static constexpr float BindTimeoutSeconds = 10.0f;

	/** 생존자가 '줄었을' 때만 소리를 내기 위한 직전 값. */
	int32 LastSurvivorCount = -1;

	//~ 음악. 두 컴포넌트를 직접 들고 있어야 페이드·볼륨 조절이 된다.
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> MusicComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> MusicLayerComponent;

	FName CurrentMusicId;
	FName CurrentMusicLayerId;

	/**
	 * 막판 곡으로 갈아끼운 상태. 이 동안에는 긴장 레이어를 얹지 않는다
	 */
	bool bFinalCountdownActive = false;

	/**
	 * 재생 직전 공통 처리 — 행 조회, 음원 로드, 쿨다운 판정, 피치 랜덤.
	 * @return 재생해도 되면 true. 이때 OutSound / OutPitch 가 채워진다.
	 */
	bool ResolveSound(FName SoundId, const FFASoundRow*& OutRow, USoundBase*& OutSound, float& OutPitch);

	/** 소리를 낼 수 있는 상황인가 (전용 서버 · 월드 상태). */
	bool CanPlay() const;

	/**
	 * ID 당 한 번만 경고한다. 애셋이 아직 없는 단계에서 로그가 도배되면 안 된다.
	 * @param bIsMisconfiguration true 면 Warning(잘못 채운 것), false 면 Verbose(아직 안 채운 것).
	 */
	void WarnOnce(FName SoundId, const TCHAR* Reason, bool bIsMisconfiguration = false);

	/** SoundId → 마지막 재생 시각(초). 쿨다운 판정용. */
	TMap<FName, double> LastPlayTimes;

	/** 이미 경고를 남긴 ID. */
	TSet<FName> WarnedSoundIds;
};
