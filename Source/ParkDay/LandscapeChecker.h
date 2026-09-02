// Copyright (c) 2026 365ParkGolf. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Texture2D.h"
#include "LandscapeChecker.generated.h"

// ✅ 순환 참조 방지: GolfBall.h를 include하지 않고 전방 선언만 사용
class AGolfBall;

// ─────────────────────────────────────────────────────────────
// 지면 타입 열거형
// ─────────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class ELandType : uint8
{
    TeeBox    UMETA(DisplayName = "티박스"),
    Grass     UMETA(DisplayName = "잔디"),
    Fairway   UMETA(DisplayName = "페어웨이"),
    Green     UMETA(DisplayName = "그린"),
    Rough     UMETA(DisplayName = "러프"),
    Sand      UMETA(DisplayName = "모래"),
    Water     UMETA(DisplayName = "물"),
    Tree      UMETA(DisplayName = "나무"),
    Leaves    UMETA(DisplayName = "나뭇잎"),
    Net       UMETA(DisplayName = "네트"),
    Rock      UMETA(DisplayName = "바위"),
    Concrete  UMETA(DisplayName = "콘크리트"),
    Mud       UMETA(DisplayName = "진흙"),
    Unknown   UMETA(DisplayName = "알 수 없음")
};

// ─────────────────────────────────────────────────────────────
// 지면 특성 구조체
// ─────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FLandProperties
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ELandType LandType = ELandType::Grass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FrictionMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BounceMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SpeedReduction = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor DebugColor = FLinearColor::Green;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
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

// ─────────────────────────────────────────────────────────────
// Physical Material 매핑 구조체
// ─────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FPhysicalMaterialMapping
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material Mapping")
    UPhysicalMaterial* PhysicalMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material Mapping",
        meta = (ShowOnlyInnerProperties))
    FLandProperties LandProperties;
};

