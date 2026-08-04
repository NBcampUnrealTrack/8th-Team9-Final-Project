// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FallingApart : ModuleRules
{
	public FallingApart(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			
			//스팀 연동을 위한 모듈 포함
			"OnlineSubsystemSteam"
		});

		// UMG: 발화 중 머리 위 아이콘(UWidgetComponent)
		PrivateDependencyModuleNames.AddRange(new string[] { "UMG" });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// 로컬 토커 등록(IOnlineVoice::RegisterLocalTalker)은 UOnlineEngineInterface 에 노출돼 있지 않아
		// VoiceInterface 를 직접 잡아야 한다. 엔진은 이걸 자동으로 해주지 않는다.
		PrivateDependencyModuleNames.AddRange(new string[] { "OnlineSubsystem", "OnlineSubsystemUtils" });

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
