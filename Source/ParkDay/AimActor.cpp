// ============================================================================
// AimActor.cpp - Implementation of the aim point actor
// ============================================================================

#include "AimActor.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"

AAimActor::AAimActor()
{
    PrimaryActorTick.bCanEverTick = true;

    // Create and set up the mesh component (Skeletal Mesh)
    AimMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("AimMesh"));
    RootComponent = AimMesh;

    // ✅ Blueprint 클래스를 생성자에서 로드 (FClassFinder는 생성자에서만 사용 가능)
    static ConstructorHelpers::FClassFinder<AActor> AimMeshBPClass(
        TEXT("Blueprint'/Game/info_aim/aim_mesh.aim_mesh_C'"));

    if (AimMeshBPClass.Succeeded())
    {
        // Blueprint 클래스 저장 (나중에 BeginPlay에서 사용)
        AimMeshBlueprintClass = AimMeshBPClass.Class;
        UE_LOG(LogTemp, Log, TEXT("✅ AimActor: Blueprint class found in constructor"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ AimActor: Failed to load Blueprint class in constructor"));
        AimMeshBlueprintClass = nullptr;
    }

    // SkeletalMesh는 fallback이 없으므로 로드하지 않음

    // Disable collision
    AimMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Set visibility for debugging
#if WITH_EDITOR
    AimMesh->SetVisibility(bDebugMode);
#endif
}

void AAimActor::BeginPlay()
{
    Super::BeginPlay();

    // ============================================================
    // Blueprint 클래스를 현재 위치에 스폰
    // ============================================================
    if (AimMeshBlueprintClass)
    {
        UE_LOG(LogTemp, Log, TEXT("🔍 === Spawning Blueprint Instance ==="));
        UE_LOG(LogTemp, Log, TEXT("Blueprint Class: %s"), *AimMeshBlueprintClass->GetName());

        // 스폰할 위치 및 회전 설정
        FVector SpawnLocation = GetActorLocation();
        FRotator SpawnRotation = GetActorRotation();

        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;  // AimActor를 Owner로 설정
        SpawnParams.Instigator = nullptr;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        UE_LOG(LogTemp, Log, TEXT("Spawn Location: %s"), *SpawnLocation.ToString());
        UE_LOG(LogTemp, Log, TEXT("Spawn Rotation: %s"), *SpawnRotation.ToString());

        // Blueprint 인스턴스 스폰
        AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(
            AimMeshBlueprintClass,
            SpawnLocation,
            SpawnRotation,
            SpawnParams
            );

        if (SpawnedActor)
        {
            SpawnedBlueprintActor = SpawnedActor;

            UE_LOG(LogTemp, Log, TEXT("✅ Blueprint instance spawned successfully"));
            UE_LOG(LogTemp, Log, TEXT("   Actor Name: %s"), *SpawnedActor->GetName());
            UE_LOG(LogTemp, Log, TEXT("   Actor Location: %s"), *SpawnedActor->GetActorLocation().ToString());

            // ============================================================
            // Blueprint의 자식 컴포넌트 분석
            // ============================================================
            UE_LOG(LogTemp, Log, TEXT("🔹 Blueprint Component Structure:"));

            // RootComponent 확인
            UActorComponent* RootComp = SpawnedActor->GetRootComponent();
            if (RootComp)
            {
                UE_LOG(LogTemp, Log, TEXT("   RootComponent: %s (Class: %s)"),
                    *RootComp->GetName(),
                    *RootComp->GetClass()->GetName());
            }

            // 모든 컴포넌트 나열
            const TSet<UActorComponent*>& AllComponents = SpawnedActor->GetComponents();
            UE_LOG(LogTemp, Log, TEXT("   Total Components: %d"), AllComponents.Num());

            int32 SkeletalMeshCount = 0;
            for (UActorComponent* Component : AllComponents)
            {
                if (Component)
                {
                    UE_LOG(LogTemp, Log, TEXT("      📦 %s (Class: %s)"),
                        *Component->GetName(),
                        *Component->GetClass()->GetName());

                    if (USkeletalMeshComponent* SkeletalMesh = Cast<USkeletalMeshComponent>(Component))
                    {
                        SkeletalMeshCount++;
                        UE_LOG(LogTemp, Log, TEXT("         ✓ SkeletalMeshComponent #%d"), SkeletalMeshCount);
                        if (SkeletalMesh->SkeletalMesh)
                        {
                            UE_LOG(LogTemp, Log, TEXT("           Mesh: %s"),
                                *SkeletalMesh->SkeletalMesh->GetName());
                        }
                    }
                }
            }

            UE_LOG(LogTemp, Log, TEXT("✅ Blueprint spawned with %d SkeletalMeshComponents"), SkeletalMeshCount);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("❌ Failed to spawn Blueprint instance"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ AimMeshBlueprintClass is not loaded"));
    }

    // ============================================================
    // AimMesh 컴포넌트는 숨김 (Blueprint 인스턴스가 표시됨)
    // ============================================================
    if (AimMesh)
    {
        AimMesh->SetVisibility(false);
        AimMesh->SetHiddenInGame(true);
    }

    // ============================================================
    // Final Status
    // ============================================================
    if (SpawnedBlueprintActor)
    {
        UE_LOG(LogTemp, Log, TEXT("✅ AimActor Final Status:"));
        UE_LOG(LogTemp, Log, TEXT("   Mode: Blueprint Instance (Full Hierarchy)"));
        UE_LOG(LogTemp, Log, TEXT("   Instance: %s"), *SpawnedBlueprintActor->GetName());
        UE_LOG(LogTemp, Log, TEXT("   Location: %s"), *SpawnedBlueprintActor->GetActorLocation().ToString());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ AimActor: No Blueprint instance spawned!"));
    }
}

void AAimActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);



//#if WITH_EDITOR
//    // Draw debug sphere at aim position
//    if (bDebugMode)
//    {
//        UWorld* World = GetWorld();
//        if (World)
//        {
//            FVector Location = GetActorLocation();
//
//            // Draw a debug sphere
//            DrawDebugSphere(
//                World,
//                Location,
//                25.0f,              // Radius
//                12,                 // Segments
//                FColor::Red,        // Color
//                false,              // Persistent lines
//                0.0f,               // Lifetime (0 = one frame)
//                0,                  // Depth priority
//                2.0f                // Thickness
//            );
//
//            // Optional: Draw debug string with coordinates
//            DrawDebugString(
//                World,
//                Location + FVector(0, 0, 50),
//                FString::Printf(TEXT("Aim: %.1f, %.1f, %.1f"), Location.X, Location.Y, Location.Z),
//                nullptr,
//                FColor::Yellow,
//                0.0f
//            );
//        }
//    }
//#endif
}

void AAimActor::SetAimPosition(const FVector& Origin, const FVector& Direction, float Distance)
{
    // Calculate aim position based on origin, direction, and distance
    FVector AimLocation = Origin + (Direction.GetSafeNormal() * Distance);
    SetActorLocation(AimLocation);

    // Blueprint 인스턴스도 함께 이동 (DefaultSceneRoot + 자식 컴포넌트들)
    if (SpawnedBlueprintActor)
    {
        SpawnedBlueprintActor->SetActorLocation(AimLocation);

        UE_LOG(LogTemp, VeryVerbose, TEXT("🎯 Blueprint actor moved to: %s"), *AimLocation.ToString());
    }

    // On-screen debug message
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            -1,
            5.0f,
            FColor::Green,
            FString::Printf(TEXT("Aim Position Set: %s"), *AimLocation.ToString())
        );
    }
}

