// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerProfileWidget.generated.h"


class AInGameMode;
UCLASS()
class PARKDAY_API UPlayerProfileWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeConstruct() override;

	UPROPERTY(BlueprintReadWrite, EditAnyWhere)
	bool bIsInGame = false;

	UFUNCTION(BlueprintCallable)
	bool CheckLastPlayer();

private:
	AInGameMode* GM;
};
