// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "TestMenu.generated.h"

class UButton;
/**
 * 
 */
UCLASS()
class FALLINGAPART_API UTestMenu : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual bool Initialize() override;

	/// 
	/// @param NumberOfPublicConnections 최대 입장 가능한 플레이어 수
	/// @param TypeOfMatch 
	UFUNCTION(BlueprintCallable)
	void MenuSetup(int32 NumberOfPublicConnections = 4, FString TypeOfMatch = FString(TEXT("FreeForAll")));
	
	UPROPERTY(EditDefaultsOnly,Category = "FallingApart|Level")
	TSoftObjectPtr<UWorld> TargetLevelPath;
	
protected:
	
	virtual void NativeDestruct() override;
	
	// 서브시스템의 대리자에 등록할 콜백함수
	UFUNCTION()
	void OnCreateSession(bool bWasSuccessful);

	// 아래 두 함수는 다이나믹 델리게이트(블루프린트 공개)가 아니므로 UFUNCTION을 사용하지않음)
	void OnFindSession(const TArray<FOnlineSessionSearchResult>& SessionResult, bool bWasSuccessful);
	
	void OnJoinSession(EOnJoinSessionCompleteResult::Type Result);
	
	UFUNCTION()
	void OnDestroySession(bool bWasSuccessful);
	
	UFUNCTION()
	void OnStartSession(bool bWasSuccessful);
	
	
	
	
private:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> HostButton;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> JoinButton;
	
	UFUNCTION()
	void HostbuttonClicked();
	
	UFUNCTION()
	void JoinbuttonClicked();
	
	void MenuTearDown();
	
	// 온라인 세션을 제어하기위한 서브시스템 포인터
	class UFAOnlineSessionSubsystem* OnlineSessionSubsystem;
	
	int32 NumPublicConnections{4};
	
	FString MatchType{TEXT("FreeForAll")};
};