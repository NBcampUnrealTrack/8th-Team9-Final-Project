// Fill out your copyright notice in the Description page of Project Settings.

#include "Audio/FAAudioSubsystem.h"

#include "Audio/FAAudioSettings.h"
#include "Audio/FAHearingSubsystem.h" // [Z22 귀먹음] 청취 판정과 발소리 추적을 공통 클래스로 위임한다.
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Game/FAGameState.h"
#include "Game/FALobbyGameState.h"
#include "GameplayTagContainer.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundConcurrency.h"

namespace FAAudioIds
{
	// [Z22 귀먹음] 생성한 소리를 반환하면서 발소리만 추적한다. 다른 소리는 그대로 통과한다.
	UAudioComponent* TrackFootstep(const UObject* Context, FName SoundId, UAudioComponent* Component)
	{
		if (UFAHearingSubsystem* Hearing = UFAHearingSubsystem::Get(Context))
		{
			return Hearing->TrackFootstep(SoundId, Component);
		}
		return Component;
	}

	// 라운드 진행 사운드. 표의 RowName 과 같아야 한다 (DT_Sound_Game).
	static const FName RoundCountdown(TEXT("Round_Countdown"));
	static const FName RoundStart(TEXT("Round_Start"));
	static const FName ResultRoundOpen(TEXT("Result_RoundOpen"));
	static const FName DeathMatchIntro(TEXT("DM_Intro"));
	static const FName RoundEndHumanWin(TEXT("Round_End_HumanWin"));
	static const FName RoundEndZombieWin(TEXT("Round_End_ZombieWin"));
	static const FName SurvivorDown(TEXT("Survivor_Down"));

	// 음악 (DT_Sound_Music).
	static const FName BgmLobby(TEXT("BGM_Lobby"));
	static const FName BgmRoundBase(TEXT("BGM_Round_Base"));
	static const FName BgmRoundTension(TEXT("BGM_Round_Tension"));
	static const FName BgmRoundFinal(TEXT("BGM_Round_Final"));
	static const FName BgmRoundResult(TEXT("BGM_RoundResult"));
	static const FName BgmMatchResult(TEXT("BGM_MatchResult"));
	static const FName BgmDeathMatch(TEXT("BGM_DeathMatch"));
	static const FName StingLastHuman(TEXT("Sting_LastHuman"));

	/** 생존자가 이 비율 아래로 떨어지면 긴장 레이어가 최대가 된다. */
	static constexpr float TensionFullRatio = 0.25f;

	/** 막판 곡으로 넘어갈 때의 크로스페이드(초). */
	static constexpr float FinalMusicFadeSeconds = 2.0f;
}

bool UFAAudioSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	// 전용 서버는 소리를 낼 일이 없다.
	const UWorld* World = Cast<UWorld>(Outer);
	return !World || World->GetNetMode() != NM_DedicatedServer;
}

void UFAAudioSubsystem::Deinitialize()
{
	if (AFAGameState* GameState = BoundGameState.Get())
	{
		GameState->OnRoundPhaseChanged.RemoveAll(this);
		GameState->OnRoundEnded.RemoveAll(this);
		GameState->OnTeamCountsChanged.RemoveAll(this);
	}
	BoundGameState.Reset();
	bBoundGameState = false;

	// bAutoDestroy 를 꺼둔 컴포넌트라 직접 정리해야 한다.
	StopMusic(0.0f);

	Super::Deinitialize();
}

//~ ---------------- 라운드 사운드 바인딩 ----------------

bool UFAAudioSubsystem::IsTickable() const
{
	if (IsTemplate())
	{
		return false;
	}

	// GameState 를 잡을 때까지, 그리고 긴장 레이어가 살아 있는 동안만 돈다.
	// 메뉴·로비처럼 AFAGameState 가 아예 없는 레벨에서는 포기하고 틱을 끈다.
	const bool bWaitingForGameState = !bBoundGameState && !bGaveUpBindingGameState;
	return bWaitingForGameState || MusicLayerComponent != nullptr;
}

TStatId UFAAudioSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFAAudioSubsystem, STATGROUP_Tickables);
}

void UFAAudioSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TryBindGameState();
	UpdateTensionLayer();
}

