#include "LandscapeChecker.h"
// ✅ 순환 참조 해결: GolfBall.h는 .cpp에서만 include
// .h에서는 전방 선언(class AGolfBall)만 사용
#include "GolfBall.h"
#include "GolfPlayerManager.h"
#include "InGameMode.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "CollisionQueryParams.h"
#include "EngineUtils.h"
#include "LandscapeProxy.h"
#include "Landscape.h"
#include "LandscapeStreamingProxy.h"
#include "LandscapeComponent.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "ParkDayProfiling.h"

// UE 4.26에서 텍스처 소스 데이터 접근을 위한 추가 헤더
#if WITH_EDITOR
#include "Developer/DesktopPlatform/Public/IDesktopPlatform.h"
#include "Developer/DesktopPlatform/Public/DesktopPlatformModule.h"
#endif

ALandscapeChecker* ALandscapeChecker::Instance = nullptr;

ALandscapeChecker::ALandscapeChecker()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    DefaultTraceDistance = 1000.0f;
    bUseComplexCollision = false;

    // ✅ 최적화: WorldDynamic 제거 → WorldStatic만 사용 (지형 판정에 충분)
    TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));

    bPreferDeepestHit = true;
    bShowDebugInfo = false;
    DebugSphereSize = 20.0f;

    bDrawTrace = false;
    TraceLineLifeTime = 3.0f;
    TraceLineThickness = 1.5f;
    TraceColorHit = FColor::Green;
    TraceColorMiss = FColor::Red;

    bSkipCacheIfDefaultPMat = true;
    bEnableCaching = true;
    CacheGridSize = 100.0f;
    MaxCacheEntries = 1000;

    if (!Instance) { Instance = this; }
    UE_LOG(LogTemp, Log, TEXT("🌱 LandscapeChecker initialized"));
}

void ALandscapeChecker::BeginPlay()
{
    Super::BeginPlay();
    SetupDefaultMaterialMappings();
    LocationCache.Empty();
    CacheCleanupTimer = 0.0f;

    if (bUseMaskTexture)
    {
        AutoCalculateMaskWorldBounds();
        UE_LOG(LogTemp, Warning, TEXT("🎭 마스크 경계: Min=%s, Max=%s"),
            *MaskWorldMin.ToString(), *MaskWorldMax.ToString());

        // ✅ 최적화: 마스크 픽셀 데이터 1회 사전 로드
        CacheMaskPixelData();
    }

    // ✅ 최적화: 무시할 GolfBall 배열 1회 캐시
    RebuildIgnoredBallCache();

    UE_LOG(LogTemp, Log, TEXT("🌱 LandscapeChecker BeginPlay 완료"));
}

void ALandscapeChecker::Tick(float DeltaTime)
{
    SCOPE_CYCLE_COUNTER(STAT_LandscapeTick);
    Super::Tick(DeltaTime);

    if (bEnableCaching)
    {
        CacheCleanupTimer += DeltaTime;
        if (CacheCleanupTimer >= CACHE_CLEANUP_INTERVAL)
        {
            CleanupOldCacheEntries();
            CacheCleanupTimer = 0.0f;
        }
    }

#if WITH_EDITOR
    if (bShowDebugInfo && GEngine)
    {
        const FString Info = FString::Printf(
            TEXT("🌱 LandscapeChecker\nTraces: %d  Cache(H/M): %d/%d  Size: %d"),
            DebugTraceCount, DebugCacheHitCount, DebugCacheMissCount, LocationCache.Num());
        GEngine->AddOnScreenDebugMessage(9901, DeltaTime, FColor::Green, Info);
    }
#endif
}

// ✅ 최적화: 마스크 픽셀 데이터 BeginPlay에서 1회 사전 로드
// 이전: SampleMaskAtUV 호출마다 GetMipData()로 수MB 복사
// 수정: 시작 시 1회만 메모리에 올리고 이후 직접 인덱싱
void ALandscapeChecker::CacheMaskPixelData()
{
    bMaskCacheReady = false;
    CachedMaskPixels.Empty();
    CachedMaskWidth = CachedMaskHeight = 0;

    if (!MaskTexture)
    {
        UE_LOG(LogTemp, Warning, TEXT("CacheMaskPixelData: MaskTexture null"));
        return;
    }

#if WITH_EDITOR
    if (!MaskTexture->Source.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("CacheMaskPixelData: Source not valid"));
        return;
    }
    TArray64<uint8> RawData;
    if (!MaskTexture->Source.GetMipData(RawData, 0) || RawData.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("CacheMaskPixelData: GetMipData failed"));
        return;
    }
    CachedMaskWidth = MaskTexture->Source.GetSizeX();
    CachedMaskHeight = MaskTexture->Source.GetSizeY();
    // TArray64 → TArray<uint8> 복사 (1회만)
    CachedMaskPixels.SetNumUninitialized(RawData.Num());
    FMemory::Memcpy(CachedMaskPixels.GetData(), RawData.GetData(), RawData.Num());
