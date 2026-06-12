#include "CameraManager.h"
#include "Camera/CameraComponent.h"
#include "GolfBall.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "InGameMode.h" // CachedGameMode 접근을 위해 필요
#include "AimActor.h"
#include "GolfPlayerController.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"
#include "Components/SphereComponent.h"  // 이 줄 추가
#include "Engine/World.h" // UEnum::GetValueAsString 등을 위해 필요
#include "GolfPlayerManager.h" // CachedPlayerManager 접근을 위해 필요
#include "ParkDayProfiling.h"


static constexpr float HOLE_CUP_ALIGN_DISTANCE_THRESHOLD = 7000.0f; // 70 meters
ACameraManager::ACameraManager()
{
    PrimaryActorTick.bCanEverTick = true;
    // ⭐ 로그 카테고리 (선택적: 필요하다면 GameMode.cpp처럼 별도 정의)
    // DEFINE_LOG_CATEGORY_STATIC(LogCameraManager, Log, All);

    // Create camera component
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    RootComponent = Camera;
    Camera->SetFieldOfView(DEFAULT_FOV);

    //CameraCollision = CreateDefaultSubobject<USphereComponent>(TEXT("CameraCollision"));
    //CameraCollision->SetupAttachment(Camera);
    //CameraCollision->SetSphereRadius(50.0f);
    //CameraCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    //CameraCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
    //CameraCollision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);

    // Initialize with ready mode settings
    CameraMode = ECameraMode::Ready;
    ApplyCameraModeSettings();

    // 동기화 초기화
    LastKnownBallState = EBallState::Ball_Init;
    LastSyncTime = 0.0f;
    bIsSyncing = false;

    PlayerController = nullptr;

    UE_LOG(LogTemp, Log, TEXT("CameraManager: Initialized with sync system"));

    // 물리 추적 변수 초기화
    LastBallVelocity = FVector::ZeroVector;
    LastBallSpeed = 0.0f;
    LastMoveDirection = FVector(1, 0, 0);
    ShotWatchTime = 0.0f;
    bWasMovingLastFrame = false;

    InitialFollowingSpeed = 0.0f;
    FollowingWaitTime = 0.0f;
    bIsWaiting = false;

    // Ready 상태에서 고정 모드 사용 여부 초기화
    bUseFixedModeInReady = false;

    // ⭐ 5미터 내 샷 시 카메라 고정 관련 변수 초기화
    bEnableCloseToHoleShotFixed = true;
    bIsCameraFixedForCloseShot = false;
    CloseShotFixedLocation = FVector::ZeroVector;
    CloseShotFixedRotation = FRotator::ZeroRotator;

    // ⭐ 캐시 변수 초기화
    CachedGameMode = nullptr;
    CachedPlayerManager = nullptr;


    // ============================================================================
    // ⭐ 움직임 개선: 기본 속도를 약간 낮춰 전반적으로 부드럽게 만듭니다.
    // 이 값들은 이제 공이 빠를 때의 '최대' 속도로 사용됩니다.
    // ============================================================================
    InterpSpeed = 2.8f;
    RotationInterpSpeed = 4.5f;

    // ⭐ 샷 지연 기능: 변수를 초기화합니다.
    bIsWaitingForFollowDelay = false;
    FollowDelayTimer = 0.0f;

    FixedCameraLocation = FVector(500.0f, 0.0f, 200.0f); // (0,0,0) 대신 안전한 기본값
    FixedCameraRotation = FRotator(-30.0f, 0.0f, 0.0f);
}

void ACameraManager::BeginPlay()
{
    Super::BeginPlay();
    // ⭐ GameMode 및 PlayerManager 캐시
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        if (AGolfPlayerController* GolfPC = Cast<AGolfPlayerController>(PC))
        {
            CachedGameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
            if (CachedGameMode)
            {
                CachedPlayerManager = CachedGameMode->PlayerManager;

                // ⭐ 새로 추가: GameOptionInfo.Camera_Mode 체크
                CheckAndApplyCameraModeOption();
            }
            SetPlayerController(GolfPC);
            UE_LOG(LogTemp, Log, TEXT("CameraManager: PlayerController set"));
        }
    }
}

// ⭐ 새로 추가: 카메라 모드 옵션을 체크하고 적용하는 함수
void ACameraManager::CheckAndApplyCameraModeOption()
{
    if (!CachedGameMode)
    {
        UE_LOG(LogTemp, Warning, TEXT("CameraManager: CachedGameMode is null, cannot check Camera_Mode option"));
        return;
    }

    int32 CameraModeOption = CachedGameMode->GameInfo.GameOptions.Camera_Mode;

    switch (CameraModeOption)
    {
    case 0:
        // 기본 카메라 모드 (완전 자동 전환)
        bUsePartialFixedMode = false;
        bUseFixedModeInReady = false;
        UE_LOG(LogTemp, Log, TEXT("📷 Camera Mode Option: Auto (Dynamic camera transitions for all modes)"));
        break;

    case 1:
        // 부분 고정 카메라 모드 (Ready는 자유, Flying/Following/Stop은 고정)
        bUsePartialFixedMode = true;
        bUseFixedModeInReady = false; // Ready 모드는 자유 카메라 유지

        UE_LOG(LogTemp, Log, TEXT("📷 Camera Mode Option: Partial Fixed (Ready=Free, Flying/Following/Stop=Fixed)"));
        break;

    default:
        // 알 수 없는 값은 기본 모드로 처리
        bUsePartialFixedMode = false;
        bUseFixedModeInReady = false;
        UE_LOG(LogTemp, Warning, TEXT("📷 Unknown Camera Mode Option: %d, using default auto mode"), CameraModeOption);
        break;
    }

    // 화면에 디버그 메시지 표시
#if WITH_EDITOR
    if (GEngine)
    {
        FString ModeText = (CameraModeOption == 1) ? TEXT("PARTIAL FIXED") : TEXT("AUTO");
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan,
            FString::Printf(TEXT("Camera Mode: %s"), *ModeText));
    }
#endif
}

AActor* ACameraManager::SpawnInFrontOfCamera(UWorld* World, TSubclassOf<AActor> ClassToSpawn, float Distance)
{
    if (!World || !*ClassToSpawn) return nullptr;

    APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
    if (!PC) return nullptr;

    FVector CamLoc; FRotator CamRot;
    PC->GetPlayerViewPoint(CamLoc, CamRot);            // 카메라(뷰포인트) 위치/회전
    const FVector Forward = CamRot.Vector();

    FVector SpawnLoc = CamLoc + Forward * Distance;     // 카메라 정면 Distance 앞
    FActorSpawnParameters Params;
    Params.Owner = PC->GetPawn();
    Params.Instigator = PC->GetPawn<APawn>();
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    return World->SpawnActor<AActor>(ClassToSpawn, SpawnLoc, CamRot, Params);
}


void ACameraManager::SetPlayerController(AGolfPlayerController* NewController)
{
    PlayerController = NewController;
    UE_LOG(LogTemp, Log, TEXT("CameraManager: PlayerController set"));
}

void ACameraManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearAllTimers();
    Super::EndPlay(EndPlayReason);
    UE_LOG(LogTemp, Log, TEXT("CameraManager: EndPlay - All timers cleared"));
}

void ACameraManager::Tick(float DeltaTime)
{
    SCOPE_CYCLE_COUNTER(STAT_CameraTick);
    Super::Tick(DeltaTime);

    // ===== 성능 최적화: 프레임 단위 캐시 리셋 =====
    // UpdateFollowingCamera, UpdateReadyCamera 등에서 재사용
    bFrameGroundCacheValid = false;
    bReadyPCCacheValid = false;
    CachedReadyPC = nullptr;

    // Handle initialization delay
    if (!bIsInitialized)
    {
        InitializationDelay -= DeltaTime;
        if (InitializationDelay <= 0)
        {
            bIsInitialized = true;
            UE_LOG(LogTemp, Log, TEXT("CameraManager: Initialization completed"));
        }
        return;
    }



    // ⭐ 캐시된 GameMode 및 PlayerManager 유효성 검사 추가
    if (!IsValidTargetBall() || !CachedGameMode || !CachedPlayerManager)
    {
        return;
    }

    // 볼 물리 상태 추적
  //  UpdateBallPhysicsTracking();

    // 자동 모드 전환 체크
  //  CheckForAutomaticModeTransitionImproved();

    // 카메라 전환 카운트다운 업데이트
    if (CameraMode == ECameraMode::Following)
    {
        //     UpdateCameraTransitionCountdown();
    }

    // 모드별 카메라 업데이트
    switch (CameraMode)
    {
    case ECameraMode::Ready:
        UpdateReadyCamera(DeltaTime);
        break;
    case ECameraMode::Flying:
        UpdateFlyingCamera(DeltaTime);
        break;
    case ECameraMode::Following:
        UpdateFollowingCamera(DeltaTime);
        break;
    case ECameraMode::Stop:
        UpdateStopCamera(DeltaTime);
        break;
    case ECameraMode::Fixed:
        UpdateFixedCamera(DeltaTime);
        break;
    case ECameraMode::Tour:
        UpdateTourCamera(DeltaTime);
        break; // ⭐ [추가]
    }
    return;

    // 동기화 체크
    static float LastSyncCheckTime = 0.0f;
    float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LastSyncCheckTime > 1.0f) // 1초마다 동기화 체크
    {
        if (IsValidTargetBall())
        {
            EBallState BallState = TargetBall->GetBallState();
            ECameraMode CurrentMode = CameraMode;
            bool bShouldBeReady = (BallState == EBallState::Ball_Ready);
            bool bCameraIsReadyOrFixed = (CurrentMode == ECameraMode::Ready || (CurrentMode == ECameraMode::Fixed && bUseFixedModeInReady)); // Ready or Fixed in Ready state
            if (bShouldBeReady != bCameraIsReadyOrFixed)
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("⚠️ SYNC ISSUE: Ball=%s, Camera=%s (BallSpeed: %.1f)"), // 로그에 공 속도 추가
                    *UEnum::GetValueAsString(BallState),
                    *UEnum::GetValueAsString(CurrentMode),
                    TargetBall->GetBallSpeed()); // 공 속도 정보 추가
                //생성자에서 사용 금지
                //if (GEngine)
                //{
                //    GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red,
                //        FString::Printf(TEXT("🔴 State Desync: Ball=%s, Cam=%s"),
                //            *UEnum::GetValueAsString(BallState).Right(4),
                //            *UEnum::GetValueAsString(CurrentMode).Right(4)));
                //}
            }
        }
        LastSyncCheckTime = CurrentTime;
    }
}

void ACameraManager::UpdateBallPhysicsTracking()
{ // 공의 물리 상태 추적
    if (!IsValidTargetBall()) return;

    FVector CurrentVelocity = TargetBall->GetBallVelocity();
    float CurrentSpeed = TargetBall->GetBallSpeed();
    EBallState BallState = TargetBall->GetBallState();

    bool bIsMovingNow = CurrentSpeed > MIN_BALL_SPEED_THRESHOLD;

    if (CurrentSpeed > LastBallSpeed + 100.0f && LastBallSpeed < MIN_BALL_SPEED_THRESHOLD)
    {
        ShotWatchTime = 0.0f;
        UE_LOG(LogTemp, Log, TEXT("CameraManager: Shot detected, Speed: %.1f cm/s"), CurrentSpeed);
    }

    if (CurrentSpeed > MIN_BALL_SPEED_THRESHOLD)
    {
        LastMoveDirection = CurrentVelocity.GetSafeNormal();
    }

    LastBallVelocity = CurrentVelocity;
    LastBallSpeed = CurrentSpeed;
    bWasMovingLastFrame = bIsMovingNow;

    if (ShotWatchTime < SHOT_WATCH_DURATION)
    {
        ShotWatchTime += GetWorld()->GetDeltaSeconds();
    }
}

