#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "InGameEnum.generated.h"

UENUM(BlueprintType)
enum class EBallLocation : uint8
{
    BUNKER     UMETA(DisplayName = "Bunker"),
    FAIR     UMETA(DisplayName = "Fair"),
    ROUGH     UMETA(DisplayName = "Rough"),
    TEE     UMETA(DisplayName = "Tee"),
    WATER     UMETA(DisplayName = "Water")
};

UENUM(BlueprintType)
enum class EMenuButtonType : uint8
{
    NONE            UMETA(DisplayName = "None"),
    MULLIGAN        UMETA(DisplayName = "Mulligan"),
    NEXT_HOLE       UMETA(DisplayName = "NextHole"),
    BULTADROP       UMETA(DisplayName = "BultaDrop"),
    SWING_MOTION    UMETA(DisplayName = "SwingMotion"),
    PREVIEW         UMETA(DisplayName = "Preview"),
    CAMERA_MODE     UMETA(DisplayName = "CameraMode"),
    SKIP_TURN       UMETA(DisplayName = "SkipTurn"),
    OK              UMETA(DisplayName = "Ok"),
    END_ROUND       UMETA(DisplayName = "EndRound")
};

