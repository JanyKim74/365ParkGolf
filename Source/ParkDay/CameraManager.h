#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GolfDataStructures.h"
#include "Engine/World.h"
#include "CameraManager.generated.h"


class AGolfBall;
class AGolfPlayerController;
class UCameraComponent;
class AInGameMode; // ⭐ 추가: AInGameMode 전방 선언
class UGolfPlayerManager; // ⭐ 추가: UGolfPlayerManager 전방 선언


/**
 * Golf game camera manager
 * Handles different camera behaviors based on game state
 */
UCLASS()
class PARKDAY_API ACameraManager : public AActor
{
    GENERATED_BODY()

public:
    ACameraManager();
    virtual void Tick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void BeginPlay() override;

    AActor* SpawnInFrontOfCamera(UWorld* World, TSubclassOf<AActor> ClassToSpawn, float Distance = 200.f);
    //--====//

    //

    // Camera mode management
    UFUNCTION(BlueprintCallable, Category = "Camera")
    void ChangeCameraMode(ECameraMode NewMode);

    // Target ball management
    UFUNCTION(BlueprintCallable, Category = "Camera")
    void SetTargetBall(AGolfBall* NewTargetBall);


    // Get current hole cup position
    UFUNCTION(BlueprintCallable, Category = "Camera")
    FVector GetCurrentHolecupPosition() const;


    UFUNCTION(BlueprintCallable, Category = "Camera")
    void SetCameraMode(ECameraMode NewMode, bool bForceUpdate = false);

    UFUNCTION(BlueprintCallable, Category = "Camera")
    float GetCurrentBallSpeed() const;

    UFUNCTION(BlueprintCallable, Category = "Camera")
    bool IsBallMoving() const;

    UFUNCTION(BlueprintCallable, Category = "Camera")
    void ClearAllTimers();

    // Camera component
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UCameraComponent* Camera;

    UFUNCTION(BlueprintCallable, Category = "Camera")
    void ForceReadyMode();

    UFUNCTION(BlueprintCallable, Category = "Camera")
    void SetReadyModeDelay(float DelaySeconds);

    UFUNCTION(BlueprintPure, Category = "Camera")
    bool IsBallCompletelyStoppedForCamera() const;

    UFUNCTION(BlueprintPure, Category = "Camera")
    float GetReadyModeDelayTime() const { return ReadyModeDelayTime; }

    // ===== 카메라 고정 관련 함수들 =====
    UFUNCTION(BlueprintCallable, Category = "Camera")
    void UnfreezeCameraImmediate();

    UFUNCTION(BlueprintCallable, Category = "Camera")
    void SetCameraFreezeTime(float NewFreezeTime);

    UFUNCTION(BlueprintPure, Category = "Camera")
    bool IsCameraFrozen() const { return bBallStoppedForCamera; }

    UFUNCTION(BlueprintPure, Category = "Camera")
    float GetCameraFreezeTimeRemaining() const;

    // ⭐ 새로 추가: 동기화 함수들
    UFUNCTION()
    void OnBallStateChanged(AGolfBall* Ball, EBallState PreviousState, EBallState NewState);

    // 더 빠른 직접 호출용 (델리게이트 오버헤드 없음)
    void OnBallStateChangedImmediate(AGolfBall* Ball, EBallState PreviousState, EBallState NewState);

    // 카메라 모드 매핑 함수
    UFUNCTION(BlueprintPure, Category = "Camera Logic")
    ECameraMode GetRequiredCameraModeForBallState(EBallState BallState, float BallSpeed = 0.0f) const;

    // 동기화 상태 검증
    UFUNCTION(BlueprintCallable, Category = "Camera Debug")
    bool IsSyncedWithBall();

    // 강제 재동기화
    UFUNCTION(BlueprintCallable, Category = "Camera Debug")
    void ForceSyncWithBall();

    AGolfBall* GetTargetBall() { return TargetBall; }