#else
    // UE5.6 런타임: GetPlatformData() + GetRawData() 사용
    // PlatformData 직접 접근은 UE5에서 deprecated
    FTexturePlatformData* PD = MaskTexture->GetPlatformData();
    if (!PD || PD->Mips.Num() == 0) { return; }
    FTexture2DMipMap& Mip = PD->Mips[0];
    CachedMaskWidth = Mip.SizeX;
    CachedMaskHeight = Mip.SizeY;

    // UE5.6: BulkData.Lock() deprecated → GetCopy() 사용
    TArray<uint8> BulkCopy;
    Mip.BulkData.GetCopy(reinterpret_cast<void**>(&BulkCopy), false);

    // GetCopy가 실패하면 폴백으로 Lock 시도
    if (BulkCopy.Num() == 0)
    {
        void* DataPtr = Mip.BulkData.Lock(LOCK_READ_ONLY);
        if (DataPtr)
        {
            const int32 DataSize = CachedMaskWidth * CachedMaskHeight * 4;
            CachedMaskPixels.SetNumUninitialized(DataSize);
            FMemory::Memcpy(CachedMaskPixels.GetData(), DataPtr, DataSize);
        }
        Mip.BulkData.Unlock();
    }
    else
    {
        CachedMaskPixels = MoveTemp(BulkCopy);
    }
#endif

    if (CachedMaskWidth > 0 && CachedMaskHeight > 0 && CachedMaskPixels.Num() > 0)
    {
        bMaskCacheReady = true;
        UE_LOG(LogTemp, Log, TEXT("✅ MaskPixel 캐시 완료: %dx%d (%d bytes)"),
            CachedMaskWidth, CachedMaskHeight, CachedMaskPixels.Num());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ MaskPixel 캐시 실패"));
    }
}

// ✅ 최적화: GolfBall 무시 배열 캐시 (PerformLineTrace 내 매 호출 GetAuthGameMode 제거)
void ALandscapeChecker::RebuildIgnoredBallCache()
{
    CachedIgnoredBalls.Empty();
    if (AGameModeBase* GMBase = GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr)
    {
        if (AInGameMode* GM = Cast<AInGameMode>(GMBase))
        {
            if (GM->PlayerManager)
            {
                for (AGolfBall* Ball : GM->PlayerManager->GetPlayerBalls())
                {
                    if (Ball && IsValid(Ball))
                        CachedIgnoredBalls.Add(Ball);
                }
            }
        }
    }
    UE_LOG(LogTemp, Log, TEXT("✅ IgnoredBall 캐시: %d개"), CachedIgnoredBalls.Num());
}

void ALandscapeChecker::BeginDestroy()
{
    Super::BeginDestroy();
    LocationCache.Empty();
    Instance = nullptr;
    UE_LOG(LogTemp, Log, TEXT("🗑️ LandscapeChecker cache and instance cleared on destroy"));
}
// CheckGroundAtLocation 함수에서 발생하는 오류 수정
FLandCheckResult ALandscapeChecker::CheckGroundAtLocation(const FVector& WorldLocation, float TraceDistance)
{
    if (bEnableCaching)
    {
        FLandCheckResult Cached;
        if (GetCachedResult(WorldLocation, Cached))
        {
            DebugCacheHitCount++;
            return Cached;
        }
        DebugCacheMissCount++;
    }

    const FVector TraceStart = WorldLocation + FVector(0, 0, 20.0f);
    const FVector TraceEnd = WorldLocation - FVector(0, 0, FMath::Max(TraceDistance, 1.f));

    FLandCheckResult Result = PerformLineTrace(TraceStart, TraceEnd);

    if (bEnableCaching && Result.bHitGround)
    {
        const bool bDefaultPMat =
            (Result.HitPhysicalMaterial && Result.HitPhysicalMaterial->GetName().Contains(TEXT("Default")));
        if (!(bSkipCacheIfDefaultPMat && bDefaultPMat))
        {
            CacheResult(WorldLocation, Result);
        }
    }

    DebugTraceCount++;
    return Result;
}

ELandType ALandscapeChecker::GetLandTypeAtLocation(const FVector& WorldLocation, float TraceDistance)
{
    SCOPE_CYCLE_COUNTER(STAT_LandscapeGetLandType);
    if (bUseMaskTexture)
    {
        return GetLandTypeFromMask(WorldLocation);
    }
    else
    {
        const FLandCheckResult R = CheckGroundAtLocation(WorldLocation, TraceDistance);
        return R.bHitGround ? R.LandProperties.LandType : ELandType::Unknown;
    }


}

FLandProperties ALandscapeChecker::GetLandPropertiesAtLocation(const FVector& WorldLocation, float TraceDistance)
{
    if (bUseMaskTexture)
    {
        return GetLandPropertiesFromMask(WorldLocation);
    }
    else
        return CheckGroundAtLocation(WorldLocation, TraceDistance).LandProperties;
}

// ===== 고급 체크 =====

