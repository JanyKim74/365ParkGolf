#include "ParkDay/Widgets/CameraModePopupWidget.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "ParkDay/SoundManager.h"
#include "ParkDay/InGameMode.h"
#include "ParkDay/GolfBall.h"
#include "ParkDay/GolfPlayer.h"
#include "ParkDay/CameraManager.h"
#include "ParkDay/GolfPlayerManager.h"
#include "ParkDay/Utils/UtilLibrary.h"
#include "ParkDay/StrokeMenuWidget.h"

void UCameraModePopupWidget::NativeConstruct()
{
    Super::NativeConstruct();

    GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());

    Button_Confirm->OnPressed.AddDynamic(this, &UCameraModePopupWidget::HandleOnPressedConfirmButton);
    CheckBox_Static->OnCheckStateChanged.AddDynamic(this, &UCameraModePopupWidget::HandleOnPressedStaticCheckBox);
    CheckBox_Move->OnCheckStateChanged.AddDynamic(this, &UCameraModePopupWidget::HandleOnPressedMoveCheckBox);
    
    if (GM->GameInfo.GameOptions.Camera_Mode == 1)
    {
        CheckBox_Static->SetIsChecked(true);
        CheckBox_Move->SetIsChecked(false);
    }
    else
    {
        CheckBox_Static->SetIsChecked(false);
        CheckBox_Move->SetIsChecked(true);
    }
}

void UCameraModePopupWidget::HandleOnPressedMoveCheckBox(bool bIsChecked)
{
    if (!GM || !GM->PlayerManager) return;

    AGolfBall* Ball = GM->GetCurrentTurnGolfBall();

    if (!Ball || !Ball->LinkedCameraManager) return;

    if (!bIsChecked)
    {
        CheckBox_Move->SetIsChecked(true);
        return;
    }

    UUtilLibrary::LockCheckBoxForSeconds(CheckBox_Move, GetWorld(), 0.2f);
    UUtilLibrary::LockButtonForSeconds(Button_Confirm, GetWorld(), 0.2f);
    UUtilLibrary::LockCheckBoxForSeconds(CheckBox_Static, GetWorld(), 0.2f);

    CheckBox_Static->SetIsChecked(false);

    const FVector  BeforeLocation = Ball->LinkedCameraManager->GetActorLocation();
    const FRotator BeforeRotator = Ball->LinkedCameraManager->GetActorRotation();

    Ball->LinkedCameraManager->SetFixedCameraPosition(BeforeLocation, BeforeRotator);
    Ball->LinkedCameraManager->bUsePartialFixedMode = false;
    Ball->LinkedCameraManager->SetUseFixedModeInReady(false);
    Ball->LinkedCameraManager->ChangeCameraMode(ECameraMode::Ready);

    GM->GameInfo.GameOptions.Camera_Mode = 0;
}

void UCameraModePopupWidget::HandleOnPressedStaticCheckBox(bool bIsChecked)
{
    if (!GM || !GM->PlayerManager) return;

    AGolfBall* Ball = GM->GetCurrentTurnGolfBall();

    if (!Ball || !Ball->LinkedCameraManager) return;

    if (!bIsChecked)
    {
        CheckBox_Static->SetIsChecked(true);
        return;
    }

    UUtilLibrary::LockCheckBoxForSeconds(CheckBox_Move, GetWorld(), 0.2f);
    UUtilLibrary::LockButtonForSeconds(Button_Confirm, GetWorld(), 0.2f);
    UUtilLibrary::LockCheckBoxForSeconds(CheckBox_Static, GetWorld(), 0.2f);

    CheckBox_Move->SetIsChecked(false);

    const FVector  BeforeLocation = Ball->LinkedCameraManager->GetActorLocation();
    const FRotator BeforeRotator = Ball->LinkedCameraManager->GetActorRotation();

    Ball->LinkedCameraManager->SetFixedCameraPosition(BeforeLocation, BeforeRotator);
    Ball->LinkedCameraManager->bUsePartialFixedMode = true;
    Ball->LinkedCameraManager->SetUseFixedModeInReady(false);
    Ball->LinkedCameraManager->ChangeCameraMode(ECameraMode::Fixed);
    GM->GameInfo.GameOptions.Camera_Mode = 1;
}

void UCameraModePopupWidget::HandleOnPressedConfirmButton()
{
    FTimerHandle TH;
    GetWorld()->GetTimerManager().SetTimer(
        TH,
        [this]() 
        {
            GM->StrokeMenuWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
            SetVisibility(ESlateVisibility::Collapsed);
        },
        0.2f,
		false
	);
}
