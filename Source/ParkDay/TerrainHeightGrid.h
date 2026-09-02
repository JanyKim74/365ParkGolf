#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Landscape.h"
#include "TerrainHeightGrid.generated.h"

// 전방 선언
class ALandscape;

// 격자 포인트 정보
USTRUCT(BlueprintType)
struct FGridPoint
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FVector WorldPosition;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float Height;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float Slope;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FVector FlowDirection;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FLinearColor HeightColor;

    // ★ 추가: 이 포인트가 "랜드스케이프 위에 있는지" 여부
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    bool bOnLandscape;

    FGridPoint()
        : WorldPosition(FVector::ZeroVector)
        , Height(0.f)
        , Slope(0.f)
        , FlowDirection(FVector::ZeroVector)
        , HeightColor(FLinearColor::White)
        , bOnLandscape(false)
    {}

    FGridPoint(const FVector& InPosition, float InHeight, bool bInOnLandscape)
        : WorldPosition(InPosition)
        , Height(InHeight)
        , Slope(0.f)
        , FlowDirection(FVector::ZeroVector)
        , HeightColor(FLinearColor::White)
        , bOnLandscape(bInOnLandscape)
    {}
};

// 높이별 컬러 설정
USTRUCT(BlueprintType)
struct FHeightColorSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Height Colors")
    float MinHeight = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Height Colors")
    float MaxHeight = 1000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Height Colors")
    FLinearColor LowHeightColor = FLinearColor::Blue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Height Colors")
    FLinearColor MidHeightColor = FLinearColor::Green;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Height Colors")
    FLinearColor HighHeightColor = FLinearColor::Red;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Height Colors")
    float LowToMidThreshold = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Height Colors")
    float MidToHighThreshold = 0.7f;
};

// 물 흐름(속도/조건) 설정
USTRUCT(BlueprintType)
struct FWaterFlowSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Flow")
    float FlowSpeed = 0.33f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Flow")
    float MinSlopeForFlow = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Flow")
    float DotSizeCm = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Flow")
    float BaseSpeedPerMeter = 0.6f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Flow")
    float HeightAccelScale = 0.5f;

    void Init()
    {
        FlowSpeed = 0.23f;
        MinSlopeForFlow = 0.01f;
        DotSizeCm = 0.25f;
        BaseSpeedPerMeter = 0.5f;
        HeightAccelScale = 2.5f;
    }
};

// 선분 위를 이동하는 "흐름 점" 정보
USTRUCT()
struct FFlowMover
{
    GENERATED_BODY()

    int32 HighIdx = INDEX_NONE;
    int32 LowIdx = INDEX_NONE;

    int32 DotInstanceIndex = INDEX_NONE;

    float T = 0.0f;
    float LengthCm = 0.0f;
    float SpeedTPerSec = 0.2f;

    FVector P0 = FVector::ZeroVector; // High
    FVector P1 = FVector::ZeroVector; // Low
};

UCLASS()
class PARKDAY_API ATerrainHeightGrid : public AActor
{
    GENERATED_BODY()

public:
    ATerrainHeightGrid();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

public:
    // 월드에서 이미 있으면 가져오고, 없으면 하나 생성해서 반환
    static ATerrainHeightGrid* GetOrCreateTerrainGrid(UWorld* World);

    // ===== 공개 인터페이스 =====
    UFUNCTION(BlueprintCallable, Category = "Terrain Grid")
    void GenerateGrid(const FVector& CenterPosition);

    UFUNCTION(BlueprintCallable, Category = "Terrain Grid")
    void UpdateGrid(const FVector& NewCenterPosition);

    UFUNCTION(BlueprintCallable, Category = "Terrain Grid")
    void SetGridVisible(bool bVisible);

    UFUNCTION(BlueprintCallable, Category = "Terrain Grid")
    void RefreshHeightColors();

    UFUNCTION(BlueprintCallable, Category = "Terrain Grid")
    void SetGridRadius(float NewRadius);

    UFUNCTION(BlueprintCallable, Category = "Terrain Grid")
    void SetTargetPosition(const FVector& TargetPos);

