// =============================================================================
// GolfPlayerController.h - Single AimActor System (Complete File)
// =============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GolfPlayer.h"
#include "GolfBall.h"
#include "TerrainHeightGrid.h"
#include "AimActor.h"
#include "VideoBufferComponent.h"  // ✅ 추가: FVideoFrame 사용
#include "SwingVideoWidget.h"
#include "GolfPlayerController.generated.h"

// 전방 선언들 - 순환 참조 방지
class UGolfShotControlWidget;
class AInGameMode;
class UGolfPlayerManager;
class ACameraManager;
class AWebcamCapture;
class UShotCinematicComponent;

USTRUCT(BlueprintType)
struct FLastShotInfo
{
    GENERATED_BODY()

        UPROPERTY(BlueprintReadWrite)
        FVector ShotDirection = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite)
        float ShotPower = 0.0f;

    UPROPERTY(BlueprintReadWrite)
        float ShotPitchAngle = 0.0f;

    UPROPERTY(BlueprintReadWrite)
        float ShotYawAngle = 0.0f;

    UPROPERTY(BlueprintReadWrite)
        FVector BallStartLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite)
        bool bIsValid = false;

    // 기본 생성자
    FLastShotInfo()
    {
        ShotDirection = FVector::ZeroVector;
        ShotPower = 0.0f;
        ShotPitchAngle = 0.0f;
        ShotYawAngle = 0.0f;
        BallStartLocation = FVector::ZeroVector;
        bIsValid = false;
    }

    // 매개변수 생성자
    FLastShotInfo(const FVector& Direction, float Power, float PitchAngle, float YawAngle, const FVector& StartLocation)
        : ShotDirection(Direction)
        , ShotPower(Power)
        , ShotPitchAngle(PitchAngle)
        , ShotYawAngle(YawAngle)
        , BallStartLocation(StartLocation)
        , bIsValid(true)
    {
    }

    // 유효성 확인
    bool IsValidShot() const
    {
        return bIsValid &&
            !ShotDirection.IsNearlyZero() &&
            ShotPower > 0.0f &&
            !BallStartLocation.IsNearlyZero();
    }
};

