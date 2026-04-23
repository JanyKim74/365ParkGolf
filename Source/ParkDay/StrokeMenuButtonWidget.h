// StrokeMenuButtonWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h" // 여전히 UButton을 사용할 수 있도록 포함
#include "StrokeMenuButtonWidget.generated.h"

class AInGameMode;

UCLASS()
class PARKDAY_API UStrokeMenuButtonWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget)) UButton* Button_Menu;

};