#pragma once

#include "CoreMinimal.h"

struct FGameplayTagContainer;
enum class EFAAnimStatusOverride : uint8;

namespace FAAnimStatusPolicy
{
	// 한 프레임의 태그 스냅샷에서 표시 상태만 고른다. 효과·어빌리티·몽타주는 변경하지 않는다.
	EFAAnimStatusOverride Resolve(const FGameplayTagContainer& ActiveTags);
}
