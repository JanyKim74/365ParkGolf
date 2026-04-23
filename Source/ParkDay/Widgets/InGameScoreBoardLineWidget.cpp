#include "InGameScoreBoardLineWidget.h"
#include "../InGameMode.h"

#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "ParkDay/InGameMode.h"
#include "ParkDay/GolfDataStructures.h"
#include "ParkDay/Widgets/InGameScoreBoardWidget.h"
#include "ParkDay/GolfPlayerManager.h"
#include "ParkDay/GolfPlayer.h"

void UInGameScoreBoardLineWidget::NativeConstruct()
{
	GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
	Init();
}

void UInGameScoreBoardLineWidget::Init()
{
	TextBlock_PlayerNickName->SetText(FText::GetEmpty());
	TextBlock_InPar_Total->SetText(FText::GetEmpty());
	TextBlock_OutPar_Total->SetText(FText::GetEmpty());
	TextBlock_ScoreTotal->SetText(FText::GetEmpty());

	ScoreTextArray.Empty();
	ScoreImageArray.Empty();

	for (UWidget* CanvasWidget : HorizontalBox_OutCourse->GetAllChildren())
	{
		UCanvasPanel* CanvasPanel = Cast<UCanvasPanel>(CanvasWidget);
		for (UWidget* Widget : CanvasPanel->GetAllChildren())
		{
			if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
			{
				TextBlock->SetText(FText::GetEmpty());
				ScoreTextArray.Add(TextBlock);
			}
		}
	}

	for (UWidget* CanvasWidget : HorizontalBox_InCourse->GetAllChildren())
	{
		UCanvasPanel* CanvasPanel = Cast<UCanvasPanel>(CanvasWidget);
		for (UWidget* Widget : CanvasPanel->GetAllChildren())
		{
			if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
			{
				TextBlock->SetText(FText::GetEmpty());
				ScoreTextArray.Add(TextBlock);
			}
		}
	}

	for (UWidget* CanvasWidget : HorizontalBox_OutCourse->GetAllChildren())
	{
		UCanvasPanel* CanvasPanel = Cast<UCanvasPanel>(CanvasWidget);
		for (UWidget* Widget : CanvasPanel->GetAllChildren())
		{
			if (UImage* Image = Cast<UImage>(Widget))
			{
				Image->Brush.DrawAs = ESlateBrushDrawType::NoDrawType;
				ScoreImageArray.Add(Image);
			}
		}
	}

	for (UWidget* CanvasWidget : HorizontalBox_InCourse->GetAllChildren())
	{
		UCanvasPanel* CanvasPanel = Cast<UCanvasPanel>(CanvasWidget);
		for (UWidget* Widget : CanvasPanel->GetAllChildren())
		{
			if (UImage* Image = Cast<UImage>(Widget))
			{
				Image->Brush.DrawAs = ESlateBrushDrawType::NoDrawType;
				ScoreImageArray.Add(Image);
			}
		}
	}
}

void UInGameScoreBoardLineWidget::UpdateScore()
{
    if (!GM || !GM->PlayerManager)
    {
        ScoreArray.Empty();
        return;
    }

    const TArray<AGolfPlayer*>& Players = GM->PlayerManager->GetPlayers();
    if (!Players.IsValidIndex(PlayerIndex) || !IsValid(Players[PlayerIndex]))
    {
        ScoreArray.Empty();
        return;
    }

    ScoreArray = Players[PlayerIndex]->PlayerInfo.HoleScores;
}

void UInGameScoreBoardLineWidget::UpdateScoreTextBlock_FirstLine()
{
	int32 StartIndex = 0;
	int32 EndIndex = 18;
	int32 SelectCourseOptionValue = GM->GameInfo.GameOptions.SelectCourse;

	//0, 2�� 0���ͽ���
	StartIndex = SelectCourseOptionValue == 1 ? 9 : 0;
	//1,2 �� 18����
	EndIndex = SelectCourseOptionValue == 0 ? 9 : 18;

	for (int32 i = StartIndex; i < EndIndex; i++)
	{
		if (ScoreArray[i] != 100)
		{
			ScoreTextArray[i]->SetText(FText::AsNumber(ScoreArray[i]));
		}
	}
}

void UInGameScoreBoardLineWidget::UpdateScoreTextBlock()
{
	for (int32 i = 0 ; i < ScoreArray.Num() ; i ++)
	{
		if (ScoreArray[i] != 100)
		{
			ScoreTextArray[i]->SetText(FText::AsNumber(ScoreArray[i]));
		}
	}
}

