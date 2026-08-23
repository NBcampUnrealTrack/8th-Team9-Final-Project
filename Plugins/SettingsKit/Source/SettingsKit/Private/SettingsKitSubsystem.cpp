// Fill out your copyright notice in the Description page of Project Settings.

#include "SettingsKitSubsystem.h"

#include "SettingsKitLog.h"
#include "SettingsKitPluginSettings.h"

#include "InputMappingContext.h"

#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameUserSettings.h"
#include "GameplayTagContainer.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Kismet/GameplayStatics.h"
#include "UserSettings/EnhancedInputUserSettings.h"

USKSettingsSubsystem* USKSettingsSubsystem::Get(const UObject* WorldContextObject)
{
	if (const UGameInstance* GI = UGameplayStatics::GetGameInstance(WorldContextObject))
	{
		return GI->GetSubsystem<USKSettingsSubsystem>();
	}
	return nullptr;
}

void USKSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 저장된 값을 읽어온다. 파일이 없으면 구조체 기본값이 그대로 남는다.
	LoadConfig();

	// 감마만 여기서 바로 반영한다 — 나머지는 리스너가 붙은 뒤 BroadcastAll() 로 전달된다.
	ApplyGamma();

	TakeSnapshot();
}

void USKSettingsSubsystem::Deinitialize()
{
	OnSettingsChanged.Clear();
	OnUISoundRequested.Clear();

	Super::Deinitialize();
}

// ============================================================
// 오디오
// ============================================================
void USKSettingsSubsystem::SetMasterVolume(float NewVolume)
{
	Values.MasterVolume = FMath::Clamp(NewVolume, 0.f, 1.f);
	NotifyChanged(ESKSettingsTab::Audio);
}

void USKSettingsSubsystem::SetMusicVolume(float NewVolume)
{
	Values.MusicVolume = FMath::Clamp(NewVolume, 0.f, 1.f);
	NotifyChanged(ESKSettingsTab::Audio);
}

void USKSettingsSubsystem::SetSFXVolume(float NewVolume)
{
	Values.SFXVolume = FMath::Clamp(NewVolume, 0.f, 1.f);
	NotifyChanged(ESKSettingsTab::Audio);
}

void USKSettingsSubsystem::SetUIVolume(float NewVolume)
{
	Values.UIVolume = FMath::Clamp(NewVolume, 0.f, 1.f);
	NotifyChanged(ESKSettingsTab::Audio);
}

void USKSettingsSubsystem::SetMuted(bool bNewMuted)
{
	Values.bMuted = bNewMuted;
	NotifyChanged(ESKSettingsTab::Audio);
}

// ============================================================
// 보이스
// ============================================================
void USKSettingsSubsystem::SetVoiceOutputVolume(float NewVolume)
{
	Values.VoiceOutputVolume = FMath::Clamp(NewVolume, 0.f, 1.f);
	NotifyChanged(ESKSettingsTab::Voice);
}

void USKSettingsSubsystem::SetVoiceInputGain(float NewGain)
{
	Values.VoiceInputGain = FMath::Clamp(NewGain, 0.f, 1.f);
	NotifyChanged(ESKSettingsTab::Voice);
}

void USKSettingsSubsystem::SetVoiceGateThreshold(float NewThreshold)
{
	Values.VoiceGateThreshold = FMath::Clamp(NewThreshold, 0.f, 1.f);
	NotifyChanged(ESKSettingsTab::Voice);
}

void USKSettingsSubsystem::SetVoiceMuted(bool bNewMuted)
{
	Values.bVoiceMuted = bNewMuted;
	NotifyChanged(ESKSettingsTab::Voice);
}

// ============================================================
// 화면
// ============================================================
void USKSettingsSubsystem::SetGamma(float NewGamma)
{
	Values.Gamma = FMath::Clamp(NewGamma, 1.f, 4.f);
	ApplyGamma();
	NotifyChanged(ESKSettingsTab::Video);
}

void USKSettingsSubsystem::SetFieldOfView(float NewFOV)
{
	Values.FieldOfView = FMath::Clamp(NewFOV, 60.f, 120.f);
	NotifyChanged(ESKSettingsTab::Video);
}

