// ============================================================================
// GolfBall.h - 궤적 추적 시스템이 추가된 파크골프볼 헤더
// ============================================================================

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GolfDataStructures.h"
#include "Dom/JsonObject.h"
#include "GolfMinimap.h"
#include "LandscapeChecker.h"  // ⭐ 추가: LandscapeChecker include
#include "GolfBall.generated.h"

#define SAFE_GET_MASS(MeshComponent) \
    (MeshComponent && MeshComponent->IsSimulatingPhysics() ? MeshComponent->GetMass() : 0.035f)

#define SAFE_SET_MASS(MeshComponent, Mass) \
    if (MeshComponent) {  MeshComponent->BodyInstance.SetMassOverride(Mass); }

// 전방 선언
class UStaticMeshComponent;
class UGolfPlayerManager;
class AInGameMode;
class AGolfPlayerController;
class ACameraManager;  // ⭐ 추가
class UCameraFXComponent;
class UChildActorComponent;
class ABoomLine;
class UBillboardComponent;
class UBallNamePlateComponent;

UENUM(BlueprintType)
enum class EPhysicsState : uint8
{
    Disabled,    // 물리 끔, 충돌 끔 (Init, 숨김 상태)
    Static,      // 물리 끔, 충돌 켜짐 (Ready 상태)
    Simulating   // 물리 켜짐, 충돌 켜짐 (Fly, Bound 상태)
};
// 궤적 포인트 구조체
USTRUCT(BlueprintType)
struct FTrajectoryPoint
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Position;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Speed;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TimeStamp;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EBallState BallState;

    FTrajectoryPoint()
    {
        Position = FVector::ZeroVector;
        Speed = 0.0f;
        TimeStamp = 0.0f;
        BallState = EBallState::Ball_Init;
    }

    FTrajectoryPoint(const FVector& InPosition, float InSpeed, float InTime, EBallState InState)
        : Position(InPosition), Speed(InSpeed), TimeStamp(InTime), BallState(InState) {
    }
};


// 궤적 설정 구조체
USTRUCT(BlueprintType)
struct FTrajectorySettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
    bool bShowTrajectory = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
    bool bShowSpeedColors = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
    bool bShowStateMarkers = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
    float TrajectoryDuration = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
    float PointInterval = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
    float LineThickness = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
    int32 MaxTrajectoryPoints = 500;

    FTrajectorySettings()
    {
        bShowTrajectory = true;
        bShowSpeedColors = true;
        bShowStateMarkers = true;
        TrajectoryDuration = 5.0f;
        PointInterval = 0.1f;
        LineThickness = 3.0f;
        MaxTrajectoryPoints = 500;
    }
};

USTRUCT(BlueprintType)
struct FBallTrailSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail")
    bool bShowTrail = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail")
    float TrailDuration = 3.0f; // 꼬리 지속 시간 (초)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail")
    float TrailUpdateInterval = 0.05f; // 꼬리 업데이트 간격

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail")
    float TrailThickness = 2.0f; // 꼬리 두께

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail")
    float MinSpeedForTrail = 10.0f; // 꼬리 표시 최소 속도

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail")
    int32 MaxTrailPoints = 500; // 최대 꼬리 포인트 수

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail")
    bool bUseSpeedBasedColors = true; // 속도별 색상 사용

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail")
    bool bUseFadeEffect = true; // 페이드 효과 사용

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail")
    FLinearColor TrailBaseColor = FLinearColor::White;

    FBallTrailSettings()
    {
        bShowTrail = true;
        TrailDuration = 1.5f;
        TrailUpdateInterval = 0.05f;
        TrailThickness = 5.0f;
        MinSpeedForTrail = 10.0f;
        MaxTrailPoints = 500;
        bUseSpeedBasedColors = true;
        bUseFadeEffect = true;
        TrailBaseColor = FLinearColor::White;
    }
};

// Trail 포인트 구조체
USTRUCT(BlueprintType)
struct FTrailPoint
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Position;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Speed;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TimeStamp;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Alpha; // 투명도 (0-1)

    FTrailPoint()
    {
        Position = FVector::ZeroVector;
        Speed = 0.0f;
        TimeStamp = 0.0f;
        Alpha = 0.7f;
    }

    FTrailPoint(const FVector& InPosition, float InSpeed, float InTime, float InAlpha = 0.7f)
        : Position(InPosition), Speed(InSpeed), TimeStamp(InTime), Alpha(InAlpha) {
    }
};


// 파크골프 상수 구조체 (파일에서 로드)
USTRUCT(BlueprintType)
struct FParkGolfConstants
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed")
    float MIN_SPEED = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed")
    float MAX_SPEED = 25.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed")
    float TYPICAL_SPEED = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Angle")
    float MIN_LAUNCH_ANGLE = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Angle")
    float MAX_LAUNCH_ANGLE = 12.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Angle")
    float TYPICAL_LAUNCH_ANGLE = 4.0f;

    FParkGolfConstants()
    {
        MIN_SPEED = 1.0f;
        MAX_SPEED = 25.0f;
        TYPICAL_SPEED = 15.0f;
        MIN_LAUNCH_ANGLE = 0.5f;
        MAX_LAUNCH_ANGLE = 12.0f;
        TYPICAL_LAUNCH_ANGLE = 4.0f;
    }
};


UENUM(BlueprintType)
enum class EShotType : uint8
{
    TeeShot = 0,      // 티샷 (처음 공 발사)
    NormalShot = 1    // 일반샷 (그 이후 샷)
};

// ⭐⭐⭐ 샷 조정값을 위한 구조체 추가 (Line 221 근처에 추가)
USTRUCT(BlueprintType)
struct FShotAdjustments
{
    GENERATED_BODY()

    // ⭐ 기본 조정값 (하위 호환성)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Adjustments")
    float SpeedMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Adjustments")
    float SpeedOffset = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Adjustments")
    float PitchAngleOffset = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Adjustments")
    float YawAngleOffset = 0.0f;

    // ⭐ 티샷 전용 조정값
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Adjustments|Tee Shot")
    float TeeSpeedMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Adjustments|Tee Shot")
    float TeeSpeedOffset = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Adjustments|Tee Shot")
    float TeePitchAngleOffset = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Adjustments|Tee Shot")
    float TeeYawAngleOffset = 0.0f;

    // ⭐ 일반샷 전용 조정값
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Adjustments|Normal Shot")
    float NormalSpeedMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Adjustments|Normal Shot")
    float NormalSpeedOffset = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Adjustments|Normal Shot")
    float NormalPitchAngleOffset = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Adjustments|Normal Shot")
    float NormalYawAngleOffset = 0.0f;

