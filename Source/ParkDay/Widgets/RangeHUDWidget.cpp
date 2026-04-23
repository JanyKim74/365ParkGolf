#include "RangeHUDWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/WrapBox.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/DrawElements.h"

#include "../InGameMode.h"
#include "../GolfPlayerManager.h"
#include "../GolfPlayer.h"
#include "ParkDay/GolfPlayerController.h"
#include "ParkDay/CameraManager.h"
#include "ParkDay/Utils/UtilLibrary.h"
#include "ParkDay/Widgets/RangeHUDStatLineWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Widgets/SWidget.h"
#include "HAL/PlatformTime.h"
#include <ParkDay/SoundManager.h>

DEFINE_LOG_CATEGORY_STATIC(LogRangeHUD, Log, All);

// 정적 멤버
double URangeHUDWidget::GLastPaintLogTimeSec = 0.0;

static void DumpWidgetTree(UWidgetTree* Tree)
{
    if (!Tree || !Tree->RootWidget) return;

    TArray<UWidget*> All;
    Tree->GetAllWidgets(All);

    UE_LOG(LogRangeHUD, Warning, TEXT("========== [Diag] WidgetTree Dump (%d widgets) =========="), All.Num());
    for (UWidget* W : All)
    {
        UE_LOG(LogRangeHUD, Warning, TEXT(" - %s (%s)"),
            *GetNameSafe(W),
            W ? *W->GetClass()->GetName() : TEXT("None"));
    }
    UE_LOG(LogRangeHUD, Warning, TEXT("========================================================="));
}

// ---------- Lifecycle ----------
void URangeHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    ValidateAndReBindWidgets();

    GM = Cast<AInGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr);
    if (!GM)
    {
        UE_LOG(LogRangeHUD, Warning, TEXT("⚠️ GameMode not found yet (may be set later)"));
    }
    else
    {
        UE_LOG(LogRangeHUD, Log, TEXT("✅ GameMode reference obtained"));
    }

    Init();
    UE_LOG(LogRangeHUD, Log, TEXT("✅ Init() called"));

    SetTargetMarkerVisible(false);

    InvalidateLayoutAndVolatility();

    if (AverageLine)
        AverageLine->bIsAverageLine = true;

    RefreshApproachTargetWidget();
    UpdateApproachTargetMarker();
    UE_LOG(LogRangeHUD, Warning, TEXT("✅ RangeHUDWidget::NativeConstruct() COMPLETED SUCCESSFULLY"));
}

void URangeHUDWidget::SetTextForTextBlock(UTextBlock& TextBlock, FString Text)
{
    TextBlock.SetText(FText::FromString(Text));
}

void URangeHUDWidget::HandleOnPressedLeftArrowButton()
{
    if (!GM || GM->GetCurrentTurnGolfBall()->CurrentBallState != EBallState::Ball_Ready)
        return;

    UUtilLibrary::LockButtonForSeconds(Button_LeftArrow, GetWorld(), 0.07f);


    FString RangeText;
    if (GM->CurrentPracticeMode == EPracticeMode::Approach)
    {
        if (ApproachModeDistance <= 10.f * M_TO_CM)
        {
            Button_LeftArrow->SetIsEnabled(false);
            return;
        }
        Button_RightArrow->SetIsEnabled(true);


        ApproachModeDistance -= 10.f * M_TO_CM;
        RangeText = FString::Printf(TEXT("%.0fm"), ApproachModeDistance * CM_TO_M);

        GM->MoveBallOnPracticeMode();
        RefreshApproachTargetWidget();
    }
    else if (GM->CurrentPracticeMode == EPracticeMode::Putting)
    {
        if (PuttingModeDistance <= 5.f * M_TO_CM)
        {
            Button_LeftArrow->SetIsEnabled(false);
            return;
        }
        Button_RightArrow->SetIsEnabled(true);

        PuttingModeDistance -= 5.f * M_TO_CM;
        RangeText = FString::Printf(TEXT("%.0fm"), PuttingModeDistance * CM_TO_M);


        GM->MoveBallOnPracticeMode();
        RefreshApproachTargetWidget(); // Putting은 Target 숨김이지만 동기화용
    }


    if (TextBlock_Range)
        SetTextForTextBlock(*TextBlock_Range, RangeText);
    PuttingModeStartPoint = GM->GetCurrentTurnGolfBall()->GetActorLocation();

    RequestRedraw();
}

void URangeHUDWidget::HandleOnPressedRightArrowButton()
{
    if (!GM || GM->GetCurrentTurnGolfBall()->CurrentBallState != EBallState::Ball_Ready)
        return;
    UUtilLibrary::LockButtonForSeconds(Button_RightArrow, GetWorld(), 0.07f);

    FString RangeText;
    if (GM->CurrentPracticeMode == EPracticeMode::Approach)
    {
        if (ApproachModeDistance >= 150.f * M_TO_CM)
        {
            Button_RightArrow->SetIsEnabled(false);
            return;
        }
        Button_LeftArrow->SetIsEnabled(true);

        ApproachModeDistance += 10.f * M_TO_CM;
        RangeText = FString::Printf(TEXT("%.0fm"), ApproachModeDistance * CM_TO_M);

        GM->MoveBallOnPracticeMode();
        RefreshApproachTargetWidget();
    }
    else if (GM->CurrentPracticeMode == EPracticeMode::Putting)
    {
        if (PuttingModeDistance >= 60.f * M_TO_CM)
        {
            Button_RightArrow->SetIsEnabled(false);
            return;
        }
        Button_LeftArrow->SetIsEnabled(true);

        PuttingModeDistance += 5.f * M_TO_CM;
        RangeText = FString::Printf(TEXT("%.0fm"), PuttingModeDistance * CM_TO_M);
        GM->MoveBallOnPracticeMode();
        RefreshApproachTargetWidget();
    }

    if (TextBlock_Range)
        SetTextForTextBlock(*TextBlock_Range, RangeText);
    PuttingModeStartPoint = GM->GetCurrentTurnGolfBall()->GetActorLocation();

    RequestRedraw();
}

void URangeHUDWidget::HandleOnPressedStatButton()
{
    if (GM && GM->RangeHUDStatWidgetInstance)
    {
        UUtilLibrary::LockButtonForSeconds(Button_Stat, GetWorld(), 0.2f);

        GM->RangeHUDStatWidgetInstance->SetVisibility(ESlateVisibility::Visible);
    }
}

