// LandscapeSectionBoundaryDetector.cpp
#include "LandscapeSectionBoundaryDetector.h"

// UE4 상수 정의
const float FLandscapeSectionBoundaryDetector::SECTION_BOUNDARY_THRESHOLD = 5.0f;   // 15.0f -> 5.0f로 완화
const float FLandscapeSectionBoundaryDetector::NORMAL_VARIANCE_THRESHOLD = 3.0f;    // 10.0f -> 3.0f로 완화
const int32 FLandscapeSectionBoundaryDetector::SAMPLE_COUNT = 9;                    // 9 -> 16개로 증가
const float FLandscapeSectionBoundaryDetector::SAMPLE_RADIUS = 40.0f;               // 20.0f -> 40.0f로 확장

FLandscapeSectionBoundaryDetector::FLandscapeHitInfo
FLandscapeSectionBoundaryDetector::AnalyzeLandscapeHit(
    const FHitResult& Hit,
    UWorld* World,
    bool bApplySmoothing)
{
    FLandscapeHitInfo HitInfo;
    HitInfo.Location = Hit.ImpactPoint;
    HitInfo.Normal = Hit.Normal;

    ALandscapeProxy* Landscape = Cast<ALandscapeProxy>(Hit.GetActor());
    if (!Landscape)
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("Not a Landscape actor"));
        return HitInfo;
    }

    // 컴포넌트 찾기
    ULandscapeComponent* HitComponent = nullptr;
    for (ULandscapeComponent* Component : Landscape->LandscapeComponents)
    {
        if (Component && IsValid(Component))
        {
            FBox ComponentBox = Component->Bounds.GetBox();
            if (ComponentBox.IsInside(Hit.ImpactPoint))
            {
                HitComponent = Component;
                break;
            }
        }
    }

    if (HitComponent && IsValid(HitComponent))
    {
        HitInfo.HitComponent = HitComponent;

        // 1. 섹션 경계면 감지 (완화된 기준 적용)
        HitInfo.bIsSectionBoundary = IsSectionBoundary(
            Hit.ImpactPoint,
            HitComponent,
            World,
            HitInfo.NormalVariance
        );

        // 2. 샘플 노말들 수집
        TArray<FVector> SamplePositions = GenerateSamplePositions(Hit.ImpactPoint, 40.0f, 16);
        for (const FVector& SamplePos : SamplePositions)
        {
            FVector SampleNormal = GetNormalAtPosition(SamplePos, World);
            if (!SampleNormal.IsZero())
            {
                HitInfo.SampleNormals.Add(SampleNormal);
            }
        }

        // 3. 완화된 스무딩 적용 조건
        // 기존: 섹션 경계면이거나 분산이 10도 이상
        // 수정: 분산이 3도 이상이면 스무딩 적용
        if (bApplySmoothing && (HitInfo.bIsSectionBoundary || HitInfo.NormalVariance > 1.0f))
        {
            HitInfo.Normal = GetSmoothedNormal(Hit.ImpactPoint, World, 40.0f, 16);

            UE_LOG(LogTemp, Log, TEXT("Landscape 노말 스무딩 적용: 분산=%.3f, 경계면=%s"),
                HitInfo.NormalVariance,
                HitInfo.bIsSectionBoundary ? TEXT("예") : TEXT("아니오"));
        }

        // 4. 컴포넌트 좌표 계산
        CalculateComponentCoordinates(Hit.ImpactPoint, HitComponent, HitInfo);
    }

    return HitInfo;
}