    FShotAdjustments()
        : SpeedMultiplier(1.0f), SpeedOffset(0.0f),
        PitchAngleOffset(0.0f), YawAngleOffset(0.0f),
        TeeSpeedMultiplier(1.0f), TeeSpeedOffset(0.0f),
        TeePitchAngleOffset(0.0f), TeeYawAngleOffset(0.0f),
        NormalSpeedMultiplier(1.0f), NormalSpeedOffset(0.0f),
        NormalPitchAngleOffset(0.0f), NormalYawAngleOffset(0.0f)
    {
    }
};
// 물리 설정 구조체
USTRUCT(BlueprintType)
struct FBallPhysicsConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float BaseLinearDamping = 0.3f;  // 증가: 구름 속도 빨리 줄임 (기존 0.4f → 0.3f로 조정, 더 부드럽게)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float BaseAngularDamping = 0.25f;  // 증가: 회전(스핀) 빨리 멈춤 (기존 0.2f → 0.25f)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float RollingFriction = 0.2f;  // 증가: 구름 마찰 높여 거리 줄임 (기존 0.25f → 0.35f, 잔디에서 빨리 멈춤)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float BounceDamping = 0.4f;  // 감소: 바운스 에너지 손실 줄여 많이 튕김 (기존 0.9f → 0.6f, 낮은 각도에서 2~3회 바운스 자연스럽게)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float AirResistance = 0.025f;  // 증가: 공기 저항으로 낮은 각도 비행/구름 제어 (기존 0.015f → 0.025f)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float GravityScale = 1.0f;  // 유지 or 약간 증가: 중력으로 바운스 높이 제어 (기존 0.8f → 1.0f, 현실적)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float ForwardSpinFactor = 500.0f;  // 감소: 탑스핀 줄여 구름 적게 (기존 500.0f → 300.0f, 과도한 구름 방지)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float MaxBounceSpeedRatio = 2.0f;   //볼의 기존 속도 n배 체크

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float MinPreImpactSpeed = 1000.f;   //볼 튐 방지 최소 속도

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float MinBounceFixHeight = 50.f;   //볼 튐 방지 최소 높이

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float TeeShotPowerModify = 0.85f;     //티샷 파워 배율

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float SecondShotPowerModify = 1.f;     //세컨드 샷 파워 배율

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float MulltiflyGrassCondition = 0.1f;     //잔디상태에 따른 마찰력 계수

    FBallPhysicsConfig()
    {
        BaseLinearDamping = 0.3f;
        BaseAngularDamping = 0.25f;
        RollingFriction = 0.2f;
        BounceDamping = 0.4f;
        AirResistance = 0.025f;
        GravityScale = 0.8f;
        ForwardSpinFactor = 500.0f;
        MaxBounceSpeedRatio = 2.0f;
        MinBounceFixHeight = 100.f;
        MinPreImpactSpeed = 1000.f;
        TeeShotPowerModify = 0.85f;
        SecondShotPowerModify = 1.f;
        MulltiflyGrassCondition = 0.1f;
    }
};


// 지형별 물리 설정 구조체
USTRUCT(BlueprintType)
struct FTerrainPhysicsSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain Physics")
    float RollingFriction = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain Physics")
    float BounceDamping = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain Physics")
    float LinearDamping = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain Physics")
    float AngularDamping = 0.12f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain Physics")
    float AirResistance = 0.025f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain Physics")
    FString TerrainName = TEXT("Default");

    FTerrainPhysicsSettings()
    {
        RollingFriction = 0.25f;
        BounceDamping = 0.5f;
        LinearDamping = 0.15f;
        AngularDamping = 0.12f;
        AirResistance = 0.025f;
        TerrainName = TEXT("Default");
    }

    FTerrainPhysicsSettings(float InRollingFriction, float InBounceDamping,
        float InLinearDamping, float InAngularDamping,
        float InAirResistance, const FString& InTerrainName)
        : RollingFriction(InRollingFriction)
        , BounceDamping(InBounceDamping)
        , LinearDamping(InLinearDamping)
        , AngularDamping(InAngularDamping)
        , AirResistance(InAirResistance)
        , TerrainName(InTerrainName)
    {
    }
};


// 전체 지형 물리 설정 컨테이너
USTRUCT(BlueprintType)
struct FTerrainPhysicsConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain Physics")
    TMap<FString, FTerrainPhysicsSettings> TerrainSettings;

    FTerrainPhysicsConfig()
    {
        // 기본 지형 설정들 초기화
        InitializeDefaultSettings();
    }

    void InitializeDefaultSettings()
    {
        // 그린 (기본)
        TerrainSettings.Add(TEXT("Green"), FTerrainPhysicsSettings(0.25f, 0.5f, 0.1f, 0.1f, 0.03f, TEXT("Green")));

        // 러프 (마찰력 높음)
        TerrainSettings.Add(TEXT("Rough"), FTerrainPhysicsSettings(0.28f, 0.9f, 0.15f, 0.15f, 0.03f, TEXT("Rough")));

        TerrainSettings.Add(TEXT("FairWay"), FTerrainPhysicsSettings(0.28f, 0.051f, 0.15f, 0.15f, 0.03f, TEXT("FairWay")));

        // 벙커/모래 (마찰력 매우 높음, 반발력 낮음)
        TerrainSettings.Add(TEXT("Bunker"), FTerrainPhysicsSettings(0.7f, 0.1f, 0.15f, 0.12f, 0.03f, TEXT("Bunker")));

        // 도로 (마찰력 낮음, 반발력 높음)
        TerrainSettings.Add(TEXT("Road"), FTerrainPhysicsSettings(0.15f, 0.9f, 0.06f, 0.06f, 0.01f, TEXT("Road")));

        // 물 (특수 처리)
        TerrainSettings.Add(TEXT("Water"), FTerrainPhysicsSettings(0.8f, 0.2f, 0.3f, 0.2f, 0.05f, TEXT("Water")));

        // 나무껍질 (높은 마찰력, 낮은 반발력)
        TerrainSettings.Add(TEXT("Bark"), FTerrainPhysicsSettings(0.9f, 0.15f, 10.14f, 0.11f, 0.022f, TEXT("Bark")));

        // 나뭇잎 (중간 마찰력, 낮은 반발력)
        TerrainSettings.Add(TEXT("Leaves"), FTerrainPhysicsSettings(0.30f, 0.5f, 0.10f, 0.09f, 0.018f, TEXT("Leaves")));
        TerrainSettings.Add(TEXT("Leavese"), FTerrainPhysicsSettings(0.30f, 0.5f, 0.10f, 0.09f, 0.018f, TEXT("Leavese"))); // 오타 버전도 추가

        // 매트 (낮은 마찰력)
        //TerrainSettings.Add(TEXT("Mat"), FTerrainPhysicsSettings(0.20f, 0.7f, 0.07f, 0.07f, 0.012f, TEXT("Mat")));

        // 네트 (특수 처리)
        TerrainSettings.Add(TEXT("Net"), FTerrainPhysicsSettings(0.60f, 0.2f, 5.13f, 0.10f, 0.020f, TEXT("Net")));
    }
};



// 상태별 물리 설정
USTRUCT(BlueprintType)
struct FStatePhysicsSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnablePhysics = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableGravity = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LinearDamping = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AngularDamping = 0.0f;

    FStatePhysicsSettings() = default;
    FStatePhysicsSettings(bool Physics, bool Gravity, float Linear, float Angular)
        : bEnablePhysics(Physics), bEnableGravity(Gravity),
        LinearDamping(Linear), AngularDamping(Angular) {
    }
};

// 볼 게임 흐름 이벤트를 위한 새로운 열거형
UENUM(BlueprintType)
enum class EBallEvent : uint8
{
    None,
    BallStopped,
    HoleIn,
    OutOfBounds,
    Conceded,
    // 필요에 따라 다른 상위 수준 이벤트 추가
};

// 볼의 내부 상태 변경을 위한 동적 멀티캐스트 델리게이트(볼 상태, 소유 플레이어 인덱스)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBallStateChangedInternal, EBallState, NewState, int32, OwningPlayerIndex);
// 게임 흐름에 영향을 미치는 주요 볼 상태 *이벤트*를 위한 새로운 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBallGameFlowEvent, EBallEvent, EventType); // EBallEvent 열거형 정의


namespace PhysMatResolveUtil
{
    /** 기본 방식(Hit/BodyInstance) 우선 → 실패 시 머터리얼에서 역추적 */
    UPhysicalMaterial* ResolveFromHit(const FHitResult& Hit, UPrimitiveComponent* OtherComp, bool bDoShortComplexTrace = true);

    /** 기본 방식만 시도 (Hit.PhysMaterial → BodyInstance) */
    UPhysicalMaterial* TryDefault(const FHitResult& Hit, UPrimitiveComponent* OtherComp);

    /** FaceIndex로 슬롯 머터리얼을 찾아 PhysMat 반환 */
    UPhysicalMaterial* TryFromMaterialSlot(UPrimitiveComponent* OtherComp, int32 FaceIndex);

