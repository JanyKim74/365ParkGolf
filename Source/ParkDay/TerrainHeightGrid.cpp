// TerrainHeightGrid.cpp
// - Dynamic Material 전면 제거
// - 선분별 "흐름 점(스피어)" 인스턴스 추가/이동
// - 수정: 포인트/라인/플로우DOT 모두 "랜드스케이프 위"에서만 생성/유지
// - 수정: 흐름 점이 선분의 중간에서 시작하도록 T를 0.5f로 설정
// - 수정: 높이 차이가 0이어도 흐름 점 생성

#include "TerrainHeightGrid.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Engine.h"
#include "Landscape.h"
#include "EngineUtils.h"
#include "InGameMode.h"
#include "Engine/StaticMeshActor.h"  // TerrainHeightGrid.cpp 상단에 추가


ATerrainHeightGrid::ATerrainHeightGrid()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
    RootComponent = RootSceneComponent;

    InitializeComponents();
    WaterFlowSettings.Init();
}

void ATerrainHeightGrid::InitializeComponents()
{
    // 포인트
    GridPointMesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("GridPointMesh"));
    GridPointMesh->SetupAttachment(RootComponent);
    GridPointMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GridPointMesh->SetCastShadow(false);

    // 라인
    GridLineMesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("GridLineMesh"));
    GridLineMesh->SetupAttachment(RootComponent);
    GridLineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GridLineMesh->SetCastShadow(false);
    GridLineMesh->NumCustomDataFloats = 1; // ★ 인스턴스별 흐름 속도(부호 포함) 슬롯

    // 흐름 점
    FlowDotMesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("FlowDotMesh"));
    FlowDotMesh->SetupAttachment(RootComponent);
    FlowDotMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FlowDotMesh->SetCastShadow(false);
    FlowDotMesh->SetVisibility(false);   // ★ 추가: 기본 비활성화

    LoadDefaultResources();
}

void ATerrainHeightGrid::LoadDefaultResources()
{
    // 기본 스피어/플레인
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshFinder(TEXT("/Engine/BasicShapes/Sphere"));
    if (SphereMeshFinder.Succeeded())
    {
        GridPointStaticMesh = SphereMeshFinder.Object;
        FlowDotStaticMesh = SphereMeshFinder.Object;
        if (GridPointMesh) GridPointMesh->SetStaticMesh(GridPointStaticMesh);
        if (FlowDotMesh)   FlowDotMesh->SetStaticMesh(FlowDotStaticMesh);

    }

   // static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshFinder(TEXT("/Engine/BasicShapes/Plane"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshFinder(TEXT("/Game/1_ParkGolf/gridline/line.line"));
    if (PlaneMeshFinder.Succeeded())
    {
        GridLineStaticMesh = PlaneMeshFinder.Object;
        if (GridLineMesh) GridLineMesh->SetStaticMesh(GridLineStaticMesh);
    }

    // 기본 머티리얼 (정적 할당)
    static ConstructorHelpers::FObjectFinder<UMaterial> GridLineMatFinder(TEXT("/Game/1_ParkGolf/gridline/M_GridLine.M_GridLine"));
    if (GridLineMatFinder.Succeeded())
    {
        GridLineMaterial = GridLineMatFinder.Object;
        if (GridLineMesh && GridLineMaterial) GridLineMesh->SetMaterial(0, GridLineMaterial);
    }

    static ConstructorHelpers::FObjectFinder<UMaterial> GridPointMatFinder(TEXT("/Game/1_ParkGolf/gridline/M_FlowDot.M_FlowDot"));
    if (GridPointMatFinder.Succeeded())
    {
        GridPointMaterial = GridPointMatFinder.Object;
        if (GridPointMesh && GridPointMaterial) GridPointMesh->SetMaterial(0, GridPointMaterial);
    }

    static ConstructorHelpers::FObjectFinder<UMaterial> FlowDotMatFinder(TEXT("/Game/1_ParkGolf/gridline/M_FlowDot.M_FlowDot"));
    if (FlowDotMatFinder.Succeeded())
    {
        FlowDotMaterial = FlowDotMatFinder.Object;

        //UMaterialInstanceDynamic* DynamicFlowDotMat = UMaterialInstanceDynamic::Create(FlowDotMaterial, this);
        //DynamicFlowDotMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
        //FlowDotMesh->SetMaterial(0, DynamicFlowDotMat);

       if (FlowDotMesh && FlowDotMaterial) FlowDotMesh->SetMaterial(0, FlowDotMaterial);
    }
}

void ATerrainHeightGrid::BeginPlay()
{
    Super::BeginPlay();

    // Landscape 캐시(있으면 하나 잡아둠)
    for (TActorIterator<ALandscape> It(GetWorld()); It; ++It)
    {
        CachedLandscape = *It;
        break;
    }
}

