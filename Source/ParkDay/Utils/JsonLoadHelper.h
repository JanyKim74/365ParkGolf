#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"   

class PARKDAY_API JsonLoadHelper
{
public:
    static bool LoadJsonObject(const FString& RelativeFilePath, TSharedPtr<FJsonObject>& OutObject)
    {
        // ✅ 상대경로 → ProjectContentDir() 기준으로 변환
        // ✅ 절대경로 → 그대로 사용
        const FString FullPath = FPaths::IsRelative(RelativeFilePath)
            ? FPaths::ConvertRelativePathToFull(
                FPaths::Combine(FPaths::ProjectContentDir(), RelativeFilePath))
            : RelativeFilePath;

        FString JsonString;
        if (!FFileHelper::LoadFileToString(JsonString, *FullPath))
        {
            UE_LOG(LogTemp, Error, TEXT("[JsonLoadHelper] 파일 로드 실패: %s"), *FullPath);
            return false;
        }
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
        return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
    }

    static bool LoadSaveJsonObject(const FString& SaveFileName, TSharedPtr<FJsonObject>& OutObject)
    {
        // ✅ ConvertRelativePathToFull 제거 - 상대경로 그대로 FileHelper에 넘김
        const FString FullPath = FPaths::ProjectSavedDir() / SaveFileName;

        FString JsonString;
        if (!FFileHelper::LoadFileToString(JsonString, *FullPath))
        {
            UE_LOG(LogTemp, Error, TEXT("[JsonLoadHelper] Path: %s"), *FullPath);
            return false;
        }
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
        return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
    }

    template<typename TStructType>
    static bool LoadJsonToStruct(const FString& RelativeFilePath, TStructType& OutStruct)
    {
        TSharedPtr<FJsonObject> JsonObject;
        if (!LoadJsonObject(RelativeFilePath, JsonObject)) return false;
        return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), &OutStruct, 0, 0);
    }

    template<typename TStructType>
    static bool LoadSaveJsonToStruct(const FString& SaveFileName, TStructType& OutStruct)
    {
        TSharedPtr<FJsonObject> JsonObject;
        if (!LoadSaveJsonObject(SaveFileName, JsonObject)) return false;
        return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), &OutStruct, 0, 0);
    }
};