void URangeHUDWidget::ChangePracticeModeState(EPracticeMode ChangeMode)
{
    if (GM)
    {
        GM->CurrentPracticeMode = ChangeMode;
        CurrentShotStat = FShotStat();
        ShotCount = 0;
        ShotStats.Empty();
        ShotPaths.Empty();
        OnModeChangeDele.Broadcast();
    }

    RequestRedraw();
}

void URangeHUDWidget::SetVisibilityModes(bool bIsVisible)
{
    if (!CanvasPanel_Modes) return;
    CanvasPanel_Modes->SetVisibility(bIsVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void URangeHUDWidget::HandleOnChangedCheckBoxMode(bool bIsChecked)
{
    if (GM && GM->GetCurrentTurnGolfPlayer()->GetPlayerState() == EPlayerState::Player_Ready)
    {
        SetVisibilityModes(bIsChecked);
        UUtilLibrary::LockCheckBoxForSeconds(CheckBox_Mode, GetWorld(), 0.2f);
    }
}

void URangeHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!GM) return;

    if (GM->GetCurrentTurnGolfPlayer()->GetPlayerState() == EPlayerState::Player_Shot)
    {
        CurrentShotStat.Distance = FVector::Dist(GM->GetCurrentTurnGolfPlayer()->BEFOREPos, GM->GetCurrentTurnGolfBall()->GetActorLocation()) * CM_TO_M;
        if (TextBlock_ShotInfo_Distance)
            TextBlock_ShotInfo_Distance->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), CurrentShotStat.Distance)));

        if (CanvasPanel_Modes) CanvasPanel_Modes->SetVisibility(ESlateVisibility::Collapsed);
        if (CheckBox_Mode)     CheckBox_Mode->SetIsChecked(false);
    }
    else if (GM->GetCurrentTurnGolfPlayer()->GetPlayerState() == EPlayerState::Player_Ready)
    {
        if (TextBlock_ShotInfo_Distance)
            TextBlock_ShotInfo_Distance->SetText(FText::FromString(TEXT("0")));

        if (AGolfPlayerController* GolfPC = Cast<AGolfPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
        {
            switch (GM->CurrentPracticeMode)
            {
            case EPracticeMode::Driving:
                if (GM->PracticeModeEndPoint) GolfPC->GetAimActor()->SetActorLocation(GM->PracticeModeEndPoint->GetActorLocation());
                break;
            case EPracticeMode::Approach:
                if (GM->PracticeModeEndPoint) GolfPC->GetAimActor()->SetActorLocation(GM->PracticeModeEndPoint->GetActorLocation());
                break;
            case EPracticeMode::Putting:
                if (GM->PracticePuttingModeEndPoint) GolfPC->GetAimActor()->SetActorLocation(GM->PracticePuttingModeEndPoint->GetActorLocation());
                break;
            }
        }
    }
    UpdateApproachTargetMarker();
    // 타겟은 이벤트/버튼에서만 갱신(현재 코드 흐름 유지)
}

