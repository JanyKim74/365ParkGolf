#include "InGamePlayerSelectWidget.h"
#include "Menu/PlayerSelectWidget.h"
#include "Menu/PlayerSelectProfileWidget.h"
#include "ParkDay/StrokeMenuWidget.h"
#include "Components/WrapBox.h"
#include "../InGameMode.h"

UInGamePlayerSelectWidget::UInGamePlayerSelectWidget(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	static ConstructorHelpers::FClassFinder<UPlayerSelectWidget> PlayerSelectBPClass(TEXT("/Game/UMG/UI/PlayerSelect/WBP_PlayerSelect.WBP_PlayerSelect_C"));
	if (PlayerSelectBPClass.Succeeded())
	{
		PlayerSelectClass = PlayerSelectBPClass.Class;
		UE_LOG(LogTemp, Log, TEXT("✅ PlayerSelectClass loaded via ConstructorHelpers"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("⚠️ Failed to load PlayerSelectClass via ConstructorHelpers. Check path."));
	}
}

void UInGamePlayerSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (PlayerSelectClass)
	{
		PlayerSelect = Cast<UPlayerSelectWidget>(CreateWidget<UUserWidget>(UGameplayStatics::GetPlayerController(GetWorld(), 0), PlayerSelectClass));
		//if (auto* GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
		//{
		//	GM->LoadGameInfoFromJSON();
		//	PlayerSelect->GameInfo = GM->GameInfo;
		//}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UPlayerSelectWidget Class is null"));
	}

	CanvasPanel_PlayerList->AddChildToCanvas(PlayerSelect);
	PlayerSelect->Button_Back->SetVisibility(ESlateVisibility::Collapsed);
	PlayerSelect->Button_Next->SetVisibility(ESlateVisibility::Collapsed);
	PlayerSelect->Image_Background->SetVisibility(ESlateVisibility::Collapsed);

	Button_Next->OnClicked.AddDynamic(this, &UInGamePlayerSelectWidget::HandleOnClickNextButton);
	Button_Back->OnClicked.AddDynamic(this, &UInGamePlayerSelectWidget::HandleOnClickBackButton);
	Init();
}

void UInGamePlayerSelectWidget::Init()
{
	if (PlayerSelect)
	{
		for (UWidget* Widget : PlayerSelect->WrapBox_PlayerProfiles->GetAllChildren())
		{
			UPlayerSelectProfileWidget* Profile = Cast<UPlayerSelectProfileWidget>(Widget);
			Profile->SetProfileInfo();
		}
	}
}

void UInGamePlayerSelectWidget::HandleOnClickNextButton()
{
	SetVisibility(ESlateVisibility::Collapsed);

	if (auto* GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GM->StrokeMenuWidgetInstance->SetVisibility(ESlateVisibility::Visible);
	}
}

void UInGamePlayerSelectWidget::HandleOnClickBackButton()
{
	SetVisibility(ESlateVisibility::Collapsed);

	if (auto* GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GM->StrokeMenuWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
	}
}