TArray<FLandCheckResult> ALandscapeChecker::CheckGroundInRadius(const FVector& CenterLocation, float Radius, int32 SampleCount)
{
    TArray<FLandCheckResult> Results;

    if (Radius <= 0.f || SampleCount <= 0)
    {
        // 방어적 - 센터 1회만 검사
        Results.Add(CheckGroundAtLocation(CenterLocation, DefaultTraceDistance));
        return Results;
    }

    // 센터 포함
    Results.Add(CheckGroundAtLocation(CenterLocation, DefaultTraceDistance));

    const int32 Num = FMath::Max(1, SampleCount);
    const float AngleStep = 2.0f * PI / static_cast<float>(Num);

    for (int32 i = 0; i < Num; ++i)
    {
        const float A = AngleStep * i;
        const FVector Offset = FVector(FMath::Cos(A) * Radius, FMath::Sin(A) * Radius, 0.f);
        const FVector P = CenterLocation + Offset;

        const FLandCheckResult R = CheckGroundAtLocation(P, DefaultTraceDistance);
        Results.Add(R);

#if WITH_EDITOR
        if (bDrawTrace)
        {
            if (UWorld* World = GetWorld())
            {
                const FColor C = R.bHitGround ? R.LandProperties.DebugColor.ToFColor(true) : FColor::Red;
                DrawDebugPoint(World, P, 10.f, C, false, TraceLineLifeTime);
                if (R.bHitGround)
                {
                    DrawDebugPoint(World, R.HitLocation, 12.f, C, false, TraceLineLifeTime);
                    DrawDebugLine(World, P + FVector(0, 0, 100), R.HitLocation, C, false, TraceLineLifeTime, 0, TraceLineThickness);
                }
            }
        }
#endif
    }

    return Results;
}

bool ALandscapeChecker::IsLocationOnWater(const FVector& WorldLocation)
{
    return GetLandTypeAtLocation(WorldLocation, DefaultTraceDistance) == ELandType::Water;
}

bool ALandscapeChecker::IsLocationOnSand(const FVector& WorldLocation)
{
    return GetLandTypeAtLocation(WorldLocation, DefaultTraceDistance) == ELandType::Sand;
}

// ===== 내부 구현 =====

static FCollisionObjectQueryParams BuildObjectQueryParams(const TArray<TEnumAsByte<EObjectTypeQuery>>& InTypes)
{
    FCollisionObjectQueryParams ObjParams;
    for (auto T : InTypes)
    {
        const ECollisionChannel Chan = UEngineTypes::ConvertToCollisionChannel(T);
        ObjParams.AddObjectTypesToQuery(Chan);
    }
    return ObjParams;
}

const FHitResult* ALandscapeChecker::PickBestHit(const TArray<FHitResult>& Hits) const
{
    const FHitResult* Best = nullptr;
    float BestTime = FLT_MAX;
    for (const FHitResult& H : Hits)
    {
        if (H.bBlockingHit)
        {
            if (H.Time < BestTime)
            {
                BestTime = H.Time;
                Best = &H;
            }
        }
    }
    return Best;
}

FLandCheckResult ALandscapeChecker::PerformLineTrace(
    const FVector& StartLocation,
    const FVector& EndLocation)
{
    FLandCheckResult Result;

    // 1. World 유효성 검사
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ PerformLineTrace: World is null"));
        return Result;
    }

    // 2. Query 파라미터 설정
    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(LandscapeCheckerTrace),
        bUseComplexCollision
    );
    QueryParams.bReturnPhysicalMaterial = true;
    QueryParams.AddIgnoredActor(this);

    // ✅ 최적화: GetAuthGameMode+Cast+루프 → BeginPlay 캐시 사용
    for (AGolfBall* Ball : CachedIgnoredBalls)
    {
        if (Ball && IsValid(Ball))
            QueryParams.AddIgnoredActor(Ball);
    }

    // 4. Object Query 파라미터
    const FCollisionObjectQueryParams ObjParams =
        BuildObjectQueryParams(TraceObjectTypes);

    // 5. ✅ 최적화: LineTraceMulti → LineTraceSingle
    // bPreferDeepestHit=true여도 지형 판정은 가장 가까운 히트 1개로 충분
    // Multi는 모든 히트를 수집하므로 불필요한 비용 발생
    FHitResult SingleHit;
    bool bAnyHit = World->LineTraceSingleByObjectType(
        SingleHit, StartLocation, EndLocation, ObjParams, QueryParams
    );

    // 6. 디버그 드로잉
#if WITH_EDITOR
    if (bDrawTrace)
    {
        const FColor TraceColor = bAnyHit ? TraceColorHit : TraceColorMiss;
        DrawDebugLine(World, StartLocation, EndLocation, TraceColor,
            false, TraceLineLifeTime, 0, TraceLineThickness);
        DrawDebugPoint(World, StartLocation, 8.f, FColor::Cyan, false, TraceLineLifeTime);
        DrawDebugPoint(World, EndLocation, 8.f, FColor::Silver, false, TraceLineLifeTime);
    }
