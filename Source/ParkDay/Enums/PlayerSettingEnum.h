// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "PlayerSettingEnum.generated.h"

UENUM(BlueprintType)
enum class EPlayerSetting_Consid : uint8
{
    NONE     UMETA(DisplayName = "NONE"),
    D_50CM           UMETA(DisplayName = "0.5M"),
    D_100CM             UMETA(DisplayName = "1M"),
    D_150CM             UMETA(DisplayName = "1.5M")
};

UENUM(BlueprintType)
enum class EPlayerSetting_ContinuePutting : uint8
{
    NO     UMETA(DisplayName = "NO"),
    YES    UMETA(DisplayName = "YES")
};

UENUM(BlueprintType)
enum class EPlayerSetting_GrassCondition : uint8
{
    SLOW            UMETA(DisplayName = "SLOW"),
    NOMAL           UMETA(DisplayName = "NOMAL"),
    SLIGHTLY_FAST   UMETA(DisplayName = "SLIGHTLY_FAST"),
    FAST             UMETA(DisplayName = "FAST")
};

UENUM(BlueprintType)
enum class EPlayerSetting_Mulligan : uint8
{
    NONE     UMETA(DisplayName = "NONE"),
    ONE_TIME           UMETA(DisplayName = "ONE_TIME"),
    THREE_TIME             UMETA(DisplayName = "THREE_TIME"),
    UNLIMITED             UMETA(DisplayName = "UNLIMITED")
};

UENUM(BlueprintType)
enum class EPlayerSetting_PinLocation : uint8
{
    FRONT     UMETA(DisplayName = "FRONT"),
    BACK           UMETA(DisplayName = "BACK"),
    LEFT             UMETA(DisplayName = "LEFT"),
    RIGHT             UMETA(DisplayName = "RIGHT"),
    RANDOM             UMETA(DisplayName = "RANDOM")
};

UENUM(BlueprintType)
enum class EPlayerSetting_SelectCourse : uint8
{
    A     UMETA(DisplayName = "A"),
    B           UMETA(DisplayName = "B"),
    ALL             UMETA(DisplayName = "ALL")
};