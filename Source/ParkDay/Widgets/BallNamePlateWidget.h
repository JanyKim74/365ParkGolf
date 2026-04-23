// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BallNamePlateWidget.generated.h"

class UImage;
class UTextBlock;
class UCanvasPanel;

UCLASS()
class PARKDAY_API UBallNamePlateWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

public:
	UPROPERTY(meta = (BindWidget)) UCanvasPanel* CanvasPanel_NamePlate = nullptr;
	UPROPERTY(meta = (BindWidget)) UImage* Image_NamePlate = nullptr;
	UPROPERTY(meta = (BindWidget)) UTextBlock* TextBlock_NamePlate = nullptr;

public:
	/** 닉네임 텍스트 설정 */
	UFUNCTION(BlueprintCallable, Category="BallNamePlate")
	void SetPlayerNameText(const FText& InName);

	UFUNCTION(BlueprintCallable, Category="BallNamePlate")
	void SetPlayerNameString(const FString& InName);

	/** ✅ 전체 네임플레이트(배경+텍스트) 스케일 설정 */
	UFUNCTION(BlueprintCallable, Category="BallNamePlate")
	void SetNamePlateScale(float InScale);

private:
	/** 불필요한 SetRenderScale 호출 방지 */
	float CachedScale = 1.0f;
};