    /** 충돌 지점에서 짧게 Complex 트레이스하여 FaceIndex/PhysMat 확보 */
    UPhysicalMaterial* TryShortComplexTrace(const FHitResult& Hit, UPrimitiveComponent* OtherComp);
}


UCLASS()
class PARKDAY_API AGolfBall : public AActor
{
    GENERATED_BODY()

public:
    AGolfBall();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    void BeginDestroy() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    AActor* SpawnBallLocation(UWorld* World, TSubclassOf<AActor> ClassToSpawn, float Distance);

    UPROPERTY()
    bool bIsCinematic = false;

    UPROPERTY()
    UCameraFXComponent* CameraFXComponent;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    UBillboardComponent* ReadyBillboard;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components")
    UBallNamePlateComponent* BallNamePlateComponent = nullptr;

    // ===== 공개 인터페이스 =====
    UFUNCTION(BlueprintCallable, Category = "Ball")
    void SetBallColor(const FLinearColor& Color);

    void CalculateRoundStat();

    // ⭐ 새로 추가: 볼의 현재 색상을 반환하는 함수
    UFUNCTION(BlueprintPure, Category = "Ball")
    FLinearColor GetBallColor() const;

    UFUNCTION(BlueprintCallable, Category = "Ball")
    void SetBallState(EBallState NewState);

    UFUNCTION(BlueprintCallable, Category = "Ball")
    EBallState GetBallState() const { return CurrentBallState; }

    // ===== 볼 가시성 제어 함수들 =====
    UFUNCTION(BlueprintCallable, Category = "Ball Visibility")
    void SetBallVisibility(bool bVisible, bool bAlsoSetCollision = false);

    UFUNCTION(BlueprintCallable, Category = "Ball Visibility")
    bool IsBallVisible() const;

    UFUNCTION(BlueprintCallable, Category = "Ball Visibility")
    void SetBallCollisionEnabled(bool bEnabled);

    UFUNCTION(BlueprintPure, Category = "Ball Visibility")
    bool IsBallCollisionEnabled() const;

    UFUNCTION(BlueprintCallable, Category = "Ball Visibility")
    void ShowBallForTeeShot();

    UFUNCTION(BlueprintCallable, Category = "Ball Visibility")
    void HideBallAfterHoleIn();

    UFUNCTION(BlueprintCallable, Category = "Ball Visibility")
    void UpdateVisibilityBasedOnState();
    // ===== 홀 전환 관련 함수 =====
    UFUNCTION(BlueprintCallable, Category = "Ball Hole Management")
    void ResetForNewHole();

    UFUNCTION(BlueprintCallable, Category = "Ball Hole Management")
    void PrepareForTeeShot();

    // ===== 가시성 상태 확인 함수들 =====
    UFUNCTION(BlueprintPure, Category = "Ball Visibility")
    bool IsBallForceHidden() const { return bBallForceHidden; }

    UFUNCTION(BlueprintCallable, Category = "Ball Visibility")
    void SetBallForceHidden(bool bShouldHide) { bBallForceHidden = bShouldHide; }

    UFUNCTION()
    float GetHoleDistance() const;


    UFUNCTION()
    bool CheckTeeShot();
    // m/s + 각도 기반 샷 시스템

    void ApplyShot(const FVector& Direction, float PowerPercent);

    UFUNCTION(BlueprintCallable, Category = "Ball", meta = (DisplayName = "Apply Shot MS"))
    void ApplyShotMS(const FVector& Direction, float SpeedMS, float LaunchAngleDegrees, float YawDegrees);

    // 물리 파라미터 설정
    UFUNCTION(BlueprintCallable, Category = "Ball Physics")
    void SetFrictionWeight(float NewWeight);

    UFUNCTION(BlueprintCallable, Category = "Ball Physics")
    void SetTerrainFriction(float NewFriction);

    // 상태 확인
    UFUNCTION(BlueprintCallable, Category = "Ball")
    bool IsOutOfBounds() const { return bIsOutOfBounds; }

    UFUNCTION(BlueprintCallable, Category = "Ball")
    FVector GetBallVelocity() const;

    UFUNCTION(BlueprintCallable, Category = "Ball")
    float GetBallSpeed() const;

    // ===== 궤적 관리 함수들 =====
    UFUNCTION(BlueprintCallable, Category = "Ball Trajectory")
    void StartTrajectoryTracking();

    UFUNCTION(BlueprintCallable, Category = "Ball Trajectory")
    void StopTrajectoryTracking();

    UFUNCTION(BlueprintCallable, Category = "Ball Trajectory")
    void ClearTrajectory();

    UFUNCTION(BlueprintCallable, Category = "Ball Trajectory")
    TArray<FTrajectoryPoint> GetTrajectoryPoints() const { return TrajectoryPoints; }

    UFUNCTION(BlueprintCallable, Category = "Ball Trajectory")
    float GetTotalTrajectoryDistance() const;

    UFUNCTION(BlueprintCallable, Category = "Ball Trajectory")
    float GetMaxTrajectoryHeight() const;

    UFUNCTION(BlueprintCallable, Category = "Ball Trajectory")
    void SetTrajectorySettings(const FTrajectorySettings& NewSettings);

    // ===== 설정 파일 관리 =====
    UFUNCTION(BlueprintCallable, Category = "Ball Config")
    bool LoadPhysicsConfigFromFile(const FString& FilePath = TEXT(""));

    UFUNCTION(BlueprintCallable, Category = "Ball Config")
    bool SavePhysicsConfigToFile(const FString& FilePath = TEXT(""));

    UFUNCTION(BlueprintCallable, Category = "Ball Config")
    void ReloadPhysicsConfig();

    UFUNCTION(BlueprintCallable, Category = "Ball Config")
    void ApplyLoadedPhysicsConfig();

    UFUNCTION(BlueprintCallable, Category = "Ball Config")
    FBallPhysicsConfig GetCurrentPhysicsConfig() const { return PhysicsConfig; }

    UFUNCTION(BlueprintCallable, Category = "Ball Config")
    FParkGolfConstants GetCurrentParkGolfConstants() const { return ParkGolfConstants; }

    // ===== 파크골프 물리 상수 (6cm 파크골프볼에 최적화) =====
    static constexpr float GRAVITY_MAGNITUDE = 980.0f;  // cm/s²
    static constexpr float MIN_FLYING_SPEED = 200.0f;   // cm/s (비행->굴림 전환, 2m/s)
    static constexpr float MIN_ROLLING_SPEED = 5.0f;   // cm/s (굴림->정지 전환, 0.5m/s)
    static constexpr float GROUND_CHECK_DISTANCE = 5.0f; // cm (지면 근접 체크)
    static constexpr float VELOCITY_EPSILON = 0.5f;     // cm/s (정지 판정)


    // ===== 컴포넌트 및 속성 =====
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* BallMesh;

    UPROPERTY(VisibleAnywhere, Category = "Ball|Marker")
    UStaticMeshComponent* GroundMarkerMesh;

    FRotator MarkerFixedRotation = FRotator::ZeroRotator;


    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
    EBallState CurrentBallState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics Config")
    FBallPhysicsConfig PhysicsConfig;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Park Golf Config")
    FParkGolfConstants ParkGolfConstants;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
    FTrajectorySettings TrajectorySettings;

