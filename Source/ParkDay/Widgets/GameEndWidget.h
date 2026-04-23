// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ParkDay/GolfDataStructures.h"
#include "GameEndWidget.generated.h"

class UTextBlock;
class AInGameMode;
class UImage;
class UButton	;

UCLASS()
class PARKDAY_API UGameEndWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget)) UTextBlock* TextBlock_Name;
	UPROPERTY(meta = (BindWidget)) UTextBlock* TextBlock_ShotCount;
	UPROPERTY(meta = (BindWidget)) UButton* Button_ScoreCard;
	UPROPERTY(meta = (BindWidget)) UButton* Button_Exit;
	UPROPERTY(meta = (BindWidget)) UTextBlock* TextBlock_ScoreCard;
	UPROPERTY(meta = (BindWidget)) UTextBlock* TextBlock_Exit;

	UFUNCTION() 
	void OnClickedScoreCardButton();
	UFUNCTION() 
	void OnClickedExitButton();

	UFUNCTION()
		void SetWinnerText();

	UFUNCTION()
		void Init();

	UPROPERTY()
	FPlayerInfo WinnerPlayerInfo;
	FString WinnerNickName;
	int32 WinnerShotCount;

	AInGameMode* GM;
};
