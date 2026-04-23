// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MenuUIImageDataAsset.generated.h"

USTRUCT(BlueprintType)
struct FUIImage
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Icon",
        meta=(DisplayName="설명", MultiLine="true"))
    FText Description;

    // 키(찾을 때 쓸 ID) - 오타 방지하려면 아래 "Enum 방식"을 추천
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Icon",
        meta=(DisplayName="ID"))
    FName Id = NAME_None;

    // 실제 이미지 (Soft로 두면 의존/로딩 부담이 줄어듦)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Icon",
        meta=(DisplayName="텍스처"))
    TSoftObjectPtr<UTexture2D> Texture;
};

UCLASS()
class PARKDAY_API UMenuUIImageDataAsset : public UDataAsset
{
	GENERATED_BODY()
	
public:
    // LandType → 결과 데이터 매핑
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Image")
    TArray<FUIImage> UIImages;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Icons")
    TSoftObjectPtr<UTexture2D> DefaultTexture;

    UFUNCTION(BlueprintCallable, Category="Result")
    UTexture2D* GetUIImage(FName Id) const;
};