void ATerrainHeightGrid::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 흐름 점 이동
    UpdateFlowMovers(DeltaTime);

    if (ShouldUpdateGrid())
    {
        LastUpdateTime = GetWorld()->GetTimeSeconds();
        if (bShowDebugInfo && bGridGenerated)
        {
            // DrawDebugInfo();
        }
        if (bShowFlowVectors && bGridGenerated)
        {
            // DrawFlowVectors();
        }
    }
}

ATerrainHeightGrid* ATerrainHeightGrid::GetOrCreateTerrainGrid(UWorld* World)
{
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("GetOrCreateTerrainGrid: World is null"));
        return nullptr;
    }

    for (TActorIterator<ATerrainHeightGrid> It(World); It; ++It)
    {
        if (IsValid(*It))
        {
            UE_LOG(LogTemp, Log, TEXT("GetOrCreateTerrainGrid: Found existing"));
            return *It;
        }
    }

    ATerrainHeightGrid* NewGrid = World->SpawnActor<ATerrainHeightGrid>(
        ATerrainHeightGrid::StaticClass(),
        FVector::ZeroVector, FRotator::ZeroRotator);

    if (!NewGrid)
    {
        UE_LOG(LogTemp, Error, TEXT("GetOrCreateTerrainGrid: Spawn failed"));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("GetOrCreateTerrainGrid: Spawned new"));
    }
    return NewGrid;
}

bool ATerrainHeightGrid::ShouldUpdateGrid() const
{
    const float CurrentTime = GetWorld()->GetTimeSeconds();
    return (CurrentTime - LastUpdateTime) >= UpdateFrequency;
}

void ATerrainHeightGrid::GenerateGrid(const FVector& CenterPosition)
{
    if (!GridPointMesh || !GridLineMesh || !FlowDotMesh)
        return;

    CurrentCenterPosition = CenterPosition;

    GridPoints.Empty();
    GridPointTransforms.Empty();
    GridLineTransforms.Empty();
    ClearFlowMovers();

    GenerateGridPoints(CenterPosition);
    CalculateHeightsAndSlopes();
    CalculateFlowDirections();
    UpdateInstanceTransforms();   // 포인트/라인 인스턴스 생성 (Landscape 위만)
    BuildFlowMovers();            // 선분 양끝이 Landscape 위일 때만 플로우 점 생성
    UpdateHeightStatistics();

    bGridGenerated = true;

    //OnGridGenerated(GridPoints.Num());
    //OnHeightDataUpdated(MinHeightInGrid, MaxHeightInGrid, AverageHeightInGrid);
}

void ATerrainHeightGrid::GenerateGridPoints(const FVector& CenterPos)
{
    GridPoints.Empty();


    // ⭐ 명확한 로그: 어떤 공 기준인지 출력
    if (CurrentTrackedBall)
    {
        UE_LOG(LogTemp, Log, TEXT("📍 GenerateGridPoints: Ball #%d, CenterPos [%.0f, %.0f, %.0f]"),
            CurrentBallIndex, CenterPos.X, CenterPos.Y, CenterPos.Z);
    }

    // 격자 방향: TargetPosition 기준 전방/우측
    FVector Forward = (TargetPosition - CenterPos).GetSafeNormal();
    if (Forward.IsNearlyZero()) Forward = FVector(1, 0, 0);
    FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();

    // 길이/폭 (cm)
    const float DistanceToHolecup = FVector::Dist(CenterPos, HolecupPosition) * 1.5f ;
    const float GridLength = FMath::Min(DistanceToHolecup, MAX_GRID_LENGTH);

    // 스텝 수
    const float SpacingCm = GridSpacing;
    int32 LengthSteps = FMath::Max(1, FMath::CeilToInt(GridLength / SpacingCm));
    int32 WidthSteps = FMath::Max(1, FMath::CeilToInt(GRID_WIDTH / SpacingCm));

    int32 TotalPoints = LengthSteps * WidthSteps;
    if (TotalPoints > MaxGridPoints)
    {
        const float ScaleFactor = FMath::Sqrt((float)MaxGridPoints / TotalPoints);
        LengthSteps = FMath::Max(1, FMath::FloorToInt(LengthSteps * ScaleFactor));
        WidthSteps = FMath::Max(1, FMath::FloorToInt(WidthSteps * ScaleFactor));
    }

    const FVector StartPos = CenterPos - (Right * (GRID_WIDTH * 0.5f));
    for (int32 Y = 0; Y < LengthSteps; ++Y)
    {
        for (int32 X = 0; X < WidthSteps; ++X)
        {
            FVector P = StartPos + (Forward * (Y * SpacingCm)) + (Right * (X * SpacingCm));

            float HeightZ = P.Z;
            bool  bHitLandscape = SampleHeightAtLocation(P, HeightZ);

            // 오직 랜드스케이프 위 포인트만 유효 처리
            if (bHitLandscape)
            {
                P.Z = HeightZ + HEIGHT_OFFSET;
                GridPoints.Emplace(P, HeightZ, /*bOnLandscape=*/true);
            }
            else
            {
                UE_LOG(LogTemp, Log, TEXT("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX  FIND Z Fail  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"));
                // 인덱스 일관성을 위해 무효 포인트도 보관(bOnLandscape=false)
                P.Z = HeightZ + HEIGHT_OFFSET;
                GridPoints.Emplace(P, HeightZ, /*bOnLandscape=*/false);
            }
        }
    }
}