    UFUNCTION(BlueprintCallable, Category = "Terrain Grid")
    void SetHolecupPosition(const FVector& HolecupPos);

    UFUNCTION(BlueprintPure, Category = "Terrain Grid")
    FVector GetPlayerForwardDirection() const { return PlayerForwardDirection; }

    // Trace에서 무시할 액터 목록
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace|Ignore")
    TArray<AActor*> TraceIgnoreActors;

    UFUNCTION(BlueprintCallable, Category = "Trace|Ignore")
    void AddTraceIgnoreActor(AActor* Actor);

    UFUNCTION(BlueprintCallable, Category = "Trace|Ignore")
    void RemoveTraceIgnoreActor(AActor* Actor);

    UFUNCTION(BlueprintCallable, Category = "Trace|Ignore")
    void ClearTraceIgnoreActors();

    UFUNCTION()
    void UpdateRotationFromCamera();

    // ===== 설정 =====
    // 격자
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Settings", meta = (ClampMin = "10", ClampMax = "200.0"))
    float GridSpacing = 50.0f; //

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Settings", meta = (ClampMin = "10.0", ClampMax = "200.0"))
    float GridRadius = 50.0f; // 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Settings")
    float GridPointSize = 0.0f; // cm

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Settings")
    float GridLineThickness = 1.0f; // cm

    // 컬러/흐름
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Height Visualization")
    FHeightColorSettings HeightColorSettings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Flow")
    FWaterFlowSettings WaterFlowSettings;

    // 성능/디버그
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    bool bUseInstancedMesh = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    float UpdateFrequency = 2.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    int32 MaxGridPoints = 1000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bShowDebugInfo = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bShowFlowVectors = false;

    // 이벤트
    //UFUNCTION(BlueprintImplementableEvent, Category = "Terrain Grid Events")
    //void OnGridGenerated(int32 GridPointCount);

    //UFUNCTION(BlueprintImplementableEvent, Category = "Terrain Grid Events")
    //void OnHeightDataUpdated(float MinHeight, float MaxHeight, float AverageHeight);

protected:
    // ===== 컴포넌트 =====
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UInstancedStaticMeshComponent* GridPointMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UInstancedStaticMeshComponent* GridLineMesh;

    // 선분 위 흐르는 점
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UInstancedStaticMeshComponent* FlowDotMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* RootSceneComponent;

    // ===== 머티리얼/메시 =====
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials")
    UMaterial* GridPointMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials")
    UMaterial* GridLineMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials")
    UMaterial* FlowDotMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshes")
    UStaticMesh* GridPointStaticMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshes")
    UStaticMesh* GridLineStaticMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshes")
    UStaticMesh* FlowDotStaticMesh = nullptr;

    // 방향/타겟
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid Settings")
    FVector TargetPosition = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid Settings")
    FVector HolecupPosition = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid Settings")
    FVector PlayerForwardDirection = FVector(1, 0, 0);

private:
    // ===== 내부 데이터 =====
    TArray<FGridPoint> GridPoints;
    TArray<FTransform> GridPointTransforms;
    TArray<FTransform> GridLineTransforms;

    // 흐름 점(선분별 1개)
    TArray<FFlowMover> FlowMovers;

    FVector CurrentCenterPosition = FVector::ZeroVector;
    float LastUpdateTime = 0.f;
    bool bGridGenerated = false;

    // 높이 통계
    float MinHeightInGrid = 0.f;
    float MaxHeightInGrid = 0.f;
    float AverageHeightInGrid = 0.f;

    // Landscape 캐시
    UPROPERTY(Transient)
    ALandscape* CachedLandscape = nullptr;

    // ===== 내부 함수 =====
    void InitializeComponents();
    void LoadDefaultResources();

    void GenerateGridPoints(const FVector& CenterPos);
    void CalculateHeightsAndSlopes();
    void CalculateFlowDirections();
    void UpdateInstanceTransforms();

    // FlowDot 빌드/업데이트
    void BuildFlowMovers();
    void ClearFlowMovers();
    void UpdateFlowMovers(float DeltaTime);

