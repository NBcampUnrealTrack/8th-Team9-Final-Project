// Fill out your copyright notice in the Description page of Project Settings.

#include "LoadingScreenKitSettings.h"

#define LOCTEXT_NAMESPACE "LoadingScreenKit"

ULSKPluginSettings::ULSKPluginSettings()
{
	DefaultTitle = LOCTEXT("DefaultTitle", "불러오는 중...");
}

FLSKLoadingScreenParams ULSKPluginSettings::MakeDefaultParams() const
{
	FLSKLoadingScreenParams Result;
	Result.Title = DefaultTitle;
	Result.MinimumDisplaySeconds = MinimumDisplaySeconds;
	Result.MaximumDisplaySeconds = MaximumDisplaySeconds;
	Result.FadeOutSeconds = FadeOutSeconds;
	Result.bBlockInput = bBlockInputWhileVisible;
	Result.bAutoHideWhenMapLoaded = true;

	return Result;
}

#undef LOCTEXT_NAMESPACE
