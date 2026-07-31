#include "RangeHUDStatLineWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/CheckBox.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/DrawElements.h"

#include "../InGameMode.h"
#include "../GolfPlayerManager.h"
#include "../GolfPlayer.h"
#include "ParkDay/GolfPlayerController.h"
#include "ParkDay/CameraManager.h"
#include "ParkDay/Utils/UtilLibrary.h"
#include "Blueprint/WidgetTree.h"   // 중요
#include "Widgets/SWidget.h"
#include "HAL/PlatformTime.h"


void URangeHUDStatLineWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!TextBlock_BallSpeed)
	{
		TextBlock_BallSpeed = Cast<UTextBlock>(WidgetTree ? WidgetTree->FindWidget(TEXT("TextBlock_BallSpeed")) : nullptr);
		UE_LOG(LogTemp, Warning, TEXT("[Fixup] TextBlock_BallSpeed reassigned via WidgetTree: %s"), *GetNameSafe(TextBlock_BallSpeed));
	}
	//UE_LOG(LogTemp, Log, TEXT("✅ TextBlock_BallSpeed validated"));

	if (!TextBlock_DirectionAngle)
	{
		TextBlock_DirectionAngle = Cast<UTextBlock>(WidgetTree ? WidgetTree->FindWidget(TEXT("TextBlock_DirectionAngle")) : nullptr);
		UE_LOG(LogTemp, Warning, TEXT("[Fixup] TextBlock_DirectionAngle reassigned via WidgetTree: %s"), *GetNameSafe(TextBlock_DirectionAngle));
	}
	//UE_LOG(LogTemp, Log, TEXT("✅ TextBlock_DirectionAngle validated"));

	if (!TextBlock_Distance)
	{
		TextBlock_Distance = Cast<UTextBlock>(WidgetTree ? WidgetTree->FindWidget(TEXT("TextBlock_Distance")) : nullptr);
		UE_LOG(LogTemp, Warning, TEXT("[Fixup] TextBlock_Distance reassigned via WidgetTree: %s"), *GetNameSafe(TextBlock_Distance));
	}
	//UE_LOG(LogTemp, Log, TEXT("✅ TextBlock_Distance validated"));

	if (!TextBlock_LaunchAngle)
	{
		TextBlock_LaunchAngle = Cast<UTextBlock>(WidgetTree ? WidgetTree->FindWidget(TEXT("TextBlock_LaunchAngle")) : nullptr);
		UE_LOG(LogTemp, Warning, TEXT("[Fixup] TextBlock_LaunchAngle reassigned via WidgetTree: %s"), *GetNameSafe(TextBlock_LaunchAngle));
	}
	//UE_LOG(LogTemp, Log, TEXT("✅ TextBlock_LaunchAngle validated"));

	if (!TextBlock_RemainDistance)
	{
		TextBlock_RemainDistance = Cast<UTextBlock>(WidgetTree ? WidgetTree->FindWidget(TEXT("TextBlock_RemainDistance")) : nullptr);
		UE_LOG(LogTemp, Warning, TEXT("[Fixup] TextBlock_RemainDistance reassigned via WidgetTree: %s"), *GetNameSafe(TextBlock_RemainDistance));
	}
	//UE_LOG(LogTemp, Log, TEXT("✅ TextBlock_BallSpeed validated"));

	if (!TextBlock_ShotCount)
	{
		TextBlock_ShotCount = Cast<UTextBlock>(WidgetTree ? WidgetTree->FindWidget(TEXT("TextBlock_ShotCount")) : nullptr);
		UE_LOG(LogTemp, Warning, TEXT("[Fixup] TextBlock_ShotCount reassigned via WidgetTree: %s"), *GetNameSafe(TextBlock_ShotCount));
	}
	//UE_LOG(LogTemp, Log, TEXT("✅ TextBlock_ShotCount validated"));

	GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());

	if (GM)
	{
		GM->RangeHUDWidgetInstance->OnAddShotStatDele.AddDynamic(this, &URangeHUDStatLineWidget::HandleOnAddShotStat);
		GM->RangeHUDWidgetInstance->OnBallStopDele.AddDynamic(this, &URangeHUDStatLineWidget::HandleOnBallStop);
		GM->RangeHUDWidgetInstance->OnModeChangeDele.AddDynamic(this, &URangeHUDStatLineWidget::HandleOnModeChange);
	}

	SetEmptyAllText();
}