void ACameraManager::CheckForAutomaticModeTransition()
{ // 자동 카메라 모드 전환
    if (!IsValidTargetBall()) return;

    // ⭐ 새로 추가: 강제 카메라 모드가 설정된 경우 자동 전환 비활성화
    if (bUseForcedCameraMode)
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("📷 Automatic mode transition disabled - forced camera mode active"));
        return;
    }


    // Fixed 모드이거나 Ready 상태에서 고정 모드를 사용하면 자동 전환 비활성화
    if (CameraMode == ECameraMode::Fixed || (TargetBall->GetBallState() == EBallState::Ball_Ready && bUseFixedModeInReady))
        return;

    // ⭐ 5미터 내 샷으로 카메라가 고정된 경우 자동 전환 비활성화
    if (bIsCameraFixedForCloseShot)
        return;

    EBallState BallState = TargetBall->GetBallState();
    float CurrentSpeed = TargetBall->GetBallSpeed();

    switch (CameraMode)
    {
    case ECameraMode::Ready:
        if (BallState == EBallState::Ball_Fly && CurrentSpeed > MIN_BALL_SPEED_THRESHOLD)
        {
            ChangeCameraMode(ECameraMode::Flying);
        }
        break;
    case ECameraMode::Flying:
        if (BallState == EBallState::Ball_Bound || BallState == EBallState::Ball_Rolling || CurrentSpeed < TRANSITION_SPEED_THRESHOLD) // 바운드 또는 속도 임계값 이하
        {
            ChangeCameraMode(ECameraMode::Following);
        }
        break;
    case ECameraMode::Following:
        if (BallState == EBallState::Ball_Stop || CurrentSpeed < 1.f)
        {
            if (!bBallStoppedForCamera)
            { // 카메라가 멈춘 상태가 아니라면
                bBallStoppedForCamera = true;
                BallStopTime = GetWorld()->GetTimeSeconds();
                FrozenCameraLocation = GetActorLocation();
                FrozenCameraRotation = GetActorRotation();
                UE_LOG(LogTemp, Log, TEXT("CameraManager: Ball stopped, camera frozen"));
            }

            if (!GetWorld()->GetTimerManager().IsTimerActive(ModeTransitionTimer))
            { // 타이머가 활성화되어 있지 않다면
                GetWorld()->GetTimerManager().SetTimer(
                    ModeTransitionTimer,
                    [this]() {
                        ChangeCameraMode(ECameraMode::Stop);
                    },
                    CameraFreezeTime,
                    false
                );
                UE_LOG(LogTemp, Log, TEXT("CameraManager: %d-second freeze timer set (Following -> Stop)"), (int32)CameraFreezeTime);
            }
        }
        else
        {
            if (bBallStoppedForCamera)
            { // 카메라가 멈춰있었다면
                bBallStoppedForCamera = false;
                BallStopTime = 0.0f;
                if (GetWorld()->GetTimerManager().IsTimerActive(ModeTransitionTimer))
                {
                    GetWorld()->GetTimerManager().ClearTimer(ModeTransitionTimer);
                    UE_LOG(LogTemp, Log, TEXT("CameraManager: Ball started moving again, freeze cancelled"));
                }
            }
        }
        break;



    case ECameraMode::Stop:
        // 자동 전환은 타이머에서 처리
        break;
    }
}

void ACameraManager::ChangeCameraMode(ECameraMode NewMode) // 카메라 모드 변경
{
    // 부분 고정 모드에서 Flying/Following/Stop 모드를 Fixed로 변환
    if (bUsePartialFixedMode)
    {
        if (NewMode == ECameraMode::Flying || NewMode == ECameraMode::Following || NewMode == ECameraMode::Stop)
        {
            UE_LOG(LogTemp, Log, TEXT("📷 Partial Fixed Mode: Converting %s to Fixed mode"),
                *UEnum::GetValueAsString(NewMode));

            // ⭐ Ready 모드에서 전환될 때만 현재 위치를 Fixed 위치로 저장
            if (CameraMode == ECameraMode::Ready)
            {
                FixedCameraLocation = GetActorLocation();
                FixedCameraRotation = Camera->GetComponentRotation(); // 카메라 컴포넌트의 월드 회전값 사용
                UE_LOG(LogTemp, Log, TEXT("📷 Ready position saved for Fixed mode: %s, rotation: %s"),
                    *FixedCameraLocation.ToString(), *FixedCameraRotation.ToString());
            }

            NewMode = ECameraMode::Fixed;
        }
    }

    if (bIsCameraFixedForCloseShot)
    {
        UE_LOG(LogTemp, Log, TEXT("CameraManager: Camera mode change blocked - fixed for close shot"));
        return;
    }

    if (CameraMode == NewMode)
        return;

    // 나머지 기존 코드는 동일...
    // Stop 모드에서 나갈 때 ExitStopMode 호출
    if (CameraMode == ECameraMode::Stop && NewMode != ECameraMode::Stop)
    {
        ExitStopMode();
    }

    // 타이머 클리어 로직...
    if (GetWorld())
    {
        FTimerManager& TimerManager = GetWorld()->GetTimerManager();
        if (TimerManager.IsTimerActive(ModeTransitionTimer))
        {
            TimerManager.ClearTimer(ModeTransitionTimer);
            UE_LOG(LogTemp, Log, TEXT("CameraManager: Mode transition timer cleared during mode change"));
        }
        if (TimerManager.IsTimerActive(StopModeTimer))
        {
            TimerManager.ClearTimer(StopModeTimer);
            UE_LOG(LogTemp, Log, TEXT("CameraManager: StopModeTimer cleared during mode change"));
        }
    }

    // Flying/Following 모드 초기화 로직...
    if (NewMode == ECameraMode::Flying && IsValidTargetBall())
    {
        InitialFollowingSpeed = TargetBall->GetBallSpeed();
        FollowingWaitTime = 1.5f;
        bIsWaiting = true;
        ElapsedFollowingTime = 0.0f;
        UE_LOG(LogTemp, Log, TEXT("CameraManager: InitialFollowingSpeed set to %.1f cm/s, Waiting for 1.0s"), InitialFollowingSpeed);
    }
    else if (NewMode == ECameraMode::Following && IsValidTargetBall())
    {
        InitialFollowingSpeed = TargetBall->GetBallSpeed();
        ElapsedFollowingTime = 0.0f;
        // ⭐ Following 진입 시 0.5초 대기 후 부드럽게 추적 시작
        FollowingWaitTime = 0.5f;
        bIsWaiting = true;
        UE_LOG(LogTemp, Log, TEXT("CameraManager: Following mode entered, waiting %.1fs before tracking (Speed: %.1f cm/s)"), FollowingWaitTime, InitialFollowingSpeed);
    }
    else if (NewMode != ECameraMode::Following)
    {
        bBallStoppedForCamera = false;
        BallStopTime = 0.0f;
    }

    // Ready 모드로 전환 시 5미터 내 샷 고정 해제 (Ready에서는 정상 카메라 동작)
    if (NewMode == ECameraMode::Ready && bIsCameraFixedForCloseShot)
    {
        ReleaseCloseToHoleShotFixed();
        UE_LOG(LogTemp, Log, TEXT("📷 Camera fixed released - entering Ready mode"));
    }

    ECameraMode PreviousMode = CameraMode;
    CameraMode = NewMode;
    ApplyCameraModeSettings();

    UE_LOG(LogTemp, Log, TEXT("Camera mode changed: %s -> %s (Ball Speed: %.1f cm/s)"),
        *UEnum::GetValueAsString(PreviousMode),
        *UEnum::GetValueAsString(NewMode),
        GetCurrentBallSpeed());

#if WITH_EDITOR
    if (GEngine)
    {
        FColor ModeColor = (NewMode == ECameraMode::Ready) ? FColor::Green :
            (NewMode == ECameraMode::Fixed) ? FColor::Purple : FColor::Blue;
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, ModeColor,
            FString::Printf(TEXT("Camera: %s"), *UEnum::GetValueAsString(NewMode)));
    }
#endif
}


void ACameraManager::SetTargetBall(AGolfBall* NewTargetBall) // 타겟 공 설정
{
    if (!NewTargetBall || !IsValid(NewTargetBall))
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid target ball provided to CameraManager"));
        return;
    }

    if (IsValid(TargetBall) && TargetBall != NewTargetBall)
    {
        if (TargetBall->IsInTurnTransition())
        {
            TargetBall->SkipTurnTransitionCountdown();
            UE_LOG(LogTemp, Log, TEXT("🔄 Previous ball transition skipped for camera sync"));
        }
    }

    ClearAllTimers();
    TargetBall = NewTargetBall;
    TargetBall->LinkCameraManager(this);

    LastBallVelocity = FVector::ZeroVector;
    LastBallSpeed = 0.0f;
    ShotWatchTime = 0.0f;
    bWasMovingLastFrame = false;
    CameraRotation = 0.0f;
    bBallStoppedForCamera = false;
    BallStopTime = 0.0f;

    // ⭐ 새로운 공으로 전환할 때만 5미터 내 샷 고정 해제
    // 같은 공의 턴이 계속되는 경우에는 고정 유지
    if (IsValid(TargetBall))
    {
        static AGolfBall* PreviousTargetBall = nullptr;
        if (PreviousTargetBall != TargetBall)
        {
            ReleaseCloseToHoleShotFixed();
            PreviousTargetBall = TargetBall;
            UE_LOG(LogTemp, Log, TEXT("📷 Camera fixed released - new target ball"));
        }
    }

    // 카메라 모드 동기화 로직...
    EBallState CurrentBallState = TargetBall->GetBallState();
    float CurrentBallSpeed = TargetBall->GetBallSpeed();

    if (CurrentBallState == EBallState::Ball_Ready && bUseFixedModeInReady)
    {
        ChangeCameraMode(ECameraMode::Fixed);
        UE_LOG(LogTemp, Log, TEXT("📷 Camera immediately synced to Fixed mode for Ready state"));
    }
    else if (CurrentBallState == EBallState::Ball_Ready)
    {
        ChangeCameraMode(ECameraMode::Ready);
        UE_LOG(LogTemp, Log, TEXT("📷 Camera immediately synced to Ready mode"));
    }
    else if (CurrentBallState == EBallState::Ball_Fly)
    {
        ChangeCameraMode(ECameraMode::Flying);
    }
    else if (CurrentBallState == EBallState::Ball_Bound || CurrentBallState == EBallState::Ball_Rolling || CurrentBallState == EBallState::Ball_Stop)
    {
        ChangeCameraMode(ECameraMode::Following);
    }
    else
    {
        GetWorld()->GetTimerManager().SetTimer(
            ModeTransitionTimer,
            [this]() {
                if (TargetBall && TargetBall->GetBallState() == EBallState::Ball_Ready)
                {
                    ChangeCameraMode(bUseFixedModeInReady ? ECameraMode::Fixed : ECameraMode::Ready);
                    UE_LOG(LogTemp, Log, TEXT("📷 Camera delayed sync to %s mode"),
                        bUseFixedModeInReady ? TEXT("Fixed") : TEXT("Ready"));
                }
                else
                {
                    ForceSyncWithBall();
                }
            },
            0.3f,
            false
        );
    }

    ActivateAsMainCamera();
    if (CameraMode != ECameraMode::Fixed && !bIsCameraFixedForCloseShot)
    {
        FVector BallLocation = TargetBall->GetActorLocation();
        FVector HolecupLocation = GetCurrentHolecupPosition();
        FVector TeeLocation = CachedGameMode->GetCurrentTeeLocation();
        FRotator TeeRotation = CachedGameMode->TeeRotationArray[CachedGameMode->CurrentHole - 1];
        if (CachedGameMode->CheckFirstShot() && CachedGameMode->IsStrokeMode())
        {
            UE_LOG(LogTemp, Log, TEXT("✅================SetTargetBall()  Camera target set with immediate sync"));
            const FVector  AimPoint = TeeLocation + TeeRotation.Vector() * 5000.f;
            if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
            {
                if (AGolfPlayerController* GolfPC = Cast<AGolfPlayerController>(PC))
                    GolfPC->SetAimToPosition(AimPoint);
            }

            PositionCameraForHoleView(BallLocation, AimPoint); // 카메라 위치 조정  
        }
        else
        {
            float fDist = FVector::Dist(BallLocation, HolecupLocation);
            FVector  AimPoint = TeeLocation + TeeRotation.Vector() * fDist;
            if (!CachedGameMode->CheckFirstShot())
                AimPoint = HolecupLocation;
            if (CachedGameMode->IsRangeMode())
                AimPoint = HolecupLocation;

            if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
            {
                if (AGolfPlayerController* GolfPC = Cast<AGolfPlayerController>(PC))
                    GolfPC->SetAimToPosition(AimPoint);
                PositionCameraForHoleView(BallLocation, AimPoint); // 카메라 위치 조정    UE_LOG(LogTemp, Log, TEXT("Camera reset to optimal position")); // 로그 메시지

            }

        }
    }
    else
    {
        FVector BallLocation = TargetBall->GetActorLocation();
        FVector HolecupLocation = GetCurrentHolecupPosition();
        FVector TeeLocation = CachedGameMode->GetCurrentTeeLocation();
        FRotator TeeRotation = CachedGameMode->TeeRotationArray[CachedGameMode->CurrentHole - 1];
        float fDist = FVector::Dist(BallLocation, HolecupLocation);
        FVector  AimPoint = TeeLocation + TeeRotation.Vector() * fDist;
        if (!CachedGameMode->CheckFirstShot())
            AimPoint = HolecupLocation;
        if (CachedGameMode->IsRangeMode())
            AimPoint = HolecupLocation;

        if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
        {
            if (AGolfPlayerController* GolfPC = Cast<AGolfPlayerController>(PC))
                GolfPC->SetAimToPosition(AimPoint);
            PositionCameraForHoleView(BallLocation, AimPoint); // 카메라 위치 조정    UE_LOG(LogTemp, Log, TEXT("Camera reset to optimal position")); // 로그 메시지

        }

        UE_LOG(LogTemp, Log, TEXT("✅================bIsCameraFixedForCloseShot  - Holecup Dist Near 5M"));
    }

    UE_LOG(LogTemp, Log, TEXT("✅ Camera target set with immediate sync: %s (State: %s)"),
        *TargetBall->GetName(),
        *UEnum::GetValueAsString(CurrentBallState));
}