bool FLandscapeSectionBoundaryDetector::IsSectionBoundary(
    const FVector& HitLocation,
    ULandscapeComponent* Component,
    UWorld* World,
    float& OutNormalVariance)
{
    if (!Component || !IsValid(Component) || !World)
    {
        OutNormalVariance = 0.0f;
        UE_LOG(LogTemp, Warning, TEXT("IsSectionBoundary: Invalid Component or World"));
        return false;
    }

  //  UE_LOG(LogTemp, Log, TEXT("🔍 Checking section boundary at: %s"), *HitLocation.ToString());

    // 1. 컴포넌트 경계 근처인지 체크 (임계값 완화)
    bool bNearBoundary = IsNearComponentBoundaryImproved(HitLocation, Component, 10.0f); // 30cm -> 50cm
  //  UE_LOG(LogTemp, Log, TEXT("   Near boundary (10cm): %s"), bNearBoundary ? TEXT("YES") : TEXT("NO"));

    // 2. 더 넓은 범위에서 노말값들의 변화량 체크
    TArray<FVector> SampleNormals;
    TArray<FVector> SamplePositions = GenerateSamplePositions(HitLocation, 20.0f, 16); // 25cm->40cm, 12개->16개

    int32 ValidSamples = 0;
    for (const FVector& SamplePos : SamplePositions)
    {
        FVector SampleNormal = GetNormalAtPositionImproved(SamplePos, World);
        if (!SampleNormal.IsZero() && SampleNormal.Z > 0.1f)
        {
            SampleNormals.Add(SampleNormal);
            ValidSamples++;
        }
    }

 //   UE_LOG(LogTemp, Log, TEXT("   Valid samples: %d/%d"), ValidSamples, SamplePositions.Num());

    if (ValidSamples < 3)
    {
        OutNormalVariance = 0.0f;
        return false;
    }

    // 3. 노말 분산 계산
    OutNormalVariance = CalculateNormalVariance(SampleNormals);
 //   UE_LOG(LogTemp, Log, TEXT("   Normal variance: %.3f degrees"), OutNormalVariance);

    // 4. 개선된 인접 컴포넌트 체크
    bool bHasAdjacentComponents = CheckAdjacentComponentsImproved(HitLocation, Component);
  //  UE_LOG(LogTemp, Log, TEXT("   Adjacent components: %s"), bHasAdjacentComponents ? TEXT("YES") : TEXT("NO"));

    // ⭐ 5. 완화된 경계면 판정 기준
    // 기존: 경계근처 + 높은분산 + 인접컴포넌트 (모두 만족)
    // 수정: 다음 중 하나만 만족하면 경계면으로 판정

    bool bIsSectionBoundary = false;
    FString DetectionReason = TEXT("None");

    // 조건 1: 경계 근처 + 분산이 5도 이상 (기존 15도에서 완화)
    if (bNearBoundary && OutNormalVariance > 0.1f)
    {
        bIsSectionBoundary = true;
        DetectionReason = TEXT("Near boundary + variance");
    }

    // 조건 2: 분산이 10도 이상 (경계 위치와 관계없이)
    else if (OutNormalVariance > 10.0f)
    {
        bIsSectionBoundary = true;
        DetectionReason = TEXT("High variance only");
    }

    // 조건 3: 경계 근처 + 인접 컴포넌트 존재
    else if (bNearBoundary && bHasAdjacentComponents)
    {
        bIsSectionBoundary = true;
        DetectionReason = TEXT("Near boundary + adjacent components");
    }

    // 조건 4: 노말값이 급격히 변하는 패턴 감지
    else if (bNearBoundary && DetectNormalChangePattern(SampleNormals))
    {
        bIsSectionBoundary = true;
        DetectionReason = TEXT("Normal change pattern detected");
    }

    //UE_LOG(LogTemp, Warning, TEXT("🔍 Section boundary result: %s"),
    //    bIsSectionBoundary ? TEXT("TRUE") : TEXT("FALSE"));
    //UE_LOG(LogTemp, Warning, TEXT("   Detection reason: %s"), *DetectionReason);
    //UE_LOG(LogTemp, Warning, TEXT("   Boundary: %s, Variance: %.2f°, Adjacent: %s"),
    //    bNearBoundary ? TEXT("YES") : TEXT("NO"),
    //    OutNormalVariance,
    //    bHasAdjacentComponents ? TEXT("YES") : TEXT("NO"));

    return bIsSectionBoundary;
}