void URangeHUDStatLineWidget::HandleOnModeChange()
{
	SetEmptyAllText();
}

void URangeHUDStatLineWidget::HandleOnBallStop(FShotStat InShotStat)
{
	ShotStat = InShotStat;

	if (ShotIndex == InShotStat.ShotCount)
	{

		FText ShotCount = FText::AsNumber(ShotStat.ShotCount);
		FText Distance = FText::FromString(FString::Printf(TEXT("%.1fm"), ShotStat.Distance));
		FText DirectionAngle = FText::FromString(FString::Printf(TEXT("%.1f"), ShotStat.DirectionAngle));
		FText LaunchAngle = FText::FromString(FString::Printf(TEXT("%.1f"), ShotStat.LaunchAngle));
		FText BallSpeed = FText::FromString(FString::Printf(TEXT("%.1fm/s"), ShotStat.BallSpeed));
		FText RemainDistance = FText::FromString(FString::Printf(TEXT("%.1fm"), ShotStat.RemainDistance));
		SetRedText();
		TextBlock_ShotCount->SetText(ShotCount);
		TextBlock_BallSpeed->SetText(BallSpeed);
		TextBlock_DirectionAngle->SetText(DirectionAngle);
		TextBlock_LaunchAngle->SetText(LaunchAngle);
		TextBlock_Distance->SetText(Distance);
	}
	else if (bIsAverageLine == false)
	{
		SetBlackText();
	}
	else if (bIsAverageLine)
	{
		TextBlock_ShotCount->SetColorAndOpacity(FLinearColor::White);
		TextBlock_BallSpeed->SetColorAndOpacity(FLinearColor::White);
		TextBlock_DirectionAngle->SetColorAndOpacity(FLinearColor::White);
		TextBlock_LaunchAngle->SetColorAndOpacity(FLinearColor::White);
		TextBlock_Distance->SetColorAndOpacity(FLinearColor::White);
		TextBlock_RemainDistance->SetColorAndOpacity(FLinearColor::White);
	}
}

void URangeHUDStatLineWidget::SetRedText()
{
	FLinearColor TextColor = FLinearColor(0.8f, 0.f, 0.f);
	TextBlock_ShotCount->SetColorAndOpacity(TextColor);
	TextBlock_BallSpeed->SetColorAndOpacity(TextColor);
	TextBlock_DirectionAngle->SetColorAndOpacity(TextColor);
	TextBlock_LaunchAngle->SetColorAndOpacity(TextColor);
	TextBlock_Distance->SetColorAndOpacity(TextColor);
	TextBlock_RemainDistance->SetColorAndOpacity(TextColor);
}

void URangeHUDStatLineWidget::SetBlackText()
{
	TextBlock_ShotCount->SetColorAndOpacity(FLinearColor::White);
	TextBlock_BallSpeed->SetColorAndOpacity(FLinearColor::White);
	TextBlock_DirectionAngle->SetColorAndOpacity(FLinearColor::White);
	TextBlock_LaunchAngle->SetColorAndOpacity(FLinearColor::White);
	TextBlock_Distance->SetColorAndOpacity(FLinearColor::White);
	TextBlock_RemainDistance->SetColorAndOpacity(FLinearColor::White);
}

void URangeHUDStatLineWidget::SetEmptyAllText()
{
	if (TextBlock_RemainDistance)
		TextBlock_RemainDistance->SetText(FText::GetEmpty());
	if (TextBlock_LaunchAngle)
		TextBlock_LaunchAngle->SetText(FText::GetEmpty());
	if (TextBlock_Distance)
		TextBlock_Distance->SetText(FText::GetEmpty());
	if (TextBlock_ShotCount)
		TextBlock_ShotCount->SetText(FText::GetEmpty());
	if (TextBlock_DirectionAngle)
		TextBlock_DirectionAngle->SetText(FText::GetEmpty());
	if (TextBlock_BallSpeed)
		TextBlock_BallSpeed->SetText(FText::GetEmpty());

	ShotStat = FShotStat();
}

void URangeHUDStatLineWidget::HandleOnAddShotStat(FShotStat InShotStat)
{

}