// ---------- Approach Target Widget ----------
void URangeHUDWidget::SetTargetMarkerVisible(bool bVisible)
{
    if (!Image_Target) return;
    Image_Target->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void URangeHUDWidget::RefreshApproachTargetWidget()
{
    if (!GM)
    {
        SetTargetMarkerVisible(false);
        return;
    }

    if (GM->CurrentPracticeMode == EPracticeMode::Putting)
    {
        SetTargetMarkerVisible(false);
        return;
    }

    if (!GM->BP_Target)
    {
        SetTargetMarkerVisible(false);
        return;
    }

    SetTargetMarkerVisible(true);
    UpdateApproachTargetMarker();
}

void URangeHUDWidget::UpdateApproachTargetMarker()
{
    if (!GM || !Image_Target || !CanvasPanel_Left || !GM->BP_Target) return;

    FGeometry CanvasGeo;
    if (!GetCanvasLeftGeometry(CanvasGeo)) return;

    if (!GM->PracticeModeStartPoint || !GM->PracticeModeEndPoint) return;

    const FVector StartWorld = GM->PracticeModeStartPoint->GetActorLocation();
    const FVector EndWorld = GM->PracticeModeEndPoint->GetActorLocation(); // ✅ 라인과 동일 축
    const FVector TargetWorld = GM->BP_Target->GetActorLocation();

    const FVector2D LocalPos = WorldToCanvasLocalBasis(StartWorld, EndWorld, TargetWorld, CanvasGeo);

    if (UCanvasPanelSlot* CSlot = Cast<UCanvasPanelSlot>(Image_Target->Slot))
    {
        CSlot->SetAlignment(FVector2D(0.5f, 0.5f));
        CSlot->SetPosition(LocalPos);
    }
}

// ---------- Paint ----------
int32 URangeHUDWidget::NativePaint(
    const FPaintArgs& Args,
    const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements,
    int32 LayerId,
    const FWidgetStyle& InWidgetStyle,
    bool bParentEnabled
) const
{
    const int32 Base = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

    if (bDebugLogPaint)
    {
        const double Now = FPlatformTime::Seconds();
        if (Now - GLastPaintLogTimeSec >= DebugLogIntervalSec)
        {
            GLastPaintLogTimeSec = Now;
            FGeometry CanvasGeo;
            const bool bHasCanvas = GetCanvasLeftGeometry(CanvasGeo);
            UE_LOG(LogRangeHUD, Log, TEXT("[Paint] CanvasLeft: %s  Shots=%d  BaseLayer=%d"),
                bHasCanvas ? *DumpGeometry(CanvasGeo) : TEXT("NONE"), ShotPaths.Num(), Base);
        }
    }

    return DrawShotLines(AllottedGeometry, OutDrawElements, Base);
}

void URangeHUDWidget::HandleOnChangedDrivingModeCheckBoxState(bool bIsChecked)
{
    if (!bIsChecked)
    {
        if (CheckBox_DrivingMode) CheckBox_DrivingMode->SetIsChecked(true);
        return;
    }

    UUtilLibrary::LockCheckBoxForSeconds(CheckBox_DrivingMode, GetWorld(), 0.2f);

    if (GM && GM->GetCurrentTurnGolfPlayer()->GetPlayerState() == EPlayerState::Player_Ready)
    {
        ChangePracticeModeState(EPracticeMode::Driving);

        if (CheckBox_ApproachMode) CheckBox_ApproachMode->SetIsChecked(false);
        if (CheckBox_PuttingMode)  CheckBox_PuttingMode->SetIsChecked(false);
        if (CanvasPanel_DIstance)  CanvasPanel_DIstance->SetVisibility(ESlateVisibility::Hidden);

        OnChangedDrivingCheckBoxStateDele.Broadcast();
        GM->PlayerManager->SetSensorClub(CR2CLUB_DRIVER);
    }
    else
    {
        if (CheckBox_DrivingMode) CheckBox_DrivingMode->SetIsChecked(!bIsChecked);
    }

    RefreshApproachTargetWidget();
    RequestRedraw();
}

void URangeHUDWidget::HandleOnChangedApproachModeCheckBoxState(bool bIsChecked)
{
    if (!bIsChecked)
    {
        if (CheckBox_ApproachMode) CheckBox_ApproachMode->SetIsChecked(true);
        return;
    }
    UUtilLibrary::LockCheckBoxForSeconds(CheckBox_ApproachMode, GetWorld(), 0.2f);

    if (GM && GM->GetCurrentTurnGolfPlayer()->GetPlayerState() == EPlayerState::Player_Ready)
    {
        int32 RangeInt = (int32)(ApproachModeDistance * CM_TO_M);
        if (TextBlock_Range) TextBlock_Range->SetText(FText::FromString(FString::Printf(TEXT("%dm"), RangeInt)));

        if (CheckBox_DrivingMode) CheckBox_DrivingMode->SetIsChecked(false);
        if (CheckBox_PuttingMode) CheckBox_PuttingMode->SetIsChecked(false);

        ChangePracticeModeState(EPracticeMode::Approach);
        GM->PlayerManager->SetSensorClub(CR2CLUB_IRON7);

        if (CanvasPanel_DIstance) CanvasPanel_DIstance->SetVisibility(ESlateVisibility::Visible);

        OnChangedApproachCheckBoxStateDele.Broadcast();
    }
    else
    {
        if (CheckBox_ApproachMode) CheckBox_ApproachMode->SetIsChecked(!bIsChecked);
    }

    RefreshApproachTargetWidget();
    RequestRedraw();
}

void URangeHUDWidget::HandleOnChangedPuttingModeCheckBoxState(bool bIsChecked)
{
    if (!bIsChecked)
    {
        if (CheckBox_PuttingMode) CheckBox_PuttingMode->SetIsChecked(true);
        return;
    }
    UUtilLibrary::LockCheckBoxForSeconds(CheckBox_PuttingMode, GetWorld(), 0.2f);

    if (GM && GM->GetCurrentTurnGolfPlayer()->GetPlayerState() == EPlayerState::Player_Ready)
    {
        int32 RangeInt = (int32)(PuttingModeDistance * CM_TO_M);
        if (TextBlock_Range) TextBlock_Range->SetText(FText::FromString(FString::Printf(TEXT("%dm"), RangeInt)));

        if (CheckBox_DrivingMode)  CheckBox_DrivingMode->SetIsChecked(false);
        if (CheckBox_ApproachMode) CheckBox_ApproachMode->SetIsChecked(false);

        ChangePracticeModeState(EPracticeMode::Putting);

        // Putting에서는 Target 숨김
        RefreshApproachTargetWidget();

        if (CanvasPanel_DIstance) CanvasPanel_DIstance->SetVisibility(ESlateVisibility::Visible);

        GM->PlayerManager->SetSensorClub(CR2CLUB_PUTTER);
        OnChangedPuttingCheckBoxStateDele.Broadcast();
    }
    else
    {
        if (CheckBox_PuttingMode) CheckBox_PuttingMode->SetIsChecked(!bIsChecked);
    }
    PuttingModeStartPoint = GM->GetCurrentTurnGolfBall()->GetActorLocation();
    RequestRedraw();
}

// ---------- Game/Delegates ----------
void URangeHUDWidget::Init()
{
    if (!GM || !GM->PlayerManager) return;

    if (GM->PlayerManager->GetPlayers().IsValidIndex(0))
    {
        if (AGolfPlayer* Player = GM->PlayerManager->GetPlayers()[0])
        {
            Player->OnPlayerStateChangedDelegate.AddDynamic(this, &URangeHUDWidget::HandlePlayerState);
        }
    }
    if (GM->PlayerManager->GetPlayerBalls().IsValidIndex(0))
    {
        if (AGolfBall* Ball = GM->PlayerManager->GetPlayerBalls()[0])
        {
            Ball->OnBallGameFlowEvent.AddDynamic(this, &URangeHUDWidget::HandleBallEvent);
        }
    }

    if (Button_Menu)          Button_Menu->OnClicked.AddDynamic(this, &URangeHUDWidget::OnMenuButtonClicked);
    if (Button_ResetShotInfo) Button_ResetShotInfo->OnClicked.AddDynamic(this, &URangeHUDWidget::HandleResetButtonClicked);
    if (Button_CameraMode)    Button_CameraMode->OnClicked.AddDynamic(this, &URangeHUDWidget::HandleCameraModeButtonClicked);
    if (Button_SwingMotion)   Button_SwingMotion->OnClicked.AddDynamic(this, &URangeHUDWidget::HandleSwingMotionButtonClicked);
}

void URangeHUDWidget::UpdateAverageLine()
{
    float AverageBallSpeed = 0;
    float AverageDistance = 0;
    float AverageRemainDistance = 0;
    float AverageDiractionAngle = 0;
    float AverageHeightAngle = 0;

    float TotalBallSpeed = 0;
    float TotalDistance = 0;
    float TotalRemainDistance = 0;
    float TotalDiractionAngle = 0;
    float TotalHeightAngle = 0;

    for (FShotStat ShotStat : ShotStats)
    {
        TotalBallSpeed += ShotStat.BallSpeed;
        TotalDistance += ShotStat.Distance;
        TotalRemainDistance += ShotStat.RemainDistance;
        TotalDiractionAngle += ShotStat.DirectionAngle;
        TotalHeightAngle += ShotStat.LaunchAngle;
    }

    if (ShotCount > 0)
    {
        AverageBallSpeed = TotalBallSpeed / ShotCount;
        AverageDistance = TotalDistance / ShotCount;
        AverageRemainDistance = TotalRemainDistance / ShotCount;
        AverageHeightAngle = TotalHeightAngle / ShotCount;
        AverageDiractionAngle = TotalDiractionAngle / ShotCount;
    }

    if (AverageLine)
    {
        AverageLine->bIsAverageLine = true;
        AverageLine->TextBlock_BallSpeed->SetText(FText::FromString(FString::Printf(TEXT("%.1fm/s"), AverageBallSpeed)));
        AverageLine->TextBlock_Distance->SetText(FText::FromString(FString::Printf(TEXT("%.1fm"), AverageDistance)));
        AverageLine->TextBlock_DirectionAngle->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), AverageDiractionAngle)));
        //AverageLine->TextBlock_RemainDistance->SetText(FText::FromString(FString::Printf(TEXT("%.1fm"), AverageRemainDistance)));
        AverageLine->TextBlock_LaunchAngle->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), AverageHeightAngle)));
    }
}