bool ATerrainHeightGrid::SampleHeightAtLocation(const FVector& Location, float& OutZ)
{
    UWorld* World = GetWorld();
    if (!World) { OutZ = Location.Z; return false; }

    const FVector TraceStart = Location + FVector(0, 0, 10000.f);
    const FVector TraceEnd = Location - FVector(0, 0, 10000.f);

    FCollisionQueryParams Params(SCENE_QUERY_STAT(TerrainHeightTrace), /*bTraceComplex=*/false);
    Params.AddIgnoredActor(this);
    Params.bReturnPhysicalMaterial = false;

    if (TraceIgnoreActors.Num() > 0)
    {
        TraceIgnoreActors.RemoveAll([](AActor* A) { return A == nullptr; });
        Params.AddIgnoredActors(TraceIgnoreActors);
    }

    // Cup_hole 식별: 이름이 "Cup_hole"로 시작하고(접미 숫자는 옵션) → 유효 표면으로 인정
    auto IsCupHoleActor = [](const AActor* Actor) -> bool
    {
        if (!Actor) return false;
        const FString& Name = Actor->GetActorNameOrLabel(); // 예: "Cup_hole12"

        UE_LOG(LogTemp, Log, TEXT("------------------>>>>>>>>>>>>ISHOLECUP CHECK ACTOR NAME -[%s]"), *Name);

        // Cup_hole로 시작하면 무조건 유효한 표면으로 인정하도록 수정
       if (Name.Contains(TEXT("green_hole"), ESearchCase::IgnoreCase)) return true;

        return false;
    };

    // ★ 추가: Landphysic 이름을 가진 StaticMeshActor 식별
    auto IsLandphysicActor = [](const AActor* Actor) -> bool
    {
        if (!Actor) return false;

        // StaticMeshActor인지 확인
        if (!Actor->IsA<AStaticMeshActor>()) return false;

        // 이름에 "Landphysic"이 포함되어 있는지 확인
        const FString& Name = Actor->GetName();
        if (Name.Contains(TEXT("landphysic")) || Name.Contains(TEXT("Landphysic")))
            return true;
        else
            return false;
    };


    TArray<FHitResult> Hits;
    World->LineTraceMultiByChannel(Hits, TraceStart, TraceEnd, ECC_Visibility, Params);

    for (const FHitResult& Hit : Hits)
    {
        const AActor* A = Hit.GetActor();
        if (!A) continue;

        // 1) Cup_hole%d 액터 먼저 체크
        if (IsCupHoleActor(A))
        {          

            UE_LOG(LogTemp, Log, TEXT("------------------------- FIND IS Cup_hole--- Z value  %f  --- HolecubPosition -[%f]"), Hit.ImpactPoint.Z, HolecupPosition.Z);
           // OutZ = Hit.ImpactPoint.Z - 1.0f;
            OutZ = HolecupPosition.Z  - 3.0f;
            return true;
        }

        // 2) ★ 추가: Landphysic StaticMeshActor 체크
        if (IsLandphysicActor(A))
        {
            OutZ = Hit.ImpactPoint.Z;
            return true;
        }

        // 3) Landscape / LandscapeProxy
        if (A->IsA<ALandscape>() || A->IsA<ALandscapeProxy>())
        {
            OutZ = Hit.ImpactPoint.Z;
            return true;
        }


    }

    // 유효 표면을 못 찾으면 실패
    OutZ = Location.Z;
    return false;
}

