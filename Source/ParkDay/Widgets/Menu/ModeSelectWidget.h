// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ModeSelectWidget.generated.h"

class UTextBlock;
class AInGameMode;
class UImage;
class UButton;

class AMenuGameMode;



UCLASS()
class PARKDAY_API UModeSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta=(BindWidget))
		UButton* Button_Exit;

	UPROPERTY(meta = (BindWidget))
		UButton* Button_StrokeMode;
	
	UPROPERTY(meta = (BindWidget))
		UButton* Button_TraningMode;

	UPROPERTY(meta = (BindWidget))
		UButton* Button_PracticeMode;

public:
	UFUNCTION()
	virtual void NativeConstruct();
	UFUNCTION()
	void OnClickStrokeModeButton();
	UFUNCTION()
	void OnClickTraningModeButton();
	UFUNCTION()
	void OnClickPracticeModeButton();
	UFUNCTION()
	void OnClickExitButton();

	void ResetGameInfoAndAddPlayer();
	UFUNCTION()
	void OpenPracticeLevel();

	UFUNCTION() void OpenStrokeMode();
	UFUNCTION() void OpenTrainingMode();

	UFUNCTION() void HandleOnConfirmPasswordForStroke();
	UFUNCTION() void HandleOnConfirmPasswordForTraining();
	UFUNCTION() void HandleOnConfirmPasswordForPractice();
private:
	AMenuGameMode* GM;
};