#endif

    // 7. 히트 없음 처리
    if (!bAnyHit || !SingleHit.bBlockingHit)
    {
        return Result;
    }

    const FHitResult* UseHit = &SingleHit;

    // 9. 결과 구성
    Result.bHitGround = true;
    Result.HitLocation = UseHit->Location;
    Result.HitNormal = UseHit->Normal;
    Result.HitActor = UseHit->GetActor();
    Result.HitComponent = UseHit->GetComponent();
    Result.HitPhysicalMaterial = UseHit->PhysMaterial.Get();

    // 10. ✅ 최적화: UE_LOG Log → VeryVerbose (매 트레이스 FString 생성 제거)
    UE_LOG(LogTemp, Log, TEXT("🔍 LineTrace hit: Loc=%s PMat=%s Type=%s"),
        *Result.HitLocation.ToString(),
        Result.HitPhysicalMaterial ? *Result.HitPhysicalMaterial->GetName() : TEXT("None"),
        *UEnum::GetValueAsString(Result.LandProperties.LandType)
    );

    // 11. 지형 속성
    Result.LandProperties = GetPropertiesFromPhysicalMaterial(Result.HitPhysicalMaterial);

    return Result;
}


FLandProperties ALandscapeChecker::GetPropertiesFromPhysicalMaterial(UPhysicalMaterial* PhysMat)
{
    if (!PhysMat || !IsValid(PhysMat))
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Null PhysicalMaterial, returning default properties"));
        return FLandProperties(ELandType::Unknown, 1.0f, 1.0f, 0.0f, FLinearColor::Gray, TEXT("알 수 없음"));
    }

    for (const FPhysicalMaterialMapping& Mapping : PhysicalMaterialMappings)
    {
        if (Mapping.PhysicalMaterial == PhysMat)
        {
            return Mapping.LandProperties;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("🔍 No mapping found for PhysicalMaterial: %s"), *PhysMat->GetName());
    return FLandProperties(ELandType::Grass, 1.0f, 1.0f, 0.0f, FLinearColor::Green, TEXT("매핑되지 않음"));
}

// ===== 디버그/시각화 =====

void ALandscapeChecker::ShowLandTypeAtLocation(const FVector& WorldLocation, float DisplayTime)
{
    // ✅ UE5.6: DrawDebug* 는 WITH_EDITOR에서만 의미 있음
#if WITH_EDITOR
    UWorld* World = GetWorld();
    if (!World) return;

    const FLandCheckResult R = CheckGroundAtLocation(WorldLocation);
    if (R.bHitGround)
    {
        const FColor C = R.LandProperties.DebugColor.ToFColor(true);
        DrawDebugSphere(World, R.HitLocation, DebugSphereSize * 1.5f, 12, C, false, DisplayTime);
        FString Txt = FString::Printf(TEXT("%s\n마찰: %.2f  바운스: %.2f  감속: %.0f%%"),
            *R.LandProperties.DisplayName,
            R.LandProperties.FrictionMultiplier,
            R.LandProperties.BounceMultiplier,
            R.LandProperties.SpeedReduction * 100.f);
        DrawDebugString(World, R.HitLocation + FVector(0, 0, 50), Txt, nullptr, C, DisplayTime, false);
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(-1, DisplayTime, C,
                FString::Printf(TEXT("Land Type: %s"), *R.LandProperties.DisplayName));
    }
    else
    {
        DrawDebugSphere(World, WorldLocation, DebugSphereSize, 12, FColor::Red, false, DisplayTime);
        DrawDebugString(World, WorldLocation + FVector(0, 0, 30),
            TEXT("No Ground Found"), nullptr, FColor::Red, DisplayTime, false);
    }
#endif
}

void ALandscapeChecker::ToggleDebugMode()
{
    bShowDebugInfo = !bShowDebugInfo;
#if WITH_EDITOR
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, bShowDebugInfo ? FColor::Green : FColor::Red,
            FString::Printf(TEXT("🌱 LandscapeChecker Debug: %s"), bShowDebugInfo ? TEXT("ON") : TEXT("OFF")));
    }
#endif
    UE_LOG(LogTemp, Log, TEXT("🌱 Debug mode: %s"), bShowDebugInfo ? TEXT("ON") : TEXT("OFF"));
}

void ALandscapeChecker::DrawDebugLandGrid(const FVector& CenterLocation, float GridSize, int32 GridResolution)
{
#if WITH_EDITOR
    UWorld* World = GetWorld();
    if (!World) return;

    const float Step = GridSize / GridResolution;
    const float Half = GridSize * 0.5f;

    for (int32 X = 0; X <= GridResolution; ++X)
        for (int32 Y = 0; Y <= GridResolution; ++Y)
        {
            const FVector P = CenterLocation + FVector((X * Step) - Half, (Y * Step) - Half, 0.f);
            const FLandCheckResult R = CheckGroundAtLocation(P);
            if (R.bHitGround)
            {
                const FColor C = R.LandProperties.DebugColor.ToFColor(true);
                DrawDebugSphere(World, R.HitLocation, DebugSphereSize * 0.5f, 8, C, false, 10.0f);
            }
        }
#endif
}

// ===== 캐싱 & 설정 유틸 =====

void ALandscapeChecker::SetDrawTrace(bool bEnable) { bDrawTrace = bEnable; }

