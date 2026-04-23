// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "PlayerStructs.generated.h"

USTRUCT(BlueprintType)
struct FInGame_PlayerStatus
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerStatus")
	FString Name = TEXT("");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerStatus")
	int32 Stroke = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerStatus")
	float PinDistance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerStatus")
	int32 UnderPar = 0;
};