void ATerrainHeightGrid::UpdateInstanceTransforms()
{
    if (!GridPointMesh || !GridLineMesh || !FlowDotMesh) return;

    GridPointMesh->ClearInstances();
    GridLineMesh->ClearInstances();
    FlowDotMesh->ClearInstances();

    GridPointTransforms.Empty();
    GridLineTransforms.Empty();

    // 포인트(랜드스케이프 위 포인트만)
    for (const FGridPoint& Pt : GridPoints)
    {
        if (!Pt.bOnLandscape) continue;

        float SizeMul = FMath::Clamp(1.0f + (Pt.Slope * 2.0f), 0.5f, 3.0f);
        const FVector Scale = FVector((GridPointSize * SizeMul) / 100.f);
        const FTransform T(FRotator::ZeroRotator, Pt.WorldPosition, Scale);
        GridPointTransforms.Add(T);
        GridPointMesh->AddInstance(T);
    }

    const float SpacingCm = GridSpacing;
    const int32 WidthSteps = (GridPoints.Num() > 0) ? FMath::Max(1, FMath::CeilToInt(GRID_WIDTH / SpacingCm)) : 0;
    const int32 LengthSteps = (GridPoints.Num() > 0 && WidthSteps > 0) ? (GridPoints.Num() / WidthSteps) : 0;

    // ★ 라인 인스턴스 생성 + 세그먼트별 흐름 속도(CustomData 0) 기록
    //   - 경사가 MinSlopeForFlow 미만이면 정지 (평지/경사와 수직인 라인은 흐르지 않음)
    //   - 데드존 경계에서 속도가 튀지 않게 2배 지점까지 부드럽게 램프
    //   - 부호: S가 높은 내리막(S→E 흐름)이면 UV offset은 음수
    auto AddLineInstance = [&](const FGridPoint& S, const FGridPoint& E, float ThicknessCm)
        {
            const FVector SP = S.WorldPosition;
            const FVector EP = E.WorldPosition;
            const FVector C = (SP + EP) * 0.5f;
            const FVector Dir = (EP - SP).GetSafeNormal();
            const float   Len = FVector::Dist(SP, EP);
            const FRotator Rot = Dir.Rotation();

            // ★ line 메시는 X=50cm(±25) 기준. Scale 1.0 = 50cm.
            //   길이 방향: 세그먼트 길이를 메시 원본 길이(50)로 나눔
            //   두께 방향: 원하는 두께(cm)를 메시 원본 폭(50)으로 나눔
            //   Z: 평면 메시이므로 1.0 (절대 0.01 쓰지 말 것)
            constexpr float MeshLenCm = 50.0f; // line 메시 X 실치수
            constexpr float MeshWidthCm = 2.0f; // line 메시 Y 실치수 (정사각 25,25 기준)

            const FVector Scale(Len / MeshLenCm, ThicknessCm / MeshWidthCm, 1.0f);
            const int32 InstIdx = GridLineMesh->AddInstance(FTransform(Rot, C, Scale));

            const float HeightDiff = S.Height - E.Height;
            const float SegSlope = (Len > 0.1f) ? FMath::Abs(HeightDiff) / Len : 0.f;

            float FlowValue = 0.f;
            if (SegSlope >= WaterFlowSettings.MinSlopeForFlow)
            {
                const float Ramp = FMath::Clamp(
                    (SegSlope - WaterFlowSettings.MinSlopeForFlow) / WaterFlowSettings.MinSlopeForFlow,
                    0.f, 1.f);
                const float Speed = CalcSegmentFlowSpeed(S, E) * GridLineFlowSpeedScale * Ramp;
                const float Sign = (HeightDiff >= 0.f) ? +1.f : -1.f;
                FlowValue = Sign * Speed;
            }

            GridLineMesh->SetCustomDataValue(InstIdx, 0, FlowValue, /*bMarkRenderStateDirty=*/false);
        };

    for (int32 Y = 0; Y < LengthSteps; ++Y)
    {
        for (int32 X = 0; X < WidthSteps; ++X)
        {
            const int32 Index = Y * WidthSteps + X;
            if (!GridPoints.IsValidIndex(Index)) continue;
            const FGridPoint& Cur = GridPoints[Index];
            if (!Cur.bOnLandscape) continue;

            // ----- 가로 줄(좌↔우 이웃): 두께 2배 -----
            if (X < WidthSteps - 1)
            {
                const int32 R = Index + 1;
                if (GridPoints.IsValidIndex(R) && GridPoints[R].bOnLandscape)
                {
                    AddLineInstance(Cur, GridPoints[R], GridLineThickness * 4.0f);
                }
            }

            // ----- 세로 줄(앞 이웃): 기존 두께 유지 -----
            if (Y < LengthSteps - 1)
            {
                const int32 Fwd = (Y + 1) * WidthSteps + X;
                if (GridPoints.IsValidIndex(Fwd) && GridPoints[Fwd].bOnLandscape)
                {
                    AddLineInstance(Cur, GridPoints[Fwd], GridLineThickness * 4.0f);
                }
            }
        }
    }

    // ★ 루프 안에서는 dirty 마킹을 미루고, 마지막에 한 번만
    GridLineMesh->MarkRenderStateDirty();

    UE_LOG(LogTemp, Warning, TEXT("🟩 GridLine 인스턴스 수=%d, Steps(W=%d,L=%d), Mesh=%s, Mat=%s"),
        GridLineMesh->GetInstanceCount(),
        WidthSteps, LengthSteps,
        *GetNameSafe(GridLineMesh->GetStaticMesh()),
        *GetNameSafe(GridLineMesh->GetMaterial(0)));
}

