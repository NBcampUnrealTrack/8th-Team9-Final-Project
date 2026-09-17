// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FAOverHeadWidget.generated.h"

class AFACharacter;
class UTextBlock;
/**
 * 
 */
UCLASS()
class FALLINGAPART_API UFAOverHeadWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	/** 이 위젯이 이름을 표시할 캐릭터. 로컬 UI 소유 Pawn 과 표시 대상을 분리한다. */
	void SetTargetCharacter(AFACharacter* InTargetCharacter);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;
	
	UPROPERTY(BlueprintReadOnly, Category="FallingApart|Widget", meta=(OptionalWidget))
	TObjectPtr<UTextBlock> Img_TeamIcon;// 진영 아이콘?
	
	UPROPERTY(BlueprintReadOnly, Category="FallingApart|Widget", meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_Nickname;

private:
	/** 위젯의 수명은 WidgetComponent 가 관리하므로 표시 대상은 약한 참조로만 보관한다. */
	TWeakObjectPtr<AFACharacter> TargetCharacter;

	/** 주입된 캐릭터의 PlayerState 에서 현재 닉네임을 읽어온다. */
	void RefreshNickname();
};