    // Camera settings constants
 /*    FOV = 60 
    static constexpr float CAMERA_FREEZE_TIME = 4.0f;  // 기존 4초 유지
    static constexpr float DEFAULT_FOV = 60.0f;
    static constexpr float READY_DISTANCE = 350.0f;
    static constexpr float READY_HEIGHT = 80.0f;
    static constexpr float FLYING_DISTANCE = 400.0f;
    static constexpr float FLYING_HEIGHT = 150.0f;
    static constexpr float FOLLOWING_DISTANCE = 400.0f;
    static constexpr float FOLLOWING_HEIGHT = 150.0f;
    static constexpr float BALL_SCREEN_OFFSET = 0.3f;
    static constexpr float SHOT_WATCH_DURATION = 0.5f;  // 약간 늘림
    static constexpr float MIN_BALL_SPEED_THRESHOLD = 30.0f;  // 임계값 조정
    static constexpr float TRANSITION_SPEED_THRESHOLD = 500.0f; // 모드 전환 임계값
*/
    //FOV = 90
    static constexpr float CAMERA_FREEZE_TIME = 4.0f;  // 기존 4초 유지
    static constexpr float DEFAULT_FOV = 90.0f;
    static constexpr float READY_DISTANCE = 220.0f;
    static constexpr float READY_HEIGHT = 80.0f;
    static constexpr float FLYING_DISTANCE = 300.0f;
    static constexpr float FLYING_HEIGHT = 150.0f;
    static constexpr float FOLLOWING_DISTANCE = 300.0f;
    static constexpr float FOLLOWING_HEIGHT = 150.0f;
    static constexpr float BALL_SCREEN_OFFSET = 0.3f;
    static constexpr float SHOT_WATCH_DURATION = 0.5f;  // 약간 늘림
    static constexpr float MIN_BALL_SPEED_THRESHOLD = 30.0f;  // 임계값 조정
    static constexpr float TRANSITION_SPEED_THRESHOLD = 500.0f; // 모드 전환 임계값



    // Camera settings constants - Stop 모드
    static constexpr float STOP_DURATION = 4.0f;           // Stop 모드 지속 시간
    static constexpr float STOP_HEIGHT_OFFSET = 20.0f;     // Stop 모드 시 카메라 높이 오프셋
    static constexpr float RESULT_DISPLAY_DISTANCE = 300.0f; // 결과 표시용 카메라 거리
    static constexpr float CLOSE_TO_HOLE_THRESHOLD_CM = 500.0f;

    // ⭐ Stop 모드 제어 함수들
    UFUNCTION(BlueprintCallable, Category = "Camera Stop")
    void ForceStopMode();

    UFUNCTION(BlueprintCallable, Category = "Camera Stop")
    void SkipStopMode();

    UFUNCTION(BlueprintPure, Category = "Camera Stop")
    bool IsInStopMode() const { return bInStopMode; }

    UFUNCTION(BlueprintPure, Category = "Camera Stop")
    float GetStopModeTimeRemaining() const;

    UFUNCTION(BlueprintCallable, Category = "Camera Stop")
    void SetStopModeDuration(float NewDuration);

    // ⭐ 새로 추가: Fixed 모드 관련 함수들
    UFUNCTION(BlueprintCallable, Category = "Camera Fixed")
    void SetFixedCameraPosition(FVector NewPosition, FRotator NewRotation);

    UFUNCTION(BlueprintCallable, Category = "Camera Fixed")
    void ForceFixedMode();

    UFUNCTION(BlueprintPure, Category = "Camera Fixed")
    bool IsInFixedMode() const { return CameraMode == ECameraMode::Fixed; }

    UFUNCTION(BlueprintCallable, Category = "Camera Fixed")
    FVector GetFixedCameraPosition() const { return FixedCameraLocation; }

    UFUNCTION(BlueprintCallable, Category = "Camera Fixed")
    FRotator GetFixedCameraRotation() const { return FixedCameraRotation; }

    // ⭐ 새로 추가: Ready 상태에서 고정 모드 유지 옵션
    UFUNCTION(BlueprintCallable, Category = "Camera Fixed")
    void SetUseFixedModeInReady(bool bUse) { bUseFixedModeInReady = bUse; }

    UFUNCTION(BlueprintPure, Category = "Camera Fixed")
    bool IsUsingFixedModeInReady() const { return bUseFixedModeInReady; }

    // ⭐ 새로 추가: 5미터 내 샷 시 카메라 고정 기능
    UFUNCTION(BlueprintCallable, Category = "Camera")
    void SetCloseToHoleShotFixedMode(bool bEnable) { bEnableCloseToHoleShotFixed = bEnable; }

    UFUNCTION(BlueprintPure, Category = "Camera")
    bool IsCloseToHoleShotFixedEnabled() const { return bEnableCloseToHoleShotFixed; }

    UFUNCTION(BlueprintPure, Category = "Camera")
    bool IsCameraFixedForCloseShot() const { return bIsCameraFixedForCloseShot; }

