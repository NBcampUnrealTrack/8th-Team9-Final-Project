// Fill out your copyright notice in the Description page of Project Settings.

#include "Voice/FAVoiceUserSettings.h"

#include "Engine/GameInstance.h"
#include "GameFramework/GameStateBase.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "TimerManager.h"
#include "Voice/FAVoiceTypes.h"

namespace FAVoiceUserSettingsInternal
{
	/** 값이 남는 곳. GameUserSettings.ini 는 사용자별 파일이라 팀원 설정이 서로 섞이지 않는다. */
	static const TCHAR* VolumeSection = TEXT("FA.VoicePlayerVolumes");
	static const TCHAR* MuteSection   = TEXT("FA.VoicePlayerMutes");

	/** 슬라이더를 놓고 이만큼 뒤에 한 번만 저장한다. */
	static constexpr float SaveDelaySeconds = 1.0f;
}

UFAVoiceUserSettings* UFAVoiceUserSettings::Get(const UObject* WorldContextObject)
{
	const UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);
	return GameInstance != nullptr ? GameInstance->GetSubsystem<UFAVoiceUserSettings>() : nullptr;
}

FString UFAVoiceUserSettings::MakePlayerKey(const APlayerState* PlayerState)
{
	if (PlayerState == nullptr)
	{
		return FString();
	}

	const FUniqueNetIdRepl& NetId = PlayerState->GetUniqueId();
	if (!NetId.IsValid())
	{
		// 복제가 아직 안 온 것이다. 부르는 쪽이 다시 시도해야 한다 — 이름으로 대신하면 안 된다.
		return FString();
	}

	return NetId->ToString();
}

void UFAVoiceUserSettings::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LoadFromConfig();
}

void UFAVoiceUserSettings::Deinitialize()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetTimerManager().ClearTimer(SaveTimerHandle);
	}

	// 슬라이더를 만지고 바로 게임을 끄는 흐름에서 마지막 값이 날아가지 않게.
	SaveToConfig();

	Super::Deinitialize();
}

UFAVoiceUserSettings::FEntry& UFAVoiceUserSettings::FindOrAddEntry(const FString& PlayerKey)
{
	return Entries.FindOrAdd(PlayerKey);
}

float UFAVoiceUserSettings::GetPlayerVolume(const FString& PlayerKey) const
{
	const FEntry* Entry = Entries.Find(PlayerKey);
	return Entry != nullptr ? Entry->Volume : DefaultPlayerVolume;
}

void UFAVoiceUserSettings::SetPlayerVolume(const FString& PlayerKey, float NewVolume)
{
	if (PlayerKey.IsEmpty())
	{
		return;
	}

	const float Clamped = FMath::Clamp(NewVolume, MinPlayerVolume, MaxPlayerVolume);

	FEntry& Entry = FindOrAddEntry(PlayerKey);
	if (FMath::IsNearlyEqual(Entry.Volume, Clamped))
	{
		return;
	}

	Entry.Volume = Clamped;

	NotifyChanged(PlayerKey);
	RequestSave();
}

bool UFAVoiceUserSettings::IsPlayerMuted(const FString& PlayerKey) const
{
	const FEntry* Entry = Entries.Find(PlayerKey);
	return Entry != nullptr && Entry->bMuted;
}

void UFAVoiceUserSettings::SetPlayerMuted(const FString& PlayerKey, bool bMuted)
{
	if (PlayerKey.IsEmpty())
	{
		return;
	}

	FEntry& Entry = FindOrAddEntry(PlayerKey);
	if (Entry.bMuted == bMuted)
	{
		return;
	}

	Entry.bMuted = bMuted;

	NotifyChanged(PlayerKey);
	RequestSave();
}

float UFAVoiceUserSettings::GetPlayerVolumeScale(const FString& PlayerKey) const
{
	const FEntry* Entry = Entries.Find(PlayerKey);
	if (Entry == nullptr)
	{
		return DefaultPlayerVolume;
	}

	return Entry->bMuted ? 0.f : Entry->Volume;
}

void UFAVoiceUserSettings::ResetPlayer(const FString& PlayerKey)
{
	if (Entries.Remove(PlayerKey) > 0)
	{
		NotifyChanged(PlayerKey);
		RequestSave();
	}
}

void UFAVoiceUserSettings::ResetAll()
{
	TArray<FString> Keys;
	Entries.GetKeys(Keys);

	Entries.Empty();

	for (const FString& Key : Keys)
	{
		NotifyChanged(Key);
	}

	RequestSave();
}

void UFAVoiceUserSettings::GetConfiguredPlayers(TArray<FString>& OutKeys) const
{
	Entries.GetKeys(OutKeys);
}

void UFAVoiceUserSettings::NotifyChanged(const FString& PlayerKey)
{
	OnPlayerVoiceSettingsChanged.Broadcast(PlayerKey, GetPlayerVolumeScale(PlayerKey));
}

