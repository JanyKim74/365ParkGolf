#include "ModeSelectWidget.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "../../MenuGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "../../SoundManager.h"
#include "../../TerraParkgameInstance.h"
#include "../LoadingWidget.h"
#include "ParkDay/Utils/UtilLibrary.h"
#include "ParkDay/Widgets/PasswordWidget.h"
#include "ParkDay/Utils/JsonLoader.h"

void UModeSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();

	GM = Cast<AMenuGameMode>(GetWorld()->GetAuthGameMode());

	Button_StrokeMode->OnPressed.AddDynamic(this, &UModeSelectWidget::OnClickStrokeModeButton);
	Button_TraningMode->OnPressed.AddDynamic(this, &UModeSelectWidget::OnClickTraningModeButton);
	Button_PracticeMode->OnPressed.AddDynamic(this, &UModeSelectWidget::OnClickPracticeModeButton);
	Button_Exit->OnPressed.AddDynamic(this, &UModeSelectWidget::OnClickExitButton);

}

void UModeSelectWidget::HandleOnConfirmPasswordForStroke()
{
	OpenStrokeMode();
}

void UModeSelectWidget::HandleOnConfirmPasswordForTraining()
{
	OpenTrainingMode();
}

void UModeSelectWidget::HandleOnConfirmPasswordForPractice()
{
	OpenPracticeLevel();
}

void UModeSelectWidget::ResetGameInfoAndAddPlayer()
{
	//플레이어 한 명으로 초기화
	GM->ResetGameData();
	FGameInfo CachedGameInfo = GM->GetGameInfo();
	FPlayerInfo NewPlayerInfo;
	CachedGameInfo.Players.Add(NewPlayerInfo);
	GM->SetGameInfo(CachedGameInfo);
}

void UModeSelectWidget::OnClickStrokeModeButton()
{
	UUtilLibrary::LockButtonForSeconds(Button_StrokeMode, GetWorld(), 0.5f);
	UUtilLibrary::LockButtonForSeconds(Button_PracticeMode, GetWorld(), 0.5f);
	UUtilLibrary::LockButtonForSeconds(Button_TraningMode, GetWorld(), 0.5f);

	//StopIntro();
	FAdminConfig AdminConfig;
	UJsonLoader::LoadAdminConfigFromJson(TEXT("adminConfig.json"), AdminConfig);

	if (AdminConfig.StrokePW)
	{
		GM->PasswordWidget->OnConfirmPasswordDele.RemoveAll(this);
		GM->PasswordWidget->OnConfirmPasswordDele.AddDynamic(this, &UModeSelectWidget::HandleOnConfirmPasswordForStroke);
		GM->PasswordWidget->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		OpenStrokeMode();
	}
}

void UModeSelectWidget::OnClickTraningModeButton()
{
	UUtilLibrary::LockButtonForSeconds(Button_StrokeMode, GetWorld(), 0.5f);
	UUtilLibrary::LockButtonForSeconds(Button_PracticeMode, GetWorld(), 0.5f);
	UUtilLibrary::LockButtonForSeconds(Button_TraningMode, GetWorld(), 0.5f);

	//StopIntro();
	FAdminConfig AdminConfig;
	UJsonLoader::LoadAdminConfigFromJson(TEXT("adminConfig.json"), AdminConfig);

	if (AdminConfig.TrainingPW)
	{
		GM->PasswordWidget->OnConfirmPasswordDele.RemoveAll(this);
		GM->PasswordWidget->OnConfirmPasswordDele.AddDynamic(this, &UModeSelectWidget::HandleOnConfirmPasswordForTraining);
		GM->PasswordWidget->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		OpenTrainingMode();
	}
}

void UModeSelectWidget::OnClickPracticeModeButton()
{
	UUtilLibrary::LockButtonForSeconds(Button_StrokeMode, GetWorld(), 0.5f);
	UUtilLibrary::LockButtonForSeconds(Button_PracticeMode, GetWorld(), 0.5f);
	UUtilLibrary::LockButtonForSeconds(Button_TraningMode, GetWorld(), 0.5f);

	//StopIntro();
	FAdminConfig AdminConfig;
	UJsonLoader::LoadAdminConfigFromJson(TEXT("adminConfig.json"), AdminConfig);

	if (AdminConfig.RangePW)
	{
		GM->PasswordWidget->OnConfirmPasswordDele.RemoveAll(this);
		GM->PasswordWidget->OnConfirmPasswordDele.AddDynamic(this, &UModeSelectWidget::HandleOnConfirmPasswordForPractice);
		GM->PasswordWidget->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		OpenPracticeLevel();
	}
}