void ACameraManager::OnBallStateChanged(AGolfBall* Ball, EBallState PreviousState, EBallState NewState)
{
    OnBallStateChangedImmediate(Ball, PreviousState, NewState);
}

void ACameraManager::OnBallStateChangedImmediate(AGolfBall* Ball, EBallState PreviousState, EBallState NewState)
{

    UE_LOG(LogTemp, Warning, TEXT("📷 [VERIFY] OnBallStateChangedImmediate RECEIVED!"));
    UE_LOG(LogTemp, Warning, TEXT("📷 [VERIFY] Ball: %p, State: %s → %s"),
        Ball,
        *UEnum::GetValueAsString(PreviousState),
        *UEnum::GetValueAsString(NewState));

    if (Ball != TargetBall)
        return;

    // 부분 고정 모드에서는 Ready 상태에서만 상태 변화 반응
    if (bUsePartialFixedMode)
    {
        // Ready 모드에서 Flying으로의 전환만 허용 (나머지는 Fixed에서 처리)
        if (CameraMode == ECameraMode::Ready && PreviousState == EBallState::Ball_Ready && NewState == EBallState::Ball_Fly)
        {
            // ⭐ Ready 위치를 Fixed 위치로 저장
            FixedCameraLocation = GetActorLocation();
            FixedCameraRotation = Camera->GetComponentRotation();
            UE_LOG(LogTemp, Log, TEXT("📷 Ready position captured for Fixed mode: %s, rotation: %s"),
                *FixedCameraLocation.ToString(), *FixedCameraRotation.ToString());

            // 홀컵까지의 거리 체크
            if (bEnableCloseToHoleShotFixed && IsCloseToHoleShot())
            {
                HandleCloseToHoleShot();
                UE_LOG(LogTemp, Log, TEXT("📷 Camera fixed for close-to-hole shot (within 5m) - will not follow ball"));
                return;
            }

            // 5미터 밖의 일반 샷인 경우 Flying 모드로 전환 (Fixed로 변환됨)
            bIsWaitingForFollowDelay = true;
            FollowDelayTimer = 1.0f;
            UE_LOG(LogTemp, Log, TEXT("Partial Fixed Mode: Shot detected, will transition to Fixed mode after delay"));
            return;
        }
        else if (CameraMode == ECameraMode::Fixed)
        {
            // Fixed 모드에서는 Ready 상태로의 복귀만 허용
            if (NewState == EBallState::Ball_Ready)
            {
                ChangeCameraMode(ECameraMode::Ready);
                UE_LOG(LogTemp, Log, TEXT("📷 Partial Fixed Mode: Returning to Ready mode"));
            }
            else
            {
                UE_LOG(LogTemp, Log, TEXT("📷 Partial Fixed Mode: Staying in Fixed mode for state %s (preserving Ready position)"),
                    *UEnum::GetValueAsString(NewState));
            }
            return;
        }
    }

    // 기존 코드 계속...
    if (IsInFixedMode())
        return;

    if (bIsSyncing)
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("Sync already in progress, skipping"));
        return;
    }

    // 샷 감지 시 카메라 고정 처리 개선
    if (PreviousState == EBallState::Ball_Ready && NewState == EBallState::Ball_Fly)
    {
        UE_LOG(LogTemp, Log, TEXT("========================================= bEnableCloseToHoleShotFixed "));
        // 홀컵까지의 거리 체크
        if (bEnableCloseToHoleShotFixed && IsCloseToHoleShot())
        {
            HandleCloseToHoleShot();
            UE_LOG(LogTemp, Log, TEXT("📷 Camera fixed for close-to-hole shot (within 5m) - will not follow ball"));
            return;
        }

        // 5미터 밖의 일반 샷인 경우 기존 지연 로직 수행
        bIsWaitingForFollowDelay = true;
        FollowDelayTimer = 1.0f;
        UE_LOG(LogTemp, Log, TEXT("Normal shot detected. Waiting 1 second before following."));
        return;
    }

    // 나머지 기존 코드들...
    if (bIsCameraFixedForCloseShot)
    {
        if (NewState == EBallState::Ball_Init || NewState == EBallState::Ball_Ready)
        {
            ReleaseCloseToHoleShotFixed();
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("📷 Camera sync blocked - fixed for close shot, ignoring state change %s -> %s"),
                *UEnum::GetValueAsString(PreviousState), *UEnum::GetValueAsString(NewState));
            return;
        }
    }

    if (NewState == EBallState::Ball_Ready && bUseFixedModeInReady && CameraMode == ECameraMode::Fixed)
    {
        UE_LOG(LogTemp, Log, TEXT("📷 Camera remains in Fixed mode for Ready state"));
        return;
    }

    if (NewState == EBallState::Ball_Init)
        return;

    bIsSyncing = true;

    float BallSpeed = Ball ? Ball->GetBallSpeed() : 0.0f;

    UE_LOG(LogTemp, Log, TEXT("📷 Camera sync: Ball %s → %s (Speed: %.1f cm/s)"),
        *UEnum::GetValueAsString(PreviousState),
        *UEnum::GetValueAsString(NewState),
        BallSpeed);

    ECameraMode RequiredMode = GetRequiredCameraModeForBallState(NewState, BallSpeed);
    if (RequiredMode != CameraMode)
    {
        ChangeCameraMode(RequiredMode);
    }

    LastKnownBallState = NewState;
    LastSyncTime = GetWorld()->GetTimeSeconds();
    bIsSyncing = false;
}

ECameraMode ACameraManager::GetRequiredCameraModeForBallState(EBallState BallState, float BallSpeed) const
{
    switch (BallState)
    {
    case EBallState::Ball_Ready:
    case EBallState::Ball_Init:
        return bUseFixedModeInReady ? ECameraMode::Fixed : ECameraMode::Ready;
    case EBallState::Ball_Fly:
        return ECameraMode::Flying;
    case EBallState::Ball_Bound:
    case EBallState::Ball_Rolling:
        return ECameraMode::Following;
    case EBallState::Ball_Stop:
        return ECameraMode::Following;
    default:
        UE_LOG(LogTemp, Warning, TEXT("Unknown ball state: %s"), *UEnum::GetValueAsString(BallState));
        return ECameraMode::Ready;
    }
}

void ACameraManager::PerformCameraSync(EBallState BallState, float BallSpeed, bool bImmediate) // 카메라 동기화 수행
{
    if (CameraMode == ECameraMode::Fixed && BallState == EBallState::Ball_Ready && bUseFixedModeInReady) // Ready 상태에서 Fixed 모드 사용 중이라면 스킵
    { // 로그 메시지
        UE_LOG(LogTemp, Log, TEXT("📷 Camera remains in Fixed mode for Ready state, sync skipped"));
        return;
    }

    // ⭐ 5미터 내 샷으로 고정된 경우 동기화 스킵
    if (bIsCameraFixedForCloseShot)
    {
        UE_LOG(LogTemp, Log, TEXT("📷 Camera sync skipped - fixed for close shot"));
        return;
    }

    ECameraMode RequiredMode = GetRequiredCameraModeForBallState(BallState, BallSpeed);
    if (RequiredMode != CameraMode) // 필요한 모드가 현재 모드와 다르면
    {
        UE_LOG(LogTemp, Log, TEXT("📷 Camera mode change: %s → %s (Ball: %s, Speed: %.1f)"),
            *UEnum::GetValueAsString(CameraMode),
            *UEnum::GetValueAsString(RequiredMode),
            *UEnum::GetValueAsString(BallState),
            BallSpeed / 100.0f);

        if (bImmediate) // 즉시 전환이라면
        {
            CameraMode = RequiredMode;
            ApplyCameraModeSettings();
#if WITH_EDITOR
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(99, 1.0f, FColor::Cyan,
                    FString::Printf(TEXT("📷 %s"), *UEnum::GetValueAsString(RequiredMode)));
            }
#endif
        }
        else // 부드러운 전환이라면
        {
            ChangeCameraMode(RequiredMode);
        }
    }
}

bool ACameraManager::IsSyncedWithBall() // 카메라가 공 상태와 동기화되었는지 확인
{
    if (!IsValid(TargetBall))
        return false; // 유효한 공이 없다면 동기화되지 않음

    if (CameraMode == ECameraMode::Fixed && TargetBall->GetBallState() == EBallState::Ball_Ready && bUseFixedModeInReady)
        return true;

    // ⭐ 5미터 내 샷으로 고정된 경우 동기화된 것으로 간주
    if (bIsCameraFixedForCloseShot)
        return true;


    EBallState CurrentBallState = TargetBall->GetBallState();
    ECameraMode CurrentCameraMode = CameraMode;
    bool bIsSynced = (CameraMode == GetRequiredCameraModeForBallState(CurrentBallState, TargetBall->GetBallSpeed())); // 캐시된 공 속도 사용

    if (!bIsSynced)
    {
        UE_LOG(LogTemp, Warning, TEXT("❌ Camera NOT synced: Ball=%s, Camera=%s, Expected=%s"),
            *UEnum::GetValueAsString(CurrentBallState),
            *UEnum::GetValueAsString(CameraMode),
            *UEnum::GetValueAsString(GetRequiredCameraModeForBallState(CurrentBallState, TargetBall->GetBallSpeed())));
    }

    return bIsSynced;
}

void ACameraManager::ForceSyncWithBall() // 강제 동기화
{
    if (!IsValid(TargetBall)) // 타겟 공이 유효하지 않다면
    { // 로그 메시지
        UE_LOG(LogTemp, Warning, TEXT("No target ball for force sync"));
        return;
    }

    if (CameraMode == ECameraMode::Fixed && TargetBall->GetBallState() == EBallState::Ball_Ready && bUseFixedModeInReady)
    {
        UE_LOG(LogTemp, Log, TEXT("Camera in Fixed mode for Ready state, sync skipped"));
        return; // Fixed 모드 유지 중이라면 스킵
    }
    // ⭐ 5미터 내 샷으로 고정된 경우 강제 동기화 스킵
    if (bIsCameraFixedForCloseShot)
    {
        UE_LOG(LogTemp, Log, TEXT("Camera fixed for close shot, force sync skipped"));
        return;
    }


    UE_LOG(LogTemp, Warning, TEXT("🔄 Force syncing camera with ball..."));
    EBallState CurrentBallState = TargetBall->GetBallState();
    float CurrentBallSpeed = TargetBall->GetBallSpeed();
    PerformCameraSync(CurrentBallState, CurrentBallSpeed, true);

#if WITH_EDITOR
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
            TEXT("📷 Camera force synced!"));
    }
#endif
}


void ACameraManager::UpdateReadyCamera(float DeltaTime) // Ready 모드 카메라 업데이트
{
    SCOPE_CYCLE_COUNTER(STAT_CameraReady);
    if (!IsValidTargetBall()) return; // 유효하지 않은 공이라면 반환

    // ⭐ 5미터 내 샷으로 카메라가 고정된 경우 고정된 위치 유지
    // ⭐ 중요: 이 조건에서 반드시 "return"으로 함수를 종료해야 함!
    if (bIsCameraFixedForCloseShot)
    {
        SetActorLocation(CloseShotFixedLocation);
        Camera->SetWorldRotation(CloseShotFixedRotation);
#if WITH_EDITOR
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(103, 0.2f, FColor::Orange,
                TEXT("Camera Fixed for Close Shot (Ready Mode)"));
#endif
        return;
    }

    // ============================================================
    // 이 섹션은 bIsCameraFixedForCloseShot이 FALSE일 때만 실행됨
    // ============================================================

    if (bUseFixedModeInReady)
    {
        ChangeCameraMode(ECameraMode::Fixed);
        return;
    }

    FVector BallLocation = TargetBall->GetActorLocation();
    FVector HolecupLocation = GetCurrentHolecupPosition();
    FVector TargetCameraPos = CalculateOptimalCameraPosition(BallLocation, HolecupLocation);
    FVector LookDirection = (BallLocation - TargetCameraPos).GetSafeNormal();
    FRotator TargetRotation = FRotationMatrix::MakeFromX(LookDirection).Rotator();

    SmoothCameraTransition(TargetCameraPos, TargetRotation, DeltaTime);
    FVector TeeLocation = CachedGameMode->GetCurrentTeeLocation();
    FRotator TeeRotation = FRotator::ZeroRotator;

    float fDist = FVector::Dist(BallLocation, HolecupLocation);

    if (CachedGameMode->TeeRotationArray.IsValidIndex(CachedGameMode->CurrentHole - 1))
    {
        TeeRotation = CachedGameMode->TeeRotationArray[CachedGameMode->CurrentHole - 1];
    }

    // ✅ 최적화: GetPlayerController + Cast 3회 중복 → 상단에서 1회만 수행
    if (!bReadyPCCacheValid)
    {
        if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
            CachedReadyPC = Cast<AGolfPlayerController>(PC);
        bReadyPCCacheValid = true;
    }

    if (CachedGameMode->IsStrokeMode())
    {
        FVector AimPoint = TeeLocation + TeeRotation.Vector() * 5000.f;
        if (CachedReadyPC && CachedReadyPC->GetAimActor())
            AimPoint = CachedReadyPC->GetAimActor()->GetActorLocation();

        AimPoint.Z = BallLocation.Z;  // 퍼팅/일반 무관하게 동일 처리
        PositionCameraForAimView(BallLocation, AimPoint);
        CachedGameMode->StrokeWidgetInstance->PositionCanvasPanelAboveHole();
    }
    else if (CachedGameMode->IsTrainingMode())
    {
        FVector AimPoint = TeeLocation + TeeRotation.Vector() * 5000.f;
        if (CachedReadyPC && CachedReadyPC->GetAimActor())
            AimPoint = CachedReadyPC->GetAimActor()->GetActorLocation();
        PositionCameraForAimView(BallLocation, AimPoint);
    }
    else if (CachedGameMode->IsRangeMode())
    {
        FVector AimPoint = TeeLocation + TeeRotation.Vector() * 5000.f;
        if (CachedReadyPC && CachedReadyPC->GetAimActor())
            AimPoint = CachedReadyPC->GetAimActor()->GetActorLocation();
       // PositionCameraForHoleView(BallLocation, AimPoint);
        PositionCameraForAimView(BallLocation, AimPoint);
    }
}



