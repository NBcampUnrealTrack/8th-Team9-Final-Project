// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FAMapTypes.generated.h"

class UTexture2D;
class UWorld;

USTRUCT(BlueprintType)
struct FALLINGAPART_API FFAMapDataRow : public FTableRowBase
{
	GENERATED_BODY()
	
	/** UI에 표시될 이름 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FallingApart|Map")
	FText DisplayName;
	
	/** UI에 표시될 맵 설명*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FallingApart|Map", meta = (MultiLine = true))
	FText Description;
	
	/** 최소 권장 인원 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FallingApart|Map", meta = (ClampMin = "1"))
	int32 RecommendedMinPlayers = 4;
	
	/** 최대 권장 인원 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FallingApart|Map", meta = (ClampMin = "1"))
	int32 RecommendedMaxPlayers = 12;
	
	/** UI에 표시될 썸네일 이미지. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FallingApart|Map")
	TSoftObjectPtr<UTexture2D> Thumbnail;
	
	/** 실제 이동할 레벨 에셋 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FallingApart|Map")
	TSoftObjectPtr<UWorld> Level;
	
	/** 필요시 정렬 순서 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FallingApart|Map")
	int32 SortOrder = 0;
	
	/** 사용 가능/불가능. 제작중이나 수정 등으로 제외시 사용 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FallingApart|Map")
	bool bSelectable = true;

};
