#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Texture2D.h"
#include "LandscapeChecker.generated.h"

// ✅ 순환 참조 방지: GolfBall.h를 include하지 않고 전방 선언만 사용
// GolfBall.h → LandscapeChecker.h (include)
// LandscapeChecker.h → AGolfBall* (전방 선언으로 해결)
class AGolfBall;

// 지면 타입 열거형
UENUM(BlueprintType)
enum class ELandType : uint8
{
    TeeBox     UMETA(DisplayName = "티박스"),
    Grass      UMETA(DisplayName = "잔디"),
    Fairway    UMETA(DisplayName = "페어웨이"),
    Green      UMETA(DisplayName = "그린"),
    Rough      UMETA(DisplayName = "러프"),
    Sand       UMETA(DisplayName = "모래"),
    Water      UMETA(DisplayName = "물"),
    Rock       UMETA(DisplayName = "바위"),
    Concrete   UMETA(DisplayName = "콘크리트"),
    Mud        UMETA(DisplayName = "진흙"),
    Unknown    UMETA(DisplayName = "알 수 없음")
};

// 지면 특성 구조체
USTRUCT(BlueprintType)
struct FLandProperties
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Land Properties")
    ELandType LandType = ELandType::Grass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Land Properties")
    float FrictionMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Land Properties")
    float BounceMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Land Properties")
    float SpeedReduction = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Land Properties")
    FLinearColor DebugColor = FLinearColor::Green;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Land Properties")
    FString DisplayName = TEXT("Grass");

    FLandProperties() {}
    FLandProperties(ELandType InLandType, float InFriction, float InBounce, float InSpeedReduction,
        const FLinearColor& InColor, const FString& InDisplayName)
        : LandType(InLandType)
        , FrictionMultiplier(InFriction)
        , BounceMultiplier(InBounce)
        , SpeedReduction(InSpeedReduction)
        , DebugColor(InColor)
        , DisplayName(InDisplayName)
    {
    }
};

// Physical Material 매핑 구조체
USTRUCT(BlueprintType)
struct FPhysicalMaterialMapping
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material Mapping")
    UPhysicalMaterial* PhysicalMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material Mapping")
    FLandProperties LandProperties;
};

// 지면 체크 결과 구조체
USTRUCT(BlueprintType)
struct FLandCheckResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Check Result")
    bool bHitGround = false;

    UPROPERTY(BlueprintReadOnly, Category = "Check Result")
    FVector HitLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Check Result")
    FVector HitNormal = FVector::UpVector;

    UPROPERTY(BlueprintReadOnly, Category = "Check Result")
    UPhysicalMaterial* HitPhysicalMaterial = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Check Result")
    FLandProperties LandProperties;

    UPROPERTY(BlueprintReadOnly, Category = "Check Result")
    AActor* HitActor = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Check Result")
    UPrimitiveComponent* HitComponent = nullptr;
};

/**
 * Landscape Physical Material 기반 지면 타입 체크 시스템
 * UE4.26 호환
 */
UCLASS(BlueprintType, Blueprintable)
class PARKDAY_API ALandscapeChecker : public AActor
{
    GENERATED_BODY()

public:
    ALandscapeChecker();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void BeginDestroy() override;

    // ===== 메인 지면 체크 =====
    UFUNCTION(BlueprintCallable, Category = "Landscape Checker")
    FLandCheckResult CheckGroundAtLocation(const FVector& WorldLocation, float TraceDistance = 1000.0f);

    UFUNCTION(BlueprintCallable, Category = "Landscape Checker")
    ELandType GetLandTypeAtLocation(const FVector& WorldLocation, float TraceDistance = 1000.0f);

    UFUNCTION(BlueprintCallable, Category = "Landscape Checker")
    FLandProperties GetLandPropertiesAtLocation(const FVector& WorldLocation, float TraceDistance = 1000.0f);

    // ===== 고급 체크 =====
    UFUNCTION(BlueprintCallable, Category = "Landscape Checker")
    TArray<FLandCheckResult> CheckGroundInRadius(const FVector& CenterLocation, float Radius, int32 SampleCount = 8);

    UFUNCTION(BlueprintCallable, Category = "Landscape Checker")
    bool IsLocationOnWater(const FVector& WorldLocation);