UCLASS()
class PARKDAY_API AGolfPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AGolfPlayerController();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void SetupInputComponent() override;
    virtual void BeginDestroy() override;

    UPROPERTY()
    UShotCinematicComponent* ShotCinematicComponent;

    // =================================================================
    // 기본 조작 함수들
    // =================================================================
    void MoveAimHorizontal(float Value);
    void AdjustPower(float Value);
    void OnShot();

    // 디버그 함수들
    void RotateLeft();
    void RotateRight();
    void CameraSetView();
    void DebugInputStatus();

    // =================================================================
    // 물리 디버깅 함수들
    // =================================================================
    UFUNCTION(BlueprintCallable, Category = "Debug")
        void LogCurrentBallPhysics() const;

    UFUNCTION(BlueprintCallable, Category = "Debug")
        void ToggleBallPhysicsDebug();

    UFUNCTION(BlueprintCallable, Category = "Debug")
        bool IsBallMoving() const;

    // =================================================================
    // 샷 조절 UI 관련 함수들
    // =================================================================
    UFUNCTION(BlueprintCallable, Category = "Shot Control")
        void ShowShotControlUI(bool bShow);

    UFUNCTION(BlueprintCallable, Category = "Shot Control")
        void OpenShotControlUI();

    UFUNCTION(BlueprintCallable, Category = "Shot Control")
        void CloseShotControlUI();

    UFUNCTION(BlueprintCallable, Category = "Shot Control")
        void AdjustShotPower(float Delta);

    UFUNCTION(BlueprintCallable, Category = "Shot Control")
        void AdjustShotAngle(float Delta);

    // =================================================================
    // 샷 실행 조건 확인 함수들
    // =================================================================
    UFUNCTION(BlueprintCallable, Category = "Shot Control")
        bool CanExecuteShot() const;

    UFUNCTION(BlueprintCallable, Category = "Shot Control")
        bool IsReadyForShot() const;

    UFUNCTION(BlueprintCallable, Category = "Shot Control")
        FString GetShotBlockReason() const;

    // =================================================================
    // Training Mode 전용 함수들
    // =================================================================
    UFUNCTION(BlueprintCallable, Category = "Training Mode")
        bool CanExecuteShot_TrainingMode() const;

    UFUNCTION(BlueprintCallable, Category = "Training Mode")
        void UpdateMiniMapInfo_TrainingMode();

    UFUNCTION(BlueprintCallable, Category = "Training Mode")
        void OnTrainingModeReset();

    UFUNCTION(BlueprintCallable, Category = "Training Mode")
        void OnTrainingModeToggleBallMovement();

    // =================================================================
    // 게임 모드 확인 함수들
    // =================================================================
    UFUNCTION(BlueprintPure, Category = "Game Mode")
        bool IsInTrainingMode() const;

    UFUNCTION(BlueprintPure, Category = "Game Mode")
        bool IsInStrokeMode() const;

    // =================================================================
    // 마지막 샷 정보 관련 함수들
    // =================================================================
    UPROPERTY(BlueprintReadOnly, Category = "Last Shot")
        FLastShotInfo LastShotInfo;

    UFUNCTION(BlueprintCallable, Category = "Shot Control")
        void RepeatLastShot();

    UFUNCTION(BlueprintCallable, Category = "Shot Control")
        void SaveCurrentShotInfo();

    UFUNCTION(BlueprintCallable, Category = "Shot Control")
        bool HasValidLastShot() const;

    UFUNCTION(BlueprintCallable, Category = "Shot Control")
        FString GetLastShotInfoString() const;

    void OnRepeatLastShot();

    // =================================================================
    // 기타 기본 함수들
    // =================================================================
    void IncreaseFriction();
    void DecreaseFriction();
    void UpdateAim(float DeltaX);
    void UpdatePower(float DeltaY);
    void ExecuteShot();
    void SetMulligan();

    // 현재 플레이어 반환
    AGolfPlayer* GetCurrentGolfPlayer() const;

    // 미니맵 업데이트
    UFUNCTION(BlueprintCallable, Category = "MiniMap")
        void UpdateMiniMapInfo();

    // 미니맵 관련 함수들
    void UpdateMiniMapAim();
    void UpdateMiniMapDistance();

    void CreateShotControlWidget();

    // =================================================================
    // 난이도 및 게임 설정
    // =================================================================
    UFUNCTION(BlueprintCallable, Category = "Golf Settings")
        void ChangeDifficulty(int32 DifficultyLevel);

    // =================================================================
    // 지형 디버그 함수들
    // =================================================================
    UFUNCTION(BlueprintCallable, Category = "Landscape Debug")
        void ToggleLandscapeDebug();

    UFUNCTION(BlueprintCallable, Category = "Landscape Debug")
        void ShowCurrentLandType();

    UFUNCTION(BlueprintCallable, Category = "Landscape Debug")
        void ShowLandTypeGrid();

    // =================================================================
    // 턴 전환 관련 함수들
    // =================================================================
    UFUNCTION(BlueprintCallable, Category = "Turn Transition")
        void SkipTurnTransition();

    UFUNCTION(BlueprintPure, Category = "Turn Transition")
        bool IsAnyBallInTurnTransition() const;

    UFUNCTION(BlueprintPure, Category = "Turn Transition")
        float GetCurrentBallTurnTransitionTime() const;

    // =================================================================
    // TerrainGrid 관련 함수들
    // =================================================================
    UFUNCTION(BlueprintCallable, Category = "Terrain Grid")
        void ToggleTerrainGrid();

        void ShowTerrainGrid(bool bShow = true);

        bool IsTerrainGrid();

    UFUNCTION(BlueprintCallable, Category = "Terrain Grid")
        void UpdateTerrainGridPosition();

    UFUNCTION(BlueprintCallable, Category = "Terrain Grid")
        void SetTerrainGridRadius(float NewRadius);

    UFUNCTION(BlueprintCallable, Category = "Terrain Grid")
        void RefreshTerrainGrid();

    UFUNCTION(BlueprintCallable, Category = "Terrain Grid")
        void HideTerrainGridOnShot();

    // =================================================================
    // Single AimActor 관련 함수들 (수정된 부분)
    // =================================================================
    UFUNCTION(BlueprintCallable, Category = "Aim")
        void InitializeAimActor();

    UFUNCTION(BlueprintCallable, Category = "Aim")
        void UpdateAimActorPosition();

    UFUNCTION(BlueprintPure, Category = "Aim")
        AAimActor* GetAimActor() const { return AimActor; }


    // 미니맵 AimActor 업데이트 함수
    UFUNCTION(BlueprintCallable, Category = "MiniMap")
        void UpdateMiniMapAimActor();


    UFUNCTION(BlueprintCallable, Category = "Aim Sync")
        void SetAimToPosition(const FVector& TargetPosition);

    UFUNCTION(BlueprintCallable, Category = "Aim Sync")
        void RotateCameraToDirection(const FVector& Direction);

    // OB 회피 AimActor 위치 계산 함수들
    UFUNCTION(BlueprintCallable, Category = "Aim")
        FVector FindOptimalAimActorPosition(const FVector& StartLocation, const FVector& Direction, float TargetDistance);

    UFUNCTION(BlueprintCallable, Category = "Aim")
        bool IsPositionValid(const FVector& Position);

    UFUNCTION(BlueprintCallable, Category = "Aim")
        FVector FindSafeDistanceOnLine(const FVector& StartLocation, const FVector& Direction, float MaxDistance);

    UFUNCTION(BlueprintCallable, Category = "Aim")
        FVector FindAlternativeDirection(const FVector& StartLocation, const FVector& Direction, float TargetDistance);

    UFUNCTION(BlueprintCallable, Category = "Aim")
        FVector AdjustToTerrainHeight(const FVector& Position);

    // GolfPlayerController.h (public 섹션에 추가)
    UFUNCTION(BlueprintCallable, Category = "Aim")
        FVector FindSafeDistanceLinear5m(const FVector& StartLocation, const FVector& Direction, float MaxDistance);

    // =================================================================
    // 샷 디버그 함수들
    // =================================================================
    UFUNCTION(BlueprintCallable, Category = "Shot Debug")
        void DebugCurrentBallShot();

    UFUNCTION(BlueprintCallable, Category = "Shot Debug")
        void ForceCurrentBallShot();

    void SetNextHole();
    void OnResetMinimap();

    // =================================================================
    // 스윙 감지 시스템
    // =================================================================
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
        TSubclassOf<class AWebcamCapture> WebcamCaptureClass;

    UPROPERTY(BlueprintReadWrite)
        class AWebcamCapture* WebcamCaptureActor;

    // ✅ 스윙 녹화 완료 시 자동으로 Player_shot 호출 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swing Recording")
        bool bAutoExecutePlayerShotOnSwing = true;

    // ✅ 스윙 이벤트 로그 표시
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swing Recording")
        bool bLogSwingEvents = true;

    // ✅ 스윙 비디오 자동 재생
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swing Recording")
        bool bAutoPlaySwingReplay = true;

    // ✅ 리플레이 재생 지연 시간 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swing Recording",
        meta = (ClampMin = "0.0", ClampMax = "5.0"))
        float SwingReplayDelay = 0.5f;

    // ========== 스윙 녹화 제어 함수 ==========

   // 스윙 녹화 시스템 초기화
    UFUNCTION(BlueprintCallable, Category = "Swing Recording")
        void InitializeSwingRecording();

    // 스윙 녹화 중단
    UFUNCTION(BlueprintCallable, Category = "Swing Recording")
        void StopSwingRecording();

    // 스윙 녹화 재시작
    UFUNCTION(BlueprintCallable, Category = "Swing Recording")
        void RestartSwingRecording();

    // ✅ 스윙 녹화 트리거 (수동 - 키 입력이나 이벤트로 호출)
    UFUNCTION(BlueprintCallable, Category = "Swing Recording")
        void TriggerSwingRecording();

    // ✅ 마지막 스윙 리플레이 재생
    UFUNCTION(BlueprintCallable, Category = "Swing Recording")
        void PlayLastSwingReplay();

    // ✅ 스윙 녹화 시스템 활성 상태 확인
    UFUNCTION(BlueprintPure, Category = "Swing Recording")
        bool IsSwingRecordingActive() const;


    // =================================================================
    // 공개 변수들
    // =================================================================
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player")
        ACameraManager* CameraManager;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player")
        FVector AimDirection;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot", meta = (ClampMin = "10.0", ClampMax = "50.0"))
        float ShotPower;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
        float ShotPitchAngle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
        float ShotYawAngle;

    // UI 관련 변수들
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
        TSubclassOf<UGolfShotControlWidget> ShotControlWidgetClass;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
        UGolfShotControlWidget* ShotControlWidget;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Settings")
        float PowerAdjustStep = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Settings")
        float AngleAdjustStep = 1.0f;

    // 샷 관련 상수들
    static constexpr float CAMERA_AIM_PROJECTION_Z = 0.0f;
    static constexpr float AIM_LINE_CLEAR_DELAY = 0.5f;
    static constexpr float SHOT_FEEDBACK_DURATION = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim Settings")
        bool bLimitAimToHolecup = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim Settings")
        float HolecupDistanceRatio = 0.95f;

    void SetAimToExactPosition(const FVector& ExactPosition);


    UFUNCTION(BlueprintCallable, Category = "Aim Initialization")
        void OnHoleChanged(int32 NewHoleNumber);

    UFUNCTION(BlueprintCallable, Category = "Aim Initialization")
        void OnPlayerIndexChanged(int32 NewPlayerIndex);

    void SetPenaltyDrop();

        UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain Grid")
        bool bTerrainGridVisible;


        void BeginSwingRecording();
        void EndSwingRecording();
        void PrepareNextSwing();

        UPROPERTY(BlueprintReadWrite)
            AWebcamCapture* WebcamCapture;

        UPROPERTY(BlueprintReadWrite)
            USwingVideoWidget* VideoWidget;

        // 샷 감지 시 호출
        UFUNCTION()
            void OnShotDetected(float ShotTime);

        // 클립 재생 완료 시 호출
        UFUNCTION()
            void OnClipFinished();