void UModeSelectWidget::OpenStrokeMode()
{
	GM->ResetGameData();
	FGameInfo CachedGameInfo = GM->GetGameInfo();
	GM->SetGameInfo(CachedGameInfo);
	GM->SaveGameInfoToJSON();
	GM->ChangeUIState(EUIState::PlayerSelect);
	GM->SetCurrentGameType(EGameType::StrokeMode);
}

void UModeSelectWidget::OpenTrainingMode()
{
	ResetGameInfoAndAddPlayer();
	GM->ChangeUIState(EUIState::CourseSelect);
	GM->SetCurrentGameType(EGameType::TrainingMode);
}

void UModeSelectWidget::OpenPracticeLevel()
{
	ResetGameInfoAndAddPlayer();
	FGameInfo CachedGameInfo = GM->GetGameInfo();

	int32 GameType = 2;
	CachedGameInfo.bIsRoundEnd = true;
	CachedGameInfo.GameOptions.GameType = GameType;
	CachedGameInfo.SelectedMap.MapName = TEXT("연습장");
	CachedGameInfo.SelectedMap.PakName = "practice";
	CachedGameInfo.SelectedMap.CCName = "practice";
	CachedGameInfo.SelectedMap.Sublevel = 0;

	GM->SetGameInfo(CachedGameInfo);
	GM->SaveGameInfoToJSON();

	GM->SetCurrentGameType(EGameType::RangeMode);

	if (auto* SM = GetGameInstance()->GetSubsystem<USoundManager>())
	{
		SM->PlayTTS_Interrupt_ById(TEXT("Voice.GameStart"));
		SM->StopBGM(3.0f);
	}

	this->SetIsEnabled(false);

	UTerraParkgameInstance* GI = Cast<UTerraParkgameInstance>(GetGameInstance());

	if (GI)
	{
		if (!GI->ActiveLoadingWidget.IsValid())
			GI->ActiveLoadingWidget = CreateWidget<ULoadingWidget>(GetWorld(), GI->LoadingScreenWidgetClass);
		GI->bAutoCompleteWhenLoadingCompletes = true;
		GI->bPlayUntilStopped = false;

		GI->SetupMoviePlayerWithWidget(GI->ActiveLoadingWidget.Get());
		SetIsEnabled(false);

		UUtilLibrary::FadeIn(GetWorld(), 4.0f, FFadeCallback::CreateLambda([this, GI, GameType]()
			{
				GI->ActiveLoadingWidget.Get()->AddToViewport(10000);
				GI->ActiveLoadingWidget.Get()->SetVisibility(ESlateVisibility::Visible);
				const FString Options = FString::Printf(TEXT("?game=/Script/ParkDay.InGameMode?GameMode=%d"), GameType);
				UUtilLibrary::OpenLevelCPP(GetWorld(), TEXT("practice"), Options);
			}));
	}
}

void UModeSelectWidget::OnClickExitButton()
{
	if (auto* SM = GetGameInstance()->GetSubsystem<USoundManager>())
	{
		SM->PlayTTS_Interrupt_ById(TEXT("Voice.EndGame"));	
		SM->StopBGM(1.0f);
	}
	
	//SetIsEnabled(false);
	Button_Exit->SetIsEnabled(false);
	
	FTimerHandle TH;

	GetWorld()->GetTimerManager().SetTimer(TH,
		FTimerDelegate::CreateLambda([this]()
			{
				if (UWorld* World = GetWorld())
				{
					APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
					UKismetSystemLibrary::QuitGame(
						World,
						PC,
						EQuitPreference::Quit,
						/*bIgnorePlatformRestrictions=*/ false
					);
				}
			}), 4.f, false);
}
