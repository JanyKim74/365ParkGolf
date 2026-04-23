#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "InGameScoreBoardLineWidget.generated.h"

class UHorizontalBox;
class UInGameScoreBoardWidget;
class AInGameMode;
class UImage;
class UTextBlock;

UCLASS()
class PARKDAY_API UInGameScoreBoardLineWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (Bindwidget))
		UHorizontalBox* HorizontalBox_OutCourse;
	UPROPERTY(meta = (Bindwidget))
		UHorizontalBox* HorizontalBox_InCourse;
	UPROPERTY(meta = (Bindwidget))
		UTextBlock* TextBlock_OutPar_Total;
	UPROPERTY(meta = (Bindwidget))
		UTextBlock* TextBlock_InPar_Total;
	UPROPERTY(meta = (Bindwidget))
		UTextBlock* TextBlock_ScoreTotal;
	UPROPERTY(meta = (Bindwidget))
		UTextBlock* TextBlock_PlayerNickName;
	UPROPERTY(meta = (Bindwidget))
		UHorizontalBox* HorizontalBox_Mulligan;

public:
	void Init();

	UFUNCTION()
	void UpdateScore();
	UFUNCTION()
	void UpdateScoreTextBlock();
	UFUNCTION()
	void UpdateOutCourseTotal();
	UFUNCTION()
	void UpdateInCourseTotal();
	UFUNCTION()
	void UpdateScoreTotal();
	UFUNCTION()
	void UpdateNickName();
	UFUNCTION()
	void UpdateScoreBoardLine();
	UFUNCTION()
	void UpdateScoreIcon();

	void UpdateScoreTextBlock_FirstLine();
	void UpdateScoreTotal_FirstLine();

public:
	TArray<UTextBlock*> ScoreTextArray;
	TArray<int32> ScoreArray;
	TArray<UImage*> ScoreImageArray;
	int32 SlotIndex;
	int32 PlayerIndex;
	UInGameScoreBoardWidget* ScoreBoardWidget;
private:
	AInGameMode* GM;
};