void ATerrainHeightGrid::BuildFlowMovers()
{
    if (!FlowDotMesh) return;

    FlowMovers.Empty();
    FlowDotMesh->ClearInstances();

    // ★ 추가: 꺼져 있으면 Mover/인스턴스 생성 안 함
    if (!bShowFlowDots) return;

    const float SpacingCm = GridSpacing;
    const int32 WidthSteps = (GridPoints.Num() > 0) ? FMath::Max(1, FMath::CeilToInt(GRID_WIDTH / SpacingCm)) : 0;
    const int32 LengthSteps = (GridPoints.Num() > 0 && WidthSteps > 0) ? (GridPoints.Num() / WidthSteps) : 0;

    auto AddMoverForPair = [&](int32 A, int32 B)
    {
        if (!GridPoints.IsValidIndex(A) || !GridPoints.IsValidIndex(B)) return;

        const FGridPoint& P0 = GridPoints[A];
        const FGridPoint& P1 = GridPoints[B];

        // 양 끝이 모두 랜드스케이프 위가 아니면 SKIP
        if (!P0.bOnLandscape || !P1.bOnLandscape) return;

        const bool bAisHigh = (P0.Height >= P1.Height);
        const int32 High = bAisHigh ? A : B;
        const int32 Low = bAisHigh ? B : A;

        const FVector HP = GridPoints[High].WorldPosition;
        const FVector LP = GridPoints[Low].WorldPosition;

        FFlowMover M;
        M.HighIdx = High;
        M.LowIdx = Low;
        M.P0 = HP;
        M.P1 = LP;
        M.T = 0.5f; // 중간에서 시작

        M.LengthCm = FVector::Dist(HP, LP);
        const float HeightDiff = FMath::Max(0.f, GridPoints[High].Height - GridPoints[Low].Height); // cm

        // ★ 기울기(Slope) 계산: 높이 차이를 거리로 나눔
        const float Slope = (M.LengthCm > 0.1f) ? HeightDiff / M.LengthCm : 0.f;

        // ★ 수정: 작은 높이차에서도 천천히 흐르게 위해 최소 속도 보장 (기존 조건 제거)
        const float LenM = FMath::Max(1.f, M.LengthCm / 100.f);
        float BaseT = WaterFlowSettings.BaseSpeedPerMeter / LenM;
        float AccelT = WaterFlowSettings.HeightAccelScale * (HeightDiff / FMath::Max(1.f, M.LengthCm));

        // ★ 수정: 기울기에 따라 속도 스케일링 (기울기 클수록 더 빠르게, 최대 5배로 확대)
        const float SlopeScale = FMath::Clamp(Slope * 20.f, 0.5f, 5.f); // 최소 0.5배 ~ 최대 5배 (작은 차이 천천히, 큰 차이 빨리)
        M.SpeedTPerSec = (BaseT + AccelT) * FMath::Max(0.01f, WaterFlowSettings.FlowSpeed) * SlopeScale;

        const float DotScaleCm = FMath::Clamp(WaterFlowSettings.DotSizeCm, 4.f, 40.f);
        const FVector StartPos = FMath::Lerp(HP, LP, 0.5f); // 초기 위치를 중간으로 설정
        const FTransform DotXF(FRotator::ZeroRotator, StartPos, FVector(DotScaleCm / 100.f));
        const int32 NewIdx = FlowDotMesh->AddInstance(DotXF);
        M.DotInstanceIndex = NewIdx;

        FlowMovers.Add(M);
    };

    for (int32 Y = 0; Y < LengthSteps; ++Y)
    {
        for (int32 X = 0; X < WidthSteps; ++X)
        {
            const int32 I = Y * WidthSteps + X;
            if (!GridPoints.IsValidIndex(I)) continue;

            // 오른쪽
            if (X < WidthSteps - 1)
            {
                AddMoverForPair(I, I + 1);
            }
            // 앞쪽
            if (Y < LengthSteps - 1)
            {
                AddMoverForPair(I, (Y + 1) * WidthSteps + X);
            }
        }
    }
}

void ATerrainHeightGrid::ClearFlowMovers()
{
    FlowMovers.Empty();
    if (FlowDotMesh) FlowDotMesh->ClearInstances();
}

