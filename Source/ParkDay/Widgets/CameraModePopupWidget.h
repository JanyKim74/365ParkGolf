// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CameraModePopupWidget.generated.h"

class UButton;
class UCheckBox;
class AInGameMode;

UCLASS()
class PARKDAY_API UCameraModePopupWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeConstruct() override;
public:
	UPROPERTY(meta = (BindWidget)) UButton* Button_Confirm;
	UPROPERTY(meta = (BindWidget)) UCheckBox* CheckBox_Static;
	UPROPERTY(meta = (BindWidget)) UCheckBox* CheckBox_Move;

	UFUNCTION() void HandleOnPressedMoveCheckBox(bool bIsChecked);
	UFUNCTION() void HandleOnPressedStaticCheckBox(bool bIsChecked);
	UFUNCTION() void HandleOnPressedConfirmButton();

private:
	AInGameMode* GM;
};