void UFAAudioSubsystem::TryBindGameState()
{
	UWorld* World = GetWorld();
	if (bBoundGameState || bGaveUpBindingGameState || !World)
	{
		return;
	}

	// 정리에 들어간 월드에는 걸어봐야 곧 같이 사라진다. 이 프레임의 GameState 는 이미
	// 파괴 예약된 상태로 돌아오므로 여기서 끊어야 한다.
	if (World->bIsTearingDown)
	{
		return;
	}

	if (AFAGameState* GameState = World->GetGameState<AFAGameState>())
	{
		GameState->OnRoundPhaseChanged.AddDynamic(this, &UFAAudioSubsystem::HandleRoundPhaseChanged);
		GameState->OnRoundEnded.AddDynamic(this, &UFAAudioSubsystem::HandleRoundEnded);
		GameState->OnTeamCountsChanged.AddDynamic(this, &UFAAudioSubsystem::HandleTeamCountsChanged);

		BoundGameState = GameState;
		bBoundGameState = true;
		LastSurvivorCount = GameState->GetSurvivorCount();

		// 늦게 들어온 클라이언트는 페이즈 전환 이벤트를 이미 놓쳤다 — 현재 페이즈로 음악을 맞춘다.
		UpdateMusicForPhase(GameState->GetRoundPhase());
		return;
	}

	// 로비는 AFAGameState 가 아니라 AFALobbyGameState 를 쓴다 — 라운드 페이즈라는 개념이 없으므로
	// 여기서 로비 BGM 만 걸고 더는 기다리지 않는다.
	if (World->GetGameState<AFALobbyGameState>())
	{
		PlayMusic(FAAudioIds::BgmLobby);
		bGaveUpBindingGameState = true;
		return;
	}

	// 클라이언트는 GameState 가 복제돼 오기까지 몇 프레임 걸린다. 다만 메인 메뉴처럼 아예 없는
	// 레벨도 있으므로 무한정 기다리지는 않는다.
	if (World->GetTimeSeconds() > BindTimeoutSeconds)
	{
		bGaveUpBindingGameState = true;
	}
}

//~ ---------------- 음악 ----------------

void UFAAudioSubsystem::PlayMusic(FName SoundId, float FadeTime)
{
	if (!CanPlay() || CurrentMusicId == SoundId)
	{
		// 같은 곡 재요청은 무시한다 — 페이즈 이벤트가 두 번 와도 곡이 처음부터 다시 시작되면 안 된다.
		return;
	}

	StopMusic(FadeTime);

	CurrentMusicId = SoundId;
	if (SoundId.IsNone())
	{
		return;
	}

	const FFASoundRow* Row = nullptr;
	USoundBase* Sound = nullptr;
	float Pitch = 1.0f;
	if (!ResolveSound(SoundId, Row, Sound, Pitch))
	{
		CurrentMusicId = NAME_None;
		return;
	}

	// bAutoDestroy = false — 페이드 아웃과 볼륨 조절을 하려면 컴포넌트를 계속 들고 있어야 한다.
	MusicComponent = UGameplayStatics::SpawnSound2D(GetWorld(), Sound, Row->VolumeMultiplier, Pitch, 0.0f,
		Row->Concurrency.LoadSynchronous(), /*bPersistAcrossLevelTransition=*/false, /*bAutoDestroy=*/false);

	if (MusicComponent)
	{
		MusicComponent->FadeIn(FMath::Max(0.0f, FadeTime), Row->VolumeMultiplier);
	}
}

void UFAAudioSubsystem::EnterFinalCountdown()
{
	if (bFinalCountdownActive || !CanPlay())
	{
		return;
	}

	const FFASoundRow* Row = UFAAudioSettings::FindSound(FAAudioIds::BgmRoundFinal);
	if (!Row || Row->Sound.IsNull())
	{
		return;
	}

	bFinalCountdownActive = true;

	PlayMusic(FAAudioIds::BgmRoundFinal, FAAudioIds::FinalMusicFadeSeconds);
}

void UFAAudioSubsystem::StopMusic(float FadeTime)
{
	if (MusicComponent)
	{
		MusicComponent->FadeOut(FMath::Max(0.0f, FadeTime), 0.0f);
		MusicComponent = nullptr;
	}
	CurrentMusicId = NAME_None;

	StopMusicLayer(FadeTime);
}