void USKSettingsSubsystem::SetColorblindType(ESKColorblindType NewType)
{
	Values.ColorblindType = NewType;
	NotifyChanged(ESKSettingsTab::Video);
}

void USKSettingsSubsystem::SetSubtitlesEnabled(bool bEnabled)
{
	Values.bSubtitlesEnabled = bEnabled;
	NotifyChanged(ESKSettingsTab::Video);
}

void USKSettingsSubsystem::SetSubtitleScale(float NewScale)
{
	Values.SubtitleScale = FMath::Clamp(NewScale, 0.5f, 2.f);
	NotifyChanged(ESKSettingsTab::Video);
}

void USKSettingsSubsystem::SetLanguageCulture(const FString& NewCulture)
{
	Values.LanguageCulture = NewCulture;

	// 언어도 감마와 같은 이유로 여기서 직접 적용한다 — 엔진 전역이라 경쟁할 상대가 없다.
	FInternationalization& I18N = FInternationalization::Get();
	const FString Target = NewCulture.IsEmpty() ? I18N.GetDefaultCulture()->GetName() : NewCulture;
	I18N.SetCurrentCulture(Target);

	NotifyChanged(ESKSettingsTab::Video);
}

// ============================================================
// 조작
// ============================================================
void USKSettingsSubsystem::SetMouseSensitivity(float NewSensitivity)
{
	Values.MouseSensitivity = FMath::Clamp(NewSensitivity, 0.1f, 3.f);
	NotifyChanged(ESKSettingsTab::Controls);
}

void USKSettingsSubsystem::SetInvertYAxis(bool bInvert)
{
	Values.bInvertYAxis = bInvert;
	NotifyChanged(ESKSettingsTab::Controls);
}

// ============================================================
// 저장 · 되돌리기
// ============================================================
void USKSettingsSubsystem::ApplyAndSave()
{
	if (UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		// 해상도/화면모드는 여기서 처음 실제로 바뀐다 (그 전까지는 대기 상태).
		GUS->ApplyResolutionSettings(false);
		GUS->ApplyNonResolutionSettings();
		GUS->SaveSettings();
	}

	ApplyGamma();
	SaveConfig();

	bDirty = false;

	// 확정했으니 되돌림 기준점도 여기로 옮긴다.
	TakeSnapshot();
}

void USKSettingsSubsystem::TakeSnapshot()
{
	Snapshot = Values;
	bDirty = false;
}

void USKSettingsSubsystem::RevertToSnapshot()
{
	Values = Snapshot;

	// 그래픽은 우리가 값을 들고 있지 않으므로 디스크의 마지막 저장값을 다시 읽는다.
	if (UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		GUS->LoadSettings();
		GUS->ApplyNonResolutionSettings();
	}

	ApplyGamma();
	SetLanguageCulture(Values.LanguageCulture);

	BroadcastAll();
	bDirty = false;
}

void USKSettingsSubsystem::ResetCategoryToDefaults(ESKSettingsTab Category)
{
	const FSKSettingsValues Defaults;

	switch (Category)
	{
	case ESKSettingsTab::Audio:
		Values.MasterVolume = Defaults.MasterVolume;
		Values.MusicVolume  = Defaults.MusicVolume;
		Values.SFXVolume    = Defaults.SFXVolume;
		Values.UIVolume     = Defaults.UIVolume;
		Values.bMuted       = Defaults.bMuted;
		break;

	case ESKSettingsTab::Voice:
		Values.VoiceOutputVolume  = Defaults.VoiceOutputVolume;
		Values.VoiceInputGain     = Defaults.VoiceInputGain;
		Values.VoiceGateThreshold = Defaults.VoiceGateThreshold;
		Values.bVoiceMuted        = Defaults.bVoiceMuted;
		break;

	case ESKSettingsTab::Video:
		Values.Gamma             = Defaults.Gamma;
		Values.FieldOfView       = Defaults.FieldOfView;
		Values.ColorblindType    = Defaults.ColorblindType;
		Values.bSubtitlesEnabled = Defaults.bSubtitlesEnabled;
		Values.SubtitleScale     = Defaults.SubtitleScale;
		Values.LanguageCulture   = Defaults.LanguageCulture;

		// 엔진 쪽 그래픽도 같이 되돌린다 — 화질 High, VSync 켬, 프레임 무제한.
		// 해상도는 건드리지 않는다. 되돌리다 화면이 안 나오면 사용자가 손쓸 방법이 없다.
		if (UGameUserSettings* GUS = GEngine ? GEngine->GetGameUserSettings() : nullptr)
		{
			GUS->SetOverallScalabilityLevel(2);
			GUS->SetVSyncEnabled(true);
			GUS->SetFrameRateLimit(0.f);
			GUS->ApplyNonResolutionSettings();
		}
		ApplyGamma();
		break;

	case ESKSettingsTab::Controls:
		Values.MouseSensitivity = Defaults.MouseSensitivity;
		Values.bInvertYAxis     = Defaults.bInvertYAxis;
		break;

	default:
		return;
	}

	NotifyChanged(Category);
}