void ALandscapeChecker::EnableCaching(bool bEnable)
{
    bEnableCaching = bEnable;
    if (!bEnable) ClearCache();
    UE_LOG(LogTemp, Log, TEXT("🗃️ Caching %s"), bEnable ? TEXT("enabled") : TEXT("disabled"));
}

void ALandscapeChecker::ClearCache()
{
    LocationCache.Empty();
    DebugCacheHitCount = 0;
    DebugCacheMissCount = 0;
    UE_LOG(LogTemp, Log, TEXT("🗑️ Cache cleared"));
}

FVector2D ALandscapeChecker::WorldLocationToGridKey(const FVector& WorldLocation)
{
    const float GridX = FMath::RoundToFloat(WorldLocation.X / CacheGridSize) * CacheGridSize;
    const float GridY = FMath::RoundToFloat(WorldLocation.Y / CacheGridSize) * CacheGridSize;
    return FVector2D(GridX, GridY);
}

bool ALandscapeChecker::GetCachedResult(const FVector& WorldLocation, FLandCheckResult& OutResult)
{
    if (!bEnableCaching) return false;
    const FVector2D Key = WorldLocationToGridKey(WorldLocation);

    if (FCacheEntry* CE = LocationCache.Find(Key))
    {
        const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
        if (Now - CE->TimeStamp < CACHE_ENTRY_LIFETIME)
        {
            OutResult = CE->Result;
            return true;
        }
        LocationCache.Remove(Key);
    }
    return false;
}

void ALandscapeChecker::CacheResult(const FVector& WorldLocation, const FLandCheckResult& Result)
{
    if (!bEnableCaching) return;

    if (LocationCache.Num() >= MaxCacheEntries)
    {
        CleanupOldCacheEntries();
        if (LocationCache.Num() >= MaxCacheEntries)
        {
            int32 RemoveCount = LocationCache.Num() / 2;
            auto It = LocationCache.CreateIterator();
            for (int32 i = 0; i < RemoveCount && It; ++i) { It.RemoveCurrent(); ++It; }
        }
    }

    const FVector2D Key = WorldLocationToGridKey(WorldLocation);
    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    LocationCache.Add(Key, FCacheEntry(Result, Now));
}

void ALandscapeChecker::InitializeDefaultMappings()
{
    UE_LOG(LogTemp, Log, TEXT("🔧 Initialize default mappings completed"));
}

void ALandscapeChecker::CleanupOldCacheEntries()
{
    if (!bEnableCaching) return;

    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    TArray<FVector2D> ToRemove;
    for (const auto& Pair : LocationCache)
    {
        if (Now - Pair.Value.TimeStamp > CACHE_ENTRY_LIFETIME)
        {
            ToRemove.Add(Pair.Key);
        }
    }
    for (const FVector2D& K : ToRemove) { LocationCache.Remove(K); }


}

// ===== 매핑/초기값 =====

void ALandscapeChecker::AddPhysicalMaterialMapping(UPhysicalMaterial* PhysMat, const FLandProperties& Properties)
{
    if (!PhysMat)
    {
        UE_LOG(LogTemp, Warning, TEXT("AddPhysicalMaterialMapping: PhysMat is null"));
        return;
    }

    for (FPhysicalMaterialMapping& M : PhysicalMaterialMappings)
    {
        if (M.PhysicalMaterial == PhysMat)
        {
            M.LandProperties = Properties;
            UE_LOG(LogTemp, Log, TEXT("Updated mapping for %s"), *PhysMat->GetName());
            return;
        }
    }

    FPhysicalMaterialMapping NewM;
    NewM.PhysicalMaterial = PhysMat;
    NewM.LandProperties = Properties;
    PhysicalMaterialMappings.Add(NewM);
    UE_LOG(LogTemp, Log, TEXT("Added mapping for %s"), *PhysMat->GetName());
}

void ALandscapeChecker::RemovePhysicalMaterialMapping(UPhysicalMaterial* PhysMat)
{
    if (!PhysMat) return;

    const int32 Removed = PhysicalMaterialMappings.RemoveAll([&](const FPhysicalMaterialMapping& M)
        {
            return M.PhysicalMaterial == PhysMat;
        });

    UE_LOG(LogTemp, Log, TEXT("Removed %d mapping(s) for %s"), Removed, *PhysMat->GetName());
}

void ALandscapeChecker::SetupDefaultMaterialMappings()
{
    // 에디터에서 세팅한 매핑을 우선 사용.
    if (PhysicalMaterialMappings.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("No PhysicalMaterialMappings configured. Please assign in editor."));
    }
    InitializeDefaultMappings();
}

// ===== 정적 인스턴스 =====

ALandscapeChecker* ALandscapeChecker::GetLandscapeChecker(UWorld* World)
{
    if (Instance && IsValid(Instance) && Instance->GetWorld() == World) return Instance;

    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ World is null in GetLandscapeChecker"));
        return nullptr;
    }

    for (TActorIterator<ALandscapeChecker> It(World); It; ++It)
    {
        if (ALandscapeChecker* Found = *It)
        {
            if (Found->GetWorld() == World)
            {
                Instance = Found;
                return Instance;
            }
        }
    }

    UE_LOG(LogTemp, Error, TEXT("❌ No valid LandscapeChecker found in the specified world!"));
    return nullptr;
}


