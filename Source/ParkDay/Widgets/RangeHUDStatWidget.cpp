#include "RangeHUDStatWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/CheckBox.h"
#include "Components/GridPanel.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/DrawElements.h"

#include "../InGameMode.h"
#include "../GolfPlayerManager.h"
#include "../GolfPlayer.h"
#include "../GolfBall.h"
#include "ParkDay/GolfPlayerController.h"
#include "ParkDay/CameraManager.h"
#include "ParkDay/Utils/UtilLibrary.h"
#include "Widgets/SWidget.h"
#include "HAL/PlatformTime.h"
#include "ParkDay/Widgets/RangeHUDStatLineWidget.h"
#include "Components/GridPanel.h"
#include "Blueprint/WidgetTree.h"   // 중요
#include "Widgets/SWidget.h"

void URangeHUDStatWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!Button_close)
	{
		Button_close = Cast<UButton>(WidgetTree ? WidgetTree->FindWidget(TEXT("Button_close")) : nullptr);
		UE_LOG(LogTemp, Warning, TEXT("[Fixup] Button_close reassigned via WidgetTree: %s"), *GetNameSafe(Button_close));
	}
	UE_LOG(LogTemp, Log, TEXT("✅ Button_close validated"));

	if (!GridPanel_StatLines)
	{
		GridPanel_StatLines = Cast<UGridPanel>(WidgetTree ? WidgetTree->FindWidget(TEXT("GridPanel_StatLines")) : nullptr);
		UE_LOG(LogTemp, Warning, TEXT("[Fixup] GridPanel_StatLines reassigned via WidgetTree: %s"), *GetNameSafe(GridPanel_StatLines));
	}
	UE_LOG(LogTemp, Log, TEXT("✅ GridPanel_StatLines validated"));

	if (!AverageLine)
	{
		AverageLine = Cast<URangeHUDStatLineWidget>(WidgetTree ? WidgetTree->FindWidget(TEXT("AverageLine")) : nullptr);
		UE_LOG(LogTemp, Warning, TEXT("[Fixup] AverageLine reassigned via WidgetTree: %s"), *GetNameSafe(AverageLine));
	}
	UE_LOG(LogTemp, Log, TEXT("✅ AverageLine validated"));

	if (Button_close)
		Button_close->OnPressed.AddDynamic(this, &URangeHUDStatWidget::HandleOnPressedCloseButton);
	int32 Index = 0;

	for (UWidget* Widget : GridPanel_StatLines->GetAllChildren())
	{
		Index++;
		URangeHUDStatLineWidget* LineWidget = Cast<URangeHUDStatLineWidget>(Widget);
		if (LineWidget)
		{
			LineWidget->ShotIndex = Index;
		}
	}

	AverageLine->SetEmptyAllText();
	AverageLine->TextBlock_BallSpeed->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f));
	AverageLine->TextBlock_DirectionAngle->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f));
	AverageLine->TextBlock_Distance->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f));
	AverageLine->TextBlock_LaunchAngle->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f));
	AverageLine->TextBlock_RemainDistance->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f));
}

void URangeHUDStatWidget::HandleOnPressedCloseButton()
{
	SetVisibility(ESlateVisibility::Collapsed);
}