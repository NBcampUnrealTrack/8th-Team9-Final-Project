// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class LoadingScreenKit : ModuleRules
{
	public LoadingScreenKit(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UMG",
			// ULSKPluginSettings 가 UDeveloperSettings 를 상속한다 (공개 헤더).
			"DeveloperSettings",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// 위젯을 뷰포트 오버레이(SWidget)에 직접 걸고, WBP 없이 임시 레이아웃을 조립한다.
			"Slate",
			"SlateCore",
		});
	}
}
