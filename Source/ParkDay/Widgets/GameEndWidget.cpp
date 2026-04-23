#include "GameEndWidget.h"

#include "ParkDay/GolfPlayer.h"
#include "ParkDay/GolfPlayerController.h"
#include "ParkDay/GolfPlayerManager.h"
#include "ParkDay/InGameMode.h"
#include "ParkDay/SoundManager.h"
#include "ParkDay/Utils/UtilLibrary.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Algo/MinElement.h"
#include "Kismet/GameplayStatics.h"

void UGameEndWidget::NativeConstruct()
{
    Super::NativeConstruct();

    GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());

	if (Button_Exit)
	{
		Button_Exit->OnClicked.AddDynamic(this, &UGameEndWidget::OnClickedExitButton);
	}

	if (Button_ScoreCard)
	{
		Button_ScoreCard->OnClicked.AddDynamic(this, &UGameEndWidget::OnClickedScoreCardButton);
	}
}

void UGameEndWidget::Init()
{
	SetWinnerText();

	if (auto* SM = GetGameInstance()->GetSubsystem<USoundManager>())
	{
		SM->Play2D_ById("BGM.GameEnd");
	}
}

void UGameEndWidget::OnClickedScoreCardButton()
{
	GM->InGameScoreBoardWidgetInstance->SetVisibility(ESlateVisibility::Visible);
}

void UGameEndWidget::OnClickedExitButton()
{
	FString FromInGameStr = "true";
	const FString Options = FString::Printf(TEXT("?game=/Game/UMG/GM_UMG.GM_UMG_C?bFromInGame=%s"), *FromInGameStr);
	UGameplayStatics::OpenLevel(GetWorld(),TEXT("Level_UI"), false, Options);
}

void UGameEndWidget::SetWinnerText()
{
	if (GM)
	{
		Algo::SortBy(GM->GameInfo.Players, &FPlayerInfo::TotalScore, TLess<int32>()); // 오름차순
	}
	WinnerPlayerInfo = GM->GameInfo.Players[0];
	WinnerShotCount = WinnerPlayerInfo.TotalScore;

	TextBlock_Name->SetText(FText::FromString(WinnerPlayerInfo.NickName));
	TextBlock_ShotCount->SetText(FText::FromString(FString::FormatAsNumber(WinnerShotCount)));
}