void ACameraManager::UpdateFlyingCamera(float DeltaTime)
{
    SCOPE_CYCLE_COUNTER(STAT_CameraFlying);
    if (!IsValidTargetBall())
        return;

    // ⭐ 5미터 내 샷으로 카메라가 고정된 경우 고정된 위치 유지
    if (bIsCameraFixedForCloseShot)
    {
        SetActorLocation(CloseShotFixedLocation);
        Camera->SetWorldRotation(CloseShotFixedRotation);

#if WITH_EDITOR
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(104, 0.2f, FColor::Orange,
                TEXT("Camera Fixed for Close Shot (Flying Mode)"));
#endif
        return;
    }

    // ⭐ 1초 대기 로직 추가
    if (bIsWaiting)
    {
        // ✅ 여기에 추가: 대기 중에도 close shot 고정 체크
        if (bIsCameraFixedForCloseShot)
        {
            SetActorLocation(CloseShotFixedLocation);
            Camera->SetWorldRotation(CloseShotFixedRotation);
            return;
        }

        ElapsedFollowingTime += DeltaTime;

        if (ElapsedFollowingTime >= FollowingWaitTime)
        {
            // 1초가 지나면 대기 종료
            bIsWaiting = false;
            UE_LOG(LogTemp, Log, TEXT("CameraManager: Following wait time completed (%.1fs), now following ball"), ElapsedFollowingTime);
        }
        else
        {
            // 1초 동안 대기 중 - 현재 위치 유지하거나 아주 천천히 이동
            float WaitProgress = ElapsedFollowingTime / FollowingWaitTime;

            // 대기 중에는 카메라를 거의 고정하되, 아주 약간의 부드러운 이동만 허용
            FVector BallLocation = TargetBall->GetActorLocation();
            FVector CurrentCameraPos = Camera->GetComponentLocation();

            // ⭐    

            // 대기 시간 디버그 표시
#if WITH_EDITOR
            if (GEngine)
            {
                int32 SecondsLeft = FMath::CeilToInt(FollowingWaitTime - ElapsedFollowingTime);
                GEngine->AddOnScreenDebugMessage(996, 0.2f, FColor::Orange,
                    FString::Printf(TEXT("Following in %d second(s)..."), SecondsLeft));
            }
#endif

            UE_LOG(LogTemp, VeryVerbose, TEXT("Following Camera: Waiting... %.1f/%.1f seconds"),
                ElapsedFollowingTime, FollowingWaitTime);
            return; // 대기 중이므로 여기서 함수 종료
        }
    }

    // 공이 정지한 경우 카메라 고정
    if (bBallStoppedForCamera)
    {
        // 고정된 카메라 위치 설정
        SetActorLocation(FrozenCameraLocation);
        // ⭐ 회전 제거: 정지 상태에서도 회전하지 않음
        return;
    }

    // ⭐ 1초 대기가 끝난 후 - 정상적인 Following 로직 실행
    // 공의 현재 위치 및 속도
    FVector BallLocation = TargetBall->GetActorLocation();
    FVector CurrentVelocity = TargetBall->GetBallVelocity();
    float CurrentSpeed = GetCurrentBallSpeed();

    // 이동 방향 결정
    FVector MoveDirection;
    if (CurrentSpeed > MIN_BALL_SPEED_THRESHOLD && !CurrentVelocity.IsNearlyZero())
    {
        MoveDirection = CurrentVelocity.GetSafeNormal();
        LastMoveDirection = MoveDirection;
    }
    else
    {
        MoveDirection = LastMoveDirection; // 마지막 방향 유지
    }

    // ⭐ 실제 지면 높이를 라인 트레이스로 계산
    float GroundZ = BallLocation.Z; // 기본값은 공의 Z 위치

    // 라인 트레이스를 사용해 실제 지면 높이 찾기
    FVector TraceStart = FVector(BallLocation.X, BallLocation.Y, BallLocation.Z + 1000.0f);
    FVector TraceEnd = FVector(BallLocation.X, BallLocation.Y, BallLocation.Z - 1000.0f);

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(TargetBall); // 공은 무시
    QueryParams.bTraceComplex = false;

    bool bFoundLandscape = false;

    // 방법 B: ObjectType 필터링 (기본 방법)
    FCollisionObjectQueryParams ObjectQueryParams;
    ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);

    if (GetWorld()->LineTraceSingleByObjectType(HitResult, TraceStart, TraceEnd, ObjectQueryParams, QueryParams))
    {
        UPrimitiveComponent* HitComponent = HitResult.GetComponent();
        if (HitComponent)
        {
            FString ComponentClassName = HitComponent->GetClass()->GetName();
            // 빠른 문자열 체크로 Landscape 확인
           // if (ComponentClassName.Contains(TEXT("Landscape")))
            if (ComponentClassName.Contains(TEXT("landphysic")) || ComponentClassName.Contains(TEXT("Landphysic")))
            {
                GroundZ = HitResult.Location.Z;
                bFoundLandscape = true;
                UE_LOG(LogTemp, VeryVerbose, TEXT("Landscape found at Z: %.1f"), GroundZ);
            }
            else
            {
                UE_LOG(LogTemp, VeryVerbose, TEXT("Non-Landscape WorldStatic ignored: %s"), *ComponentClassName);
            }
        }
    }

    if (!bFoundLandscape)
    {
        GroundZ = BallLocation.Z - 50.0f;
        UE_LOG(LogTemp, Warning, TEXT("No Landscape found, using estimated Z: %.1f"), GroundZ);
    }

    // ⭐ 카메라 위치 계산: 공이 화면 중앙에 오도록 조정
    // 카메라의 현재 forward 방향 벡터를 구함
    FVector CameraForward = Camera->GetForwardVector();

    // 공에서 카메라 forward 방향의 반대로 일정 거리만큼 떨어진 위치에 카메라 배치
    FVector CameraOffset = -CameraForward * CameraDistanceFromBall;
    FVector DesiredCameraXY = BallLocation + CameraOffset;

    // ⭐ 지면에서 1미터(100cm) 위에 카메라 위치 설정
    const float CameraHeightAboveGround = 80.0f; // 1미터 = 100cm
    FVector TargetCameraPos = FVector(DesiredCameraXY.X, DesiredCameraXY.Y, GroundZ + CameraHeightAboveGround);

    // ⭐ 대기 시간이 끝난 직후에는 조금 더 부드럽게 이동 시작
    float FollowInterpSpeed = InterpSpeed;
    if (ElapsedFollowingTime < FollowingWaitTime + 0.5f) // 대기 끝난 후 0.5초 동안
    {
        FollowInterpSpeed *= 0.7f; // 속도를 70%로 감소하여 부드럽게 시작
    }

    // 부드럽게 카메라 이동
    FVector CurrentCameraPos = Camera->GetComponentLocation();
    FVector NewCameraPos = FMath::VInterpTo(CurrentCameraPos, TargetCameraPos, DeltaTime, FollowInterpSpeed);

    // ⭐ 회전 계산 및 적용 부분 제거 - 카메라는 위치만 이동하고 회전하지 않음

    // 카메라 위치만 적용 (회전은 그대로 유지)
    SetActorLocation(NewCameraPos);
}

void ACameraManager::UpdateFollowingCamera(float DeltaTime)
{
    SCOPE_CYCLE_COUNTER(STAT_CameraFollowing);
    if (!IsValidTargetBall())
        return;

    // 5미터 내 샷으로 카메라가 고정된 경우 고정된 위치 유지
    if (bIsCameraFixedForCloseShot)
    {
        SetActorLocation(CloseShotFixedLocation);
        Camera->SetWorldRotation(CloseShotFixedRotation);

        // 디버그 메시지 표시
#if WITH_EDITOR
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(105, 0.2f, FColor::Orange,
                TEXT("Camera Fixed for Close Shot (Following Mode)"));
#endif
        return;
    }

    // 공이 정지한 경우 카메라 고정
    if (bBallStoppedForCamera)
    {
        // 고정된 카메라 위치 설정 (지면 침투 방지 체크 추가)
        FVector SafeFrozenLocation = EnsureMinimumGroundClearance(FrozenCameraLocation, 80.0f);
        SetActorLocation(SafeFrozenLocation);
        return;
    }

    // ⭐ 0.5초 대기: Following 진입 직후 카메라를 현재 위치에 유지
    if (bIsWaiting)
    {
        ElapsedFollowingTime += DeltaTime;

        if (ElapsedFollowingTime >= FollowingWaitTime)
        {
            bIsWaiting = false;
            UE_LOG(LogTemp, Log, TEXT("CameraManager: Following wait done (%.2fs), starting smooth tracking"), ElapsedFollowingTime);
        }
        else
        {
#if WITH_EDITOR
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(997, 0.2f, FColor::Yellow,
                    FString::Printf(TEXT("Following: waiting %.2f / %.2f s"), ElapsedFollowingTime, FollowingWaitTime));
            }
#endif
            // 대기 중에는 공을 바라보는 회전만 부드럽게 유지
            FVector BallLoc = TargetBall->GetActorLocation();
            FVector LookDir = (BallLoc - Camera->GetComponentLocation()).GetSafeNormal();
            if (!LookDir.IsNearlyZero())
            {
                FRotator TargetRot = FRotationMatrix::MakeFromX(LookDir).Rotator();
                FRotator NewRot = FMath::RInterpTo(Camera->GetComponentRotation(), TargetRot, DeltaTime, RotationInterpSpeed * 0.5f);
                Camera->SetWorldRotation(NewRot);
            }
            return;
        }
    }

    // ⭐ EaseIn용 경과 시간 누적 (대기 종료 이후부터 카운트)
    ElapsedFollowingTime += DeltaTime;

    // 공의 현재 위치 및 속도
    FVector BallLocation = TargetBall->GetActorLocation();
    FVector CurrentVelocity = TargetBall->GetBallVelocity();
    float CurrentSpeed = GetCurrentBallSpeed();

    // 이동 방향 결정
    FVector MoveDirection;
    if (CurrentSpeed > MIN_BALL_SPEED_THRESHOLD && !CurrentVelocity.IsNearlyZero())
    {
        MoveDirection = CurrentVelocity.GetSafeNormal();
        LastMoveDirection = MoveDirection;
    }
    else
    {
        MoveDirection = LastMoveDirection; // 마지막 방향 유지
    }
    // ===================================================
        // 지면 Z 계산: 공 Z 직접 사용 → LineTrace 지면 기반으로 변경
        // 공이 바운스/진동해도 지면 자체는 고정 → 카메라 Z 떨림 제거
        // ===================================================
    float RawGroundZ = BallLocation.Z;  // fallback용

    // LineTrace: 공 위에서 아래로 쏴서 실제 지면 Z 획득
    {
        FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(CameraGroundTrace), false);
        TraceParams.AddIgnoredActor(TargetBall);
        TraceParams.AddIgnoredActor(this);

        FVector TraceStart = FVector(BallLocation.X, BallLocation.Y, BallLocation.Z + 200.f);
        FVector TraceEnd = FVector(BallLocation.X, BallLocation.Y, BallLocation.Z - 500.f);
        FHitResult GroundHit;

        if (GetWorld()->LineTraceSingleByChannel(
            GroundHit, TraceStart, TraceEnd, ECC_WorldStatic, TraceParams))
        {
            RawGroundZ = GroundHit.ImpactPoint.Z;
        }
    }

    // SmoothedGroundZ: 지면 Z를 낮은 InterpSpeed로 별도 보간
    // → 언덕 오르내릴 때도 카메라가 서서히 높이 변화, 바운스 진동은 흡수
    if (!bGroundZInitialized)
    {
        SmoothedGroundZ = RawGroundZ;
        bGroundZInitialized = true;
    }
    else
    {
        // Z 전용 InterpSpeed: XY보다 훨씬 낮게 (0.8~1.5 권장)
        // → 공이 바운스로 0.1초 튀어도 SmoothedZ는 거의 안 변함
        SmoothedGroundZ = FMath::FInterpTo(SmoothedGroundZ, RawGroundZ, DeltaTime, 1.2f);
    }

    const float CameraHeightAboveGround = 80.0f;

    // ===================================================
    // 카메라 목표 위치 계산
    // XY: 기존 로직 유지 (CameraForward 기반)
    // Z : SmoothedGroundZ 사용 (공 Z 직접 참조 제거)
    // ===================================================
    FVector CameraForward = Camera->GetForwardVector();
    FVector CameraOffset = -CameraForward * CameraDistanceFromBall;
    FVector DesiredCameraXY = BallLocation + CameraOffset;

    FVector TargetCameraPos = FVector(
        DesiredCameraXY.X,
        DesiredCameraXY.Y,
        SmoothedGroundZ + CameraHeightAboveGround);  // ← 공 Z 대신 SmoothedGroundZ

    // EaseIn
    const float EaseInDuration = 1.5f;
    float EaseAlpha = FMath::Clamp(ElapsedFollowingTime / EaseInDuration, 0.0f, 1.0f);
    float SmoothEase = EaseAlpha * EaseAlpha * (3.0f - 2.0f * EaseAlpha);
    float CurrentInterpSpeed = FMath::Lerp(InterpSpeed * 0.15f, InterpSpeed, SmoothEase);

    FVector CurrentCameraPos = Camera->GetComponentLocation();
    FVector NewCameraPos = FMath::VInterpTo(CurrentCameraPos, TargetCameraPos, DeltaTime, CurrentInterpSpeed);

    // 최소 지면 높이 보장도 SmoothedGroundZ 기준
    NewCameraPos.Z = FMath::Max(NewCameraPos.Z, SmoothedGroundZ + CameraHeightAboveGround);

    SetActorLocation(NewCameraPos);

    // ⭐ 공의 중간 지점을 바라보도록 카메라 회전 적용 (EaseIn 동일 적용)
    // ✅ LookAt 보정
    FVector LookAtTarget = BallLocation + FVector(0.f, 0.f, 50.f);


    FVector LookDirection = (LookAtTarget - NewCameraPos).GetSafeNormal();
    if (!LookDirection.IsNearlyZero())
    {
        FRotator TargetRotation = FRotationMatrix::MakeFromX(LookDirection).Rotator();
        FRotator CurrentRotation = Camera->GetComponentRotation();
        float CurrentRotSpeed = FMath::Lerp(RotationInterpSpeed * 0.3f, RotationInterpSpeed, SmoothEase);
        FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, CurrentRotSpeed);
        Camera->SetWorldRotation(NewRotation);
    }
}