    // 기존 호환성을 위한 속성들
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ball", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float FrictionWeight = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ball", meta = (ClampMin = "1.0", ClampMax = "50.0"))
    float HoleInRadius = 20.0f;

    // 설정 파일 경로
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    FString ConfigFilePath = TEXT("Config/TerrainPhysics.json");


    // OB 드롭 처리 함수들
    UFUNCTION(BlueprintCallable, Category = "Ball OB")
    void HandleOBDrop();

    UFUNCTION(BlueprintCallable, Category = "Ball OB")
    FVector CalculateDropPosition() const;

    // ⭐ 추가: LandscapeChecker 관련 함수들
    UFUNCTION(BlueprintCallable, Category = "Ball Landscape")
    void UpdateCurrentLandType();

    UFUNCTION(BlueprintCallable, Category = "Ball Landscape")
    void ApplyLandTypePhysics();

    UFUNCTION(BlueprintCallable, Category = "Ball Landscape")
    ELandType GetCurrentLandType();

    UFUNCTION(BlueprintCallable, Category = "Ball Landscape")
    FLandProperties GetCurrentLandProperties() const { return CurrentLandProperties; }

    // ⭐ 추가: LandscapeChecker 관련 변수들
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Landscape")
    ALandscapeChecker* LandscapeChecker;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Landscape")
    ELandType CurrentLandType;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Landscape")
    FLandProperties CurrentLandProperties;

