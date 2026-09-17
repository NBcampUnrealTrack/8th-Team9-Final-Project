// Fill out your copyright notice in the Description page of Project Settings.

#include "Utils/FASessionTextCodec.h"

#include "Misc/Base64.h"

namespace
{
	const FString SessionTextPrefix(TEXT("B64:"));
}

FString FFASessionTextCodec::Encode(const FString& Text)
{
	return SessionTextPrefix + FBase64::Encode(Text);
}

FString FFASessionTextCodec::Decode(const FString& StoredText)
{
	if (!StoredText.StartsWith(SessionTextPrefix))
	{
		return StoredText; // Prefix가 없는 기존 세션 호환
	}

	FString DecodedText;
	const FString Payload = StoredText.RightChop(SessionTextPrefix.Len());
	return FBase64::Decode(Payload, DecodedText) ? DecodedText : StoredText;
}
