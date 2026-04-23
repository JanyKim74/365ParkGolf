#include "InGameScoreBoardWidget.h"
#include "Engine/DataTable.h"
#include "../InGameMode.h"
#include "Components/VerticalBox.h"
#include "ParkDay/Widgets/InGameScoreBoardLineWidget.h"
#include "ParkDay/Structs/DataTableStruct.h"
#include "ParkDay/GolfPlayerManager.h"
#include "ParkDay/GolfPlayer.h"
#include "Components/HorizontalBox.h"
#include "ParkDay/Utils/UtilLibrary.h"

UInGameScoreBoardWidget::UInGameScoreBoardWidget(const FObjectInitializer& ObjectInitializer) 
	: Super(ObjectInitializer)
{

}

void UInGameScoreBoardWidget::BuildTextureMapFromDataTable()
{
	ScoreIconMap.Empty();

	if (!GM->DT_ScoreIcon)
	{
		UE_LOG(LogTemp, Warning, TEXT("BuildTextureMapFromDataTable: DT_ScoreIcon is null"));
		return;
	}

	static const FString ContextString(TEXT("BuildTextureMapFromDataTable"));

	TArray<FScoreIcon*> Rows;
	GM->DT_ScoreIcon->GetAllRows(ContextString, Rows);

	for (const FScoreIcon* Row : Rows)
	{
		if (!Row)
		{
			continue;
		}

		if (!Row->Icon)
		{
			UE_LOG(LogTemp, Warning, TEXT("Id %d has null Texture."), Row->Score);
			continue;
		}

		
		if (ScoreIconMap.Contains(Row->Score))
		{
			UE_LOG(LogTemp, Warning, TEXT("Duplicate Id %d found. Overwriting."), Row->Score);
		}

		ScoreIconMap.Add(Row->Score, Row->Icon);
	}

	UE_LOG(LogTemp, Log, TEXT("BuildTextureMapFromDataTable: Loaded %d entries"), ScoreIconMap.Num());
}

void UInGameScoreBoardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());

	Button_Next->OnClicked.AddDynamic(this, &UInGameScoreBoardWidget::SetShow);
	Button_Stat->OnPressed.AddDynamic(this, &UInGameScoreBoardWidget::HandleOnPressedStatButton);
	Init();
	UpdateScoreBoard();
}

void UInGameScoreBoardWidget::SoftResetInGameInfo()
{
	AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
	GameMode->GameInfo.SoftReset();
}

void UInGameScoreBoardWidget::SetShow()
{
	UUtilLibrary::LockButtonForSeconds(Button_Next, GetWorld(), 0.33f);
	UUtilLibrary::LockButtonForSeconds(Button_Next_End, GetWorld(), 0.33f);

	FTimerHandle TH;
	GetWorld()->GetTimerManager().SetTimer(TH, [this]() 
		{
			AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
			GameMode->SetShowScoreBoard(0);
		}
	, 0.33f, false);
}

