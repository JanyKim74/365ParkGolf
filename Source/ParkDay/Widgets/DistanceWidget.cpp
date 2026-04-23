#include "DistanceWidget.h"

#include "ParkDay/GolfPlayer.h"
#include "ParkDay/GolfPlayerController.h"
#include "ParkDay/InGameMode.h"

void UDistanceWidget::NativeConstruct()
{
    Super::NativeConstruct();

    GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
}

void UDistanceWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

    if (GM)
    {
        AGolfPlayer* Player = GM->GetCurrentTurnGolfPlayer();
        AGolfBall* Ball = GM->GetCurrentTurnGolfBall();
        if (Player && Ball)
        {
            if (Player->GetPlayerState() == EPlayerState::Player_Shot
                || Ball->GetBallState() == EBallState::Ball_Stop)
            {
                UpdateShotDistance();
            }
        }

    }
}

void UDistanceWidget::UpdateShotDistance()
{
    FVector BallLocation = GM->GetCurrentTurnGolfBall()->GetActorLocation();
    float ShotDistance = FVector::Dist(GM->GetCurrentTurnGolfPlayer()->BEFOREPos, BallLocation) * 0.01f;
    UpdateShotDistanceText(ShotDistance);
}

void UDistanceWidget::UpdateSensorTextData()
{
    AGolfPlayerController* PlayerController = Cast<AGolfPlayerController>(UGameplayStatics::GetPlayerController(this, 0));

    float BallSpeed = GM->GetCurrentTurnGolfPlayer()->GetSecsorShotPower();
    float ShotPitchAngle = PlayerController->ShotPitchAngle;
    float ShotYawAngle = PlayerController->ShotYawAngle;

    UpdateSensorDataText(BallSpeed, ShotYawAngle,ShotPitchAngle);
}

void UDistanceWidget::InitShotDistanceText()
{
    if (TextBlock_Shot_Distance)
    {
        TextBlock_Shot_Distance->SetText(FText::FromString(TEXT("0.0")));
    }
}



void UDistanceWidget::UpdateShotDistanceText(float ShotDistance)
{
    if (TextBlock_Shot_Distance)
    {
        TextBlock_Shot_Distance->SetText(
            FText::FromString(FString::Printf(TEXT("%.1f"),
                ShotDistance)
            )
        );
    }
}

void UDistanceWidget::UpdateSensorDataText(float BallSpeed, float LeftRight, float UpDown)
{
    TextBlock_BallSpeed->SetText(
        FText::FromString(FString::Printf(TEXT("%.1f"),
            BallSpeed)
        )
    );
    TextBlock_LeftRight_Angle->SetText(
        FText::FromString(FString::Printf(TEXT("%.1f"),
            LeftRight)
        )
    );
    TextBlock_UpDown_Angle->SetText(
        FText::FromString(FString::Printf(TEXT("%.1f"),
            UpDown)
        )
    );
}
