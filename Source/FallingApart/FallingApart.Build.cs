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
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG",
			"GameplayAbilities", "GameplayTags", "GameplayTasks", "DeveloperSettings" });

		// UI/FADraftWidget.cpp 가 WBP 없이 위젯을 직접 조립하면서 FSlateFontInfo·FCoreStyle 을 쓴다.
		// EngineSettings — ESC 메뉴의 "메인 메뉴" 가 돌아갈 레벨 기본값으로 UGameMapsSettings 의
		// Game Default Map 을 읽는다 (Player/FAPlayerController.cpp).
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "EngineSettings" });

		// 로컬 토커 등록(IOnlineVoice::RegisterLocalTalker)은 UOnlineEngineInterface 에 노출돼 있지 않아
		// VoiceInterface 를 직접 잡아야 한다. 엔진은 이걸 자동으로 해주지 않는다.
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"OnlineSubsystem", "OnlineSubsystemUtils",
			//스팀 연동을 위한 모듈 포함
			"OnlineSubsystemSteam"
		});

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
