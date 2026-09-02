// Copyright (c) 2026 365ParkGolf. All Rights Reserved.

#include "LandscapeChecker.h"
#include "GolfBall.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeComponent.h"

// 정적 인스턴스
ALandscapeChecker* ALandscapeChecker::Instance = nullptr;


// LandscapeChecker.cpp 파일 상단에 추가 (또는 다른 헬퍼 파일)
static FString EnumToString(EPixelFormat Format)
{
    switch (Format)
    {
    case PF_Unknown:         return TEXT("PF_Unknown");
    case PF_A32B32G32R32F:   return TEXT("PF_A32B32G32R32F");
    case PF_B8G8R8A8:        return TEXT("PF_B8G8R8A8 (표준)");
    case PF_G8:              return TEXT("PF_G8 (그레이스케일!)");
    case PF_G16:             return TEXT("PF_G16");
    case PF_DXT1:            return TEXT("PF_DXT1 (압축!)");
    case PF_DXT3:            return TEXT("PF_DXT3 (압축!)");
    case PF_DXT5:            return TEXT("PF_DXT5 (압축!)");
    case PF_BC5:             return TEXT("PF_BC5 (압축!)");
    case PF_R8G8B8A8:        return TEXT("PF_R8G8B8A8");
    default:                 return FString::Printf(TEXT("PF_%d"), (int32)Format);
    }
}

// ═══════════════════════════════════════════════════════════
// 생성자 / 라이프사이클
// ═══════════════════════════════════════════════════════════
ALandscapeChecker::ALandscapeChecker()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.1f;

    TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
    TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));

    // ★★ 하드코딩 경로에서 마스크 자동 로드 (CDO)
    static ConstructorHelpers::FObjectFinder<UTexture2D> DefaultMaskFinder(
        TEXT("/Game/Landscape_Material/mask.mask"));

    if (DefaultMaskFinder.Succeeded())
    {
        MaskTexture = DefaultMaskFinder.Object;
    }

    MaskTexturePath = TEXT("/Game/Landscape_Material/mask.mask");
    bForceUseHardcodedMask = true;
}

void ALandscapeChecker::BeginPlay()
{
    Super::BeginPlay();

    Instance = this;
    InitializeDefaultMappings();

    // ★★ 하드코딩 강제 사용 or MaskTexture 없으면 재로드
    if (bForceUseHardcodedMask || !IsValid(MaskTexture))
    {
        LoadDefaultMaskTexture();
    }

    // ★★ 마스크 사용 강제 활성화 (Landscape 유무 무관)
    bUseMaskTexture = true;

    CacheMaskPixelData();
    RebuildIgnoredBallCache();

    // AnalyzeLandscapeBounds는 유지 (Bounds 자동 계산에 필요)
    AnalyzeLandscapeBounds();

    // ★★ Landscape 없어도 마스크는 강제 사용
    bUseMaskTexture = true;

    UE_LOG(LogTemp, Warning, TEXT("🌍 LandscapeChecker BeginPlay 완료:"));
    UE_LOG(LogTemp, Warning, TEXT("  ├─ MaskTexture: %s"),
        IsValid(MaskTexture) ? *MaskTexture->GetName() : TEXT("nullptr!"));
    UE_LOG(LogTemp, Warning, TEXT("  ├─ bUseMaskTexture: %d"), (int32)bUseMaskTexture);
    UE_LOG(LogTemp, Warning, TEXT("  ├─ bMaskCacheReady: %d"), (int32)bMaskCacheReady);
    UE_LOG(LogTemp, Warning, TEXT("  ├─ MaskWorldMin: %s"), *MaskWorldMin.ToString());
    UE_LOG(LogTemp, Warning, TEXT("  └─ MaskWorldMax: %s"), *MaskWorldMax.ToString());
}

void ALandscapeChecker::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 캐시 주기적 정리
    CacheCleanupTimer += DeltaTime;
    if (CacheCleanupTimer >= CACHE_CLEANUP_INTERVAL)
    {
        CleanupOldCacheEntries();
        CacheCleanupTimer = 0.0f;
    }
}

void ALandscapeChecker::BeginDestroy()
{
    if (Instance == this)
    {
        Instance = nullptr;
    }

    CachedMaskPixels.Empty();
    CachedIgnoredBalls.Empty();
    LocationCache.Empty();

    Super::BeginDestroy();
}

// ═══════════════════════════════════════════════════════════
// ★ 통합 API — 진입점
// ═══════════════════════════════════════════════════════════

ELandType ALandscapeChecker::ResolveLandTypeAt(const FVector& WorldLocation)
{
    // ─── 우선순위 1: 마스크 텍스처 ───
    if (bUseMaskTexture)
    {
        ELandType MaskResult;
        if (TryGetLandTypeFromMask(WorldLocation, MaskResult))
        {
            if (bVerboseMaskSampling)
            {
                UE_LOG(LogTemp, Log, TEXT("✅ [Resolve] Mask → %s"),
                    *UEnum::GetValueAsString(MaskResult));
            }
            return MaskResult;
        }
    }

    // ─── 우선순위 2: PhysMat 트레이스 ───
    ELandType PhysMatResult;
    if (TryGetLandTypeFromPhysMat(WorldLocation, PhysMatResult))
    {
        if (bVerboseMaskSampling)
        {
            UE_LOG(LogTemp, Log, TEXT("✅ [Resolve] PhysMat → %s"),
                *UEnum::GetValueAsString(PhysMatResult));
        }
        return PhysMatResult;
    }

    // ─── 폴백: Rough ───
    if (bVerboseMaskSampling)
    {
        UE_LOG(LogTemp, Log, TEXT("✅ [Resolve] Fallback → Rough"));
    }
    return ELandType::Rough;
}