    // 샘플/유틸
    // ★ 변경: 랜드스케이프 히트 여부를 반환
    bool SampleHeightAtLocation(const FVector& Location, float& OutZ);
    FLinearColor CalculateHeightColor(float Height);
    FLinearColor InterpolateHeightColor(float NormalizedHeight);
    FVector CalculateFlowDirection(const FGridPoint& Point, const TArray<FGridPoint>& NearbyPoints);
    float CalculateSlope(const FGridPoint& Point, const TArray<FGridPoint>& NearbyPoints);
    TArray<FGridPoint> GetNearbyGridPoints(const FGridPoint& CenterPoint, float SearchRadius = 2.0f);
    void UpdateHeightStatistics();

    void DrawDebugInfo();
    void DrawFlowVectors();
    bool ShouldUpdateGrid() const;

    // ★ 추가: Cup_hole%d 이름의 액터인지 판정
    bool IsCupHoleActor(const AActor* A) const;

    // ⭐ 현재 추적 중인 공 (순서 변경 시 중요!)
    class AGolfBall* CurrentTrackedBall = nullptr;

    int32 CurrentBallIndex = -1;  // 공 배열 인덱스

    float CalcSegmentFlowSpeed(const FGridPoint& A, const FGridPoint& B) const;



public:
    // ===== 상수 =====
    static constexpr float MAX_GRID_LENGTH = 1000.0f; // cm (50m)
    static constexpr float GRID_WIDTH      = 300.2f;  // cm (8m로 가정)
    static constexpr float HEIGHT_OFFSET   = 10.0f;   // cm
    static constexpr float DEFAULT_GRID_SPACING = 50.0f; // cm
    static constexpr float DEFAULT_GRID_RADIUS  = 1000.0f; // cm
    static constexpr float MIN_HEIGHT_DIFFERENCE = 0.01f; // cm
    static constexpr float FLOW_VECTOR_LENGTH    = 50.0f; // cm
    // ★ 라인 텍스처 흐름 전용 속도 배율 (FlowDot 점 속도에는 영향 없음)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Flow", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float GridLineFlowSpeedScale = 1.0f;

    static constexpr int32 MAX_NEARBY_POINTS = 8;


    // 정보 조회
    UFUNCTION(BlueprintPure, Category = "Terrain Grid Info")
    int32 GetGridPointCount() const { return GridPoints.Num(); }

    UFUNCTION(BlueprintPure, Category = "Terrain Grid Info")
    float GetMinHeight() const { return MinHeightInGrid; }

    UFUNCTION(BlueprintPure, Category = "Terrain Grid Info")
    float GetMaxHeight() const { return MaxHeightInGrid; }

    UFUNCTION(BlueprintPure, Category = "Terrain Grid Info")
    float GetAverageHeight() const { return AverageHeightInGrid; }

    UFUNCTION(BlueprintPure, Category = "Terrain Grid Info")
    FVector GetCurrentCenter() const { return CurrentCenterPosition; }

    UFUNCTION(BlueprintPure, Category = "Terrain Grid Info")
    bool IsGridVisible() const { return GridPointMesh ? GridPointMesh->IsVisible() : false; }

    // ⭐ 명시적으로 현재 공 설정 (필수!)
    UFUNCTION(BlueprintCallable, Category = "Terrain Grid")
        void SetCurrentBall(AGolfBall* Ball, int32 BallIndex);

    // ⭐ 현재 공 가져오기
    UFUNCTION(BlueprintPure, Category = "Terrain Grid Info")
        class AGolfBall* GetCurrentBall() const { return CurrentTrackedBall; }

    // ⭐ 검증 함수 (디버깅)
    void ValidateBallTracking() const;

    // ★ 추가: 격자 포인트(스피어) 렌더링 on/off (기본 비활성화)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Settings")
    bool bShowGridPoints = false;

    // ★ 추가: 흐름 점(FlowDot) 렌더링 on/off (기본 비활성화)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Settings")
    bool bShowFlowDots = false;
};