public:
    UPROPERTY() TSet<TWeakObjectPtr<AActor>> OverlapLocked;

    UFUNCTION()
    void OnComponentBeginOverlap(
        UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION()
    void OnComponentEndOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex);

    void SpawnBallParticle(UPhysicalMaterial* PM);
    //UFUNCTION()
    //	void OnCompEndOverlap(
    //		UPrimitiveComponent* OverlappedComp,
    //		AActor* OtherActor,
    //		UPrimitiveComponent* OtherComp,
    //		int32 OtherBodyIndex);


    UFUNCTION(BlueprintCallable, Category = "Ball Turn Transition")
    void StartTurnTransitionCountdown(float DelayTime = 3.0f);

    UFUNCTION(BlueprintCallable, Category = "Ball Turn Transition")
    void UpdateTurnTransitionCountdown();

    UFUNCTION(BlueprintCallable, Category = "Ball Turn Transition")
    void SkipTurnTransitionCountdown();

    // 상황별 대기 시간 계산
    UFUNCTION(BlueprintCallable, Category = "Ball Turn Transition")
    float CalculateTurnTransitionDelay(bool bHoleIn, bool bOutOfBounds);


    // 상태 확인 함수들
    UFUNCTION(BlueprintPure, Category = "Ball Turn Transition")
    float GetTurnTransitionCountdown() const { return TurnTransitionCountdown; }

    UFUNCTION(BlueprintPure, Category = "Ball Turn Transition")
    bool IsInTurnTransition() const { return TurnTransitionCountdown > 0.0f; }

    UFUNCTION(BlueprintPure, Category = "Ball Turn Transition")
    float GetTurnTransitionProgress() const
    {
        return TurnTransitionMaxTime > 0.0f ?
            (1.0f - TurnTransitionCountdown / TurnTransitionMaxTime) : 0.0f;
    }

    // 설정 가능한 옵션들
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn Transition")
    bool bShowCountdownOnScreen = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn Transition")
    bool bShowCountdownCircle = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn Transition")
    bool bAllowCountdownSkip = false;  // 관리자 모드에서만 true





    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera Link")
    ACameraManager* LinkedCameraManager;

    UFUNCTION(BlueprintCallable, Category = "Ball Camera")
    void LinkCameraManager(ACameraManager* CameraManager);

    UFUNCTION(BlueprintCallable, Category = "Ball Camera")
    void RequestCameraSync();



    static constexpr float TURN_TRANSITION_DELAY = 4.0f;  // ⭐ 4초로 통일
    static constexpr float CAMERA_FREEZE_TIME = 4.0f;    // ⭐ 동일하게 4초

    // 게임 로직
    bool CheckHoleIn();
    bool CheckOutOfBounds();

    // ⭐ 추가: 실시간 OB 교차점 체크 (매 프레임마다 호출)
    void CheckRealtimeOBCrossing();


    // ⭐ 새로 추가: 샷 디버깅 함수들
    UFUNCTION(BlueprintCallable, Category = "Ball Debug")
    void LogShotDebugInfo() const;

    UFUNCTION(BlueprintCallable, Category = "Ball Debug")
    void ForceApplyShot(const FVector& Direction, float SpeedMS);

    UFUNCTION(BlueprintCallable, Category = "Ball Debug")
    float GetCurrentSpeedDebug() const;


    // ===== Trail 관련 함수들 =====
    UFUNCTION(BlueprintCallable, Category = "Ball Trail")
    void SetTrailVisible(bool bVisible);

    UFUNCTION(BlueprintCallable, Category = "Ball Trail")
    void ClearTrail();

    UFUNCTION(BlueprintCallable, Category = "Ball Trail")
    void SetTrailSettings(const FBallTrailSettings& NewSettings);

    UFUNCTION(BlueprintPure, Category = "Ball Trail")
    FBallTrailSettings GetTrailSettings() const { return TrailSettings; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail")
    FBallTrailSettings TrailSettings;


    // 홀컵과의 컨시드 거리 체크
    UFUNCTION(BlueprintCallable, Category = "Ball")
    bool CheckConcedeDistance() const;

    // 홀인 상태 반환
    UFUNCTION(BlueprintPure, Category = "Ball")
    bool IsHoleIn() const { return bIsHoleIn; }

    // 컨시드 상태 반환
    UFUNCTION(BlueprintPure, Category = "Ball")
    bool IsConceded() const { return bIsConceded; }

    // 홀인 상태 설정
    void SetHoleIn(bool bHoleIn);

    // 컨시드 상태 설정
    void SetConceded(bool bConceded);

    // 볼의 상태 변경 시 브로드캐스트할 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Ball Events")
    FOnBallStateChangedInternal OnBallStateChangedInternal; // ⭐ 새로 추가: 내부 델리게이트

    // 이 볼을 소유한 플레이어의 인덱스
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball Info")
    int32 OwningPlayerIndex; // ⭐ 새로 추가: 소유 플레이어 인덱스

    // GameMode에 중요한 볼 이벤트를 알리는 공개 이벤트
    UPROPERTY(BlueprintAssignable, Category = "Ball Events")
    FOnBallGameFlowEvent OnBallGameFlowEvent; // 새로운 델리게이트 추가


    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Physics")
    UPhysicalMaterial* CurrentBallPhysicalMaterial;

    UPhysicalMaterial* GetPhysMatBelow_ThroughEmpty(
        float TraceUp = 20.f,
        float TraceDown = 5000.f,
        ECollisionChannel Channel = ECC_Visibility,
        bool bTraceComplex = true);

    //public 으로 옮김
    void AdjustBallToGroundLevel();

    bool IsWorldStaticComponent(const UPrimitiveComponent* Comp);
    bool PerformLineTrace(FHitResult& OutHit);

    // ⭐ 새로 추가: 소유 플레이어의 샷 카운트를 증가시키는 함수
    UFUNCTION(BlueprintCallable, Category = "Ball Player")
    void IncrementOwningPlayerShotCount();

    // ===== UE4 수동 슬리핑 시스템 변수들 =====
    UPROPERTY()
    FTimerHandle ManualSleepCheckTimer;

    UPROPERTY()
    float LastSpeedCheckTime;

    UPROPERTY()
    float LastRecordedSpeed;
    UPROPERTY()
    int32 LowSpeedFrameCount;

    //==== RoundStat ====//

    float GetShotDistance();

    UPROPERTY()
    TArray<float> TeeShotDistanceArray;

    UPROPERTY()
    TArray<ELandType> TeeShotSettlementArray;

    UPROPERTY()
    TArray<FShotInfo> ShotInfoArray;
    UPROPERTY()
    float MaxDistanceTeeShot;

    UFUNCTION()
    bool CheckWasTeeShot();

    //============================//

    UFUNCTION()
    void PlaySoundByMaterial(UPhysicalMaterial* PM, float ImpulseSize);


    // 진행방향 앞의 지면 정보를 가져오는 함수
    FVector GetForwardTerrainNormal(const FVector& Direction, float ForwardDistance = 10.0f) const;

    // 앞 지면을 고려한 발사 방향 계산
    FVector CalculateShotDirectionWithForwardTerrain(const FVector& BaseDirection, float LaunchAngleDegrees, float YawDegrees);

    UFUNCTION()
    void SetBounceFix(bool bISBouncFix);

    UFUNCTION()
    bool GetBounceFix();
protected:
    // ===== 물리 시스템 =====

    void ConfigureStatePhysics();
    void ApplyStatePhysics(EBallState State);
    void UpdatePhysicsBasedOnState(float DeltaTime);

    // 상태별 물리 업데이트
    void UpdateFlyingPhysics(float DeltaTime);
    void UpdateRollingPhysics(float DeltaTime); // UpdateRollingPhysicsLandtype
    void UpdateBouncePhysicsLandtype(float DeltaTime);
    void UpdateStoppedPhysics();

    // 물리 효과 적용
    void ApplyAirResistance(float DeltaTime);
    void ApplyRollingFriction(float DeltaTime);
    void ApplySlopeEffect(float DeltaTime);
    void ApplyBackspin(float ShotPower);

    // 상태 전환 로직
    void CheckAutoStateTransitions();
    void HandleStateTransition(EBallState PreviousState, EBallState NewState);
    void HandleBallStopped();

    // =====  궤적 추적 시스템 =====
    void UpdateTrajectoryTracking(float DeltaTime);
    void AddTrajectoryPoint();
    void CleanupOldTrajectoryPoints();
    void DrawTrajectory() const;
    FColor GetSpeedBasedColor(float Speed) const;
    FColor GetStateBasedColor(EBallState State) const;

    // 플레이어 결과 처리
    int32 FindOwnerPlayerIndex(UGolfPlayerManager* PlayerManager);


    // 지형 및 충돌 처리
    FVector GetTerrainNormal() const;
    bool IsNearGround(float Distance = 10.0f) const;
    bool IsGroundCollision(AActor* OtherActor, UPrimitiveComponent* OtherComp) const;
    void HandleGroundBounce(const FHitResult& Hit);



    // OB 체크 개선 함수들
    float FindMinDistanceToOBLine(const FVector2D& Point, const TArray<FVector>& OBPoints) const;
    float DistancePointToLineSegment(const FVector2D& Point, const FVector2D& LineStart, const FVector2D& LineEnd) const;

    // 유틸리티
    FVector CalculateShotVelocity(const FVector& Direction, float Power) const;
    void UpdatePhysicsParameters();


    // 파크골프 m/s + 각도 시스템
    float CalculateExpectedDistance(float SpeedMS, float LaunchAngleDegrees) const;
    void TrackShotDistance();
    FString EvaluateParkGolfDistance(float Distance);
    FString EvaluateAccuracy(float AccuracyPercent);

    // 방향 계산 함수들
// 방향 계산 함수들
    FVector CalculateShotDirectionWithElevation(const FVector& BaseDirection, float YawDegrees = 0.0f);
    FVector CalculateShotDirectionWithElevation(const FVector& BaseDirection, float LaunchAngleDegrees, float YawDegrees = 0.0f);
    //FVector CalculateShotDirectionWithElevation(const FVector& BaseDirection, float LaunchAngleDegrees);

     // ===== 설정 파일 관련 함수들 =====
    bool LoadConfigFromJson(const FString& FilePath);
    bool SaveConfigToJson(const FString& FilePath);
    FString GetDefaultConfigFilePath() const;
    TSharedPtr<FJsonObject> CreateDefaultConfigJson() const;
    void LoadConfigFromJsonObject(TSharedPtr<FJsonObject> JsonObject);

    UPROPERTY(EditAnywhere, Category = "Hit")
    float HitCooldown = 0.1f;     // 80ms 정도가 보통 적당
    float LastHitTime = -1000.f;

    // ===== 이벤트 핸들러 =====
    UFUNCTION()
    void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, FVector NormalImpulse,
        const FHitResult& Hit);

    UFUNCTION()
    void ResetToReady();

    // OB 드롭 관련 헬퍼 함수들
    FVector FindClosestPointOnOBLine(const TArray<FVector>& OBPoints) const;
    FVector CalculateInBoundsPosition(const FVector& ClosestOBPoint, const TArray<FVector>& OBPoints, float InsetDistance = 20.0f) const;
    void ExecuteDrop(const FVector& DropPosition);
    void ExecutePenaltyDrop(const FVector& DropPosition);

    // OB 드롭 계산 헬퍼 함수들
    FVector2D GetClosestPointOnLineSegment(const FVector2D& Point, const FVector2D& LineStart, const FVector2D& LineEnd) const;
    FVector2D CalculateInwardNormal(const FVector& ClosestOBPoint, const TArray<FVector>& OBPoints) const;
    float GetGroundHeightAtPosition(const FVector& Position) const;
    bool IsPositionInBounds(const FVector& Position, const TArray<FVector>& OBPoints) const;

    // 개선된 OB 체크 함수들
    bool IsPointOutsidePolygonImproved(const FVector2D& Point, const TArray<FVector>& PolygonPoints) const;
    bool IsRayIntersectingSegment(const FVector2D& Point, const FVector2D& P1, const FVector2D& P2) const;
    float CalculatePolygonSignedArea(const TArray<FVector>& PolygonPoints) const;

    void CheckGroundPenetration();
    void CheckBallOutOfBounds();
    void EnablePhysicsSafely();

    void TryExtendedGroundSearch(const FVector& CurrentLocation, float BallRadius);
    float GetActualBallRadius() const;

    // ⭐ 추가: 지면 타입 체크 및 물리 적용 함수들
    void CheckGroundType();


    //void TriggerPlayerResultProcessing(bool bHoleIn, bool bOutOfBounds);


    // ===== 가시성 관련 헬퍼 함수들 =====
    void HandleVisibilityOnStateChange(EBallState PreviousState, EBallState NewState);

    bool ShouldBallBeVisible(EBallState State) const;
    void UpdateCollisionBasedOnVisibility();
    void SaveOriginalCollisionSettings();
    void RestoreOriginalCollisionSettings();

    // ⭐ 새로 추가: 물리 시스템 강화 함수들
    void ForceEnableCollisionForShot();
    void LogPhysicsState(const FString& Context);

    void SetPhysicsAndCollisionSafely(bool bEnablePhysics, bool bEnableCollision);
    void LogCurrentPhysicsState(const FString& Context);



    void ValidateAndFixPhysicsState();


    void SetBallMassSafely(float NewMass);

    void ValidateAndLogPhysicsState();

    void ApplyShotVelocityDelayed(const FVector& ShotVelocity, float SpeedMS);

    // Trail 업데이트 함수들
    void UpdateBallTrail(float DeltaTime);
    void AddTrailPoint();
    void CleanupOldTrailPoints();
    void DrawBallTrail() const;

    // Trail 색상 계산
    FLinearColor GetTrailColorForSpeed(float Speed) const;
    FLinearColor GetTrailColorWithFade(const FLinearColor& BaseColor, float Alpha) const;


    /** UE4 수동 슬리핑 체크 */
    UFUNCTION()
    void CheckManualSleeping();

    /** 수동 슬리핑 체크 재시작 */
    UFUNCTION()
    void RestartManualSleepCheck();


    /** 최적화된 물리 재질 생성 */
    UFUNCTION()
    UPhysicalMaterial* CreateOptimizedPhysicalMaterial();



    void InitializeUE4PhysicsSystem();






    // 안전한 충돌 처리를 위한 변수들
    UPROPERTY()
    bool bIsInCollisionCallback = false;

    UPROPERTY()
    bool bHasPendingBounce = false;

    UPROPERTY()
    FHitResult PendingBounceHit;

    UPROPERTY()
    FTimerHandle SafeBounceTimer;

    UPROPERTY()
    bool bIsBeingDestroyed = false;

    // 안전한 바운스 처리 함수들
    UFUNCTION()
    void ProcessPendingBounce();

    UFUNCTION()
    void SafeHandleBounce(const FHitResult& Hit);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball")
    bool bIsHoleIn;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball")
    bool bIsConceded;