void URangeHUDWidget::OnMenuButtonClicked()
{

 /*   
        if (GM->GetCurrentTurnGolfPlayer()->GetPlayerState() != EPlayerState::Player_Ready)
        return;
        */

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    AInGameMode* GMPtr = GM;
    if (!IsValid(GMPtr))
    {
        GMPtr = Cast<AInGameMode>(World->GetAuthGameMode());
        GM = GMPtr;
    }

    UUtilLibrary::LockButtonForSeconds(Button_Menu, World, 0.2f);
    SetVisibility(ESlateVisibility::Collapsed);

    if (UGameInstance* GameInstance = World->GetGameInstance())
    {
        if (auto* SM = GameInstance->GetSubsystem<USoundManager>())
        {
            SM->PlayTTS_Interrupt_ById(TEXT("Voice.EndGame"), 0.5f, 1.f);
        }
    }
    if (IsValid(GMPtr) && IsValid(GMPtr->PlayerManager))
    {
        GMPtr->PlayerManager->OnLevelUnload();
    }

    if (IsValid(GMPtr) && IsValid(GMPtr->PlayerManager))
    {
        const TArray<AGolfBall*> Balls = GMPtr->PlayerManager->GetPlayerBalls();
        if (Balls.IsValidIndex(0) && IsValid(Balls[0]))
        {
            const FString Options = TEXT("?game=/Game/UMG/GM_UMG.GM_UMG_C?bFromInGame=true");
            TWeakObjectPtr<UWorld> WorldPtr = World;
            UUtilLibrary::FadeIn(World, 4.0f, FFadeCallback::CreateLambda([WorldPtr, Options]()
                {
                    if (UWorld* FadeWorld = WorldPtr.Get())
                    {
                        UGameplayStatics::OpenLevel(FadeWorld, "Level_UI", false, Options);
                    }
                }));
        }
    }
}




void URangeHUDWidget::SetEnableButtons(bool bIsEnable)
{
    if (Button_ResetShotInfo) Button_ResetShotInfo->SetIsEnabled(bIsEnable);
    if (Button_SwingMotion)   Button_SwingMotion->SetIsEnabled(bIsEnable);
    if (Button_CameraMode)    Button_CameraMode->SetIsEnabled(true);

    const FLinearColor On = FLinearColor::White;
    const FLinearColor Off = FLinearColor(0.49f, 0.49f, 0.49f, 1.f);

    if (Button_ResetShotInfo) Button_ResetShotInfo->SetBackgroundColor(bIsEnable ? On : Off);
    if (Button_SwingMotion)   Button_SwingMotion->SetBackgroundColor(bIsEnable ? On : Off);
    if (Button_CameraMode)    Button_CameraMode->SetBackgroundColor(On);

    UUtilLibrary::LockButtonForSeconds(Button_ResetShotInfo, GetWorld(), 0.5f);
    UUtilLibrary::LockButtonForSeconds(Button_SwingMotion, GetWorld(), 0.5f);
    UUtilLibrary::LockButtonForSeconds(Button_CameraMode, GetWorld(), 0.5f);
}

void URangeHUDWidget::HandlePlayerState(int32 PlayerIndex, EPlayerState PlayerState)
{
    if (PlayerState == EPlayerState::Player_Shot)
    {
        StartShotCapture();
        UpdateShotBallSpeedAndAngle();
        SetEnableButtons(false);
        CheckBox_Mode->SetIsChecked(false);
        CanvasPanel_Modes->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void URangeHUDWidget::HandleBallEvent(EBallEvent BallEvent)
{
    if (!GM) return;

    if (BallEvent == EBallEvent::BallStopped)
    {
        StopShotCapture();
        UpdateShotDistance();
        SetEnableButtons(true);
        ShotCount++;

        if (ShotCount > 10)
        {
            ShotStats.Empty();
            ShotCount = 1;
        }

        FVector BallLocation = GM->GetCurrentTurnGolfBall()->GetActorLocation();
        FVector TargetLocation = GM->PracticeModeEndPoint ? GM->PracticeModeEndPoint->GetActorLocation() : FVector::ZeroVector;

        if (GM->CurrentPracticeMode == EPracticeMode::Approach)
        {
            if (GM->BP_Target) TargetLocation = GM->BP_Target->GetActorLocation();
        }
        else if (GM->CurrentPracticeMode == EPracticeMode::Putting)
        {
            if (GM->PracticePuttingModeEndPoint) TargetLocation = GM->PracticePuttingModeEndPoint->GetActorLocation();
        }

        CurrentShotStat.RemainDistance = FVector::Dist2D(BallLocation, TargetLocation) * CM_TO_M;
        CurrentShotStat.ShotCount = ShotCount;
        UpdateAverageLine();
        OnBallStopDele.Broadcast(CurrentShotStat);
        AddShotStat();
        RequestRedraw();
    }
}

void URangeHUDWidget::AddShotStat()
{
    ShotStats.Add(CurrentShotStat);
    CurrentShotStat = FShotStat();
}

void URangeHUDWidget::HandleResetButtonClicked()
{
    UUtilLibrary::LockButtonForSeconds(Button_ResetShotInfo, GetWorld(), 0.2f);

    ShotPaths.Empty();
    ShotStats.Empty();
    ShotBases.Empty(); // ✅ 추가
    ShotCount = 0;

    if (UWorld* W = GetWorld())
        W->GetTimerManager().ClearTimer(CheckTimerHandle);

    RequestRedraw();
    OnModeChangeDele.Broadcast();
}

void URangeHUDWidget::HandleSwingMotionButtonClicked()
{
    UUtilLibrary::LockButtonForSeconds(Button_SwingMotion, GetWorld(), 0.2f);
    if (GM) GM->ShowSwingMotion(false);
}

void URangeHUDWidget::HandleCameraModeButtonClicked()
{
    if (!GM || !GM->PlayerManager) return;

    AGolfBall* Ball = nullptr;
    if (GM->PlayerManager->GetPlayerBalls().IsValidIndex(0))
        Ball = GM->PlayerManager->GetPlayerBalls()[0];

    if (!Ball || !Ball->LinkedCameraManager) return;

    UUtilLibrary::LockButtonForSeconds(Button_CameraMode, GetWorld(), 0.2f);

    const FVector  BeforeLocation = Ball->LinkedCameraManager->GetActorLocation();
    const FRotator BeforeRotator = Ball->LinkedCameraManager->GetActorRotation();

    if (bFlipCameraMode)
    {
        Ball->LinkedCameraManager->SetFixedCameraPosition(BeforeLocation, BeforeRotator);
        if (Image_CameraMode && CameraModeTexture_Fix)
            Image_CameraMode->SetBrushFromTexture(CameraModeTexture_Fix);

        Ball->LinkedCameraManager->SetUseFixedModeInReady(true);
        Ball->LinkedCameraManager->ChangeCameraMode(ECameraMode::Fixed);
    }
    else
    {
        if (Image_CameraMode && CameraModeTexture_Move)
            Image_CameraMode->SetBrushFromTexture(CameraModeTexture_Move);

        Ball->LinkedCameraManager->SetUseFixedModeInReady(false);
        Ball->LinkedCameraManager->ChangeCameraMode(ECameraMode::Ready);
    }

    bFlipCameraMode = !bFlipCameraMode;
}

void URangeHUDWidget::UpdateShotDistance()
{
    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    UGolfPlayerManager* PM = GameMode ? GameMode->PlayerManager : nullptr;
    if (!GameMode || !PM) return;

    const int32 Idx = GameMode->CurrentPlayerIndex;
    if (!PM->GetPlayers().IsValidIndex(Idx)) return;

    AGolfPlayer* Player = PM->GetPlayers()[Idx];
    if (!Player) return;

    if (TextBlock_ShotInfo_Distance)
        TextBlock_ShotInfo_Distance->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), Player->ShotDistance)));
}