FLandProperties ALandscapeChecker::ResolveLandPropertiesAt(const FVector& WorldLocation)
{
    return GetPropertiesFromMaskLandType(ResolveLandTypeAt(WorldLocation));
}

// ═══════════════════════════════════════════════════════════
// ★ 통합 API 내부 구현
// ═══════════════════════════════════════════════════════════

bool ALandscapeChecker::TryGetLandTypeFromMask(const FVector& WorldLocation, ELandType& OutLandType)
{
    // ① 마스크 사용 여부 및 텍스처 유효성
    if (!bUseMaskTexture || !IsValid(MaskTexture))
    {
        return false;
    }

    // ② 마스크 픽셀 캐시 준비 여부
    if (!bMaskCacheReady)
    {
        // 지연 초기화 재시도 (BeginPlay에서 실패했을 수 있음)
        CacheMaskPixelData();
        if (!bMaskCacheReady)
        {
            return false;
        }
    }

    // ③ UV 변환 및 범위 체크
    const FVector2D UV = WorldLocationToMaskUV(WorldLocation);
    if (!IsMaskUVInBounds(UV))
    {
        return false;  // 마스크 커버리지 밖 → PhysMat 폴백으로 넘김
    }

    // ④ 색상 판정
    const FColor MaskColor = SampleMaskAtUV(UV);
    const ELandType Detected = ConvertMaskColorToLandType(MaskColor);

    // Unknown이면 실패로 취급 → PhysMat 폴백에 기회
    if (Detected == ELandType::Unknown)
    {
        return false;
    }

    OutLandType = Detected;
    return true;
}

bool ALandscapeChecker::TryGetLandTypeFromPhysMat(const FVector& WorldLocation, ELandType& OutLandType)
{
    UWorld* World = GetWorld();
    if (!World) return false;

    // 볼 위치 기준 수직 트레이스 (위→아래)
    const FVector Start = WorldLocation + FVector(0, 0, 100.0f);
    const FVector End = WorldLocation - FVector(0, 0, 200.0f);

    FCollisionQueryParams Params(SCENE_QUERY_STAT(LandChecker_PhysMatTrace),
        /*bTraceComplex=*/true);
    Params.bReturnPhysicalMaterial = true;
    Params.AddIgnoredActor(this);

    // 등록된 볼들 무시 (트레이스가 볼 자체에 걸리는 것 방지)
    for (AGolfBall* Ball : CachedIgnoredBalls)
    {
        if (IsValid(Ball))
        {
            Params.AddIgnoredActor(Ball);
        }
    }

    FHitResult Hit;
    if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
    {
        return false;
    }

    // PhysMat 획득
    UPhysicalMaterial* PM = Hit.PhysMaterial.IsValid() ? Hit.PhysMaterial.Get() : nullptr;
    if (!PM)
    {
        return false;
    }

    // 엔진 기본 PhysMat이면 판정 불가로 간주
    if (GEngine && PM == GEngine->DefaultPhysMaterial)
    {
        return false;
    }

    // PhysMat 이름 → ELandType 매핑
    const ELandType Detected = PhysMatNameToLandType(PM->GetName());
    if (Detected == ELandType::Unknown)
    {
        return false;
    }

    OutLandType = Detected;
    return true;
}

ELandType ALandscapeChecker::PhysMatNameToLandType(const FString& PhysMatName) const
{
    // 우선순위 매칭 (더 구체적인 것부터 검사)

    if (PhysMatName.Contains(TEXT("Bunker"), ESearchCase::IgnoreCase) ||
        PhysMatName.Contains(TEXT("Sand"), ESearchCase::IgnoreCase))
    {
        return ELandType::Sand;
    }

    if (PhysMatName.Contains(TEXT("Fairway"), ESearchCase::IgnoreCase) ||
        PhysMatName.Contains(TEXT("FairWay"), ESearchCase::IgnoreCase))
    {
        return ELandType::Fairway;
    }

    if (PhysMatName.Contains(TEXT("Green"), ESearchCase::IgnoreCase))
    {
        return ELandType::Green;
    }

    if (PhysMatName.Contains(TEXT("Water"), ESearchCase::IgnoreCase) ||
        PhysMatName.Contains(TEXT("Pond"), ESearchCase::IgnoreCase))
    {
        return ELandType::Water;
    }

    if (PhysMatName.Contains(TEXT("Bark"), ESearchCase::IgnoreCase) ||
        PhysMatName.Contains(TEXT("Tree"), ESearchCase::IgnoreCase) ||
        PhysMatName.Contains(TEXT("Wood"), ESearchCase::IgnoreCase))
    {
        return ELandType::Tree;
    }

    if (PhysMatName.Contains(TEXT("Leaves"), ESearchCase::IgnoreCase) ||
        PhysMatName.Contains(TEXT("Leaf"), ESearchCase::IgnoreCase))
    {
        return ELandType::Leaves;
    }

    if (PhysMatName.Contains(TEXT("Net"), ESearchCase::IgnoreCase))
    {
        return ELandType::Net;
    }

    if (PhysMatName.Contains(TEXT("Rock"), ESearchCase::IgnoreCase) ||
        PhysMatName.Contains(TEXT("Stone"), ESearchCase::IgnoreCase))
    {
        return ELandType::Rock;
    }

    if (PhysMatName.Contains(TEXT("Concrete"), ESearchCase::IgnoreCase) ||
        PhysMatName.Contains(TEXT("Road"), ESearchCase::IgnoreCase) ||
        PhysMatName.Contains(TEXT("Path"), ESearchCase::IgnoreCase))
    {
        return ELandType::Concrete;
    }

    if (PhysMatName.Contains(TEXT("Mud"), ESearchCase::IgnoreCase))
    {
        return ELandType::Mud;
    }

    if (PhysMatName.Contains(TEXT("Rough"), ESearchCase::IgnoreCase) ||
        PhysMatName.Contains(TEXT("Grass"), ESearchCase::IgnoreCase))
    {
        return ELandType::Rough;
    }

    return ELandType::Unknown;
}