void ATerrainHeightGrid::UpdateFlowMovers(float DeltaTime)
{
    if (!FlowDotMesh || FlowMovers.Num() == 0) return;
        if (!bShowFlowDots) return;   // ★ 추가



    for (FFlowMover& M : FlowMovers)
    {
        if (M.DotInstanceIndex == INDEX_NONE) continue;

        // 진행
        M.T += M.SpeedTPerSec * DeltaTime;
        if (M.T >= 1.0f)
        {
            M.T = 0.0f; // 루프
        }

        const FVector Pos = FMath::Lerp(M.P0, M.P1, M.T);

        // ★ 수정: 구체 크기를 절반으로 줄임
        const float OriginalDotSize = FMath::Clamp(WaterFlowSettings.DotSizeCm, 4.f, 40.f);
        const float DotScaleCm = OriginalDotSize * 0.5f; // 절반 크기로 감소

        const FTransform NewXF(FRotator::ZeroRotator, Pos, FVector(DotScaleCm / 100.f));

        // 로컬 스페이스 기준 업데이트
        FlowDotMesh->UpdateInstanceTransform(M.DotInstanceIndex, NewXF, false /*bWorldSpace*/, true /*markDirty*/, true /*teleport*/);
    }

    FlowDotMesh->MarkRenderStateDirty();
}

void ATerrainHeightGrid::SetHolecupPosition(const FVector& HolecupPos)
{
    HolecupPosition = HolecupPos;
}

void ATerrainHeightGrid::CalculateHeightsAndSlopes()
{
    for (int32 i = 0; i < GridPoints.Num(); ++i)
    {
        FGridPoint& P = GridPoints[i];

        if (!P.bOnLandscape)
        {
            // 무효 포인트는 기본값 유지
            P.Slope = 0.f;
            P.FlowDirection = FVector::ZeroVector;
            P.HeightColor = FLinearColor::Black;
            continue;
        }

        const TArray<FGridPoint> Near = GetNearbyGridPoints(P, GridSpacing * 1.5f);
        P.Slope = CalculateSlope(P, Near);
        P.HeightColor = CalculateHeightColor(P.Height);
    }
}

void ATerrainHeightGrid::CalculateFlowDirections()
{
    for (int32 i = 0; i < GridPoints.Num(); ++i)
    {
        FGridPoint& P = GridPoints[i];
        if (!P.bOnLandscape)
        {
            P.FlowDirection = FVector::ZeroVector;
            continue;
        }

        const TArray<FGridPoint> Near = GetNearbyGridPoints(P, GridSpacing * 2.0f);
        P.FlowDirection = CalculateFlowDirection(P, Near);
    }
}

FVector ATerrainHeightGrid::CalculateFlowDirection(const FGridPoint& Point, const TArray<FGridPoint>& NearbyPoints)
{
    FVector FlowDir = FVector::ZeroVector;
    float Lowest = Point.Height;
    FVector LowPos = Point.WorldPosition;

    for (const FGridPoint& N : NearbyPoints)
    {
        if (!N.bOnLandscape) continue;
        if (N.Height < Lowest)
        {
            Lowest = N.Height;
            LowPos = N.WorldPosition;
        }
    }

    if (Lowest < Point.Height - MIN_HEIGHT_DIFFERENCE)
    {
        FlowDir = (LowPos - Point.WorldPosition).GetSafeNormal();
        FlowDir.Z = 0.f;
        const float H = Point.Height - Lowest;
        const float Strength = FMath::Clamp(H / 100.f, 0.f, 1.f);
        FlowDir *= Strength;
    }
    return FlowDir;
}

float ATerrainHeightGrid::CalculateSlope(const FGridPoint& Point, const TArray<FGridPoint>& NearbyPoints)
{
    if (NearbyPoints.Num() < 2) return 0.f;

    float MaxSlope = 0.f;
    for (const FGridPoint& N : NearbyPoints)
    {
        if (!N.bOnLandscape) continue;
        const float Dist = FVector::Dist2D(Point.WorldPosition, N.WorldPosition);
        if (Dist > 0.1f)
        {
            const float Slope = FMath::Abs(Point.Height - N.Height) / Dist;
            MaxSlope = FMath::Max(MaxSlope, Slope);
        }
    }
    return MaxSlope;
}

TArray<FGridPoint> ATerrainHeightGrid::GetNearbyGridPoints(const FGridPoint& CenterPoint, float SearchRadius)
{
    TArray<FGridPoint> Out;
    for (const FGridPoint& O : GridPoints)
    {
        if (&O == &CenterPoint) continue;
        if (!O.bOnLandscape) continue;
        if (FVector::Dist2D(CenterPoint.WorldPosition, O.WorldPosition) <= SearchRadius)
        {
            Out.Add(O);
            if (Out.Num() >= MAX_NEARBY_POINTS) break;
        }
    }
    return Out;
}

