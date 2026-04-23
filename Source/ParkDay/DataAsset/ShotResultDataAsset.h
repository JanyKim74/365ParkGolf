// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "../LandscapeChecker.h"
#include "../Structs/DataTableStruct.h"
#include "ShotResultDataAsset.generated.h"


UCLASS()
class PARKDAY_API UShotResultDataAsset : public UDataAsset
{
	GENERATED_BODY()
	
public:
    // LandType → 결과 데이터 매핑
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Result")
    TMap<ELandType, FShotResultData> LandResultMap;

    // 전체 데이터 반환
    UFUNCTION(BlueprintCallable, Category="Result")
    bool GetResultData(ELandType LandType, FShotResultData& OutData) const;

    // 이미지만 필요할 때
    UFUNCTION(BlueprintCallable, Category="Result")
    UTexture2D* GetResultImage(ELandType LandType) const;

    // 사운드만 필요할 때
    UFUNCTION(BlueprintCallable, Category="Result")
    USoundBase* GetResultSound(ELandType LandType) const;
};
