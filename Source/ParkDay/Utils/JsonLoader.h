#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "../Structs/CorseStruct.h"
#include "JsonLoadHelper.h"
#include "ParkDay/GolfDataStructures.h"
#include "JsonLoader.generated.h"


/**
 * 
 */
UCLASS()
class PARKDAY_API UJsonLoader : public UObject
{
    GENERATED_BODY()

public:
    static bool LoadJsonObject(const FString& RelativeFilePath, TSharedPtr<FJsonObject>& OutJsonObject);
    static bool LoadSaveJsonObject(const FString& SaveFileName, TSharedPtr<FJsonObject>& OutJsonObject);

    
    UFUNCTION(BlueprintCallable, Category = "JSON")
        static bool LoadGameOptionFromJson(const FString& RelativeFilePath, FDefaultGameOption& OutData)
    {
		return JsonLoadHelper::LoadJsonToStruct<FDefaultGameOption>(RelativeFilePath, OutData);
    }

    UFUNCTION(BlueprintCallable, Category = "JSON")
        static bool LoadCCListFromJson(const FString& RelativeFilePath, FCCList& OutData)
    {
        return JsonLoadHelper::LoadJsonToStruct<FCCList>(RelativeFilePath, OutData);
    }

    UFUNCTION(BlueprintCallable, Category = "JSON")
        static bool LoadFieldMapInfoFromJson(const FString& RelativeFilePath, FFieldMapInfo& OutData)
    {
        return JsonLoadHelper::LoadJsonToStruct<FFieldMapInfo>(RelativeFilePath, OutData);
    }

    UFUNCTION(BlueprintCallable, Category = "JSON")
        static bool LoadPlayerSettingFromJson(const FString& SaveFileName, FPlayerSetting& OutData)
    {
        return JsonLoadHelper::LoadSaveJsonToStruct<FPlayerSetting>(SaveFileName, OutData);
	}

	UFUNCTION(BlueprintCallable, Category = "JSON")
		static bool LoadAdminConfigFromJson(const FString& SaveFileName, FAdminConfig& OutData)
	{
		return JsonLoadHelper::LoadSaveJsonToStruct<FAdminConfig>(SaveFileName, OutData);
	}
};