FLinearColor ATerrainHeightGrid::CalculateHeightColor(float Height)
{
    float Norm = 0.5f;
    if (MaxHeightInGrid > MinHeightInGrid)
    {
        Norm = (Height - MinHeightInGrid) / (MaxHeightInGrid - MinHeightInGrid);
        Norm = FMath::Clamp(Norm, 0.f, 1.f);
    }
    return InterpolateHeightColor(Norm);
}

FLinearColor ATerrainHeightGrid::InterpolateHeightColor(float NormalizedHeight)
{
    const auto& C = HeightColorSettings;
    if (NormalizedHeight <= C.LowToMidThreshold)
    {
        const float t = NormalizedHeight / C.LowToMidThreshold;
        return FMath::Lerp(C.LowHeightColor, C.MidHeightColor, t);
    }
    else if (NormalizedHeight <= C.MidToHighThreshold)
    {
        const float t = (NormalizedHeight - C.LowToMidThreshold) / (C.MidToHighThreshold - C.LowToMidThreshold);
        return FMath::Lerp(C.MidHeightColor, C.HighHeightColor, t);
    }
    return C.HighHeightColor;
}

void ATerrainHeightGrid::UpdateHeightStatistics()
{
    // 랜드스케이프 위 포인트만 통계에 반영
    TArray<float> Heights;
    Heights.Reserve(GridPoints.Num());
    for (const FGridPoint& P : GridPoints)
    {
        if (P.bOnLandscape)
        {
            Heights.Add(P.Height);
        }
    }

    if (Heights.Num() == 0)
    {
        MinHeightInGrid = MaxHeightInGrid = AverageHeightInGrid = 0.f;
        return;
    }

    MinHeightInGrid = Heights[0];
    MaxHeightInGrid = Heights[0];
    double Sum = 0.0;

    for (float H : Heights)
    {
        MinHeightInGrid = FMath::Min(MinHeightInGrid, H);
        MaxHeightInGrid = FMath::Max(MaxHeightInGrid, H);
        Sum += H;
    }

    AverageHeightInGrid = (float)(Sum / Heights.Num());
}

void ATerrainHeightGrid::UpdateGrid(const FVector& NewCenterPosition)
{
    //const float MoveDist = FVector::Dist(CurrentCenterPosition, NewCenterPosition);
    //if (MoveDist > (GridSpacing * 2.f))
    //{
    //    GenerateGrid(NewCenterPosition);
    //}
    //else
    //{
    //    UE_LOG(LogTemp, Log, TEXT("🌍 ----- UpdateGrid Fail  reFresh  --"));
    //}

    GenerateGrid(NewCenterPosition);
}

void ATerrainHeightGrid::SetGridVisible(bool bVisible)
{
    if (GridPointMesh) GridPointMesh->SetVisibility(bVisible);
    if (GridLineMesh)  GridLineMesh->SetVisibility(bVisible);
    if (FlowDotMesh)   FlowDotMesh->SetVisibility(bVisible && bShowFlowDots);  // ★ 수정
}

void ATerrainHeightGrid::RefreshHeightColors()
{
    for (FGridPoint& P : GridPoints)
    {
        if (!P.bOnLandscape) continue;
        P.HeightColor = CalculateHeightColor(P.Height);
    }
}

void ATerrainHeightGrid::SetGridRadius(float NewRadius)
{
    NewRadius = FMath::Clamp(NewRadius, 5.f, 200.f); // m
    if (FMath::Abs(GridRadius - NewRadius) > KINDA_SMALL_NUMBER)
    {
        GridRadius = NewRadius;
        if (bGridGenerated) GenerateGrid(CurrentCenterPosition);
    }
}

void ATerrainHeightGrid::DrawDebugInfo()
{
    if (!GetWorld() || !bShowDebugInfo) return;

    DrawDebugSphere(GetWorld(), CurrentCenterPosition, 50.f, 12, FColor::Yellow, false, UpdateFrequency);
    DrawDebugCircle(GetWorld(), CurrentCenterPosition, GridRadius * 100.f, 32, FColor::Cyan, false, UpdateFrequency);
}

void ATerrainHeightGrid::DrawFlowVectors()
{
    if (!GetWorld() || !bShowFlowVectors) return;

    for (const FGridPoint& Point : GridPoints)
    {
        if (!Point.bOnLandscape) continue;

        if (!Point.FlowDirection.IsNearlyZero() && Point.Slope > WaterFlowSettings.MinSlopeForFlow)
        {
            const FVector S = Point.WorldPosition + FVector(0, 0, 10);
            const FVector E = S + (Point.FlowDirection * FLOW_VECTOR_LENGTH);
            DrawDebugDirectionalArrow(GetWorld(), S, E, 20.f, FColor::Blue, false, UpdateFrequency, 0, 2.f);
        }
    }
}