// ─────────────────────────────────────────────────────────────
// 지면 체크 결과 구조체
// ─────────────────────────────────────────────────────────────
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
 * Landscape / StaticMesh 지형 판정 통합 시스템
 *
 * 판정 우선순위:
 *   1) Mask 텍스처 (RGB 픽셀 판정)
 *   2) PhysMat 트레이스 (Landscape LayerInfo + StaticMesh)
 *   3) 폴백 = Rough
 *
 * Landscape 맵, Mesh 맵, Landscape+Mesh 혼합 맵을 하나의 API로 처리.
 * AnalyzeLandscapeBounds()가 Landscape 없으면 자동으로 bUseMaskTexture=false 전환.
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

    // ═══════════════════════════════════════════════════════════
    // ★ 통합 API — GolfBall에서는 이것만 호출하면 됨
    // ═══════════════════════════════════════════════════════════

    /**
     * 지형 타입 단일 진입점.
     * 우선순위: Mask → PhysMat 트레이스 → Rough.
     * 반드시 유효한 값 반환 (Unknown 없음).
     */
    UFUNCTION(BlueprintCallable, Category = "Landscape Checker|Unified")
    ELandType ResolveLandTypeAt(const FVector& WorldLocation);

    /**
     * 지형 특성 단일 진입점.
     * ResolveLandTypeAt() + FLandProperties 매핑.
     */
    UFUNCTION(BlueprintCallable, Category = "Landscape Checker|Unified")
    FLandProperties ResolveLandPropertiesAt(const FVector& WorldLocation);

    // ═══════════════════════════════════════════════════════════
    // 기존 API (호환성 유지) — 내부적으로 통합 API로 delegate
    // ═══════════════════════════════════════════════════════════

    UFUNCTION(BlueprintCallable, Category = "Landscape Checker")
    FLandCheckResult CheckGroundAtLocation(const FVector& WorldLocation, float TraceDistance = 1000.0f);

    UFUNCTION(BlueprintCallable, Category = "Landscape Checker")
    ELandType GetLandTypeAtLocation(const FVector& WorldLocation, float TraceDistance = 1000.0f);

    UFUNCTION(BlueprintCallable, Category = "Landscape Checker")
    FLandProperties GetLandPropertiesAtLocation(const FVector& WorldLocation, float TraceDistance = 1000.0f);

    // ═══════════════════════════════════════════════════════════
    // 고급 체크
    // ═══════════════════════════════════════════════════════════

    UFUNCTION(BlueprintCallable, Category = "Landscape Checker")
    TArray<FLandCheckResult> CheckGroundInRadius(const FVector& CenterLocation, float Radius, int32 SampleCount = 8);

    UFUNCTION(BlueprintCallable, Category = "Landscape Checker")
    bool IsLocationOnWater(const FVector& WorldLocation);

    UFUNCTION(BlueprintCallable, Category = "Landscape Checker")
    bool IsLocationOnSand(const FVector& WorldLocation);

    // ═══════════════════════════════════════════════════════════
    // 매핑 관리
    // ═══════════════════════════════════════════════════════════

    UFUNCTION(BlueprintCallable, Category = "Landscape Checker")
    void AddPhysicalMaterialMapping(UPhysicalMaterial* PhysMat, const FLandProperties& Properties);

    UFUNCTION(BlueprintCallable, Category = "Landscape Checker")
    void RemovePhysicalMaterialMapping(UPhysicalMaterial* PhysMat);

    UFUNCTION(BlueprintCallable, Category = "Landscape Checker")
    void SetupDefaultMaterialMappings();

    // ═══════════════════════════════════════════════════════════
    // 디버그/시각화
    // ═══════════════════════════════════════════════════════════

    UFUNCTION(BlueprintCallable, Category = "Landscape Debug")
    void ShowLandTypeAtLocation(const FVector& WorldLocation, float DisplayTime = 3.0f);

    UFUNCTION(BlueprintCallable, Category = "Landscape Debug")
    void ToggleDebugMode();

    UFUNCTION(BlueprintCallable, Category = "Landscape Debug")
    void DrawDebugLandGrid(const FVector& CenterLocation, float GridSize = 1000.0f, int32 GridResolution = 10);

    UFUNCTION(BlueprintCallable, Category = "Landscape Debug")
    void SetDrawTrace(bool bEnable);

    // ═══════════════════════════════════════════════════════════
    // 캐싱
    // ═══════════════════════════════════════════════════════════

    UFUNCTION(BlueprintCallable, Category = "Landscape Cache")
    void EnableCaching(bool bEnable);

    UFUNCTION(BlueprintCallable, Category = "Landscape Cache")
    void ClearCache();

    // ═══════════════════════════════════════════════════════════
    // 마스크 설정 API (기존 유지)
    // ═══════════════════════════════════════════════════════════

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

    UFUNCTION(BlueprintCallable, Category = "Landscape Checker|Debug")
    void AutoCalculateMaskWorldBounds();

    void AnalyzeLandscapeBounds();

    // ═══════════════════════════════════════════════════════════
    // 설정 (Editor 노출)
    // ═══════════════════════════════════════════════════════════

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape Settings")
    TArray<FPhysicalMaterialMapping> PhysicalMaterialMappings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape Settings")
    float DefaultTraceDistance = 1000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape Settings")
    bool bUseComplexCollision = false;  // Landscape는 Simple 권장

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape Settings")
    TArray<TEnumAsByte<EObjectTypeQuery>> TraceObjectTypes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape Settings")
    bool bPreferDeepestHit = false;

    // ─── 디버그 표시 ─────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings")
    bool bShowDebugInfo = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Settings")
    float DebugSphereSize = 20.0f;

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

    // ─── 캐시 설정 ─────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cache Settings")
    bool bSkipCacheIfDefaultPMat = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cache Settings")
    bool bEnableCaching = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cache Settings")
    float CacheGridSize = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cache Settings")
    int32 MaxCacheEntries = 1000;

    // ─── 마스크 설정 ─────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask Settings")
    UTexture2D* MaskTexture = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask Settings")
    FVector MaskWorldMin = FVector(-5000.0f, -5000.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask Settings")
    FVector MaskWorldMax = FVector(5000.0f, 5000.0f, 0.0f);

    /** 마스크 사용 여부 (Landscape 부재 시 AnalyzeLandscapeBounds가 false로 전환) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask Settings")
    bool bUseMaskTexture = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask Settings")
    uint8 BunkerRedThreshold =8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask Settings")
    uint8 GreenGreenThreshold = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask Settings")
    uint8 FairWayGreenThreshold = 8;

    /** UV 범위 밖일 때 Clamp할지 (false = PhysMat 폴백으로 넘김, 권장) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask Settings")
    bool bClampUVOutOfBounds = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask Settings")
    FColor DefaultOutOfBoundsColor = FColor::Black;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask Settings")
    bool bVerboseMaskSampling = false;

    // 정적 인스턴스
    static ALandscapeChecker* GetLandscapeChecker(UWorld* World);

    /**
         * 하드코딩 마스크 경로 (Landscape와 무관한 지형 판정용 텍스처)
         * 이 경로의 텍스처가 자동 로드됨
         */
    static constexpr const TCHAR* DEFAULT_MASK_ASSET_PATH = TEXT("/Game/Landscape_Material/mask.mask");

    /**
     * MaskTexture가 nullptr일 때 하드코딩 경로에서 로드 시도
     * BeginPlay와 생성자에서 자동 호출됨
     * @return 로드 성공 여부
     */
    bool LoadDefaultMaskTexture();

    /**
     * 마스크 텍스처 경로 (에디터에서 오버라이드 가능)
     * 비어있으면 DEFAULT_MASK_ASSET_PATH 사용
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask Settings")
    FString MaskTexturePath;

    /**
     * BeginPlay에서 강제로 하드코딩 경로 사용 여부
     * true = 에디터에서 세팅한 값을 덮어쓰고 하드코딩 경로 로드
     * false = 에디터에서 세팅한 MaskTexture 우선
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mask Settings")
    bool bForceUseHardcodedMask = true;

protected:
    // ═══════════════════════════════════════════════════════════
    // ★ 통합 API 내부 구현
    // ═══════════════════════════════════════════════════════════

    /** 마스크 텍스처로 지형 판정 시도. 실패(마스크 없음/UV 범위 밖/Unknown) 시 false */
    bool TryGetLandTypeFromMask(const FVector& WorldLocation, ELandType& OutLandType);

    /** PhysMat 트레이스로 지형 판정 시도. Landscape+StaticMesh 공통 처리 */
    bool TryGetLandTypeFromPhysMat(const FVector& WorldLocation, ELandType& OutLandType);

    /** PhysMat 이름 → ELandType 매핑 (Contains 기반) */
    ELandType PhysMatNameToLandType(const FString& PhysMatName) const;

    /** UV가 마스크 유효 범위 내인지 (bClampUVOutOfBounds 옵션 반영) */
    bool IsMaskUVInBounds(const FVector2D& UV) const;

    // ═══════════════════════════════════════════════════════════
    // 마스크 관련 내부
    // ═══════════════════════════════════════════════════════════

    FColor SampleMaskAtWorldLocation(const FVector& WorldLocation);
    ELandType ConvertMaskColorToLandType(const FColor& MaskColor);
    FLandProperties GetPropertiesFromMaskLandType(ELandType LandType);
    FVector2D WorldLocationToMaskUV(const FVector& WorldLocation);

    // ═══════════════════════════════════════════════════════════
    // 트레이스/캐시 내부
    // ═══════════════════════════════════════════════════════════

    FLandCheckResult PerformLineTrace(const FVector& StartLocation, const FVector& EndLocation);
    FLandProperties GetPropertiesFromPhysicalMaterial(UPhysicalMaterial* PhysMat);
    FVector2D WorldLocationToGridKey(const FVector& WorldLocation);
    bool GetCachedResult(const FVector& WorldLocation, FLandCheckResult& OutResult);
    void CacheResult(const FVector& WorldLocation, const FLandCheckResult& Result);
    void InitializeDefaultMappings();
    void CleanupOldCacheEntries();

    const FHitResult* PickBestHit(const TArray<FHitResult>& Hits) const;

    struct FCacheEntry
    {
        FLandCheckResult Result;
        float TimeStamp = 0.f;
        FCacheEntry() = default;
        FCacheEntry(const FLandCheckResult& InResult, float InTimeStamp)
            : Result(InResult), TimeStamp(InTimeStamp) {
        }
    };

    TMap<FVector2D, FCacheEntry> LocationCache;

    static ALandscapeChecker* Instance;

private:
    float CacheCleanupTimer = 0.0f;
    static constexpr float CACHE_CLEANUP_INTERVAL = 30.0f;
    static constexpr float CACHE_ENTRY_LIFETIME = 60.0f;

    mutable int32 DebugTraceCount = 0;
    mutable int32 DebugCacheHitCount = 0;
    mutable int32 DebugCacheMissCount = 0;

    // ─── 성능 최적화: 마스크 픽셀 데이터 BeginPlay 1회 캐시 ───
    TArray<uint8> CachedMaskPixels;
    int32 CachedMaskWidth = 0;
    int32 CachedMaskHeight = 0;
    bool bMaskCacheReady = false;
    void CacheMaskPixelData();

    // ─── 성능 최적화: GolfBall 배열 캐시 (트레이스 시 Ignore 목록) ───
    TArray<AGolfBall*> CachedIgnoredBalls;
    void RebuildIgnoredBallCache();

    // ─── 내부 마스크 샘플링 ───
    FColor SampleMaskAtUV(const FVector2D& UV);
};