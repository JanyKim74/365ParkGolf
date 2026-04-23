#include "JsonLoader.h"   
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

/**
 * UJsonLoader::LoadJsonObject
 */
bool UJsonLoader::LoadJsonObject(const FString& RelativeFilePath, TSharedPtr<FJsonObject>& OutJsonObject)
{
    const FString FullPath = RelativeFilePath;

    FString JsonRaw;
    if (!FFileHelper::LoadFileToString(JsonRaw, *FullPath))
    {
        UE_LOG(LogTemp, Error, TEXT("[LoadJsonObject] JSON File read error: %s"), *FullPath);
        return false;
    }


    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonRaw);
    TSharedPtr<FJsonObject> JsonObj = MakeShareable(new FJsonObject());
    if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("[LoadJsonObject] JSON file parsing error: %s"), *FullPath);
        return false;
    }

    OutJsonObject = JsonObj;
    return true;
}

bool UJsonLoader::LoadSaveJsonObject(const FString& SaveFileName, TSharedPtr<FJsonObject>& OutJsonObject)
{
    // Content ���� ���� ������ ����
    const FString FullPath = FPaths::ProjectSavedDir() / SaveFileName;

    // 1) JSON ������ FString���� �б�
    FString JsonRaw;
    if (!FFileHelper::LoadFileToString(JsonRaw, *FullPath))
    {
        UE_LOG(LogTemp, Error, TEXT("[LoadJsonObject] JSON File read error: %s"), *FullPath);
        return false;
    }


    // 2) FString �� FJsonObject �Ľ�
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonRaw);
    TSharedPtr<FJsonObject> JsonObj = MakeShareable(new FJsonObject());
    if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("[LoadJsonObject] JSON file parsing error: %s"), *FullPath);
        return false;
    }

    OutJsonObject = JsonObj;
    return true;
}