    UFUNCTION(BlueprintCallable, Category = "Landscape Checker")
    bool IsLocationOnSand(const FVector& WorldLocation);

    // ===== 유틸리티 =====
    UFUNCTION(BlueprintCallable, Category = "Landscape Checker")
    void AddPhysicalMaterialMapping(UPhysicalMaterial* PhysMat, const FLandProperties& Properties);

    UFUNCTION(BlueprintCallable, Category = "Landscape Checker")
    void RemovePhysicalMaterialMapping(UPhysicalMaterial* PhysMat);

    UFUNCTION(BlueprintCallable, Category = "Landscape Checker")
    void SetupDefaultMaterialMappings();

    // ===== 디버그/시각화 =====
    UFUNCTION(BlueprintCallable, Category = "Landscape Debug")
    void ShowLandTypeAtLocation(const FVector& WorldLocation, float DisplayTime = 3.0f);

    UFUNCTION(BlueprintCallable, Category = "Landscape Debug")
    void ToggleDebugMode();

    UFUNCTION(BlueprintCallable, Category = "Landscape Debug")
    void DrawDebugLandGrid(const FVector& CenterLocation, float GridSize = 1000.0f, int32 GridResolution = 10);

    UFUNCTION(BlueprintCallable, Category = "Landscape Debug")
    void SetDrawTrace(bool bEnable);

    // ===== 캐싱 =====
    UFUNCTION(BlueprintCallable, Category = "Landscape Cache")
    void EnableCaching(bool bEnable);

    UFUNCTION(BlueprintCallable, Category = "Landscape Cache")
    void ClearCache();

    // ===== 설정 =====

    // 레이어/PMat 매핑
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape Settings")
    TArray<FPhysicalMaterialMapping> PhysicalMaterialMappings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape Settings")
    float DefaultTraceDistance = 1000.0f;

    // 복잡 콜리전 사용 여부 (Landscape는 Simple 권장)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape Settings")
    bool bUseComplexCollision = false; // 기본값: false (중요)

    // 트레이스 대상 오브젝트 타입(기본: WorldStatic, WorldDynamic)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape Settings")
    TArray<TEnumAsByte<EObjectTypeQuery>> TraceObjectTypes;

    // 중간 충돌물을 넘어 최하단(가장 깊은) 히트 선택
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape Settings")
    bool bPreferDeepestHit = false;

    // 디버그 표시
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings")
    bool bShowDebugInfo = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings")
    float DebugSphereSize = 20.0f;

    // 라인 트레이스 가시화
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings")
    bool bDrawTrace = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings")
    float TraceLineLifeTime = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings")
    float TraceLineThickness = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings")
    FColor TraceColorHit = FColor::Green;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings")
    FColor TraceColorMiss = FColor::Red;

    // 기본 PMat(이름에 "Default" 포함) 결과는 캐시 생략
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cache Settings")
    bool bSkipCacheIfDefaultPMat = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cache Settings")
    bool bEnableCaching = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cache Settings")
    float CacheGridSize = 100.0f; // 1m 스냅

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cache Settings")
    int32 MaxCacheEntries = 1000;

    // 정적 인스턴스
    static ALandscapeChecker* GetLandscapeChecker(UWorld* World);

protected:
    // 내부
    FLandCheckResult PerformLineTrace(const FVector& StartLocation, const FVector& EndLocation);
    FLandProperties GetPropertiesFromPhysicalMaterial(UPhysicalMaterial* PhysMat);
    FVector2D WorldLocationToGridKey(const FVector& WorldLocation);
    bool GetCachedResult(const FVector& WorldLocation, FLandCheckResult& OutResult);
    void CacheResult(const FVector& WorldLocation, const FLandCheckResult& Result);
    void InitializeDefaultMappings();
    void CleanupOldCacheEntries();

    // 보조: 멀티 트레이스에서 최적 히트 선택(가장 깊은 것)
    const FHitResult* PickBestHit(const TArray<FHitResult>& Hits) const;

    struct FCacheEntry
    {
        FLandCheckResult Result;
        float TimeStamp = 0.f;
        FCacheEntry() = default;
        FCacheEntry(const FLandCheckResult& InResult, float InTimeStamp) : Result(InResult), TimeStamp(InTimeStamp) {}
    };

