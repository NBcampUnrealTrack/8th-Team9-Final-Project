// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FallingApart : ModuleRules
{
	public FallingApart(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// UMG 는 Public — UI/FAHUDWidget.h 가 공개 헤더에서 UUserWidget 을 상속한다.
		// (그 외 용도: 발화 중 머리 위 아이콘 UWidgetComponent)
		// DeveloperSettings 도 Public — Public/Rule/FARuleSettings.h 가 UDeveloperSettings 를 상속한다.
		// SettingsKit 도 Public — Public/Settings/FASettingsBridge.h 가 공개 헤더에서 SettingsKitTypes.h 를 쓴다.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG",
			"GameplayAbilities", "GameplayTags", "GameplayTasks", "DeveloperSettings", "SettingsKit" });

		// UI/FADraftWidget.cpp 가 WBP 없이 위젯을 직접 조립하면서 FSlateFontInfo·FCoreStyle 을 쓴다.
		// EngineSettings — ESC 메뉴의 "메인 메뉴" 가 돌아갈 레벨 기본값으로 UGameMapsSettings 의
		// Game Default Map 을 읽는다 (Player/FAPlayerController.cpp).
		// LoadingScreenKit — 트래블 로딩 화면. 공개 헤더에는 안 나오고 .cpp 세 곳에서만 부른다.
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "EngineSettings", "HTTP", "LoadingScreenKit" });

		// 로컬 토커 등록(IOnlineVoice::RegisterLocalTalker)은 UOnlineEngineInterface 에 노출돼 있지 않아
		// VoiceInterface 를 직접 잡아야 한다. 엔진은 이걸 자동으로 해주지 않는다.
		// AudioMixer — UFAVoiceStatics 가 월드 정리 때 UVoipListenerSynthComponent(USynthComponent 파생)의
		// Stop()/GetAudioComponent() 를 직접 불러 등록을 푼다.
		// AudioCaptureCore / SignalProcessing — UFAVoiceCalibration 이 마이크 잡음을 재는 데 쓴다.
		// 엔진의 IVoiceCapture 인스턴스는 FVoiceEngineImpl 안에 protected 로 갇혀 있어서
		// 게임 코드가 진폭을 못 읽는다. 그래서 측정용 스트림을 따로 연다.
		// 실제 캡처 구현(RtAudio/Wasapi)은 AudioCapture 플러그인이 런타임에 등록한다.
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreOnline", "OnlineSubsystem", "OnlineSubsystemUtils", "AudioMixer",
			"AudioCaptureCore", "SignalProcessing",
			//스팀 연동을 위한 모듈 포함
			"OnlineSubsystemSteam"
		});

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