private:
    // ===== 물리 설정 맵 =====
    TMap<EBallState, FStatePhysicsSettings> StatePhysicsMap;

    // ===== 상태 관리 타이머 =====
    FTimerHandle ResetReadyTimer;
    FTimerHandle StateTransitionTimer;

    // ===== 물리 추적 변수 =====
    FVector LastValidVelocity;
    float LastGroundContactTime;
    bool bWasInAir;

    // ===== 성능 최적화: static 로컬 → 멤버 변수 =====
    float LastValidationTime = 0.0f;
    float LastErrorCheckTime = 0.0f;

    // TrackShotDistance 전용 (static 로컬 제거)
    FVector ShotStartLocation = FVector::ZeroVector;
    float ShotSpeed_Track = 0.0f;
    float ShotAngle_Track = 0.0f;
    bool bTrackingShot = false;

    // ===== 성능 최적화: 프레임당 LineTrace 캐시 =====
    // 같은 프레임 내에서 반복 호출되는 LineTrace 결과를 재사용
    bool bFrameGroundCacheValid = false;
    FHitResult CachedGroundHit;
    bool bCachedNearGround = false;
    float CachedNearGroundDistance = 0.0f;

    // ===== 성능 최적화: GroundMarker용 볼 배열 캐시 =====
    // GetAllActorsOfClass(AGolfBall) 매 프레임 호출 제거
    TArray<AActor*> CachedGolfBallActors;

    // ===== 성능 최적화: LandType 캐시 (CheckGroundType 131ms → 거의 0ms) =====
    // 전략: Ball_Rolling/Bound 진입 시 1회 + 일정 거리 이동마다만 갱신
    //       Ball_Fly 상태에서는 스킵 (공중에서 지형 판정 불필요)
    FVector    LandTypeLastCheckPos = FVector::ZeroVector;  // 마지막 체크한 위치
    float      LandTypeCheckInterval = 150.0f;               // 갱신 거리 임계값 (cm, 기본 1.5m)
    bool       bLandTypeDirty = true;                 // true 이면 다음 Tick에 즉시 갱신
    EBallState LandTypeLastState = EBallState::Ball_Init;// 직전 상태 (전환 감지용)

    // ===== 게임 상태 변수 =====
    bool bIsOutOfBounds;
    FLinearColor TrailColor;

    // ===== 설정 파일 관련 변수 =====
    bool bConfigLoaded = false;
    FDateTime LastConfigLoadTime;

    // ===== 궤적 추적 변수들 =====
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trajectory", meta = (AllowPrivateAccess = "true"))
    TArray<FTrajectoryPoint> TrajectoryPoints;

    bool bIsTrackingTrajectory = false;
    float LastTrajectoryPointTime = 0.0f;
    float TrajectoryStartTime = 0.0f;

    // 추가 필요한 함수 선언들
    void UpdateFriction();

    // OB 드롭 상수
    static constexpr float OB_DROP_INSET_DISTANCE = 120.0f; // cm (OB 라인에서 안쪽으로 80cm)
    static constexpr float OB_DROP_HEIGHT_OFFSET = 3.0f;  // cm (드롭 시 지면에서 약간 위)

    // 시각화 상수
    static constexpr float OB_POINT_SIZE = 30.0f;           // OB 포인트 구체 크기
    static constexpr float OB_LINE_THICKNESS = 8.0f;       // OB 라인 두께
    static constexpr float OB_HEIGHT_OFFSET = 55.0f;       // 지면에서 얼마나 위에 그릴지
    static constexpr float OB_VISUALIZATION_DURATION = 30.0f; // 시각화 지속 시간 (초)

    // 기존 상수들을 수정
    static constexpr float UNREAL_SPHERE_RADIUS = 3.2f;    // 언리얼 기본 구체 반지름 (cm)
    static constexpr float PARKGOLF_BALL_DIAMETER = 6.0f;   // 파크골프볼 지름 (cm)
    static constexpr float PARKGOLF_BALL_RADIUS = 3.2f;     // 파크골프볼 반지름 (cm)

    // 스케일 계산: 원하는 크기 / 기본 구체 크기
    static constexpr float BALL_SCALE = PARKGOLF_BALL_DIAMETER / (UNREAL_SPHERE_RADIUS * 2.0f); // 0.06f

    // ⭐ 추가: 지면 타입별 물리 조정을 위한 기본값 저장
    float BaseFrictionWeight = 1.0f;
    float BaseBounceDamping = 0.6f;
    float BaseLinearDamping = 0.08f;

    // ===== 개선된 턴 전환 관련 변수들 =====

    // 턴 전환 카운트다운 타이머
    FTimerHandle CountdownUpdateTimer;

    // 현재 카운트다운 시간 (초)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turn Transition", meta = (AllowPrivateAccess = "true"))
    float TurnTransitionCountdown = 0.0f;

    // 전체 카운트다운 시간 (진행률 계산용)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turn Transition", meta = (AllowPrivateAccess = "true"))
    float TurnTransitionMaxTime = 0.0f;

    //===== 물리 튐 보정 =====
    UPROPERTY(EditAnywhere, CAtegory = "Bounce Fix")
    bool bBounceFix = true;

    UPROPERTY(EditAnywhere, Category = "Bounce Fix")
    float MaxBounceSpeedRatio = 2.f;

    UPROPERTY(EditAnywhere, Category = "Bounce Fix")
    float MinBounceFixHeight = 100.f;
    UPROPERTY(EditAnywhere, Category = "Bounce Fix")
    float MinPreImpactSpeed = 1000.f;
    //UPROPERTY(EditAnywhere, Category="Bounce Fix")
    //float FloorNormalDotThreshold = 0.75f;

    FVector LastLinearVelocity;

    // ===== 턴 전환 상수들 =====

    static constexpr float COUNTDOWN_UPDATE_INTERVAL = 0.1f;   // 카운트다운 업데이트 간격 (초)
    static constexpr float HOLE_IN_DELAY = 4.0f;              // 홀인 시 대기 시간 (초)
    static constexpr float FINAL_HOLE_DELAY = 5.0f;           // 마지막 홀 완료 시 대기 시간 (초)

    // ⭐ 새로 추가할 private 함수들
    void NotifyCameraStateChange(EBallState PreviousState, EBallState NewState);

    // ===== 가시성 관련 변수들 =====
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball Visibility", meta = (AllowPrivateAccess = "true"))
    bool bBallCurrentlyVisible = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball Visibility", meta = (AllowPrivateAccess = "true"))
    bool bBallForceHidden = false; // 홀인 후 강제 숨김용

    // ===== 콜리젼 관련 변수들 =====
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball Collision", meta = (AllowPrivateAccess = "true"))
    bool bBallCollisionEnabled = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball Collision", meta = (AllowPrivateAccess = "true"))
    TEnumAsByte<ECollisionEnabled::Type> OriginalCollisionType = ECollisionEnabled::QueryAndPhysics;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball Collision", meta = (AllowPrivateAccess = "true"))
    FName OriginalCollisionProfileName = TEXT("BlockAll");

    // 가시성 제어 상수들
    static constexpr float FADE_IN_TIME = 0.5f;    // 나타날 때 페이드인 시간
    static constexpr float FADE_OUT_TIME = 0.3f;   // 사라질 때 페이드아웃 시간

    // 페이드 효과용 변수들 (선택사항)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball Visibility", meta = (AllowPrivateAccess = "true"))
    bool bIsFading = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball Visibility", meta = (AllowPrivateAccess = "true"))
    float CurrentFadeAlpha = 1.0f;

    bool bIsChangingPhysicsState = false;

    EPhysicsState CurrentPhysicsState = EPhysicsState::Disabled;

    mutable float CachedMass = 0.085f; // 기본 35g
    mutable bool bMassValidated = false;


    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shot State", meta = (AllowPrivateAccess = "true"))
    bool bHasPendingShot = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shot State", meta = (AllowPrivateAccess = "true"))
    FVector PendingShotVelocity = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shot State", meta = (AllowPrivateAccess = "true"))
    float PendingShotSpeed = 0.0f;


    // Trail 포인트 배열
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trail", meta = (AllowPrivateAccess = "true"))
    TArray<FTrailPoint> TrailPoints;

    float LastTrailUpdateTime = 0.0f;

    // ⭐ 수정: bIsConcededResult 매개변수 추가
    //void TriggerPlayerResultProcessing(bool bHoleIn, bool bOutOfBounds, bool bIsConcededResult = false);