bool ALandscapeChecker::IsMaskUVInBounds(const FVector2D& UV) const
{
    // Clamp 옵션 켜져 있으면 항상 유효 (기존 동작 유지)
    if (bClampUVOutOfBounds)
    {
        return true;
    }

    // 0.0 ~ 1.0 정규화 범위 체크
    return UV.X >= 0.0f && UV.X <= 1.0f &&
        UV.Y >= 0.0f && UV.Y <= 1.0f;
}

// ═══════════════════════════════════════════════════════════
// 기존 API — 통합 API로 delegate (호환성 유지)
// ═══════════════════════════════════════════════════════════

FLandCheckResult ALandscapeChecker::CheckGroundAtLocation(const FVector& WorldLocation, float TraceDistance)
{
    FLandCheckResult Result;

    // 통합 API로 지형 판정
    const ELandType Type = ResolveLandTypeAt(WorldLocation);
    Result.bHitGround = (Type != ELandType::Unknown);
    Result.HitLocation = WorldLocation;
    Result.HitNormal = FVector::UpVector;
    Result.LandProperties = GetPropertiesFromMaskLandType(Type);

    return Result;
}

ELandType ALandscapeChecker::GetLandTypeAtLocation(const FVector& WorldLocation, float TraceDistance)
{
    return ResolveLandTypeAt(WorldLocation);
}

FLandProperties ALandscapeChecker::GetLandPropertiesAtLocation(const FVector& WorldLocation, float TraceDistance)
{
    return ResolveLandPropertiesAt(WorldLocation);
}

ELandType ALandscapeChecker::GetLandTypeFromMask(const FVector& WorldLocation)
{
    ELandType Result;
    if (TryGetLandTypeFromMask(WorldLocation, Result))
    {
        return Result;
    }
    return ELandType::Unknown;
}

FLandProperties ALandscapeChecker::GetLandPropertiesFromMask(const FVector& WorldLocation)
{
    return GetPropertiesFromMaskLandType(GetLandTypeFromMask(WorldLocation));
}

FLandCheckResult ALandscapeChecker::CheckGroundAtLocationWithMask(const FVector& WorldLocation, float TraceDistance)
{
    return CheckGroundAtLocation(WorldLocation, TraceDistance);
}

// ═══════════════════════════════════════════════════════════
// 고급 체크
// ═══════════════════════════════════════════════════════════

TArray<FLandCheckResult> ALandscapeChecker::CheckGroundInRadius(const FVector& CenterLocation, float Radius, int32 SampleCount)
{
    TArray<FLandCheckResult> Results;
    if (SampleCount <= 0) return Results;

    // 방사형 샘플링
    const float AngleStep = 2.0f * PI / SampleCount;
    for (int32 i = 0; i < SampleCount; ++i)
    {
        const float Angle = i * AngleStep;
        const FVector Offset(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f);
        Results.Add(CheckGroundAtLocation(CenterLocation + Offset));
    }

    return Results;
}

bool ALandscapeChecker::IsLocationOnWater(const FVector& WorldLocation)
{
    return ResolveLandTypeAt(WorldLocation) == ELandType::Water;
}

bool ALandscapeChecker::IsLocationOnSand(const FVector& WorldLocation)
{
    return ResolveLandTypeAt(WorldLocation) == ELandType::Sand;
}

// ═══════════════════════════════════════════════════════════
// RGB → ELandType 변환 (Unknown 대신 Rough 폴백)
// ═══════════════════════════════════════════════════════════

ELandType ALandscapeChecker::ConvertMaskColorToLandType(const FColor& MaskColor)
{
    const uint8 R = MaskColor.R;
    const uint8 G = MaskColor.G;
    const uint8 B = MaskColor.B;
    const uint8 A = MaskColor.A;

    // ★★ 진단 로그 (임시)
    UE_LOG(LogTemp, Warning,
        TEXT("🎨 MaskColor R=%d G=%d A=%d | Bunker(R>%d) Green(G>%d) Fairway(A>%d)"),
        R, G, A, BunkerRedThreshold,   GreenGreenThreshold, FairWayGreenThreshold);

    // ─── 우선순위 1: Bunker (강한 빨강) ───
    if (R > BunkerRedThreshold )
    {
        return ELandType::Sand;
    }

    // ─── 우선순위 2: Water (강한 파랑) ───
    //if (B > 128 && R < 128 && G < 128)
    //{
    //    return ELandType::Water;
    //}

    // ─── 우선순위 3: Green (진녹, R 낮고 G 매우 높음) ───
    if (G > GreenGreenThreshold )
    {
        return ELandType::Green;
    }

    // ─── 우선순위 4: Fairway (연녹, R 중간) ───
    if (A > FairWayGreenThreshold )
    {
        return ELandType::Fairway;
    }

    // ─── 폴백: Rough ───
    // 회색톤, 어두운 톤, 기타 모든 색상은 Rough로 처리
    return ELandType::Rough;
}