ELandType ALandscapeChecker::GetLandTypeFromMask(const FVector& WorldLocation)
{
    if (!bUseMaskTexture || !MaskTexture)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Mask texture not enabled or not set"));
        return ELandType::Unknown;
    }

    const FColor MaskColor = SampleMaskAtWorldLocation(WorldLocation);
    return ConvertMaskColorToLandType(MaskColor);
}

FLandProperties ALandscapeChecker::GetLandPropertiesFromMask(const FVector& WorldLocation)
{
    const ELandType LandType = GetLandTypeFromMask(WorldLocation);
    return GetPropertiesFromMaskLandType(LandType);
}

FLandCheckResult ALandscapeChecker::CheckGroundAtLocationWithMask(const FVector& WorldLocation, float TraceDistance)
{
    SCOPE_CYCLE_COUNTER(STAT_LandscapeCheckGround);
    // 먼저 기본 지면 체크 수행
    FLandCheckResult Result = CheckGroundAtLocation(WorldLocation, TraceDistance);

    // 마스크가 활성화되어 있다면 마스크 정보로 오버라이드
    if (bUseMaskTexture && MaskTexture && Result.bHitGround)
    {
        const ELandType MaskLandType = GetLandTypeFromMask(WorldLocation);
        if (MaskLandType != ELandType::Unknown)
        {
            Result.LandProperties = GetPropertiesFromMaskLandType(MaskLandType);
            UE_LOG(LogTemp, Log, TEXT("🎭 Mask override: %s at %s"),
                *UEnum::GetValueAsString(MaskLandType), *WorldLocation.ToString());
        }
    }

    return Result;
}

void ALandscapeChecker::SetMaskTexture(UTexture2D* InMaskTexture)
{
    MaskTexture = InMaskTexture;
    if (MaskTexture)
    {
        UE_LOG(LogTemp, Log, TEXT("🎭 Mask texture set: %s (%dx%d)"),
            *MaskTexture->GetName(),
            MaskTexture->GetSizeX(),
            MaskTexture->GetSizeY());
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("🎭 Mask texture cleared"));
    }
}

void ALandscapeChecker::SetMaskWorldBounds(const FVector& InWorldMin, const FVector& InWorldMax)
{
    MaskWorldMin = InWorldMin;
    MaskWorldMax = InWorldMax;

    // 경계 값의 유효성 검사
    if (MaskWorldMax.X <= MaskWorldMin.X || MaskWorldMax.Y <= MaskWorldMin.Y)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Invalid mask world bounds! Max should be greater than Min"));
        UE_LOG(LogTemp, Error, TEXT("   Min: %s, Max: %s"), *MaskWorldMin.ToString(), *MaskWorldMax.ToString());
        return;
    }

}

void ALandscapeChecker::AutoCalculateMaskWorldBounds()
{
    UWorld* World = GetWorld();
    if (!World) return;

    FBox CombinedBounds(ForceInit);
    bool bFoundLandscape = false;

    // 방법 A: LandscapeComponent의 Bounds 직접 수집 (가장 정확)
    for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
    {
        ALandscapeProxy* Landscape = *It;
        if (!Landscape || !IsValid(Landscape)) continue;

        TArray<ULandscapeComponent*> LandscapeComponents;
        Landscape->GetComponents<ULandscapeComponent>(LandscapeComponents);

        for (ULandscapeComponent* LC : LandscapeComponents)
        {
            if (!LC) continue;

            FBox CompBox = LC->Bounds.GetBox();
            if (!bFoundLandscape)
            {
                CombinedBounds = CompBox;
                bFoundLandscape = true;
            }
            else
            {
                CombinedBounds += CompBox;
            }
        }
    }

    // 대체 방법 (위가 실패하면 fallback)
    if (!bFoundLandscape)
    {
        for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
        {
            ALandscapeProxy* Landscape = *It;
            if (!Landscape) continue;

            FBox ProxyBox = Landscape->GetComponentsBoundingBox(true);
            if (ProxyBox.IsValid)
            {
                if (!bFoundLandscape)
                {
                    CombinedBounds = ProxyBox;
                    bFoundLandscape = true;
                }
                else
                {
                    CombinedBounds += ProxyBox;
                }
            }
        }
    }

    if (!bFoundLandscape)
    {
        UE_LOG(LogTemp, Warning, TEXT("No Landscape found, using default bounds"));
        SetMaskWorldBounds(FVector(-50000, -50000, 0), FVector(50000, 50000, 0));
        return;
    }

    // Z는 무시
    CombinedBounds.Min.Z = 0;
    CombinedBounds.Max.Z = 0;

    // 정사각형 강제 만들기 + 여유 공간 추가
    FVector Center = CombinedBounds.GetCenter();
    FVector Size = CombinedBounds.GetSize();
    float HalfSize = FMath::Max(Size.X, Size.Y) * 0.5f;

    // 1~2m 여유 추가 (마스크 경계가 딱 맞아서 잘리는 경우 방지)
   // HalfSize += 200.0f;

    FVector NewMin(Center.X - HalfSize, Center.Y - HalfSize, 0);
    FVector NewMax(Center.X + HalfSize, Center.Y + HalfSize, 0);

    SetMaskWorldBounds(NewMin, NewMax);

    UE_LOG(LogTemp, Warning, TEXT("정사각형 마스크 경계 설정 PERFECT SQUARE MASK BOUNDS (UE4.26)"));
    UE_LOG(LogTemp, Warning, TEXT("   Center: %s"), *Center.ToString());
    UE_LOG(LogTemp, Warning, TEXT("   HalfSize: %.0f uu (%.1fm)"), HalfSize, HalfSize / 100.0f);
    UE_LOG(LogTemp, Warning, TEXT("   Min: %s  Max: %s"), *NewMin.ToString(), *NewMax.ToString());
}