void URangeHUDWidget::UpdateShotBallSpeedAndAngle()
{
    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    UGolfPlayerManager* PM = GameMode ? GameMode->PlayerManager : nullptr;
    AGolfPlayerController* PC = Cast<AGolfPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
    if (!GameMode || !PM || !PC) return;

    const int32 Idx = GameMode->CurrentPlayerIndex;
    if (!PM->GetPlayers().IsValidIndex(Idx)) return;

    AGolfPlayer* Player = PM->GetPlayers()[Idx];
    if (!Player) return;

    CurrentShotStat.BallSpeed = Player->GetSecsorShotPower();
    CurrentShotStat.DirectionAngle = PC->ShotYawAngle;
    CurrentShotStat.LaunchAngle = PC->ShotPitchAngle;

    if (TextBlock_ShotInfo_BallSpeed)      TextBlock_ShotInfo_BallSpeed->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), CurrentShotStat.BallSpeed)));
    if (TextBlock_ShotInfo_EscapeAngle)    TextBlock_ShotInfo_EscapeAngle->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), PC->ShotPitchAngle)));
    if (TextBlock_ShotInfo_LeftRightAngle) TextBlock_ShotInfo_LeftRightAngle->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), PC->ShotYawAngle)));
}

// ---------- 샷 샘플링 ----------
void URangeHUDWidget::StartShotCapture()
{
    // 히스토리 제한: ShotPaths와 ShotBases를 함께 제거
    if (MaxShotHistory > 0 && ShotPaths.Num() >= MaxShotHistory)
    {
        ShotPaths.RemoveAt(0);
        if (ShotBases.Num() > 0)
        {
            ShotBases.RemoveAt(0);
        }
    }

    // 새 샷 추가
    FShotPath NewShot;
    NewShot.StartBall = GetCurrentBallLocation();
    NewShot.Holecup = GetCurrentCupLocation();
    ShotPaths.Add(MoveTemp(NewShot));

    // ✅ 이 샷의 Basis를 "지금 시점" 기준으로 고정 저장
    FVector BasisStart, BasisEnd;
    if (!GetLineBasisForCurrentMode(BasisStart, BasisEnd))
    {
        // Basis를 못 구하면 일단 기본 축으로라도 세팅 (StartBall → Holecup)
        BasisStart = ShotPaths.Last().StartBall;
        BasisEnd = ShotPaths.Last().Holecup;
    }

    FShotBasis NewBasis;
    NewBasis.Start = BasisStart;
    NewBasis.End = BasisEnd;
    ShotBases.Add(NewBasis);

    // 타이머/샘플링
    if (UWorld* W = GetWorld())
    {
        W->GetTimerManager().ClearTimer(CheckTimerHandle);
        W->GetTimerManager().SetTimer(CheckTimerHandle, this, &URangeHUDWidget::SampleBallPosition, CheckInterval, true, 0.f);
    }

    SampleBallPosition();
    RequestRedraw();
}

void URangeHUDWidget::StopShotCapture()
{
    if (UWorld* W = GetWorld())
        W->GetTimerManager().ClearTimer(CheckTimerHandle);

    SampleBallPosition();
    TrimShotHistory();
    RequestRedraw();
}

void URangeHUDWidget::SampleBallPosition()
{
    if (ShotPaths.Num() == 0) return;
    ShotPaths.Last().Samples.Add(GetCurrentBallLocation());
    RequestRedraw();
}