void ACameraManager::PositionCameraForHoleView(const FVector& BallLocation, const FVector& AimLocation) // 홀 뷰를 위한 카메라 위치 조정
{

    // ⭐ 5미터 내 샷으로 카메라가 고정된 경우 위치 조정 금지
    //if (bIsCameraFixedForCloseShot)
    //    return;
    FVector AimActorLocation = AimLocation;
    AimActorLocation.Z = AimLocation.Z;

    // ⭐ 캐시된 PlayerController 사용
    if (!PlayerController || !PlayerController->GetAimActor() || !IsValidTargetBall())
    {
        FVector BallToHole = AimLocation - BallLocation;
        BallToHole.Z = 0.0f;
        if (!BallToHole.Normalize())
        {
            BallToHole = FVector(1.0f, 0.0f, 0.0f);
        } // Z축 제외 후 정규화
        FVector CameraDirection = -BallToHole;
        if (FMath::Abs(CameraRotation) > 0.1f)
        {
            CameraDirection = CameraDirection.RotateAngleAxis(CameraRotation, FVector::UpVector);
        }
        FVector CameraLocation = BallLocation + (CameraDirection * CameraDistanceFromBall);
        CameraLocation.Z = BallLocation.Z + CameraHeightFromBall; // 카메라 높이 설정

        SetActorLocation(CameraLocation); // 카메라 위치 설정
        FVector LookDirection = (BallLocation - CameraLocation).GetSafeNormal();
        FRotator LookRotation = FRotationMatrix::MakeFromX(LookDirection).Rotator();
        LookRotation.Pitch += 22.0f;

        if (CachedGameMode->GetCurrentTurnGolfPlayer()->PlayerInfo.ShotCountPerHole[CachedGameMode->CurrentHole - 1] == 0)
        {
            if (CachedGameMode->TeeRotationArray.Num() > 0)
                LookRotation = CachedGameMode->TeeRotationArray[CachedGameMode->CurrentHole - 1];
        }

        Camera->SetWorldRotation(LookRotation); // 카메라 회전 설정


        UE_LOG(LogTemp, Warning, TEXT("Camera fallback to holecup due to missing AimActor or PlayerController"));
        return;
    }


    FVector CameraDirection = (AimActorLocation - BallLocation).GetSafeNormal();
    if (FMath::Abs(CameraRotation) > 0.1f)
    {
        CameraDirection = CameraDirection.RotateAngleAxis(CameraRotation, FVector::UpVector);
    }
    FVector CameraLocation = BallLocation - (CameraDirection * CameraDistanceFromBall);
    CameraLocation.Z = BallLocation.Z + CameraHeightFromBall;
    SetActorLocation(CameraLocation); // 카메라 위치 설정
    Camera->SetWorldRotation(FRotationMatrix::MakeFromX(CameraDirection).Rotator()); // 카메라 회전 설정
    // 뷰포트 크기 및 공의 화면 좌표 계산
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (PC)
    { // 뷰포트 크기
        FVector2D ViewportSize;
        GEngine->GameViewport->GetViewportSize(ViewportSize);
        FVector2D BallScreenPos;
        bool bProjected = UGameplayStatics::ProjectWorldToScreen(PC, BallLocation, BallScreenPos);
        if (bProjected)
        {

            if (CachedGameMode->IsRangeMode())
            {
                float TargetScreenY = ViewportSize.Y * 0.33f;
                float DeltaScreenY = BallScreenPos.Y - TargetScreenY;
                FRotator CurrentCameraRotation = Camera->GetComponentRotation();
                float TargetPitch = CurrentCameraRotation.Pitch + (DeltaScreenY * 0.01f);
                float fInterpSpeed = 5.0f;
                float NewPitch = FMath::FInterpTo(CurrentCameraRotation.Pitch, TargetPitch, GetWorld()->GetDeltaSeconds(), fInterpSpeed); // 새로운 피치 값
                FRotator NewRotation = FRotator(NewPitch, CurrentCameraRotation.Yaw, CurrentCameraRotation.Roll); // 새로운 회전
                Camera->SetWorldRotation(NewRotation);
            }
            else
            {
                float TargetScreenY = ViewportSize.Y * 0.33f;
                float DeltaScreenY = BallScreenPos.Y - TargetScreenY;
                FRotator CurrentCameraRotation = Camera->GetComponentRotation();
                float TargetPitch = CurrentCameraRotation.Pitch + (DeltaScreenY * 0.00f);
                float fInterpSpeed = 5.0f;
                float NewPitch = FMath::FInterpTo(CurrentCameraRotation.Pitch, TargetPitch, GetWorld()->GetDeltaSeconds(), fInterpSpeed); // 새로운 피치 값
                FRotator NewRotation = FRotator(NewPitch, CurrentCameraRotation.Yaw, CurrentCameraRotation.Roll); // 새로운 회전
                Camera->SetWorldRotation(NewRotation);
            }

        }
    }
}

void ACameraManager::PositionCameraForAimView(const FVector& BallLocation, const FVector& AimLocation) // 홀 뷰를 위한 카메라 위치 조정
{

    SCOPE_CYCLE_COUNTER(STAT_CameraAimView);
    // ⭐ 5미터 내 샷으로 카메라가 고정된 경우 위치 조정 금지
    if (bIsCameraFixedForCloseShot)
        return;

    FVector AimActorLocation = AimLocation;
    AimActorLocation.Z = AimLocation.Z;

    // ⭐ 캐시된 PlayerController 사용
    if (!PlayerController || !PlayerController->GetAimActor() || !IsValidTargetBall())
    {
        FVector BallToHole = AimActorLocation - BallLocation;
        BallToHole.Z = 0.0f;
        if (!BallToHole.Normalize())
        {
            BallToHole = FVector(1.0f, 0.0f, 0.0f);
        } // Z축 제외 후 정규화
        FVector CameraDirection = -BallToHole;
        if (FMath::Abs(CameraRotation) > 0.1f)
        {
            CameraDirection = CameraDirection.RotateAngleAxis(CameraRotation, FVector::UpVector);
        }
        FVector CameraLocation = BallLocation + (CameraDirection * CameraDistanceFromBall);
        CameraLocation.Z = BallLocation.Z + CameraHeightFromBall; // 카메라 높이 설정

        SetActorLocation(CameraLocation); // 카메라 위치 설정
        FVector LookDirection = (BallLocation - CameraLocation).GetSafeNormal();
        FRotator LookRotation = FRotationMatrix::MakeFromX(LookDirection).Rotator();
        LookRotation.Pitch += 22.0f;

        if (CachedGameMode->GetCurrentTurnGolfPlayer()->PlayerInfo.ShotCountPerHole[CachedGameMode->CurrentHole - 1] == 0)
        {
            if (CachedGameMode->TeeRotationArray.Num() > 0)
                LookRotation = CachedGameMode->TeeRotationArray[CachedGameMode->CurrentHole - 1];
        }

        Camera->SetWorldRotation(LookRotation); // 카메라 회전 설정


        UE_LOG(LogTemp, Warning, TEXT("Camera fallback to holecup due to missing AimActor or PlayerController"));
        return;
    }


    FVector CameraDirection = (AimActorLocation - BallLocation).GetSafeNormal();
    if (FMath::Abs(CameraRotation) > 0.1f)
    {
        CameraDirection = CameraDirection.RotateAngleAxis(CameraRotation, FVector::UpVector);
    }
    FVector CameraLocation = BallLocation - (CameraDirection * CameraDistanceFromBall);
    CameraLocation.Z = BallLocation.Z + CameraHeightFromBall;
    SetActorLocation(CameraLocation); // 카메라 위치 설정
    Camera->SetWorldRotation(FRotationMatrix::MakeFromX(CameraDirection).Rotator()); // 카메라 회전 설정
    // 뷰포트 크기 및 공의 화면 좌표 계산
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (PC)
    { // 뷰포트 크기
        FVector2D ViewportSize;
        GEngine->GameViewport->GetViewportSize(ViewportSize);
        FVector2D BallScreenPos;
        bool bProjected = UGameplayStatics::ProjectWorldToScreen(PC, BallLocation, BallScreenPos);
        if (bProjected)
        {
            float TargetScreenY = ViewportSize.Y * 0.33f;
            float DeltaScreenY = BallScreenPos.Y - TargetScreenY;
            FRotator CurrentCameraRotation = Camera->GetComponentRotation();
            float TargetPitch = CurrentCameraRotation.Pitch + (DeltaScreenY * 0.00f);
            float fInterpSpeed = 5.0f;
            float NewPitch = FMath::FInterpTo(CurrentCameraRotation.Pitch, TargetPitch, GetWorld()->GetDeltaSeconds(), fInterpSpeed); // 새로운 피치 값
            FRotator NewRotation = FRotator(NewPitch, CurrentCameraRotation.Yaw, CurrentCameraRotation.Roll); // 새로운 회전
            Camera->SetWorldRotation(NewRotation);
        }
    }
}

float ACameraManager::GetCurrentBallSpeed() const // 현재 공의 속도 반환
{
    return IsValidTargetBall() ? TargetBall->GetBallSpeed() : 0.0f; // 타겟 공이 유효하면 속도 반환, 아니면 0.0f
}

bool ACameraManager::IsBallMoving() const // 공이 움직이는지 확인
{
    return GetCurrentBallSpeed() > MIN_BALL_SPEED_THRESHOLD; // 현재 속도가 최소 임계값보다 크면 움직이는 것으로 간주
}

bool ACameraManager::IsValidTargetBall() const // 타겟 공이 유효한지 확인
{
    return TargetBall && IsValid(TargetBall) && TargetBall->BallMesh; // 타겟 공이 null이 아니고 유효하며 BallMesh가 있는지 확인
}


void ACameraManager::SetCameraMode(ECameraMode NewMode, bool bForceUpdate) // 카메라 모드 설정
{
    if (bForceUpdate || CameraMode != NewMode)
    {
        ChangeCameraMode(NewMode);
    }
}

void ACameraManager::ClearAllTimers()
{
    if (GetWorld())
    {
        FTimerManager& TimerManager = GetWorld()->GetTimerManager();
        if (TimerManager.IsTimerActive(ModeTransitionTimer))
        {
            TimerManager.ClearTimer(ModeTransitionTimer);
            UE_LOG(LogTemp, Log, TEXT("CameraManager: Mode transition timer cleared"));
        }
        if (TimerManager.IsTimerActive(CameraResetTimer))
        {
            TimerManager.ClearTimer(CameraResetTimer);
            UE_LOG(LogTemp, Log, TEXT("CameraManager: Camera reset timer cleared"));
        }
        if (TimerManager.IsTimerActive(StopModeTimer))
        {
            TimerManager.ClearTimer(StopModeTimer);
            UE_LOG(LogTemp, Log, TEXT("CameraManager: Stop mode timer cleared"));
        }
        if (TimerManager.IsTimerActive(ResultDisplayTimer))
        {
            TimerManager.ClearTimer(ResultDisplayTimer); // 결과 표시 타이머 클리어
            UE_LOG(LogTemp, Log, TEXT("CameraManager: Result display timer cleared"));
        }
    }
}

