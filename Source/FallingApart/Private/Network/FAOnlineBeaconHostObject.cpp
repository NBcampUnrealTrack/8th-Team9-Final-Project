// Fill out your copyright notice in the Description page of Project Settings.


#include "Network/FAOnlineBeaconHostObject.h"

#include "Network/FAOnlineBeaconClient.h"

AFAOnlineBeaconHostObject::AFAOnlineBeaconHostObject(const FObjectInitializer& OI)
	: Super(OI)
{
	ClientBeaconActorClass = AFAOnlineBeaconClient::StaticClass();
	BeaconTypeName = AFAOnlineBeaconClient::StaticClass()->GetName();
}