void UFAAudioSubsystem::SetMusicLayer(FName SoundId, float TargetVolume, float FadeTime)
{
	if (!CanPlay() || SoundId.IsNone())
	{
		return;
	}

	const FFASoundRow* Row = UFAAudioSettings::FindSound(SoundId);
	if (!Row)
	{
		return;
	}

	const float Volume = FMath::Clamp(TargetVolume, 0.0f, 1.0f) * Row->VolumeMultiplier;

	// 아직 없으면 0 볼륨으로 깔아둔다. 베이스와 같은 시점에 시작해야 박자가 맞는다.
	if (!MusicLayerComponent || CurrentMusicLayerId != SoundId)
	{
		StopMusicLayer(0.5f);

		USoundBase* Sound = Row->Sound.LoadSynchronous();
		if (!Sound)
		{
			return;
		}

		MusicLayerComponent = UGameplayStatics::SpawnSound2D(GetWorld(), Sound, 0.0f, 1.0f, 0.0f,
			Row->Concurrency.LoadSynchronous(), false, /*bAutoDestroy=*/false);

		CurrentMusicLayerId = MusicLayerComponent ? SoundId : NAME_None;
	}

	if (MusicLayerComponent)
	{
		MusicLayerComponent->AdjustVolume(FMath::Max(0.0f, FadeTime), Volume);
	}
}

void UFAAudioSubsystem::StopMusicLayer(float FadeTime)
{
	if (MusicLayerComponent)
	{
		MusicLayerComponent->FadeOut(FMath::Max(0.0f, FadeTime), 0.0f);
		MusicLayerComponent = nullptr;
	}
	CurrentMusicLayerId = NAME_None;
}

UAudioComponent* UFAAudioSubsystem::PlayLoopAtLocation(FName SoundId, FVector Location)
{
	const FFASoundRow* Row = nullptr;
	USoundBase* Sound = nullptr;
	float Pitch = 1.0f;

	if (!ResolveSound(SoundId, Row, Sound, Pitch))
	{
		return nullptr;
	}

	USoundAttenuation* Attenuation = Row->Attenuation.IsNull()
		? UFAAudioSettings::GetDefaultAttenuation(Row->Category)
		: Row->Attenuation.LoadSynchronous();

	if (Row->bIs2D)
	{
		return FAAudioIds::TrackFootstep(this, SoundId, UGameplayStatics::SpawnSound2D(GetWorld(), Sound, Row->VolumeMultiplier, Pitch, 0.0f,
			Row->Concurrency.LoadSynchronous(), false, /*bAutoDestroy=*/false));
	}

	return FAAudioIds::TrackFootstep(this, SoundId, UGameplayStatics::SpawnSoundAtLocation(GetWorld(), Sound, Location, FRotator::ZeroRotator,
		Row->VolumeMultiplier, Pitch, 0.0f, Attenuation, Row->Concurrency.LoadSynchronous(), /*bAutoDestroy=*/false));
}

void UFAAudioSubsystem::UpdateMusicForPhase(EFARoundPhase Phase)
{
	bFinalCountdownActive = false;

	switch (Phase)
	{
	// 로비가 별도 레벨로 분리된 뒤로, 매치 레벨의 WaitingToStart 는 '인원 차기를 기다리는 몇 초'다.
	// 여기서 로비 BGM 을 틀면 1초 뒤에 곧바로 갈아끼워야 하므로 처음부터 매치 곡으로 간다.
	case EFARoundPhase::WaitingToStart:
	case EFARoundPhase::RoundStarting:
	case EFARoundPhase::InProgress:
		PlayMusic(FAAudioIds::BgmRoundBase);
		break;

	case EFARoundPhase::RoundEnd:
		PlayMusic(FAAudioIds::BgmRoundResult);
		break;

	case EFARoundPhase::MatchEnd:
		PlayMusic(FAAudioIds::BgmMatchResult);
		break;

	case EFARoundPhase::DeathMatch:
		PlayMusic(FAAudioIds::BgmDeathMatch);
		break;

	default:
		break;
	}
}

void UFAAudioSubsystem::UpdateTensionLayer()
{
	const AFAGameState* GameState = BoundGameState.Get();
	if (!GameState)
	{
		return;
	}

	// 긴장 레이어는 라운드가 도는 동안에만 의미가 있다.
	if (GameState->GetRoundPhase() != EFARoundPhase::InProgress)
	{
		StopMusicLayer();
		return;
	}

	if (bFinalCountdownActive)
	{
		StopMusicLayer();
		return;
	}

	const int32 Survivors = GameState->GetSurvivorCount();
	const int32 Total = Survivors + GameState->GetZombieCount();
	if (Total <= 0)
	{
		return;
	}

	// 생존자가 줄수록 1 에 가까워진다. 25% 이하면 최대.
	const float Ratio = static_cast<float>(Survivors) / static_cast<float>(Total);
	const float Tension = 1.0f - FMath::GetMappedRangeValueClamped(
		FVector2f(FAAudioIds::TensionFullRatio, 1.0f), FVector2f(0.0f, 1.0f), Ratio);

	SetMusicLayer(FAAudioIds::BgmRoundTension, Tension);
}

