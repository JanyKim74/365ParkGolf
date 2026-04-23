// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DistanceWidget.generated.h"

class UTextBlock;
class AInGameMode;
class UImage;
class UTexture2D;

UCLASS()
class PARKDAY_API UDistanceWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidget)) UImage* Image_ShotInfo;
	UPROPERTY(meta = (BindWidget)) UTextBlock* TextBlock_Shot_Distance;
	UPROPERTY(meta = (BindWidget)) UTextBlock* TextBlock_BallSpeed;
	UPROPERTY(meta = (BindWidget)) UTextBlock* TextBlock_LeftRight_Angle;
	UPROPERTY(meta = (BindWidget)) UTextBlock* TextBlock_UpDown_Angle;



	UFUNCTION()
		void UpdateShotDistance();
	UFUNCTION()
		void UpdateSensorTextData();
	UFUNCTION()
	void UpdateShotDistanceText(float ShotDistance);

	UFUNCTION()
		void InitShotDistanceText();

private:
		void UpdateSensorDataText(float BallSpeed, float LeftRight, float UpDown);
	AInGameMode* GM;
};