    UFUNCTION(BlueprintCallable, Category = "Camera")
    void ReleaseCloseToHoleShotFixed();
    // ⭐ 새로 추가: 5미터 내 샷 감지 및 처리
    bool IsCloseToHoleShot() const;
    void HandleCloseToHoleShot();

    // 카메라 Forward 방향 가져오기
    FVector GetCameraForwardDirection() const;

    void PositionCameraForHoleView(const FVector& BallLocation, const FVector& HolecupLocation);
    void PositionCameraForAimView(const FVector& BallLocation, const FVector& AimLocation);

    // ⭐ 새로 추가: CameraMode Getter
    UFUNCTION(BlueprintPure, Category = "Camera")
    ECameraMode GetCameraMode() const { return CameraMode; }

    // ⭐ 새로 추가: Ready 모드 확인 헬퍼 함수
    UFUNCTION(BlueprintPure, Category = "Camera")
    bool IsInReadyMode() const { return CameraMode == ECameraMode::Ready; }

protected:
    // Target ball reference
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Camera")
    AGolfBall* TargetBall;

    // Current camera mode
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Camera")
    ECameraMode CameraMode;

    // Camera settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings")
    float CameraDistanceFromBall = READY_DISTANCE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings")
    float CameraHeightFromBall = READY_HEIGHT;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings")
    float BallOffsetFromCenter = BALL_SCREEN_OFFSET;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings")
    float CameraRotation = 0.0f;

    // Interpolation settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings",
        meta = (ClampMin = "0.1", ClampMax = "10.0"))
    float InterpSpeed = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings",
        meta = (ClampMin = "0.1", ClampMax = "20.0"))
    float RotationInterpSpeed = 5.0f;

    // Initialization
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    bool bIsInitialized = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float InitializationDelay = 1.0f;

    // ===== 물리 추적 변수들 =====
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera State")
    FVector LastBallVelocity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera State")
    float LastBallSpeed;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera State")
    FVector LastMoveDirection;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera State")
    float ShotWatchTime;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera State")
    bool bWasMovingLastFrame;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings",
        meta = (ClampMin = "1.0", ClampMax = "10.0"))
    float ReadyModeDelayTime = 4.0f;  // 기본 4초

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera Settings")
    bool bShowTransitionCountdown = true;  // 카운트다운 표시 여부

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera State")
    bool bBallStoppedForCamera = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera State")
    float BallStopTime = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera State")
    FVector FrozenCameraLocation;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera State")
    FRotator FrozenCameraRotation;

    // 고정 시간 설정 (블루프린트에서 조정 가능)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings",
        meta = (ClampMin = "1.0", ClampMax = "10.0"))
    float CameraFreezeTime = 4.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    AGolfPlayerController* PlayerController;

    UFUNCTION(BlueprintCallable, Category = "Camera")
    void SetPlayerController(AGolfPlayerController* NewController);

    // ⭐ Stop 모드 관련 변수들
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera Stop")
    bool bInStopMode = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera Stop")
    float StopModeStartTime = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera Stop")
    FVector StopModeCameraLocation;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera Stop")
    FRotator StopModeCameraRotation;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera Stop")
    bool bShowResultUI = true;  // 결과 UI 표시 여부

    // FOLLOWING 모드 진입 시 공의 초기 속도 저장
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera State")
    float InitialFollowingSpeed = 0.0f;

    // FOLLOWING 모드 대기 시간 관리
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera State")
    float FollowingWaitTime = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera State")
    bool bIsWaiting = false;
    // ⭐ 새로 추가: Fixed 모드 관련 변수들
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Fixed")
    FVector FixedCameraLocation = FVector(0.0f, 0.0f, 500.0f); // 기본 고정 위치

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Fixed")
    FRotator FixedCameraRotation = FRotator(-45.0f, 0.0f, 0.0f); // 기본 고정 회전

    // ⭐ 새로 추가: Ready 상태에서 고정 모드 유지 옵션
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Fixed")
    bool bUseFixedModeInReady = false;

    // ⭐ 최적화: 캐시된 GameMode 및 PlayerManager
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimization", meta = (AllowPrivateAccess = "true"))
    AInGameMode* CachedGameMode; // ⭐ 추가

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimization", meta = (AllowPrivateAccess = "true"))
    UGolfPlayerManager* CachedPlayerManager; // ⭐ 추가



    // ⭐ 새로 추가: 5미터 내 샷 시 카메라 고정 관련 변수들
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Close Shot")
    bool bEnableCloseToHoleShotFixed = true; // 기본적으로 활성화

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera Close Shot")
    bool bIsCameraFixedForCloseShot = false; // 현재 5미터 내 샷으로 인해 고정 중인지

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera Close Shot")
    FVector CloseShotFixedLocation; // 5미터 내 샷 시 고정된 카메라 위치

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera Close Shot")
    FRotator CloseShotFixedRotation; // 5미터 내 샷 시 고정된 카메라 회전
    // ⭐ 추가된 기능