// ═══════════════════════════════════════════════════════════
// 지형 특성 매핑
// ═══════════════════════════════════════════════════════════

FLandProperties ALandscapeChecker::GetPropertiesFromMaskLandType(ELandType LandType)
{
    switch (LandType)
    {
    case ELandType::Green:
        return FLandProperties(ELandType::Green, 0.20f, 1.20f, 0.02f,
            FLinearColor(0.0f, 1.0f, 0.0f), TEXT("Green"));

    case ELandType::Fairway:
        return FLandProperties(ELandType::Fairway, 0.35f, 1.00f, 0.05f,
            FLinearColor(0.3f, 0.9f, 0.3f), TEXT("Fairway"));

    case ELandType::Rough:
        return FLandProperties(ELandType::Rough, 0.65f, 0.70f, 0.15f,
            FLinearColor(0.4f, 0.6f, 0.2f), TEXT("Rough"));

    case ELandType::Sand:
        return FLandProperties(ELandType::Sand, 1.50f, 0.30f, 0.40f,
            FLinearColor(1.0f, 0.9f, 0.5f), TEXT("Sand/Bunker"));

    case ELandType::Water:
        return FLandProperties(ELandType::Water, 2.00f, 0.10f, 0.90f,
            FLinearColor(0.2f, 0.5f, 1.0f), TEXT("Water"));

    case ELandType::Tree:
        return FLandProperties(ELandType::Tree, 1.00f, 0.30f, 0.30f,
            FLinearColor(0.5f, 0.3f, 0.1f), TEXT("Tree/Bark"));

    case ELandType::Leaves:
        return FLandProperties(ELandType::Leaves, 0.80f, 0.50f, 0.20f,
            FLinearColor(0.3f, 0.7f, 0.2f), TEXT("Leaves"));

    case ELandType::Net:
        return FLandProperties(ELandType::Net, 1.20f, 0.20f, 0.35f,
            FLinearColor(0.7f, 0.7f, 0.7f), TEXT("Net"));

    case ELandType::Rock:
        return FLandProperties(ELandType::Rock, 0.50f, 0.90f, 0.05f,
            FLinearColor(0.5f, 0.5f, 0.5f), TEXT("Rock"));

    case ELandType::Concrete:
        return FLandProperties(ELandType::Concrete, 0.40f, 0.95f, 0.05f,
            FLinearColor(0.7f, 0.7f, 0.7f), TEXT("Concrete/Road"));

    case ELandType::Mud:
        return FLandProperties(ELandType::Mud, 1.30f, 0.20f, 0.50f,
            FLinearColor(0.4f, 0.3f, 0.2f), TEXT("Mud"));

    case ELandType::TeeBox:
        return FLandProperties(ELandType::TeeBox, 0.30f, 1.00f, 0.05f,
            FLinearColor(0.6f, 0.8f, 0.6f), TEXT("Tee Box"));

    case ELandType::Grass:
        return FLandProperties(ELandType::Grass, 0.50f, 0.80f, 0.10f,
            FLinearColor(0.3f, 0.7f, 0.3f), TEXT("Grass"));

    case ELandType::Unknown:
    default:
        return FLandProperties(ELandType::Rough, 0.65f, 0.70f, 0.15f,
            FLinearColor(0.5f, 0.5f, 0.5f), TEXT("Rough (Fallback)"));
    }
}

// ═══════════════════════════════════════════════════════════
// 마스크 픽셀 캐시 (BeginPlay 1회 + 지연 재시도)
// ═══════════════════════════════════════════════════════════