void ACameraManager::SmoothCameraTransition(const FVector& TargetPosition, const FRotator& TargetRotation, float DeltaTime) // 부드러운 카메라 전환
{
    FVector CurrentPos = GetActorLocation();
    FRotator CurrentRot = GetActorRotation();
    FVector NewPos = FMath::VInterpTo(CurrentPos, TargetPosition, DeltaTime, InterpSpeed); // 위치 보간
    FRotator NewRot = FMath::RInterpTo(CurrentRot, TargetRotation, DeltaTime, RotationInterpSpeed); // 회전 보간
    SetActorLocation(NewPos); // 새로운 위치 설정
    SetActorRotation(NewRot); // 새로운 회전 설정
}

FVector ACameraManager::GetCurrentHolecupPosition() const // 현재 홀컵 위치 반환
{
    // ⭐ 캐시된 GameMode 사용
    if (CachedGameMode)
    {
        if (CachedGameMode->MapInfo.HolecupPositions.IsValidIndex(CachedGameMode->CurrentHole - 1))
        {
            return CachedGameMode->MapInfo.HolecupPositions[CachedGameMode->CurrentHole - 1]; // 홀컵 위치 반환
        }
    }

    if (TargetBall)
    {
        return TargetBall->GetActorLocation() + FVector(500.0f, 0.0f, 0.0f);
    }

    return FVector::ZeroVector;
}

FVector ACameraManager::CalculateOptimalCameraPosition(const FVector& BallLocation, const FVector& HolecupLocation) // 최적 카메라 위치 계산
{
    FVector BallToHole = HolecupLocation - BallLocation; // 공에서 홀컵으로의 벡터
    BallToHole.Z = 0.0f; // Z축 제외
    BallToHole.Normalize();
    FVector CameraDirection = -BallToHole;
    if (FMath::Abs(CameraRotation) > 0.1f)
    {
        CameraDirection = CameraDirection.RotateAngleAxis(CameraRotation, FVector::UpVector);
    }
    FVector CameraLocation = BallLocation + (CameraDirection * CameraDistanceFromBall);
    CameraLocation.Z = BallLocation.Z + CameraHeightFromBall; // 카메라 높이 설정
    return CameraLocation; // 계산된 카메라 위치 반환
}

void ACameraManager::ActivateAsMainCamera() // 이 카메라를 메인 카메라로 활성화
{
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (PC)
    {
        PC->SetViewTargetWithBlend(this, 1.0f, EViewTargetBlendFunction::VTBlend_EaseInOut);

    }
}

void ACameraManager::ApplyCameraModeSettings() // 카메라 모드 설정 적용
{
    switch (CameraMode)
    {
    case ECameraMode::Ready:
        CameraDistanceFromBall = READY_DISTANCE;
        CameraHeightFromBall = READY_HEIGHT;
        BallOffsetFromCenter = BALL_SCREEN_OFFSET;
        break;
    case ECameraMode::Flying:
        CameraDistanceFromBall = FLYING_DISTANCE;
        CameraHeightFromBall = FLYING_HEIGHT;
        BallOffsetFromCenter = 0.4f;
        break;
    case ECameraMode::Following:
        CameraDistanceFromBall = FOLLOWING_DISTANCE;
        CameraHeightFromBall = FOLLOWING_HEIGHT;
        BallOffsetFromCenter = 0.5f;
        break;
    case ECameraMode::Stop:
        EnterStopMode();
        break;
    case ECameraMode::Fixed:
        // Fixed 위치가 유효하지 않다면 현재 위치 사용
        if (FixedCameraLocation.IsNearlyZero())
        {
            FixedCameraLocation = GetActorLocation();
            FixedCameraRotation = GetActorRotation();
            UE_LOG(LogTemp, Warning, TEXT("CameraManager: Fixed position was invalid, using current position: %s"),
                *FixedCameraLocation.ToString());
        }

        SetActorLocation(FixedCameraLocation);
        Camera->SetWorldRotation(FixedCameraRotation);
        UE_LOG(LogTemp, Log, TEXT("CameraManager: Applied Fixed mode settings at %s"),
            *FixedCameraLocation.ToString());
        break;
    }

    UE_LOG(LogTemp, VeryVerbose, TEXT("Camera settings applied for mode: %s"),
        *UEnum::GetValueAsString(CameraMode));
}

void ACameraManager::UpdateCameraTransitionCountdown() // 카메라 전환 카운트다운 업데이트
{
    if (!GetWorld() || !GetWorld()->GetTimerManager().IsTimerActive(ModeTransitionTimer))
        return;

    float TimeRemaining = GetWorld()->GetTimerManager().GetTimerRemaining(ModeTransitionTimer);
    if (TimeRemaining > 0.0f)
    {
        int32 SecondsLeft = FMath::CeilToInt(TimeRemaining);
#if WITH_EDITOR
        if (GEngine)
        {
            FColor CountdownColor = SecondsLeft <= 1 ? FColor::Orange : FColor::Cyan;
            GEngine->AddOnScreenDebugMessage(998, 0.2f, CountdownColor,
                FString::Printf(TEXT("Camera Ready: %d seconds"), SecondsLeft));
        }
#endif
    }
}

void ACameraManager::ForceReadyMode() // Ready 모드로 강제 전환
{
    if (GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(ModeTransitionTimer))
    {
        GetWorld()->GetTimerManager().ClearTimer(ModeTransitionTimer);
        UE_LOG(LogTemp, Log, TEXT("CameraManager: Force transition timer cleared"));
    }

    // ⭐ 5미터 내 샷 고정 해제
    ReleaseCloseToHoleShotFixed();

    ChangeCameraMode(bUseFixedModeInReady ? ECameraMode::Fixed : ECameraMode::Ready);

    UE_LOG(LogTemp, Log, TEXT("CameraManager: Forced to %s mode"), bUseFixedModeInReady ? TEXT("Fixed") : TEXT("Ready")); // 로그 메시지
}

void ACameraManager::SetReadyModeDelay(float DelaySeconds) // Ready 모드 지연 시간 설정
{
    ReadyModeDelayTime = FMath::Clamp(DelaySeconds, 1.0f, 10.0f);
    UE_LOG(LogTemp, Log, TEXT("CameraManager: Ready mode delay set to %.1f seconds"), ReadyModeDelayTime);
}

bool ACameraManager::IsBallCompletelyStoppedForCamera() const // 카메라 관점에서 공이 완전히 멈췄는지 확인
{
    if (!IsValidTargetBall())
        return false;

    EBallState BallState = TargetBall->GetBallState();
    float BallSpeed = GetCurrentBallSpeed();
    bool bIsStateStopped = (BallState == EBallState::Ball_Stop);
    bool bIsSpeedStopped = (BallSpeed < 3.0f); // 속도 임계값 이하
    return bIsStateStopped || bIsSpeedStopped;
}

void ACameraManager::CheckForAutomaticModeTransitionImproved() // 자동 카메라 모드 전환 개선
{
    if (!IsValidTargetBall()) return;

    // ⭐ 새로 추가: 부분 고정 모드에서는 Ready 모드만 자동 전환 허용
    if (bUsePartialFixedMode)
    {
        // Ready 모드에서만 자동 전환 허용 (Flying으로의 전환)
        if (CameraMode == ECameraMode::Ready)
        {
            EBallState BallState = TargetBall->GetBallState();
            float CurrentSpeed = TargetBall->GetBallSpeed();

            if (BallState == EBallState::Ball_Fly && CurrentSpeed > MIN_BALL_SPEED_THRESHOLD)
            {
                // Flying 모드로 전환 시도하면 Fixed 모드로 변환됨
                ChangeCameraMode(ECameraMode::Flying);
            }
        }
        else
        {
            // Fixed 모드에 있을 때는 자동 전환 없음
            UE_LOG(LogTemp, VeryVerbose, TEXT("📷 Partial Fixed Mode: No automatic transitions from Fixed mode"));
        }
        return;
    }

    // Fixed 모드이거나 Ready 상태에서 고정 모드를 사용하면 자동 전환 비활성화
    if (CameraMode == ECameraMode::Fixed || (TargetBall->GetBallState() == EBallState::Ball_Ready && bUseFixedModeInReady))
        return;

    // 5미터 내 샷으로 카메라가 고정된 경우 자동 전환 비활성화
    if (bIsCameraFixedForCloseShot)
        return;

    // 나머지 기존 자동 전환 로직...
    EBallState BallState = TargetBall->GetBallState();
    float CurrentSpeed = TargetBall->GetBallSpeed();

    switch (CameraMode)
    {
    case ECameraMode::Ready:
        if (BallState == EBallState::Ball_Fly && CurrentSpeed > MIN_BALL_SPEED_THRESHOLD)
        {
            ChangeCameraMode(ECameraMode::Flying);
        }
        break;
    case ECameraMode::Flying:
        if (BallState == EBallState::Ball_Bound || BallState == EBallState::Ball_Rolling || CurrentSpeed < TRANSITION_SPEED_THRESHOLD)
        {
            ChangeCameraMode(ECameraMode::Following);
        }
        break;
    case ECameraMode::Following:
        if (BallState == EBallState::Ball_Stop || CurrentSpeed < 1.f)
        {
            if (!bBallStoppedForCamera)
            {
                bBallStoppedForCamera = true;
                BallStopTime = GetWorld()->GetTimeSeconds();
                FrozenCameraLocation = GetActorLocation();
                FrozenCameraRotation = GetActorRotation();
                UE_LOG(LogTemp, Log, TEXT("CameraManager: Ball stopped, camera frozen"));
            }

            if (!GetWorld()->GetTimerManager().IsTimerActive(ModeTransitionTimer))
            {
                GetWorld()->GetTimerManager().SetTimer(
                    ModeTransitionTimer,
                    [this]() {
                        ChangeCameraMode(ECameraMode::Stop);
                    },
                    CameraFreezeTime,
                    false
                );
                UE_LOG(LogTemp, Log, TEXT("CameraManager: %d-second freeze timer set (Following -> Stop)"), (int32)CameraFreezeTime);
            }
        }
        else
        {
            if (bBallStoppedForCamera)
            {
                bBallStoppedForCamera = false;
                BallStopTime = 0.0f;
                if (GetWorld()->GetTimerManager().IsTimerActive(ModeTransitionTimer))
                {
                    GetWorld()->GetTimerManager().ClearTimer(ModeTransitionTimer);
                    UE_LOG(LogTemp, Log, TEXT("CameraManager: Ball started moving again, freeze cancelled"));
                }
            }
        }
        break;

    case ECameraMode::Stop:
        // 자동 전환은 타이머에서 처리
        break;
    }
}


void ACameraManager::UnfreezeCameraImmediate() // 카메라 즉시 고정 해제
{
    if (bBallStoppedForCamera)
    {
        bBallStoppedForCamera = false;
        BallStopTime = 0.0f;
        if (GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(ModeTransitionTimer))
        {
            GetWorld()->GetTimerManager().ClearTimer(ModeTransitionTimer);
        }
        UE_LOG(LogTemp, Log, TEXT("CameraManager: Camera unfrozen immediately"));
#if WITH_EDITOR
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
                TEXT("📷 Camera unfreeze!"));
        }
#endif
    } //

      // ⭐ 5미터 내 샷 고정도 해제
    ReleaseCloseToHoleShotFixed();
}

float ACameraManager::GetCameraFreezeTimeRemaining() const // 카메라 고정 남은 시간 반환
{
    if (!bBallStoppedForCamera || !GetWorld())
        return 0.0f;

    float CurrentTime = GetWorld()->GetTimeSeconds();
    float ElapsedTime = CurrentTime - BallStopTime;
    float TimeRemaining = CameraFreezeTime - ElapsedTime;
    return FMath::Max(0.0f, TimeRemaining);
}

void ACameraManager::SetCameraFreezeTime(float NewFreezeTime) // 카메라 고정 시간 설정
{
    CameraFreezeTime = FMath::Clamp(NewFreezeTime, 1.0f, 10.0f);
    UE_LOG(LogTemp, Log, TEXT("CameraManager: Freeze time set to %.1f seconds"), CameraFreezeTime);
#if WITH_EDITOR
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan,
            FString::Printf(TEXT("📷 Camera freeze time: %.1f seconds"), CameraFreezeTime));
    }
#endif
}

bool ACameraManager::IsStatesSynchronized() const // 카메라 상태가 공 상태와 동기화되었는지 확인
{
    if (!IsValidTargetBall())
        return false; // 유효한 공이 없다면 동기화되지 않음

    if (CameraMode == ECameraMode::Fixed && TargetBall->GetBallState() == EBallState::Ball_Ready && bUseFixedModeInReady)
        return true;

    EBallState CurrentBallState = TargetBall->GetBallState();
    ECameraMode CurrentCameraMode = CameraMode;
    bool bIsSynced = (CameraMode == GetRequiredCameraModeForBallState(CurrentBallState, TargetBall->GetBallSpeed())); // 캐시된 공 속도 사용

    if (!bIsSynced)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ State desync: Ball=%s, Camera=%s, Expected=%s"),
            *UEnum::GetValueAsString(CurrentBallState),
            *UEnum::GetValueAsString(CurrentCameraMode),
            *UEnum::GetValueAsString(GetRequiredCameraModeForBallState(CurrentBallState, TargetBall->GetBallSpeed())));
    }

    return bIsSynced;
}