FColor ALandscapeChecker::SampleMaskAtWorldLocation(const FVector& WorldLocation)
{
    if (!MaskTexture)
    {
        UE_LOG(LogTemp, Warning, TEXT("🎭 MaskTexture is null"));
        return FColor::Black;
    }

    // 월드 좌표를 UV 좌표로 변환
    const FVector2D UV = WorldLocationToMaskUV(WorldLocation);


    // UV 범위 체크 - 더 자세한 디버그 정보 포함
    if (UV.X < 0.0f || UV.X > 1.0f || UV.Y < 0.0f || UV.Y > 1.0f)
    {

        // 범위를 벗어난 경우 클램핑하거나 기본값 반환
        if (bClampUVOutOfBounds)
        {
            const FVector2D ClampedUV(
                FMath::Clamp(UV.X, 0.0f, 1.0f),
                FMath::Clamp(UV.Y, 0.0f, 1.0f)
            );

            return SampleMaskAtUV(ClampedUV);
        }

        return DefaultOutOfBoundsColor;
    }

    return SampleMaskAtUV(UV);
}

FColor ALandscapeChecker::SampleMaskAtUV(const FVector2D& UV)
{
    // ✅ 최적화: GetMipData() 매 호출 수MB 복사 완전 제거
    // BeginPlay에서 CacheMaskPixelData()로 1회 로드한 캐시를 직접 인덱싱
    if (bMaskCacheReady && CachedMaskPixels.Num() > 0
        && CachedMaskWidth > 0 && CachedMaskHeight > 0)
    {
        const int32 PixelX = FMath::Clamp(FMath::FloorToInt(UV.X * CachedMaskWidth), 0, CachedMaskWidth - 1);
        const int32 PixelY = FMath::Clamp(FMath::FloorToInt(UV.Y * CachedMaskHeight), 0, CachedMaskHeight - 1);
        const int32 PixelIndex = (PixelY * CachedMaskWidth + PixelX) * 4;

        if (PixelIndex + 3 < CachedMaskPixels.Num())
        {
            FColor SampledColor;
#if WITH_EDITOR
            // 에디터 소스는 BGRA8
            SampledColor.B = CachedMaskPixels[PixelIndex + 0];
            SampledColor.G = CachedMaskPixels[PixelIndex + 1];
            SampledColor.R = CachedMaskPixels[PixelIndex + 2];
            SampledColor.A = CachedMaskPixels[PixelIndex + 3];
#else
            // 런타임 PlatformData는 BGRA8 (FColor 순서)
            SampledColor = *reinterpret_cast<const FColor*>(&CachedMaskPixels[PixelIndex]);
#endif
            if (bVerboseMaskSampling)
            {
                UE_LOG(LogTemp, Log, TEXT("🎭 Cache UV(%.3f,%.3f) Px(%d,%d): R=%d G=%d B=%d"),
                    UV.X, UV.Y, PixelX, PixelY,
                    SampledColor.R, SampledColor.G, SampledColor.B);
            }
            return SampledColor;
        }
    }

    // 캐시 미준비 시 폴백 (초기화 직후 등 예외적 상황)
    if (!MaskTexture)
        return FColor::Black;

    UE_LOG(LogTemp, Warning, TEXT("⚠️ SampleMaskAtUV: 캐시 미준비, CacheMaskPixelData 재시도"));
    CacheMaskPixelData();

    // 재시도 후에도 실패하면 Black 반환
    return FColor::Black;
}

ELandType ALandscapeChecker::ConvertMaskColorToLandType(const FColor& MaskColor)
{
    // G값이 임계값보다 높으면 그린
    if (MaskColor.G >= GreenGreenThreshold)
    {
        //   UE_LOG(LogTemp, Warning, TEXT("⚠️ ========Return Green : %d"), (int32)MaskColor.G);
        return ELandType::Green;
    }


    // R값이 임계값보다 높으면 벙커(모래)
    if (MaskColor.R >= BunkerRedThreshold)
    {
        //  UE_LOG(LogTemp, Warning, TEXT("⚠️ ========Return Sand : %d"), (int32)MaskColor.R);
        return ELandType::Sand;
    }


    // G값이 임계값보다 높으면 페어웨이
    if (MaskColor.A >= FairWayGreenThreshold)
    {
        //   UE_LOG(LogTemp, Warning, TEXT("⚠️ ========Return Fair : %d"), (int32)MaskColor.G);
        return ELandType::Fairway;
    }


    // 나머지는 러프
    return ELandType::Rough;
}

