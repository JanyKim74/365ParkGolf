#include "GolfPlayerController.h"

#include "CameraFXComponent.h"
#include "CameraManager.h"
#include "GolfPlayer.h"
#include "GolfBall.h"
#include "InGameMode.h" // InGameMode 참조를 위해 필요
#include "GolfPlayerManager.h" // UGolfPlayerManager 참조를 위해 필요
#include "DrawDebugHelpers.h"
#include "StrokeWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h" // 추가
#include "Math/Rotator.h"
#include "WebcamCapture.h"
#include "Widgets/ResultWidget.h"
#include "Engine/StaticMeshActor.h"  // TerrainHeightGrid.cpp 상단에 추가
#include "InGameMode.h"
#include "ShotCinematicCameraActor.h"
#include "ShotCinematicComponent.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "ParkDayProfiling.h"


AGolfPlayerController::AGolfPlayerController()
{
    AimDirection = FVector(1, 0, 0).GetSafeNormal();
    ShotPower = 25.0f;
    ShotPitchAngle = 15.0f; // 기본 상승 각도 10도

    // 샷 조절 설정 ⭐ 추가
    PowerAdjustStep = 2.0f;   // 2m/s 단위로 조절
    AngleAdjustStep = 2.0f;   // 2도 단위로 조절

    // 포인터 초기화
    ShotControlWidget = nullptr;
    ShotControlWidgetClass = nullptr;

    AimActor = nullptr; // ⭐ Added

    // ⭐ GameMode 및 PlayerManager 캐시 초기화
    CachedGameMode = nullptr;
    CachedPlayerManager = nullptr;

    ShotYawAngle = 0.0f;

    // ✅ 스윙 녹화 시스템 초기화
    WebcamCaptureActor = nullptr;
    WebcamCaptureClass = nullptr;

    bAutoExecutePlayerShotOnSwing = true;
    bLogSwingEvents = true;
    bAutoPlaySwingReplay = true;
    SwingReplayDelay = 0.5f;

    LastSwingRecordingTime = 0.0f;
    SwingRecordingCooldown = 1.0f;  // 1초 쿨다운



    ShotCinematicComponent = CreateDefaultSubobject<UShotCinematicComponent>(TEXT("ShotCinematicComponent"));
    //ShotCinematicComponent->SetupAttachment(RootComponent);


}


void AGolfPlayerController::BeginPlay()
{
    Super::BeginPlay();


    //SetInputMode(FInputModeGameOnly());
   // bShowMouseCursor = true;
   // EnableInput(this);
    FInputModeGameAndUI InputMode;
    InputMode.SetWidgetToFocus(nullptr);  // 또는 미니맵 위젯 포커스
    SetInputMode(InputMode);
    bShowMouseCursor = true;


    if (CameraManager)
    {
        FViewTargetTransitionParams TransitionParams;
        TransitionParams.BlendTime = 0.5f;
        SetViewTarget(CameraManager, TransitionParams);
        UE_LOG(LogGameMode, Log, TEXT("CameraManager set as view target: %s"), *CameraManager->GetName());


        // 초기 AimDirection을 카메라 전방 방향으로 설정
        //AimDirection = CameraManager->Camera->GetForwardVector().GetSafeNormal();
        UE_LOG(LogGameMode, Log, TEXT("Initial AimDirection set to camera forward: %s"), *AimDirection.ToString());


    }
    else
    {
        UE_LOG(LogGameMode, Warning, TEXT("CameraManager is null in BeginPlay"));
    }

    // ⭐ GameMode 및 PlayerManager 캐시
    CachedGameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    if (CachedGameMode)
    {
        CachedPlayerManager = CachedGameMode->PlayerManager;
    }


    DebugInputStatus();

    // 위젯 생성을 지연시켜서 InGameMode에서 클래스를 설정할 시간을 줍니다
    GetWorld()->GetTimerManager().SetTimer(
        DelayedWidgetCreationTimer,
        this,
        &AGolfPlayerController::CreateShotControlWidget,
        0.1f,  // 0.1초 지연
        false
    );

    // 샷 상태 초기화
    bShotInProgress = false;

    // ⭐ 새로 추가: TerrainGrid 초기화
    InitializeTerrainGrid();

    // ⭐ 새로 추가: AimActor 초기화
    InitializeAimActor();

    // ✅ WebcamCapture BP 지연 로드 (CDO 경고 방지)
    if (!WebcamCaptureClass)
    {
        WebcamCaptureClass = LoadClass<AWebcamCapture>(
            nullptr,
            TEXT("/Game/GolfGameBluePrint/SwingAnalyzer/BP_WebCamCapture_ints.BP_WebCamCapture_ints_C")
        );
        if (WebcamCaptureClass)
        {
            UE_LOG(LogTemp, Log, TEXT("✅ WebcamCaptureClass 지연 로드 성공: %s"),
                *WebcamCaptureClass->GetName());
        }
        else
        {
            UE_LOG(LogTemp, Warning,
                TEXT("⚠️ WebcamCaptureClass 지연 로드 실패 — 경로 확인 필요: BP_WebCamCapture_ints"));
        }
    }

    // ✅ 스윙 녹화 시스템 자동 초기화
    if (WebcamCaptureClass)
    {

        FTimerHandle SwingRecordingTimer;
        GetWorld()->GetTimerManager().SetTimer(
            SwingRecordingTimer,
            this,
            &AGolfPlayerController::InitializeSwingRecording,
            2.0f,   // 2초 후 초기화
            false
        );

        UE_LOG(LogTemp, Log, TEXT("📹 Swing recording initialization scheduled"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ WebcamCaptureClass not set"));
    }
    if (VideoWidget)
    {
        VideoWidget->OnClipPlaybackFinished.AddDynamic(
            this, &AGolfPlayerController::OnClipFinished);
    }
}


// GolfPlayerController.cpp BeginDestroy 오버라이드
void AGolfPlayerController::BeginDestroy()
{
    // 모든 타이머 정리
    if (GetWorld())
    {
        FTimerManager& TimerManager = GetWorld()->GetTimerManager();
        TimerManager.ClearAllTimersForObject(this);
    }

    // ✅ 델리게이트 해제 (메모리 누수 방지)
    if (WebcamCaptureActor && IsValid(WebcamCaptureActor))
    {
        UE_LOG(LogTemp, Log, TEXT("  🔓 Cleaning up WebcamCaptureActor"));

        // ✅ 1. 델리게이트 해제 (주석 해제!)
        WebcamCaptureActor->OnSwingDetected.RemoveDynamic(
            this,
            &AGolfPlayerController::OnSwingRecordedHandler
        );

        // ✅ 2. 캡처 중지
        WebcamCaptureActor->StopCapture();

        // ✅ 3. VideoWidget 정리 (크래시 방지!)
        if (WebcamCaptureActor->VideoWidget && IsValid(WebcamCaptureActor->VideoWidget))
        {
            if (WebcamCaptureActor->VideoWidget->VideoDisplay &&
                IsValid(WebcamCaptureActor->VideoWidget->VideoDisplay))
            {
                FSlateBrush EmptyBrush;
                WebcamCaptureActor->VideoWidget->VideoDisplay->SetBrush(EmptyBrush);
            }
            WebcamCaptureActor->VideoWidget->RemoveFromParent();
        }

        // ✅ 4. 액터 파괴
       // WebcamCaptureActor->Destroy();
        WebcamCaptureActor = nullptr;

        UE_LOG(LogTemp, Log, TEXT("  ✅ WebcamCaptureActor destroyed"));
    }

    Super::BeginDestroy();
}


void AGolfPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (InputComponent)
    {
        InputComponent->BindAxis("RotateLeft", this, &AGolfPlayerController::MoveAimHorizontal).AxisValue = -1.0f;
        InputComponent->BindAxis("RotateRight", this, &AGolfPlayerController::MoveAimHorizontal).AxisValue = 1.0f;
        InputComponent->BindAxis("AdjustPower", this, &AGolfPlayerController::AdjustPower);
        // InputComponent->BindAction("Shot", IE_Pressed, this, &AGolfPlayerController::OnShot);
        InputComponent->BindAction("Changeview", IE_Pressed, this, &AGolfPlayerController::CameraSetView);
        InputComponent->BindAction("RotateLeft", IE_Pressed, this, &AGolfPlayerController::RotateLeft);
        InputComponent->BindAction("RotateRight", IE_Pressed, this, &AGolfPlayerController::RotateRight);

        // 저항 조절 입력
        InputComponent->BindAction("IncreaseFriction", IE_Pressed, this, &AGolfPlayerController::IncreaseFriction);
        InputComponent->BindAction("DecreaseFriction", IE_Pressed, this, &AGolfPlayerController::DecreaseFriction);
        UE_LOG(LogTemp, Log, TEXT("Input bindings set up successfully"));


        // 샷 조절 UI 입력 바인딩들 ⭐ 추가
        InputComponent->BindAction("OpenShotControl", IE_Pressed, this, &AGolfPlayerController::OnOpenShotControl);
        InputComponent->BindAction("PowerUp", IE_Pressed, this, &AGolfPlayerController::OnPowerUp);
        InputComponent->BindAction("PowerDown", IE_Pressed, this, &AGolfPlayerController::OnPowerDown);
        InputComponent->BindAction("AngleUp", IE_Pressed, this, &AGolfPlayerController::OnAngleUp);
        InputComponent->BindAction("AngleDown", IE_Pressed, this, &AGolfPlayerController::OnAngleDown);
        InputComponent->BindAction("BounceFix", IE_Pressed, this, &AGolfPlayerController::OnBounceFix);

        // ===== 새로운 디버깅 바인딩들 추가 =====
       // InputComponent->BindAction("DebugBallPhysics", IE_Pressed, this, AGolfPlayerController::LogCurrentBallPhysics);
        InputComponent->BindAction("TogglePhysicsDebug", IE_Pressed, this, &AGolfPlayerController::ToggleBallPhysicsDebug);


        // ⭐ 추가: LandscapeChecker 디버그 키 바인딩 (선택사항)
        InputComponent->BindAction("ToggleLandDebug", IE_Pressed, this, &AGolfPlayerController::ToggleLandscapeDebug);
        InputComponent->BindAction("ShowLandType", IE_Pressed, this, &AGolfPlayerController::ShowCurrentLandType);
        InputComponent->BindAction("ShowLandGrid", IE_Pressed, this, &AGolfPlayerController::ShowLandTypeGrid);

        // ⭐ 새로 추가: 턴 전환 스킵 바인딩 (개발자/관리자용)
        InputComponent->BindAction("SkipTurnTransition", IE_Pressed, this, &AGolfPlayerController::SkipTurnTransition);


        // ⭐ 새로 추가: TerrainGrid 관련 키 바인딩
        InputComponent->BindAction("ToggleTerrainGrid", IE_Pressed, this, &AGolfPlayerController::ToggleTerrainGrid);
        InputComponent->BindAction("RefreshTerrainGrid", IE_Pressed, this, &AGolfPlayerController::RefreshTerrainGrid);

        InputComponent->BindAction("Mulligan", IE_Pressed, this, &AGolfPlayerController::SetMulligan);

        InputComponent->BindAction("DebugShot", IE_Pressed, this, &AGolfPlayerController::DebugCurrentBallShot);
        InputComponent->BindAction("ForceShot", IE_Pressed, this, &AGolfPlayerController::ForceCurrentBallShot);

        InputComponent->BindAction("NextHole", IE_Pressed, this, &AGolfPlayerController::SetNextHole);
        InputComponent->BindAction("Score", IE_Pressed, this, &AGolfPlayerController::SetNextHole);

        InputComponent->BindAction("Landtype", IE_Pressed, this, &AGolfPlayerController::SetLandtype);

        InputComponent->BindAction("LastHole", IE_Pressed, this, &AGolfPlayerController::SetLastHole);

        InputComponent->BindAction("Particle", IE_Pressed, this, &AGolfPlayerController::SetParticle);

        InputComponent->BindAction("RepeatLastShot", IE_Pressed, this, &AGolfPlayerController::OnRepeatLastShot);

        InputComponent->BindAction("ResetMinimap", IE_Pressed, this, &AGolfPlayerController::OnResetMinimap);

        UE_LOG(LogTemp, Log, TEXT("Input bindings set up successfully (including physics debug)"));

        InputComponent->BindAction("PenaltyDrop", IE_Pressed, this, &AGolfPlayerController::SetPenaltyDrop);

        // InputComponent->BindAction("ShowSwingVideo", IE_Pressed, this, &AGolfPlayerController::ShowSwingVideoWidget);
        InputComponent->BindAction("ShowSwingVideo", IE_Pressed, this, &AGolfPlayerController::ShowSwingMovieWidget);
        // InputComponent->BindAction("ShowSwingVideo", IE_Pressed, this, &AGolfPlayerController::PlaySwingReplayDelayed);

         // ✅ 스윙 녹화 트리거 키 바인딩
        InputComponent->BindAction("RecordSwing", IE_Pressed, this,
            &AGolfPlayerController::TriggerSwingRecording);

        // ✅ 스윙 리플레이 재생 키 바인딩 (선택사항)
        InputComponent->BindAction("PlaySwingReplay", IE_Pressed, this,
            &AGolfPlayerController::PlayLastSwingReplay);


        InputComponent->BindAction("SimpleBall", IE_Pressed, this, &AGolfPlayerController::SettingSimpleBall);

        InputComponent->BindAction("ComplexBall", IE_Pressed, this, &AGolfPlayerController::SettingComplexBall);



        UE_LOG(LogTemp, Log, TEXT("✅ Swing recording input bindings set up"));

    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("InputComponent is null in SetupInputComponent"));
    }

}

void AGolfPlayerController::SetParticle()
{
    if (AGolfBall* Ball = CachedPlayerManager->GetPlayerBalls()[CachedGameMode->CurrentPlayerIndex])
    {
        //Ball->CameraFXComponent->PlayHoleInFX(1);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("AGolfPlayerController::SetParticle() = Ball Is null"));
    }
}

void AGolfPlayerController::Tick(float DeltaTime)
{
    SCOPE_CYCLE_COUNTER(STAT_PCTick);

    Super::Tick(DeltaTime);




    // ✅ Swing 기록 중 프레임 모니터링
    if (bIsRecordingSwing && WebcamCaptureActor->VideoBufferComponent)
    {
        int32 BufferedFrames = WebcamCaptureActor->VideoBufferComponent->GetBufferedFrameCount();

        // 목표 프레임 도달?
        if (BufferedFrames >= TargetFramesForSwing)
        {
            UE_LOG(LogTemp, Warning, TEXT(""));
            UE_LOG(LogTemp, Warning, TEXT("✅ Swing frame completion!"));
            UE_LOG(LogTemp, Warning, TEXT("   Buffered: %d frames"), BufferedFrames);
            UE_LOG(LogTemp, Warning, TEXT("   Target: %d frames"), TargetFramesForSwing);

            // ✅ 프레임 완성 후 중지
            WebcamCaptureActor->StopCapture();
            bIsRecordingSwing = false;

            UE_LOG(LogTemp, Warning, TEXT("🛑 Capture stopped after frame completion"));
        }
    }


    if (CachedGameMode && CachedGameMode->PlayerManager)
    {
        int32 CurrentPlayerIndex = CachedGameMode->CurrentPlayerIndex;

        // 플레이어 인덱스 변경 감지
        if (PreviousPlayerIndex != CurrentPlayerIndex)
        {
            if (PreviousPlayerIndex >= 0)
            {
                NotifyMiniMapPlayerChanged(CurrentPlayerIndex, PreviousPlayerIndex);

                if (AimActor)
                {
                    UpdateAimActorPosition();
                    UpdateMiniMapAim();
                }
            }
            PreviousPlayerIndex = CurrentPlayerIndex;
        }

        // ⭐⭐⭐ 카메라 방향 동기화 - 수동 입력이 없을 때만
        //if (CameraManager && CameraManager->Camera)
        //{
        //    // 미니맵 클릭 상태 체크
        //    bool bIsMinimapClicked = CachedGameMode->bClickedMinimap &&
        //        !CachedGameMode->AimLocation.IsZero();

        //    // ⭐ 수동 회전 중인지 체크 (추가된 플래그)
        //    if (!bIsMinimapClicked && !bIsManuallyRotating)
        //    {
        //        FVector CurrentCameraForward = CameraManager->Camera->GetForwardVector();
        //        CurrentCameraForward.Z = 0.0f;
        //        CurrentCameraForward.Normalize();

        //        if (!AimDirection.Equals(CurrentCameraForward, 0.01f))
        //        {
        //            AimDirection = CurrentCameraForward;
        //            // Tick에서는 AimActor를 업데이트하지 않음!
        //            // MoveAimHorizontal에서만 업데이트
        //        }
        //    }

        //    // ⭐⭐⭐ bClickedMinimap을 여기서 절대 false로 설정하지 않음!
        //    // 오직 MoveAimHorizontal에서만 false로 설정
        //}

        // 물리 디버깅 정보 표시
#if WITH_EDITOR
        if (bShowPhysicsDebug)
        {
            TArray<AGolfBall*> PlayerBalls = CachedGameMode->PlayerManager->GetPlayerBalls();
            if (PlayerBalls.IsValidIndex(CurrentPlayerIndex))
            {
                AGolfBall* Ball = PlayerBalls[CurrentPlayerIndex];
                if (Ball && GEngine)
                {
                    float Speed = Ball->GetBallSpeed();
                    EBallState State = Ball->GetBallState();

                    GEngine->AddOnScreenDebugMessage(100, DeltaTime, FColor::White,
                        FString::Printf(TEXT("Player %d | State: %s"), CurrentPlayerIndex,
                            *UEnum::GetValueAsString(State)));

                    GEngine->AddOnScreenDebugMessage(101, DeltaTime, FColor::Yellow,
                        FString::Printf(TEXT("Speed: %.1f cm/s (%.2f m/s)"),
                            Speed, Speed / 100.0f));

                    GEngine->AddOnScreenDebugMessage(102, DeltaTime, FColor::Cyan,
                        FString::Printf(TEXT("Physics: %s | Gravity: %s"),
                            Ball->BallMesh->IsSimulatingPhysics() ? TEXT("ON") : TEXT("OFF"),
                            Ball->BallMesh->IsGravityEnabled() ? TEXT("ON") : TEXT("OFF")));
                }
            }
        }

        // 턴 전환 상태 모니터링
        if (IsAnyBallInTurnTransition())
        {
            float CurrentTransitionTime = GetCurrentBallTurnTransitionTime();
            if (CurrentTransitionTime > 0.0f && GEngine)
            {
                GEngine->AddOnScreenDebugMessage(998, DeltaTime + 0.01f, FColor::Orange,
                    FString::Printf(TEXT("🕐 Turn Transition: %.1fs"), CurrentTransitionTime));
            }
        }
#endif
    }
}


void AGolfPlayerController::SetMulligan()
{
    CachedGameMode->OnAutoTeeKeyPressed(EAutoTeeKey::Mulligan);

    //  CachedGameMode->StrokeWidgetInstance->OnMulliganButtonClicked();
}


void AGolfPlayerController::MoveAimHorizontal(float Value)
{
    if (Value == 0.0f) return;

    // 1. 수동 회전 시작 플래그 설정
    bIsManuallyRotating = true;

    // 2. 미니맵 클릭 상태 해제
    if (CachedGameMode)
    {
        CachedGameMode->bClickedMinimap = false;
    }

    // 3. ⭐⭐⭐ AimActor를 회전시키고 새 위치 계산
    UpdateAimActorByRotation(Value * 2.0f);

    // 4. ⭐⭐⭐ 카메라를 새로운 AimActor 위치 기준으로 즉시 재배치
    PositionCameraForAim();

    // 5. ⭐⭐⭐ 미니맵 전체 업데이트 (방향 + 위치 + 라인)
    UpdateMiniMapForRotation();


    // 6. 수동 회전 플래그 리셋 타이머
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ManualRotationTimer);
        World->GetTimerManager().SetTimer(
            ManualRotationTimer,
            [this]() { bIsManuallyRotating = false; },
            0.1f,
            false
        );
    }
    if (CachedGameMode->IsStrokeMode())
    {
        CachedGameMode->StrokeWidgetInstance->ShowPuttingGuidancePanel(false);
        CachedGameMode->StrokeWidgetInstance->PositionCanvasPanelAboveHole();
    }
}

