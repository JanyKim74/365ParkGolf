#include "CourseSelectWidget.h"

#include "Components/Button.h"
#include "CourseSelectMapWidget.h"
#include "CourseSelectMapPanelWidget.h"
#include "Kismet/GameplayStatics.h"
#include "../../MenuGameMode.h"
#include "../../SoundManager.h"
#include "../../TerraParkgameInstance.h"
#include "../LoadingWidget.h"
#include "ParkDay/Utils/UtilLibrary.h"
#include "ParkDay/Widgets/Menu/CourseSelectMapWidget.h"

void UCourseSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();

	Button_Back->OnPressed.AddDynamic(this, &UCourseSelectWidget::HandleOnClickBackButton);
	Button_GameStart->OnPressed.AddDynamic(this, &UCourseSelectWidget::HandleOnClickGameStartButton);

	Init();
}

void UCourseSelectWidget::Init()
{
	GM = Cast<AMenuGameMode>(GetWorld()->GetAuthGameMode());
}

void UCourseSelectWidget::HandleOnClickBackButton()
{
	UUtilLibrary::LockButtonForSeconds(Button_Back, GetWorld(), 0.5f);
	EGameType CurrentGameMode = GM->GetCurrentGameType();

	if (CurrentGameMode == EGameType::StrokeMode)
	{
		GM->ChangeUIState(EUIState::PlayerSelect);
	}
	else if (CurrentGameMode == EGameType::TrainingMode)
	{
		GM->ChangeUIState(EUIState::ModeSelect);
	}
}

void UCourseSelectWidget::UpdateSelectedMapInfo()
{
	GM->LoadGameInfoFromJSON();
	FGameInfo CachedGameInfo = GM->GetGameInfo();
	UCourseSelectMapWidget* MapWidget = WBP_CorseMap_Panel->GetSelectedMapWidget();


	CachedGameInfo.SelectedMap.CCName = MapWidget->CCFolderName;
	CachedGameInfo.SelectedMap.PakName = MapWidget->FieldMapInfo.PakFile;
	CachedGameInfo.SelectedMap.MapName = MapWidget->FieldMapInfo.CCname;

	CachedGameInfo.SelectedMap.Sublevel = MapWidget->FieldMapInfo.Sublevel;

	for (int32 i = 0; i < MapWidget->FieldMapInfo.HoleInfos.Num(); i++)
	{
		CachedGameInfo.SelectedMap.ParScores[i] = MapWidget->FieldMapInfo.HoleInfos[i].ParCount;
	}

	GM->SetGameInfo(CachedGameInfo);
	GM->SaveGameInfoToJSON();
}

void UCourseSelectWidget::HandleOnClickGameStartButton()
{
	GM->SaveGameInfoToJSON();

	FGameInfo CachedGameInfo = GM->GetGameInfo();

	int32 GameType = CachedGameInfo.GameOptions.GameType;
	EGameType CurrentGameMode = GM->GetCurrentGameType();

	UCourseSelectMapWidget* MapWidget = WBP_CorseMap_Panel->GetSelectedMapWidget();
	const FString LevelName = MapWidget->FieldMapInfo.PakFile;

	UpdateSelectedMapInfo();

	SetIsEnabled(false);

	if (auto* SM = GetWorld()->GetGameInstance()->GetSubsystem<USoundManager>())
	{
		SM->PlayTTS_Interrupt_ById(TEXT("Voice.GameStart"), 0.5f);
	}

	UTerraParkgameInstance* GI = Cast<UTerraParkgameInstance>(GetGameInstance());
	if (GI)
	{
		if (!GI->ActiveLoadingWidget.IsValid())
			GI->ActiveLoadingWidget = CreateWidget<ULoadingWidget>(GetWorld(), GI->LoadingScreenWidgetClass);
		GI->bAutoCompleteWhenLoadingCompletes = true;
		GI->bPlayUntilStopped = false;
		GI->SetupMoviePlayerWithWidget(GI->ActiveLoadingWidget.Get());
		SetIsEnabled(false);

			UUtilLibrary::FadeIn(
			GetWorld(),
			4.f,
			FFadeCallback::CreateLambda([GI, this, GameType, LevelName]()
				{
					GI->ActiveLoadingWidget.Get()->AddToViewport(10000);
					GI->ActiveLoadingWidget.Get()->SetVisibility(ESlateVisibility::Visible);
					const FString Options = FString::Printf(TEXT("?game=/Script/ParkDay.InGameMode?GameMode=%d"), GameType);
					UUtilLibrary::OpenLevelCPP(GetWorld(), LevelName, Options);				
				})
		);

		//FTimerHandle TH;
		//GetWorld()->GetTimerManager().SetTimer(TH,
		//	FTimerDelegate::CreateLambda([this, GameType, LevelName, GI]()
		//		{

		//		}), 4.f, false);
	}
}