void UInGameScoreBoardWidget::Init()
{
	TArray<UWidget*> Children = VerticalBox_PlayerList->GetAllChildren();

	ScoreBoardLineArray.Empty();

	for (int32 i = 0 ; i < Children.Num() ; i++)
	{
		if (UInGameScoreBoardLineWidget* Line = Cast<UInGameScoreBoardLineWidget>(Children[i]))
		{
			if (i == 0)
			{
				FirstLine = Line;
			}
			else
			{
				Line->TextBlock_PlayerNickName->SetText(FText::GetEmpty());
				Line->Init();
				ScoreBoardLineArray.Add(Line);
			}
		}
	}

	for (UTextBlock* Block : FirstLine->ScoreTextArray)
	{
		Block->SetColorAndOpacity(FSlateColor(FColor(255.f, 255.f, 255.f)));
	}

	FirstLine->TextBlock_InPar_Total->SetColorAndOpacity(FSlateColor(FColor(255.f, 255.f, 255.f)));
	FirstLine->TextBlock_OutPar_Total->SetColorAndOpacity(FSlateColor(FColor(255.f, 255.f, 255.f)));
	FirstLine->TextBlock_ScoreTotal->SetColorAndOpacity(FSlateColor(FColor(255.f, 255.f, 255.f)));
	
	BuildTextureMapFromDataTable();


	TArray<AGolfPlayer*> ActivePlayers;
	if (GM && GM->PlayerManager)
	{
		for (AGolfPlayer* Player : GM->PlayerManager->GetPlayers())
		{
			if (IsValid(Player) && !Player->bIsPendingDelete)
			{
				ActivePlayers.Add(Player);
			}
		}
	}

	for (int32 i = 0; i < ScoreBoardLineArray.Num(); i++)
	{
		UInGameScoreBoardLineWidget* Line = ScoreBoardLineArray[i];
		if (!Line)
		{
			continue;
		}

		if (ActivePlayers.IsValidIndex(i))
		{
			Line->PlayerIndex = ActivePlayers[i]->PlayerIndex;
			Line->SlotIndex = ActivePlayers[i]->PlayerInfo.SlotIndex;
			Line->ScoreBoardWidget = this;
		}
		else
		{
			Line->PlayerIndex = INDEX_NONE;
			Line->SlotIndex = -1;
			Line->ScoreBoardWidget = this;
			Line->TextBlock_PlayerNickName->SetText(FText::GetEmpty());
			Line->ScoreArray.Empty();
		}
	}

	UpdateParScore();
}

void UInGameScoreBoardWidget::UpdateParScore()
{
	if (GM)
	{
		TArray<int32> ParScores = GM->GameInfo.SelectedMap.ParScores;

		FirstLine->Init();
		for (int32 Score : ParScores)
		{
			FirstLine->ScoreArray.Add(Score);
		}
		FirstLine->UpdateScoreTextBlock_FirstLine();
		FirstLine->UpdateOutCourseTotal();
		FirstLine->UpdateInCourseTotal();
		FirstLine->UpdateScoreTotal_FirstLine();
	}
}

void UInGameScoreBoardWidget::RemovePlayer(FPlayerInfo PlayerInfo)
{
	for (int32 i = 0; i < ScoreBoardLineArray.Num(); i++)
	{
		if (ScoreBoardLineArray[i] && ScoreBoardLineArray[i]->SlotIndex == PlayerInfo.SlotIndex)
		{
			ScoreBoardLineArray.RemoveAt(i);
			return;
		}
	}
}

