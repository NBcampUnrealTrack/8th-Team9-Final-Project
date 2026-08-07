// Copyright Epic Games, Inc. All Rights Reserved.

#include "FallingApart.h"

#include "GAS/FAGameplayTags.h"
#include "Modules/ModuleManager.h"

/**
 * 네이티브 GameplayTag 를 등록하기 위해 기본 모듈 구현 대신 직접 정의한다.
 * (FDefaultGameModuleImpl 은 StartupModule 훅이 없다)
 */
class FFallingApartModule : public FDefaultGameModuleImpl
{
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();

		// DT·GE 애셋이 로드되기 전에 태그가 존재해야 에디터 드롭다운과 애셋 참조가 깨지지 않는다.
		FAGameplayTags::InitializeNativeTags();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE( FFallingApartModule, FallingApart, "FallingApart" );