void ALandscapeChecker::CacheMaskPixelData()
{
    bMaskCacheReady = false;
    CachedMaskPixels.Empty();
    CachedMaskWidth = 0;
    CachedMaskHeight = 0;

    if (!IsValid(MaskTexture))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("⚠️ CacheMaskPixelData: MaskTexture 미지정, 로드 재시도"));

        // 자동 재시도
        if (!LoadDefaultMaskTexture())
        {
            UE_LOG(LogTemp, Error, TEXT("❌ 마스크 로드 최종 실패"));
            return;
        }
    }

    FTexture2DMipMap& Mip = MaskTexture->GetPlatformData()->Mips[0];
    const int32 Width = Mip.SizeX;
    const int32 Height = Mip.SizeY;

    if (Width <= 0 || Height <= 0)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("⚠️ CacheMaskPixelData: 잘못된 크기 %dx%d"), Width, Height);
        return;
    }

    // ★★ 텍스처 설정 확인 로그
    UE_LOG(LogTemp, Warning,
        TEXT("📸 마스크 텍스처 정보:"));
    UE_LOG(LogTemp, Warning,
        TEXT("  ├─ 이름: %s"), *MaskTexture->GetName());
    UE_LOG(LogTemp, Warning,
        TEXT("  ├─ 크기: %dx%d"), Width, Height);
    UE_LOG(LogTemp, Warning,
        TEXT("  ├─ 포맷: %d (%s)"),
        (int32)MaskTexture->GetPixelFormat(),
        *EnumToString(MaskTexture->GetPixelFormat()));
    UE_LOG(LogTemp, Warning,
        TEXT("  ├─ sRGB: %d %s"),
        (int32)MaskTexture->SRGB,
        MaskTexture->SRGB ? TEXT("⚠️(감마 왜곡 가능)") : TEXT("✓"));
    UE_LOG(LogTemp, Warning,
        TEXT("  ├─ Compression: %d"),
        (int32)MaskTexture->CompressionSettings);


    const void* BulkData = Mip.BulkData.LockReadOnly();
    if (!BulkData)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("⚠️ CacheMaskPixelData: BulkData Lock 실패"));
        return;
    }

    const int32 NumBytes = Width * Height * 4;  // BGRA8
    CachedMaskPixels.SetNumUninitialized(NumBytes);
    FMemory::Memcpy(CachedMaskPixels.GetData(), BulkData, NumBytes);

    Mip.BulkData.Unlock();

    CachedMaskWidth = Width;
    CachedMaskHeight = Height;
    bMaskCacheReady = true;

    // ★★ 실제 픽셀 샘플 확인 (중심 픽셀 + 좌측 상단 픽셀)
    if (CachedMaskPixels.Num() >= 4)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("🎨 샘플 픽셀 값:"));

        // 첫 픽셀 (0,0)
        UE_LOG(LogTemp, Warning,
            TEXT("  ├─ (0,0): BGRA[%d,%d,%d,%d]"),
            CachedMaskPixels[0], CachedMaskPixels[1],
            CachedMaskPixels[2], CachedMaskPixels[3]);

        // 중심 픽셀
        int32 CenterIdx = (Height / 2 * Width + Width / 2) * 4;
        if (CenterIdx + 3 < CachedMaskPixels.Num())
        {
            UE_LOG(LogTemp, Warning,
                TEXT("  ├─ 중심(%d,%d): BGRA[%d,%d,%d,%d]"),
                Width / 2, Height / 2,
                CachedMaskPixels[CenterIdx + 0], CachedMaskPixels[CenterIdx + 1],
                CachedMaskPixels[CenterIdx + 2], CachedMaskPixels[CenterIdx + 3]);
        }

        // 마지막 픽셀
        int32 LastIdx = NumBytes - 4;
        UE_LOG(LogTemp, Warning,
            TEXT("  └─ 끝(%d,%d): BGRA[%d,%d,%d,%d]"),
            Width - 1, Height - 1,
            CachedMaskPixels[LastIdx + 0], CachedMaskPixels[LastIdx + 1],
            CachedMaskPixels[LastIdx + 2], CachedMaskPixels[LastIdx + 3]);
    }

    UE_LOG(LogTemp, Warning,
        TEXT("✅ MaskPixel 캐시 완료: %dx%d (%d bytes)"),
        Width, Height, NumBytes);
}

FColor ALandscapeChecker::SampleMaskAtUV(const FVector2D& UV)
{
    if (!bMaskCacheReady || CachedMaskPixels.Num() == 0)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("⚠️ Mask cache not ready! Ready=%d PixelNum=%d"),
            (int32)bMaskCacheReady, CachedMaskPixels.Num());
        return DefaultOutOfBoundsColor;  // ← 이게 (0,0,0)일 가능성
    }

    const float U = FMath::Clamp(UV.X, 0.0f, 1.0f);
    const float V = FMath::Clamp(UV.Y, 0.0f, 1.0f);

    const int32 X = FMath::Clamp(FMath::FloorToInt(U * CachedMaskWidth), 0, CachedMaskWidth - 1);
    const int32 Y = FMath::Clamp(FMath::FloorToInt(V * CachedMaskHeight), 0, CachedMaskHeight - 1);
    const int32 Index = (Y * CachedMaskWidth + X) * 4;

    // ★★ 진단 로그 추가
    UE_LOG(LogTemp, Warning,
        TEXT("🖼️ SamplePixel: UV=(%.3f,%.3f) → XY=(%d,%d) → Idx=%d/%d | Size=%dx%d"),
        UV.X, UV.Y, X, Y, Index, CachedMaskPixels.Num(),
        CachedMaskWidth, CachedMaskHeight);

    if (Index + 3 >= CachedMaskPixels.Num())
    {
        return DefaultOutOfBoundsColor;
    }

    const uint8 B = CachedMaskPixels[Index + 0];
    const uint8 G = CachedMaskPixels[Index + 1];
    const uint8 R = CachedMaskPixels[Index + 2];
    const uint8 A = CachedMaskPixels[Index + 3];

    return FColor(R, G, B, A);
}

FColor ALandscapeChecker::SampleMaskAtWorldLocation(const FVector& WorldLocation)
{
    return SampleMaskAtUV(WorldLocationToMaskUV(WorldLocation));
}