// ---------- 좌표계/그리기 ----------
bool URangeHUDWidget::GetCanvasLeftGeometry(FGeometry& OutGeo) const
{
    if (!CanvasPanel_Left)
    {
        UE_LOG(LogRangeHUD, Error, TEXT("[GetCanvasLeftGeometry] CanvasPanel_Left 가 nullptr 입니다. BP에 BindWidget 이름을 'CanvasPanel_Left'로 맞춰주세요."));
        return false;
    }
    OutGeo = CanvasPanel_Left->GetCachedGeometry();
    return true;
}
FVector2D URangeHUDWidget::WorldToCanvasLocalBasis(
    const FVector& StartWorld,
    const FVector& EndWorld,
    const FVector& WorldLoc,
    const FGeometry& CanvasGeo) const
{
    FVector Fwd = EndWorld - StartWorld;
    Fwd.Z = 0.f;
    if (Fwd.IsNearlyZero())
    {
        Fwd = FVector(1, 0, 0);
    }
    Fwd = Fwd.GetSafeNormal();

    const FVector Right(-Fwd.Y, Fwd.X, 0.f);

    FVector Delta = WorldLoc - StartWorld;
    Delta.Z = 0.f;

    const FVector2D Size = CanvasGeo.GetLocalSize();
    const float CenterX = Size.X * 0.5f;

    const float ForwardMeters = FVector::DotProduct(Delta, Fwd) * CM_TO_M;
    float LateralMeters = FVector::DotProduct(Delta, Right) * CM_TO_M;
    if (bInvertLateralX)
    {
        LateralMeters = -LateralMeters;
    }

    // 분모: 기존 규칙 유지 (Putting은 Start-End 실제 길이, 그 외는 FieldLengthMeters)
    float DenomFieldMeters = FieldLengthMeters;
    if (GM && GM->CurrentPracticeMode == EPracticeMode::Putting)
    {
        const float BasisLenMeters = FVector::Dist2D(StartWorld, EndWorld) * CM_TO_M;
        DenomFieldMeters = FMath::Max(1.0f, BasisLenMeters);
    }

    const float T = FMath::Clamp(ForwardMeters / DenomFieldMeters, 0.f, 1.f);

    const float UseHalfPx = (LateralHalfWidthPixels > 0.f)
        ? LateralHalfWidthPixels
        : FMath::Max(2.f, CenterX - 2.f);

    const float Ratio = FMath::Clamp(LateralMeters / (LateralTotalMeters * 0.5f), -1.f, 1.f);
    const float X = CenterX + Ratio * UseHalfPx;

    // ✅ 여기만 핵심 변경
    // "아래는 -35 기준", "위는 +35 기준"이 되도록 출력 범위를 [Size.Y - 35, 35]로 설정
    const float EdgePadPx = 35.0f;
    const float BottomY = FMath::Max(0.f, Size.Y - EdgePadPx);
    const float TopY = EdgePadPx;

    // Start(T=0) -> BottomY (Size.Y - 35)
    // End(T=1)   -> TopY    (35)
    const float Y = FMath::Lerp(BottomY, TopY, T);

    return FVector2D(X, Y);
}



bool URangeHUDWidget::GetLineBasisForMode(int32 ShotIdx, FVector& OutStartWorld, FVector& OutEndWorld) const
{
    if (!GM) return false;

    // Putting 모드: Putting Start/End로 투영 축 교체
    if (GM->CurrentPracticeMode == EPracticeMode::Putting)
    {
        if (GM->PracticePuttingModeStartPoint && GM->PracticePuttingModeEndPoint)
        {
            //OutStartWorld = GM->PracticePuttingModeStartPoint->GetActorLocation();
            OutStartWorld = PuttingModeStartPoint;
            OutEndWorld = GM->PracticePuttingModeEndPoint->GetActorLocation();
            return true;
        }
    }

    // Driving/Approach 기본: Practice Start/End
    if (GM->PracticeModeStartPoint && GM->PracticeModeEndPoint)
    {
        OutStartWorld = GM->PracticeModeStartPoint->GetActorLocation();
        OutEndWorld = GM->PracticeModeEndPoint->GetActorLocation();
        return true;
    }

    // Fallback: 샷 자체의 Start/Holecup
    if (ShotPaths.IsValidIndex(ShotIdx))
    {
        OutStartWorld = ShotPaths[ShotIdx].StartBall;
        OutEndWorld = ShotPaths[ShotIdx].Holecup;
        return true;
    }

    return false;
}

int32 URangeHUDWidget::DrawShotLines(
    const FGeometry& RootGeo,
    FSlateWindowElementList& OutDrawElements,
    int32 BaseLayerId) const
{
    if (ShotPaths.Num() == 0) return BaseLayerId;

    FGeometry CanvasGeo;
    if (!GetCanvasLeftGeometry(CanvasGeo)) return BaseLayerId;

    const int32 LastIdx = ShotPaths.Num() - 1;
    int32 Layer = BaseLayerId;

    for (int32 i = 0; i < ShotPaths.Num(); ++i)
    {
        const FShotPath& S = ShotPaths[i];
        if (S.Samples.Num() < 2) continue;

        // ✅ 매 프레임/매 샷마다 "현재 모드 기준" Basis를 계산해서 사용
        FVector BasisStart, BasisEnd;
        if (!GetLineBasisForMode(i, BasisStart, BasisEnd))
        {
            continue;
        }

        TArray<FVector2D> Points;
        Points.Reserve(S.Samples.Num());

        for (const FVector& W : S.Samples)
        {
            Points.Add(WorldToCanvasLocalBasis(BasisStart, BasisEnd, W, CanvasGeo));
        }

        const FLinearColor Col = (i == LastIdx) ? RecentStrokeColor : PastStrokeColor;
        const float Thick = FMath::Max(0.5f, LineThickness);

        FSlateDrawElement::MakeLines(
            OutDrawElements,
            ++Layer,
            CanvasGeo.ToPaintGeometry(),
            Points,
            ESlateDrawEffect::None,
            Col,
            true,
            Thick
        );

        if (bDebugDrawGizmos)
        {
            DrawCross(OutDrawElements, CanvasGeo, Points[0], FLinearColor::Green, DebugMarkerPx, Layer);
            DrawCross(OutDrawElements, CanvasGeo, Points.Last(), FLinearColor::Yellow, DebugMarkerPx, Layer);
            DrawCross(OutDrawElements, CanvasGeo,
                FVector2D(CanvasGeo.GetLocalSize().X * 0.5f, CanvasGeo.GetLocalSize().Y * 0.5f),
                FLinearColor::Blue, DebugMarkerPx, Layer);
        }
    }

    if (bDebugLogPaint)
    {
        UE_LOG(LogRangeHUD, Log, TEXT("[DrawShotLines] Canvas=%s  LayerOut=%d"),
            *DumpGeometry(CanvasGeo), Layer);
    }

    return Layer;
}

// ---------- 유틸 ----------
void URangeHUDWidget::RequestRedraw()
{
    if (TSharedPtr<SWidget> Slate = GetCachedWidget())
        Slate->Invalidate(EInvalidateWidgetReason::Paint);
    InvalidateLayoutAndVolatility();
}

void URangeHUDWidget::TrimShotHistory()
{
    while (ShotPaths.Num() > MaxShotHistory)
    {
        ShotPaths.RemoveAt(0);
        if (ShotBases.Num() > 0)
        {
            ShotBases.RemoveAt(0);
        }
    }
}

FVector URangeHUDWidget::GetCurrentBallLocation() const
{
    if (!GM || !GM->PlayerManager) return FVector::ZeroVector;
    const int32 Idx = GM->CurrentPlayerIndex;
    if (!GM->PlayerManager->GetPlayerBalls().IsValidIndex(Idx)) return FVector::ZeroVector;
    AGolfBall* Ball = GM->PlayerManager->GetPlayerBalls()[Idx];
    return Ball ? Ball->GetActorLocation() : FVector::ZeroVector;
}