protected:

    // SetBallState에 대한 직접 호출 대신
// InternalSetBallState를 호출하여 내부 상태 변경을 트리거한 다음
// 필요한 경우 이벤트를 브로드캐스트합니다.
    void InternalSetBallState(EBallState NewState, bool bForceUpdate = false); // `SetBallStateInternal` 함수 이름 변경 및 목적 변경


    // 볼의 현재 색상을 저장하는 private 변수를 추가할 수 있습니다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ball", meta = (AllowPrivateAccess = "true"))
    FLinearColor CurrentBallColor; // ⭐ 추가: 볼의 현재 색상을 저장하는 변수

    AInGameMode* GM;


public:
    // 지형 물리 설정 관련 함수들
    UFUNCTION(BlueprintCallable, Category = "Ball Terrain Physics")
    bool LoadTerrainPhysicsConfig(const FString& FilePath = TEXT(""));

    UFUNCTION(BlueprintCallable, Category = "Ball Terrain Physics")
    bool SaveTerrainPhysicsConfig(const FString& FilePath = TEXT(""));

    UFUNCTION(BlueprintCallable, Category = "Ball Terrain Physics")
    void ApplyTerrainPhysicsSettings(const FString& TerrainName);

    UFUNCTION(BlueprintCallable, Category = "Ball Terrain Physics")
    FTerrainPhysicsSettings GetTerrainPhysicsSettings(const FString& TerrainName) const;

    UFUNCTION(BlueprintCallable, Category = "Ball Terrain Physics")
    void SetTerrainPhysicsSettings(const FString& TerrainName, const FTerrainPhysicsSettings& Settings);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain Physics Config")
    FTerrainPhysicsConfig TerrainPhysicsConfig;
    void SetPhysicsState(EPhysicsState NewState);

    UFUNCTION()
    bool GetIsConcede();

protected:
    // 지형 물리 설정 관련 헬퍼 함수들
    void ApplyPhysicsSettingsFromTerrain(const FTerrainPhysicsSettings& Settings);
    FString GetTerrainNameFromPhysicalMaterial(UPhysicalMaterial* PhysMaterial) const;
    void LoadTerrainConfigFromJsonObject(TSharedPtr<FJsonObject> JsonObject);
    void SaveTerrainConfigToJsonObject(TSharedPtr<FJsonObject> JsonObject);
    TSharedPtr<FJsonObject> CreateDefaultTerrainConfigJson() const;

    // 벌타 드롭 헬퍼 함수들
    FVector FindSafeDropPosition(const FVector& CurrentPosition, const TArray<FVector>& SearchDirections) const;
    bool IsPositionSafe(const FVector& Position, float SafeRadius = 100.0f) const;
    FVector GetGroundLevelPosition(const FVector& Position) const;
    void VisualizePenaltyDrop(const FVector& DropPosition) const;

    TArray<FVector> GetCurrentHoleOBPoints() const;
    float CalculateDistanceToOBLineSegment(const FVector& Point, const FVector& LineStart, const FVector& LineEnd) const;


private:
    // 현재 적용된 지형 설정 추적
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain Physics", meta = (AllowPrivateAccess = "true"))
    FString CurrentAppliedTerrain = TEXT("Grass");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain Physics", meta = (AllowPrivateAccess = "true"))
    FTerrainPhysicsSettings CurrentTerrainSettings;

    UFUNCTION(BlueprintCallable, Category = "Ball Debug")
    void LogCurrentTerrainSettings() const;

    FVector GetAccurateTerrainNormal() const;
    float GetGroundSlopeAngle() const;
    FVector GetGroundSlopeDirection() const;
    FVector CalculateShotDirectionWithTerrainSlope(const FVector& BaseDirection, float LaunchAngleDegrees, float YawDegrees);
    FVector CalculateFallbackDirection(const FVector& Direction, float LaunchAngle) const;

    bool bShowShotArrow = true;

    float ShotArrowDuration = 5.0f; // 화살표 표시 시간
    float ShotArrowThickness = 8.0f; // 화살표 두께
    float ShotArrowScale = 100.0f; // 화살표 크기 배율
    FVector LastShotDirection = FVector::ZeroVector;
    float LastShotPower = 0.0f;
    FVector LastShotStartLocation;


    void DrawShotDirectionArrow(const FVector& StartLocation, const FVector& Direction, float Power, float LaunchAngle);
    FLinearColor GetShotPowerColor(float Power);
    void DrawShotPowerIndicator(const FVector& StartLocation, float Power);
    void DrawShotAngleIndicator(const FVector& StartLocation, const FVector& Direction, float LaunchAngle);
    void DrawEstimatedLandingPoint(const FVector& StartLocation, const FVector& Direction, float Power, float LaunchAngle);
    FVector GetGroundAdjustedPosition(const FVector& Position);
    void SetShotArrowVisible(bool bVisible);
    void SetShotArrowDuration(float Duration);

    void DrawGroundNormalVector(const FHitResult& Hit);
    void DrawGroundPlaneVisualization(const FVector& HitLocation, const FVector& GroundNormal);
    void DrawVelocityComponents(const FVector& HitLocation, const FVector& Velocity, const FVector& GroundNormal);
    void DrawSpeedColorLegend(const FVector& BaseLocation);

    void ResetToDefaultPhysicalMaterial();
    UPhysicalMaterial* DefaultPhysicalMaterial;
    UPhysicalMaterial* CurrentAppliedPhysicalMaterial;


    // 벌타 드롭 상수들
    static constexpr float PENALTY_DROP_SEARCH_RADIUS = 150.0f;  // 드롭 위치 검색 반경 (1.5m)
    static constexpr float PENALTY_DROP_SAFE_RADIUS = 100.0f;    // 안전 반경 (0.5m)
    static constexpr float PENALTY_DROP_HEIGHT_OFFSET = 5.0f;    // 지면에서 높이 오프셋 (5cm)
    static constexpr float PENALTY_DROP_OB_SAFETY_MARGIN = 100.0f; // OB 라인과의 안전 거리 (50cm)
    static constexpr int32 PENALTY_DROP_MAX_ATTEMPTS = 12;       // 최대 시도 횟수 (30도씩 12방향)

    TArray<FVector> GenerateCircularSearchDirections() const;

    FVector GetCurrentHolePosition() const;
    FVector GetDirectionToHole() const;
    FVector GetDirectionToHole2D() const;
    FVector GetStabilizedTerrainNormal() const;

    UPROPERTY(EditAnywhere, Category = "Debug")
    bool bShowGroundNormalDebug = true;

    FHitResult CreateStabilizedHitResult(const FHitResult& OriginalHit);
    bool IsLandscapeHit(const FHitResult& Hit);
    void HandleConstrainedBounceWithStabilizedNormal(const FHitResult& Hit);
    void DrawNormalComparison(const FHitResult& Hit);

    void HandleBoundaryBounceWithDirectionCorrection(const FHitResult& Hit);
    FVector CalculateForwardCorrectedVelocity(
        const FVector& OriginalVelocity,
        const FVector& StabilizedNormal,
        float TargetSpeed);
    FVector CalculateGroundAlignedForwardDirection(
        const FVector& DesiredDirection,
        const FVector& GroundNormal);

    // 경계면 바운스 처리를 위한 새로운 함수들
    FVector CalculateHorizontalCorrectedVelocity(const FVector& OriginalVelocity, float TargetSpeed);

    void DebugVelocityDirection(const FString& Context, const FVector& Velocity);

    FVector CalculateSmoothGroundVelocity(const FVector& OriginalVelocity, float TargetSpeed);
    FVector CalculateZeroGroundVelocity(const FVector& OriginalVelocity, float TargetSpeed);

    FVector CalculateStrictHorizontalVelocity(const FVector& OriginalVelocity, float TargetSpeed);