// =============================================================================
// UpdateAimActorByRotation - 회전값으로 AimActor 위치 업데이트
// =============================================================================
void AGolfPlayerController::UpdateAimActorByRotation(float DeltaYaw)
{
    if (!AimActor || !CachedGameMode || !CachedGameMode->PlayerManager)
    {
        UE_LOG(LogTemp, Log, TEXT("UpdateAimActorByRotation: Missing components"));
        return;
    }

    // 현재 볼 위치 가져오기
    TArray<AGolfBall*> PlayerBalls = CachedGameMode->PlayerManager->GetPlayerBalls();
    if (!PlayerBalls.IsValidIndex(CachedGameMode->CurrentPlayerIndex))
        return;

    AGolfBall* CurrentBall = PlayerBalls[CachedGameMode->CurrentPlayerIndex];
    if (!CurrentBall)
        return;

    FVector BallLocation = CurrentBall->GetActorLocation();

    // ⭐⭐⭐ 현재 AimDirection을 기준으로 회전 (홀컵 기준 아님)
    // 기존 방향에서 회전
    FVector CurrentDirection = AimDirection;
    CurrentDirection.Z = 0.0f;
    CurrentDirection.Normalize();

    // ⭐ Yaw 회전 적용
    FVector RotatedDirection = CurrentDirection.RotateAngleAxis(DeltaYaw, FVector::UpVector);
    RotatedDirection.Normalize();

    // AimDirection 업데이트
    AimDirection = RotatedDirection;

    // ✅ 최적화: 매 입력 Log → VeryVerbose
    UE_LOG(LogTemp, VeryVerbose, TEXT("🎯 Direction rotated: %.1f degrees, New direction=%s"),
        DeltaYaw, *AimDirection.ToString());

    // ⭐⭐⭐ 새로운 방향으로 AimActor 위치 계산
    FVector TargetPosition = BallLocation + (RotatedDirection * AimActorDistance);

    // 홀컵 거리 제한 체크
    FVector HolecupLocation = GetCurrentHolecupPosition();
    FVector OptimalPosition;
    if (!HolecupLocation.IsZero())
    {
        float DistanceToHole = FVector::Dist(BallLocation, HolecupLocation);

        // 홀컵매칭
        if (AimActorDistance >= DistanceToHole)
        {
            TargetPosition = HolecupLocation;
            UE_LOG(LogTemp, Log, TEXT("🎯 Aim limited to holecup distance"));


            if (CachedGameMode->IsStrokeMode())
            {
                // OB 회피 및 최적 위치 찾기
               // OptimalPosition = FindOptimalAimActorPosition(BallLocation, RotatedDirection,
              //      FVector::Dist(BallLocation, TargetPosition));
              // 
                // OBline 체크안하고 그냥 회전한 거리체크
                OptimalPosition = BallLocation + (RotatedDirection * FVector::Dist(BallLocation, TargetPosition));
            }
            else
            {
                // OBline 체크안하고 그냥 회전한 거리체크
                OptimalPosition = BallLocation + (RotatedDirection * FVector::Dist(BallLocation, TargetPosition));
            }

            OptimalPosition = AdjustToTerrainHeight(OptimalPosition);
            float DistHoletoAim = FVector::Dist(OptimalPosition, HolecupLocation);

            if (DistHoletoAim > 50.0f)
            {
                UE_LOG(LogTemp, Log, TEXT("🎯 Aim to holecup disMatching  Dist-  %f "), DistHoletoAim);
                AimActor->SetAimVisibility(true);
                AimActor->SetAimLocation(OptimalPosition);
                // 거리에 따라 스케일 조절
                float DistanceToAim = FVector::Dist(BallLocation, OptimalPosition);
                AimActor->SetScaleByDistance(DistanceToAim);
            }
            else
            {
                UE_LOG(LogTemp, Log, TEXT("🎯 Aim to holecup Matching  Dist-  %f "), DistHoletoAim);
                AimActor->SetAimVisibility(false);
                AimActor->SetActorLocation(OptimalPosition);
            }

        }
        else
        {
            if (CachedGameMode->IsStrokeMode())
            {
                // OB 회피 및 최적 위치 찾기
            /*    OptimalPosition = FindOptimalAimActorPosition(BallLocation, RotatedDirection,
                    FVector::Dist(BallLocation, TargetPosition));*/

                    // OBline 체크안하고 그냥 회전한 거리체크
                OptimalPosition = BallLocation + (RotatedDirection * FVector::Dist(BallLocation, TargetPosition));
            }
            else
            {
                // OBline 체크안하고 그냥 회전한 거리체크
                OptimalPosition = BallLocation + (RotatedDirection * FVector::Dist(BallLocation, TargetPosition));
            }

            OptimalPosition = AdjustToTerrainHeight(OptimalPosition);
            AimActor->SetAimVisibility(true);
            AimActor->SetAimLocation(OptimalPosition);
            float DistanceToAim = FVector::Dist(BallLocation, OptimalPosition);
            AimActor->SetScaleByDistance(DistanceToAim);
        }

    }


    UE_LOG(LogTemp, Log, TEXT("------  UpdateAimActorByRotation()::SetActorLocation() - %s"), *AimActor->GetActorLocation().ToString());
    // GameMode 동기화
    CachedGameMode->AimLocation = OptimalPosition;


    UE_LOG(LogTemp, Log, TEXT("🎯 AimActor moved to: %s"), *OptimalPosition.ToString());
}

// =============================================================================
// UpdateMiniMapForRotation - 회전 시 미니맵 전체 업데이트
// =============================================================================
void AGolfPlayerController::UpdateMiniMapForRotation()
{
    if (!CachedGameMode || !CachedGameMode->MiniMapWidget || !AimActor)
        return;

    int32 CurrentPlayerIndex = CachedGameMode->CurrentPlayerIndex;
    FVector AimLocation = AimActor->GetActorLocation();

    UE_LOG(LogTemp, Log, TEXT("🗺️ Updating minimap for player %d"), CurrentPlayerIndex);

    // ⭐⭐⭐ 1. Aim 방향 업데이트
    CachedGameMode->MiniMapWidget->UpdateAimDirection(CurrentPlayerIndex, AimDirection);

    // ⭐⭐⭐ 2. AimActor 위치 업데이트
    CachedGameMode->MiniMapWidget->UpdateAimActorPosition(CurrentPlayerIndex, AimLocation);

    // ⭐⭐⭐ 3. Aim 라인 업데이트
    CachedGameMode->MiniMapWidget->UpdateAimLinePosition(CurrentPlayerIndex);

    // ⭐⭐⭐ 4. Ball → Aim 라인 업데이트
    CachedGameMode->MiniMapWidget->UpdateBallToAimLinePosition(CurrentPlayerIndex);

    // ⭐⭐⭐ 5. Tip 업데이트 (있다면)
    CachedGameMode->MiniMapWidget->UpdateTip2();

    // ⭐⭐⭐ 6. 강제 새로고침 (위젯 레이아웃 갱신)
    CachedGameMode->MiniMapWidget->ForceLayoutPrepass();

    UE_LOG(LogTemp, Log, TEXT("✅ Minimap updated: Direction=%s, Position=%s"),
        *AimDirection.ToString(), *AimLocation.ToString());
}


// =============================================================================
// PositionCameraForAim - AimActor 기준으로 카메라 배치 (새 함수)
// =============================================================================
void AGolfPlayerController::PositionCameraForAim()
{
    if (!AimActor || !CameraManager || !CachedGameMode || !CachedGameMode->PlayerManager)
        return;

    TArray<AGolfBall*> PlayerBalls = CachedGameMode->PlayerManager->GetPlayerBalls();
    if (!PlayerBalls.IsValidIndex(CachedGameMode->CurrentPlayerIndex))
        return;

    AGolfBall* CurrentBall = PlayerBalls[CachedGameMode->CurrentPlayerIndex];
    if (!CurrentBall)
        return;

    FVector BallLocation = CurrentBall->GetActorLocation();
    FVector AimLocation = AimActor->GetActorLocation();
    AimLocation.Z = BallLocation.Z;

    // ⭐⭐⭐ Getter 함수 사용으로 변경
    if (CameraManager->IsInReadyMode())
    {
        CameraManager->PositionCameraForAimView(BallLocation, AimLocation);
        UE_LOG(LogTemp, Log, TEXT("📷 Camera repositioned for aim change"));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("📷 Camera repositioning skipped (not in Ready mode)"));
    }

    // TerrainGrid 업데이트
    if (TerrainGrid && bTerrainGridVisible)
    {
        TerrainGrid->SetTargetPosition(AimLocation);
        UpdateTerrainGridPosition();
    }
}



void AGolfPlayerController::AdjustPower(float Value)
{
    if (Value != 0.0f)
    {
        UpdatePower(Value * 0.5f); // 0.5 m/s 단위로 조절
        UE_LOG(LogTemp, Log, TEXT("AdjustPower: Value=%f"), Value);
    }
}

void AGolfPlayerController::OnShot()
{

    // ⭐ 추가: 턴 전환 중이면 샷 무시
    if (IsAnyBallInTurnTransition())
    {
        if (GEngine)
        {
            float RemainingTime = GetCurrentBallTurnTransitionTime();
            if (RemainingTime > 0.0f)
            {
                GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow,
                    FString::Printf(TEXT("⏳ Please wait %.1f seconds"), RemainingTime));
            }
            else
            {
                GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow,
                    TEXT("⏳ Another player's turn is transitioning"));
            }
        }
        return;
    }

    // ⭐ 캐시된 GameMode 사용
    if (CachedGameMode && CachedPlayerManager) // CachedPlayerManager도 함께 유효성 검사
    {
        if (CachedPlayerManager->GetPlayerBalls().IsValidIndex(CachedGameMode->CurrentPlayerIndex))
        {
            AGolfBall* Ball = CachedPlayerManager->GetPlayerBalls()[CachedGameMode->CurrentPlayerIndex];
            if (Ball && Ball->GetBallState() != EBallState::Ball_Ready)
            {
                UE_LOG(LogTemp, Warning, TEXT("Cannot shoot: Ball state is %s, PlayerIndex=%d"),
                    *UEnum::GetValueAsString(Ball->GetBallState()), CachedGameMode->CurrentPlayerIndex);
                return;
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("CachedGameMode or CachedPlayerManager is null in OnShot!"));
        return;
    }


    // UI가 열려있으면 UI를 통해서만 샷 실행
    if (IsShotControlUIOpen())
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow,
                TEXT("Use Shot Control UI to execute shot"));
        }
        return;
    }

    // 기존 샷 실행 로직
    if (!CanExecuteShot())
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red,
                TEXT("Cannot execute shot: Check ball state"));
        }
        return;
    }

    ExecuteShot();

    if (CachedGameMode) // ⭐ 캐시된 GameMode 사용
    {
        UE_LOG(LogTemp, Log, TEXT("Shot action triggered, PlayerIndex=%d"), CachedGameMode->CurrentPlayerIndex);
    }
}

void AGolfPlayerController::RotateLeft()
{
    UE_LOG(LogTemp, Warning, TEXT("AGolfPlayerController::RotateLeft()"));
    MoveAimHorizontal(-0.5f);
}

void AGolfPlayerController::RotateRight()
{
    UE_LOG(LogTemp, Warning, TEXT("AGolfPlayerController::RotateRight()"));
    MoveAimHorizontal(0.5f);
}

void AGolfPlayerController::CameraSetView()
{
    UE_LOG(LogTemp, Warning, TEXT("AGolfPlayerController::CameraSetView()"));

    // CameraManager를 뷰 타겟으로 설정
    if (CameraManager)
    {
        FViewTargetTransitionParams TransitionParams;
        TransitionParams.BlendTime = 0.5f;
        SetViewTarget(CameraManager, TransitionParams);
        UE_LOG(LogTemp, Log, TEXT("AGolfPlayerController::BeginPlay() CameraManager set as view target: %s"), *CameraManager->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("AGolfPlayerController::BeginPlay() CameraManager is null in BeginPlay"));
    }
}

void AGolfPlayerController::UpdateAim(float DeltaX)
{
    // 홀컵 방향 기준으로 Yaw 회전
    FRotator Rot = AimDirection.Rotation();
    Rot.Yaw += DeltaX;
    AimDirection = Rot.Vector().GetSafeNormal();



    // AimActor 위치 업데이트
    UpdateAimActorPosition();

    // 미니맵에 에임 방향과 AimActor 위치 업데이트
    UpdateMiniMapAim();

    // GameMode AimLocation 동기화
    if (CachedGameMode && AimActor)
    {
        CachedGameMode->AimLocation = AimActor->GetActorLocation();

    }


    if (CachedGameMode)
    {
        UE_LOG(LogTemp, Log, TEXT("UpdateAim: AimDirection=%s, CurrentPlayerIndex=%d, DeltaX=%f"),
            *AimDirection.ToString(), CachedGameMode->CurrentPlayerIndex, DeltaX);
    }
}

void AGolfPlayerController::UpdatePower(float DeltaY)
{
    ShotPower = FMath::Clamp(ShotPower + DeltaY, 2.0f, 50.0f); // m/s 단위

    // 미니맵 정보 업데이트 ⭐ 추가
    UpdateMiniMapInfo();
    if (CachedGameMode) // ⭐ 캐시된 GameMode 사용
    {
        UE_LOG(LogTemp, Log, TEXT("UpdatePower: ShotPower=%f m/s, CurrentPlayerIndex=%d"), ShotPower, CachedGameMode->CurrentPlayerIndex);
    }
}

void AGolfPlayerController::ExecuteShot()
{
    // 기존 ExecuteShot 로직 전에 현재 샷 정보 저장
    SaveCurrentShotInfo();

    // 기존 ExecuteShot 로직은 그대로 유지...
    if (!CanExecuteShot())
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot execute shot: Preconditions not met"));
        return;
    }

    if (!CachedGameMode)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to get GameMode for shot execution (CachedGameMode is null)"));
        return;
    }

    AGolfPlayer* ShotPlayer = GetCurrentGolfPlayer();
    if (!ShotPlayer)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to get current golf player for index: %d"), CachedGameMode->CurrentPlayerIndex);
        return;
    }

    bShotInProgress = true;
    FVector CalculatedAimDirection = CalculateAimDirection();
    LogShotInfo(CalculatedAimDirection);

    bool bShotSuccess = ExecuteShotInternal(ShotPlayer, CalculatedAimDirection, CachedGameMode->CurrentPlayerIndex);

    if (bShotSuccess)
    {
        AimActor->SetAimVisibility(false);
        HandleSuccessfulShot(CalculatedAimDirection);
    }
    else
    {
        HandleFailedShot();
    }
}


FVector AGolfPlayerController::CalculateAimDirection() const
{
    FVector CalculatedDirection = AimDirection; // 기본값

    if (CameraManager && CameraManager->Camera)
    {
        // 🔧 수정: 카메라의 현재 회전을 기준으로 상대 회전 적용
        FRotator CameraRotation = CameraManager->Camera->GetComponentRotation();

        // 카메라 회전에 샷 각도를 더함
        FRotator FinalRotation = CameraRotation + FRotator(ShotPitchAngle, ShotYawAngle, 0.0f);

        // 최종 회전의 Forward 벡터를 구함
        CalculatedDirection = FinalRotation.Vector();
        CalculatedDirection.Normalize();

        UE_LOG(LogTemp, Log, TEXT("🔧 Fixed AimDirection: CameraRot=%s, ShotAngles=(P:%.2f,Y:%.2f), Final=%s"),
            *CameraRotation.ToString(), ShotPitchAngle, ShotYawAngle, *CalculatedDirection.ToString());
    }

    return CalculatedDirection;
}

void AGolfPlayerController::LogShotInfo(const FVector& Direction) const
{
    FString CameraForwardStr = TEXT("None");
    if (CameraManager && CameraManager->Camera)
    {
        CameraForwardStr = CameraManager->Camera->GetForwardVector().ToString();
    }

    if (CachedGameMode) // ⭐ 캐시된 GameMode 사용
    {
        UE_LOG(LogTemp, Log, TEXT("ExecuteShot: PlayerIndex=%d, AimDirection=%s, ShotPower=%.2f m/s, ShotPitchAngle=%.2f, CameraForward=%s"),
            CachedGameMode->CurrentPlayerIndex,
            *Direction.ToString(),
            ShotPower,
            ShotPitchAngle,
            *CameraForwardStr);
    }
}

bool AGolfPlayerController::ExecuteShotInternal(AGolfPlayer* ShotPlayer, const FVector& Direction, int32 PlayerIndex)
{
    if (!ShotPlayer)
        return false;

    try
    {
        // 샷 준비
        ShotPlayer->PrepareShot(Direction, ShotPower);

        // 샷 실행
        ShotPlayer->ExecuteShot();

        // 성공 로그
        UE_LOG(LogTemp, Log, TEXT("Shot executed successfully for player %d"), PlayerIndex);
        return true;
    }
    catch (...)
    {
        UE_LOG(LogTemp, Error, TEXT("Exception occurred during shot execution"));
        return false;
    }
}

void AGolfPlayerController::HandleSuccessfulShot(const FVector& Direction)
{

    // TerrainGrid 숨기기
    if (TerrainGrid && bTerrainGridVisible)
    {
        HideTerrainGridOnShot();
    }

    // 게임 모드 처리
    if (CachedGameMode)
    {
        // 카메라 모드 변경
        if (CameraManager)
        {
            if (CachedGameMode->CurrentGameMode == EGolfGameMode::StrokeMode)
            {
                CameraManager->ChangeCameraMode(ECameraMode::Flying);
            }
            else if (CachedGameMode->CurrentGameMode == EGolfGameMode::TrainingMode)
            {
                CameraManager->ChangeCameraMode(ECameraMode::Flying);
            }
            else if (CachedGameMode->CurrentGameMode == EGolfGameMode::RangeMode)
            {
                if (!CachedGameMode->GetCurrentTurnGolfBall()->LinkedCameraManager->IsInFixedMode())
                    CameraManager->ChangeCameraMode(ECameraMode::Flying);
            }
        }



        // 볼 정보 로깅
        LogBallInfo(CachedGameMode);

        // 현재 플레이어의 미니맵 에임 라인 지연 제거
        int32 CurrentPlayerIndex = CachedGameMode->CurrentPlayerIndex;
        ScheduleAimLineClear(CachedGameMode, CurrentPlayerIndex);
    }

    // 샷 진행 상태 업데이트
    GetWorld()->GetTimerManager().SetTimer(
        AimLineClearTimer,
        [this]() { bShotInProgress = false; },
        SHOT_FEEDBACK_DURATION,
        false
    );
}


// ⭐ 새로 추가: 샷 시 TerrainGrid 숨기기 전용 함수
void AGolfPlayerController::HideTerrainGridOnShot()
{
    if (!TerrainGrid)
        return;

    // 격자가 표시 중이었다면 상태 저장 후 숨김
    if (bTerrainGridVisible)
    {
        TerrainGrid->SetGridVisible(false);
        bTerrainGridVisible = false;

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow,
                TEXT("🌍 Terrain Grid hidden for shot"));
        }

        UE_LOG(LogTemp, Log, TEXT("🏌️ TerrainGrid hidden due to shot execution"));
    }
}

void AGolfPlayerController::HandleFailedShot()
{
    // ⭐ 캐시된 GameMode 사용 (GetWorld()->GetAuthGameMode() 대신)
    int32 PlayerIndexForLog = -1;
    if (CachedGameMode)
    {
        PlayerIndexForLog = CachedGameMode->CurrentPlayerIndex;
    }
    UE_LOG(LogTemp, Error, TEXT("Shot execution failed for player %d"), PlayerIndexForLog);

    // 샷 진행 상태 즉시 리셋
    bShotInProgress = false;

    // 실패 시 UI 피드백 (선택사항)
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red,
            TEXT("Shot failed! Please try again."));
    }
}