    TMap<FVector2D, FCacheEntry> LocationCache;

    static ALandscapeChecker* Instance;

private:
    float CacheCleanupTimer = 0.0f;
    static constexpr float CACHE_CLEANUP_INTERVAL = 30.0f;
    static constexpr float CACHE_ENTRY_LIFETIME = 60.0f;

    // 디버그 카운터
    mutable int32 DebugTraceCount = 0;
    mutable int32 DebugCacheHitCount = 0;
    mutable int32 DebugCacheMissCount = 0;

    // ===== 성능 최적화: 마스크 픽셀 데이터 BeginPlay 1회 캐시 =====
    // SampleMaskAtUV 매 호출마다 GetMipData() 수MB 복사 → 제거
    TArray<uint8>  CachedMaskPixels;   // 사전 로드된 픽셀 Raw 데이터
    int32          CachedMaskWidth = 0;
    int32          CachedMaskHeight = 0;
    bool           bMaskCacheReady = false;
    void           CacheMaskPixelData(); // BeginPlay에서 호출

    // ===== 성능 최적화: GolfBall 배열 캐시 =====
    // PerformLineTrace 매 호출마다 GetAuthGameMode+Cast+루프 → 제거
    TArray<AGolfBall*> CachedIgnoredBalls;
    void               RebuildIgnoredBallCache();




public:
    // ===== 마스크 기반 체크 함수들 =====
    UFUNCTION(BlueprintCallable, Category = "Landscape Checker|Mask")
    ELandType GetLandTypeFromMask(const FVector& WorldLocation);

    UFUNCTION(BlueprintCallable, Category = "Landscape Checker|Mask")
    FLandProperties GetLandPropertiesFromMask(const FVector& WorldLocation);

    UFUNCTION(BlueprintCallable, Category = "Landscape Checker|Mask")
    FLandCheckResult CheckGroundAtLocationWithMask(const FVector& WorldLocation, float TraceDistance = 1000.0f);

    UFUNCTION(BlueprintCallable, Category = "Landscape Checker|Mask")
    void SetMaskTexture(UTexture2D* InMaskTexture);

    UFUNCTION(BlueprintCallable, Category = "Landscape Checker|Mask")
    void SetMaskWorldBounds(const FVector& InWorldMin, const FVector& InWorldMax);

protected:
    // 마스크 관련 내부 함수들
    FColor SampleMaskAtWorldLocation(const FVector& WorldLocation);
    ELandType ConvertMaskColorToLandType(const FColor& MaskColor);
    FLandProperties GetPropertiesFromMaskLandType(ELandType LandType);
    FVector2D WorldLocationToMaskUV(const FVector& WorldLocation);

public:
    // ===== 마스크 설정 =====
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask Settings")
    UTexture2D* MaskTexture = nullptr;

    // 월드 좌표 범위 (마스크 텍스처가 커버하는 월드 영역)
    // 월드 좌표 범위 (마스크 텍스처가 커버하는 월드 영역)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask Settings")
    FVector MaskWorldMin = FVector(-5000.0f, -5000.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask Settings")
    FVector MaskWorldMax = FVector(5000.0f, 5000.0f, 0.0f);

    // 마스크 체크 활성화
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask Settings")
    bool bUseMaskTexture = false;

    // RGB 임계값 설정 (0-255)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask Settings")
    uint8 BunkerRedThreshold = 128;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask Settings")
    uint8 GreenGreenThreshold = 128;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask Settings")
    uint8 FairWayGreenThreshold = 128;

public:
    // 마스크 관련 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask Settings")
    bool bClampUVOutOfBounds = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask Settings")
    FColor DefaultOutOfBoundsColor = FColor::Black;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask Settings")
    bool bVerboseMaskSampling = false;

    // 디버그 함수들
    UFUNCTION(BlueprintCallable, Category = "Landscape Checker|Debug")
    void AutoCalculateMaskWorldBounds();

    void AnalyzeLandscapeBounds();

private:
    // 내부 함수
    FColor SampleMaskAtUV(const FVector2D& UV);

};