FVector FLandscapeSectionBoundaryDetector::GetSmoothedNormal(
    const FVector& CenterLocation,
    UWorld* World,
    float SampleRadius,
    int32 SampleCount)
{
    if (!World || !IsValid(World))
    {
        return FVector::UpVector;
    }

    TArray<FVector> SampleNormals;
    TArray<float> SampleWeights;
    TArray<FVector> SamplePositions = GenerateSamplePositions(CenterLocation, SampleRadius, SampleCount);

    // 각 샘플 위치에서 노말값과 가중치 수집
    for (const FVector& SamplePos : SamplePositions)
    {
        FHitResult SampleHit;
        FVector TraceStart = SamplePos + FVector(0, 0, 50.0f);
        FVector TraceEnd = SamplePos - FVector(0, 0, 100.0f);

        FCollisionQueryParams QueryParams;
        QueryParams.bTraceComplex = true;

        if (World->LineTraceSingleByChannel(SampleHit, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
        {
            if (SampleHit.Normal.Z > 0.1f) // 유효한 노말값만
            {
                SampleNormals.Add(SampleHit.Normal);

                // 거리 기반 가중치 계산
                float Distance = FVector::Dist2D(CenterLocation, SamplePos);
                float Weight = 1.0f / (1.0f + Distance / SampleRadius);
                SampleWeights.Add(Weight);
            }
        }
    }

    if (SampleNormals.Num() < 3)
    {
        UE_LOG(LogTemp, Warning, TEXT("노말 스무딩용 샘플이 부족함: %d개"), SampleNormals.Num());
        return FVector::UpVector;
    }

    // 가중 평균으로 스무딩된 노말 계산
    FVector SmoothedNormal = GetWeightedAverageNormal(SampleNormals, SampleWeights);

    // 추가 필터링
    SmoothedNormal = ClampNormalToReasonableRange(SmoothedNormal);

    UE_LOG(LogTemp, Log, TEXT("노말 스무딩 결과: %d개 샘플 → %s"),
        SampleNormals.Num(), *SmoothedNormal.ToString());

    return SmoothedNormal;
}

bool FLandscapeSectionBoundaryDetector::IsNearComponentBoundary(
    const FVector& WorldLocation,
    ULandscapeComponent* Component,
    float BoundaryThreshold)
{
    if (!Component || !IsValid(Component))
    {
        return false;
    }

    // UE4에서 컴포넌트 바운딩 박스 가져오기
    FBox ComponentBounds = Component->Bounds.GetBox();

    // 각 면까지의 거리 계산
    float DistToMinX = FMath::Abs(WorldLocation.X - ComponentBounds.Min.X);
    float DistToMaxX = FMath::Abs(WorldLocation.X - ComponentBounds.Max.X);
    float DistToMinY = FMath::Abs(WorldLocation.Y - ComponentBounds.Min.Y);
    float DistToMaxY = FMath::Abs(WorldLocation.Y - ComponentBounds.Max.Y);

    // 가장 가까운 경계까지의 거리
    float MinDistToBoundary = FMath::Min(FMath::Min(DistToMinX, DistToMaxX), FMath::Min(DistToMinY, DistToMaxY));

    bool bNearBoundary = MinDistToBoundary <= BoundaryThreshold;

    UE_LOG(LogTemp, Log, TEXT("컴포넌트 경계 체크: 최단거리=%.1fcm, 임계값=%.1fcm, 근처=%s"),
        MinDistToBoundary, BoundaryThreshold, bNearBoundary ? TEXT("예") : TEXT("아니오"));

    return bNearBoundary;
}

float FLandscapeSectionBoundaryDetector::CalculateNormalVariance(const TArray<FVector>& Normals)
{
    if (Normals.Num() < 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("Not enough normals for variance calculation: %d"), Normals.Num());
        return 0.0f;
    }

    // 평균 노말 계산
    FVector MeanNormal = FVector::ZeroVector;
    for (const FVector& Normal : Normals)
    {
        float Length = Normal.Size();

        MeanNormal += Normal;
    }
    MeanNormal /= static_cast<float>(Normals.Num());
    MeanNormal = MeanNormal.GetSafeNormal();


    // 각 노말과 평균 노말 간의 각도 분산 계산
    float VarianceSum = 0.0f;
    for (const FVector& Normal : Normals)
    {
        float DotProduct = FVector::DotProduct(Normal, MeanNormal);
        float ClampedDot = FMath::Clamp(DotProduct, -0.9999f, 0.9999f);
        float Angle = FMath::Acos(ClampedDot);
        float AngleDegrees = FMath::RadiansToDegrees(Angle);
        VarianceSum += Angle * Angle;
    }

    float Variance = VarianceSum / static_cast<float>(Normals.Num());
    float Result = FMath::RadiansToDegrees(FMath::Sqrt(Variance));
    UE_LOG(LogTemp, Log, TEXT("VarianceSum: %.3f, Variance: %.3f, OutNormalVariance: %.3f"), VarianceSum, Variance, Result);
    return Result;
}


FVector FLandscapeSectionBoundaryDetector::GetWeightedAverageNormal(
    const TArray<FVector>& Normals,
    const TArray<float>& Weights)
{
    if (Normals.Num() != Weights.Num() || Normals.Num() == 0)
    {
        return FVector::UpVector;
    }

    FVector WeightedSum = FVector::ZeroVector;
    float TotalWeight = 0.0f;

    for (int32 i = 0; i < Normals.Num(); i++)
    {
        WeightedSum += Normals[i] * Weights[i];
        TotalWeight += Weights[i];
    }

    if (TotalWeight > 0.0f)
    {
        return (WeightedSum / TotalWeight).GetSafeNormal();
    }

    return FVector::UpVector;
}

TArray<FVector> FLandscapeSectionBoundaryDetector::GenerateSamplePositions(
    const FVector& CenterLocation,
    float Radius,
    int32 Count)
{
    TArray<FVector> Positions;

    // 중앙점 추가
    Positions.Add(CenterLocation);

    // 원형으로 샘플 위치 생성
    for (int32 i = 0; i < Count - 1; i++)
    {
        float Angle = (static_cast<float>(i) / static_cast<float>(Count - 1)) * 2.0f * PI;
        FVector Offset = FVector(
            FMath::Cos(Angle) * Radius,
            FMath::Sin(Angle) * Radius,
            0.0f
        );
        Positions.Add(CenterLocation + Offset);
    }

    return Positions;
}

FVector FLandscapeSectionBoundaryDetector::GetNormalAtPosition(
    const FVector& WorldLocation,
    UWorld* World)
{
    if (!World || !IsValid(World))
    {
        return FVector::ZeroVector;
    }

    FHitResult HitResult;
    FVector TraceStart = WorldLocation + FVector(0, 0, 50.0f);
    FVector TraceEnd = WorldLocation - FVector(0, 0, 100.0f);

    FCollisionQueryParams QueryParams;
    QueryParams.bTraceComplex = true;

    if (World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
    {
        return HitResult.Normal;
    }

    return FVector::ZeroVector;
}

FVector FLandscapeSectionBoundaryDetector::ClampNormalToReasonableRange(const FVector& Normal)
{
    // 너무 가파른 경사 제한 (최대 45도)
    float MaxSlopeAngle = FMath::DegreesToRadians(45.0f);
    float CurrentAngle = FMath::Acos(FMath::Clamp(FMath::Abs(Normal.Z), 0.0f, 1.0f));

    if (CurrentAngle > MaxSlopeAngle)
    {
        // 45도로 제한
        float NewZ = FMath::Cos(MaxSlopeAngle);
        FVector HorizontalComponent = FVector(Normal.X, Normal.Y, 0.0f).GetSafeNormal();
        float HorizontalMagnitude = FMath::Sin(MaxSlopeAngle);

        return (HorizontalComponent * HorizontalMagnitude + FVector::UpVector * NewZ).GetSafeNormal();
    }

    return Normal;
}

void FLandscapeSectionBoundaryDetector::CalculateComponentCoordinates(
    const FVector& WorldLocation,
    ULandscapeComponent* Component,
    FLandscapeHitInfo& HitInfo)
{
    if (!Component || !IsValid(Component))
    {
        return;
    }

    // UE4에서 컴포넌트 좌표 계산
    FVector LocalPosition = Component->GetComponentTransform().InverseTransformPosition(WorldLocation);

    // 컴포넌트 내 로컬 좌표 (0-1 범위로 정규화)
    FBox ComponentBounds = Component->Bounds.GetBox();
    FVector BoundsSize = ComponentBounds.GetSize();

    if (BoundsSize.X > 0.0f && BoundsSize.Y > 0.0f)
    {
        HitInfo.LocalCoordinates.X = (LocalPosition.X - ComponentBounds.Min.X) / BoundsSize.X;
        HitInfo.LocalCoordinates.Y = (LocalPosition.Y - ComponentBounds.Min.Y) / BoundsSize.Y;
    }

    // 컴포넌트 그리드 좌표
    HitInfo.ComponentCoordinates = Component->GetSectionBase();
}

void FLandscapeSectionBoundaryDetector::DebugVisualizeSectionBoundary(
    UWorld* World,
    const FLandscapeHitInfo& HitInfo,
    float Duration)
{
    if (!World || !IsValid(World))
        return;

    FColor VisualColor = HitInfo.bIsSectionBoundary ? FColor::Red : FColor::Green;

    // 충돌 지점 표시
    DrawDebugSphere(World, HitInfo.Location, 5.0f, 12, VisualColor, false, Duration, 0, 2.0f);

    // 노말 벡터 표시
    FVector NormalEnd = HitInfo.Location + (HitInfo.Normal * 50.0f);
    DrawDebugLine(World, HitInfo.Location, NormalEnd, VisualColor, false, Duration, 0, 3.0f);

    // 샘플 노말들 표시
    for (int32 i = 0; i < HitInfo.SampleNormals.Num(); i++)
    {
        float AngleStep = 2.0f * PI / static_cast<float>(FMath::Max(1, HitInfo.SampleNormals.Num()));
        FVector SamplePos = HitInfo.Location + FVector(
            FMath::Cos(static_cast<float>(i) * AngleStep) * 20.0f,
            FMath::Sin(static_cast<float>(i) * AngleStep) * 20.0f,
            0.0f
        );
        FVector SampleNormalEnd = SamplePos + (HitInfo.SampleNormals[i] * 30.0f);
        DrawDebugLine(World, SamplePos, SampleNormalEnd, FColor::Blue, false, Duration, 0, 1.0f);
    }

    // 디버그 텍스트
    FString DebugText = FString::Printf(
        TEXT("섹션 경계: %s\n분산: %.2f도\n샘플 수: %d"),
        HitInfo.bIsSectionBoundary ? TEXT("예") : TEXT("아니오"),
        HitInfo.NormalVariance,
        HitInfo.SampleNormals.Num()
    );

    DrawDebugString(World, HitInfo.Location + FVector(0, 0, 30.0f),
        DebugText, nullptr, FColor::White, Duration, false, 1.0f);
}


bool FLandscapeSectionBoundaryDetector::IsNearComponentBoundaryImproved(
    const FVector& WorldLocation,
    ULandscapeComponent* Component,
    float BoundaryThreshold)
{
    if (!Component || !IsValid(Component))
    {
        return false;
    }

    FBox ComponentBounds = Component->Bounds.GetBox();

    // 각 면까지의 거리 계산
    float DistToMinX = FMath::Abs(WorldLocation.X - ComponentBounds.Min.X);
    float DistToMaxX = FMath::Abs(WorldLocation.X - ComponentBounds.Max.X);
    float DistToMinY = FMath::Abs(WorldLocation.Y - ComponentBounds.Min.Y);
    float DistToMaxY = FMath::Abs(WorldLocation.Y - ComponentBounds.Max.Y);

    // 가장 가까운 경계까지의 거리
    float MinDistToBoundary = FMath::Min(FMath::Min(DistToMinX, DistToMaxX), FMath::Min(DistToMinY, DistToMaxY));

    bool bNearBoundary = MinDistToBoundary <= BoundaryThreshold;

    UE_LOG(LogTemp, VeryVerbose, TEXT("Boundary check: MinDist=%.1fcm, Threshold=%.1fcm, Near=%s"),
        MinDistToBoundary, BoundaryThreshold, bNearBoundary ? TEXT("YES") : TEXT("NO"));

    // 추가 정보: 어느 경계에 가까운지
    if (bNearBoundary)
    {
        FString ClosestBoundary;
        if (MinDistToBoundary == DistToMinX) ClosestBoundary = TEXT("MinX");
        else if (MinDistToBoundary == DistToMaxX) ClosestBoundary = TEXT("MaxX");
        else if (MinDistToBoundary == DistToMinY) ClosestBoundary = TEXT("MinY");
        else ClosestBoundary = TEXT("MaxY");

        UE_LOG(LogTemp, VeryVerbose, TEXT("Closest to %s boundary"), *ClosestBoundary);
    }

    return bNearBoundary;
}

// 개선된 노말 획득 함수
FVector FLandscapeSectionBoundaryDetector::GetNormalAtPositionImproved(
    const FVector& WorldLocation,
    UWorld* World)
{
    if (!World || !IsValid(World))
    {
        return FVector::ZeroVector;
    }

    // 더 정밀한 트레이스 설정
    FHitResult HitResult;
    FVector TraceStart = WorldLocation + FVector(0, 0, 100.0f); // 1m 위에서 시작
    FVector TraceEnd = WorldLocation - FVector(0, 0, 200.0f);   // 2m 아래까지

    FCollisionQueryParams QueryParams;
    QueryParams.bTraceComplex = true;
    QueryParams.bReturnPhysicalMaterial = true;

    if (World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
    {
        // Landscape 액터인지 확인
        if (Cast<ALandscapeProxy>(HitResult.GetActor()))
        {
            return HitResult.Normal;
        }
    }

    return FVector::ZeroVector;
}

// 인접 컴포넌트 존재 확인
bool FLandscapeSectionBoundaryDetector::CheckAdjacentComponents(
    const FVector& HitLocation,
    ULandscapeComponent* Component)
{
    if (!Component || !IsValid(Component))
    {
        return false;
    }

    ALandscapeProxy* Landscape = Component->GetLandscapeProxy();
    if (!Landscape || !IsValid(Landscape))
    {
        return false;
    }

    // 현재 컴포넌트 좌표
    FIntPoint CurrentCoord = Component->GetSectionBase();

    // 4방향 인접 컴포넌트 체크
    TArray<FIntPoint> AdjacentCoords = {
        FIntPoint(CurrentCoord.X + 1, CurrentCoord.Y),     // 동쪽
        FIntPoint(CurrentCoord.X - 1, CurrentCoord.Y),     // 서쪽
        FIntPoint(CurrentCoord.X, CurrentCoord.Y + 1),     // 북쪽
        FIntPoint(CurrentCoord.X, CurrentCoord.Y - 1)      // 남쪽
    };

    int32 AdjacentCount = 0;
    for (const FIntPoint& Coord : AdjacentCoords)
    {
        // 인접 컴포넌트 찾기
        ULandscapeComponent* AdjacentComponent = nullptr;
        for (ULandscapeComponent* TestComponent : Landscape->LandscapeComponents)
        {
            if (TestComponent && IsValid(TestComponent))
            {
                if (TestComponent->GetSectionBase() == Coord)
                {
                    AdjacentComponent = TestComponent;
                    AdjacentCount++;
                    break;
                }
            }
        }
    }

    UE_LOG(LogTemp, VeryVerbose, TEXT("Adjacent components: %d/4"), AdjacentCount);

    // 최소 2개 이상의 인접 컴포넌트가 있어야 경계면 가능성 높음
    return AdjacentCount >= 2;
}

// 높이 변화 분석
float FLandscapeSectionBoundaryDetector::CalculateHeightVariance(
    const FVector& CenterLocation,
    UWorld* World)
{
    if (!World || !IsValid(World))
    {
        return 0.0f;
    }

    TArray<float> Heights;
    TArray<FVector> SamplePositions = GenerateSamplePositions(CenterLocation, 20.0f, 8);

    for (const FVector& SamplePos : SamplePositions)
    {
        FHitResult HitResult;
        FVector TraceStart = SamplePos + FVector(0, 0, 50.0f);
        FVector TraceEnd = SamplePos - FVector(0, 0, 100.0f);

        FCollisionQueryParams QueryParams;
        QueryParams.bTraceComplex = true;

        if (World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
        {
            if (Cast<ALandscapeProxy>(HitResult.GetActor()))
            {
                Heights.Add(HitResult.Location.Z);
            }
        }
    }

    if (Heights.Num() < 3)
    {
        return 0.0f;
    }

    // 높이 분산 계산
    float MeanHeight = 0.0f;
    for (float Height : Heights)
    {
        MeanHeight += Height;
    }
    MeanHeight /= Heights.Num();

    float Variance = 0.0f;
    for (float Height : Heights)
    {
        float Diff = Height - MeanHeight;
        Variance += Diff * Diff;
    }
    Variance /= Heights.Num();

    return FMath::Sqrt(Variance);
}

// 디버깅용 상세 로깅 함수 추가
void FLandscapeSectionBoundaryDetector::LogDetailedBoundaryInfo(
    const FVector& HitLocation,
    ULandscapeComponent* Component,
    const FLandscapeHitInfo& HitInfo)
{
    if (!Component || !IsValid(Component))
    {
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("=== Detailed Boundary Analysis ==="));
    UE_LOG(LogTemp, Warning, TEXT("Hit Location: %s"), *HitLocation.ToString());
    UE_LOG(LogTemp, Warning, TEXT("Component: %s"), *Component->GetName());
    UE_LOG(LogTemp, Warning, TEXT("Section Base: (%d, %d)"),
        Component->GetSectionBase().X, Component->GetSectionBase().Y);
    UE_LOG(LogTemp, Warning, TEXT("Component Size: %d quads"), Component->ComponentSizeQuads);
    UE_LOG(LogTemp, Warning, TEXT("Subsection Size: %d quads"), Component->SubsectionSizeQuads);
    UE_LOG(LogTemp, Warning, TEXT("Is Section Boundary: %s"),
        HitInfo.bIsSectionBoundary ? TEXT("TRUE") : TEXT("FALSE"));
    UE_LOG(LogTemp, Warning, TEXT("Normal Variance: %.3f degrees"), HitInfo.NormalVariance);
    UE_LOG(LogTemp, Warning, TEXT("Sample Count: %d"), HitInfo.SampleNormals.Num());
    UE_LOG(LogTemp, Warning, TEXT("Original Normal: %s"), *HitInfo.Normal.ToString());

    // 컴포넌트 바운딩 박스 정보
    FBox ComponentBounds = Component->Bounds.GetBox();
    UE_LOG(LogTemp, Warning, TEXT("Component Bounds: Min=%s, Max=%s"),
        *ComponentBounds.Min.ToString(), *ComponentBounds.Max.ToString());

    UE_LOG(LogTemp, Warning, TEXT("==============================="));
}

// 개선된 인접 컴포넌트 체크
bool FLandscapeSectionBoundaryDetector::CheckAdjacentComponentsImproved(
    const FVector& HitLocation,
    ULandscapeComponent* Component)
{
    if (!Component || !IsValid(Component))
    {
        return false;
    }

    ALandscapeProxy* Landscape = Component->GetLandscapeProxy();
    if (!Landscape || !IsValid(Landscape))
    {
        UE_LOG(LogTemp, Warning, TEXT("No valid LandscapeProxy found"));
        return false;
    }

    // 현재 컴포넌트 정보
    FIntPoint CurrentCoord = Component->GetSectionBase();
    UE_LOG(LogTemp, VeryVerbose, TEXT("Current component section base: (%d, %d)"),
        CurrentCoord.X, CurrentCoord.Y);

    // 8방향 인접 컴포넌트 체크 (4방향에서 8방향으로 확장)
    TArray<FIntPoint> AdjacentCoords = {
        FIntPoint(CurrentCoord.X + 1, CurrentCoord.Y),     // 동쪽
        FIntPoint(CurrentCoord.X - 1, CurrentCoord.Y),     // 서쪽
        FIntPoint(CurrentCoord.X, CurrentCoord.Y + 1),     // 북쪽
        FIntPoint(CurrentCoord.X, CurrentCoord.Y - 1),     // 남쪽
        FIntPoint(CurrentCoord.X + 1, CurrentCoord.Y + 1), // 북동쪽
        FIntPoint(CurrentCoord.X - 1, CurrentCoord.Y + 1), // 북서쪽
        FIntPoint(CurrentCoord.X + 1, CurrentCoord.Y - 1), // 남동쪽
        FIntPoint(CurrentCoord.X - 1, CurrentCoord.Y - 1)  // 남서쪽
    };

    int32 AdjacentCount = 0;
    for (const FIntPoint& Coord : AdjacentCoords)
    {
        for (ULandscapeComponent* TestComponent : Landscape->LandscapeComponents)
        {
            if (TestComponent && IsValid(TestComponent))
            {
                FIntPoint TestCoord = TestComponent->GetSectionBase();
                if (TestCoord == Coord)
                {
                    AdjacentCount++;
                    UE_LOG(LogTemp, VeryVerbose, TEXT("Found adjacent component at (%d, %d)"),
                        Coord.X, Coord.Y);
                    break;
                }
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Adjacent components found: %d/8"), AdjacentCount);

    // 완화된 기준: 1개 이상의 인접 컴포넌트만 있으면 OK (기존 2개에서 완화)
    return AdjacentCount >= 1;
}

// 노말 변화 패턴 감지 (새로운 방법)
bool FLandscapeSectionBoundaryDetector::DetectNormalChangePattern(const TArray<FVector>& SampleNormals)
{
    if (SampleNormals.Num() < 4)
    {
        return false;
    }

    // 1. 노말 방향의 급격한 변화 감지
    int32 DirectionChanges = 0;
    for (int32 i = 1; i < SampleNormals.Num(); i++)
    {
        float DotProduct = FVector::DotProduct(SampleNormals[i - 1], SampleNormals[i]);
        float AngleDiff = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(DotProduct, -1.0f, 1.0f)));

        if (AngleDiff > 2.0f) // 2도 이상 차이
        {
            DirectionChanges++;
        }
    }

    UE_LOG(LogTemp, VeryVerbose, TEXT("Direction changes detected: %d"), DirectionChanges);

    // 2. 노말값의 클러스터링 패턴 감지
    TArray<FVector> UniqueNormals;
    for (const FVector& Normal : SampleNormals)
    {
        bool bIsUnique = true;
        for (const FVector& UniqueNormal : UniqueNormals)
        {
            float Similarity = FVector::DotProduct(Normal, UniqueNormal);
            if (Similarity > 0.999f) // 매우 비슷한 노말
            {
                bIsUnique = false;
                break;
            }
        }
        if (bIsUnique)
        {
            UniqueNormals.Add(Normal);
        }
    }

    UE_LOG(LogTemp, VeryVerbose, TEXT("Unique normal groups: %d"), UniqueNormals.Num());

    // 3. 패턴 판정
    // - 방향 변화가 3회 이상 또는
    // - 고유한 노말 그룹이 3개 이상
    bool bHasPattern = (DirectionChanges >= 3) || (UniqueNormals.Num() >= 3);

    if (bHasPattern)
    {
        UE_LOG(LogTemp, Log, TEXT("Normal change pattern detected: Changes=%d, Groups=%d"),
            DirectionChanges, UniqueNormals.Num());
    }

    return bHasPattern;
}