void AGolfPlayerController::LogBallInfo(AInGameMode* GameMode) const
{
    if (!GameMode || !GameMode->PlayerManager)
        return; // Early exit if GameMode or PlayerManager is null

    TArray<AGolfBall*> PlayerBalls = GameMode->PlayerManager->GetPlayerBalls();
    if (PlayerBalls.IsValidIndex(GameMode->CurrentPlayerIndex)) // GameMode->CurrentPlayerIndex 사용
    {
        AGolfBall* Ball = PlayerBalls[GameMode->CurrentPlayerIndex];
        if (Ball)
        {
            // ===== 수정된 부분: BallMesh를 통한 속도 확인 =====
            FVector BallLocation = Ball->GetActorLocation();
            FVector BallVelocity = Ball->GetBallVelocity(); // 새로운 함수 사용
            float BallSpeed = Ball->GetBallSpeed(); // 새로운 함수 사용

            UE_LOG(LogTemp, Log, TEXT("Ball Info - Location: %s, Velocity: %s (Speed: %.1f cm/s), FrictionWeight: %.2f"),
                *BallLocation.ToString(),
                *BallVelocity.ToString(), // Updated log format
                BallSpeed,
                Ball->FrictionWeight);
        }
    }
}


void AGolfPlayerController::ScheduleAimLineClear(AInGameMode* GameMode, int32 PlayerIndex)
{
    if (!GameMode || !GameMode->MiniMapWidget)
        return;

    // 특정 플레이어의 에임 라인만 제거하도록 수정
    GetWorld()->GetTimerManager().SetTimer(
        AimLineClearTimer,
        [GameMode, PlayerIndex]()
        {
            if (GameMode && GameMode->MiniMapWidget)
            {
                // 특정 플레이어의 에임 방향을 0으로 설정 (라인 숨기기)
                GameMode->MiniMapWidget->UpdateAimDirection(PlayerIndex, FVector::ZeroVector);
                // 해당 플레이어의 모든 에임 관련 요소 숨기기
                GameMode->MiniMapWidget->HidePlayerElements(PlayerIndex);
            }
        },
        AIM_LINE_CLEAR_DELAY,
        false
    );
}
AGolfPlayer* AGolfPlayerController::GetCurrentGolfPlayer() const
{
    if (CachedGameMode) // ⭐ 캐시된 GameMode 사용
    { // Cast to InGameMode and check for validity
        UE_LOG(LogTemp, Warning, TEXT("AGolfPlayerController::GetCurrentGolfPlayer() - CurrentPlayerindex - [%d]"), CachedGameMode->CurrentPlayerIndex); // CachedGameMode의 CurrentPlayerIndex 사용
        return CachedGameMode->PlayerManager->GetPlayers()[CachedGameMode->CurrentPlayerIndex]; // CachedGameMode의 CurrentPlayerIndex 사용
    }
    return nullptr;
}

void AGolfPlayerController::DebugInputStatus()
{
    //  UE_LOG(LogTemp, Log, TEXT("InputEnabled: %s"), IsInputEnabled() ? TEXT("True") : TEXT("False"));
    if (InputComponent)
    {
        UE_LOG(LogTemp, Log, TEXT("InputComponent is valid"));
        float RotateLeftValue = GetInputAxisValue("RotateLeft");
        float RotateRightValue = GetInputAxisValue("RotateRight");
        UE_LOG(LogTemp, Log, TEXT("RotateLeft: %f, RotateRight: %f"), RotateLeftValue, RotateRightValue);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("InputComponent is null"));
    }
}


void AGolfPlayerController::IncreaseFriction()
{
    if (CachedGameMode) // ⭐ 캐시된 GameMode 사용
    {
        if (CachedGameMode->PlayerManager->GetPlayerBalls().IsValidIndex(CachedGameMode->CurrentPlayerIndex))
        {
            AGolfBall* Ball = CachedGameMode->PlayerManager->GetPlayerBalls()[CachedGameMode->CurrentPlayerIndex];
            Ball->SetFrictionWeight(Ball->FrictionWeight + 0.1f);
        }
    }
}

void AGolfPlayerController::DecreaseFriction()
{
    if (CachedGameMode) // ⭐ 캐시된 GameMode 사용
    {
        if (CachedGameMode->PlayerManager->GetPlayerBalls().IsValidIndex(CachedGameMode->CurrentPlayerIndex))
        {
            AGolfBall* Ball = CachedGameMode->PlayerManager->GetPlayerBalls()[CachedGameMode->CurrentPlayerIndex];
            Ball->SetFrictionWeight(Ball->FrictionWeight - 0.1f);
        }
    }
}


void AGolfPlayerController::UpdateMiniMapDistance()
{
    // ⭐ 캐시된 GameMode 사용
    if (!CachedGameMode || !IsValid(CachedGameMode->MiniMapWidget) || !IsValid(CachedGameMode->PlayerManager))
    {
        return;
    }

    TArray<AGolfBall*> PlayerBalls = CachedGameMode->PlayerManager->GetPlayerBalls();
    if (!PlayerBalls.IsValidIndex(CachedGameMode->CurrentPlayerIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Invalid CurrentPlayerIndex: %d"), CachedGameMode->CurrentPlayerIndex);
        return;
    }

    AGolfBall* CurrentBall = PlayerBalls[CachedGameMode->CurrentPlayerIndex];
    if (!IsValid(CurrentBall))
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Current ball not valid"));
        return;
    }

    // 홀컵 위치 유효성 검사
    if (!CachedGameMode->MapInfo.HolecupPositions.IsValidIndex(CachedGameMode->CurrentHole - 1))
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Invalid hole index: %d"), CachedGameMode->CurrentHole);
        return;
    }

    FVector BallPos = CurrentBall->GetActorLocation();
    FVector HolePos = CachedGameMode->MapInfo.HolecupPositions[CachedGameMode->CurrentHole - 1];

    // 거리와 고도차 계산
    float Distance = FVector::Dist(BallPos, HolePos);
    float Elevation = HolePos.Z - BallPos.Z;

    // 미니맵 업데이트
    CachedGameMode->MiniMapWidget->UpdateDistanceAndElevation(Distance, Elevation);

    UE_LOG(LogTemp, VeryVerbose, TEXT("📏 Distance: %.1fcm, Elevation: %.1fcm"), Distance, Elevation);
}
void AGolfPlayerController::UpdateMiniMapInfo()
{
    if (!CachedGameMode)
        return;

    if (CachedGameMode->IsTrainingMode())
    {
        UpdateMiniMapInfo_TrainingMode();
    }
    else
    {
        // 기존 Stroke Mode 처리
        UpdateMiniMapAim();
        UpdateMiniMapDistance();
    }
}


// 샷 조절 UI 관련 새로운 함수들 ⭐ 추가
void AGolfPlayerController::ShowShotControlUI(bool bShow)
{
    if (ShotControlWidget)
    {
        ShotControlWidget->ShowShotControl(bShow);

        if (bShow)
        {
            // UI 표시 시 현재 값들로 위젯 업데이트
            ShotControlWidget->SetShotPower(ShotPower);
            ShotControlWidget->SetShotAngle(ShotPitchAngle);

            // 마우스 커서 표시
          //  bShowMouseCursor = true;
            SetInputMode(FInputModeGameAndUI());
        }
        else
        {
            // UI 숨김 시 게임 모드로 복귀
           // bShowMouseCursor = false;
            SetInputMode(FInputModeGameOnly());
        }
    }
}

void AGolfPlayerController::OpenShotControlUI()
{
    // 샷 가능한 상태인지 확인
    if (!CanExecuteShot())
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
                TEXT("Cannot open shot control: Ball not ready"));
        }
        return;
    }

    ShowShotControlUI(true);
}

void AGolfPlayerController::CloseShotControlUI()
{
    ShowShotControlUI(false);
}

void AGolfPlayerController::AdjustShotPower(float Delta)
{
    float NewPower = FMath::Clamp(ShotPower + Delta, 5.0f, 50.0f);
    ShotPower = NewPower;

    // UI 업데이트
    if (ShotControlWidget)
    {
        ShotControlWidget->SetShotPower(ShotPower);
    }

    // 화면에 현재 값 표시
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(1, 1.0f, FColor::Green,
            FString::Printf(TEXT("Shot Power: %.1f m/s"), ShotPower));
    }
}

void AGolfPlayerController::AdjustShotAngle(float Delta)
{
    float NewAngle = FMath::Clamp(ShotPitchAngle + Delta, 5.0f, 45.0f);
    ShotPitchAngle = NewAngle;

    // UI 업데이트
    if (ShotControlWidget)
    {
        ShotControlWidget->SetShotAngle(ShotPitchAngle);
    }

    // 화면에 현재 값 표시
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(2, 1.0f, FColor::Blue,
            FString::Printf(TEXT("Launch Angle: %.1f°"), ShotPitchAngle));
    }
}

// 입력 핸들러들 ⭐ 추가
void AGolfPlayerController::OnOpenShotControl()
{
    if (IsShotControlUIOpen())
    {
        CloseShotControlUI();
    }
    else
    {
        OpenShotControlUI();
    }
}

void AGolfPlayerController::OnPowerUp()
{
    AdjustShotPower(PowerAdjustStep);
}

void AGolfPlayerController::OnPowerDown()
{
    AdjustShotPower(-PowerAdjustStep);
}

void AGolfPlayerController::OnAngleUp()
{
    AdjustShotAngle(AngleAdjustStep);
}

void AGolfPlayerController::OnAngleDown()
{
    AdjustShotAngle(-AngleAdjustStep);
}

void AGolfPlayerController::OnBounceFix()
{
    if (AInGameMode* GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
    {
        AGolfBall* Ball = GM->GetCurrentTurnGolfBall();
        Ball->SetBounceFix(!Ball->GetBounceFix());

        if (GEngine)
        {
            FString BounceFixStr = Ball->GetBounceFix() ? "ON" : "OFF";
            GEngine->AddOnScreenDebugMessage(
                -1,
                5.0f,
                FColor::Purple,
                FString::Printf(TEXT("BallBounce Fix : %s"), *BounceFixStr)
            );
        }
    }
}

void AGolfPlayerController::CreateShotControlWidget()
{
    UE_LOG(LogTemp, Warning, TEXT("=== CreateShotControlWidget Started ==="));

    // 1차 검증: 클래스 유효성 상세 확인
    // ⭐ 캐시된 GameMode 사용
    if (!ShotControlWidgetClass)
    {
        UE_LOG(LogTemp, Error, TEXT("🔴 ShotControlWidgetClass is NULL!"));

        if (CachedGameMode) // ⭐ 캐시된 GameMode 사용
        {
            UE_LOG(LogTemp, Warning, TEXT("Trying to get widget class from GameMode..."));

            if (CachedGameMode->DefaultShotControlWidget)
            {
                ShotControlWidgetClass = CachedGameMode->DefaultShotControlWidget;
                UE_LOG(LogTemp, Warning, TEXT("✅ Got widget class from GameMode: %s"),
                    *ShotControlWidgetClass->GetName());
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("🔴 GameMode DefaultShotControlWidget is also NULL!"));
                ShowWidgetSetupInstructions();
                return;
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("🔴 CachedGameMode is null or wrong type!"));
            return;
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("✅ ShotControlWidgetClass is valid: %s"),
            *ShotControlWidgetClass->GetName());
    }

    // 2차 검증: 위젯 생성
    UE_LOG(LogTemp, Warning, TEXT("Creating widget instance..."));
    ShotControlWidget = CreateWidget<UGolfShotControlWidget>(this, ShotControlWidgetClass);

    if (ShotControlWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("✅ Widget instance created successfully"));

        // 뷰포트에 추가
        ShotControlWidget->AddToViewport(2000);
        ShotControlWidget->ShowShotControl(false);

        UE_LOG(LogTemp, Warning, TEXT("✅ Widget added to viewport and hidden"));

        // 성공 메시지 표시
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
                TEXT("🎉 Shot Control UI Ready!"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("🔴 Failed to create widget instance!"));
        UE_LOG(LogTemp, Error, TEXT("   Widget Class: %s"), *ShotControlWidgetClass->GetName());
        UE_LOG(LogTemp, Error, TEXT("   → Check if widget blueprint is valid"));
        UE_LOG(LogTemp, Error, TEXT("   → Check if widget has correct parent class"));

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Red,
                TEXT("🔴 Widget Creation Failed!"));
            GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Yellow,
                TEXT("Check widget blueprint configuration"));
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("=== CreateShotControlWidget Completed ==="));
}
void AGolfPlayerController::ShowErrorMessage()
{
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red,
            TEXT("🔴 Shot Control UI not available"));
        GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Yellow,
            TEXT("Check GameMode Blueprint: Set DefaultShotControlWidget"));
    }
}

void AGolfPlayerController::UpdateShotControlWidget()
{
    if (ShotControlWidget)
    {
        ShotControlWidget->SetShotPower(ShotPower);
        ShotControlWidget->SetShotAngle(ShotPitchAngle);
    }
}

bool AGolfPlayerController::IsShotControlUIOpen() const
{
    return ShotControlWidget && ShotControlWidget->GetVisibility() == ESlateVisibility::Visible;
}

// 기존 OnAdjustPower 함수 수정 ⭐ 개선
void AGolfPlayerController::OnAdjustPower(float Value)
{
    if (Value != 0.0f)
    {
        // UI가 열려있지 않을 때만 직접 조절
        if (!IsShotControlUIOpen())
        {
            AdjustShotPower(Value * PowerAdjustStep);
        }
    }
}

// 기존 OnAimHorizontal 함수는 유지하되, UI 상태 확인 추가
void AGolfPlayerController::OnAimHorizontal(float Value)
{
    if (Value != 0.0f && !IsShotControlUIOpen()) // UI 열린 상태에서는 에임 조절 비활성화
    {
        UpdateAim(Value * 3.0f);
    }
}


// GolfPlayerController.cpp에 추가할 CanExecuteShot 함수 구현


bool AGolfPlayerController::CanExecuteShot() const
{
    // 1. 이미 샷이 진행 중인지 확인
    if (bShotInProgress)
    {
        UE_LOG(LogTemp, Warning, TEXT("Shot already in progress"));
        return false;
    }

    // 2. 게임 모드 유효성 확인
    if (!CachedGameMode)
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid game mode (CachedGameMode is null)"));
        return false;
    }

    // ⭐ Training Mode에서는 더 관대한 조건 적용
    if (CachedGameMode->IsTrainingMode())
    {
        return CanExecuteShot_TrainingMode();
    }

    // 기존 Stroke Mode 조건들
    if (!CachedGameMode->PlayerManager)
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerManager is null"));
        return false;
    }

    TArray<AGolfBall*> PlayerBalls = CachedGameMode->PlayerManager->GetPlayerBalls();
    if (!PlayerBalls.IsValidIndex(CachedGameMode->CurrentPlayerIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid player ball index: %d"), CachedGameMode->CurrentPlayerIndex);
        return false;
    }

    AGolfBall* CurrentBall = PlayerBalls[CachedGameMode->CurrentPlayerIndex];
    if (!CurrentBall)
    {
        UE_LOG(LogTemp, Error, TEXT("Current ball is null for player index: %d"), CachedGameMode->CurrentPlayerIndex);
        return false;
    }

    if (CurrentBall->IsInTurnTransition())
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot shoot during turn transition (%.1f seconds remaining)"),
            CurrentBall->GetTurnTransitionCountdown());
        return false;
    }

    for (int32 i = 0; i < PlayerBalls.Num(); i++)
    {
        if (i != CachedGameMode->CurrentPlayerIndex && PlayerBalls[i] && PlayerBalls[i]->IsInTurnTransition())
        {
            UE_LOG(LogTemp, Warning, TEXT("Cannot shoot while another player's ball is in turn transition"));
            return false;
        }
    }

    EBallState BallState = CurrentBall->GetBallState();
    if (BallState != EBallState::Ball_Ready)
    {
        UE_LOG(LogTemp, Warning, TEXT("Ball is not ready for shot. Current state: %s"),
            *UEnum::GetValueAsString(BallState));
        return false;
    }

    if (ShotPower < 0.0f || ShotPower > 60.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid shot power: %.2f (should be 5.0-50.0)"), ShotPower);
        return false;
    }

    if (ShotPitchAngle < 0.0f || ShotPitchAngle > 45.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid shot angle: %.2f (should be 5.0-45.0)"), ShotPitchAngle);
        return false;
    }

    return true;
}

// 추가: 샷 실행 가능 상태를 간단히 확인하는 헬퍼 함수
bool AGolfPlayerController::IsReadyForShot() const
{
    return CanExecuteShot();
}



void AGolfPlayerController::ShowWidgetSetupInstructions()
{
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red,
            TEXT("🔴 Shot Control Widget not configured!"));
        GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow,
            TEXT("1. Create Widget Blueprint (WBP_ShotControl)"));
        GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow,
            TEXT("2. Set Parent Class to 'Golf Shot Control Widget'"));
        GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow,
            TEXT("3. GameMode BP → Class Defaults → Set Default Shot Control Widget"));
    }
}

// GolfPlayerController.cpp에 추가할 구현부
void AGolfPlayerController::LogCurrentBallPhysics() const
{
    if (CachedGameMode) // ⭐ 캐시된 GameMode 사용
    {
        TArray<AGolfBall*> PlayerBalls = CachedGameMode->PlayerManager->GetPlayerBalls();
        if (PlayerBalls.IsValidIndex(CachedGameMode->CurrentPlayerIndex))
        {
            AGolfBall* Ball = PlayerBalls[CachedGameMode->CurrentPlayerIndex];
            if (Ball)
            {
                FVector Velocity = Ball->GetBallVelocity();
                float Speed = Ball->GetBallSpeed();
                EBallState State = Ball->GetBallState();
                FVector Location = Ball->GetActorLocation();

                UE_LOG(LogTemp, Warning, TEXT("=== Ball Physics Debug ==="));
                UE_LOG(LogTemp, Warning, TEXT("Player Index: %d"), CachedGameMode->CurrentPlayerIndex);
                UE_LOG(LogTemp, Warning, TEXT("Ball State: %s"), *UEnum::GetValueAsString(State));
                UE_LOG(LogTemp, Warning, TEXT("Location: %s"), *Location.ToString());
                UE_LOG(LogTemp, Warning, TEXT("Velocity: %s"), *Velocity.ToString());
                UE_LOG(LogTemp, Warning, TEXT("Speed: %.2f cm/s (%.2f m/s)"), Speed, Speed / 100.0f);
                UE_LOG(LogTemp, Warning, TEXT("Friction Weight: %.2f"), Ball->FrictionWeight);
                UE_LOG(LogTemp, Warning, TEXT("Physics Enabled: %s"),
                    Ball->BallMesh->IsSimulatingPhysics() ? TEXT("True") : TEXT("False"));
                UE_LOG(LogTemp, Warning, TEXT("Gravity Enabled: %s"),
                    Ball->BallMesh->IsGravityEnabled() ? TEXT("True") : TEXT("False"));
                UE_LOG(LogTemp, Warning, TEXT("========================"));

                // 화면에도 표시
                if (GEngine)
                {
                    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan,
                        FString::Printf(TEXT("Ball State: %s, Speed: %.1f cm/s"),
                            *UEnum::GetValueAsString(State), Speed));
                }
            }
        }
    }
}

void AGolfPlayerController::ToggleBallPhysicsDebug()
{

    // ⭐ 캐시된 GameMode 사용
    if (CachedGameMode && CachedGameMode->PlayerManager && CachedGameMode->PlayerManager->GetPlayerBalls().IsValidIndex(CachedGameMode->CurrentPlayerIndex))
    {
        AGolfBall* GolfBall = CachedGameMode->PlayerManager->GetPlayerBalls()[CachedGameMode->CurrentPlayerIndex];
        if (GolfBall)
        {
            GolfBall->SetBallCollisionEnabled(true);
        }
    }


    bShowPhysicsDebug = !bShowPhysicsDebug;

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
            FString::Printf(TEXT("Physics Debug: %s"),
                bShowPhysicsDebug ? TEXT("ON") : TEXT("OFF")));
    }

    UE_LOG(LogTemp, Log, TEXT("Ball physics debug toggled: %s"),
        bShowPhysicsDebug ? TEXT("ON") : TEXT("OFF"));


}

bool AGolfPlayerController::IsBallMoving() const
{
    if (CachedGameMode) // ⭐ 캐시된 GameMode 사용
    {
        TArray<AGolfBall*> PlayerBalls = CachedGameMode->PlayerManager->GetPlayerBalls();
        if (PlayerBalls.IsValidIndex(CachedGameMode->CurrentPlayerIndex))
        {
            AGolfBall* Ball = PlayerBalls[CachedGameMode->CurrentPlayerIndex];
            if (Ball)
            {
                return Ball->GetBallSpeed() > 1.0f; // 1 cm/s 이상이면 움직이는 것으로 간주
            }
        }
    }
    return false;
}