void UInGameScoreBoardLineWidget::UpdateOutCourseTotal()
{
	if (GM->GameInfo.GameOptions.SelectCourse == 1 ||
		ScoreArray.Num() <= 0)
	{
		TextBlock_OutPar_Total->SetText(FText::GetEmpty());
		return;
	}

	int32 TotalScore = 0;

	for (int32 i = 0 ; i < 9 ; i ++)
	{
		if (ScoreArray.IsValidIndex(i))
		{
			int32 Score = ScoreArray[i];
			if (Score != 100)
				TotalScore += Score;
		}
		else
		{
			break;
		}
	}
	TextBlock_OutPar_Total->SetText(FText::AsNumber(TotalScore));

	UE_LOG(LogTemp, Log, TEXT("UInGameScoreBoardLineWidget::UpdateOutCourseTotal() ==> Success"));
}
void UInGameScoreBoardLineWidget::UpdateInCourseTotal()
{
	if (GM->GameInfo.GameOptions.SelectCourse == 0 ||
		ScoreArray.Num() <= 0)
	{
		TextBlock_InPar_Total->SetText(FText::GetEmpty());
		return;
	}

	int32 TotalScore = 0;

	for (int32 i = 9; i < 18; i++)
	{
		if (ScoreArray.IsValidIndex(i))
		{
			int32 Score = ScoreArray[i];
			if (Score != 100)
				TotalScore += Score;
		}
		else
		{
			break;
		}
	}

	TextBlock_InPar_Total->SetText(FText::AsNumber(TotalScore));
	UE_LOG(LogTemp, Log, TEXT("UInGameScoreBoardLineWidget::UpdateInCourseTotal() ==> Success"));
}
void UInGameScoreBoardLineWidget::UpdateScoreTotal()
{
	if (ScoreArray.Num() > 0)
	{
		int32 TotalScore = 0;

		for (int32 Score : ScoreArray)
		{
			if (Score != 100)
				TotalScore += Score;
		}
		TextBlock_ScoreTotal->SetText(FText::AsNumber(TotalScore));
		UE_LOG(LogTemp, Log, TEXT("UInGameScoreBoardLineWidget::UpdateScoreTotal() ==> Success"));
	}
	else
	{
		TextBlock_ScoreTotal->SetText(FText::GetEmpty());
	}
}

void UInGameScoreBoardLineWidget::UpdateScoreTotal_FirstLine()
{
	int32 TotalScore = 0;
	int32 StartIndex = 0;
	int32 EndIndex = 18;
	int32 SelectCourseOptionValue = GM->GameInfo.GameOptions.SelectCourse;

	//0, 2�� 0���ͽ���
	StartIndex = SelectCourseOptionValue == 1 ? 9 : 0;
	//1,2 �� 18����
	EndIndex = SelectCourseOptionValue == 0 ? 9 : 18;

	for (int32 i = StartIndex; i < EndIndex; i++)
	{
		TotalScore += ScoreArray[i];
	}

	TextBlock_ScoreTotal->SetText(FText::AsNumber(TotalScore));
}

void UInGameScoreBoardLineWidget::UpdateNickName()
{
    if (!GM || !GM->PlayerManager)
    {
        TextBlock_PlayerNickName->SetText(FText::GetEmpty());
        return;
    }

    const TArray<AGolfPlayer*>& Players = GM->PlayerManager->GetPlayers();
    if (!Players.IsValidIndex(PlayerIndex) || !IsValid(Players[PlayerIndex]))
    {
        TextBlock_PlayerNickName->SetText(FText::GetEmpty());
        return;
    }

    FString PlayerNickName = Players[PlayerIndex]->PlayerInfo.NickName;
    TextBlock_PlayerNickName->SetText(FText::FromString(PlayerNickName));
    UE_LOG(LogTemp, Log, TEXT("UInGameScoreBoardLineWidget::UpdateNickName() ==> Success"));
}

void UInGameScoreBoardLineWidget::UpdateScoreBoardLine()
{
	UpdateScore();
	UpdateNickName();
	UpdateScoreTextBlock();
	UpdateOutCourseTotal();
	UpdateInCourseTotal();
	UpdateScoreTotal();
	UpdateScoreIcon();
}

void UInGameScoreBoardLineWidget::UpdateScoreIcon()
{
	if (ScoreBoardWidget)
	{
		for (int32 i = 0; i < ScoreArray.Num(); i++)
		{
			int32 Score = ScoreArray[i];

			if (Score != 100)
			{
				UImage* ScoreImage = ScoreImageArray[i];

				//홀인원
				if (Score == (ScoreBoardWidget->FirstLine->ScoreArray[i] * -1) + 1)
				{
					ScoreImage->SetBrushFromTexture(ScoreBoardWidget->ScoreIconMap[-4]);
					ScoreImage->Brush.DrawAs = ESlateBrushDrawType::Image;
				}
				else if (Score == ScoreBoardWidget->FirstLine->ScoreArray[i])	//double par
				{
					ScoreImage->SetBrushFromTexture(ScoreBoardWidget->ScoreIconMap[100]);
					ScoreImage->Brush.DrawAs = ESlateBrushDrawType::Image;
				}
				else if (ScoreBoardWidget->ScoreIconMap.Find(Score) != nullptr)
				{
					ScoreImage->SetBrushFromTexture(ScoreBoardWidget->ScoreIconMap[Score]);
					ScoreImage->Brush.DrawAs = ESlateBrushDrawType::Image;
				}
			}
		}
	}
}

