#include "InGameScoreBoardStatWidget.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "ParkDay/InGameMode.h"
#include "ParkDay/Utils/UtilLibrary.h"
#include "ParkDay/GolfPlayerManager.h"
#include "ParkDay/GolfPlayer.h"
#include "ParkDay/Widgets/InGameScoreBoardStatLineWidget.h"

void UInGameScoreBoardStatWidget::NativeConstruct()
{
	Super::NativeConstruct();

	GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());

	Button_PlayerCard->OnPressed.AddDynamic(this, &UInGameScoreBoardStatWidget::HandleOnPressedPlayerCardButton);
	Button_Next->OnPressed.AddDynamic(this, &UInGameScoreBoardStatWidget::HandleOnPressedNextButton);

	Init();
}

void UInGameScoreBoardStatWidget::Init()
{
	StatLines.Empty();

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

	for (int32 i = 0; i < VerticalBox_Stats->GetAllChildren().Num(); i++)
	{
		UWidget* ChildWidget = VerticalBox_Stats->GetAllChildren()[i];
		UInGameScoreBoardStatLineWidget* LineWidget = Cast<UInGameScoreBoardStatLineWidget>(ChildWidget);
		if (LineWidget)
		{
			LineWidget->PlayerIndex = -1;
			LineWidget->SlotIndex = -1;

			StatLines.Add(LineWidget);
		}

		if (ActivePlayers.IsValidIndex(i))
		{
			LineWidget->PlayerIndex = ActivePlayers[i]->PlayerIndex;
			LineWidget->SlotIndex = ActivePlayers[i]->PlayerInfo.SlotIndex;
		}
	}
}

void UInGameScoreBoardStatWidget::UpdateScoreBoardStats()
{
	Init();

	for (UInGameScoreBoardStatLineWidget* LineWidget : StatLines)
	{
		if (LineWidget)
		{
			LineWidget->UpdateStatLine();
		}
	}
}

void UInGameScoreBoardStatWidget::HandleOnPressedPlayerCardButton()
{
	UUtilLibrary::LockButtonForSeconds(Button_PlayerCard, GetWorld(), 0.2f);
	if (GM)
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UInGameScoreBoardStatWidget::HandleOnPressedNextButton()
{
	if (GM)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		GM->SetShowScoreBoard(0);
	}
}

UInGameScoreBoardStatLineWidget* UInGameScoreBoardStatWidget::FindStatLineBySlotIndex(int32 SlotIndex)
{
	for (UInGameScoreBoardStatLineWidget* Line : StatLines)
	{
		if (IsValid(Line) && Line->SlotIndex == SlotIndex)
		{
			return Line;
		}
	}

	return nullptr;
}

UInGameScoreBoardStatLineWidget* UInGameScoreBoardStatWidget::FindStatLineByPlayerIndex(int32 PlayerIndex)
{
	for (UInGameScoreBoardStatLineWidget* Line : StatLines)
	{
		if (IsValid(Line) && Line->PlayerIndex == PlayerIndex)
		{
			return Line;
		}
	}

	return nullptr;
}