FVector2D ALandscapeChecker::WorldLocationToMaskUV(const FVector& WorldLocation)
{
    const float RangeX = MaskWorldMax.X - MaskWorldMin.X;
    const float RangeY = MaskWorldMax.Y - MaskWorldMin.Y;

    if (FMath::IsNearlyZero(RangeX) || FMath::IsNearlyZero(RangeY))
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ MaskBounds Range=0! Min=%s Max=%s"),
            *MaskWorldMin.ToString(), *MaskWorldMax.ToString());
        return FVector2D::ZeroVector;
    }

    const float U = (WorldLocation.X - MaskWorldMin.X) / RangeX;
    const float V = (WorldLocation.Y - MaskWorldMin.Y) / RangeY;

    // ★★ 진단 로그 추가
    UE_LOG(LogTemp, Warning,
        TEXT("🗺️ UV Calc: World=(%.0f,%.0f) | Min=(%.0f,%.0f) Max=(%.0f,%.0f) | UV=(%.3f,%.3f) | InBounds=%d"),
        WorldLocation.X, WorldLocation.Y,
        MaskWorldMin.X, MaskWorldMin.Y,
        MaskWorldMax.X, MaskWorldMax.Y,
        U, V,
        (U >= 0.0f && U <= 1.0f && V >= 0.0f && V <= 1.0f) ? 1 : 0);

    return FVector2D(U, V);
}
// ═══════════════════════════════════════════════════════════
// GolfBall 캐시 (트레이스 Ignore 목록)
// ═══════════════════════════════════════════════════════════

void ALandscapeChecker::RebuildIgnoredBallCache()
{
    CachedIgnoredBalls.Empty();

    UWorld* World = GetWorld();
    if (!World) return;

    TArray<AActor*> FoundBalls;
    UGameplayStatics::GetAllActorsOfClass(World, AGolfBall::StaticClass(), FoundBalls);

    for (AActor* Actor : FoundBalls)
    {
        if (AGolfBall* Ball = Cast<AGolfBall>(Actor))
        {
            CachedIgnoredBalls.Add(Ball);
        }
    }
}

// ═══════════════════════════════════════════════════════════
// 매핑 관리
// ═══════════════════════════════════════════════════════════

void ALandscapeChecker::AddPhysicalMaterialMapping(UPhysicalMaterial* PhysMat, const FLandProperties& Properties)
{
    if (!PhysMat) return;

    for (FPhysicalMaterialMapping& Mapping : PhysicalMaterialMappings)
    {
        if (Mapping.PhysicalMaterial == PhysMat)
        {
            Mapping.LandProperties = Properties;
            return;
        }
    }

    FPhysicalMaterialMapping NewMapping;
    NewMapping.PhysicalMaterial = PhysMat;
    NewMapping.LandProperties = Properties;
    PhysicalMaterialMappings.Add(NewMapping);
}

void ALandscapeChecker::RemovePhysicalMaterialMapping(UPhysicalMaterial* PhysMat)
{
    if (!PhysMat) return;

    PhysicalMaterialMappings.RemoveAll([PhysMat](const FPhysicalMaterialMapping& M)
        {
            return M.PhysicalMaterial == PhysMat;
        });
}

void ALandscapeChecker::SetupDefaultMaterialMappings()
{
    InitializeDefaultMappings();
}

void ALandscapeChecker::InitializeDefaultMappings()
{
    // PhysicalMaterialMappings는 에디터에서 세팅. 기본 매핑이 필요하면 여기서 추가.
    // 지금 구조에서는 PhysMatNameToLandType()가 이름 기반 매칭을 하므로 대부분 불필요.
}

FLandProperties ALandscapeChecker::GetPropertiesFromPhysicalMaterial(UPhysicalMaterial* PhysMat)
{
    // 1) 등록된 매핑에서 찾기
    if (PhysMat)
    {
        for (const FPhysicalMaterialMapping& Mapping : PhysicalMaterialMappings)
        {
            if (Mapping.PhysicalMaterial == PhysMat)
            {
                return Mapping.LandProperties;
            }
        }

        // 2) 이름 기반 매칭 fallback
        return GetPropertiesFromMaskLandType(PhysMatNameToLandType(PhysMat->GetName()));
    }

    // 3) 폴백
    return GetPropertiesFromMaskLandType(ELandType::Rough);
}

// ═══════════════════════════════════════════════════════════
// 트레이스 (기존 API 유지)
// ═══════════════════════════════════════════════════════════

FLandCheckResult ALandscapeChecker::PerformLineTrace(const FVector& StartLocation, const FVector& EndLocation)
{
    FLandCheckResult Result;

    UWorld* World = GetWorld();
    if (!World) return Result;

    FCollisionQueryParams Params(SCENE_QUERY_STAT(LandChecker_PerformTrace), bUseComplexCollision);
    Params.bReturnPhysicalMaterial = true;
    Params.AddIgnoredActor(this);

    for (AGolfBall* Ball : CachedIgnoredBalls)
    {
        if (IsValid(Ball))
        {
            Params.AddIgnoredActor(Ball);
        }
    }

    FHitResult Hit;
    const bool bHit = World->LineTraceSingleByChannel(Hit, StartLocation, EndLocation, ECC_WorldStatic, Params);

    if (bDrawTrace)
    {
        DrawDebugLine(World, StartLocation, EndLocation,
            bHit ? TraceColorHit : TraceColorMiss,
            false, TraceLineLifeTime, 0, TraceLineThickness);
    }

    if (bHit)
    {
        Result.bHitGround = true;
        Result.HitLocation = Hit.ImpactPoint;
        Result.HitNormal = Hit.ImpactNormal;
        Result.HitActor = Hit.GetActor();
        Result.HitComponent = Hit.GetComponent();
        Result.HitPhysicalMaterial = Hit.PhysMaterial.IsValid() ? Hit.PhysMaterial.Get() : nullptr;
        Result.LandProperties = GetPropertiesFromPhysicalMaterial(Result.HitPhysicalMaterial);

        DebugTraceCount++;
    }

    return Result;
}