protected:
    // =================================================================
    // 입력 핸들러들
    // =================================================================
    void OnAimHorizontal(float Value);
    void OnAdjustPower(float Value);

    // 샷 조절 UI 입력 핸들러들
    void OnOpenShotControl();
    void OnPowerUp();
    void OnPowerDown();
    void OnAngleUp();
    void OnAngleDown();
    void OnBounceFix();

    void SetLastHole();
    void ShowScoreBoard();

    void SettingSimpleBall();
    void SettingComplexBall();


    // =================================================================
    // 샷 실행 상태 추적
    // =================================================================
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shot State")
        bool bShotInProgress;

    // =================================================================
    // TerrainHeightGrid 관련 변수들
    // =================================================================
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain Grid")
        ATerrainHeightGrid* TerrainGrid;



    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain Grid Settings")
        float TerrainGridRadius = 3000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain Grid Settings")
        bool bAutoUpdateGridPosition = true;

    // =================================================================
    // Single AimActor 관련 변수들 (수정된 부분)
    // =================================================================
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aim")
        AAimActor* AimActor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim Settings")
        float AimActorDistance = 5000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim Settings")
        float AimActorHeightOffset = 0.0f;

    // =================================================================
    // Training Mode 설정값들
    // =================================================================
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training Mode Settings")
        bool bTrainingModeEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training Mode Settings")
        float TrainingModeQuickShotPower = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training Mode Settings")
        float TrainingModeQuickShotAngle = 15.0f;


    // ✅ 스윙 녹화 완료 이벤트 핸들러 (델리게이트 콜백)
    UFUNCTION()
        void OnSwingRecordedHandler(const TArray<FVideoFrame>& SwingFrames);