void AGolfPlayerController::ChangeDifficulty(int32 DifficultyLevel)
{
    // ⭐ 캐시된 GameMode 사용
    if (!CachedGameMode || !CachedGameMode->PlayerManager || !CachedGameMode->PlayerManager->GetPlayerBalls().IsValidIndex(CachedGameMode->CurrentPlayerIndex))
    {
        return;
    }
    AGolfBall* GolfBall = CachedGameMode->PlayerManager->GetPlayerBalls()[CachedGameMode->CurrentPlayerIndex];
    if (!GolfBall) return;

    FString ConfigPath;

    switch (DifficultyLevel)
    {
    case 1: // Easy
        ConfigPath = FPaths::ProjectContentDir() + TEXT("Configs/EasyMode.json");
        break;
    case 2: // Normal
        ConfigPath = FPaths::ProjectContentDir() + TEXT("Configs/NormalMode.json");
        break;
    case 3: // Hard
        ConfigPath = FPaths::ProjectContentDir() + TEXT("Configs/HardMode.json");
        break;
    default:
        ConfigPath = TEXT(""); // 기본 설정
        break;
    }

    if (GolfBall->LoadPhysicsConfigFromFile(ConfigPath))
    {
        UE_LOG(LogTemp, Log, TEXT("🎯 Difficulty changed to level %d"), DifficultyLevel);

        // UI에 변경사항 표시
        if (GetPawn())
        {
            FString Message = FString::Printf(TEXT("난이도 변경: %d단계"), DifficultyLevel);
            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, Message);
        }
    }
}



// 디버그 함수들 구현
void AGolfPlayerController::ToggleLandscapeDebug()
{
    if (CachedGameMode) // ⭐ 캐시된 GameMode 사용
    {
        if (CachedGameMode->LandscapeChecker)
        {
            CachedGameMode->LandscapeChecker->ToggleDebugMode();
        }
    }
}

void AGolfPlayerController::ShowCurrentLandType()
{
    if (CachedGameMode) // ⭐ 캐시된 GameMode 사용
    {
        TArray<AGolfBall*> PlayerBalls = CachedGameMode->PlayerManager->GetPlayerBalls();
        if (PlayerBalls.IsValidIndex(CachedGameMode->CurrentPlayerIndex))
        {
            AGolfBall* CurrentBall = PlayerBalls[CachedGameMode->CurrentPlayerIndex];
            if (CurrentBall && CachedGameMode->LandscapeChecker)
            {
                FVector BallLocation = CurrentBall->GetActorLocation();
                CachedGameMode->LandscapeChecker->ShowLandTypeAtLocation(BallLocation, 5.0f);
            }
        }
    }
}

void AGolfPlayerController::ShowLandTypeGrid()
{
    if (CachedGameMode) // ⭐ 캐시된 GameMode 사용
    {
        TArray<AGolfBall*> PlayerBalls = CachedGameMode->PlayerManager->GetPlayerBalls();
        if (PlayerBalls.IsValidIndex(CachedGameMode->CurrentPlayerIndex))
        {
            AGolfBall* CurrentBall = PlayerBalls[CachedGameMode->CurrentPlayerIndex];
            if (CurrentBall && CachedGameMode->LandscapeChecker)
            {
                FVector BallLocation = CurrentBall->GetActorLocation();
                CachedGameMode->LandscapeChecker->DrawDebugLandGrid(BallLocation, 1000.0f, 20);
            }
        }
    }
}

// 새로운 함수 추가: 샷 불가능한 이유를 반환하는 함수 업데이트
FString AGolfPlayerController::GetShotBlockReason() const
{
    if (bShotInProgress)
        return TEXT("Shot already in progress");

    // ⭐ 캐시된 GameMode 사용
    if (!CachedGameMode)
        return TEXT("Invalid game mode (CachedGameMode is null)");

    if (!CachedGameMode->PlayerManager)
        return TEXT("PlayerManager is null");

    TArray<AGolfBall*> PlayerBalls = CachedGameMode->PlayerManager->GetPlayerBalls();
    if (!PlayerBalls.IsValidIndex(CachedGameMode->CurrentPlayerIndex))
        return FString::Printf(TEXT("Invalid player index: %d"), CachedGameMode->CurrentPlayerIndex);

    AGolfBall* CurrentBall = PlayerBalls[CachedGameMode->CurrentPlayerIndex];
    if (!CurrentBall)
        return TEXT("Current ball is null");

    // ⭐ 6. 새로 추가: 턴 전환 대기 중인지 확인
    if (CurrentBall->IsInTurnTransition())
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot shoot during turn transition (%.1f seconds remaining)"),
            CurrentBall->GetTurnTransitionCountdown());
        return FString::Printf(TEXT("IsInTurnTransition player index: %d"), CachedGameMode->CurrentPlayerIndex);
    }

    // ⭐ 7. 새로 추가: 다른 플레이어 볼이 턴 전환 대기 중인지 확인
    for (int32 i = 0; i < PlayerBalls.Num(); i++)
    {
        if (i != CachedGameMode->CurrentPlayerIndex && PlayerBalls[i] && PlayerBalls[i]->IsInTurnTransition())
        {
            return FString::Printf(TEXT("Player %d turn transition in progress"), i);
        }
    }

    EBallState BallState = CurrentBall->GetBallState();
    if (BallState != EBallState::Ball_Ready)
        return FString::Printf(TEXT("Ball not ready: %s"), *UEnum::GetValueAsString(BallState));

    EGameState GameState = CachedGameMode->GetCurrentGameState();
    if (GameState != EGameState::Game_Play)
        return FString::Printf(TEXT("Game not in play: %s"), *UEnum::GetValueAsString(GameState));

    if (CachedGameMode->CurrentPlayerIndex != CachedGameMode->CurrentPlayerIndex)
        return FString::Printf(TEXT("Not your turn (Current: %d)"), CachedGameMode->CurrentPlayerIndex);

    if (ShotPower < 5.0f || ShotPower > 50.0f)
        return FString::Printf(TEXT("Invalid power: %.2f"), ShotPower);

    if (ShotPitchAngle < 5.0f || ShotPitchAngle > 45.0f)
        return FString::Printf(TEXT("Invalid angle: %.2f"), ShotPitchAngle);

    return TEXT("Ready for shot");
}

// 새로운 함수 추가: 턴 전환 카운트다운 스킵 (디버그/관리자 기능)
void AGolfPlayerController::SkipTurnTransition()
{
    if (CachedGameMode) // ⭐ 캐시된 GameMode 사용
    {
        TArray<AGolfBall*> PlayerBalls = CachedGameMode->PlayerManager->GetPlayerBalls();

        // 모든 볼의 턴 전환 카운트다운 스킵
        for (AGolfBall* Ball : PlayerBalls)
        {
            if (Ball && Ball->IsInTurnTransition())
            {
                Ball->SkipTurnTransitionCountdown();
                UE_LOG(LogTemp, Log, TEXT("⏭️ Skipped turn transition for ball"));
            }
        }

        // 화면에 알림 표시
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow,
                TEXT("⏭️ Turn transitions skipped"));
        }
    }
}

// 1. 모든 볼의 턴 전환 상태 확인
bool AGolfPlayerController::IsAnyBallInTurnTransition() const
{
    if (CachedGameMode && CachedGameMode->PlayerManager)
    {
        // ✅ 최적화: TArray 값 복사 → const ref (복사 비용 0)
        const TArray<AGolfBall*>& PlayerBalls = CachedGameMode->PlayerManager->GetPlayerBalls();

        for (AGolfBall* Ball : PlayerBalls)
        {
            if (Ball && Ball->IsInTurnTransition())
            {
                return true;
            }
        }
    }
    return false;
}

// 2. 현재 플레이어 볼의 턴 전환 시간 반환
float AGolfPlayerController::GetCurrentBallTurnTransitionTime() const
{
    if (CachedGameMode && CachedGameMode->PlayerManager)
    {
        // ✅ 최적화: const ref 사용
        const TArray<AGolfBall*>& PlayerBalls = CachedGameMode->PlayerManager->GetPlayerBalls();

        if (PlayerBalls.IsValidIndex(CachedGameMode->CurrentPlayerIndex))
        {
            AGolfBall* CurrentBall = PlayerBalls[CachedGameMode->CurrentPlayerIndex];
            if (CurrentBall)
            {
                return CurrentBall->GetTurnTransitionCountdown();
            }
        }
    }
    return 0.0f;
}

// 3. 턴 전환 상태 체크 헬퍼 함수
bool AGolfPlayerController::CheckTurnTransitionStatus() const
{
    if (CachedGameMode) // ⭐ 캐시된 GameMode 사용
    {
        if (CachedGameMode->PlayerManager)
        {
            TArray<AGolfBall*> PlayerBalls = CachedGameMode->PlayerManager->GetPlayerBalls();

            // 현재 플레이어 볼 체크
            if (PlayerBalls.IsValidIndex(CachedGameMode->CurrentPlayerIndex))
            {
                AGolfBall* CurrentBall = PlayerBalls[CachedGameMode->CurrentPlayerIndex];
                if (CurrentBall && CurrentBall->IsInTurnTransition())
                {
                    UE_LOG(LogTemp, VeryVerbose, TEXT("Current player ball in turn transition: %.1fs"),
                        CurrentBall->GetTurnTransitionCountdown());
                    return true;
                }
            }

            // 다른 플레이어 볼들 체크
            for (int32 i = 0; i < PlayerBalls.Num(); i++)
            {
                if (i != CachedGameMode->CurrentPlayerIndex && PlayerBalls[i] && PlayerBalls[i]->IsInTurnTransition())
                {
                    UE_LOG(LogTemp, VeryVerbose, TEXT("Player %d ball in turn transition: %.1fs"),
                        i, PlayerBalls[i]->GetTurnTransitionCountdown());
                    return true;
                }
            }
        }
    }
    return false;
}

// 새로 추가할 함수들 구현
void AGolfPlayerController::InitializeTerrainGrid()
{
    UE_LOG(LogTemp, Log, TEXT("🌍 Initializing TerrainGrid..."));

    if (UWorld* World = GetWorld())
    {
        // TerrainHeightGrid 가져오거나 생성
        TerrainGrid = ATerrainHeightGrid::GetOrCreateTerrainGrid(World);

        if (TerrainGrid)
        {
            // 초기 설정 적용
            TerrainGrid->SetGridRadius(TerrainGridRadius);

            // 초기에는 숨김 상태
            bTerrainGridVisible = false;
            TerrainGrid->SetGridVisible(false);

            UE_LOG(LogTemp, Log, TEXT("✅ TerrainGrid initialized successfully"));
        }
        else
        {
            TerrainGrid->WaterFlowSettings.Init();
            TerrainGrid->SetGridRadius(TerrainGridRadius / 100.f); // 3000cm → 30m
            TerrainGrid->SetGridVisible(bTerrainGridVisible);
            UE_LOG(LogTemp, Log, TEXT("[TerrainGrid] Initialized successfully"));

            UE_LOG(LogTemp, Error, TEXT("❌ Failed to initialize TerrainGrid"));
        }
    }
}

void AGolfPlayerController::ToggleTerrainGrid()
{
    if (!TerrainGrid)
    {
        InitializeTerrainGrid();
        if (!TerrainGrid) return;
    }

    if (CachedGameMode->GetCurrentTurnGolfPlayer()->GetPlayerState() != EPlayerState::Player_Ready)
        return;

    bTerrainGridVisible = !bTerrainGridVisible;

    if (bTerrainGridVisible)
    {
        // 격자 표시 시 현재 위치에서 생성
        AGolfBall* CurrentBall = CachedGameMode->GetCurrentTurnGolfBall();
        TerrainGrid->SetCurrentBall(CurrentBall, CachedGameMode->CurrentPlayerIndex);

        UpdateTerrainGridPosition();
        TerrainGrid->SetGridVisible(true);

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
                TEXT("🌍 Terrain Grid: ON"));
        }
    }
    else
    {
        TerrainGrid->SetGridVisible(false);

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
                TEXT("🌍 Terrain Grid: OFF"));
        }
    }

    UE_LOG(LogTemp, Log, TEXT("🌍 Terrain Grid toggled: %s"),
        bTerrainGridVisible ? TEXT("ON") : TEXT("OFF"));
}

void AGolfPlayerController::ShowTerrainGrid(bool bShow)
{
    if (!TerrainGrid)
    {
        InitializeTerrainGrid();
        if (!TerrainGrid) return;
    }

    bTerrainGridVisible = bShow;

    if (bShow)
    {
        UpdateTerrainGridPosition();
    }

    TerrainGrid->SetGridVisible(bShow);

    UE_LOG(LogTemp, Log, TEXT("🌍 Terrain Grid visibility: %s"), bShow ? TEXT("ON") : TEXT("OFF"));
}
bool AGolfPlayerController::IsTerrainGrid()
{
    return TerrainGrid->IsGridVisible();
}



void AGolfPlayerController::UpdateTerrainGridPosition()
{
    if (!TerrainGrid || !bTerrainGridVisible)
        return;

    FVector GridCenter = FVector::ZeroVector;
    bool bValidPosition = false;

    // ⭐ 캐시된 GameMode 사용
    if (CachedGameMode && CachedGameMode->PlayerManager)
    {
        TArray<AGolfBall*> PlayerBalls = CachedGameMode->PlayerManager->GetPlayerBalls();

        AGolfBall* CurrentBall = CachedGameMode->GetCurrentTurnGolfBall();
        UE_LOG(LogTemp, Log, TEXT("TerrainGrid GetCurrentTurnGolfBall Pos: %s"), *CurrentBall->GetActorLocation().ToString());
        // TerrainGrid->SetCurrentBall(CurrentBall, CachedGameMode->CurrentPlayerIndex);

        if (PlayerBalls.IsValidIndex(CachedGameMode->CurrentPlayerIndex))
        {
            UE_LOG(LogTemp, Log, TEXT("TerrainGrid Player Index: %d"), CachedGameMode->CurrentPlayerIndex);
            //CurrentBall = CachedGameMode->GetCurrentTurnGolfBall();
            if (CurrentBall)
            {
                TerrainGrid->SetCurrentBall(CurrentBall, CachedGameMode->CurrentPlayerIndex);
                GridCenter = CurrentBall->GetActorLocation();
                bValidPosition = true;

                if (AimActor)
                {
                    TerrainGrid->SetTargetPosition(AimActor->GetActorLocation());
                }
                else if (CachedGameMode->MapInfo.HolecupPositions.IsValidIndex(CachedGameMode->CurrentHole - 1))
                {
                    TerrainGrid->SetHolecupPosition(CachedGameMode->MapInfo.HolecupPositions[CachedGameMode->CurrentHole - 1]);
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("❌ AimActor is null in UpdateTerrainGridPosition"));
                }
            }
        }
    }

    if (!bValidPosition && CameraManager)
    {
        GridCenter = CameraManager->GetActorLocation();
        bValidPosition = true;

        if (AimActor)
        {
            TerrainGrid->SetTargetPosition(AimActor->GetActorLocation());
            UE_LOG(LogTemp, Warning, TEXT("❌ CameraManager in UpdateTerrainGridPosition"));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("❌ AimActor is null in UpdateTerrainGridPosition"));
        }

        if (CachedGameMode && CachedGameMode->MapInfo.HolecupPositions.IsValidIndex(CachedGameMode->CurrentHole - 1))
        {
            TerrainGrid->SetHolecupPosition(CachedGameMode->MapInfo.HolecupPositions[CachedGameMode->CurrentHole - 1]);
        }
    }

    if (bValidPosition)
    {
        TerrainGrid->UpdateGrid(GridCenter);
        UE_LOG(LogTemp, Log, TEXT("TerrainGrid centered at: %s"), *GridCenter.ToString());
    }
}

void AGolfPlayerController::SetTerrainGridRadius(float NewRadius)
{
    // 반지름 범위 제한 (10m ~ 100m)
    NewRadius = FMath::Clamp(NewRadius, 1000.0f, 10000.0f);

    if (FMath::Abs(TerrainGridRadius - NewRadius) > 100.0f) // 1m 이상 차이날 때만
    {
        TerrainGridRadius = NewRadius;

        if (TerrainGrid)
        {
            TerrainGrid->SetGridRadius(NewRadius);

            // 표시 중이면 즉시 업데이트
            if (bTerrainGridVisible)
            {
                UpdateTerrainGridPosition();
            }
        }

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan,
                FString::Printf(TEXT("📏 Grid Radius: %.0fm"), NewRadius / 100.0f));
        }

        UE_LOG(LogTemp, Log, TEXT("📏 Terrain grid radius set to %.1f meters"), NewRadius / 100.0f);
    }
}

void AGolfPlayerController::RefreshTerrainGrid()
{
    if (!TerrainGrid)
    {
        InitializeTerrainGrid();
        return;
    }

    if (bTerrainGridVisible)
    {
        // 현재 위치에서 격자 재생성
        UpdateTerrainGridPosition();

        // 높이 컬러 새로고침
        //TerrainGrid->RefreshHeightColors();

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
                TEXT("🔄 Terrain Grid Refreshed"));
        }
    }

    UE_LOG(LogTemp, Log, TEXT("🔄 Terrain grid refreshed"));
}

void AGolfPlayerController::UpdateTerrainGridSettings()
{
    if (!TerrainGrid)
        return;

    // 현재 설정값들을 TerrainGrid에 적용
    TerrainGrid->SetGridRadius(TerrainGridRadius);

    // 격자 간격이나 기타 설정들도 여기서 업데이트 가능
    // TerrainGrid->GridSpacing = SomeValue;

    UE_LOG(LogTemp, Log, TEXT("🔧 Terrain grid settings updated"));
}
void AGolfPlayerController::InitializeAimActor()
{
    if (!GetWorld())
    {
        UE_LOG(LogTemp, Error, TEXT("World is null in InitializeAimActor"));
        return;
    }

    // AimActor 스폰
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AimActor = GetWorld()->SpawnActor<AAimActor>(AAimActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

    if (AimActor)
    {
        UE_LOG(LogTemp, Log, TEXT("✅ AimActor spawned successfully"));
        UpdateAimActorPosition(); // 스폰 후 초기 위치 설정
        // ⭐ AimActor를 항상 보이게 하거나, 특정 조건에서 보이게 설정
        AimActor->SetAimVisibility(false); // 디버깅을 위해 일단 보이게 설정
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to spawn AimActor"));
    }
}

void AGolfPlayerController::UpdateAimActorPosition()
{
    if (!AimActor || !CachedGameMode || !CachedGameMode->PlayerManager)
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot update AimActor: Missing components"));
        return;
    }

    TArray<AGolfBall*> PlayerBalls = CachedGameMode->PlayerManager->GetPlayerBalls();
    if (!PlayerBalls.IsValidIndex(CachedGameMode->CurrentPlayerIndex))
        return;

    AGolfBall* CurrentBall = PlayerBalls[CachedGameMode->CurrentPlayerIndex];
    if (!CurrentBall)
        return;

    FVector BallLocation = CurrentBall->GetActorLocation();

    // ⭐ 미니맵 클릭 상태가 아닐 때만 현재 방향 기준으로 업데이트
    if (!CachedGameMode->bClickedMinimap || CachedGameMode->AimLocation.IsZero())
    {
        // AimDirection 기준으로 AimActor 위치 계산
        FVector TargetAimPosition = BallLocation + (AimDirection * AimActorDistance);



        // 홀컵 거리 제한
        FVector HolecupLocation = GetCurrentHolecupPosition();
        if (!HolecupLocation.IsZero())
        {
            FVector OptimalPosition = FindOptimalAimActorPosition(BallLocation, AimDirection,
                FVector::Dist(BallLocation, TargetAimPosition));

            float DistanceToHole = FVector::Dist(BallLocation, HolecupLocation);

            // 홀컵매칭
            if (AimActorDistance >= DistanceToHole)
            {
                TargetAimPosition = HolecupLocation;

                float DistHoletoAim = FVector::Dist(TargetAimPosition, HolecupLocation);
                if (DistHoletoAim > 100.0f)
                {
                    //  UE_LOG(LogTemp, Log, TEXT("🎯 UpdateAimActorPosition Aim to holecup disMatching  Dist------    %f "), DistHoletoAim);
                      //AimActor->SetAimVisibility(true);
                    AimActor->SetAimLocation(OptimalPosition);
                    float DistanceToAim = FVector::Dist(BallLocation, OptimalPosition);
                    AimActor->SetScaleByDistance(DistanceToAim);
                }
                else
                {
                    // UE_LOG(LogTemp, Log, TEXT("🎯UpdateAimActorPosition Aim to holecup Matching  Dist------    %f "), DistHoletoAim);
                    AimActor->SetAimVisibility(false);
                    AimActor->SetActorLocation(TargetAimPosition);
                }

            }
            else
            {
                // AimActor 위치 설정
               // AimActor->SetAimVisibility(true);
                AimActor->SetAimLocation(OptimalPosition);
                float DistanceToAim = FVector::Dist(BallLocation, OptimalPosition);
                AimActor->SetScaleByDistance(DistanceToAim);
            }
        }

    }

    // ⭐⭐⭐ AimActor 위치가 정해진 후 카메라 배치
    PositionCameraForAim();

    // 미니맵 업데이트
    if (CachedGameMode->MiniMapWidget)
    {
        FVector CurrentAimLocation = AimActor->GetActorLocation();
        CachedGameMode->MiniMapWidget->UpdateAimActorPosition(CachedGameMode->CurrentPlayerIndex, CurrentAimLocation);
        CachedGameMode->MiniMapWidget->UpdateAimLinePosition(CachedGameMode->CurrentPlayerIndex);
        CachedGameMode->MiniMapWidget->UpdateBallToAimLinePosition(CachedGameMode->CurrentPlayerIndex);
    }

    // TerrainGrid 업데이트
    if (TerrainGrid && bTerrainGridVisible)
    {
        TerrainGrid->SetTargetPosition(AimActor->GetActorLocation());
        UpdateTerrainGridPosition();
    }

}