const FHitResult* ALandscapeChecker::PickBestHit(const TArray<FHitResult>& Hits) const
{
    if (Hits.Num() == 0) return nullptr;

    const FHitResult* Best = &Hits[0];
    if (bPreferDeepestHit)
    {
        float DeepestZ = Best->ImpactPoint.Z;
        for (const FHitResult& H : Hits)
        {
            if (H.ImpactPoint.Z < DeepestZ)
            {
                DeepestZ = H.ImpactPoint.Z;
                Best = &H;
            }
        }
    }
    return Best;
}

// ═══════════════════════════════════════════════════════════
// 캐시
// ═══════════════════════════════════════════════════════════

FVector2D ALandscapeChecker::WorldLocationToGridKey(const FVector& WorldLocation)
{
    const float SnapX = FMath::FloorToFloat(WorldLocation.X / CacheGridSize) * CacheGridSize;
    const float SnapY = FMath::FloorToFloat(WorldLocation.Y / CacheGridSize) * CacheGridSize;
    return FVector2D(SnapX, SnapY);
}

bool ALandscapeChecker::GetCachedResult(const FVector& WorldLocation, FLandCheckResult& OutResult)
{
    if (!bEnableCaching) return false;

    const FVector2D Key = WorldLocationToGridKey(WorldLocation);
    if (const FCacheEntry* Entry = LocationCache.Find(Key))
    {
        OutResult = Entry->Result;
        DebugCacheHitCount++;
        return true;
    }

    DebugCacheMissCount++;
    return false;
}

void ALandscapeChecker::CacheResult(const FVector& WorldLocation, const FLandCheckResult& Result)
{
    if (!bEnableCaching) return;

    // 기본 PMat 결과는 캐시하지 않음 (옵션)
    if (bSkipCacheIfDefaultPMat && Result.HitPhysicalMaterial &&
        Result.HitPhysicalMaterial->GetName().Contains(TEXT("Default")))
    {
        return;
    }

    if (LocationCache.Num() >= MaxCacheEntries)
    {
        CleanupOldCacheEntries();
    }

    const FVector2D Key = WorldLocationToGridKey(WorldLocation);
    LocationCache.Add(Key, FCacheEntry(Result, GetWorld()->GetTimeSeconds()));
}

void ALandscapeChecker::CleanupOldCacheEntries()
{
    if (!GetWorld()) return;

    const float Now = GetWorld()->GetTimeSeconds();
    TArray<FVector2D> KeysToRemove;

    for (const auto& Pair : LocationCache)
    {
        if (Now - Pair.Value.TimeStamp > CACHE_ENTRY_LIFETIME)
        {
            KeysToRemove.Add(Pair.Key);
        }
    }

    for (const FVector2D& K : KeysToRemove)
    {
        LocationCache.Remove(K);
    }
}

void ALandscapeChecker::EnableCaching(bool bEnable)
{
    bEnableCaching = bEnable;
    if (!bEnable)
    {
        LocationCache.Empty();
    }
}

void ALandscapeChecker::ClearCache()
{
    LocationCache.Empty();
    UE_LOG(LogTemp, Log, TEXT("🗑️ Cache cleared"));
}

// ═══════════════════════════════════════════════════════════
// 디버그
// ═══════════════════════════════════════════════════════════

void ALandscapeChecker::ShowLandTypeAtLocation(const FVector& WorldLocation, float DisplayTime)
{
    const ELandType Type = ResolveLandTypeAt(WorldLocation);
    const FLandProperties Props = GetPropertiesFromMaskLandType(Type);

    DrawDebugSphere(GetWorld(), WorldLocation, DebugSphereSize, 12,
        Props.DebugColor.ToFColor(true), false, DisplayTime);

    const FString Msg = FString::Printf(TEXT("Land: %s"), *Props.DisplayName);
    DrawDebugString(GetWorld(), WorldLocation + FVector(0, 0, DebugSphereSize + 10),
        Msg, nullptr, FColor::White, DisplayTime);
}

void ALandscapeChecker::ToggleDebugMode()
{
    bShowDebugInfo = !bShowDebugInfo;
    UE_LOG(LogTemp, Log, TEXT("🐛 Debug Mode: %s"), bShowDebugInfo ? TEXT("ON") : TEXT("OFF"));
}

void ALandscapeChecker::DrawDebugLandGrid(const FVector& CenterLocation, float GridSize, int32 GridResolution)
{
    if (GridResolution <= 0) return;

    const float StepSize = GridSize / GridResolution;
    const float HalfGrid = GridSize * 0.5f;

    for (int32 x = 0; x < GridResolution; ++x)
    {
        for (int32 y = 0; y < GridResolution; ++y)
        {
            const FVector Loc = CenterLocation + FVector(
                x * StepSize - HalfGrid,
                y * StepSize - HalfGrid,
                0.0f);

            const FLandProperties Props = ResolveLandPropertiesAt(Loc);
            DrawDebugPoint(GetWorld(), Loc, 5.0f, Props.DebugColor.ToFColor(true), false, 5.0f);
        }
    }
}

