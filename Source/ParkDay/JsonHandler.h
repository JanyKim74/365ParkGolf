// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GolfDataStructures.h"
#include "JsonHandler.generated.h"

// JSON 데이터 읽기/쓰기를 처리하는 유틸리티 클래스
UCLASS()
class UJsonHandler : public UObject
{
    GENERATED_BODY()
public:
    // 게임 정보를 JSON 파일로 저장
    UFUNCTION(BlueprintCallable, Category = "Json")
        static bool SaveGameInfoToJson(const FGameInfo& GameInfo, const FString& FilePath);

    // JSON 파일에서 게임 정보를 로드
    UFUNCTION(BlueprintCallable, Category = "Json")
        static bool LoadGameInfoFromJson(FGameInfo& OutGameInfo, const FString& FilePath);

    // FPlayerInfo를 JSON 객체로 변환
    static TSharedPtr<FJsonObject> PlayerInfoToJson(const FPlayerInfo& PlayerInfo);

    // JSON 객체를 FPlayerInfo로 변환
    static bool JsonToPlayerInfo(const TSharedPtr<FJsonObject>& JsonObject, FPlayerInfo& OutPlayerInfo);

    // FGameOptionInfo를 JSON 객체로 변환
    static TSharedPtr<FJsonObject> GameOptionInfoToJson(const FGameOptionInfo& GameOptionInfo);

    // JSON 객체를 FGameOptionInfo로 변환
    static bool JsonToGameOptionInfo(const TSharedPtr<FJsonObject>& JsonObject, FGameOptionInfo& OutGameOptionInfo);

    // FMapInfo를 JSON 객체로 변환
    static TSharedPtr<FJsonObject> MapInfoToJson(const FMapInfo& MapInfo);

    // JSON 객체를 FMapInfo로 변환
    static bool JsonToMapInfo(const TSharedPtr<FJsonObject>& JsonObject, FMapInfo& OutMapInfo);

    static bool LoadSystemConfigFromJson(FSystemConfig& OutConfig, const FString& FilePath);
    static bool SaveSystemConfigToJson(const FSystemConfig& Config, const FString& FilePath);

    // ✅ RoundStat (추가)
    static TSharedPtr<FJsonObject> RoundStatToJson(const FRoundStat& Stat);
    static bool JsonToRoundStat(const TSharedPtr<FJsonObject>& JsonObject, FRoundStat& OutStat);
};