void UInGameScoreBoardWidget::UpdateScoreBoard()
{
	if (GM)
	{
		GM->LoadGameInfoFromJSON();

		TArray<AGolfPlayer*> ActivePlayers;
		if (GM && GM->PlayerManager)
		{
			for (AGolfPlayer* Player : GM->PlayerManager->GetPlayers())
			{
				if (IsValid(Player) && !Player->bIsPendingDelete)
				{
					ActivePlayers.Add(Player);
				}
			}
		}

		if (Image_PlayHole)
		{
			FWidgetTransform CurrentTransform = Image_PlayHole->GetRenderTransform();
			CurrentTransform.Translation.X += 85.0f * (GM->CurrentHole - 1);
			Image_PlayHole->SetRenderTransform(CurrentTransform);
		}

		for (int32 i = 0; i < ScoreBoardLineArray.Num(); i++)
		{
			UInGameScoreBoardLineWidget* Line = ScoreBoardLineArray[i];
			if (!Line)
			{
				continue;
			}

			if (ActivePlayers.IsValidIndex(i))
			{
				Line->TextBlock_PlayerNickName->SetText(FText::FromString(ActivePlayers[i]->PlayerInfo.NickName));
				Line->PlayerIndex = ActivePlayers[i]->PlayerIndex;
				Line->SlotIndex = ActivePlayers[i]->PlayerInfo.SlotIndex;
				Line->ScoreBoardWidget = this;
			}
			else
			{
				Line->PlayerIndex = INDEX_NONE;
				Line->SlotIndex = -1;
				Line->ScoreBoardWidget = this;
				Line->TextBlock_PlayerNickName->SetText(FText::GetEmpty());
				Line->ScoreArray.Empty();
			}

			Line->UpdateScoreBoardLine();
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UInGameScoreBoardWidget::UpdateScoreBoard() ==> GM is null"));
	}
}

void UInGameScoreBoardWidget::DeleteMulliganForPlayer(int32 SlotIndex)
{
	if (GM)
	{
		for (int32 i = 0; i < GM->InGameScoreBoardWidgetInstance->ScoreBoardLineArray.Num(); i++)
		{
			UInGameScoreBoardLineWidget* Line = GM->InGameScoreBoardWidgetInstance->ScoreBoardLineArray[i];

			if (Line->SlotIndex == SlotIndex)
			{
				TArray<UWidget*> Mulligans = Line->HorizontalBox_Mulligan->GetAllChildren();
				for (UWidget* ForMulligan : Mulligans)
				{
					ForMulligan->SetVisibility(ESlateVisibility::Hidden);
				}
			}
		}
	}
}

void UInGameScoreBoardWidget::UpdateMulliganUse()
{
	if (GM)
	{
		for (int32 i = 0; i < GM->InGameScoreBoardWidgetInstance->ScoreBoardLineArray.Num(); i++)
		{
			UInGameScoreBoardLineWidget* Line = GM->InGameScoreBoardWidgetInstance->ScoreBoardLineArray[i];
			TArray<UWidget*> Mulligans = Line->HorizontalBox_Mulligan->GetAllChildren();

			// Hide all icons first, then enable for the matching player only.
			for (UWidget* ForMulligan : Mulligans)
			{
				if (ForMulligan)
				{
					ForMulligan->SetVisibility(ESlateVisibility::Hidden);
				}
			}

			AGolfPlayer* MatchingPlayer = nullptr;
			for (AGolfPlayer* Player : GM->PlayerManager->GetPlayers())
			{
				if (Player && Player->PlayerInfo.SlotIndex == Line->SlotIndex)
				{
					MatchingPlayer = Player;
					break;
				}
			}

			if (MatchingPlayer)
			{
				const int32 MaxHoles = FMath::Min(GM->CurrentHole, MatchingPlayer->PlayerInfo.HoleMulligans.Num());
				const int32 MaxIcons = FMath::Min(MaxHoles, Mulligans.Num());

				for (int32 j = 0; j < MaxIcons; j++)
				{
					if (MatchingPlayer->PlayerInfo.HoleMulligans[j])
					{
						if (UImage* Mulligan = Cast<UImage>(Mulligans[j]))
						{
							Mulligan->SetVisibility(ESlateVisibility::Visible);
						}
					}
				}
			}
		}
	}
}

void UInGameScoreBoardWidget::HandleOnPressedStatButton()
{
	UUtilLibrary::LockButtonForSeconds(Button_Stat, GetWorld(), 0.2f);

	if (GM)
	{
		GM->SetShowScoreStatBoard(true);
	}
}

UInGameScoreBoardLineWidget* UInGameScoreBoardWidget::FindPlayerLine(int32 SlotIndex)
{
	return FindPlayerLineBySlotIndex(SlotIndex);
}

UInGameScoreBoardLineWidget* UInGameScoreBoardWidget::FindPlayerLineBySlotIndex(int32 SlotIndex)
{
	for (UInGameScoreBoardLineWidget* Line : ScoreBoardLineArray)
	{
		if (IsValid(Line) && Line->SlotIndex == SlotIndex)
		{
			return Line;
		}
	}

	return nullptr;
}

UInGameScoreBoardLineWidget* UInGameScoreBoardWidget::FindPlayerLineByPlayerIndex(int32 PlayerIndex)
{
	for (UInGameScoreBoardLineWidget* Line : ScoreBoardLineArray)
	{
		if (IsValid(Line) && Line->PlayerIndex == PlayerIndex)
		{
			return Line;
		}
	}

	return nullptr;
}