void UFAAudioSubsystem::HandleRoundPhaseChanged(EFARoundPhase NewPhase)
{
	UpdateMusicForPhase(NewPhase);

	switch (NewPhase)
	{
	case EFARoundPhase::RoundStarting:
		PlaySound(FAAudioIds::RoundCountdown);
		break;

	case EFARoundPhase::InProgress:
		PlaySound(FAAudioIds::RoundStart);
		break;

	case EFARoundPhase::RoundEnd:
		PlaySound(FAAudioIds::ResultRoundOpen);
		break;

	case EFARoundPhase::DeathMatch:
		PlaySound(FAAudioIds::DeathMatchIntro);
		break;

	default:
		// MatchEnd 는 최종 순위표 위젯이 한 줄씩 틱을 내므로 여기서는 소리를 내지 않는다.
		break;
	}

	// 긴장 레이어는 컴포넌트가 살아 있는 동안에만 틱이 돈다 — 라운드에 들어가는 이 시점에
	// 한 번 깔아두지 않으면 첫 감염이 일어날 때까지 아예 시작되지 않는다.
	UpdateTensionLayer();
}

void UFAAudioSubsystem::HandleRoundEnded(int32 RoundIndex, EFARoundResult Result)
{
	if (Result == EFARoundResult::HumanWin)
	{
		PlaySound(FAAudioIds::RoundEndHumanWin);
	}
	else if (Result == EFARoundResult::ZombieWin)
	{
		PlaySound(FAAudioIds::RoundEndZombieWin);
	}
}

void UFAAudioSubsystem::HandleTeamCountsChanged()
{
	const AFAGameState* GameState = BoundGameState.Get();
	if (!GameState)
	{
		return;
	}

	const int32 SurvivorCount = GameState->GetSurvivorCount();

	// 라운드 시작 때는 0 → N 으로 늘어난다. 줄어들 때만 소리를 낸다.
	if (LastSurvivorCount >= 0 && SurvivorCount < LastSurvivorCount)
	{
		PlaySound(FAAudioIds::SurvivorDown);

		// 마지막 1명이 남는 순간은 별도 스팅어로 때린다.
		if (SurvivorCount == 1)
		{
			PlaySound(FAAudioIds::StingLastHuman);
		}
	}

	LastSurvivorCount = SurvivorCount;

	UpdateTensionLayer();
}

UFAAudioSubsystem* UFAAudioSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;

	return World ? World->GetSubsystem<UFAAudioSubsystem>() : nullptr;
}

void UFAAudioSubsystem::PlayFASound(const UObject* WorldContextObject, FName SoundId)
{
	if (UFAAudioSubsystem* Audio = Get(WorldContextObject))
	{
		Audio->PlaySound(SoundId);
	}
}

bool UFAAudioSubsystem::CanPlay() const
{
	const UWorld* World = GetWorld();
	return World != nullptr && World->GetNetMode() != NM_DedicatedServer;
}

bool UFAAudioSubsystem::IsOnCooldown(FName SoundId) const
{
	const FFASoundRow* Row = UFAAudioSettings::FindSound(SoundId);
	if (!Row || Row->RetriggerCooldown <= 0.0f)
	{
		return false;
	}

	const double* LastTime = LastPlayTimes.Find(SoundId);
	if (!LastTime)
	{
		return false;
	}

	return (FPlatformTime::Seconds() - *LastTime) < Row->RetriggerCooldown;
}

void UFAAudioSubsystem::WarnOnce(FName SoundId, const TCHAR* Reason, bool bIsMisconfiguration)
{
	if (WarnedSoundIds.Contains(SoundId))
	{
		return;
	}
	WarnedSoundIds.Add(SoundId);

	if (bIsMisconfiguration)
	{
		// 실제로 잘못 채워둔 것. 기본 로그 레벨에서 보여야 한다.
		UE_LOG(LogTemp, Warning, TEXT("[Audio] '%s' — %s"), *SoundId.ToString(), Reason);
		return;
	}

	// 아직 안 채운 행은 개발 중 정상 상태다. 이 로그가 "남은 목록" 노릇을 한다.
	UE_LOG(LogTemp, Verbose, TEXT("[Audio] '%s' 를 재생하지 못했다 — %s"), *SoundId.ToString(), Reason);
}

