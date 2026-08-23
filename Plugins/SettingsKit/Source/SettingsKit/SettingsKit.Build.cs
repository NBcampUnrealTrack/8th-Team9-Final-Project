// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class SettingsKit : ModuleRules
{
	public SettingsKit(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UMG",
			"EnhancedInput",
			"GameplayTags",
			// USKPluginSettings 가 UDeveloperSettings 를 상속한다 (공개 헤더).
			"DeveloperSettings",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"InputCore",
			"Slate",
			"SlateCore",
			"ApplicationCore",
		});
	}
}