void ATerrainHeightGrid::SetTargetPosition(const FVector& TargetPos)
{
    TargetPosition = TargetPos;
    if (bGridGenerated && IsGridVisible())
    {
        GenerateGrid(CurrentCenterPosition);
        UE_LOG(LogTemp, Log, TEXT("🔄 Target position updated to %s, grid regenerated"), *TargetPos.ToString());
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("🌍 Target position set to: %s"), *TargetPos.ToString());
    }
}

void ATerrainHeightGrid::AddTraceIgnoreActor(AActor* Actor)
{
    if (IsValid(Actor))
    {
        TraceIgnoreActors.AddUnique(Actor);
        UE_LOG(LogTemp, Verbose, TEXT("[Grid] AddTraceIgnoreActor: %s"), *Actor->GetName());
    }
}

void ATerrainHeightGrid::RemoveTraceIgnoreActor(AActor* Actor)
{
    const int32 Removed = TraceIgnoreActors.Remove(Actor);
    if (Removed > 0 && IsValid(Actor))
    {
        UE_LOG(LogTemp, Verbose, TEXT("[Grid] RemoveTraceIgnoreActor: %s"), *Actor->GetName());
    }
}

void ATerrainHeightGrid::ClearTraceIgnoreActors()
{
    TraceIgnoreActors.Reset();
    UE_LOG(LogTemp, Verbose, TEXT("[Grid] ClearTraceIgnoreActors"));
}

void ATerrainHeightGrid::UpdateRotationFromCamera()
{
    AInGameMode* GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    FRotator CameraRotation = FRotator::ZeroRotator;

    if (GM)
    {
        // 필요 시 카메라 기준 회전 반영
        // UGameplayStatics::GetPlayerController(GetWorld(), 0)->PlayerCameraManager->GetActorRotation();
    }
}

void ATerrainHeightGrid::SetCurrentBall(AGolfBall* Ball, int32 BallIndex)
{
    if (!IsValid(Ball))
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ SetCurrentBall: Ball is invalid"));
        CurrentTrackedBall = nullptr;
        CurrentBallIndex = -1;
        return;
    }

    // ⭐ 공이 변경되었을 때만 처리
    if (CurrentTrackedBall != Ball || CurrentBallIndex != BallIndex)
    {
        CurrentTrackedBall = Ball;
        CurrentBallIndex = BallIndex;

        UE_LOG(LogTemp, Log, TEXT("🎯 TerrainGrid: Tracking Ball #%d at [%.0f, %.0f, %.0f]"),
            BallIndex, Ball->GetActorLocation().X, Ball->GetActorLocation().Y, Ball->GetActorLocation().Z);

        // ⭐ 새 공 기준으로 그리드 재생성
        UpdateGrid(Ball->GetActorLocation());
    }
}
// 검증 함수
void ATerrainHeightGrid::ValidateBallTracking() const
{
    if (!CurrentTrackedBall)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ No ball is currently tracked!"));
        return;
    }

    FVector BallPos = CurrentTrackedBall->GetActorLocation();
    FVector GridCenter = CurrentCenterPosition;
    float Distance = FVector::Dist(BallPos, GridCenter);

    UE_LOG(LogTemp, Log, TEXT("✅ Ball Tracking Valid"));
    UE_LOG(LogTemp, Log, TEXT("   Ball Position: [%.0f, %.0f, %.0f]"), BallPos.X, BallPos.Y, BallPos.Z);
    UE_LOG(LogTemp, Log, TEXT("   Grid Center: [%.0f, %.0f, %.0f]"), GridCenter.X, GridCenter.Y, GridCenter.Z);
    UE_LOG(LogTemp, Log, TEXT("   Distance: %.2f cm"), Distance);
}


// ★ 기존 BuildFlowMovers의 속도 공식을 그대로 추출 (부호 없는 T/초)
float ATerrainHeightGrid::CalcSegmentFlowSpeed(const FGridPoint& A, const FGridPoint& B) const
{
    const float LengthCm = FVector::Dist(A.WorldPosition, B.WorldPosition);
    const float HeightDiff = FMath::Abs(A.Height - B.Height); // cm
    const float Slope = (LengthCm > 0.1f) ? HeightDiff / LengthCm : 0.f;

    const float LenM = FMath::Max(1.f, LengthCm / 100.f);
    const float BaseT = WaterFlowSettings.BaseSpeedPerMeter / LenM;
    const float AccelT = WaterFlowSettings.HeightAccelScale * (HeightDiff / FMath::Max(1.f, LengthCm));

    // 기울기 클수록 빠르게 (0.5배 ~ 5배)
    const float SlopeScale = FMath::Clamp(Slope * 20.f, 0.5f, 5.f);

    return (BaseT + AccelT) * FMath::Max(0.01f, WaterFlowSettings.FlowSpeed) * SlopeScale;
}