void AGolfPlayerController::DebugCurrentBallShot()
{
    if (CachedGameMode) // ⭐ 캐시된 GameMode 사용
    {
        TArray<AGolfBall*> PlayerBalls = CachedGameMode->PlayerManager->GetPlayerBalls();
        if (PlayerBalls.IsValidIndex(CachedGameMode->CurrentPlayerIndex))
        {
            AGolfBall* CurrentBall = PlayerBalls[CachedGameMode->CurrentPlayerIndex];
            if (CurrentBall)
            {
                CurrentBall->LogShotDebugInfo();
            }
        }
    }
}

void AGolfPlayerController::ForceCurrentBallShot()
{
    if (CachedGameMode) // ⭐ 캐시된 GameMode 사용
    {
        TArray<AGolfBall*> PlayerBalls = CachedGameMode->PlayerManager->GetPlayerBalls();
        if (PlayerBalls.IsValidIndex(CachedGameMode->CurrentPlayerIndex))
        {
            AGolfBall* CurrentBall = PlayerBalls[CachedGameMode->CurrentPlayerIndex];
            if (CurrentBall)
            {
                CurrentBall->ForceApplyShot(AimDirection, ShotPower);
            }
        }
    }
}

void AGolfPlayerController::SetNextHole()
{
    AInGameMode* GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    if (!GM)
    {
        UE_LOG(LogTemp, Error, TEXT("SetNextHole: GM is null"));
        return;
    }
    if (!CachedGameMode)
    {
        UE_LOG(LogTemp, Error, TEXT("SetNextHole: CachedGameMode is null"));
        return;
    }
    if (!CachedGameMode->PlayerManager)
    {
        UE_LOG(LogTemp, Error, TEXT("SetNextHole: PlayerManager is null"));
        return;
    }

    TArray<AGolfBall*> PlayerBalls = CachedGameMode->PlayerManager->GetPlayerBalls();

    if (GM->CurrentHole >= GM->MaxHoleCount)
        return;

    const int32 HoleIdx = GM->CurrentHole - 1;
    int32 ParScore = 0;
    if (GM->GameInfo.SelectedMap.ParScores.IsValidIndex(HoleIdx))
    {
        ParScore = GM->GameInfo.SelectedMap.ParScores[HoleIdx];
    }
    else if (GM->MapInfo.ParScores.IsValidIndex(HoleIdx))
    {
        ParScore = GM->MapInfo.ParScores[HoleIdx];
        UE_LOG(LogTemp, Warning, TEXT("SetNextHole: ParScores missing in GameInfo.SelectedMap, using MapInfo (HoleIdx=%d)"),
            HoleIdx);
        GM->GameInfo.SelectedMap.ParScores.SetNum(HoleIdx + 1);
        GM->GameInfo.SelectedMap.ParScores[HoleIdx] = ParScore;
    }
    else
    {
        ParScore = 3 + ((HoleIdx + 1) % 3);
        UE_LOG(LogTemp, Warning, TEXT("SetNextHole: ParScores missing in both GameInfo/MapInfo, using fallback=%d (HoleIdx=%d)"),
            ParScore, HoleIdx);
        GM->GameInfo.SelectedMap.ParScores.SetNum(HoleIdx + 1);
        GM->GameInfo.SelectedMap.ParScores[HoleIdx] = ParScore;
    }

    if (GM->ResultWidgetInstance)
    {
        GM->ResultWidgetInstance->bIsNextHole = true;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SetNextHole: ResultWidgetInstance is null"));
    }
    GM->bSetNextHole = true;

    // 모든 볼의 턴 전환 카운트다운 스킵
    for (AGolfBall* nBall : PlayerBalls)
    {
        if (nBall)
        {
            nBall->SetBallState(EBallState::Ball_Des);
        }
    }

    TArray<AGolfPlayer*> Players = CachedGameMode->PlayerManager->GetPlayers();
    for (AGolfPlayer* nPlayer : Players)
    {
        if (nPlayer)
        {
            if (!nPlayer->PlayerInfo.ShotCountPerHole.IsValidIndex(HoleIdx))
            {
                UE_LOG(LogTemp, Warning, TEXT("SetNextHole: Expanding ShotCountPerHole (HoleIdx=%d Num=%d) Player=%s"),
                    HoleIdx, nPlayer->PlayerInfo.ShotCountPerHole.Num(), *GetNameSafe(nPlayer));
                nPlayer->PlayerInfo.ShotCountPerHole.SetNum(HoleIdx + 1);
            }
            nPlayer->PlayerInfo.ShotCountPerHole[HoleIdx] = ParScore;

            if (FPlayerInfo* InfoPtr = GM->FindPlayerInfoPtr(nPlayer->SlotIndex))
            {
                if (!InfoPtr->ShotCountPerHole.IsValidIndex(HoleIdx))
                {
                    InfoPtr->ShotCountPerHole.SetNum(HoleIdx + 1);
                }
                InfoPtr->ShotCountPerHole[HoleIdx] = ParScore;
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("SetNextHole: FindPlayerInfoPtr failed for SlotIndex=%d"),
                    nPlayer->SlotIndex);
            }
            nPlayer->SetPlayerState(EPlayerState::Player_HoleOut);
        }
    }

    GM->SaveGameInfoToJSON();

    CachedGameMode->ChangeGameState(EGameState::Game_HoleOut);
}

void AGolfPlayerController::SetLastHole()
{
    TArray<AGolfBall*> PlayerBalls = CachedGameMode->PlayerManager->GetPlayerBalls();

    for (AGolfBall* nBall : PlayerBalls)
    {
        if (nBall)
        {
            nBall->SetBallState(EBallState::Ball_Des);
        }
    }

    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    GameMode->CurrentHole = GameMode->MapInfo.HoleCount - 1;

    TArray<AGolfPlayer*> Players = CachedGameMode->PlayerManager->GetPlayers();
    for (AGolfPlayer* nPlayer : Players)
    {
        if (nPlayer)
        {
            nPlayer->PlayerInfo.HoleCount = GameMode->CurrentHole;
            for (int32 i = nPlayer->PlayerInfo.HoleScores.Num(); i < GameMode->CurrentHole; i++)
            {
                nPlayer->PlayerInfo.HoleScores.Add(0);
            }
            nPlayer->SetPlayerState(EPlayerState::Player_HoleOut);
        }
    }
    GameMode->StateMachine.ChangeState(EGameState::Game_HoleResults, 0.1f);
}

void AGolfPlayerController::ShowScoreBoard()
{

    if (CachedGameMode && CachedGameMode->IsStrokeMode()) // ⭐ 캐시된 GameMode 사용
    {
        CachedGameMode->SetShowScoreBoard(1);
    }
}

void AGolfPlayerController::SetLandtype()
{
    CachedGameMode->StrokeWidgetInstance->SetLandType(2);
}






// ⭐ Training Mode 전용 샷 실행 조건 검사
bool AGolfPlayerController::CanExecuteShot_TrainingMode() const
{
    if (!CachedGameMode || !CachedGameMode->PlayerManager)
        return false;

    TArray<AGolfBall*> PlayerBalls = CachedGameMode->PlayerManager->GetPlayerBalls();
    if (PlayerBalls.Num() == 0 || !IsValid(PlayerBalls[0]))
        return false;

    AGolfBall* TrainingBall = PlayerBalls[0];

    // Training Mode에서는 Ready 또는 Des 상태에서 샷 가능
    EBallState BallState = TrainingBall->GetBallState();
    if (BallState != EBallState::Ball_Ready && BallState != EBallState::Ball_Des)
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("Training Mode: Ball not ready (%s)"), *UEnum::GetValueAsString(BallState));
        return false;
    }

    // 샷 파라미터 유효성 (더 관대한 범위)
    if (ShotPower < 1.0f || ShotPower > 60.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid shot power for training: %.2f"), ShotPower);
        return false;
    }

    return true;
}

// ⭐ Training Mode에서 미니맵 업데이트 처리
void AGolfPlayerController::UpdateMiniMapInfo_TrainingMode()
{
    if (!CachedGameMode || !IsValid(CachedGameMode->MiniMapWidget) || !IsValid(CachedGameMode->PlayerManager))
        return;

    TArray<AGolfBall*> PlayerBalls = CachedGameMode->PlayerManager->GetPlayerBalls();
    if (!PlayerBalls.IsValidIndex(0))
        return;

    AGolfBall* TrainingBall = PlayerBalls[0];
    if (!IsValid(TrainingBall))
        return;

    // 거리 정보 업데이트
    if (CachedGameMode->MapInfo.HolecupPositions.IsValidIndex(CachedGameMode->CurrentHole - 1))
    {
        FVector BallPos = TrainingBall->GetActorLocation();
        FVector HolePos = CachedGameMode->MapInfo.HolecupPositions[CachedGameMode->CurrentHole - 1];

        float Distance = FVector::Dist(BallPos, HolePos);
        float Elevation = HolePos.Z - BallPos.Z;

        CachedGameMode->MiniMapWidget->UpdateDistanceAndElevation(Distance, Elevation);
        CachedGameMode->MiniMapWidget->UpdateBallPosition(0, BallPos);
    }

    // 에임 방향도 업데이트
    CachedGameMode->MiniMapWidget->UpdateAimDirection(0, AimDirection);
}

// ⭐ Training Mode 전용 키 바인딩 처리
void AGolfPlayerController::OnTrainingModeReset()
{
    if (!CachedGameMode || !CachedGameMode->IsTrainingMode())
        return;

    CachedGameMode->ResetTrainingBallToTee();

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan,
            TEXT("🎯 Training ball reset to tee"));
    }
}

void AGolfPlayerController::OnTrainingModeToggleBallMovement()
{
    if (!CachedGameMode || !CachedGameMode->IsTrainingMode())
        return;

    if (IsValid(CachedGameMode->MiniMapWidget))
    {
        //bool bCurrentState = CachedGameMode->MiniMapWidget->bAllowBallMovement;
        //CachedGameMode->MiniMapWidget->bAllowBallMovement = !bCurrentState;

        //if (GEngine)
        //{
        //    FString StateText = bCurrentState ? TEXT("DISABLED") : TEXT("ENABLED");
        //    GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
        //        FString::Printf(TEXT("🎯 Ball movement %s"), *StateText));
        //}
    }
}

// 헤더에서 인라인으로 정의했던 함수들을 여기서 구현
bool AGolfPlayerController::IsInTrainingMode() const
{
    return CachedGameMode && CachedGameMode->IsTrainingMode();
}

bool AGolfPlayerController::IsInStrokeMode() const
{
    return CachedGameMode && CachedGameMode->IsStrokeMode();
}

void AGolfPlayerController::SaveCurrentShotInfo()
{
    if (!CachedGameMode || !CachedGameMode->PlayerManager)
        return;

    TArray<AGolfBall*> PlayerBalls = CachedGameMode->PlayerManager->GetPlayerBalls();
    if (!PlayerBalls.IsValidIndex(CachedGameMode->CurrentPlayerIndex))
        return;

    AGolfBall* CurrentBall = PlayerBalls[CachedGameMode->CurrentPlayerIndex];
    if (!CurrentBall)
        return;

    // 현재 샷 정보를 LastShotInfo에 저장
    FVector CurrentDirection = CalculateAimDirection();
    FVector BallLocation = CurrentBall->GetActorLocation();

    LastShotInfo = FLastShotInfo(
        CurrentDirection,
        ShotPower,
        ShotPitchAngle,
        ShotYawAngle,
        BallLocation
    );

    UE_LOG(LogTemp, Log, TEXT("📝 Shot info saved: Power=%.1f, Pitch=%.1f, Yaw=%.1f"),
        ShotPower, ShotPitchAngle, ShotYawAngle);

    // UI에 저장 알림 표시
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
            TEXT("📝 Shot info saved for repeat"));
    }
}

void AGolfPlayerController::RepeatLastShot()
{
    if (!CanRepeatLastShot())
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red,
                TEXT("❌ Cannot repeat last shot"));
        }
        return;
    }

    // 저장된 샷 정보로 현재 설정값들 복원
    ShotPower = LastShotInfo.ShotPower;
    ShotPitchAngle = LastShotInfo.ShotPitchAngle;
    ShotYawAngle = LastShotInfo.ShotYawAngle;
    AimDirection = LastShotInfo.ShotDirection;

    // 카메라 방향도 복원 (선택사항)
    if (CameraManager)
    {
        FRotator TargetRotation = LastShotInfo.ShotDirection.Rotation();
        // 카메라를 저장된 방향으로 회전
        // 이 부분은 필요에 따라 조정
    }

    UE_LOG(LogTemp, Log, TEXT("🔄 Repeating last shot: Power=%.1f, Pitch=%.1f, Yaw=%.1f"),
        ShotPower, ShotPitchAngle, ShotYawAngle);

    // 마지막 샷 정보 표시
    DisplayLastShotInfo();

    // 샷 실행
    ExecuteShot();
}

void AGolfPlayerController::OnRepeatLastShot()
{
    RepeatLastShot();
}

bool AGolfPlayerController::HasValidLastShot() const
{
    return LastShotInfo.IsValidShot();
}

bool AGolfPlayerController::CanRepeatLastShot() const
{
    // 기본 샷 실행 조건 확인
    if (!CanExecuteShot())
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot repeat shot: Basic shot conditions not met"));
        return false;
    }

    // 마지막 샷 정보 유효성 확인
    if (!HasValidLastShot())
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot repeat shot: No valid last shot info"));
        return false;
    }

    return true;
}

FString AGolfPlayerController::GetLastShotInfoString() const
{
    if (!HasValidLastShot())
    {
        return TEXT("No last shot data");
    }

    return FString::Printf(
        TEXT("Last Shot - Power: %.1f m/s, Pitch: %.1f°, Yaw: %.1f°"),
        LastShotInfo.ShotPower,
        LastShotInfo.ShotPitchAngle,
        LastShotInfo.ShotYawAngle
    );
}

void AGolfPlayerController::DisplayLastShotInfo() const
{
    if (!HasValidLastShot())
        return;

    FString InfoText = FString::Printf(
        TEXT("🔄 Repeating Last Shot:\n- Power: %.1f m/s\n- Pitch Angle: %.1f°\n- Yaw Angle: %.1f°"),
        LastShotInfo.ShotPower,
        LastShotInfo.ShotPitchAngle,
        LastShotInfo.ShotYawAngle
    );

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Cyan, InfoText);
    }

    UE_LOG(LogTemp, Log, TEXT("🔄 %s"), *InfoText.Replace(TEXT("\n"), TEXT(" | ")));
}



FVector AGolfPlayerController::FindOptimalAimActorPosition(const FVector& StartLocation, const FVector& Direction, float TargetDistance)
{
    FVector TargetPosition = StartLocation + (Direction * TargetDistance);

    if (IsPositionValid(TargetPosition))
    {
        return TargetPosition;
    }

    UE_LOG(LogTemp, Warning, TEXT("⚠️ Target position invalid, finding safe distance..."));
    FVector SafePosition = FindSafeDistanceOnLine(StartLocation, Direction, TargetDistance);

    if (!SafePosition.IsZero())
    {
        return SafePosition;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ No safe distance found, trying alternative direction..."));
        FVector AltPosition = FindAlternativeDirection(StartLocation, Direction, TargetDistance);

        if (!AltPosition.IsZero())
        {
            return AltPosition;
        }
        else
        {
            // ★ 추가: 모든 시도 실패 시 이전 Aim 위치 유지 또는 홀 방향 fallback
            if (AimActor && !AimActor->GetActorLocation().IsZero())
            {
                UE_LOG(LogTemp, Warning, TEXT("⚠️ Using previous Aim position as fallback"));
                return AimActor->GetActorLocation();  // 이전 위치 유지
            }
            else
            {
                // 홀 방향 fallback (GetCurrentHolecupPosition() 사용)
                FVector Holecup = GetCurrentHolecupPosition();
                if (!Holecup.IsZero())
                {
                    FVector FallbackDir = (Holecup - StartLocation).GetSafeNormal();
                    return StartLocation + (FallbackDir * FMath::Min(TargetDistance, 5000.0f));
                }
                return StartLocation + (Direction * 1000.0f);  // 최소 10m 앞으로 강제
            }
        }
    }
}

FVector AGolfPlayerController::FindSafeDistanceLinear5m(const FVector& StartLocation, const FVector& Direction, float MaxDistance)
{
    float Step = 500.0f;  // 5m 스텝
    for (float TestDistance = MaxDistance; TestDistance >= 100.0f; TestDistance -= Step)
    {
        FVector TestPosition = StartLocation + (Direction * TestDistance);
        if (IsPositionValid(TestPosition)) return TestPosition;
    }
    // 1m 마무리 (정밀도 보장)
    for (float TestDistance = FMath::Min(1000.0f, MaxDistance); TestDistance >= 100.0f; TestDistance -= 100.0f)
    {
        FVector TestPosition = StartLocation + (Direction * TestDistance);
        if (IsPositionValid(TestPosition)) return TestPosition;
    }
    return StartLocation;  // 볼 위치
}

bool AGolfPlayerController::IsPositionValid(const FVector& Position)
{
    // GameMode를 통해 OB 영역 체크
    if (CachedGameMode)
    {
        // OB 영역인지 체크 (InGameMode의 IsPointInOBArea 사용)
        if (CachedGameMode->IsPointInOBArea(Position))
        {
            return false;
        }
    }

    // 미니맵 경계 체크 (선택사항)
    if (CachedGameMode && CachedGameMode->MiniMapWidget)
    {
        FVector2D MapPos = CachedGameMode->MiniMapWidget->WorldToMapPosition(Position);
        if (!CachedGameMode->MiniMapWidget->IsPointInMiniMapBounds(MapPos))
        {
            return false;
        }
    }

    return true;
}

FVector AGolfPlayerController::FindSafeDistanceOnLine(const FVector& StartLocation, const FVector& Direction, float MaxDistance)
{
    float MinDistance = (MaxDistance < 5000.0f) ? 100.0f : 2000.0f;  // 기존
    float SafeDistance = 0.0f;

    // ★ 수정: ZeroPos 체크 제거! 에임은 항상 MinDistance 이상 앞으로 가야 함.
    // FVector ZeroPos = StartLocation;
    // if (IsPositionValid(ZeroPos)) return ZeroPos;  // 제거

    // ★ 추가: MinDistance 위치부터 직접 체크 (초근거리 OB 방지)
    FVector MinPos = StartLocation + (Direction * MinDistance);
    if (IsPositionValid(MinPos)) {
        SafeDistance = MinDistance;  // 최소 거리부터 시작
    }

    // 기존 이진 탐색 (MinDistance ~ MaxDistance)
    for (int32 i = 0; i < 15; ++i)
    {
        float TestDistance = (MinDistance + MaxDistance) * 0.5f;
        FVector TestPosition = StartLocation + (Direction * TestDistance);
        if (IsPositionValid(TestPosition))
        {
            SafeDistance = TestDistance;
            MinDistance = TestDistance;
        }
        else
        {
            MaxDistance = TestDistance;
        }
    }

    // ★ 수정: Threshold를 더 엄격히 (티샷 시 최소 20m 이상)
    float Threshold = (MaxDistance < 5000.0f) ? 100.0f : 3000.0f;  // Threshold 증가 (20m → 30m for safety)
    if (SafeDistance > Threshold)
    {
        return StartLocation + (Direction * SafeDistance);
    }

    // ★ 추가: 찾지 못하면 로그 출력하고 ZeroVector 대신 fallback
    UE_LOG(LogTemp, Warning, TEXT("⚠️ No safe distance found in line. Trying alternatives..."));
    return FVector::ZeroVector;  // FindOptimalAimActorPosition()에서 Alternative 처리
}

