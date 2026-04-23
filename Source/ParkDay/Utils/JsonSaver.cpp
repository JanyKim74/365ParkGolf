// Fill out your copyright notice in the Description page of Project Settings.

#include "JsonSaver.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"
#include "Misc/Base64.h"

bool UJsonSaver::SavePlayerSettingsToJson(const FPlayerSetting& Settings, const FString& FileName)
{
    FString JsonString;
    if (!FJsonObjectConverter::UStructToJsonObjectString(Settings, JsonString))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to convert struct to JSON"));
        return false;
    }

    FString FullPath = FPaths::ProjectSavedDir() / FileName;

    bool bSaved = FFileHelper::SaveStringToFile(JsonString, *FullPath);
    if (bSaved)
    {
        UE_LOG(LogTemp, Log, TEXT("Saved JSON to: %s"), *FullPath);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to save JSON file: %s"), *FullPath);
    }

    return bSaved;
}