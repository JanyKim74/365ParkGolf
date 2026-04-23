#include "PlayerSelectWidget.h"
#include "Kismet/GameplayStatics.h"
#include "../../MenuGameMode.h"
#include "../../Structs/CorseStruct.h"
#include "../../SoundManager.h"
#include "../KeyboardWidget.h"
#include "PlayerSelectProfileWidget.h"

#include "Components/WrapBox.h"
#include "Components/Button.h"
#include "ParkDay/Utils/UtilLibrary.h"
#include "ParkDay/JsonHandler.h"
#include "ParkDay/InGameMode.h"
#include "ParkDay/GolfPlayer.h"
#include "ParkDay/GolfPlayerManager.h"

UPlayerSelectWidget::UPlayerSelectWidget(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	static ConstructorHelpers::FClassFinder<UKeyboardWidget> KeyBoardBPClass(TEXT("/Game/KeyboardPro/BP/WBP/WBP_Keyboard"));
	if (KeyBoardBPClass.Succeeded())
	{
		KeyBoardWidgetClass = KeyBoardBPClass.Class;
		UE_LOG(LogTemp, Log, TEXT("✅ KeyBoardBPClass loaded via ConstructorHelpers"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("⚠️ Failed to load KeyBoardBPClass via ConstructorHelpers. Check path."));
	}
}

void UPlayerSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (KeyBoardWidgetClass)
	{
		KeyBoardWidgetInstance = Cast<UKeyboardWidget>(CreateWidget<UUserWidget>(UGameplayStatics::GetPlayerController(GetWorld(), 0), KeyBoardWidgetClass));
		KeyBoardWidgetInstance->AddToViewport(19000);
		KeyBoardWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("KeyBoardWidget Class is null"));
	}

	GM = Cast<AMenuGameMode>(GetWorld()->GetAuthGameMode());
	InGM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());

	Button_Back->OnPressed.AddDynamic(this, &UPlayerSelectWidget::HandleOnClickButtonBack);
	Button_Next->OnPressed.AddDynamic(this, &UPlayerSelectWidget::HandleOnClickButtonNext);
	if (GM)
	{
		GM->OnEnterPlayerSelectDele.AddDynamic(this, &UPlayerSelectWidget::HandleOnEnterPlayerSelect);
	}

	Init();
}

void UPlayerSelectWidget::HandleOnEnterPlayerSelect()
{
	for (UWidget* Widget : WrapBox_PlayerProfiles->GetAllChildren())
	{
		UPlayerSelectProfileWidget* Profile = Cast<UPlayerSelectProfileWidget>(Widget);
		Profile->Init();
	}
	CheckCanNext();
}

void UPlayerSelectWidget::Init()
{
	for (UWidget* Widget : WrapBox_PlayerProfiles->GetAllChildren())
	{
		UPlayerSelectProfileWidget* Profile = Cast<UPlayerSelectProfileWidget>(Widget);
		Profile->PlayerSelectWidget = this;
		Profile->Init();
	}
}

void UPlayerSelectWidget::SortPlayerInfo(FGameInfo& InGameInfo)
{
	UUtilLibrary::SortPlayersBySlot(GetWorld(), InGameInfo.Players);
	UJsonHandler::SaveGameInfoToJson(InGameInfo, FPaths::ProjectSavedDir() + TEXT("GameData.json"));
}

UPlayerSelectProfileWidget* UPlayerSelectWidget::FindProfile(int32 SlotIndex)
{
	for (UWidget* Widget : WrapBox_PlayerProfiles->GetAllChildren())
	{
		UPlayerSelectProfileWidget* Profile = Cast<UPlayerSelectProfileWidget>(Widget);

		if (Profile->ProfileNumber == SlotIndex)
			return Profile;
	}

	return nullptr;
}

//인 게임에서만 사용됨
void UPlayerSelectWidget::UpdateButtonStatus()
{
	if (InGM)
	{
		TArray<AGolfPlayer*> Players = InGM->PlayerManager->GetPlayers();
		TArray<UPlayerSelectProfileWidget*> Profiles;

		for (UWidget* Widget : WrapBox_PlayerProfiles->GetAllChildren())
		{
			UPlayerSelectProfileWidget* Profile = Cast<UPlayerSelectProfileWidget>(Widget);
			Profile->Button_Delete->SetIsEnabled(true);
			Profiles.Add(Profile);
		}

		//혼자 남았을 때 삭제 비활성
		if (Players.Num() == 1)
		{
			UPlayerSelectProfileWidget* FoundedProfile = FindProfile(Players[0]->SlotIndex);
			FoundedProfile->Button_Delete->SetIsEnabled(false);
		}
		else  //혼자 이상일때
		{
			//for (UWidget* Widget : WrapBox_PlayerProfiles->GetAllChildren())
			//{
			//	UPlayerSelectProfileWidget* Profile = Cast<UPlayerSelectProfileWidget>(Widget);
			//	Profile->Button_Delete->SetIsEnabled(true);
			//}

			//현재 플레이어 삭제 비활성
			//AGolfPlayer* CurrentTurnPlayer = InGM->GetCurrentTurnGolfPlayer();
			//UPlayerSelectProfileWidget* CurrentTurnProfile = FindProfile(CurrentTurnPlayer->SlotIndex);
			//CurrentTurnProfile->Button_Delete->SetIsEnabled(false);


			int32 HoleoutCount = 0;
			for (AGolfPlayer* Player : InGM->PlayerManager->GetPlayers())
			{
				if (Player->IsHoleIn())
					HoleoutCount++;
			}

			//전체 플레이어 수 - 1 만큼 홀 아웃일 때, 홀아웃 유저 뺀 한명 삭제 버튼 비활성화
			if (InGM->PlayerManager->GetPlayers().Num() - 1 == HoleoutCount)
			{
				for (AGolfPlayer* Player : InGM->PlayerManager->GetPlayers())
				{
					if (!Player->IsHoleIn())
					{
						FindProfile(Player->SlotIndex)->Button_Delete->SetIsEnabled(false);
					}
				}
			}
		}
	}
}


void UPlayerSelectWidget::HandleOnClickButtonBack()
{
	UUtilLibrary::LockButtonForSeconds(Button_Back, GetWorld(), 0.5f);
	UUtilLibrary::LockButtonForSeconds(Button_Next, GetWorld(), 0.5f);

	if (GM)
	{
		GM->ChangeUIState(EUIState::ModeSelect);
	}
}

void UPlayerSelectWidget::HandleOnClickButtonNext()
{
	UUtilLibrary::LockButtonForSeconds(Button_Back, GetWorld(), 0.5f);
	UUtilLibrary::LockButtonForSeconds(Button_Next, GetWorld(), 0.5f);

	if (GM)
	{
		GM->ChangeUIState(EUIState::CourseSelect);
	}
}

void UPlayerSelectWidget::CheckCanNext()
{
	if (GM)
	{
		GM->LoadGameInfoFromJSON();
		GameInfo = GM->GetGameInfo();
		if (GameInfo.Players.Num() > 0)
		{
			Button_Next->SetIsEnabled(true);
		}
		else
		{
			Button_Next->SetIsEnabled(false);
		}
	}
	
}