void ALandscapeChecker::SetDrawTrace(bool bEnable)
{
    bDrawTrace = bEnable;
}

// ═══════════════════════════════════════════════════════════
// 마스크 설정 헬퍼
// ═══════════════════════════════════════════════════════════

void ALandscapeChecker::SetMaskTexture(UTexture2D* InMaskTexture)
{
    MaskTexture = InMaskTexture;
    CacheMaskPixelData();  // 재캐시
}

void ALandscapeChecker::SetMaskWorldBounds(const FVector& InWorldMin, const FVector& InWorldMax)
{
    MaskWorldMin = InWorldMin;
    MaskWorldMax = InWorldMax;
    UE_LOG(LogTemp, Log, TEXT("🌍 Mask bounds set: Min=%s, Max=%s"),
        *InWorldMin.ToString(), *InWorldMax.ToString());
}

void ALandscapeChecker::AutoCalculateMaskWorldBounds()
{
    AnalyzeLandscapeBounds();
}

void ALandscapeChecker::AnalyzeLandscapeBounds()
{
    UWorld* World = GetWorld();
    if (!World) return;

    TArray<AActor*> Landscapes;
    UGameplayStatics::GetAllActorsOfClass(World, ALandscapeProxy::StaticClass(), Landscapes);

    if (Landscapes.Num() == 0)
    {
        // Landscape가 없으면 마스크 무의미 → PhysMat 폴백 모드로 자동 전환
        bUseMaskTexture = false;
        UE_LOG(LogTemp, Log,
            TEXT("🌍 AnalyzeLandscapeBounds: Landscape 없음 → bUseMaskTexture=false (PhysMat 폴백 모드)"));
        return;
    }

    // Landscape 존재 시 월드 바운드 계산
    FBox CombinedBox(EForceInit::ForceInit);
    for (AActor* Actor : Landscapes)
    {
        if (Actor)
        {
            CombinedBox += Actor->GetComponentsBoundingBox(true);
        }
    }

    if (CombinedBox.IsValid)
    {
        MaskWorldMin = CombinedBox.Min;
        MaskWorldMax = CombinedBox.Max;
        UE_LOG(LogTemp, Log,
            TEXT("🌍 AnalyzeLandscapeBounds: Landscape %d개, Bounds Min=%s Max=%s"),
            Landscapes.Num(), *MaskWorldMin.ToString(), *MaskWorldMax.ToString());
    }
}

// ═══════════════════════════════════════════════════════════
// 정적 인스턴스 접근
// ═══════════════════════════════════════════════════════════

ALandscapeChecker* ALandscapeChecker::GetLandscapeChecker(UWorld* World)
{
    if (Instance && IsValid(Instance)) return Instance;

    if (!World) return nullptr;

    TArray<AActor*> Found;
    UGameplayStatics::GetAllActorsOfClass(World, ALandscapeChecker::StaticClass(), Found);
    if (Found.Num() > 0)
    {
        Instance = Cast<ALandscapeChecker>(Found[0]);
        return Instance;
    }

    return nullptr;
}

bool ALandscapeChecker::LoadDefaultMaskTexture()
{
    FString AssetPath = MaskTexturePath.IsEmpty()
        ? FString(TEXT("/Game/Landscape_Material/mask.mask"))
        : MaskTexturePath;

    UE_LOG(LogTemp, Log, TEXT("🔄 LoadDefaultMaskTexture: %s"), *AssetPath);

    UTexture2D* LoadedTexture = LoadObject<UTexture2D>(
        nullptr, *AssetPath, nullptr, LOAD_None, nullptr);

    if (!LoadedTexture)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ MaskTexture 로드 실패: %s"), *AssetPath);
        return false;
    }

    MaskTexture = LoadedTexture;

    // 텍스처 속성 확인만 (변경 X, UpdateResource X)
    UE_LOG(LogTemp, Warning,
        TEXT("✅ MaskTexture 로드 완료:"));
    UE_LOG(LogTemp, Warning,
        TEXT("  ├─ 이름: %s"), *MaskTexture->GetName());
    UE_LOG(LogTemp, Warning,
        TEXT("  ├─ 크기: %dx%d"),
        MaskTexture->GetPlatformData()->Mips[0].SizeX,
        MaskTexture->GetPlatformData()->Mips[0].SizeY);
    UE_LOG(LogTemp, Warning,
        TEXT("  ├─ 포맷: %d"), (int32)MaskTexture->GetPixelFormat());
    UE_LOG(LogTemp, Warning,
        TEXT("  ├─ sRGB: %d %s"),
        (int32)MaskTexture->SRGB,
        MaskTexture->SRGB ? TEXT("⚠️(감마 왜곡)") : TEXT("✓"));
    UE_LOG(LogTemp, Warning,
        TEXT("  ├─ Compression: %d %s"),
        (int32)MaskTexture->CompressionSettings,
        (MaskTexture->CompressionSettings == TC_VectorDisplacementmap
            ? TEXT("⚠️(에셋 설정 변경 필요)")
            : TEXT("✓")));
    UE_LOG(LogTemp, Warning,
        TEXT("  └─ NeverStream: %d %s"),
        (int32)MaskTexture->NeverStream,
        MaskTexture->NeverStream ? TEXT("✓") : TEXT("⚠️(에셋에서 체크 필요)"));

    return true;
}