private:
    // =================================================================
    // 캐시된 참조들 (전방 선언된 클래스들)
    // =================================================================
    UPROPERTY()
        AInGameMode* CachedGameMode;

    UPROPERTY()
        UGolfPlayerManager* CachedPlayerManager;

    // =================================================================
    // ExecuteShot 관련 헬퍼 함수들
    // =================================================================
    FVector CalculateAimDirection() const;
    void LogShotInfo(const FVector& Direction) const;
    bool ExecuteShotInternal(AGolfPlayer* Player, const FVector& Direction, int32 PlayerIndex);
    void HandleSuccessfulShot(const FVector& Direction);
    void HandleFailedShot();
    void LogBallInfo(class AInGameMode* GameMode) const;
    void ScheduleAimLineClear(AInGameMode* GameMode, int32 PlayerIndex);

    // =================================================================
    // 샷 조절 UI 관련 함수들
    // =================================================================
    void UpdateShotControlWidget();
    bool IsShotControlUIOpen() const;

    // =================================================================
    // 타이머 핸들들
    // =================================================================
    FTimerHandle AimLineClearTimer;
    FTimerHandle DelayedWidgetCreationTimer;

    void ShowErrorMessage();
    void ShowWidgetSetupInstructions();

    // =================================================================
    // 물리 디버깅 관련 변수
    // =================================================================
    bool bShowPhysicsDebug = false;

    // =================================================================
    // 턴 전환 관련 헬퍼 함수들
    // =================================================================
    bool CheckTurnTransitionStatus() const;

    // =================================================================
    // 격자 관련 함수들
    // =================================================================

    void UpdateTerrainGridSettings();

    void SetLandtype();
    void SetParticle();

    // =================================================================
    // Training Mode 전용 타이머
    // =================================================================
    FTimerHandle TrainingResetTimer;
    FTimerHandle TrainingFeedbackTimer;

    // =================================================================
    // 마지막 샷 정보 관련 헬퍼 함수들
    // =================================================================
    void DisplayLastShotInfo() const;
    bool CanRepeatLastShot() const;

    float CalculateLimitedAimDistance(const FVector& BallLocation, const FVector& HolecupLocation, const FVector& TargetDirection) const;

    FVector GetCurrentHolecupPosition() const;
    bool IsAimPositionValid(const FVector& AimPosition, const FVector& BallPosition) const;

   public:
       // 미니맵 플레이어 전환 관련 함수
       UFUNCTION(BlueprintCallable, Category = "MiniMap")
           void NotifyMiniMapPlayerChanged(int32 NewPlayerIndex, int32 nPreviousPlayerIndex = -1);

       UFUNCTION(BlueprintCallable, Category = "MiniMap")
           void UpdateMiniMapForCurrentPlayerOnly();

       void UpdateAimActorByRotation(float DeltaYaw);

       void UpdateMiniMapForRotation();

       void ShowSwingVideoWidget();

       void ShowSwingMovieWidget();

       void InitializeTerrainGrid();