void ACameraManager::CheckStateSynchronization() // 주기적으로 상태 동기화 확인
{
    // 2초마다 동기화 체크
    static float LastSyncCheckTime = 0.0f;
    float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LastSyncCheckTime > 2.0f)
    {
        if (!IsStatesSynchronized())
        {
            UE_LOG(LogTemp, Warning, TEXT("🔄 Forcing state synchronization..."));
            ForceSyncWithBall();
        }
        LastSyncCheckTime = CurrentTime;
    }
}

void ACameraManager::UpdateStopCamera(float DeltaTime) // Stop 모드 카메라 업데이트
{
    if (!IsValidTargetBall()) return;
    // 카메라 위치와 회전 고정
    SetActorLocation(StopModeCameraLocation);
    SetActorRotation(StopModeCameraRotation);


}

void ACameraManager::EnterStopMode() // Stop 모드 진입
{
    UE_LOG(LogTemp, Log, TEXT("📷 Entering Stop mode for result display"));
    bInStopMode = true;
    StopModeStartTime = GetWorld()->GetTimeSeconds();
    // StopModeCameraLocation = CalculateStopCameraPosition();
    // StopModeCameraRotation = CalculateStopCameraRotation();
    StopModeCameraLocation = GetActorLocation();
    StopModeCameraRotation = GetActorRotation();
    // SetActorLocation(StopModeCameraLocation); // 카메라 위치 설정
   //  SetActorRotation(StopModeCameraRotation); // 카메라 회전 설정
     //ShowBallResult(); // 결과 UI 표시

    GetWorld()->GetTimerManager().SetTimer(
        StopModeTimer,
        [this]() {
            ExitStopMode();
            //  ChangeCameraMode(bUseFixedModeInReady && TargetBall && TargetBall->GetBallState() == EBallState::Ball_Ready ?
            //      ECameraMode::Fixed : ECameraMode::Ready);
        },
        STOP_DURATION,
        false
    );
    UE_LOG(LogTemp, Log, TEXT("📷 Stop mode timer set for %.1f seconds"), STOP_DURATION);
}

void ACameraManager::ExitStopMode() // Stop 모드 종료
{
    UE_LOG(LogTemp, Log, TEXT("📷 Exiting Stop mode"));
    bInStopMode = false;
    StopModeStartTime = 0.0f;
    if (GetWorld()) // 타이머 클리어
    {
        GetWorld()->GetTimerManager().ClearTimer(StopModeTimer);
        GetWorld()->GetTimerManager().ClearTimer(ResultDisplayTimer);
    }
}

FVector ACameraManager::CalculateStopCameraPosition() const // Stop 모드 카메라 위치 계산
{
    if (!IsValidTargetBall())
        return GetActorLocation();

    FVector BallLocation = TargetBall->GetActorLocation(); // 공의 현재 위치
    FVector HolecupLocation = GetCurrentHolecupPosition(); // 홀컵 위치
    FVector MidPoint = (BallLocation + HolecupLocation) * 0.5f; // 공과 홀컵의 중간 지점
    FVector BallToHole = (HolecupLocation - BallLocation).GetSafeNormal();
    FVector RightVector = FVector::CrossProduct(BallToHole, FVector::UpVector).GetSafeNormal();
    FVector CameraOffset = (-BallToHole * RESULT_DISPLAY_DISTANCE) + (RightVector * 100.0f);
    CameraOffset.Z = STOP_HEIGHT_OFFSET;
    return BallLocation + CameraOffset;
}

FRotator ACameraManager::CalculateStopCameraRotation() const // Stop 모드 카메라 회전 계산
{
    if (!IsValidTargetBall())
        return GetActorRotation();

    FVector BallLocation = TargetBall->GetActorLocation(); // 공의 현재 위치
    FVector CameraLocation = StopModeCameraLocation; // 카메라 위치
    FVector LookDirection = (BallLocation - CameraLocation).GetSafeNormal();
    FRotator LookRotation = FRotationMatrix::MakeFromX(LookDirection).Rotator(); // 공을 바라보도록 회전
    LookRotation.Pitch += 15.0f;
    return LookRotation;
}

void ACameraManager::ShowBallResult() // 볼 결과 표시
{
    if (!IsValidTargetBall()) return;

    FVector BallLocation = TargetBall->GetActorLocation();
    bool bHoleIn = TargetBall->IsHoleIn(); // ⭐ GolfBall의 IsHoleIn() 함수 사용
    bool bOutOfBounds = TargetBall->IsOutOfBounds(); // ⭐ GolfBall의 IsOutOfBounds() 함수 사용
    FString ResultMessage;
    FColor ResultColor;

    if (bHoleIn)
    {
        ResultMessage = TEXT("🏆 HOLE IN! 🏆");
        ResultColor = FColor::Blue;
    }
    else if (bOutOfBounds)
    {
        ResultMessage = TEXT("🚨 OUT OF BOUNDS 🚨");
        ResultColor = FColor::Red;
    }
    else
    {
        FVector HolecupLocation = GetCurrentHolecupPosition();
        float DistanceToHole = FVector::Dist(BallLocation, HolecupLocation) / 100.0f;
        ResultMessage = FString::Printf(TEXT("📏 홀까지: %.1fm"), DistanceToHole);
        ResultColor = FColor::Cyan;
    }

    //if (GetWorld())
    //{
    //    DrawDebugString(GetWorld(), BallLocation + FVector(0, 0, 100),
    //        ResultMessage, nullptr, ResultColor, -1.0f, false, 3.0f);
    //    DrawDebugSphere(GetWorld(), BallLocation, 30.0f, 16, ResultColor, false, -1.0f);
    //}

    //if (GEngine)
    //{
    //    GEngine->AddOnScreenDebugMessage(100, -1.0f, ResultColor, ResultMessage);
    //}
}

void ACameraManager::UpdateStopModeCountdown() // Stop 모드 카운트다운 업데이트
{
    float TimeRemaining = GetStopModeTimeRemaining();
    if (TimeRemaining > 0.0f)
    {
        int32 SecondsLeft = FMath::CeilToInt(TimeRemaining);
        FColor CountdownColor;
        if (SecondsLeft <= 1)
            CountdownColor = FColor::Orange;
        else if (SecondsLeft <= 2)
            CountdownColor = FColor::Yellow;
        else // 그 외
            CountdownColor = FColor::White;

        if (IsValidTargetBall() && GetWorld())
        {
#if WITH_EDITOR
            FVector BallLocation = TargetBall->GetActorLocation();
            DrawDebugString(GetWorld(), BallLocation + FVector(0, 0, 60),
                FString::Printf(TEXT("%d초"), SecondsLeft),
                nullptr, CountdownColor, -1.0f, false, 2.0f);
#endif
        }

#if WITH_EDITOR
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(101, -1.0f, CountdownColor,
                FString::Printf(TEXT("⏱️ 다음 턴까지: %d초"), SecondsLeft));
        }
#endif
    }
}

float ACameraManager::GetStopModeTimeRemaining() const // Stop 모드 남은 시간 반환
{
    if (!bInStopMode || !GetWorld())
        return 0.0f;

    float CurrentTime = GetWorld()->GetTimeSeconds();
    float ElapsedTime = CurrentTime - StopModeStartTime;
    float TimeRemaining = STOP_DURATION - ElapsedTime;
    return FMath::Max(0.0f, TimeRemaining);
}

void ACameraManager::ForceStopMode() // Stop 모드 강제 진입
{
    if (CameraMode != ECameraMode::Stop)
    {
        ChangeCameraMode(ECameraMode::Stop);
        UE_LOG(LogTemp, Log, TEXT("📷 Force entered Stop mode"));
#if WITH_EDITOR
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow,
                TEXT("📷 Camera forced to Stop mode")); // Stop 모드로 강제 진입 시 메시지 추가
        }
#endif
    }
}

void ACameraManager::SkipStopMode() // Stop 모드 스킵
{
    if (bInStopMode)
    {
        ExitStopMode();
        ChangeCameraMode(bUseFixedModeInReady && TargetBall && TargetBall->GetBallState() == EBallState::Ball_Ready ?
            ECameraMode::Fixed : ECameraMode::Ready);
        UE_LOG(LogTemp, Log, TEXT("📷 Stop mode skipped"));
#if WITH_EDITOR
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow,
                TEXT("⏭️ Stop mode skipped"));
        }
#endif
    }
}

void ACameraManager::SetStopModeDuration(float NewDuration) // Stop 모드 지속 시간 설정
{
    const float ClampedDuration = FMath::Clamp(NewDuration, 1.0f, 10.0f);
    // ⭐ STOP_DURATION 상수는 const이기 때문에 여기서는 직접 수정하지 않습니다.
    // 만약 이 값을 런타임에 변경 가능하게 하려면, STOP_DURATION을 UPROPERTY로 변경하거나,
    // 이 함수에서 내부적으로 사용하는 다른 UPROPERTY 변수를 정의해야 합니다.
    UE_LOG(LogTemp, Log, TEXT("📷 Stop mode duration set to %.1fs (Note: Constant STOP_DURATION is still used for timer)"), ClampedDuration);
}

void ACameraManager::SetFixedCameraPosition(FVector NewPosition, FRotator NewRotation) // Fixed 카메라 위치 및 회전 설정
{
    // 유효한 위치인지 확인
    if (NewPosition.IsNearlyZero())
    {
        UE_LOG(LogTemp, Warning, TEXT("CameraManager: Invalid fixed camera position (0,0,0), using current position instead"));
        NewPosition = GetActorLocation();
    }

    FixedCameraLocation = NewPosition;
    FixedCameraRotation = NewRotation;

    UE_LOG(LogTemp, Log, TEXT("CameraManager: Fixed camera position set to %s, rotation %s"),
        *NewPosition.ToString(), *NewRotation.ToString());

    // Fixed 모드에서 즉시 적용
    if (CameraMode == ECameraMode::Fixed)
    {
        SetActorLocation(FixedCameraLocation);
        Camera->SetWorldRotation(FixedCameraRotation);
    }

#if WITH_EDITOR
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Purple,
            FString::Printf(TEXT("📷 Fixed Camera Set: %s"), *NewPosition.ToString()));
    }
#endif
}


void ACameraManager::ForceFixedMode() // Fixed 모드 강제 진입
{
    // Fixed 카메라 위치가 설정되지 않았다면 현재 위치 사용
    if (FixedCameraLocation.IsNearlyZero())
    {
        FixedCameraLocation = GetActorLocation();
        FixedCameraRotation = GetActorRotation();
        UE_LOG(LogTemp, Log, TEXT("📷 Fixed camera position auto-initialized to: %s"),
            *FixedCameraLocation.ToString());
    }

    if (CameraMode != ECameraMode::Fixed)
    {
        ChangeCameraMode(ECameraMode::Fixed);
        UE_LOG(LogTemp, Log, TEXT("📷 Force entered Fixed mode at position: %s"),
            *FixedCameraLocation.ToString());

#if WITH_EDITOR
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Purple,
                TEXT("📷 Camera forced to Fixed mode"));
        }
#endif
    }
}

void ACameraManager::UpdateFixedCamera(float DeltaTime) // Fixed 모드 카메라 업데이트
{
    // ⭐ 부분 고정 모드에서는 저장된 Ready 위치를 확실히 유지
    if (bUsePartialFixedMode)
    {
        // Fixed 위치가 설정되어 있으면 그 위치를 사용
        if (!FixedCameraLocation.IsNearlyZero())
        {
            SetActorLocation(FixedCameraLocation);
            Camera->SetWorldRotation(FixedCameraRotation);
        }
        else
        {
            // 만약 위치가 설정되지 않았다면 현재 위치를 저장하고 사용
            FixedCameraLocation = GetActorLocation();
            FixedCameraRotation = Camera->GetComponentRotation();
            UE_LOG(LogTemp, Warning, TEXT("📷 Fixed camera position was not set, using current position: %s"),
                *FixedCameraLocation.ToString());
        }
    }
    else
    {
        // 일반 고정 모드 처리 (기존 코드)
        SetActorLocation(FixedCameraLocation);
        Camera->SetWorldRotation(FixedCameraRotation);
    }

#if WITH_EDITOR
    if (GEngine)
    {
        FString ModeText = bUsePartialFixedMode ? TEXT("Partial Fixed (Ready Position Preserved)") : TEXT("Fixed Camera Mode Active");
        GEngine->AddOnScreenDebugMessage(102, 0.2f, FColor::Purple, FString::Printf(TEXT("%s"), *ModeText));
    }
#endif
}