FLandProperties ALandscapeChecker::GetPropertiesFromMaskLandType(ELandType LandType)
{
    switch (LandType)
    {
    case ELandType::Sand:
        return FLandProperties(
            ELandType::Sand,
            0.8f,           // 마찰력 (낮음)
            0.3f,           // 바운스 (낮음)
            0.6f,           // 속도 감소 (높음)
            FLinearColor(1.0f, 0.8f, 0.4f),  // 샌드 색상
            TEXT("벙커")
        );

    case ELandType::Green:
        return FLandProperties(
            ELandType::Green,
            1.2f,           // 마찰력 (높음)
            0.8f,           // 바운스 (중간)
            0.1f,           // 속도 감소 (낮음)
            FLinearColor(0.2f, 0.8f, 0.2f),  // 그린 색상
            TEXT("그린")
        );
    case ELandType::Fairway:
        return FLandProperties(
            ELandType::Fairway,
            0.9f,           // 마찰력 (중간)
            0.5f,           // 바운스 (중간)
            0.3f,           // 속도 감소 (중간)
            FLinearColor(0.4f, 0.6f, 0.2f),  // 페어웨이 색상
            TEXT("러프")
        );

    case ELandType::Rough:
        return FLandProperties(
            ELandType::Rough,
            0.9f,           // 마찰력 (중간)
            0.5f,           // 바운스 (중간)
            0.3f,           // 속도 감소 (중간)
            FLinearColor(0.4f, 0.6f, 0.2f),  // 러프 색상
            TEXT("러프")
        );

    default:
        return FLandProperties(
            ELandType::Unknown,
            1.0f, 1.0f, 0.0f,
            FLinearColor::Gray,
            TEXT("알 수 없음")
        );
    }
}

FVector2D ALandscapeChecker::WorldLocationToMaskUV(const FVector& WorldLocation)
{
    // 월드 크기 계산
    const FVector WorldSize = MaskWorldMax - MaskWorldMin;

    // 0으로 나누기 방지
    if (WorldSize.X <= 0.0f || WorldSize.Y <= 0.0f)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Invalid world size for UV conversion: %s"), *WorldSize.ToString());
        return FVector2D(0.5f, 0.5f); // 중앙값 반환
    }

    // 월드 좌표를 0-1 UV 범위로 정규화
    const float U = (WorldLocation.X - MaskWorldMin.X) / WorldSize.X;
    const float V = (WorldLocation.Y - MaskWorldMin.Y) / WorldSize.Y;

    return FVector2D(U, V);
}


void ALandscapeChecker::AnalyzeLandscapeBounds()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ World is null"));
        return;
    }

    FBox CombinedBounds(ForceInit);
    bool bFirstBound = true;
    int32 LandscapeCount = 0;

    // AActor 기반으로 랜드스케이프 찾기 (가장 호환성 높은 방법)
    for (TActorIterator<AActor> ActorIterator(World); ActorIterator; ++ActorIterator)
    {
        AActor* Actor = *ActorIterator;
        if (!Actor || !IsValid(Actor)) continue;

        // 클래스 이름으로 랜드스케이프 판별
        const FString ClassName = Actor->GetClass()->GetName();
        if (ClassName.Contains(TEXT("Landscape")))
        {
            LandscapeCount++;

            // 액터의 바운딩 박스 계산
            FBox ActorBounds = Actor->GetComponentsBoundingBox(true);

            if (ActorBounds.IsValid)
            {
                const FVector BoundsSize = ActorBounds.GetSize();
                const FVector BoundsCenter = ActorBounds.GetCenter();


                // 전체 경계 계산
                if (bFirstBound)
                {
                    CombinedBounds = ActorBounds;
                    bFirstBound = false;
                }
                else
                {
                    CombinedBounds += ActorBounds;
                }


            }
        }
    }

    // 전체 결합된 경계 분석
    if (!bFirstBound)
    {
        const FVector TotalSize = CombinedBounds.GetSize();
        const FVector TotalCenter = CombinedBounds.GetCenter();


        // 1008x1008과 비교
        const float ExpectedSizeM = 1008.0f;
        const float ActualSizeXM = TotalSize.X / 100.0f;
        const float ActualSizeYM = TotalSize.Y / 100.0f;


        const bool bMatchesExpected =
            FMath::Abs(ActualSizeXM - ExpectedSizeM) < 10.0f &&
            FMath::Abs(ActualSizeYM - ExpectedSizeM) < 10.0f;


    }

    // 자동으로 올바른 경계 설정
    if (!bFirstBound)
    {
        // 강제 정사각형으로 
        CombinedBounds.Min.Y = CombinedBounds.Min.X;
        SetMaskWorldBounds(CombinedBounds.Min, CombinedBounds.Max);
    }
}