FString UFAVoiceUserSettings::SanitizeKey(const FString& PlayerKey)
{
	FString Sanitized;
	Sanitized.Reserve(PlayerKey.Len());

	for (const TCHAR Char : PlayerKey)
	{
		Sanitized.AppendChar(FChar::IsAlnum(Char) ? Char : TEXT('_'));
	}

	return Sanitized;
}

void UFAVoiceUserSettings::LoadFromConfig()
{
	Entries.Empty();

	if (GConfig == nullptr)
	{
		return;
	}

	// 원래 키를 그대로 못 쓰는 백엔드가 있을 수 있어 ini 에는 정제한 키로 적는다.
	// 읽을 때도 같은 규칙으로 찾으므로 왕복이 맞는다.
	if (const FConfigSection* VolumeSectionEntries = GConfig->GetSection(FAVoiceUserSettingsInternal::VolumeSection, false, GGameUserSettingsIni))
	{
		for (const TPair<FName, FConfigValue>& Pair : *VolumeSectionEntries)
		{
			FEntry& Entry = Entries.FindOrAdd(Pair.Key.ToString());
			Entry.Volume = FMath::Clamp(FCString::Atof(*Pair.Value.GetValue()), MinPlayerVolume, MaxPlayerVolume);
		}
	}

	if (const FConfigSection* MuteSectionEntries = GConfig->GetSection(FAVoiceUserSettingsInternal::MuteSection, false, GGameUserSettingsIni))
	{
		for (const TPair<FName, FConfigValue>& Pair : *MuteSectionEntries)
		{
			FEntry& Entry = Entries.FindOrAdd(Pair.Key.ToString());
			Entry.bMuted = Pair.Value.GetValue().ToBool();
		}
	}

	UE_LOG(LogFAVoice, Log, TEXT("[보이스] 플레이어별 볼륨 %d 명분을 불러왔다."), Entries.Num());
}

void UFAVoiceUserSettings::SaveToConfig()
{
	if (GConfig == nullptr || !bDirty)
	{
		return;
	}

	bDirty = false;

	// 지운 사람이 다시 살아나지 않도록 섹션을 통째로 비우고 다시 쓴다.
	GConfig->EmptySection(FAVoiceUserSettingsInternal::VolumeSection, GGameUserSettingsIni);
	GConfig->EmptySection(FAVoiceUserSettingsInternal::MuteSection, GGameUserSettingsIni);

	for (const TPair<FString, FEntry>& Pair : Entries)
	{
		if (Pair.Value.IsDefault())
		{
			// 기본값은 안 적는다. ini 가 지나간 사람들로 계속 불어나는 것을 막는다.
			continue;
		}

		const FString Key = SanitizeKey(Pair.Key);

		GConfig->SetFloat(FAVoiceUserSettingsInternal::VolumeSection, *Key, Pair.Value.Volume, GGameUserSettingsIni);

		if (Pair.Value.bMuted)
		{
			GConfig->SetBool(FAVoiceUserSettingsInternal::MuteSection, *Key, true, GGameUserSettingsIni);
		}
	}

	GConfig->Flush(false, GGameUserSettingsIni);
}

void UFAVoiceUserSettings::RequestSave()
{
	bDirty = true;

	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance == nullptr)
	{
		// 타이머를 걸 곳이 없으면 지금 쓴다. (에디터 종료 직전 등)
		SaveToConfig();
		return;
	}

	FTimerManager& TimerManager = GameInstance->GetTimerManager();
	TimerManager.ClearTimer(SaveTimerHandle);
	TimerManager.SetTimer(SaveTimerHandle, FTimerDelegate::CreateUObject(this, &UFAVoiceUserSettings::SaveToConfig),
		FAVoiceUserSettingsInternal::SaveDelaySeconds, false);
}

// ============================================================
// 콘솔 명령 — WBP 없이도, 슬라이더 없이도 검증할 수 있게
// ============================================================
namespace FAVoicePlayerVolumeDebug
{
	/** 이름 조각(대소문자 무시) 또는 키 자체로 접속자를 찾는다. */
	static const APlayerState* FindPlayer(const UWorld* World, const FString& Needle)
	{
		const AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
		if (GameState == nullptr)
		{
			return nullptr;
		}

		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (PlayerState == nullptr)
			{
				continue;
			}

			if (PlayerState->GetPlayerName().Contains(Needle, ESearchCase::IgnoreCase)
				|| UFAVoiceUserSettings::MakePlayerKey(PlayerState) == Needle)
			{
				return PlayerState;
			}
		}

