#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ParkDay/GolfDataStructures.h"

#include "InGameScoreBoardWidget.generated.h"


class UImage;
class UTextBlock;
class UVerticalBox;
class UButton;
class UUserWidget;
class UInGameScoreBoardLineWidget;

class AInGameMode;

class UDataTable;

UCLASS()
class PARKDAY_API UInGameScoreBoardWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UInGameScoreBoardWidget(const FObjectInitializer& ObjectInitializer);

    UPROPERTY()
        UDataTable* DT_ScoreIcon;

    UPROPERTY()
    TMap<int32, UTexture2D*> ScoreIconMap;

    	UFUNCTION(BlueprintCallable, Category="Data")
	void BuildTextureMapFromDataTable();

    virtual void NativeConstruct() override;

    UFUNCTION()
    void RemovePlayer(FPlayerInfo PlayerInfo);
        UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UImage* Image_Panel;

        UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* TextBlock_OutCourse;

        UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* TextBlock_InCourse;

        UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UVerticalBox* VerticalBox_PlayerList;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UImage* Image_PlayHole;

    //    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    //UUserWidget* WBP_InGame_ScoreBoard_Line;

    //    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    //UUserWidget* WBP_InGame_ScoreBoard_Line_1;
    //    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    //UUserWidget* WBP_InGame_ScoreBoard_Line_2;
    //    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    //UUserWidget* WBP_InGame_ScoreBoard_Line_3;
    //    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    //UUserWidget* WBP_InGame_ScoreBoard_Line_4;
    //    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    //UUserWidget* WBP_InGame_ScoreBoard_Line_5;
    //    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    //UUserWidget* WBP_InGame_ScoreBoard_Line_6;
    //    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    //UUserWidget* WBP_InGame_ScoreBoard_Line_7;

        UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
            UButton* Button_Next;
        UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
			UButton* Button_Next_End;
		UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
			UButton* Button_Stat;

public:
    UFUNCTION(BlueprintCallable)
    void SoftResetInGameInfo();

    UFUNCTION()
    void UpdateMulliganUse();

    UFUNCTION()
    void HandleOnPressedStatButton();

    UFUNCTION()
    UInGameScoreBoardLineWidget* FindPlayerLine(int32 SlotIndex);
    UFUNCTION()
    UInGameScoreBoardLineWidget* FindPlayerLineBySlotIndex(int32 SlotIndex);
    UFUNCTION()
    UInGameScoreBoardLineWidget* FindPlayerLineByPlayerIndex(int32 PlayerIndex);

    UFUNCTION()
        void SetShow();
    UFUNCTION()
    void Init();
    UFUNCTION()
    void UpdateParScore();
    UFUNCTION()
    void UpdateScoreBoard();
    UFUNCTION()
    void DeleteMulliganForPlayer(int32 SlotIndex);

    UInGameScoreBoardLineWidget* FirstLine;

private:
    TArray<UInGameScoreBoardLineWidget*> ScoreBoardLineArray;
    AInGameMode* GM;
};