FVector AGolfPlayerController::FindAlternativeDirection(const FVector& StartLocation, const FVector& Direction, float TargetDistance)
{
    // --------------------------------------------------------------
     // 1) 현재 목표 위치가 이미 OB라면 바로 역방향 탐색 시작
     // --------------------------------------------------------------
    FVector OriginalTarget = StartLocation + Direction * TargetDistance;

    // 목표가 유효하면 바로 반환 (이 함수는 OB 상황에만 호출된다고 가정)
    if (IsPositionValid(OriginalTarget))
    {
        UE_LOG(LogTemp, Verbose, TEXT("FindAlternativeDirection: Original target is valid"));
        return OriginalTarget;
    }

    // --------------------------------------------------------------
    // 2) 볼 → 에임을 따라 **역방향**(볼 쪽) 안전 거리 탐색
    //    - MinDistance : OB 상황에서 허용되는 최소 거리 (설정값)
    //    - MaxDistance : 원래 목표 거리 (TargetDistance)
    // --------------------------------------------------------------
    float MinDist = 1500.0f;                 // 예: 1500 cm
    float MaxDist = TargetDistance;                       // 원래 거리
    float SafeDist = 0.0f;

    // 이진 탐색 (15회 → 정밀도 약 0.1 cm)
    for (int32 i = 0; i < 15; ++i)
    {
        float TestDist = (MinDist + MaxDist) * 0.5f;
        FVector TestPos = StartLocation + Direction * TestDist;

        if (IsPositionValid(TestPos))
        {
            SafeDist = TestDist;
            MinDist = TestDist;               // 더 멀리 시도
        }
        else
        {
            MaxDist = TestDist;               // 너무 멀면 뒤로
        }
    }

    // --------------------------------------------------------------
    // 3) 안전 거리 확보 → 반환
    // --------------------------------------------------------------
    if (SafeDist >= 1500.0f)
    {
        FVector Result = StartLocation + Direction * SafeDist;
        UE_LOG(LogTemp, Log, TEXT("FindAlternativeDirection: Safe distance %.1f cm (%.1f m)"),
            SafeDist, SafeDist / 100.0f);
        return Result;
    }

    // --------------------------------------------------------------
    // 4) 안전 거리조차 못 찾으면 fallback
    // --------------------------------------------------------------
    UE_LOG(LogTemp, Warning, TEXT("FindAlternativeDirection: No safe distance on line, using fallback"));

    // 4-1) 기존 AimActor 위치가 있으면 그대로 사용
    if (AimActor && !AimActor->GetActorLocation().IsZero())
    {
        UE_LOG(LogTemp, Warning, TEXT("  → Fallback to previous AimActor location"));
        return AimActor->GetActorLocation();
    }

    // 4-2) 홀컵 방향으로 최소 거리 강제 이동
    FVector HolecupPos = GetCurrentHolecupPosition();
    if (!HolecupPos.IsZero())
    {
        FVector HoleDir = (HolecupPos - StartLocation).GetSafeNormal();
        FVector FallbackPos = StartLocation + HoleDir * 1500.0f;

        return FallbackPos;
    }

    // 4-3) 최후의 수단 : 최소 거리만큼 현재 방향으로 강제 이동
    UE_LOG(LogTemp, Error, TEXT("  → Final fallback: force min distance"));
    return StartLocation + Direction * 1500.0f;
}

FVector AGolfPlayerController::AdjustToTerrainHeight(const FVector& Position)
{
    FVector AdjustedPosition = Position;

    // Cup_hole 식별: 이름이 "Cup_hole"로 시작하고(접미 숫자는 옵션) → 유효 표면으로 인정
    auto IsCupHoleActor = [](const AActor* Actor) -> bool
        {
            if (!Actor) return false;
            const FString& Name = Actor->GetName(); // 예: "Cup_hole12"
            if (!Name.StartsWith(TEXT("Cup_hole"))) return false;

            // 숫자 접미를 엄격히 요구하려면 아래 루프 유지,
            // 엄격하지 않게 하려면 바로 true 리턴해도 됨.
            const int32 PrefixLen = 8; // "Cup_hole".Len()
            for (int32 i = PrefixLen; i < Name.Len(); ++i)
            {
                if (!FChar::IsDigit(Name[i]))
                {
                    return false;
                }
            }
            return true;
        };

    // ★ 추가: Landphysic 이름을 가진 StaticMeshActor 식별
    auto IsLandphysicActor = [](const AActor* Actor) -> bool
        {
            if (!Actor) return false;

            // StaticMeshActor인지 확인
            if (!Actor->IsA<AStaticMeshActor>()) return false;

            // 이름에 "Landphysic"이 포함되어 있는지 확인
            const FString& Name = Actor->GetName();
            if (Name.Contains(TEXT("landphysic")) || Name.Contains(TEXT("Landphysic")))
                return true;
            else
                return false;
        };


    if (UWorld* World = GetWorld())
    {
        FVector StartLocation = Position + FVector(0, 0, 2000.0f);  // 범위 확대
        FVector EndLocation = Position - FVector(0, 0, 2000.0f);

        FCollisionQueryParams CollisionParams;
        CollisionParams.AddIgnoredActor(AimActor);
        CollisionParams.bTraceComplex = true;  // 더 정확한 검사

        FCollisionObjectQueryParams ObjectQueryParams;
        ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);

        TArray<FHitResult> HitResults;
        if (World->LineTraceMultiByObjectType(HitResults, StartLocation, EndLocation, ObjectQueryParams, CollisionParams))
        {
            bool bFoundLandscape = false;
            float BestLandscapeZ = 0.0f;

            for (const FHitResult& Hit : HitResults)
            {
                if (!Hit.GetComponent())
                    continue;

                // ⭐ 여러 방법으로 Landscape 확인
                FString ComponentClassName = Hit.GetComponent()->GetClass()->GetName();
                AActor* HitActor = Hit.GetActor();

                bool bIsLandscape =
                    ComponentClassName.Contains(TEXT("Landscape")) ||
                    ComponentClassName.Contains(TEXT("LandscapeComponent")) ||
                    (HitActor && HitActor->GetClass()->GetName().Contains(TEXT("Landscape")));


                // 1) Cup_hole%d 액터 먼저 체크
                if (IsCupHoleActor(HitActor))
                {
                    bIsLandscape = true;
                }

                // 2) ★ 추가: Landphysic StaticMeshActor 체크
                if (IsLandphysicActor(HitActor))
                {
                    bIsLandscape = true;
                }

                if (bIsLandscape)
                {
                    // Position에 가장 가까운 Landscape 선택
                    if (!bFoundLandscape || FMath::Abs(Hit.Location.Z - Position.Z) < FMath::Abs(BestLandscapeZ - Position.Z))
                    {
                        BestLandscapeZ = Hit.Location.Z;
                        bFoundLandscape = true;

                        UE_LOG(LogTemp, Log, TEXT("✅ TRRAIN -ACTOR NAME- [%s] - Component Name-[%s] at Z=%.1f"),
                            *ComponentClassName, *HitActor->GetClass()->GetName(), Hit.Location.Z);
                    }
                }
            }

            if (bFoundLandscape)
            {
                AdjustedPosition.Z = BestLandscapeZ + AimActorHeightOffset;
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("⚠️ No Landscape found among %d hits, using original height"),
                    HitResults.Num());
            }
        }
    }

    return AdjustedPosition;
}

void AGolfPlayerController::OnResetMinimap()
{
    if (!CachedGameMode)
        return;

    if (IsValid(CachedGameMode->MiniMapWidget))
    {
        CachedGameMode->UpdateMiniMapForCurrentHole();
    }

}


// ⭐ 새로 추가: 미니맵에 AimActor 위치 업데이트 전용 함수
void AGolfPlayerController::UpdateMiniMapAimActor()
{
    if (!CachedGameMode || !CachedGameMode->MiniMapWidget || !AimActor)
    {
        return;
    }

    int32 CurrentPlayerIndex = CachedGameMode->CurrentPlayerIndex;
    FVector AimActorPosition = AimActor->GetActorLocation();
    //  FVector AimActorPosition = CachedGameMode->AimLocation;

      // 현재 플레이어 인덱스의 AimActor 위치만 업데이트
    CachedGameMode->MiniMapWidget->UpdateAimActorPosition(CurrentPlayerIndex, AimActorPosition);

    // UE_LOG(LogTemp, Log, TEXT("🎯 MiniMap AimActor updated for current player %d at %s"),
     //    CurrentPlayerIndex, *AimActorPosition.ToString());
}


// ⭐ 기존 UpdateMiniMapAim 함수도 수정
void AGolfPlayerController::UpdateMiniMapAim()
{
    if (!CachedGameMode || !IsValid(CachedGameMode->MiniMapWidget))
    {
        return;
    }

    if (AimDirection.IsNearlyZero() || AimDirection.ContainsNaN())
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Invalid AimDirection: %s"), *AimDirection.ToString());
        return;
    }

    int32 CurrentPlayerIndex = CachedGameMode->CurrentPlayerIndex;

    // ⭐ 수동 회전 중이면 여기서는 업데이트하지 않음
    if (bIsManuallyRotating)
    {
        UE_LOG(LogTemp, Warning, TEXT("Skipping UpdateMiniMapAim - manual rotation in progress"));
        return;
    }

    // 현재 플레이어의 에임 방향만 업데이트
    CachedGameMode->MiniMapWidget->UpdateAimDirection(CurrentPlayerIndex, AimDirection);

    // ⭐ AimActor 위치 업데이트는 선택적으로만
    if (!bIsManuallyRotating && AimActor)
    {
        UpdateMiniMapAimActor();
    }

    UE_LOG(LogTemp, Warning, TEXT("🎯 MiniMap aim updated for player %d"), CurrentPlayerIndex);
}




void AGolfPlayerController::RotateCameraToDirection(const FVector& Direction)
{
    if (!CameraManager)
        return;

    FVector HorizontalDirection = Direction;
    HorizontalDirection.Z = 0.0f;
    HorizontalDirection.Normalize();

    FRotator TargetRotation = HorizontalDirection.Rotation();
    //CameraManager->Camera->SetWorldRotation(TargetRotation);
    CameraManager->SetActorRotation(TargetRotation);

    // ⭐⭐⭐ 수정: 카메라가 이 방향을 바라보도록 회전
    // 공과 AimActor를 고려한 카메라 위치 재계산
    //if (CachedGameMode && CachedGameMode->PlayerManager)
    //{
    //    TArray<AGolfBall*> PlayerBalls = CachedGameMode->PlayerManager->GetPlayerBalls();
    //    if (PlayerBalls.IsValidIndex(CachedGameMode->CurrentPlayerIndex))
    //    {
    //        AGolfBall* CurrentBall = PlayerBalls[CachedGameMode->CurrentPlayerIndex];
    //        if (CurrentBall)
    //        {
    //            FVector BallLocation = CurrentBall->GetActorLocation();

    //            // 카메라를 공 뒤쪽에 배치
    //            FVector CameraOffset = -HorizontalDirection * CameraManager->CameraDistanceFromBall;
    //            CameraOffset.Z = CameraManager->CameraHeightFromBall;
    //            FVector NewCameraPos = BallLocation + CameraOffset;

    //            CameraManager->SetActorLocation(NewCameraPos);

    //            // 카메라가 공(그리고 결과적으로 AimActor 방향)을 바라보도록
    //            FVector LookDirection = (BallLocation - NewCameraPos).GetSafeNormal();
    //            FRotator NewRotation = FRotationMatrix::MakeFromX(LookDirection).Rotator();
    //            CameraManager->Camera->SetWorldRotation(NewRotation);

    //            UE_LOG(LogTemp, Log, TEXT("📷 Camera rotated: Position=%s, LookingAt=Ball, Forward=%s"),
    //                *NewCameraPos.ToString(), *LookDirection.ToString());
    //        }
    //    }
    //}


    // AimDirection 동기화
    AimDirection = HorizontalDirection;

    // AimActor 위치 업데이트
    UpdateAimActorPosition();

    // 미니맵 업데이트
    UpdateMiniMapAim();

    //  UE_LOG(LogTemp, Log, TEXT("Camera rotated to direction: %s"), *Direction.ToString());
}

// 2. 홀컵 거리 제한 계산 함수 추가
float AGolfPlayerController::CalculateLimitedAimDistance(const FVector& BallLocation, const FVector& HolecupLocation, const FVector& TargetDirection) const
{
    // 볼에서 홀컵까지의 거리
    float BallToHolecupDistance = FVector::Dist(BallLocation, HolecupLocation);

    // 볼에서 에임 방향으로 홀컵까지의 투영 거리 계산
    FVector BallToHolecup = HolecupLocation - BallLocation;
    float ProjectedDistance = FVector::DotProduct(BallToHolecup, TargetDirection); // 수정된 변수명 사용

    // 투영 거리가 음수면 (홀컵이 뒤쪽에 있으면) 기본 거리 사용
    if (ProjectedDistance <= 0.0f)
    {
        return FMath::Min(AimActorDistance, BallToHolecupDistance * 0.99f); // 홀컵 거리의 80%
    }

    // 에임 방향으로 갔을 때 홀컵을 넘지 않도록 제한
    float MaxAllowedDistance = FMath::Min(ProjectedDistance, BallToHolecupDistance);

    // 기본 AimActorDistance와 비교하여 더 작은 값 선택
    float LimitedDistance = FMath::Min(AimActorDistance, MaxAllowedDistance);

    // 최소 거리 보장 (너무 가깝지 않게)
    float MinDistance = 1000.0f; // 10미터
    LimitedDistance = FMath::Max(LimitedDistance, MinDistance);

    UE_LOG(LogTemp, Log, TEXT("Distance calculation: Ball→Holecup=%.1fm, Projected=%.1fm, Limited=%.1fm"),
        BallToHolecupDistance / 100.0f, ProjectedDistance / 100.0f, LimitedDistance / 100.0f);

    return LimitedDistance;
}

// 3. 현재 홀컵 위치 가져오기 함수 추가
FVector AGolfPlayerController::GetCurrentHolecupPosition() const
{
    if (!CachedGameMode)
    {
        return FVector::ZeroVector;
    }

    int32 CurrentHoleIndex = CachedGameMode->CurrentHole - 1;
    if (!CachedGameMode->MapInfo.HolecupPositions.IsValidIndex(CurrentHoleIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid hole index: %d"), CurrentHoleIndex);
        return FVector::ZeroVector;
    }

    return CachedGameMode->MapInfo.HolecupPositions[CurrentHoleIndex];
}

// 4. 에임 위치를 홀컵 기준으로 검증하는 함수 추가
bool AGolfPlayerController::IsAimPositionValid(const FVector& AimPosition, const FVector& BallPosition) const
{
    FVector HolecupPosition = GetCurrentHolecupPosition();
    if (HolecupPosition.IsZero())
    {
        return true; // 홀컵 위치를 알 수 없으면 통과
    }

    // 에임 위치가 홀컵보다 멀면 무효
    float AimDistanceFromBall = FVector::Dist(BallPosition, AimPosition);
    float HolecupDistanceFromBall = FVector::Dist(BallPosition, HolecupPosition);

    if (AimDistanceFromBall > HolecupDistanceFromBall)
    {
        UE_LOG(LogTemp, Warning, TEXT("Aim position too far: %.1fm (Holecup: %.1fm)"),
            AimDistanceFromBall / 100.0f, HolecupDistanceFromBall / 100.0f);
        return false;
    }

    return true;
}

// 5. 미니맵 클릭 시에도 홀컵 거리 제한 적용
void AGolfPlayerController::SetAimToPosition(const FVector& TargetPosition)
{
    if (!AimActor || !CachedGameMode || !CachedGameMode->PlayerManager)
        return;

    // 현재 볼 위치 가져오기
    TArray<AGolfBall*> PlayerBalls = CachedGameMode->PlayerManager->GetPlayerBalls();
    if (!PlayerBalls.IsValidIndex(CachedGameMode->CurrentPlayerIndex))
        return;

    AGolfBall* CurrentBall = PlayerBalls[CachedGameMode->CurrentPlayerIndex];
    if (!CurrentBall)
        return;

    FVector BallLocation = CurrentBall->GetActorLocation();
    FVector HolecupLocation = GetCurrentHolecupPosition();

    // 홀컵 거리 제한 적용
    FVector LimitedTargetPosition = TargetPosition;

    if (!HolecupLocation.IsZero())
    {
        // ✅ 1. 에임 위치가 홀컵 반경 30cm 이내인지 먼저 확인
        float TargetToHolecupDist = FVector::Dist(TargetPosition, HolecupLocation);
        if (TargetToHolecupDist <= 30.0f)
        {
            // 홀컵 높이값 강제 적용
            LimitedTargetPosition.Z = HolecupLocation.Z;
            // UE_LOG(LogTemp, Log, TEXT("⛳ Aim is within 30cm of Holecup. Snapping Z to Holecup Height."));
        }

        float BallToTarget = FVector::Dist(BallLocation, LimitedTargetPosition);
        float BallToHolecup = FVector::Dist(BallLocation, HolecupLocation);

        // 목표 위치가 홀컵보다 멀면 홀컵 방향으로 제한
        if (BallToTarget >= BallToHolecup)
        {
            FVector DirectionToTarget = (LimitedTargetPosition - BallLocation).GetSafeNormal();
            LimitedTargetPosition = BallLocation + (DirectionToTarget * BallToHolecup);

            // 거리 제한 후에도 홀컵 근처라면 Z값 다시 보정
            if (TargetToHolecupDist <= 30.0f)
            {
                LimitedTargetPosition.Z = HolecupLocation.Z;
            }

            UE_LOG(LogTemp, Log, TEXT("Limited aim position: %.1fm → %.1fm (Holecup: %.1fm)"),
                BallToTarget / 100.0f, FVector::Dist(BallLocation, LimitedTargetPosition) / 100.0f, BallToHolecup / 100.0f);
        }
    }

    // 방향 계산
    FVector TargetDirection = (LimitedTargetPosition - BallLocation).GetSafeNormal();
    TargetDirection.Z = 0.0f;
    TargetDirection.Normalize();

    // AimDirection 설정
    AimDirection = TargetDirection;
    // 실제 제한된 위치와의 거리를 저장하도록 수정
    AimActorDistance = FVector::Dist(BallLocation, LimitedTargetPosition);

    // 카메라 회전
    RotateCameraToDirection(TargetDirection);

    // AimActor 위치 설정
    FVector AdjustedPosition = LimitedTargetPosition;

    // ✅ 2. 지형 높이 조절 단계 (홀컵 근처 30cm 이내가 아닐 때만 지형 높이 적용)
    if (!HolecupLocation.IsZero() && !CachedGameMode->CheckFirstShot())
    {
        float FinalDistToHolecup = FVector::Dist(LimitedTargetPosition, HolecupLocation);
        if (FinalDistToHolecup > 30.0f)
        {
            AdjustedPosition = AdjustToTerrainHeight(LimitedTargetPosition);
        }
        else
        {
            // 홀컵 근처라면 지형 검사 대신 홀컵 Z값 유지
            AdjustedPosition.Z = HolecupLocation.Z;
        }
    }

    if (CachedGameMode->CheckFirstShot())
        AdjustedPosition = AdjustToTerrainHeight(LimitedTargetPosition);

    AimActor->SetAimLocation(AdjustedPosition);
    float DistanceToAim = FVector::Dist(BallLocation, AdjustedPosition);
    AimActor->SetScaleByDistance(DistanceToAim);

    UE_LOG(LogTemp, Error, TEXT("------  SetAimtoPosition()::SetActorLocation() - %s"), *AimActor->GetActorLocation().ToString());

    // GameMode 동기화
    CachedGameMode->AimLocation = AdjustedPosition;

    // 미니맵 업데이트
    UpdateMiniMapAim();
}

void AGolfPlayerController::SetAimToExactPosition(const FVector& ExactPosition)
{
    if (!AimActor || !CachedGameMode || !CachedGameMode->PlayerManager)
        return;

    TArray<AGolfBall*> PlayerBalls = CachedGameMode->PlayerManager->GetPlayerBalls();
    if (!PlayerBalls.IsValidIndex(CachedGameMode->CurrentPlayerIndex))
        return;

    AGolfBall* CurrentBall = PlayerBalls[CachedGameMode->CurrentPlayerIndex];
    if (!CurrentBall)
        return;

    FVector BallLocation = CurrentBall->GetActorLocation();

    // 방향 계산
    FVector DirectionToTarget = (ExactPosition - BallLocation).GetSafeNormal();
    DirectionToTarget.Z = 0.0f;
    DirectionToTarget.Normalize();

    AimDirection = DirectionToTarget;

    // 거리 계산
    float ClickDistance = FVector::Dist(BallLocation, ExactPosition);
    FVector HolecupLocation = GetCurrentHolecupPosition();
    FVector FinalAimPosition;


    // 홀컵 거리 체크
    if (!HolecupLocation.IsZero())
    {
        float HolecupDistance = FVector::Dist(BallLocation, HolecupLocation);
        if (ClickDistance >= HolecupDistance)
        {

            FinalAimPosition = BallLocation + (DirectionToTarget * HolecupDistance); // 홀컵의 12cm 거리
            AimActorDistance = HolecupDistance;
        }
        else
        {
            FinalAimPosition = ExactPosition;
            AimActorDistance = ClickDistance;
        }
    }
    else
    {
        FinalAimPosition = ExactPosition;
        AimActorDistance = ClickDistance;
    }

    // 지형 높이 조정
    FVector AdjustedPosition = FinalAimPosition;
    if (!HolecupLocation.IsZero())
        AdjustedPosition = AdjustToTerrainHeight(FinalAimPosition);

    // ⭐ 1. AimActor 먼저 배치
    AimActor->SetAimLocation(AdjustedPosition);
    float DistanceToAim = FVector::Dist(BallLocation, AdjustedPosition);
    AimActor->SetScaleByDistance(DistanceToAim);
    UE_LOG(LogTemp, Error, TEXT("------  SetAimToExactPosition()::SetActorLocation() - %s"), *AimActor->GetActorLocation().ToString());
    // ⭐ 2. 카메라를 Ball-AimActor 기준으로 배치
    //if (CameraManager)
    //{
    //    FVector DirectAimPosition = AdjustedPosition;
    //    CameraManager->PositionCameraForAimView(BallLocation, DirectAimPosition);
    //}

    // GameMode 동기화
    CachedGameMode->AimLocation = AdjustedPosition;
    CachedGameMode->bClickedMinimap = true;

    // 미니맵 업데이트
    if (CachedGameMode->MiniMapWidget)
    {
        CachedGameMode->MiniMapWidget->UpdateAimActorPosition(CachedGameMode->CurrentPlayerIndex, AdjustedPosition);
        CachedGameMode->MiniMapWidget->UpdateAimLinePosition(CachedGameMode->CurrentPlayerIndex);
        CachedGameMode->MiniMapWidget->UpdateBallToAimLinePosition(CachedGameMode->CurrentPlayerIndex);
        CachedGameMode->MiniMapWidget->UpdateTip2();
    }

    // TerrainGrid 업데이트
    if (TerrainGrid && bTerrainGridVisible)
    {
        TerrainGrid->SetTargetPosition(AdjustedPosition);
        UpdateTerrainGridPosition();
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
            FString::Printf(TEXT("🎯 Aim Set: %.1fm from ball"), AimActorDistance / 100.0f));
    }

    CachedGameMode->StrokeWidgetInstance->PositionCanvasPanelAboveHole();

    UE_LOG(LogTemp, Log, TEXT("✅ AimActor positioned, Camera follows - Aim at %s"), *AdjustedPosition.ToString());
}



