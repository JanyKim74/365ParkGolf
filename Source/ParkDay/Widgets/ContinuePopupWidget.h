// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ContinuePopupWidget.generated.h"

class UButton;
class AMenuGameMode;

UCLASS()
class PARKDAY_API UContinuePopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
		UButton* Button_Confirm;
	UPROPERTY(meta = (BindWidget))
		UButton* Button_Cancel;


public:
	UFUNCTION()
		void HandleOnClickButtonConfirm();
	UFUNCTION()
		void HandleOnClickButtonCancel();

private:
	AMenuGameMode* GM;
};