void USKSettingsSubsystem::ResetAllToDefaults()
{
	for (uint8 i = 0; i < static_cast<uint8>(ESKSettingsTab::MAX); ++i)
	{
		ResetCategoryToDefaults(static_cast<ESKSettingsTab>(i));
	}
}

void USKSettingsSubsystem::BroadcastAll()
{
	for (uint8 i = 0; i < static_cast<uint8>(ESKSettingsTab::MAX); ++i)
	{
		OnSettingsChanged.Broadcast(static_cast<ESKSettingsTab>(i));
	}
}

void USKSettingsSubsystem::RequestUISound(FName SoundId)
{
	if (!SoundId.IsNone())
	{
		OnUISoundRequested.Broadcast(SoundId);
	}
}

void USKSettingsSubsystem::NotifyChanged(ESKSettingsTab Category)
{
	bDirty = true;
	OnSettingsChanged.Broadcast(Category);
}

void USKSettingsSubsystem::ApplyGamma() const
{
	if (GEngine)
	{
		GEngine->DisplayGamma = FMath::Clamp(Values.Gamma, 1.f, 4.f);
	}
}

// ============================================================
// 키 리바인딩
// ============================================================
UEnhancedInputUserSettings* USKSettingsSubsystem::GetInputUserSettings(ULocalPlayer* LocalPlayer)
{
	if (!LocalPlayer)
	{
		return nullptr;
	}
	if (UEnhancedInputLocalPlayerSubsystem* Sub = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
	{
		return Sub->GetUserSettings();
	}
	return nullptr;
}

void USKSettingsSubsystem::EnsureRebindableContextsRegistered(ULocalPlayer* LocalPlayer)
{
	UEnhancedInputUserSettings* InputSettings = GetInputUserSettings(LocalPlayer);
	if (!InputSettings)
	{
		return;
	}

	const USKPluginSettings* PluginSettings = USKPluginSettings::Get();
	if (!PluginSettings)
	{
		return;
	}

	for (const TSoftObjectPtr<UInputMappingContext>& SoftContext : PluginSettings->RebindableMappingContexts)
	{
		if (const UInputMappingContext* MappingContext = SoftContext.LoadSynchronous())
		{
			// 이미 등록돼 있으면 내부에서 바로 반환한다.
			InputSettings->RegisterInputMappingContext(MappingContext);
		}
	}
}

bool USKSettingsSubsystem::RegisterMappingContextForRebinding(ULocalPlayer* LocalPlayer, const UInputMappingContext* MappingContext)
{
	if (!MappingContext)
	{
		return false;
	}

	UEnhancedInputUserSettings* InputSettings = GetInputUserSettings(LocalPlayer);
	if (!InputSettings)
	{
		return false;
	}

	InputSettings->RegisterInputMappingContext(MappingContext);
	NotifyChanged(ESKSettingsTab::Controls);
	return true;
}

TArray<FSKKeybindRow> USKSettingsSubsystem::GetKeybindRows(ULocalPlayer* LocalPlayer) const
{
	TArray<FSKKeybindRow> Rows;

	// 목록을 읽기 전에 등록부터. 게임이 AddMappingContext 로 IMC 를 넣어도 사용자 설정에는
	// 자동으로 안 올라간다 — bNotifyUserSettings 기본값이 false 다.
	EnsureRebindableContextsRegistered(LocalPlayer);

	UEnhancedInputUserSettings* InputSettings = GetInputUserSettings(LocalPlayer);
	if (!InputSettings)
	{
		// 여기서 비는 건 대부분 Enable User Settings 가 꺼져 있는 경우다.
		UE_LOG(LogSettingsKit, Verbose,
			TEXT("[SettingsKit] Enhanced Input 사용자 설정이 없다. Project Settings → Enhanced Input → Enable User Settings 확인."));
		return Rows;
	}

	UEnhancedPlayerMappableKeyProfile* Profile = InputSettings->GetActiveKeyProfile();
	if (!Profile)
	{
		return Rows;
	}

	for (const TPair<FName, FKeyMappingRow>& Pair : Profile->GetPlayerMappingRows())
	{
		for (const FPlayerKeyMapping& Mapping : Pair.Value.Mappings)
		{
			FSKKeybindRow& Row = Rows.AddDefaulted_GetRef();
			Row.MappingName = Mapping.GetMappingName();
			Row.DisplayName = Mapping.GetDisplayName();
			Row.KeyName     = Mapping.GetCurrentKey().GetDisplayName();

			// 슬롯은 첫 번째만 보여준다. 한 동작에 키 두 개를 거는 UI 는 아직 없다.
			break;
		}
	}

	// TMap 순회 순서는 보장되지 않는다 — 목록이 열 때마다 뒤바뀌면 못 쓴다.
	Rows.Sort([](const FSKKeybindRow& A, const FSKKeybindRow& B)
	{
		return A.DisplayName.CompareTo(B.DisplayName) < 0;
	});

	return Rows;
}

bool USKSettingsSubsystem::TryRebindKey(ULocalPlayer* LocalPlayer, FName MappingName, const FKey& NewKey, FText& OutError)
{
	OutError = FText::GetEmpty();

	UEnhancedInputUserSettings* InputSettings = GetInputUserSettings(LocalPlayer);
	if (!InputSettings)
	{
		OutError = NSLOCTEXT("SettingsKit", "NoInputSettings", "키 설정 기능이 꺼져 있습니다.");
		return false;
	}

	// 중복 검사 — 다른 동작이 이미 이 키를 쓰고 있으면 막는다.
	if (const UEnhancedPlayerMappableKeyProfile* Profile = InputSettings->GetActiveKeyProfile())
	{
		for (const TPair<FName, FKeyMappingRow>& Pair : Profile->GetPlayerMappingRows())
		{
			for (const FPlayerKeyMapping& Mapping : Pair.Value.Mappings)
			{
				if (Mapping.GetMappingName() != MappingName && Mapping.GetCurrentKey() == NewKey)
				{
					OutError = FText::Format(
						NSLOCTEXT("SettingsKit", "DuplicateKey", "'{0}' 키는 이미 '{1}'에 할당되어 있습니다."),
						NewKey.GetDisplayName(), Mapping.GetDisplayName());
					return false;
				}
			}
		}
	}

	FMapPlayerKeyArgs Args;
	Args.MappingName = MappingName;
	Args.Slot = EPlayerMappableKeySlot::First;
	Args.NewKey = NewKey;
	Args.bCreateMatchingSlotIfNeeded = true;

	FGameplayTagContainer FailureReason;
	InputSettings->MapPlayerKey(Args, FailureReason);

	if (!FailureReason.IsEmpty())
	{
		OutError = FText::Format(
			NSLOCTEXT("SettingsKit", "RebindFailed", "키를 바꾸지 못했습니다 ({0})."),
			FText::FromString(FailureReason.ToStringSimple()));
		return false;
	}

	// 키 배치는 우리 ini 가 아니라 Enhanced Input 이 따로 저장한다. 즉시 확정한다.
	InputSettings->ApplySettings();
	InputSettings->SaveSettings();

	NotifyChanged(ESKSettingsTab::Controls);
	return true;
}

void USKSettingsSubsystem::ResetKeybindsToDefaults(ULocalPlayer* LocalPlayer)
{
	UEnhancedInputUserSettings* InputSettings = GetInputUserSettings(LocalPlayer);
	if (!InputSettings)
	{
		return;
	}

	FGameplayTagContainer FailureReason;
	InputSettings->ResetKeyProfileIdToDefault(InputSettings->GetActiveKeyProfileId(), FailureReason);
	InputSettings->ApplySettings();
	InputSettings->SaveSettings();

	NotifyChanged(ESKSettingsTab::Controls);
}
