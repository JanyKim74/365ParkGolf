// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ParkDay/Structs/CorseStruct.h"
#include "JsonSaver.generated.h"

/**
 * 
 */
UCLASS()
class PARKDAY_API UJsonSaver : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
    // �������Ʈ�� �Լ�
    UFUNCTION(BlueprintCallable, Category = "JSON")
    static bool SavePlayerSettingsToJson(const FPlayerSetting& Settings, const FString& FileName);

};

