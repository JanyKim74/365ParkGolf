#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "InGameScoreBoardStatLineWidget.generated.h"


class UTextBlock;
class AInGameMode;
struct FRoundStat;

UCLASS()
class PARKDAY_API UInGameScoreBoardStatLineWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    virtual void NativeConstruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
        UTextBlock* TextBlock_Name;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
        UTextBlock* TextBlock_Rank;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
        UTextBlock* TextBlock_ShotCount;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
        UTextBlock* TextBlock_AverageDriverDistance;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
        UTextBlock* TextBlock_MaxDistance;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
        UTextBlock* TextBlock_Fairway_Settlement;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
        UTextBlock* TextBlock_Green_Accuracy;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
        UTextBlock* TextBlock_Green_PuttCount;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
        UTextBlock* TextBlock_PuttCount;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
        UTextBlock* TextBlock_Sand_Save;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
        UTextBlock* TextBlock_Par_Save;


public:
    void Init();
    void EmptyAllTextBlock();
    void UpdateStatLine();
    void SetLine(FString NickName, FRoundStat RoundStat);

    UPROPERTY()
    int32 SlotIndex;
    UPROPERTY()
    int32 PlayerIndex;
    UPROPERTY()
    AInGameMode* GM;
};