void AAimActor::SetAimLocation(const FVector& NewLocation)
{
    SetActorLocation(NewLocation);

    // Blueprint 인스턴스도 함께 이동 (DefaultSceneRoot + 자식 컴포넌트들)
    if (SpawnedBlueprintActor)
    {
        SpawnedBlueprintActor->SetActorLocation(NewLocation);

        UE_LOG(LogTemp, VeryVerbose, TEXT("🎯 Blueprint actor moved to: %s"), *NewLocation.ToString());
    }

    // On-screen debug message
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            -1,
            5.0f,
            FColor::Green,
            FString::Printf(TEXT("Aim Location Set: %s"), *NewLocation.ToString())
        );
    }
}

void AAimActor::SetAimVisibility(bool bVisible)
{
    // Blueprint 인스턴스의 전체 hierarchy 가시성 제어
    if (SpawnedBlueprintActor)
    {
        SpawnedBlueprintActor->SetActorHiddenInGame(!bVisible);

        UE_LOG(LogTemp, Log, TEXT("🔍 Blueprint Actor Visibility: %s"),
            bVisible ? TEXT("VISIBLE") : TEXT("HIDDEN"));
        UE_LOG(LogTemp, Log, TEXT("   Instance: %s"), *SpawnedBlueprintActor->GetName());
        UE_LOG(LogTemp, Log, TEXT("   Location: %s"), *SpawnedBlueprintActor->GetActorLocation().ToString());
    }
}