bool UFAAudioSubsystem::ResolveSound(FName SoundId, const FFASoundRow*& OutRow, USoundBase*& OutSound, float& OutPitch)
{
	OutRow = nullptr;
	OutSound = nullptr;
	OutPitch = 1.0f;

	// [Z22 귀먹음] 발소리 요청만 차단한다. 음악·UI·일반 효과음은 기존 흐름을 유지한다.
	if (UFAHearingSubsystem::IsFootstepSound(SoundId))
	{
		const UFAHearingSubsystem* Hearing = UFAHearingSubsystem::Get(this);
		if (Hearing && Hearing->ShouldMuteSound(SoundId))
		{
			return false;
		}
	}

	if (!CanPlay() || SoundId.IsNone())
	{
		return false;
	}

	OutRow = UFAAudioSettings::FindSound(SoundId);
	if (!OutRow)
	{
		WarnOnce(SoundId, TEXT("표에 행이 없다 (DT_Sound_* 확인)"));
		return false;
	}

	if (OutRow->Sound.IsNull())
	{
		// 행은 있는데 음원을 아직 안 붙인 상태. 정상적인 중간 단계다.
		WarnOnce(SoundId, TEXT("행은 있으나 Sound 가 비어 있다"));
		return false;
	}

	// 쿨다운은 행을 찾은 뒤에 본다 — 존재하지 않는 ID 가 쿨다운 표를 채우면 안 된다.
	const double Now = FPlatformTime::Seconds();
	if (OutRow->RetriggerCooldown > 0.0f)
	{
		if (const double* LastTime = LastPlayTimes.Find(SoundId))
		{
			if ((Now - *LastTime) < OutRow->RetriggerCooldown)
			{
				return false;
			}
		}
	}

	OutSound = OutRow->Sound.LoadSynchronous();
	if (!OutSound)
	{
		WarnOnce(SoundId, TEXT("음원 로드 실패"));
		return false;
	}

	// 표의 bLooping 은 '자동 파괴하지 말라'는 뜻일 뿐, 실제 반복은 애셋이 한다.
	// 둘이 어긋나면 앰비언스가 한 번 울리고 조용해지는데 원인 찾기가 까다로우므로 여기서 짚어준다.
	if (OutRow->bLooping && !OutSound->IsLooping())
	{
		WarnOnce(SoundId, TEXT("표는 bLooping 인데 음원이 루프가 아니다 — Sound Wave 나 Cue 에서 Looping 을 켤 것"),
			/*bIsMisconfiguration=*/true);
	}

	LastPlayTimes.Add(SoundId, Now);

	// 같은 파일이 반복돼도 기계음처럼 들리지 않게 하는 유일한 장치다.
	OutPitch = OutRow->PitchMultiplier;
	if (OutRow->PitchRandomRange > 0.0f)
	{
		OutPitch += FMath::FRandRange(-OutRow->PitchRandomRange, OutRow->PitchRandomRange);
	}
	OutPitch = FMath::Max(0.01f, OutPitch);

	return true;
}

void UFAAudioSubsystem::PlaySound(FName SoundId)
{
	const FFASoundRow* Row = nullptr;
	USoundBase* Sound = nullptr;
	float Pitch = 1.0f;

	if (!ResolveSound(SoundId, Row, Sound, Pitch))
	{
		return;
	}

	if (!Row->bIs2D)
	{
		// 3D 인데 위치를 안 줬다. 듣는 사람 위치에서 내면 거리감이 사라지므로,
		// 위치가 있는 소리는 PlaySoundAtLocation / PlaySoundAttached 를 쓰라고 알린다.
		WarnOnce(SoundId, TEXT("3D 사운드를 위치 없이 불렀다 — PlaySoundAtLocation 을 쓸 것"));
	}

	// [Z22 귀먹음] 적용 전에 시작한 발소리도 멈출 수 있도록 컴포넌트를 얻어 기록한다.
	if (UFAHearingSubsystem::IsFootstepSound(SoundId))
	{
		FAAudioIds::TrackFootstep(this, SoundId, UGameplayStatics::SpawnSound2D(GetWorld(), Sound,
			Row->VolumeMultiplier, Pitch, 0.0f, Row->Concurrency.LoadSynchronous()));
		return;
	}
	UGameplayStatics::PlaySound2D(GetWorld(), Sound, Row->VolumeMultiplier, Pitch, 0.0f,
		Row->Concurrency.LoadSynchronous(), /*OwningActor=*/nullptr, /*bIsUISound=*/true);
}