// 홀컵과 공의 거리가 5미터 이내인지 확인
    UFUNCTION(BlueprintCallable, Category = "Camera")
    bool IsBallNearHoleCup() const;


    FVector EnforceGroundClearance(const FVector& CameraPosition, float MinClearance /*= 50.0f*/) const;


    // 공 뒤쪽 목표 거리 (cm). 클수록 공에서 멀리서 따라감
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings|Follow",
        meta = (ClampMin = "200.0", ClampMax = "1200.0"))
    float FollowDesiredDistance = 600.0f;

    // 카메라 높이 오프셋 (지면 기준 cm)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings|Follow",
        meta = (ClampMin = "30.0", ClampMax = "400.0"))
    float FollowHeightOffset = 100.0f;

    // 위치 보간 속도. 낮을수록 더 느리게 따라감 (권장: 1.0~2.5)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings|Follow",
        meta = (ClampMin = "0.2", ClampMax = "6.0"))
    float FollowPosInterpSpeed = 2.8f;

    // 회전 보간 속도. 낮을수록 더 느리게 돌아봄 (권장: 2.0~5.0)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings|Follow",
        meta = (ClampMin = "0.2", ClampMax = "10.0"))
    float FollowRotInterpSpeed = 3.0f;

    // 고속 비행 시 위치 보간 배율 (0~1, 낮을수록 빠를 때 더 느리게)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings|Follow",
        meta = (ClampMin = "0.1", ClampMax = "1.0"))
    float FollowHighSpeedScale = 0.75f;

    // 고속 판정 기준 속도 (cm/s)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings|Follow",
        meta = (ClampMin = "100.0", ClampMax = "2000.0"))
    float FollowHighSpeedThreshold = 1000.0f;

    // EaseIn 지속 시간 (초, 대기 종료 후 추적 가속 구간)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Settings|Follow",
        meta = (ClampMin = "0.3", ClampMax = "4.0"))
    float FollowEaseInDuration = 2.0f;


private:
    // Update functions for different camera modes
    void UpdateReadyCamera(float DeltaTime);
    void UpdateFollowingCamera(float DeltaTime);
    void UpdateFlyingCamera(float DeltaTime);
    void UpdateStopCamera(float DeltaTime);
    void UpdateFixedCamera(float DeltaTime); // ⭐ New fixed mode update

    // Camera positioning
    FVector CalculateOptimalCameraPosition(const FVector& BallLocation, const FVector& HolecupLocation);

    // Ball physics monitoring
    void UpdateBallPhysicsTracking();
    void CheckForAutomaticModeTransition();

    // Utility functions
    void ActivateAsMainCamera();
    void ApplyCameraModeSettings();
    bool IsValidTargetBall() const;

    // Smooth transitions
    void SmoothCameraTransition(const FVector& TargetPosition, const FRotator& TargetRotation, float DeltaTime);

    // 카메라 전환 카운트다운 업데이트
    void UpdateCameraTransitionCountdown();

    // 개선된 모드 전환 체크
    void CheckForAutomaticModeTransitionImproved();


    // ===== 타이머 핸들들 =====
    FTimerHandle ModeTransitionTimer;
    FTimerHandle CameraResetTimer;
    FTimerHandle StopModeTimer;
    FTimerHandle ResultDisplayTimer;

    // ⭐ 동기화 관련 변수들
    UPROPERTY()
    EBallState LastKnownBallState;

    float LastSyncTime;
    bool bIsSyncing;

    // 내부 동기화 함수
    void PerformCameraSync(EBallState BallState, float BallSpeed, bool bImmediate = true);

    bool IsStatesSynchronized() const;
    void CheckStateSynchronization();

    // Stop 모드 진입/종료 처리
    void EnterStopMode();
    void ExitStopMode();

    // 결과 표시 관련
    void ShowBallResult();
    void UpdateStopModeCountdown();

    // Stop 모드용 카메라 위치 계산
    FVector CalculateStopCameraPosition() const;
    FRotator CalculateStopCameraRotation() const;

    /** 샷 이후 카메라 추적을 1초간 지연시키기 위한 플래그 */
    bool bIsWaitingForFollowDelay;

    /** 샷 이후 지연 시간 카운트다운 타이머 */
    float FollowDelayTimer;
    // FOLLOWING 모드의 자연스러운 이동을 위한 시간 추적
    float ElapsedFollowingTime = 0.0f;

    // Snapshot at wait-end to avoid jump when tracking starts
    FVector FollowWaitEndCameraPos = FVector::ZeroVector;
    FVector FollowWaitEndTargetPos = FVector::ZeroVector;
    float EaseInElapsed = 0.0f;

    // ===== 성능 최적화: 프레임 단위 캐시 =====

    // UpdateFollowingCamera LineTrace 3회 → 1회 캐시
    // 볼 지면 / 카메라 지면 / 최종보정 세 곳에서 재사용
    bool  bFrameGroundCacheValid = false;   // 이 프레임에 캐시가 유효한지
    float CachedBallGroundZ = 0.0f;   // 볼 아래 지면 Z
    bool  bCachedGroundHit = false;  // 지면 히트 여부
    FVector CachedBallGroundHitLoc;         // 히트 위치

    // UpdateReadyCamera GetPlayerController 3회 → 1회 캐시
    // Stroke/Training/Range 모드 분기마다 동일 호출 반복
    AGolfPlayerController* CachedReadyPC = nullptr;  // Ready 카메라용 PC 캐시
    bool bReadyPCCacheValid = false;                 // 프레임 내 유효 여부

    // ===== Following 카메라 Z 스무딩 =====