private:
    // 이전 플레이어 인덱스 추적
    int32 PreviousPlayerIndex = -1;

    // 미니맵 클릭 플래그 유지용 타이머
    FTimerHandle MinimapClickFlagTimer;



    // 수동 회전 중인지 추적하는 플래그
    bool bIsManuallyRotating = false;

    // 수동 회전 타이머
    FTimerHandle ManualRotationTimer;

    // ⭐ AimActor 회전 각도 저장
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aim", meta = (AllowPrivateAccess = "true"))
        float AimActorYaw = 0.0f;


    // 플레이어 스테이트의 Player_shot 호출 가능 여부 체크
    bool CanCallPlayerShot() const;

    // 플레이어 스테이트의 Player_shot 호출
    void CallPlayerShot();

    // 스윙 리플레이 지연 재생
    void PlaySwingReplayDelayed();



    // ========== 상태 추적 ==========

    // 마지막 스윙 녹화 시간 (쿨다운용)
    float LastSwingRecordingTime = 0.0f;

    // 스윙 녹화 쿨다운 (초)
    UPROPERTY(EditAnywhere, Category = "Swing Recording", meta = (AllowPrivateAccess = "true"))
        float SwingRecordingCooldown = 1.0f;

    // 리플레이 타이머
    FTimerHandle SwingReplayTimer;

    // 비동기 저장을 위한 헬퍼 함수
    void SaveFrameAsync(int32 FrameIndex, TRefCountPtr<class FRHITexture2D> TextureRHI, FString SavePath);

    int32 SwingRecordingRetryCount = 0;
    static constexpr int32 MAX_RETRIES = 50;

    bool bIsRecordingSwing = false;
    int32 TargetFramesForSwing = 150;

    FTimerHandle HideSwingVideoWidgetTimer;

    void AutoHideSwingVideoWidget();

