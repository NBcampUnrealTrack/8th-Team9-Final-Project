// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/** Steam 커스텀 세션 문자열을 ANSI 안전 형식으로 저장하고 복원한다. */
class FALLINGAPART_API FFASessionTextCodec final
{
public:
	static FString Encode(const FString& Text);
	static FString Decode(const FString& StoredText);

private:
	FFASessionTextCodec() = delete;
};