// 공 Z 직접 참조 → 바운스 진동이 카메라에 그대로 전달되는 문제 방지
    float SmoothedGroundZ = 0.f;   // 보간된 지면 Z (매 프레임 직접 변경 안 함)
    bool  bGroundZInitialized = false; // 첫 프레임 초기화 여부

public:
    // ⭐ 새로 추가: 카메라 모드 옵션 관련 함수들
    UFUNCTION(BlueprintCallable, Category = "Camera Mode Option")
    void SetCameraModeOption(int32 CameraModeOption);

    UFUNCTION(BlueprintPure, Category = "Camera Mode Option")
    int32 GetCameraModeOption() const;

    UFUNCTION(BlueprintCallable, Category = "Camera Mode Option")
    void CheckAndApplyCameraModeOption();

    // ⭐ 강제 카메라 모드 상태 확인 함수
    UFUNCTION(BlueprintPure, Category = "Camera Mode Option")
    bool IsUsingForcedCameraMode() const { return bUseForcedCameraMode; }

    // ⭐ [추가] 투어 모드 시작 함수 (블루프린트에서 버튼 클릭 시 호출)
    UFUNCTION(BlueprintCallable, Category = "Camera Tour")
    void StartTourMode();

    // ⭐ [추가] 투어 모드 종료 함수
    UFUNCTION(BlueprintCallable, Category = "Camera Tour")
    void StopTourMode();

    // ⭐ [추가] 투어 중인지 확인
    UFUNCTION(BlueprintPure, Category = "Camera Tour")
    bool IsInTourMode() const { return CameraMode == ECameraMode::Tour; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Mode Option")
    bool bUsePartialFixedMode = false; // 부분 고정 모드 (Ready=자유, Flying/Following/Stop=고정)
protected:
    // ⭐ 새로 추가: 강제 카메라 모드 관련 변수들
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Mode Option")
    bool bUseForcedCameraMode = false; // 강제 카메라 모드 사용 여부

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Mode Option")
    ECameraMode ForcedCameraMode = ECameraMode::Fixed; // 강제할 카메라 모드

    // ⭐ 새로 추가: 부분 고정 카메라 모드 관련 변수 (이 변수가 누락되어 컴파일 오류 발생)


    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class USphereComponent* CameraCollision;

    FVector EnsureMinimumGroundClearance(const FVector& CameraPosition, float MinClearance) const;

    // ⭐ [추가] 투어 모드 업데이트 루프
    void UpdateTourCamera(float DeltaTime);

    // ⭐ [추가] 투어 관련 변수들
    UPROPERTY(VisibleAnywhere, Category = "Camera Tour")
    FVector TourStartLocation;

    UPROPERTY(VisibleAnywhere, Category = "Camera Tour")
    FVector TourEndLocation;

    UPROPERTY(VisibleAnywhere, Category = "Camera Tour")
    float TourElapsedTime;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Tour")
    float TourDuration = 5.0f; // 5초 동안 이동

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Tour")
    float TourArcHeight = 2000.0f; // 이동 중 최대 높이 (아치형 이동)

};