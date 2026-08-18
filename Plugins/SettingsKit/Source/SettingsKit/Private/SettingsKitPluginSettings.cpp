// Fill out your copyright notice in the Description page of Project Settings.

#include "SettingsKitPluginSettings.h"

const USKPluginSettings* USKPluginSettings::Get()
{
	return GetDefault<USKPluginSettings>();
}