public:
    // 첫 샷 전용 에임 설정 함수
    UFUNCTION(BlueprintCallable, Category = "First Shot Aim")
        void SetFirstShotAim(const FVector& TeePosition, const FVector& HolecupPosition);

    // 첫 샷 여부 확인
    UFUNCTION(BlueprintPure, Category = "First Shot Aim")
        bool IsFirstShot() const;

    // ⭐ 새로운 함수: AimActor 기준으로 카메라 위치 조정
    UFUNCTION(BlueprintCallable, Category = "Aim")
        void PositionCameraForAim();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swing Recording")
        bool bEnableVideoSaving = true;  // ⭐ 새로 추가: 영상 저장 활성화 여부 (기본 true)

    // ⭐ 새로 추가: 영상 저장 토글 함수 (블루프린트 호출 가능)
    UFUNCTION(BlueprintCallable, Category = "Swing Recording")
        void ToggleVideoSaving(bool bEnable);

    UFUNCTION(BlueprintCallable, Category = "Swing Recording")
        void SetVideoSavingEnabled(bool bEnable);

    // ✅ 캡처 준비 완료 대기 함수
    UFUNCTION(BlueprintCallable, Category = "Swing Recording")
        void WaitForCaptureReady();

    // ✅ 캡처 준비 상태 확인
    UFUNCTION(BlueprintPure, Category = "Swing Recording")
        bool IsCaptureReady() const;



    UFUNCTION(BlueprintCallable, Category = "UI")
        void HideSwingVideoWidget();



};

// =============================================================================
// 주석: 사용법 및 주요 변경사항
// =============================================================================

/*
주요 변경사항:
1. Single AimActor System 구현
   - 하나의 AimActor만 사용
   - 현재 플레이어에게만 작용
   - 플레이어 전환 시 AimActor 위치 자동 업데이트

2. 제거된 멤버들:
   - TMap<int32, AAimActor*> PlayerAimActors (제거)
   - TMap<int32, FVector> PlayerAimActorPositions (미니맵에서 관리)

3. 새로운 함수들:
   - UpdateMiniMapAimActor() - 미니맵 AimActor 업데이트 전용
   - ValidateAimActorPosition() - AimActor 위치 검증
   - TestAimActorMiniMapMatching() - 좌표 매칭 테스트

4. 최적화된 동작:
   - 현재 플레이어만 AimActor 업데이트
   - 메모리 사용량 감소
   - 처리 속도 향상

사용법:
- 플레이어 전환 시 InGameMode에서 OnPlayerIndexChanged() 호출
- AimActor는 자동으로 새로운 현재 플레이어 기준으로 재배치
- 미니맵에서 이전 플레이어 AimActor 이미지 자동 숨김
*/