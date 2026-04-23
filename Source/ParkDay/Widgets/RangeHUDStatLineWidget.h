#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ParkDay/GolfDataStructures.h"
#include "RangeHUDStatLineWidget.generated.h"


class AInGameMode;
class UButton;
class UTextBlock;
class UImage;
class UWrapBox;
class UCanvasPanel;
class USlider;
class UCheckBox;
class UGridPanel;

UCLASS()
class PARKDAY_API URangeHUDStatLineWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget)) UTextBlock* TextBlock_ShotCount;
    UPROPERTY(meta = (BindWidget)) UTextBlock* TextBlock_Distance;
    UPROPERTY(meta = (BindWidget)) UTextBlock* TextBlock_BallSpeed;
    UPROPERTY(meta = (BindWidget)) UTextBlock* TextBlock_LaunchAngle;
    UPROPERTY(meta = (BindWidget)) UTextBlock* TextBlock_DirectionAngle;
    UPROPERTY(meta = (BindWidget)) UTextBlock* TextBlock_RemainDistance;

public:
    UPROPERTY()
    int32 ShotIndex;
    UPROPERTY()
    FShotStat ShotStat;

    UPROPERTY()
    bool bIsAverageLine = false;

public:
    UFUNCTION()
    void SetEmptyAllText();

    void SetRedText();
    void SetBlackText();

    UFUNCTION()
    void HandleOnAddShotStat(FShotStat InShotStat);

    UFUNCTION()
		void HandleOnBallStop(FShotStat InShoStat);
	UFUNCTION()
		void HandleOnModeChange();

private:
    AInGameMode* GM;
};
