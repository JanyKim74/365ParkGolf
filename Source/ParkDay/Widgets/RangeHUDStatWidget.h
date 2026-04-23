#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RangeHUDStatWidget.generated.h"


class AInGameMode;
class UButton;
class UTextBlock;
class UImage;
class UWrapBox;
class UCanvasPanel;
class USlider;
class UCheckBox;
class UGridPanel;
class URangeHUDStatLineWidget;

UCLASS()
class PARKDAY_API URangeHUDStatWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (Bindwidget)) UButton* Button_close;
    UPROPERTY(meta = (Bindwidget)) UGridPanel* GridPanel_StatLines;
    UPROPERTY(meta = (Bindwidget)) URangeHUDStatLineWidget* AverageLine;
    

    UFUNCTION() void HandleOnPressedCloseButton();
};