public:
    bool bOverlapHoleIn = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ball Settings")
    float ConcedeDistance = 50.0f; // 기본 컨시드 거리 30cm
    // ===============================================
    // 발사 직후 바운스 방지 관련 변수들
    // ===============================================
    UPROPERTY()
    bool bJustLaunched;

    UPROPERTY()
    float LaunchTime;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Launch Settings")
    float LaunchGracePeriod;

    UPROPERTY()
    FVector LaunchPosition;

    UPROPERTY()
    FTimerHandle LaunchGraceTimer;

    // 벌타 드롭 관련 함수들
    UFUNCTION(BlueprintCallable, Category = "Ball Penalty Drop")
    void HandlePenaltyDrop();

    UFUNCTION(BlueprintCallable, Category = "Ball Penalty Drop")
    FVector CalculatePenaltyDropPosition() const;

    UFUNCTION(BlueprintCallable, Category = "Ball Penalty Drop")
    bool CheckObstacleAtPosition(const FVector& Position, float CheckRadius = 50.0f) const;

    UFUNCTION(BlueprintCallable, Category = "Ball Penalty Drop")
    bool IsPositionInBounds(const FVector& Position) const;

    UFUNCTION(BlueprintCallable, Category = "Ball Penalty Drop")
    float GetDistanceToNearestOBLine(const FVector& Position) const;

    UFUNCTION(BlueprintCallable, Category = "Ball|Mesh")
    void ApplyComplexMesh();
    UFUNCTION(BlueprintCallable, Category = "Ball|Mesh")
    void ApplySimpleMesh();

protected:
    // 기존 CrosshairActor
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crosshair", meta = (AllowPrivateAccess = "true"))
    class AActor* CrosshairActor;

    // ⭐ 추가: 생성자에서 로드된 크로스헤어 BP 클래스 (이게 핵심!)
    UPROPERTY()
    TSubclassOf<AActor> CrosshairBPClass;


    // ⭐ 추가: 크로스헤어 생성/파괴 함수
    UFUNCTION(BlueprintCallable, Category = "Crosshair")
    void CreateCrosshairActor();

    UFUNCTION(BlueprintCallable, Category = "Crosshair")
    void DestroyCrosshairActor();

    // ⭐ 추가: 크로스헤어 표시/숨김 함수
    void ShowCrosshair();
    void HideCrosshair();
    bool ShouldShowCrosshair() const;

    // ⭐ 추가: 크로스헤어 위치 업데이트
    UFUNCTION(BlueprintCallable, Category = "Crosshair")
    void UpdateCrosshairPosition();



private:
    FTimerHandle MaxRollingTimer;
    float MaxRollingDuration = 5.0f; // 최대 30초

    UPROPERTY()
    bool bIsGraduallyStop = false;

    UPROPERTY()
    float GradualStopStartTime = 0.0f;

    UPROPERTY()
    float GradualStopDuration = 3.0f;

    UPROPERTY()
    float InitialRollingSpeed = 0.0f;

    bool bIsGraduallystopping = false;

    FTimerHandle GradualStopTimer;

    // 함수들 추가
    void StartGradualStop();
    void UpdateGradualStop();
    void CompleteGradualStop();
    void ForceStopBall();
    void HandleRollingStateEnter();
    void ClearAllStateTimers();


    FVector FindLandscapePosition(const FVector& SearchPosition, float BallRadius) const;
    FVector FindObstacleAvoidancePosition(const FVector& LandscapePosition, float BallRadius) const;
    FVector AdjustToLandscapeHeight(const FVector& Position, float BallRadius) const;
    bool HasObstacleCollision(const FVector& Position, float CheckRadius) const;
    bool IsFinalPositionSafe(const FVector& Position, float BallRadius, const TArray<FVector>& OBPoints) const;


    void VisualizeOBDrop(const FVector& OBPoint, const FVector& DropPoint, const FVector& InwardDirection) const;
    FVector GetGroundNormalAtPosition(const FVector& Position) const;
    bool IsDropPositionSafe(const FVector& Position, float BallRadius, const TArray<FVector>& OBPoints) const;
    FVector FindSafeDropPositionWithObstacleAvoidance(const FVector& InitialPosition, const FVector& InwardDirection, float BallRadius, const TArray<FVector>& OBPoints) const;
    FVector GetPolygonCenter(const TArray<FVector>& Points) const;
    FVector GetClosestPointOnLineSegment3D(const FVector& Point, const FVector& LineStart, const FVector& LineEnd) const;
    FVector CalculateOBDropFromCrossingPoint();

    FVector LastOBCrossingPoint;
    bool bHasValidOBCrossingPoint;

    // 이전 프레임의 볼 위치 (OB 교차점 계산용)
    FVector PreviousBallPosition;

    // 선분과 선분의 교차점 계산 (2D)
    bool LineSegmentIntersection2D(
        const FVector2D& P1, const FVector2D& P2,
        const FVector2D& P3, const FVector2D& P4,
        FVector2D& OutIntersection) const;

    // 볼 궤적과 OB 라인의 교차점 계산
    FVector CalculateOBLineIntersection(
        const FVector& PrevPos,
        const FVector& CurrentPos,
        const TArray<FVector>& OBPoints) const;

    // ⭐ 추가: 크로스헤어 상태 플래그
    bool bCrosshairActive = false;
    FTimerHandle CrosshairUpdateTimer;

    float LastBounceImpulseSquared = 0.0f;  // 이전 바운스의 NormalImpulse.SizeSquared()

    void LimitGroundBounce(const FHitResult& Hit, const FVector& NormalImpulse);

    UStaticMesh* SimpleBallMesh;  // Pball_red (기본/간단)

    UStaticMesh* ComplexBallMesh; // Pball_red_G (복잡/Ready)

    // 추가: 공개 함수 3개
    // ⭐ JSON에서 로드된 샷 조정값
    FShotAdjustments CurrentShotAdjustments;

    // ⭐ 현재 샷 타입 (자동 판단됨)

    EShotType CurrentShotType = EShotType::TeeShot;

    // ⭐ 홀의 티 위치 (홀 시작 시 설정)

    FVector HoleTeePosition = FVector::ZeroVector;

    // ⭐ 티샷 판정 범위 (cm 단위, 기본 30cm)

    float TeePositionTolerance = 30.0f;

    // ⭐ 샷 타입 판단 함수

    EShotType DetermineShotType();

    // ⭐ 샷 조정값 로드 함수

    void LoadShotAdjustmentsFromJSON();

    // ⭐ 설정 파일 경로 반환 함수

    FString GetShotAdjustmentsConfigPath() const;

    // ⭐ 조정값 적용 함수 (샷 타입 매개변수 추가)

    void ApplyShotAdjustments(UPARAM(ref) float& OutSpeed, UPARAM(ref) float& OutPitchAngle, UPARAM(ref) float& OutYawAngle, EShotType ShotType);


    FVector CalculatePenaltyDropPositionInternal(bool bUseHoleDirectionPriority) const;
    FVector FindSafePositionAtEqualDistance() const;
    FVector GetSafeDropPositionDefault() const;


};