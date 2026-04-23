// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ParkDay/Enums/InGameEnum.h"
#include "InGameMenuPopup.generated.h"

class AInGameMode;

//DECLARE_DELEGATE_OneParam(FOnClicked, FText);
DECLARE_DELEGATE(FOnClickedPopupConfirm);

UCLASS()
class PARKDAY_API UInGameMenuPopup : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

public:
	FOnClickedPopupConfirm OnClickedPopupConfirmDele;

public:
	UPROPERTY(meta = (BindWidget), BlueprintReadWrite)
	class UTextBlock* TextBlock_Description;

	UPROPERTY(meta = (BindWidget), BlueprintReadWrite)
	class UButton* Button_Confirm;

	UPROPERTY(meta = (BindWidget), BlueprintReadWrite)
	class UButton* Button_Cancel;

public:
	void ChangeDescription(FText Description);
	UFUNCTION() void HandleClickedPopupConfirmButton();
	UFUNCTION() void HandleClickedPopupCancelButton();

	void UpdatePopupForNextHole();
	void UpdatePopupForNextPlayer();
	void UpdatePopupForUseMulligan();
	void UpdatePopupForEndRound();
	void UpdatePopupForOK();

	UFUNCTION() void UseMulligan();
	UFUNCTION() void SetNextHole();
	UFUNCTION() void SetNextPlayer();
	UFUNCTION() void SetEndRound();
	UFUNCTION() void SetOK();
private:
	AInGameMode* GM;
};