// ⭐ 새로 추가: 홀 변경 시 AimActor 재설정
void AGolfPlayerController::OnHoleChanged(int32 NewHoleNumber)
{
    // 새 홀에 대해 AimActor 재설정
    //InitializeAimActorToHoleDirection();
    AimActorDistance = 5000.0f;
    UpdateAimActorPosition();

    UE_LOG(LogTemp, Log, TEXT("AimActor reset for new hole: %d"), NewHoleNumber);
}

// ⭐ 새로 추가: 플레이어 전환 시 AimActor 재설정
void AGolfPlayerController::OnPlayerIndexChanged(int32 NewPlayerIndex)
{
    if (!CachedGameMode || NewPlayerIndex != CachedGameMode->CurrentPlayerIndex)
        return;

    UE_LOG(LogTemp, Warning, TEXT("🔄 Player index changed to: %d"), NewPlayerIndex);

    // 이전 플레이어 요소들 정리 (미니맵)
    if (PreviousPlayerIndex >= 0 && PreviousPlayerIndex != NewPlayerIndex)
    {
        NotifyMiniMapPlayerChanged(NewPlayerIndex, PreviousPlayerIndex);
    }

    // 현재 플레이어로 전환된 경우 AimActor 재설정
    if (CachedGameMode->CheckFirstShot())
        AimActorDistance = 5000.0f;

    UpdateAimActorPosition();

    // 미니맵을 현재 플레이어 기준으로 업데이트
    UpdateMiniMapForCurrentPlayerOnly();

    UE_LOG(LogTemp, Log, TEXT("✅ AimActor reset for player index change: %d"), NewPlayerIndex);
}
void AGolfPlayerController::SetPenaltyDrop()
{

    if (AGolfBall* Ball = CachedPlayerManager->GetPlayerBalls()[CachedGameMode->CurrentPlayerIndex])
    {
        // 특정 위치에 장애물 있는지 체크
       // FVector BallLocation = Ball->GetActorLocation();
       // bool bHasObstacle = Ball->CheckObstacleAtPosition(BallLocation, 50.0f);

        // 벌타 드롭 실행
        Ball->HandlePenaltyDrop();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("AGolfPlayerController::SetParticle() = Ball Is null"));
    }


}

void AGolfPlayerController::NotifyMiniMapPlayerChanged(int32 NewPlayerIndex, int32 nPreviousPlayerIndex)
{
    if (!CachedGameMode || !CachedGameMode->MiniMapWidget)
    {
        return;
    }

    // 미니맵에 플레이어 전환 알림
    CachedGameMode->MiniMapWidget->OnPlayerTurnChanged(NewPlayerIndex, nPreviousPlayerIndex);

    UE_LOG(LogTemp, Log, TEXT("🔄 Notified MiniMap of player change: %d → %d"),
        nPreviousPlayerIndex, NewPlayerIndex);
}
void AGolfPlayerController::UpdateMiniMapForCurrentPlayerOnly()
{
    if (!CachedGameMode || !CachedGameMode->MiniMapWidget)
    {
        return;
    }

    int32 CurrentPlayerIndex = CachedGameMode->CurrentPlayerIndex;

    // 현재 플레이어만 표시
    CachedGameMode->MiniMapWidget->ShowOnlyCurrentPlayer(CurrentPlayerIndex);

    // AimActor 위치도 업데이트
    if (AimActor && CachedGameMode->PlayerManager)
    {
        TArray<AGolfBall*> PlayerBalls = CachedGameMode->PlayerManager->GetPlayerBalls();
        if (PlayerBalls.IsValidIndex(CurrentPlayerIndex))
        {
            AGolfBall* CurrentBall = PlayerBalls[CurrentPlayerIndex];
            if (IsValid(CurrentBall))
            {
                // 현재 플레이어 기준으로 AimActor 위치 재설정
                //InitializeAimActorToHoleDirection();
                UpdateAimActorPosition();
                // 미니맵에 AimActor 위치 업데이트
                CachedGameMode->MiniMapWidget->UpdateAimActorPosition(
                    CurrentPlayerIndex,
                    AimActor->GetActorLocation()
                );
                UE_LOG(LogTemp, Log, TEXT("=====> UpdateMiniMapForCurrentPlayerOnly CurrentPlayer: %d "), CurrentPlayerIndex);
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("✅ Updated MiniMap for current player only: %d"), CurrentPlayerIndex);
}



void AGolfPlayerController::SetFirstShotAim(const FVector& TeePosition, const FVector& HolecupPosition)
{
    if (!CachedGameMode)
        return;

    // 현재 플레이어가 첫 샷인지 확인
    AGolfPlayer* CurrentPlayer = GetCurrentGolfPlayer();
    if (!CurrentPlayer || CurrentPlayer->GetCurrentHoleShotCount() > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Not first shot, using regular aim logic"));
        return;
    }

    // 티-홀 방향 계산
    FVector TeeToHoleDirection = (HolecupPosition - TeePosition).GetSafeNormal();
    TeeToHoleDirection.Z = 0.0f;
    TeeToHoleDirection.Normalize();

    // 카메라를 티-홀 방향으로 회전
    if (CameraManager)
    {
        FRotator TargetRotation = TeeToHoleDirection.Rotation();
        CameraManager->SetActorRotation(TargetRotation);

        UE_LOG(LogTemp, Log, TEXT("🎥 Camera rotated to tee-hole direction for first shot"));
    }

    // AimDirection 설정
    AimDirection = TeeToHoleDirection;

    // 50미터 기본 거리 설정 (OB 회피는 FindFirstShotAimPosition에서 처리됨)
    AimActorDistance = 5000.0f; // 50미터

    UE_LOG(LogTemp, Log, TEXT("🎯 First shot aim configured: Direction=%s, Distance=%.1fm"),
        *TeeToHoleDirection.ToString(), AimActorDistance / 100.0f);
}

bool AGolfPlayerController::IsFirstShot() const
{
    if (!CachedGameMode)
        return false;

    AGolfPlayer* CurrentPlayer = GetCurrentGolfPlayer();
    if (!CurrentPlayer)
        return false;

    return CurrentPlayer->GetCurrentHoleShotCount() == 0;
}


// =============================================================================
// 5️⃣ ✨ InitializeSwingRecording (시스템 초기화)
// =============================================================================

void AGolfPlayerController::InitializeSwingRecording()
{
    UE_LOG(LogTemp, Warning, TEXT("📹 ========== Initializing Swing Recording =========="));


    if (!bEnableVideoSaving)
    {
        UE_LOG(LogTemp, Warning, TEXT("📹 Video Saving Disabled - Skipping swing recording"));
        return;
    }


    if (!WebcamCaptureClass)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ WebcamCaptureClass not set!"));
        return;
    }

    // 기존 액터 정리
    if (WebcamCaptureActor && IsValid(WebcamCaptureActor))
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Cleaning up existing actor"));

        // 델리게이트 해제
        WebcamCaptureActor->OnSwingDetected.RemoveDynamic(
            this,
            &AGolfPlayerController::OnSwingRecordedHandler
        );

        // ✅ InGameMode에서 로드된 설정 적용
        AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
        if (GameMode)
        {
            // WebcamCaptureActor->SetVideoSavingEnabled(GameMode->WebcamSettings.bEnableVideoSaving);
            if (GameMode->GameInfo.GameOptions.SwingMotion)
                WebcamCaptureActor->SetVideoSavingEnabled(true);
            else
                WebcamCaptureActor->SetVideoSavingEnabled(false);
        }

        WebcamCaptureActor->StopCapture();

        // VideoWidget 정리
        if (WebcamCaptureActor->VideoWidget && IsValid(WebcamCaptureActor->VideoWidget))
        {
            if (WebcamCaptureActor->VideoWidget->VideoDisplay)
            {
                FSlateBrush EmptyBrush;
                WebcamCaptureActor->VideoWidget->VideoDisplay->SetBrush(EmptyBrush);
            }
            WebcamCaptureActor->VideoWidget->RemoveFromParent();
        }

        WebcamCaptureActor->Destroy();
        WebcamCaptureActor = nullptr;

        // ✅ 0.5초 대기 후 새 액터 생성
        FTimerHandle DestroyWaitTimer;
        GetWorld()->GetTimerManager().SetTimer(
            DestroyWaitTimer,
            [this]()
            {
                FActorSpawnParameters SpawnParams;
                SpawnParams.Owner = this;
                SpawnParams.Name = FName(TEXT("WebcamCaptureActor"));

                WebcamCaptureActor = GetWorld()->SpawnActor<AWebcamCapture>(
                    WebcamCaptureClass,
                    FVector::ZeroVector,
                    FRotator::ZeroRotator,
                    SpawnParams
                );

                if (!WebcamCaptureActor)
                {
                    UE_LOG(LogTemp, Error, TEXT("❌ Failed to spawn WebcamCaptureActor"));
                    return;
                }

                UE_LOG(LogTemp, Log, TEXT("✅ New WebcamCaptureActor spawned"));

                // ✅ 초기화 추가!
                WebcamCaptureActor->CreateVideoWidget();
                WebcamCaptureActor->OnSwingDetected.AddDynamic(this, &AGolfPlayerController::OnSwingRecordedHandler);

                AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
                if (GameMode)
                {
                    // ✅ 수정: RangeMode일 때는 RangeSwingMotion으로 분기
                    bool bSwingEnabled = false;

                    if (GameMode->IsRangeMode())
                    {
                        bSwingEnabled = (GameMode->GameInfo.GameOptions.RangeSwingMotion == 1);
                        UE_LOG(LogTemp, Log, TEXT("📹 RangeMode SwingMotion: %s"), bSwingEnabled ? TEXT("ON") : TEXT("OFF"));
                    }
                    else
                    {
                        bSwingEnabled = (GameMode->GameInfo.GameOptions.SwingMotion == 1);
                        UE_LOG(LogTemp, Log, TEXT("📹 SwingMotion: %s"), bSwingEnabled ? TEXT("ON") : TEXT("OFF"));
                    }

                    WebcamCaptureActor->SetVideoSavingEnabled(bSwingEnabled);
                }

                // WebcamCaptureActor->StartCapture();
                UE_LOG(LogTemp, Warning, TEXT("✅ WebcamCapture fully initialized!"));
            },  // ← 수정! (괄호 추가)
            0.5f,
            false
        );

        return;
    }

    // WebcamCapture 액터 생성
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Name = FName(TEXT("WebcamCaptureActor"));

    WebcamCaptureActor = GetWorld()->SpawnActor<AWebcamCapture>(
        WebcamCaptureClass,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        SpawnParams
    );

    if (!WebcamCaptureActor)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to spawn WebcamCaptureActor"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("✅ WebcamCaptureActor spawned successfully"));

    // ✅✅✅ 핵심: 지연 초기화로 컴포넌트 생성 완료 대기 ✅✅✅
    FTimerHandle InitDelayTimer;
    GetWorld()->GetTimerManager().SetTimer(
        InitDelayTimer,
        [this]()
        {
            if (!WebcamCaptureActor || !WebcamCaptureActor->IsValidLowLevel())
            {
                UE_LOG(LogTemp, Error, TEXT("❌ WebcamCaptureActor invalid during delayed init"));
                return;
            }

            UE_LOG(LogTemp, Log, TEXT("🔍 Checking components..."));

            // ✅ 컴포넌트 유효성 체크
            bool bAllValid = true;

            if (!WebcamCaptureActor->MediaPlayer)
            {
                UE_LOG(LogTemp, Error, TEXT("❌ MediaPlayer is NULL"));
                bAllValid = false;
            }
            else
            {
                UE_LOG(LogTemp, Log, TEXT("✅ MediaPlayer OK"));
            }

            if (!WebcamCaptureActor->MediaTexture)
            {
                UE_LOG(LogTemp, Error, TEXT("❌ MediaTexture is NULL"));
                bAllValid = false;
            }
            else
            {
                UE_LOG(LogTemp, Log, TEXT("✅ MediaTexture OK"));
            }

            if (!WebcamCaptureActor->VideoBufferComponent)
            {
                UE_LOG(LogTemp, Error, TEXT("❌ VideoBufferComponent is NULL"));
                bAllValid = false;
            }
            else
            {
                UE_LOG(LogTemp, Log, TEXT("✅ VideoBufferComponent OK"));
            }

            if (!WebcamCaptureActor->CaptureRenderTarget)
            {
                UE_LOG(LogTemp, Error, TEXT("❌ CaptureRenderTarget is NULL"));
                bAllValid = false;
            }
            else
            {
                UE_LOG(LogTemp, Log, TEXT("✅ CaptureRenderTarget OK"));
            }

            if (!bAllValid)
            {
                UE_LOG(LogTemp, Error, TEXT("❌ Component validation failed!"));
                UE_LOG(LogTemp, Error, TEXT("   Check BP_WebcamCapture blueprint"));
                return;
            }

            UE_LOG(LogTemp, Warning, TEXT("✅ All components validated"));

            // ✅ 델리게이트 바인딩
            WebcamCaptureActor->OnSwingDetected.AddDynamic(
                this,
                &AGolfPlayerController::OnSwingRecordedHandler
            );
            UE_LOG(LogTemp, Log, TEXT("✅ Delegate bound"));

            // ✅ UI 생성
            if (WebcamCaptureActor->VideoWidgetClass)
            {
                WebcamCaptureActor->CreateVideoWidget();
                // WebcamCaptureActor->ShowVideoWidget();
                UE_LOG(LogTemp, Log, TEXT("✅ Widget created and shown"));
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("⚠️ VideoWidgetClass not set"));
            }

            // ✅ 캡처 시작 (추가 0.3초 지연)
            FTimerHandle CaptureStartTimer;
            GetWorld()->GetTimerManager().SetTimer(
                CaptureStartTimer,
                [this]()
                {
                    if (WebcamCaptureActor && WebcamCaptureActor->IsValidLowLevel())
                    {
                        //WebcamCaptureActor->StartCapture();
                        UE_LOG(LogTemp, Warning, TEXT("✅✅✅ Capture started successfully! ✅✅✅"));
                    }
                },
                0.3f,
                false
            );

            UE_LOG(LogTemp, Log, TEXT("📹 Initialization phase 1 complete, starting capture..."));
        },
        0.5f,  // ✅ 0.5초 지연 (컴포넌트 생성 대기)
        false
    );

    UE_LOG(LogTemp, Log, TEXT("📹 Delayed initialization scheduled (0.5s)"));
}


// =============================================================================
// 6️⃣ ✨ TriggerSwingRecording (수동 트리거)
// =============================================================================

void AGolfPlayerController::TriggerSwingRecording()
{
    SCOPE_CYCLE_COUNTER(STAT_PCTriggerSwing);
    if (!bEnableVideoSaving)
    {
        UE_LOG(LogTemp, Warning, TEXT("📹 Video Saving Disabled - Skipping swing recording"));
        return;
    }

    // ✅ 1. WebcamCaptureActor 유효성 체크
    if (!WebcamCaptureActor || !IsValid(WebcamCaptureActor))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ WebcamCaptureActor not valid"));
        return;
    }

    // ✅ 2. MediaPlayer 상태 체크
    if (!WebcamCaptureActor->MediaPlayer || !IsValid(WebcamCaptureActor->MediaPlayer))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ MediaPlayer not valid"));
        return;
    }

    if (!WebcamCaptureActor->MediaPlayer->IsPlaying())
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ MediaPlayer not playing - cannot record"));
        return;
    }

    // ✅ 3. VideoBuffer 유효성 체크
    if (!WebcamCaptureActor->VideoBufferComponent ||
        !IsValid(WebcamCaptureActor->VideoBufferComponent))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ VideoBufferComponent not valid"));
        return;
    }

    int32 BufferedFrames = WebcamCaptureActor->VideoBufferComponent->GetBufferedFrameCount();


    // ✅ **수정**: 최소 기준을 30에서 10으로 완화
    // 이유: StartCapture() 직후에는 첫 프레임이 아직 추가되지 않음
    //       0.1초 정도 기다리면 3-4프레임이 쌓임
    //       따라서 10프레임만 있으면 충분 (약 0.33초)
    if (BufferedFrames < 10)
    {
        SwingRecordingRetryCount++;

        UE_LOG(LogTemp, Warning, TEXT("⚠️ Not enough frames (%d), waiting..."), BufferedFrames);
        //if (SwingRecordingRetryCount > MAX_RETRIES)
        //{
        //    UE_LOG(LogTemp, Error,
        //        TEXT("❌ Max retries exceeded"));
        //    SwingRecordingRetryCount = 0;
        //    return;
        //}

        FTimerHandle RetryHandle;  // 지역변수!
        // 재시도
        GetWorld()->GetTimerManager().SetTimer(
            RetryHandle,
            [this]() { TriggerSwingRecording(); },
            0.1f,
            false
        );
        return;
    }

    SwingRecordingRetryCount = 0;  // 성공 시 리셋
    UE_LOG(LogTemp, Warning, TEXT("🎬 SWING RECORDING TRIGGERED"));
    UE_LOG(LogTemp, Log, TEXT("   Buffered frames: %d"), BufferedFrames);

    WebcamCaptureActor->TriggerShotRecording();
}