// ⭐ 추가된 함수 구현
bool ACameraManager::IsBallNearHoleCup() const
{
    // 공과 게임모드 객체가 유효한지 확인
    if (!IsValidTargetBall() || !CachedGameMode)
    {
        return false;
    }

    // 홀컵 위치를 가져옵니다.
    // GameMode가 홀컵 위치를 제공한다고 가정합니다.
    FVector HoleCupLocation = GetCurrentHolecupPosition();

    // 공의 현재 위치를 가져옵니다.
    FVector BallLocation = TargetBall->GetActorLocation();

    // 두 위치 간의 거리를 계산합니다.
    float Distance = FVector::Dist(BallLocation, HoleCupLocation);

    // 5미터(500cm) 이내인지 확인합니다.
    return Distance <= CLOSE_TO_HOLE_THRESHOLD_CM;
}

// ⭐ 새로 추가: 5미터 내 샷 감지 및 처리 함수들
bool ACameraManager::IsCloseToHoleShot() const
{
    if (!IsValidTargetBall())
        return false;

    FVector BallLocation = TargetBall->GetActorLocation();
    FVector HolecupLocation = GetCurrentHolecupPosition();
    float DistanceToHole = FVector::Dist(BallLocation, HolecupLocation);

    return (DistanceToHole <= CLOSE_TO_HOLE_THRESHOLD_CM);
}

void ACameraManager::HandleCloseToHoleShot()
{
    if (!IsValidTargetBall())
        return;

    // Fixed 모드에서는 현재 Fixed 위치를 사용하되, 유효하지 않으면 현재 카메라 위치 사용
    if (CameraMode == ECameraMode::Fixed && !FixedCameraLocation.IsNearlyZero())
    {
        CloseShotFixedLocation = FixedCameraLocation;
        CloseShotFixedRotation = FixedCameraRotation;
    }
    else
    {
        // 샷하는 순간의 카메라 위치와 회전을 고정
        CloseShotFixedLocation = GetActorLocation();
        CloseShotFixedRotation = Camera->GetComponentRotation();
    }

    // 위치가 여전히 유효하지 않다면 기본 위치 설정
    if (CloseShotFixedLocation.IsNearlyZero() && IsValidTargetBall())
    {
        FVector BallLocation = TargetBall->GetActorLocation();
        CloseShotFixedLocation = BallLocation + FVector(-400.0f, 0.0f, 150.0f);
        CloseShotFixedRotation = FRotationMatrix::MakeFromX((BallLocation - CloseShotFixedLocation).GetSafeNormal()).Rotator();
        UE_LOG(LogTemp, Warning, TEXT("📷 Using fallback position for close shot fixed camera"));
    }

    bIsCameraFixedForCloseShot = true;

    UE_LOG(LogTemp, Log, TEXT("📷 Camera FIXED for close-to-hole shot at position: %s, rotation: %s - Will NOT follow ball"),
        *CloseShotFixedLocation.ToString(), *CloseShotFixedRotation.ToString());

#if WITH_EDITOR
    if (GEngine)
    {
        float DistanceToHole = FVector::Dist(TargetBall->GetActorLocation(), GetCurrentHolecupPosition()) / 100.0f;
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Orange,
            FString::Printf(TEXT("Camera FIXED - Close Shot (%.1fm) - No Follow"), DistanceToHole));
    }
#endif
}
void ACameraManager::ReleaseCloseToHoleShotFixed()
{
    if (bIsCameraFixedForCloseShot)
    {
        bIsCameraFixedForCloseShot = false;
        CloseShotFixedLocation = FVector::ZeroVector;
        CloseShotFixedRotation = FRotator::ZeroRotator;

        UE_LOG(LogTemp, Log, TEXT("📷 Camera released from close-to-hole shot fixed mode"));

#if WITH_EDITOR
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
                TEXT("📷 Camera Released - Close Shot Fixed Mode OFF"));
        }
#endif
    }
}

// ⭐ 새로 추가: 카메라 모드 옵션 설정 함수
void ACameraManager::SetCameraModeOption(int32 CameraModeOption)
{
    switch (CameraModeOption)
    {
    case 0:
        // 자동 카메라 모드
        bUsePartialFixedMode = false;
        bUseFixedModeInReady = false;
        UE_LOG(LogTemp, Log, TEXT("📷 Camera Mode set to: Auto"));
        break;

    case 1:
        // 부분 고정 카메라 모드 (Ready는 자유, 나머지는 고정)
        bUsePartialFixedMode = true;
        bUseFixedModeInReady = false; // Ready는 자유 카메라 유지

        UE_LOG(LogTemp, Log, TEXT("📷 Camera Mode set to: Partial Fixed (Ready=Free, Others=Fixed)"));
        break;

    default:
        UE_LOG(LogTemp, Warning, TEXT("📷 Unknown Camera Mode Option: %d"), CameraModeOption);
        break;
    }
}

// ⭐ 새로 추가: 현재 카메라 모드 옵션 반환 함수
int32 ACameraManager::GetCameraModeOption() const
{
    if (bUsePartialFixedMode && ForcedCameraMode == ECameraMode::Fixed)
    {
        return 1; // Fixed mode
    }
    return 0; // Auto mode
}


FVector ACameraManager::EnsureMinimumGroundClearance(const FVector& CameraPosition, float MinClearance) const
{
    if (!GetWorld())
        return CameraPosition;

    FVector TraceStart = FVector(CameraPosition.X, CameraPosition.Y, CameraPosition.Z + 200.0f);
    FVector TraceEnd = FVector(CameraPosition.X, CameraPosition.Y, CameraPosition.Z - 200.0f);

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    if (TargetBall)
        QueryParams.AddIgnoredActor(TargetBall);
    QueryParams.AddIgnoredActor(this);

    FCollisionObjectQueryParams ObjectQueryParams;
    ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);

    if (GetWorld()->LineTraceSingleByObjectType(HitResult, TraceStart, TraceEnd, ObjectQueryParams, QueryParams))
    {
        UPrimitiveComponent* HitComponent = HitResult.GetComponent();
        if (HitComponent && HitComponent->GetClass()->GetName().Contains(TEXT("Landscape")))
        {
            float GroundZ = HitResult.Location.Z;
            float RequiredZ = GroundZ + MinClearance;

            if (CameraPosition.Z < RequiredZ)
            {
                UE_LOG(LogTemp, Log, TEXT("Ground clearance enforced: %.1f -> %.1f"), CameraPosition.Z, RequiredZ);
                return FVector(CameraPosition.X, CameraPosition.Y, RequiredZ);
            }
        }
        else if (HitComponent && HitComponent->GetClass()->GetName().Contains(TEXT("landphysic"))
            || HitComponent && HitComponent->GetClass()->GetName().Contains(TEXT("Landphysic")))
        {
            float GroundZ = HitResult.Location.Z;
            float RequiredZ = GroundZ + MinClearance;

            if (CameraPosition.Z < RequiredZ)
            {
                UE_LOG(LogTemp, Log, TEXT("Ground clearance enforced: %.1f -> %.1f"), CameraPosition.Z, RequiredZ);
                return FVector(CameraPosition.X, CameraPosition.Y, RequiredZ);
            }
        }
    }

    return CameraPosition;
}

// ⭐ 새로운 헬퍼 함수 추가 - 카메라 Forward 벡터 확인용
FVector ACameraManager::GetCameraForwardDirection() const
{
    if (Camera)
    {
        FVector Forward = Camera->GetForwardVector();
        Forward.Z = 0.0f;
        Forward.Normalize();
        return Forward;
    }
    return FVector::ForwardVector;
}


FVector ACameraManager::EnforceGroundClearance(const FVector& CameraPosition, float MinClearance /*= 50.0f*/) const
{
    UWorld* World = GetWorld();
    if (!World)
        return CameraPosition;

    // ⭐️ [핵심 수정]: 트레이스 시작점을 카메라 목표 위치보다 훨씬 위쪽(예: 10미터)으로 설정하여 
    // 급경사에서도 지면을 놓치지 않도록 합니다.
    const float TraceZOffset = 1000.0f; // 10미터 위에서 시작
    const float TraceDistance = 5000.0f; // 50미터 아래까지 트레이스하여 지면을 확실히 찾음

    FVector TraceStart = FVector(CameraPosition.X, CameraPosition.Y, CameraPosition.Z + TraceZOffset);
    FVector TraceEnd = FVector(CameraPosition.X, CameraPosition.Y, CameraPosition.Z - TraceDistance);

    FHitResult HitResult;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this); // 카메라 액터 무시
    if (TargetBall)
    {
        Params.AddIgnoredActor(TargetBall); // 공 무시
    }

    // 월드 스태틱(지형)과 충돌 검사
    // ECC_WorldStatic 채널이 지형과 충돌하는지 확인하세요. (일반적으로 맞습니다)
    World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, Params);

    if (HitResult.bBlockingHit)
    {
        // 충돌한 컴포넌트가 지형(Landscape) 또는 관련 물리 컴포넌트인지 확인
        UPrimitiveComponent* HitComponent = HitResult.GetComponent();
        if (HitComponent && (HitComponent->GetClass()->GetName().Contains(TEXT("Landscape")) ||
            HitComponent->GetClass()->GetName().Contains(TEXT("landphysic")) ||
            HitComponent->GetClass()->GetName().Contains(TEXT("Landphysic"))))
        {
            float GroundZ = HitResult.Location.Z;
            float RequiredZ = GroundZ + MinClearance; // 지면 높이 + 최소 여유 공간

            if (CameraPosition.Z < RequiredZ)
            {
                // 현재 카메라 Z 위치가 필요 높이보다 낮으면 보정
                UE_LOG(LogTemp, Log, TEXT("Ground clearance enforced: %.1f -> %.1f"), CameraPosition.Z, RequiredZ);
                return FVector(CameraPosition.X, CameraPosition.Y, RequiredZ);
            }
        }
    }

    return CameraPosition;
}

// 2. 투어 모드 시작
void ACameraManager::StartTourMode()
{
    // 조건 체크: 공이 Ready 상태일 때만 가능
    if (!IsValidTargetBall() || TargetBall->GetBallState() != EBallState::Ball_Ready)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot start tour: Ball is not in Ready state"));
        return;
    }

    // 이미 투어 중이면 무시 or 리셋
    if (CameraMode == ECameraMode::Tour) return;

    // 시작점: 현재 티 위치 (또는 현재 공 위치)
    TourStartLocation = TargetBall->GetActorLocation(); // 혹은 TeePositions 사용

    // 도착점: 홀컵 위치
    TourEndLocation = GetCurrentHolecupPosition();

    // 변수 초기화
    TourElapsedTime = 0.0f;

    // 모드 전환
    ChangeCameraMode(ECameraMode::Tour);

    UE_LOG(LogTemp, Log, TEXT("📷 Tour Mode Started: Tee -> Hole"));
}

// 3. 투어 모드 업데이트 (핵심 연출 로직)
void ACameraManager::UpdateTourCamera(float DeltaTime)
{
    TourElapsedTime += DeltaTime;

    // 진행률 (0.0 ~ 1.0)
    float Alpha = FMath::Clamp(TourElapsedTime / TourDuration, 0.0f, 1.0f);

    // Ease-In-Out 적용 (출발/도착 시 부드럽게 감속)
    float SmoothAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);

    // 1. 위치 보간 (Linear Interpolation)
    FVector NewLocation = FMath::Lerp(TourStartLocation, TourEndLocation, SmoothAlpha);

    // 2. 높이 연출 (Parabola/Arc) - 중간 지점에서 가장 높게
    // Sin(0) = 0, Sin(PI/2) = 1, Sin(PI) = 0
    float HeightOffset = FMath::Sin(SmoothAlpha * PI) * TourArcHeight;
    NewLocation.Z += HeightOffset;

    // 카메라 위치 설정 (높이 오프셋 추가)
    // 기본 높이 확보 (지면 충돌 방지)
    NewLocation.Z = FMath::Max(NewLocation.Z, TourStartLocation.Z + 500.0f);

    SetActorLocation(NewLocation);

    // 3. 회전 연출
    // 카메라가 홀컵을 바라보면서 이동하거나, 진행 방향을 바라보게 함

    // 옵션 A: 계속 홀컵 바라보기 (Focus Lock)
    /*
    FVector LookDir = (TourEndLocation - NewLocation).GetSafeNormal();
    FRotator NewRotation = FRotationMatrix::MakeFromX(LookDir).Rotator();
    */

    // 옵션 B: 살짝 아래를 굽어보며 이동 (드론 샷 느낌)
    FRotator NewRotation = GetActorRotation();
    NewRotation.Pitch = -30.0f; // 30도 아래로
    // Yaw는 시작->끝 방향으로 고정 혹은 서서히 회전
    FVector DirectionToHole = (TourEndLocation - TourStartLocation).GetSafeNormal();
    NewRotation.Yaw = DirectionToHole.Rotation().Yaw;

    SetActorRotation(NewRotation);

    // 종료 체크
    if (Alpha >= 1.0f)
    {
        StopTourMode();
    }
}

// 4. 투어 모드 종료
void ACameraManager::StopTourMode()
{
    // 다시 Ready 모드로 복귀
    ChangeCameraMode(ECameraMode::Ready);
    UE_LOG(LogTemp, Log, TEXT("📷 Tour Mode Finished"));
}