void UFAAudioSubsystem::PlaySoundAtLocation(FName SoundId, FVector Location, float VolumeMultiplier)
{
	const FFASoundRow* Row = nullptr;
	USoundBase* Sound = nullptr;
	float Pitch = 1.0f;

	if (!ResolveSound(SoundId, Row, Sound, Pitch))
	{
		return;
	}

	USoundAttenuation* Attenuation = Row->Attenuation.IsNull()
		? UFAAudioSettings::GetDefaultAttenuation(Row->Category)
		: Row->Attenuation.LoadSynchronous();

	// [Z22 귀먹음] 일반·환청·사다리 발소리를 같은 경로로 추적한다.
	if (UFAHearingSubsystem::IsFootstepSound(SoundId))
	{
		FAAudioIds::TrackFootstep(this, SoundId, UGameplayStatics::SpawnSoundAtLocation(GetWorld(), Sound,
			Location, FRotator::ZeroRotator, Row->VolumeMultiplier * FMath::Max(0.0f, VolumeMultiplier),
			Pitch, 0.0f, Attenuation, Row->Concurrency.LoadSynchronous()));
		return;
	}
	UGameplayStatics::PlaySoundAtLocation(GetWorld(), Sound, Location, FRotator::ZeroRotator,
		Row->VolumeMultiplier * FMath::Max(0.0f, VolumeMultiplier), Pitch, 0.0f, Attenuation, Row->Concurrency.LoadSynchronous());
}

UAudioComponent* UFAAudioSubsystem::PlaySoundAttached(FName SoundId, USceneComponent* AttachTo)
{
	if (!AttachTo)
	{
		return nullptr;
	}

	const FFASoundRow* Row = nullptr;
	USoundBase* Sound = nullptr;
	float Pitch = 1.0f;

	if (!ResolveSound(SoundId, Row, Sound, Pitch))
	{
		return nullptr;
	}

	USoundAttenuation* Attenuation = Row->Attenuation.IsNull()
		? UFAAudioSettings::GetDefaultAttenuation(Row->Category)
		: Row->Attenuation.LoadSynchronous();

	// 루프는 호출부가 직접 멈춰야 하므로 자동 파괴를 끈다. 단발은 끝나면 알아서 사라진다.
	// [Z22 귀먹음] 부착 재생도 동일한 추적 함수를 재사용한다.
	return FAAudioIds::TrackFootstep(this, SoundId, UGameplayStatics::SpawnSoundAttached(Sound, AttachTo, NAME_None, FVector::ZeroVector,
		EAttachLocation::KeepRelativeOffset, Row->bStopOnOwnerDestroy, Row->VolumeMultiplier, Pitch, 0.0f,
		Attenuation, Row->Concurrency.LoadSynchronous(), /*bAutoDestroy=*/!Row->bLooping));
}

void UFAAudioSubsystem::PlayRuleSound(FName RuleId, const FGameplayTagContainer& RuleTags, int32 Tier)
{
	if (!CanPlay())
	{
		return;
	}

	// 1단계 — 이 룰 전용 소리.
	if (UFAAudioSettings::FindSound(RuleId))
	{
		PlaySound(RuleId);
		return;
	}

	// 2단계 — 계열 소리. 화면을 안 봐도 "무슨 종류가 걸렸는지"는 전달된다.
	const FName SeriesSoundId = UFAAudioSettings::FindRuleSeriesSoundId(RuleTags);
	if (!SeriesSoundId.IsNone() && UFAAudioSettings::FindSound(SeriesSoundId))
	{
		PlaySound(SeriesSoundId);
		return;
	}

	// 3단계 — 티어 기본음. 여기까지 오면 최소한 "뭔가 걸렸다"는 신호는 난다.
	const FName TierSoundId = UFAAudioSettings::MakeRuleTierSoundId(Tier);
	if (!TierSoundId.IsNone())
	{
		PlaySound(TierSoundId);
	}
}