void AAimActor::SetDebugMode(bool bEnabled)
{
    bDebugMode = bEnabled;

    // Blueprint 인스턴스 가시성 제어
    if (SpawnedBlueprintActor)
    {
        SpawnedBlueprintActor->SetActorHiddenInGame(!bEnabled);
    }

    // Enable/disable tick based on debug mode
    SetActorTickEnabled(bEnabled);
}

// ============================================================
// 거리별 스케일 조절
// ============================================================
void AAimActor::SetScaleByDistance(float Distance)
{
    if (!SpawnedBlueprintActor)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ SetScaleByDistance: SpawnedBlueprintActor is null"));
        return;
    }

    // ✅ 거리별 스케일 계산 공식
    // Distance가 커질수록 스케일이 커짐
    // 
    // 거리 범위: 100 ~ 5000 (단위: cm)
    // 스케일 범위: 0.3 ~ 1.5
    // 
    // 예시:
    // 100cm -> 0.3배
    // 500cm -> 0.5배
    // 1000cm -> 0.7배
    // 2500cm -> 1.0배 (기본)
    // 5000cm -> 1.5배

    const float MinDistance = 100.0f;     // 최소 거리
    const float MaxDistance = 5000.0f;    // 최대 거리
    const float MinScale = 0.3f;          // 최소 스케일
    const float MaxScale = 1.5f;          // 최대 스케일

    // 거리를 범위 내로 클램프
    float ClampedDistance = FMath::Clamp(Distance, MinDistance, MaxDistance);

    // 정규화: 0.0 ~ 1.0 범위
    float NormalizedDistance = (ClampedDistance - MinDistance) / (MaxDistance - MinDistance);

    // 스케일 계산: 선형 보간
    float CalculatedScale = FMath::Lerp(MinScale, MaxScale, NormalizedDistance);
    CalculatedScale *= 0.8f;

    // Blueprint 인스턴스 스케일 설정
    FVector NewScale(CalculatedScale, CalculatedScale, CalculatedScale);
    SpawnedBlueprintActor->SetActorScale3D(NewScale);

    UE_LOG(LogTemp, Log, TEXT("🎯 SetScaleByDistance:"));
    UE_LOG(LogTemp, Log, TEXT("   Distance: %.1f cm"), Distance);
    UE_LOG(LogTemp, Log, TEXT("   Scale: %.3f"), CalculatedScale);
    UE_LOG(LogTemp, Log, TEXT("   NewScale: (%f, %f, %f)"), NewScale.X, NewScale.Y, NewScale.Z);
}