FVector URangeHUDWidget::GetCurrentCupLocation() const
{
    if (!GM) return FVector::ZeroVector;
    const int32 HoleIdx = FMath::Max(0, GM->CurrentHole - 1);
    if (GM->GameInfo.SelectedMap.HolecupPositions.IsValidIndex(HoleIdx))
        return GM->GameInfo.SelectedMap.HolecupPositions[HoleIdx];
    return FVector::ZeroVector;
}

// ---------- 디버그 ----------
FString URangeHUDWidget::DumpGeometry(const FGeometry& G)
{
    return FString::Printf(TEXT("LocalSize=(%.1f,%.1f) AbsPos=(%.1f,%.1f)"),
        G.GetLocalSize().X, G.GetLocalSize().Y,
        G.GetAbsolutePosition().X, G.GetAbsolutePosition().Y);
}

FString URangeHUDWidget::DumpVec(const FVector& V)
{
    return FString::Printf(TEXT("(X=%.1f Y=%.1f Z=%.1f)"), V.X, V.Y, V.Z);
}

FString URangeHUDWidget::DumpPt(const FVector2D& P)
{
    return FString::Printf(TEXT("(x=%.1f y=%.1f)"), P.X, P.Y);
}

void URangeHUDWidget::DrawCross(FSlateWindowElementList& OutDrawElements, const FGeometry& TargetGeo,
    const FVector2D& P, const FLinearColor& C, float SizePx, int32& LayerId) const
{
    const float s = SizePx * 0.5f;
    TArray<FVector2D> H; H.Add(FVector2D(P.X - s, P.Y)); H.Add(FVector2D(P.X + s, P.Y));
    TArray<FVector2D> V; V.Add(FVector2D(P.X, P.Y - s)); V.Add(FVector2D(P.X, P.Y + s));

    FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId, TargetGeo.ToPaintGeometry(), H,
        ESlateDrawEffect::None, C, true, 1.f);
    FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId, TargetGeo.ToPaintGeometry(), V,
        ESlateDrawEffect::None, C, true, 1.f);
}

bool URangeHUDWidget::GetLineBasisForCurrentMode(FVector& OutStartWorld, FVector& OutEndWorld) const
{
    if (!GM)
        return false;

    // Putting
    if (GM->CurrentPracticeMode == EPracticeMode::Putting)
    {
        if (GM->PracticePuttingModeStartPoint && GM->PracticePuttingModeEndPoint)
        {
            OutStartWorld = GM->PracticePuttingModeStartPoint->GetActorLocation();
            OutEndWorld = GM->PracticePuttingModeEndPoint->GetActorLocation();
            return true;
        }
        return false;
    }

    // Driving / Approach 공통(현재 프로젝트 구성 기준)
    if (GM->PracticeModeStartPoint && GM->PracticeModeEndPoint)
    {
        OutStartWorld = GM->PracticeModeStartPoint->GetActorLocation();
        OutEndWorld = GM->PracticeModeEndPoint->GetActorLocation();
        return true;
    }

    return false;
}

