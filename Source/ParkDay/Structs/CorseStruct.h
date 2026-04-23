// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ParkDay/Enums/PlayerSettingEnum.h"
#include "CorseStruct.generated.h"

USTRUCT(BlueprintType)
struct FCCName
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CCName")
	FString CCName;
};

USTRUCT(BlueprintType)
struct FCCList
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CCList")
	FString Desc;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CCList")
	TArray<FCCName> CCNames;
};

USTRUCT(BlueprintType)
struct FHoleInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HoleInfo")
	int32 Index = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HoleInfo")
	int32 ParCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HoleInfo")
	float SunRot = 0.f;
};

USTRUCT(BlueprintType)
struct FFieldMapInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieldMapInfo")
	FString CCname = TEXT("");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieldMapInfo")
	FString PakFile = TEXT("");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieldMapInfo")
	int32 CourseLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieldMapInfo")
	int32 Area = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieldMapInfo")
	int32 Sublevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieldMapInfo")
	FString Address = TEXT("");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieldMapInfo")
	FString OutCourse = TEXT("");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieldMapInfo")
	FString InCourse = TEXT("");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieldMapInfo")
	FString Distance = TEXT("");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieldMapInfo")
	int32 Recommend = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FieldMapInfo")
	TArray<FHoleInfo> HoleInfos;
};

USTRUCT(BlueprintType)
struct FPlayerSetting
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerSetting")
	EPlayerSetting_SelectCourse SelectCourse = EPlayerSetting_SelectCourse::ALL;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerSetting")
	EPlayerSetting_Mulligan Mulligan = EPlayerSetting_Mulligan::NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerSetting")
	EPlayerSetting_PinLocation PinLocation = EPlayerSetting_PinLocation::FRONT;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerSetting")
	EPlayerSetting_Consid Consid = EPlayerSetting_Consid::NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerSetting")
	EPlayerSetting_GrassCondition GrassCondition = EPlayerSetting_GrassCondition::NOMAL;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerSetting")
	bool ContinuePutting = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerSetting")
	uint8 CameraMode = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerSetting")
	bool SwingMotion = false;
};