		return nullptr;
	}

	static void ListPlayers(const TArray<FString>& Args, UWorld* World)
	{
		UFAVoiceUserSettings* VoiceSettings = UFAVoiceUserSettings::Get(World);
		const AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
		if (VoiceSettings == nullptr || GameState == nullptr)
		{
			UE_LOG(LogFAVoice, Warning, TEXT("FAVoice.Player.List: 아직 게임에 들어와 있지 않다."));
			return;
		}

		UE_LOG(LogFAVoice, Display, TEXT("========== 접속자별 음량 =========="));
		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (PlayerState == nullptr)
			{
				continue;
			}

			const FString Key = UFAVoiceUserSettings::MakePlayerKey(PlayerState);
			UE_LOG(LogFAVoice, Display, TEXT("  %-20s 배수 %.2f%s   키 %s"),
				*PlayerState->GetPlayerName(),
				VoiceSettings->GetPlayerVolume(Key),
				VoiceSettings->IsPlayerMuted(Key) ? TEXT(" (음소거)") : TEXT(""),
				Key.IsEmpty() ? TEXT("없음 <<< UniqueNetId 미도착") : *Key);
		}
		UE_LOG(LogFAVoice, Display, TEXT("=================================="));
	}

	static void SetVolume(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogFAVoice, Warning, TEXT("사용법: FAVoice.Player.Volume <이름조각> <0.0~2.0>"));
			return;
		}

		UFAVoiceUserSettings* VoiceSettings = UFAVoiceUserSettings::Get(World);
		const APlayerState* PlayerState = FindPlayer(World, Args[0]);
		if (VoiceSettings == nullptr || PlayerState == nullptr)
		{
			UE_LOG(LogFAVoice, Warning, TEXT("FAVoice.Player.Volume: '%s' 를 찾지 못했다."), *Args[0]);
			return;
		}

		const FString Key = UFAVoiceUserSettings::MakePlayerKey(PlayerState);
		VoiceSettings->SetPlayerVolume(Key, FCString::Atof(*Args[1]));

		UE_LOG(LogFAVoice, Display, TEXT("%s 의 음량을 %.2f 로 바꿨다."),
			*PlayerState->GetPlayerName(), VoiceSettings->GetPlayerVolume(Key));
	}

	static void SetMuted(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogFAVoice, Warning, TEXT("사용법: FAVoice.Player.Mute <이름조각> [0|1]"));
			return;
		}

		UFAVoiceUserSettings* VoiceSettings = UFAVoiceUserSettings::Get(World);
		const APlayerState* PlayerState = FindPlayer(World, Args[0]);
		if (VoiceSettings == nullptr || PlayerState == nullptr)
		{
			UE_LOG(LogFAVoice, Warning, TEXT("FAVoice.Player.Mute: '%s' 를 찾지 못했다."), *Args[0]);
			return;
		}

		const FString Key = UFAVoiceUserSettings::MakePlayerKey(PlayerState);
		const bool bMuted = Args.Num() >= 2 ? Args[1].ToBool() : !VoiceSettings->IsPlayerMuted(Key);
		VoiceSettings->SetPlayerMuted(Key, bMuted);

		UE_LOG(LogFAVoice, Display, TEXT("%s %s"), *PlayerState->GetPlayerName(),
			bMuted ? TEXT("음소거") : TEXT("음소거 해제"));
	}

	static void ResetVolumes(const TArray<FString>& Args, UWorld* World)
	{
		UFAVoiceUserSettings* VoiceSettings = UFAVoiceUserSettings::Get(World);
		if (VoiceSettings == nullptr)
		{
			return;
		}

		if (Args.Num() >= 1)
		{
			if (const APlayerState* PlayerState = FindPlayer(World, Args[0]))
			{
				VoiceSettings->ResetPlayer(UFAVoiceUserSettings::MakePlayerKey(PlayerState));
				UE_LOG(LogFAVoice, Display, TEXT("%s 를 기본값으로 되돌렸다."), *PlayerState->GetPlayerName());
			}
			return;
		}

		VoiceSettings->ResetAll();
		UE_LOG(LogFAVoice, Display, TEXT("접속자별 음량을 전부 기본값으로 되돌렸다."));
	}

	static FAutoConsoleCommandWithWorldAndArgs ListCommand(
		TEXT("FAVoice.Player.List"),
		TEXT("접속자별 보이스 음량 설정을 보여준다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ListPlayers));

	static FAutoConsoleCommandWithWorldAndArgs VolumeCommand(
		TEXT("FAVoice.Player.Volume"),
		TEXT("FAVoice.Player.Volume <이름조각> <0.0~2.0> — 그 사람 목소리만 내 쪽에서 키우거나 줄인다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetVolume));

	static FAutoConsoleCommandWithWorldAndArgs MuteCommand(
		TEXT("FAVoice.Player.Mute"),
		TEXT("FAVoice.Player.Mute <이름조각> [0|1] — 그 사람 목소리만 내 쪽에서 끈다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetMuted));

	static FAutoConsoleCommandWithWorldAndArgs ResetCommand(
		TEXT("FAVoice.Player.Reset"),
		TEXT("FAVoice.Player.Reset [이름조각] — 인자가 없으면 전부 기본값으로."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ResetVolumes));
}