void URangeHUDWidget::ValidateAndReBindWidgets()
{
    UE_LOG(LogRangeHUD, Warning, TEXT("[Diag] WidgetClass=%s"), *GetNameSafe(GetClass()));
    UE_LOG(LogRangeHUD, Warning, TEXT("[Diag] WidgetPath=%s"), *GetPathNameSafe(GetClass()));
    UE_LOG(LogRangeHUD, Warning, TEXT("[Diag] Outer=%s"), *GetPathNameSafe(GetOuter()));

    DumpWidgetTree(WidgetTree);

    UE_LOG(LogRangeHUD, Warning, TEXT("🔵 RangeHUDWidget::NativeConstruct() START"));

    // ============================================================================
    // Step 1: 텍스처 로드
    // ============================================================================
    if (!CameraModeTexture_Fix)
    {
        CameraModeTexture_Fix = LoadObject<UTexture2D>(nullptr,
            TEXT("/Game/UMG/Resources/Images/InGame/RangeHUD/T_RangeHUD_CameraMode_Fix.T_RangeHUD_CameraMode_Fix"));
        if (CameraModeTexture_Fix)
        {
            UE_LOG(LogRangeHUD, Log, TEXT("✅ CameraModeTexture_Fix loaded"));
        }
    }

    if (!CameraModeTexture_Move)
    {
        CameraModeTexture_Move = LoadObject<UTexture2D>(nullptr,
            TEXT("/Game/UMG/Resources/Images/InGame/RangeHUD/T_RangeHUD_CameraMode_Move.T_RangeHUD_CameraMode_Move"));
        if (CameraModeTexture_Move)
        {
            UE_LOG(LogRangeHUD, Log, TEXT("✅ CameraModeTexture_Move loaded"));
        }
    }

    // ============================================================================
    // Step 2: 모든 BindWidget 검증
    // ============================================================================

    // --- CheckBox 위젯 검증 ---

    if (!CanvasPanel_Modes)
    {
        CanvasPanel_Modes = Cast<UCanvasPanel>(WidgetTree ? WidgetTree->FindWidget(TEXT("CanvasPanel_Modes")) : nullptr);
        UE_LOG(LogRangeHUD, Warning, TEXT("[Fixup] CanvasPanel_Modes reassigned via WidgetTree: %s"), *GetNameSafe(CanvasPanel_Modes));
    }

    if (!CanvasPanel_Modes)
    {
        UE_LOG(LogRangeHUD, Error, TEXT("❌ CRITICAL: CanvasPanel_Modes still nullptr after fixup"));
        return;
    }

    UE_LOG(LogRangeHUD, Log, TEXT("✅ CanvasPanel_Modes validated"));


    if (!CheckBox_Mode)
    {
        CheckBox_Mode = Cast<UCheckBox>(WidgetTree ? WidgetTree->FindWidget(TEXT("CheckBox_Mode")) : nullptr);
        UE_LOG(LogRangeHUD, Warning, TEXT("[Fixup] CheckBox_Mode reassigned via WidgetTree: %s"), *GetNameSafe(CheckBox_Mode));
    }

    if (!CheckBox_Mode)
    {
        UE_LOG(LogRangeHUD, Error, TEXT("❌ CRITICAL: CheckBox_Mode still nullptr after fixup"));
        return;
    }

    UE_LOG(LogRangeHUD, Log, TEXT("✅ CheckBox_Mode validated"));


    if (!CheckBox_DrivingMode)
    {
        CheckBox_DrivingMode = Cast<UCheckBox>(WidgetTree ? WidgetTree->FindWidget(TEXT("CheckBox_DrivingMode")) : nullptr);
        UE_LOG(LogRangeHUD, Warning, TEXT("[Fixup] CheckBox_DrivingMode reassigned via WidgetTree: %s"), *GetNameSafe(CheckBox_DrivingMode));
    }

    if (!CheckBox_DrivingMode)
    {
        UE_LOG(LogRangeHUD, Error, TEXT("❌ CRITICAL: CheckBox_DrivingMode still nullptr after fixup"));
        return;
    }
    UE_LOG(LogRangeHUD, Log, TEXT("✅ CheckBox_DrivingMode validated"));



    if (!CheckBox_ApproachMode)
    {
        CheckBox_ApproachMode = Cast<UCheckBox>(WidgetTree ? WidgetTree->FindWidget(TEXT("CheckBox_ApproachMode")) : nullptr);
        UE_LOG(LogRangeHUD, Warning, TEXT("[Fixup] CheckBox_ApproachMode reassigned via WidgetTree: %s"), *GetNameSafe(CheckBox_ApproachMode));
    }
    UE_LOG(LogRangeHUD, Log, TEXT("✅ CheckBox_ApproachMode validated"));

    if (!CheckBox_PuttingMode)
    {
        CheckBox_PuttingMode = Cast<UCheckBox>(WidgetTree ? WidgetTree->FindWidget(TEXT("CheckBox_PuttingMode")) : nullptr);
        UE_LOG(LogRangeHUD, Warning, TEXT("[Fixup] CheckBox_PuttingMode reassigned via WidgetTree: %s"), *GetNameSafe(CheckBox_PuttingMode));
    }
    UE_LOG(LogRangeHUD, Log, TEXT("✅ CheckBox_PuttingMode validated"));

    if (!Button_Stat)
    {
        Button_Stat = Cast<UButton>(WidgetTree ? WidgetTree->FindWidget(TEXT("Button_Stat")) : nullptr);
        UE_LOG(LogRangeHUD, Warning, TEXT("[Fixup] Button_Stat reassigned via WidgetTree: %s"), *GetNameSafe(Button_Stat));
    }
    UE_LOG(LogRangeHUD, Log, TEXT("✅ Slider_Range validated"));

    if (!WrapBox_Menu)
    {
        WrapBox_Menu = Cast<UWrapBox>(WidgetTree ? WidgetTree->FindWidget(TEXT("WrapBox_Menu")) : nullptr);
        UE_LOG(LogRangeHUD, Warning, TEXT("[Fixup] WrapBox_Menu reassigned via WidgetTree: %s"), *GetNameSafe(WrapBox_Menu));
    }
    UE_LOG(LogRangeHUD, Log, TEXT("✅ WrapBox_Menu validated"));

    if (!Button_LeftArrow)
    {
        Button_LeftArrow = Cast<UButton>(WidgetTree ? WidgetTree->FindWidget(TEXT("Button_LeftArrow")) : nullptr);
        UE_LOG(LogRangeHUD, Warning, TEXT("[Fixup] Button_LeftArrow reassigned via WidgetTree: %s"), *GetNameSafe(Button_LeftArrow));
    }
    UE_LOG(LogRangeHUD, Log, TEXT("✅ Button_LeftArrow validated"));

    if (!Button_RightArrow)
    {
        Button_RightArrow = Cast<UButton>(WidgetTree ? WidgetTree->FindWidget(TEXT("Button_RightArrow")) : nullptr);
        UE_LOG(LogRangeHUD, Warning, TEXT("[Fixup] Button_RightArrow reassigned via WidgetTree: %s"), *GetNameSafe(Button_RightArrow));
    }
    UE_LOG(LogRangeHUD, Log, TEXT("✅ Button_RightArrow validated"));
    if (!CanvasPanel_DIstance)
    {
        CanvasPanel_DIstance = Cast<UCanvasPanel>(WidgetTree ? WidgetTree->FindWidget(TEXT("CanvasPanel_DIstance")) : nullptr);
        UE_LOG(LogRangeHUD, Warning, TEXT("[Fixup] CanvasPanel_DIstance reassigned via WidgetTree: %s"), *GetNameSafe(CanvasPanel_DIstance));
    }
    UE_LOG(LogRangeHUD, Log, TEXT("✅ CanvasPanel_DIstance validated"));
    if (!Image_Target)
    {
        Image_Target = Cast<UImage>(WidgetTree ? WidgetTree->FindWidget(TEXT("Image_Target")) : nullptr);
        UE_LOG(LogRangeHUD, Warning, TEXT("[Fixup] Image_Target reassigned via WidgetTree: %s"), *GetNameSafe(Image_Target));
    }
    UE_LOG(LogRangeHUD, Log, TEXT("✅ Image_Target validated"));

    // ============================================================================
    // Step 3: 델리게이트 바인드 (모두 유효함을 확인 후)
    // ============================================================================

    CheckBox_Mode->OnCheckStateChanged.AddDynamic(this, &URangeHUDWidget::HandleOnChangedCheckBoxMode);
    UE_LOG(LogRangeHUD, Log, TEXT("✅ CheckBox_Mode delegate bound"));

    CheckBox_DrivingMode->OnCheckStateChanged.AddDynamic(this, &URangeHUDWidget::HandleOnChangedDrivingModeCheckBoxState);
    UE_LOG(LogRangeHUD, Log, TEXT("✅ CheckBox_DrivingMode delegate bound"));

    CheckBox_ApproachMode->OnCheckStateChanged.AddDynamic(this, &URangeHUDWidget::HandleOnChangedApproachModeCheckBoxState);
    UE_LOG(LogRangeHUD, Log, TEXT("✅ CheckBox_ApproachMode delegate bound"));

    CheckBox_PuttingMode->OnCheckStateChanged.AddDynamic(this, &URangeHUDWidget::HandleOnChangedPuttingModeCheckBoxState);
    UE_LOG(LogRangeHUD, Log, TEXT("✅ CheckBox_PuttingMode delegate bound"));

    Button_LeftArrow->OnPressed.AddDynamic(this, &URangeHUDWidget::HandleOnPressedLeftArrowButton);
    Button_RightArrow->OnPressed.AddDynamic(this, &URangeHUDWidget::HandleOnPressedRightArrowButton);

    Button_Stat->OnPressed.AddDynamic(this, &URangeHUDWidget::HandleOnPressedStatButton);
    UE_LOG(LogRangeHUD, Log, TEXT("✅ Button_Stat delegate bound"));
}
