#include "ContinuePopupWidget.h"
#include "Kismet/GameplayStatics.h" // 예시용, 필요시 포함
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "../InGameMode.h"
#include "../GolfPlayerController.h"
#include "../GolfPlayerManager.h"
#include "../StrokeMenuWidget.h"
#include "ParkDay/SoundManager.h"
#include "ParkDay/MenuGameMode.h"
#include "ParkDay/Utils/UtilLibrary.h"
#include "ParkDay/TerraParkGameInstance.h"
#include "ParkDay/Widgets/LoadingWidget.h"
#include "Kismet/GameplayStatics.h"

void UContinuePopupWidget::NativeConstruct()
{
	Super::NativeConstruct();
	GM = Cast<AMenuGameMode>(GetWorld()->GetAuthGameMode());

	Button_Confirm->OnClicked.AddDynamic(this, &UContinuePopupWidget::HandleOnClickButtonConfirm);
	Button_Cancel->OnClicked.AddDynamic(this, &UContinuePopupWidget::HandleOnClickButtonCancel);
}

void UContinuePopupWidget::HandleOnClickButtonConfirm()
{
	if (GM)
	{
		if (UTerraParkgameInstance* GI = Cast<UTerraParkgameInstance>(GetGameInstance()))
		{
			if (auto* SM = GetWorld()->GetGameInstance()->GetSubsystem<USoundManager>())
			{
				SM->PlayTTS_Interrupt_ById(TEXT("Voice.GameStart"), 0.5f);
				if (!SM->BGMIsPlaying())
				{
					SM->PlayBGM_ById(TEXT("BGM.ModeSelect"));
				}
			}

			if (!GI->ActiveLoadingWidget.IsValid())
			{
				GI->ActiveLoadingWidget = CreateWidget<ULoadingWidget>(GetWorld(), GI->LoadingScreenWidgetClass);
			}

			GM->LoadGameInfoFromJSON();
			FGameInfo CachedGameInfo = GM->GetGameInfo();
			UUtilLibrary::SortPlayersBySlot(GetWorld(), CachedGameInfo.Players);
			GM->SetGameInfo(CachedGameInfo);
			GM->SaveGameInfoToJSON();
			SetIsEnabled(false);

			FTimerHandle TH;
			GetWorld()->GetTimerManager().SetTimer(TH,
				FTimerDelegate::CreateLambda([this, GI, CachedGameInfo]()
					{
						GI->ActiveLoadingWidget.Get()->AddToViewport(10000);
						GI->ActiveLoadingWidget.Get()->SetVisibility(ESlateVisibility::Visible);
						int32 GameType = CachedGameInfo.GameOptions.GameType;
						const FString Options = FString::Printf(TEXT("?game=/Script/ParkDay.InGameMode?GameMode=%d"), GameType);
						UUtilLibrary::OpenLevelCPP(GetWorld(), CachedGameInfo.SelectedMap.PakName, Options);
					}), 4.f, false);
		}
	}
}

void UContinuePopupWidget::HandleOnClickButtonCancel()
{
	SetVisibility(ESlateVisibility::Collapsed);
	GM->LoadDefaultGameOption();
	//if (GM)
	//{
	//	GM->LoadGameInfoFromJSON();
	//	FGameInfo CachedGameInfo = GM->GetGameInfo();
	//	CachedGameInfo.Reset();
	//	GM->SetGameInfo(CachedGameInfo);
	//	GM->SaveGameInfoToJSON();
	//}
}
