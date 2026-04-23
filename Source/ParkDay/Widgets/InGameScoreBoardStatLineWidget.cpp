#include "InGameScoreBoardStatLineWidget.h"
#include "Components/Button.h"
#include "ParkDay/InGameMode.h"
#include "ParkDay/GolfPlayerManager.h"
#include "ParkDay/GolfPlayer.h"
#include "ParkDay/GolfBall.h"
#include "ParkDay/Utils/UtilLibrary.h"

void UInGameScoreBoardStatLineWidget::NativeConstruct()
{
	Super::NativeConstruct();

	GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
	Init();
}

void UInGameScoreBoardStatLineWidget::Init()
{
	EmptyAllTextBlock();
}

void UInGameScoreBoardStatLineWidget::EmptyAllTextBlock()
{
	TextBlock_Name->SetText(FText::GetEmpty());
	TextBlock_ShotCount->SetText(FText::GetEmpty());
	TextBlock_AverageDriverDistance->SetText(FText::GetEmpty());
	TextBlock_Fairway_Settlement->SetText(FText::GetEmpty());
	TextBlock_Green_Accuracy->SetText(FText::GetEmpty());
	TextBlock_Green_PuttCount->SetText(FText::GetEmpty());
	TextBlock_MaxDistance->SetText(FText::GetEmpty());
	TextBlock_Par_Save->SetText(FText::GetEmpty());
	TextBlock_PuttCount->SetText(FText::GetEmpty());
	TextBlock_Rank->SetText(FText::GetEmpty());
	TextBlock_Sand_Save->SetText(FText::GetEmpty());
}

void UInGameScoreBoardStatLineWidget::SetLine(FString NickName, FRoundStat RoundStat)
{
	TextBlock_Name->SetText(FText::FromString(NickName));
	TextBlock_AverageDriverDistance->SetText(FText::FromString(FString::Printf(TEXT("%.1fm"), RoundStat.AverageDistanceOfDriver * CM_TO_M)));
	TextBlock_Fairway_Settlement->SetText(FText::FromString(FString::Printf(TEXT("%.0f%%"), RoundStat.FairwayArccuracy * M_TO_CM)));
	TextBlock_Green_Accuracy->SetText(FText::FromString(FString::Printf(TEXT("%.0f%%"), RoundStat.GreenArccuracy * M_TO_CM)));
	TextBlock_Green_PuttCount->SetText(FText::FromString(FString::Printf(TEXT("%d"), RoundStat.GreenPuttCount)));
	TextBlock_MaxDistance->SetText(FText::FromString(FString::Printf(TEXT("%.1fm"), RoundStat.MaxDistance * CM_TO_M)));
	TextBlock_Par_Save->SetText(FText::FromString(FString::Printf(TEXT("%.0f%%"), RoundStat.ParSave)));
	TextBlock_PuttCount->SetText(FText::FromString(FString::Printf(TEXT("%d"), RoundStat.PuttCount)));
	TextBlock_ShotCount->SetText(FText::FromString(FString::Printf(TEXT("%d"), RoundStat.ShotCount)));
	TextBlock_Rank->SetText(FText::FromString(FString::Printf(TEXT("%d"), RoundStat.Rank)));
	TextBlock_Sand_Save->SetText(FText::FromString(FString::Printf(TEXT("%.0f%%"), RoundStat.SandSave)));
}

void UInGameScoreBoardStatLineWidget::UpdateStatLine()
{
	AGolfBall* Ball = GM->FindBall(PlayerIndex);
	AGolfPlayer* Player = GM->FindPlayer(PlayerIndex);

	if (GM->GameInfo.bIsRoundEnd)
	{
		if (Ball && Player && !Player->bIsPendingDelete)
		{
			FRoundStat RoundStat = Player->RoundStat;

			Ball->CalculateRoundStat();
			SetLine(Player->PlayerInfo.NickName, RoundStat);
		}
		else
		{
			EmptyAllTextBlock();
		}
	}
	//�̾��ϱ� �� ���
	else
	{
		if (Ball && Player && !Player->bIsPendingDelete)
		{
			FRoundStat RoundStat;

			for (FPlayerInfo PlayerInfo : GM->GameInfo.Players)
			{
				if (PlayerInfo.SlotIndex == SlotIndex)
					RoundStat = PlayerInfo.RoundStat;
			}
			SetLine(Player->PlayerInfo.NickName, RoundStat);
		}
		else
		{
			EmptyAllTextBlock();
		}
	}
}




