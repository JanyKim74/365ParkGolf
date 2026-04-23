#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InGameScoreBoardStatWidget.generated.h"

class UImage;
class UTextBlock;
class UVerticalBox;
class UButton;
class UUserWidget;
class AInGameMode;
class UVerticalBox;
class UInGameScoreBoardStatLineWidget;

UCLASS()
class PARKDAY_API UInGameScoreBoardStatWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    virtual void NativeConstruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
        UButton* Button_Next;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
        UButton* Button_PlayerCard;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
        UVerticalBox* VerticalBox_Stats;

public:
    void Init();

    UFUNCTION()
    void HandleOnPressedPlayerCardButton();
    UFUNCTION()
    void HandleOnPressedNextButton();

    void UpdateScoreBoardStats();
    UFUNCTION()
    UInGameScoreBoardStatLineWidget* FindStatLineBySlotIndex(int32 SlotIndex);
    UFUNCTION()
    UInGameScoreBoardStatLineWidget* FindStatLineByPlayerIndex(int32 PlayerIndex);
private:
    AInGameMode* GM;
    TArray<UInGameScoreBoardStatLineWidget*> StatLines;
};