// =============================================================================
// 7️⃣ ✨✨✨ OnSwingRecordedHandler (핵심 이벤트 핸들러) ✨✨✨
// =============================================================================

void AGolfPlayerController::OnSwingRecordedHandler(const TArray<FVideoFrame>& SwingFrames)
{

    UE_LOG(LogTemp, Warning, TEXT("🎬 Swing recorded with %d frames"), SwingFrames.Num());

    if (!bEnableVideoSaving)
    {
        UE_LOG(LogTemp, Warning, TEXT("📹 Video Saving Disabled - Skipping swing recording"));
        return;
    }


    // ✅ 1. WebcamCaptureActor 유효성 체크 (추가)
    if (!WebcamCaptureActor || !IsValid(WebcamCaptureActor))
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ WebcamCaptureActor invalid - ignoring event"));
        return;
    }

    // ✅ 2. 프레임 유효성 체크 (추가)
    if (SwingFrames.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ No frames - ignoring event"));
        return;
    }
    // 로그 기록
    if (bLogSwingEvents)
    {
        UE_LOG(LogTemp, Warning, TEXT(""));
        UE_LOG(LogTemp, Warning, TEXT("🎬 =========================================="));
        UE_LOG(LogTemp, Warning, TEXT("🎬 SWING RECORDED - Processing..."));
        UE_LOG(LogTemp, Warning, TEXT("🎬 =========================================="));
        UE_LOG(LogTemp, Log, TEXT("📹 Captured frames: %d"), SwingFrames.Num());

        if (SwingFrames.Num() > 0)
        {
            float Duration = SwingFrames.Last().Timestamp - SwingFrames[0].Timestamp;
            UE_LOG(LogTemp, Log, TEXT("📹 Duration: %.2f seconds"), Duration);
            UE_LOG(LogTemp, Log, TEXT("📹 Time range: %.2fs ~ %.2fs"),
                SwingFrames[0].Timestamp,
                SwingFrames.Last().Timestamp);
        }
    }

    // 쿨다운 체크 (너무 빠른 연속 녹화 방지)
    float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LastSwingRecordingTime < SwingRecordingCooldown)
    {
        float Remaining = SwingRecordingCooldown - (CurrentTime - LastSwingRecordingTime);
        UE_LOG(LogTemp, Warning, TEXT("⏱️ Recording cooldown active (%.2f sec remaining)"), Remaining);
        UE_LOG(LogTemp, Warning, TEXT("⏸️ Please wait before recording again"));
        return;
    }
    LastSwingRecordingTime = CurrentTime;


    return;

}

// =============================================================================
// 8️⃣ ✨ CanCallPlayerShot (조건 검증)
// =============================================================================

bool AGolfPlayerController::CanCallPlayerShot() const
{
    // 1. 현재 플레이어 가져오기
    AGolfPlayer* CurrentPlayer = GetCurrentGolfPlayer();
    if (!CurrentPlayer)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Current player not found"));
        return false;
    }

    // 2. 기본 샷 실행 조건 체크
    if (!CanExecuteShot())
    {
        FString Reason = GetShotBlockReason();
        UE_LOG(LogTemp, Warning, TEXT("⛔ Cannot execute shot: %s"), *Reason);
        return false;
    }

    // 3. 공이 움직이는 중인지 확인
    if (IsBallMoving())
    {
        UE_LOG(LogTemp, Warning, TEXT("⛔ Ball is still moving"));
        UE_LOG(LogTemp, Warning, TEXT("   Wait for ball to stop before recording swing"));
        return false;
    }

    // 4. 샷이 이미 진행 중인지 확인
    if (bShotInProgress)
    {
        UE_LOG(LogTemp, Warning, TEXT("⛔ Shot already in progress"));
        return false;
    }

    // 5. 훈련 모드 체크 (선택사항)
    if (IsInTrainingMode())
    {
        if (!CanExecuteShot_TrainingMode())
        {
            UE_LOG(LogTemp, Warning, TEXT("⛔ Training mode conditions not met"));
            return false;
        }
    }

    UE_LOG(LogTemp, Log, TEXT("✅ All conditions met - ready to call Player_shot"));
    return true;
}


// =============================================================================
// 9️⃣ ✨✨✨ CallPlayerShot (플레이어 스테이트 함수 호출) ✨✨✨
// =============================================================================

void AGolfPlayerController::CallPlayerShot()
{
    UE_LOG(LogTemp, Warning, TEXT(""));
    UE_LOG(LogTemp, Warning, TEXT("⛳ =========================================="));
    UE_LOG(LogTemp, Warning, TEXT("⛳ CALLING PLAYER_SHOT"));
    UE_LOG(LogTemp, Warning, TEXT("⛳ =========================================="));

    // 현재 플레이어 가져오기
    AGolfPlayer* CurrentPlayer = GetCurrentGolfPlayer();
    if (!CurrentPlayer)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed: Current player is null"));
        return;
    }

    // 현재 샷 정보 로깅
    UE_LOG(LogTemp, Log, TEXT("📊 Shot Parameters:"));
    UE_LOG(LogTemp, Log, TEXT("   Power: %.1f"), ShotPower);
    UE_LOG(LogTemp, Log, TEXT("   Pitch Angle: %.1f°"), ShotPitchAngle);
    UE_LOG(LogTemp, Log, TEXT("   Yaw Angle: %.1f°"), ShotYawAngle);
    UE_LOG(LogTemp, Log, TEXT("   Direction: %s"), *AimDirection.ToString());

    // ✅✅✅ 플레이어 스테이트의 Player_shot 호출 ✅✅✅
   // CurrentPlayer->Player_shot();

    UE_LOG(LogTemp, Warning, TEXT("✅ Player_shot called successfully"));
    UE_LOG(LogTemp, Warning, TEXT("⛳ =========================================="));
    UE_LOG(LogTemp, Warning, TEXT(""));

    // 추가 처리 (선택사항)
    // 예: 샷 통계 기록, 사운드 재생 등
}

// =============================================================================
// 🔟 PlaySwingReplayDelayed (리플레이 재생)
// =============================================================================

void AGolfPlayerController::PlaySwingReplayDelayed()
{
    if (!WebcamCaptureActor)
    {
        return;
    }
    //else
    //{
    WebcamCaptureActor->SettingPlaySwingClip();
    //}

    // 기존 타이머 제거
    if (GetWorld()->GetTimerManager().IsTimerActive(SwingReplayTimer))
    {
        GetWorld()->GetTimerManager().ClearTimer(SwingReplayTimer);
    }



    // 지연 후 리플레이 재생
    GetWorld()->GetTimerManager().SetTimer(
        SwingReplayTimer,
        [this]()
        {
            if (WebcamCaptureActor && WebcamCaptureActor->IsValidLowLevel())
            {
                WebcamCaptureActor->PlayLastRecordedShot();
                UE_LOG(LogTemp, Log, TEXT("📹 Playing swing replay (%.1fs delay)"), SwingReplayDelay);
            }
        },
        SwingReplayDelay,
        false
    );
}


// =============================================================================
// 1️⃣1️⃣ 추가 제어 함수들
// =============================================================================

void AGolfPlayerController::StopSwingRecording()
{
    if (WebcamCaptureActor && WebcamCaptureActor->IsValidLowLevel())
    {
        WebcamCaptureActor->StopCapture();
        WebcamCaptureActor->HideVideoWidget();
        UE_LOG(LogTemp, Log, TEXT("📹 Swing recording stopped"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ WebcamCaptureActor not found"));
    }
}

void AGolfPlayerController::RestartSwingRecording()
{
    if (WebcamCaptureActor && WebcamCaptureActor->IsValidLowLevel())
    {
        WebcamCaptureActor->StartCapture();
        WebcamCaptureActor->ShowVideoWidget(true);
        UE_LOG(LogTemp, Log, TEXT("📹 Swing recording restarted"));
    }
    else
    {
        // 초기화되지 않은 경우 새로 초기화
        InitializeSwingRecording();
    }
}

void AGolfPlayerController::PlayLastSwingReplay()
{
    if (!bEnableVideoSaving)
    {
        UE_LOG(LogTemp, Warning, TEXT("📹 Video Saving Disabled - Skipping swing recording"));
        return;
    }
    if (!WebcamCaptureActor || !IsValid(WebcamCaptureActor))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ WebcamCaptureActor not valid"));
        return;
    }

    if (!WebcamCaptureActor->VideoWidget || !IsValid(WebcamCaptureActor->VideoWidget))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ VideoWidget not valid"));
        return;
    }

    // ✅ 1. WebcamCapture 참조 설정 (필수!)
    WebcamCaptureActor->VideoWidget->WebcamCaptureRef = WebcamCaptureActor;
    UE_LOG(LogTemp, Log, TEXT("✅ WebcamCaptureRef assigned"));

    // ✅ 2. MediaTexture 체크
    if (!WebcamCaptureActor->MediaTexture || !IsValid(WebcamCaptureActor->MediaTexture))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ MediaTexture not valid"));
        return;
    }

    if (WebcamCaptureActor->MediaTexture->GetResource() == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ MediaTexture Resource not initialized yet"));
        // ✅ Resource 업데이트 시도
        WebcamCaptureActor->MediaTexture->UpdateResource();
        UE_LOG(LogTemp, Log, TEXT("📹 MediaTexture resource updated"));
    }

    // ✅ 3. MediaPlayer 상태 확인
    if (!WebcamCaptureActor->MediaPlayer || !IsValid(WebcamCaptureActor->MediaPlayer))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ MediaPlayer not valid"));
        return;
    }

    if (!WebcamCaptureActor->MediaPlayer->IsPlaying())
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ MediaPlayer not playing, starting playback..."));
        WebcamCaptureActor->MediaPlayer->Play();
    }

    //WebcamCaptureActor->VideoWidget->SetVisibility(ESlateVisibility::Visible);

    PlaySwingReplayDelayed();
    UE_LOG(LogTemp, Log, TEXT("📹 Playing last swing replay (manual)"));
}

bool AGolfPlayerController::IsSwingRecordingActive() const
{
    if (!WebcamCaptureActor || !WebcamCaptureActor->IsValidLowLevel())
    {
        return false;
    }

    // WebcamCapture의 캡처 상태 반환
    return true;  // 실제로는 WebcamCapture의 bIsCapturing 상태 체크
}

void AGolfPlayerController::ShowSwingVideoWidget()
{
    if (!bEnableVideoSaving)
    {
        UE_LOG(LogTemp, Warning, TEXT("📹 Video Saving Disabled - Skipping swing recording"));
        return;
    }

    if (!bEnableVideoSaving)
    {
        UE_LOG(LogTemp, Warning, TEXT("📹 Video Saving Disabled - Skipping swing recording"));
        return;
    }

    if (!WebcamCaptureActor || !IsValid(WebcamCaptureActor))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ WebcamCaptureActor not valid"));
        return;
    }

    if (!WebcamCaptureActor->VideoWidget || !IsValid(WebcamCaptureActor->VideoWidget))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ VideoWidget not valid"));
        return;
    }

    // ✅ 1. WebcamCapture 참조 설정 (필수!)
    WebcamCaptureActor->VideoWidget->WebcamCaptureRef = WebcamCaptureActor;
    UE_LOG(LogTemp, Log, TEXT("✅ WebcamCaptureRef assigned"));

    // ✅ 2. MediaTexture 체크
    if (!WebcamCaptureActor->MediaTexture || !IsValid(WebcamCaptureActor->MediaTexture))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ MediaTexture not valid"));
        return;
    }

    if (WebcamCaptureActor->MediaTexture->GetResource() == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ MediaTexture Resource not initialized yet"));
        // ✅ Resource 업데이트 시도
        WebcamCaptureActor->MediaTexture->UpdateResource();
        UE_LOG(LogTemp, Log, TEXT("📹 MediaTexture resource updated"));
    }

    // ✅ 3. MediaPlayer 상태 확인
    if (!WebcamCaptureActor->MediaPlayer || !IsValid(WebcamCaptureActor->MediaPlayer))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ MediaPlayer not valid"));
        return;
    }

    if (!WebcamCaptureActor->MediaPlayer->IsPlaying())
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ MediaPlayer not playing, starting playback..."));
        WebcamCaptureActor->MediaPlayer->Play();
    }

    //WebcamCaptureActor->VideoWidget->SetVisibility(ESlateVisibility::Visible);

    PlaySwingReplayDelayed();

    UE_LOG(LogTemp, Log, TEXT("✅ VideoWidget shown with Swing mode"));


    // 이전 타이머 정리
    if (GetWorld()->GetTimerManager().IsTimerActive(HideSwingVideoWidgetTimer))
    {
        GetWorld()->GetTimerManager().ClearTimer(HideSwingVideoWidgetTimer);
    }

    // 5초 후 자동 숨김
    GetWorld()->GetTimerManager().SetTimer(
        HideSwingVideoWidgetTimer,
        this,
        &AGolfPlayerController::AutoHideSwingVideoWidget,
        5.5f,
        false
    );
}
// 저장 동영상 뛰우기
void AGolfPlayerController::ShowSwingMovieWidget()
{
    if (!bEnableVideoSaving)
    {
        UE_LOG(LogTemp, Warning, TEXT("📹 Video Saving Disabled - Skipping swing recording"));
        return;
    }

    if (!WebcamCaptureActor || !IsValid(WebcamCaptureActor))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ WebcamCaptureActor not valid"));
        return;
    }

    if (!WebcamCaptureActor->VideoWidget || !IsValid(WebcamCaptureActor->VideoWidget))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ VideoWidget not valid"));
        return;
    }

    // ✅ 1. WebcamCapture 참조 설정 (필수!)
    WebcamCaptureActor->VideoWidget->WebcamCaptureRef = WebcamCaptureActor;
    UE_LOG(LogTemp, Log, TEXT("✅ WebcamCaptureRef assigned"));

    // ✅ 2. MediaTexture 체크
    if (!WebcamCaptureActor->MediaTexture || !IsValid(WebcamCaptureActor->MediaTexture))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ MediaTexture not valid"));
        return;
    }

    if (WebcamCaptureActor->MediaTexture->GetResource() == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ MediaTexture Resource not initialized yet"));
        // ✅ Resource 업데이트 시도
        WebcamCaptureActor->MediaTexture->UpdateResource();
        UE_LOG(LogTemp, Log, TEXT("📹 MediaTexture resource updated"));
    }

    // ✅ 3. MediaPlayer 상태 확인
    if (!WebcamCaptureActor->MediaPlayer || !IsValid(WebcamCaptureActor->MediaPlayer))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ MediaPlayer not valid"));
        return;
    }

    if (!WebcamCaptureActor->MediaPlayer->IsPlaying())
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ MediaPlayer not playing, starting playback..."));
        WebcamCaptureActor->MediaPlayer->Play();
    }

    //WebcamCaptureActor->VideoWidget->SetVisibility(ESlateVisibility::Visible);

    PlaySwingReplayDelayed();

}

void AGolfPlayerController::ToggleVideoSaving(bool bEnable)
{
    bEnableVideoSaving = bEnable;
    UE_LOG(LogTemp, Log, TEXT("📹 Video Saving %s"), bEnable ? TEXT("ENABLED") : TEXT("DISABLED"));

    // ⭐ 옵션: WebcamCaptureActor가 존재하면 캡처 상태도 업데이트
    if (WebcamCaptureActor && IsValid(WebcamCaptureActor))
    {
        if (bEnable)
        {
            WebcamCaptureActor->StartCapture();
        }
        else
        {
            WebcamCaptureActor->StopCapture();
        }
    }
}

void AGolfPlayerController::SetVideoSavingEnabled(bool bEnable)
{
    bEnableVideoSaving = bEnable;
    UE_LOG(LogTemp, Log, TEXT("📹 Video Saving set to: %s"), bEnable ? TEXT("ENABLED") : TEXT("DISABLED"));
}


void  AGolfPlayerController::SettingSimpleBall()
{

    UE_LOG(LogTemp, Log, TEXT(" ===== StartCapture -- TRUE"));

    ToggleVideoSaving(true);


}

void  AGolfPlayerController::SettingComplexBall()
{
    UE_LOG(LogTemp, Log, TEXT(" ===== StopCapture -------"));


    ToggleVideoSaving(false);

}


void AGolfPlayerController::WaitForCaptureReady()
{
    if (!WebcamCaptureActor || !IsValid(WebcamCaptureActor))
    {
        return;
    }

    int32 BufferedFrames = WebcamCaptureActor->VideoBufferComponent->GetBufferedFrameCount();

    // ✅ 최소 30프레임(1초) 필요
    if (BufferedFrames < 30)
    {
        UE_LOG(LogTemp, Log, TEXT("⏳ Waiting for buffer... (%d/30)"), BufferedFrames);

        FTimerHandle RetryTimer;
        GetWorld()->GetTimerManager().SetTimer(
            RetryTimer,
            [this]() {
                TriggerSwingRecording();
            },
            0.1f,
            false
        );
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("✅ Capture ready! (%d frames)"), BufferedFrames);
}

bool AGolfPlayerController::IsCaptureReady() const
{
    if (!WebcamCaptureActor || !IsValid(WebcamCaptureActor))
    {
        return false;
    }

    int32 BufferedFrames = WebcamCaptureActor->VideoBufferComponent->GetBufferedFrameCount();
    return BufferedFrames >= 30;  // 최소 1초 필요
}


void AGolfPlayerController::HideSwingVideoWidget()
{
    if (!WebcamCaptureActor) return;

    if (GetWorld()->GetTimerManager().IsTimerActive(HideSwingVideoWidgetTimer))
    {
        GetWorld()->GetTimerManager().ClearTimer(HideSwingVideoWidgetTimer);
    }

    WebcamCaptureActor->HideVideoWidget();
}

void AGolfPlayerController::AutoHideSwingVideoWidget()
{
    if (WebcamCaptureActor)
    {
        WebcamCaptureActor->ShowVideoWidget(false);
        UE_LOG(LogTemp, Warning, TEXT("✅ SwingVideoWidget hidden (auto)"));
    }
}



void AGolfPlayerController::BeginSwingRecording()
{
    if (WebcamCaptureActor)
    {
        WebcamCaptureActor->StartCaptureForSwing();
    }
}

void AGolfPlayerController::EndSwingRecording()
{
    if (WebcamCaptureActor)
    {
        WebcamCaptureActor->StopCaptureAfterShot();
    }
}

void AGolfPlayerController::PrepareNextSwing()
{
    if (WebcamCaptureActor)
    {
        WebcamCaptureActor->ResumeCaptureForNextSwing();
    }
}

// MyPlayerController.cpp
void AGolfPlayerController::OnShotDetected(float ShotTime)
{
    UE_LOG(LogTemp, Warning, TEXT("⛳ Shot detected at %.2f"), ShotTime);

    if (WebcamCapture)
    {
        // ✅ 샷 기록 및 저장
        WebcamCapture->TriggerShotRecordingAtTime(ShotTime);
        // → 내부에서 SaveSwingClipToDisk() 호출
        // → 3초 후 PlaySwingClipFromPath() 호출
    }
}

void AGolfPlayerController::OnClipFinished()
{
    UE_LOG(LogTemp, Warning, TEXT("✅ Clip playback finished"));



    // ✅ LiveFeed로 복귀
    if (VideoWidget)
    {
        VideoWidget->SwitchToLiveFeed();
    }

    // ✅ 캡처 재개 (이미 타이머로 처리됨)
}