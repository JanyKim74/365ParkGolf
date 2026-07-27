// ============================================================================
// GolfBall.cpp - 파크골프 m/s + 각도 시스템 완성본 (설정 파일 시스템 포함)
// ============================================================================

#include "GolfBall.h"

#include "BallParticleManager.h"
#include "BoomLine.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "InGameMode.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Engine/StaticMeshActor.h"
#include "Landscape.h"
#include "GolfPlayer.h"
#include "GolfPlayerManager.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GolfPlayerController.h"
#include "HAL/PlatformFilemanager.h"
#include "CameraFXComponent.h"
#include "CameraManager.h"
#include "PlayerInfoSlotWidget.h"
#include "SoundManager.h"
#include "Components/BillboardComponent.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/ConstructorHelpers.h"
#include "Math/Rotator.h"
#include "CR2SensorManager.h"
#include "LandscapeSectionBoundaryDetector.h"
#include "Widgets/ShotResultWidget.h"
#include "BallDropMarkerActor.h"
#include "Utils/BallDropMarkerLibrary.h"
#include "ReadyBillboard.h"
#include "GolfPlayerController.h"
#include "ShotCinematicComponent.h"
#include "BallNamePlateComponent.h"
#include "ParkDay/Widgets/ResultWidget.h"
#include "ParkDay/Widgets/BallDistanceWidget.h"
#include "ParkDay/Widgets/InGameScoreBoardStatWidget.h"
#include "ParkDay/Widgets/InGameScoreBoardStatLineWidget.h"
#include "ParkDay/Widgets/BallNamePlateWidget.h"
#include "TourActor.h"
#include "Components/SplineComponent.h"
#include "Engine/OverlapResult.h"

//PRAGMA_DISABLE_OPTIMIZATION

// ===== UE4 호환 벡터 유효성 체크 매크로 =====
#define IS_VECTOR_VALID(Vec) (!Vec.ContainsNaN() && FMath::IsFinite(Vec.X) && FMath::IsFinite(Vec.Y) && FMath::IsFinite(Vec.Z))
#define IS_VECTOR_SAFE(Vec) (IS_VECTOR_VALID(Vec) && !Vec.IsNearlyZero())

// ============ GolfBall.cpp ============
// namespace PhysMatResolveUtil 전체 교체

namespace PhysMatResolveUtil
{
    static UPhysicalMaterial* GetPhysMatFromMaterialSlot(
        UPrimitiveComponent* Comp, int32 FaceIndex)
    {
        if (!Comp || FaceIndex == INDEX_NONE) return nullptr;

        int32 SectionIndex = INDEX_NONE;
        if (UMaterialInterface* MI = Comp->GetMaterialFromCollisionFaceIndex(
            FaceIndex, SectionIndex))
        {
            if (UPhysicalMaterial* PM = MI->GetPhysicalMaterial())
            {
                const bool bIsDefault = GEngine && (PM == GEngine->DefaultPhysMaterial);
                if (!bIsDefault) return PM;
            }
        }
        return nullptr;
    }

    // ★ 핵심 수정: Landscape(Simple Heightfield) + StaticMesh 모두 대응
    static UPhysicalMaterial* GetPhysMatByTrace(
        const FHitResult& Hit, UPrimitiveComponent* OtherComp)
    {
        if (!OtherComp) return nullptr;
        UWorld* World = OtherComp->GetWorld();
        if (!World) return nullptr;

        const FVector Start = Hit.ImpactPoint + Hit.ImpactNormal * 5.0f;
        const FVector End = Hit.ImpactPoint - Hit.ImpactNormal * 50.0f;

        // ─────────────────────────────────────────────────────
        // 1차: ECC_WorldStatic + bTraceComplex=false
        //      Landscape Heightfield는 SimpleCollision이므로
        //      bTraceComplex=true 하면 오히려 히트 안 됨
        //      ECC_Visibility는 Landscape가 Block 안 하는 경우 있음
        //      → ECC_WorldStatic이 Landscape 충돌의 정석 채널
        // ─────────────────────────────────────────────────────
        {
            FCollisionQueryParams Params(
                SCENE_QUERY_STAT(PhysMatTrace_Simple), /*bTraceComplex=*/false);
            Params.bReturnPhysicalMaterial = true;

            FHitResult SimpleHit;
            if (World->LineTraceSingleByChannel(
                SimpleHit, Start, End, ECC_WorldStatic, Params))
            {
                if (SimpleHit.PhysMaterial.IsValid())
                {
                    UPhysicalMaterial* PM = SimpleHit.PhysMaterial.Get();
                    const bool bIsDefault = GEngine && (PM == GEngine->DefaultPhysMaterial);
                    if (!bIsDefault) return PM;
                }
            }
        }

        // ─────────────────────────────────────────────────────
        // 2차: ECC_Visibility + bTraceComplex=true
        //      StaticMesh 계열 지형(Road, Bark 등)에 대한 폴백
        // ─────────────────────────────────────────────────────
        {
            FCollisionQueryParams Params(
                SCENE_QUERY_STAT(PhysMatTrace_Complex), /*bTraceComplex=*/true);
            Params.bReturnPhysicalMaterial = true;

            FHitResult ComplexHit;
            if (World->LineTraceSingleByChannel(
                ComplexHit, Start, End, ECC_Visibility, Params))
            {
                if (ComplexHit.PhysMaterial.IsValid())
                {
                    UPhysicalMaterial* PM = ComplexHit.PhysMaterial.Get();
                    const bool bIsDefault = GEngine && (PM == GEngine->DefaultPhysMaterial);
                    if (!bIsDefault) return PM;
                }
                // FaceIndex 경유 폴백
                if (UPhysicalMaterial* PM = GetPhysMatFromMaterialSlot(
                    ComplexHit.GetComponent(), ComplexHit.FaceIndex))
                    return PM;
            }
        }

        return nullptr;
    }

    UPhysicalMaterial* ResolveFromHit(
        const FHitResult& Hit,
        UPrimitiveComponent* OtherComp,
        bool bDoComplexTrace)
    {
        // 1순위: OnHit Hit.PhysMaterial (ComplexCollision 충돌 시 바로 있음)
        if (Hit.PhysMaterial.IsValid())
        {
            UPhysicalMaterial* PM = Hit.PhysMaterial.Get();
            const bool bIsDefault = GEngine && (PM == GEngine->DefaultPhysMaterial);
            if (!bIsDefault)
            {
                UE_LOG(LogTemp, Log, TEXT("✅ PhysMat [Hit]: %s"), *PM->GetName());
                return PM;
            }
        }

        // 2순위: FaceIndex → 머티리얼 슬롯 (StaticMesh Complex 충돌 시)
        if (UPhysicalMaterial* PM = GetPhysMatFromMaterialSlot(OtherComp, Hit.FaceIndex))
        {
            UE_LOG(LogTemp, Log, TEXT("✅ PhysMat [FaceIndex]: %s"), *PM->GetName());
            return PM;
        }

        // 3순위: 트레이스 재시도
        //   Simple 충돌(볼 Simple + Landscape Simple)은 위 2가지가 모두 null
        //   → ImpactPoint 기준 WorldStatic(Simple) → Visibility(Complex) 순 재트레이스
        if (bDoComplexTrace)
        {
            if (UPhysicalMaterial* PM = GetPhysMatByTrace(Hit, OtherComp))
            {
                UE_LOG(LogTemp, Log, TEXT("✅ PhysMat [Trace]: %s"), *PM->GetName());
                return PM;
            }
        }

        UE_LOG(LogTemp, Warning,
            TEXT("⚠️ PhysMat 없음: Actor=%s"),
            Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("None"));
        return nullptr;
    }
} // namespace PhysMatResolveUtil

static FVector ProjectOnPlane(const FVector& V, const FVector& PlaneNormal)
{
    return V - FVector::DotProduct(V, PlaneNormal) * PlaneNormal;
}

static bool IsGroundActor(const AActor* Actor)
{
    if (!IsValid(Actor)) return false;

    // 1순위: Actor Tag (패키징/에디터 모두 안전, 이름 변경에 영향 없음)
    if (Actor->ActorHasTag(TEXT("Ground"))) return true;

    // 2순위: Landscape 클래스 (코스 맵 호환)
    if (Actor->IsA(ALandscape::StaticClass()) ||
        Actor->IsA(ALandscapeProxy::StaticClass())) return true;

    // 3순위: 이름 폴백 — main/green 추가, sm_ 배제 규칙 미적용
    const FString N = Actor->GetActorNameOrLabel().ToLower();
    return N.StartsWith(TEXT("main")) || N.StartsWith(TEXT("green"))
        || N.Contains(TEXT("landphysic")) || N.Contains(TEXT("ground"))
        || N.Contains(TEXT("terrain")) || N.Contains(TEXT("floor"));
}


AGolfBall::AGolfBall()
{
    PrimaryActorTick.bCanEverTick = true;
    //
        //==================== 추가 : 물리 튀는 현상 보정=======================================
        // “충돌 전 속도”를 얻고 싶으면 PrePhysics가 유리합니다.
    // (OnHit은 물리 시뮬레이션 중/후에 들어오므로, Tick이 PostPhysics면 이미 튄 뒤 값을 저장할 수 있음)
    PrimaryActorTick.TickGroup = TG_PrePhysics;
    CameraFXComponent = CreateDefaultSubobject<UCameraFXComponent>(TEXT("CameraFXComponent"));

    //// ===== 메쉬 컴포넌트 생성 =====
    BallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BallMesh"));
    RootComponent = BallMesh;

    GroundMarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GroundMarkerMesh"));
    GroundMarkerMesh->SetupAttachment(RootComponent);
    GroundMarkerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GroundMarkerMesh->SetGenerateOverlapEvents(false);
    GroundMarkerMesh->SetCanEverAffectNavigation(false);
    GroundMarkerMesh->SetVisibility(false);
    //static ConstructorHelpers::FObjectFinder<UTexture2D> BillboardImage(TEXT("/Game/GolfGame/Image/ready_mark.ready_mark"));
    //if (BillboardImage.Succeeded())
    //{
    //    //레디 아이콘 (빌보드)
    //    ReadyBillboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("ReadyBillboard"));
    //    ReadyBillboard->SetupAttachment(RootComponent);
    //    ReadyBillboard->SetHiddenInGame(false);
    //    ReadyBillboard->SetVisibility(false);
    //    ReadyBillboard->SetSprite(BillboardImage.Object);
    //    ReadyBillboard->bIsScreenSizeScaled = true;  // 거리 따라 화면 크기 유지
    //    ReadyBillboard->SetDepthPriorityGroup(SDPG_Foreground);

    //}






    // ===== 파크골프볼 메쉬 설정 =====

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SimpleMeshFinder(TEXT("/Game/1_ParkGolf/ball/Pball_red"));
    if (SimpleMeshFinder.Succeeded())
    {
        SimpleBallMesh = SimpleMeshFinder.Object;
        UE_LOG(LogTemp, Log, TEXT("✅ SimpleBallMesh (Pball_red) LOADED"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ SimpleBallMesh NOT FOUND: /Game/1_ParkGolf/ball/Pball_red"));
        SimpleBallMesh = nullptr;
    }

    // Complex Mesh (Pball_red_G) - Ready/디테일용
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ComplexMeshFinder(TEXT("/Game/1_ParkGolf/ball/Pball_red_G"));
    if (ComplexMeshFinder.Succeeded())
    {
        ComplexBallMesh = ComplexMeshFinder.Object;
        UE_LOG(LogTemp, Log, TEXT("✅ ComplexBallMesh (Pball_red_G) LOADED"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ ComplexBallMesh NOT FOUND: /Game/1_ParkGolf/ball/Pball_red_G"));
        ComplexBallMesh = nullptr;
    }

    //static ConstructorHelpers::FObjectFinder<UStaticMeshComponent> BallMarkerFinder(TEXT("/Game/info_aim/b_maker/b_marker"));
    //if (BallMarkerFinder.Succeeded())
    //{
    //    GroundMarkerMesh = BallMarkerFinder.Object;
    //    UE_LOG(LogTemp, Log, TEXT("✅ GroundMarkerMesh (Pball_red_G) LOADED"));
    //}
    //else
    //{
    //    UE_LOG(LogTemp, Error, TEXT("❌ ComplexBallMesh NOT FOUND: /Game/info_aim/b_maker/b_marker"));
    //    GroundMarkerMesh = nullptr;
    //}





    if (SimpleMeshFinder.Succeeded())
    {
        BallMesh->SetStaticMesh(SimpleBallMesh);
        BallMesh->BodyInstance.bOverrideMass = true;
        BallMesh->BodyInstance.SetMassOverride(0.035f);
        BallMesh->SetGenerateOverlapEvents(true);
        // BallMesh->BodyInstance.SetContactReportForceThreshold(0.1f);  // impulse threshold 낮춰 세밀한 collision
        BallMesh->BodyInstance.SetMaxDepenetrationVelocity(50.0f);  // penetration correction 속도 제한 (갑작스러운 pop out 방지)

        BallMesh->SetLinearDamping(0.08f);   // ✅ BaseLinearDamping 기본값
        BallMesh->SetAngularDamping(0.08f);  // ✅ BaseAngularDamping 기본값

        float BallScale = 1.0f;
        BallMesh->SetWorldScale3D(FVector(BallScale, BallScale, BallScale));
        UE_LOG(LogTemp, Log, TEXT("ParkGolfBall: Custom mesh loaded successfully! Scale=%.4f"), BallScale);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to load custom park golf ball mesh, falling back to default sphere"));
        static ConstructorHelpers::FObjectFinder<UStaticMesh> FallbackSphereMesh(TEXT("/Engine/BasicShapes/Sphere"));
        if (FallbackSphereMesh.Succeeded())
        {
            BallMesh->SetStaticMesh(FallbackSphereMesh.Object);
            float BallScale = PARKGOLF_BALL_DIAMETER / (UNREAL_SPHERE_RADIUS * 2.0f);
            BallMesh->SetWorldScale3D(FVector(BallScale, BallScale, BallScale));
            UE_LOG(LogTemp, Warning, TEXT("Using fallback sphere mesh with scale=%.4f"), BallScale);
        }
    }

    // ──────────────────────────────────────
        //  Crosshair BP 클래스 로드 (생성자 안에서만!)
        // ──────────────────────────────────────
    static ConstructorHelpers::FClassFinder<AActor> CrosshairClassFinder(TEXT("/Game/2_crosshair/Crosshair"));
    if (CrosshairClassFinder.Succeeded())
    {
        CrosshairBPClass = CrosshairClassFinder.Class;
        UE_LOG(LogTemp, Log, TEXT("Crosshair BP loaded in constructor: %s"), *CrosshairBPClass->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Crosshair BP NOT FOUND: /Game/2_crosshair/Crosshair"));
        CrosshairBPClass = nullptr;
    }

    BallNamePlateComponent = CreateDefaultSubobject<UBallNamePlateComponent>(TEXT("BallNamePlateComponent"));

    FSoftClassPath WidgetClassPath(TEXT("/Game/UMG/UI/InGame/WBP_BallNamePlate.WBP_BallNamePlate_C"));

    BallNamePlateComponent->SetWidgetClass(WidgetClassPath.TryLoadClass<UUserWidget>());
    BallNamePlateComponent->SetupAttachment(RootComponent);
    BallNamePlateComponent->SetNamePlateVisible(false);

    CrosshairActor = nullptr;
    bCrosshairActive = false;
    bIsBeingDestroyed = false;

    BallMesh->SetGenerateOverlapEvents(true);

    // ===== 기본 설정 초기화 =====
    PhysicsConfig = FBallPhysicsConfig();
    ParkGolfConstants = FParkGolfConstants();
    TrajectorySettings = FTrajectorySettings();

    // ===== 물리 시스템 초기화 =====
    //InitializePhysicsSystem();
    InitializeUE4PhysicsSystem();

    // ===== 상태 초기화 =====
    CurrentBallState = EBallState::Ball_Des;
    bIsOutOfBounds = false;
    LastValidVelocity = FVector::ZeroVector;
    LastGroundContactTime = 0.0f;
    bWasInAir = false;
    TrailColor = FLinearColor::White;

    // 궤적 추적 변수 초기화
    bIsTrackingTrajectory = false;
    LastTrajectoryPointTime = 0.0f;
    TrajectoryStartTime = 0.0f;
    TrajectoryPoints.Empty();

    // ===== 이벤트 바인딩 =====
    BallMesh->OnComponentHit.AddDynamic(this, &AGolfBall::OnHit);
    BallMesh->OnComponentBeginOverlap.AddDynamic(this, &AGolfBall::OnComponentBeginOverlap);
    BallMesh->OnComponentEndOverlap.AddDynamic(this, &AGolfBall::OnComponentEndOverlap);

    // LandscapeChecker 관련 초기화
    LandscapeChecker = nullptr;
    CurrentLandType = ELandType::Rough;
    CurrentLandProperties = FLandProperties();

    // 기본 물리값 저장
    BaseFrictionWeight = 1.0f;
    BaseBounceDamping = 0.1f;
    BaseLinearDamping = 0.08f;


    // ===== 초기 가시성 설정 =====
    bBallCurrentlyVisible = false;
    bBallForceHidden = false;
    bIsFading = false;
    CurrentFadeAlpha = 0.0f;

    // ===== 초기 콜리젼 설정 =====
    bBallCollisionEnabled = false;
    OriginalCollisionType = ECollisionEnabled::NoCollision;
    OriginalCollisionProfileName = TEXT("Custom");


    // 볼을 처음에는 숨김 상태로 시작
    if (BallMesh)
    {
        BallMesh->SetVisibility(false);
        BallMesh->SetSimulatePhysics(false);
    }


    LinkedCameraManager = nullptr;

    // ⭐ Trail 설정 초기화
    TrailSettings = FBallTrailSettings();
    TrailPoints.Empty();
    LastTrailUpdateTime = 0.0f;

    bIsHoleIn = false;
    bIsConceded = false;
    ConcedeDistance = 50.0f; // 기본값

    OwningPlayerIndex = -1; // 초기화
    // ⭐ 추가: CurrentBallColor 초기화
    CurrentBallColor = FLinearColor::White; // 기본 색상 설정 (원하는 색상으로 변경 가능)

    PhysicsConfig.ForwardSpinFactor = 500.0f;  // 기본 탑스핀 강도 (조정 가능)

    LastSpeedCheckTime = 0.0f;
    LastRecordedSpeed = 0.0f;
    LowSpeedFrameCount = 0;


    // 지형 물리 설정 초기화
    TerrainPhysicsConfig = FTerrainPhysicsConfig();
    CurrentTerrainSettings = TerrainPhysicsConfig.TerrainSettings[TEXT("Rough")]; // 기본값으로 잔디 설정
    CurrentAppliedTerrain = TEXT("Rough");

    // 발사 직후 바운스 방지 초기화
    bJustLaunched = false;
    LaunchTime = 0.0f;
    LaunchGracePeriod = 0.2f;
    LaunchPosition = FVector::ZeroVector;

    // 샷 방향 화살표 표시 초기화
    bShowShotArrow = true;
    ShotArrowDuration = 5.0f;
    ShotArrowThickness = 1.0f;
    ShotArrowScale = 100.0f;
    LastShotDirection = FVector::ZeroVector;
    LastShotPower = 0.0f;
    LastShotStartLocation = FVector::ZeroVector;

    // ⭐ OB 교차점 계산 관련 변수 초기화
    LastOBCrossingPoint = FVector::ZeroVector;
    bHasValidOBCrossingPoint = false;
    PreviousBallPosition = FVector::ZeroVector;
}


void AGolfBall::BeginDestroy()
{
    UE_LOG(LogTemp, Log, TEXT("🗑️ GolfBall BeginDestroy: %s"), *GetName());

    // ⭐ 첫 번째로 파괴 플래그 설정
    bIsBeingDestroyed = true;

    // ⭐ 즉시 모든 델리게이트 언바인딩
    OnBallStateChangedInternal.Clear();
    OnBallGameFlowEvent.Clear();

    // ⭐ 모든 타이머 즉시 정리 (멀티스레드 안전)
    if (UWorld* World = GetWorld())
    {
        FTimerManager& TimerManager = World->GetTimerManager();

        // 특정 타이머들 개별 정리
        TimerManager.ClearTimer(StateTransitionTimer);
        TimerManager.ClearTimer(ResetReadyTimer);
        TimerManager.ClearTimer(CountdownUpdateTimer);
        TimerManager.ClearTimer(ManualSleepCheckTimer);
        TimerManager.ClearTimer(SafeBounceTimer);

        // 이 객체의 모든 타이머 정리
        TimerManager.ClearAllTimersForObject(this);
    }

    // ⭐ 물리를 먼저 정지
    if (BallMesh && IsValid(BallMesh))
    {
        if (BallMesh->IsSimulatingPhysics())
        {
            BallMesh->SetSimulatePhysics(false);
            BallMesh->SetEnableGravity(false);
            BallMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
            BallMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
        }

        BallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        BallMesh->OnComponentHit.RemoveAll(this);
        BallMesh->OnComponentBeginOverlap.RemoveAll(this);
    }

    // 참조 정리
    LandscapeChecker = nullptr;
    LinkedCameraManager = nullptr;
    GM = nullptr;

    Super::BeginDestroy();
}
// EndPlay에서도 안전한 정리
void AGolfBall::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UE_LOG(LogTemp, Log, TEXT("🔚 GolfBall EndPlay started: %s"), *GetName());

    bIsBeingDestroyed = true;

    // 물리를 먼저 정지
    if (BallMesh && IsValid(BallMesh))
    {
        BallMesh->SetSimulatePhysics(false);
        BallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        BallMesh->OnComponentHit.RemoveAll(this);
    }

    // 레벨 종료 시 강제 정리
    //DestroyCrosshairActor();
    // 타이머 정리
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearAllTimersForObject(this);
    }

    Super::EndPlay(EndPlayReason);
    UE_LOG(LogTemp, Log, TEXT("✅ GolfBall EndPlay completed"));
}



// 모든 물리 상태 변경 함수에서 안전성 체크 추가
void AGolfBall::SetPhysicsState(EPhysicsState NewState)
{
    if (!BallMesh || !IsValid(BallMesh))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ BallMesh invalid in SetPhysicsState"));
        return;
    }

    if (bIsChangingPhysicsState)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Physics state change already in progress"));
        return;
    }

    if (CurrentPhysicsState == NewState)
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("📋 Physics state already: %s"),
            *UEnum::GetValueAsString(NewState));
        return;
    }

    bIsChangingPhysicsState = true;
    EPhysicsState OldState = CurrentPhysicsState;

    UE_LOG(LogTemp, Log, TEXT("🔧 Physics State: %s → %s"),
        *UEnum::GetValueAsString(OldState),
        *UEnum::GetValueAsString(NewState));

    switch (NewState)
    {
    case EPhysicsState::Disabled:
        BallMesh->SetSimulatePhysics(false);
        BallMesh->SetEnableGravity(false);
        BallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        bIsChangingPhysicsState = false;
        break;

    case EPhysicsState::Static:
        BallMesh->SetSimulatePhysics(false);
        BallMesh->SetEnableGravity(false);
        BallMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        BallMesh->SetCollisionProfileName(OriginalCollisionProfileName);
        bIsChangingPhysicsState = false;
        break;

    case EPhysicsState::Simulating:
        // 즉시 충돌 활성화
        BallMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        BallMesh->SetCollisionProfileName(OriginalCollisionProfileName);

        // ⭐ 수정: 즉시 물리 활성화 시도 (지연 제거)
        BallMesh->SetSimulatePhysics(true);
        BallMesh->SetEnableGravity(true);
        BallMesh->WakeRigidBody();

        // 검증을 위한 다음 프레임 체크
        GetWorld()->GetTimerManager().SetTimerForNextTick([this]() {
            if (BallMesh && IsValid(BallMesh))
            {
                if (!BallMesh->IsSimulatingPhysics())
                {
                    UE_LOG(LogTemp, Warning, TEXT("⚠️ Physics activation failed, retrying"));
                    BallMesh->SetSimulatePhysics(true);
                    BallMesh->SetEnableGravity(true);
                    BallMesh->WakeRigidBody();
                }
                LogCurrentPhysicsState(TEXT("After Physics Activation"));
            }
            bIsChangingPhysicsState = false;
            });

        CurrentPhysicsState = NewState;
        return;
    }

    CurrentPhysicsState = NewState;
    LogCurrentPhysicsState(TEXT("After SetPhysicsState"));
}

bool AGolfBall::GetIsConcede()
{
    return bIsConceded;
}



AActor* AGolfBall::SpawnBallLocation(UWorld* World, TSubclassOf<AActor> ClassToSpawn, float Height)
{
    if (!World || !*ClassToSpawn) return nullptr;

    const FVector BallLocation = GetActorLocation();
    const FVector spawnLoc = BallLocation + FVector(0, 0, Height);
    AActor* A = World->SpawnActor<AActor>(*ClassToSpawn, spawnLoc, FRotator::ZeroRotator);
    if (!A) return nullptr;

    return A;
}


float AGolfBall::GetActualBallRadius() const
{
    if (!BallMesh) return PARKGOLF_BALL_RADIUS;

    FVector CurrentScale = BallMesh->GetComponentScale();
    float ActualRadius = UNREAL_SPHERE_RADIUS * CurrentScale.Z;

    return ActualRadius;
}

// ===== 설정 파일 관리 =====
bool AGolfBall::LoadPhysicsConfigFromFile(const FString& FilePath)
{
    FString ActualPath = FilePath.IsEmpty() ? GetDefaultConfigFilePath() : FilePath;

    if (LoadConfigFromJson(ActualPath))
    {
        ApplyLoadedPhysicsConfig();
        bConfigLoaded = true;
        LastConfigLoadTime = FDateTime::Now();
        UE_LOG(LogTemp, Log, TEXT("✅ Physics config loaded from: %s"), *ActualPath);
        return true;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("❌ Failed to load config from: %s"), *ActualPath);
        if (SavePhysicsConfigToFile(ActualPath))
        {
            UE_LOG(LogTemp, Log, TEXT("📄 Created default config file: %s"), *ActualPath);
        }
        return false;
    }
}

bool AGolfBall::SavePhysicsConfigToFile(const FString& FilePath)
{
    FString ActualPath = FilePath.IsEmpty() ? GetDefaultConfigFilePath() : FilePath;

    if (SaveConfigToJson(ActualPath))
    {
        UE_LOG(LogTemp, Log, TEXT("💾 Physics config saved to: %s"), *ActualPath);
        return true;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to save config to: %s"), *ActualPath);
        return false;
    }
}

void AGolfBall::ReloadPhysicsConfig()
{
    UE_LOG(LogTemp, Log, TEXT("🔄 Reloading physics config..."));
    LoadPhysicsConfigFromFile();

}

void AGolfBall::ApplyLoadedPhysicsConfig()
{
    UpdatePhysicsParameters();
    ConfigureStatePhysics();
    ApplyStatePhysics(CurrentBallState);
    UE_LOG(LogTemp, Log, TEXT("🎯 Applied loaded physics config"));
}

FString AGolfBall::GetDefaultConfigFilePath() const
{
    FString ProjectDir = FPaths::ProjectSavedDir();
    FString ConfigDir = FPaths::Combine(ProjectDir, TEXT("Config"));

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*ConfigDir))
    {
        PlatformFile.CreateDirectoryTree(*ConfigDir);
    }

    return FPaths::Combine(ConfigDir, TEXT("TerrainPhysics.json"));
}

bool AGolfBall::LoadConfigFromJson(const FString& FilePath)
{
    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
    {
        return false;
    }

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to parse JSON config file"));
        return false;
    }

    LoadConfigFromJsonObject(JsonObject);
    // ⭐ 새로 추가: 지형 설정 로드
    LoadTerrainConfigFromJsonObject(JsonObject);

    return true;
}

bool AGolfBall::SaveConfigToJson(const FString& FilePath)
{
    TSharedPtr<FJsonObject> JsonObject = CreateDefaultConfigJson();

    TSharedPtr<FJsonObject> PhysicsObj = MakeShareable(new FJsonObject);
    PhysicsObj->SetNumberField(TEXT("BaseLinearDamping"), PhysicsConfig.BaseLinearDamping);
    PhysicsObj->SetNumberField(TEXT("BaseAngularDamping"), PhysicsConfig.BaseAngularDamping);
    PhysicsObj->SetNumberField(TEXT("RollingFriction"), PhysicsConfig.RollingFriction);
    PhysicsObj->SetNumberField(TEXT("Restitution"), PhysicsConfig.Restitution);
    PhysicsObj->SetNumberField(TEXT("AirResistance"), PhysicsConfig.AirResistance);
    PhysicsObj->SetNumberField(TEXT("GravityScale"), PhysicsConfig.GravityScale);
    JsonObject->SetObjectField(TEXT("BallPhysicsConfig"), PhysicsObj);
    PhysicsObj->SetNumberField(TEXT("ForwardSpinFactor"), PhysicsConfig.ForwardSpinFactor);

    TSharedPtr<FJsonObject> ConstantsObj = MakeShareable(new FJsonObject);
    ConstantsObj->SetNumberField(TEXT("MIN_SPEED"), ParkGolfConstants.MIN_SPEED);
    ConstantsObj->SetNumberField(TEXT("MAX_SPEED"), ParkGolfConstants.MAX_SPEED);
    ConstantsObj->SetNumberField(TEXT("TYPICAL_SPEED"), ParkGolfConstants.TYPICAL_SPEED);
    ConstantsObj->SetNumberField(TEXT("MIN_LAUNCH_ANGLE"), ParkGolfConstants.MIN_LAUNCH_ANGLE);
    ConstantsObj->SetNumberField(TEXT("MAX_LAUNCH_ANGLE"), ParkGolfConstants.MAX_LAUNCH_ANGLE);
    ConstantsObj->SetNumberField(TEXT("TYPICAL_LAUNCH_ANGLE"), ParkGolfConstants.TYPICAL_LAUNCH_ANGLE);
    JsonObject->SetObjectField(TEXT("ParkGolfConstants"), ConstantsObj);


    // ⭐ 새로 추가: 지형 설정 저장
    SaveTerrainConfigToJsonObject(JsonObject);

    // 메타데이터 업데이트
    JsonObject->SetStringField(TEXT("ConfigVersion"), TEXT("2.0"));
    JsonObject->SetStringField(TEXT("Description"), TEXT("Park Golf Physics Configuration with Terrain Settings"));
    JsonObject->SetStringField(TEXT("LastModified"), FDateTime::Now().ToString());

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    return FFileHelper::SaveStringToFile(OutputString, *FilePath);
}

TSharedPtr<FJsonObject> AGolfBall::CreateDefaultConfigJson() const
{
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

    TSharedPtr<FJsonObject> PhysicsObj = MakeShareable(new FJsonObject);
    PhysicsObj->SetNumberField(TEXT("BaseLinearDamping"), 0.08);
    PhysicsObj->SetNumberField(TEXT("BaseAngularDamping"), 0.08);
    PhysicsObj->SetNumberField(TEXT("RollingFriction"), 0.25);
    PhysicsObj->SetNumberField(TEXT("BounceDamping"), 0.6);
    PhysicsObj->SetNumberField(TEXT("AirResistance"), 0.015);
    PhysicsObj->SetNumberField(TEXT("GravityScale"), 1.0);
    JsonObject->SetObjectField(TEXT("BallPhysicsConfig"), PhysicsObj);

    TSharedPtr<FJsonObject> ConstantsObj = MakeShareable(new FJsonObject);
    ConstantsObj->SetNumberField(TEXT("MIN_SPEED"), 2.0);
    ConstantsObj->SetNumberField(TEXT("MAX_SPEED"), 25.0);
    ConstantsObj->SetNumberField(TEXT("TYPICAL_SPEED"), 15.0);
    ConstantsObj->SetNumberField(TEXT("MIN_LAUNCH_ANGLE"), 0.5);
    ConstantsObj->SetNumberField(TEXT("MAX_LAUNCH_ANGLE"), 12.0);
    ConstantsObj->SetNumberField(TEXT("TYPICAL_LAUNCH_ANGLE"), 4.0);
    JsonObject->SetObjectField(TEXT("ParkGolfConstants"), ConstantsObj);

    JsonObject->SetStringField(TEXT("ConfigVersion"), TEXT("1.0"));
    JsonObject->SetStringField(TEXT("Description"), TEXT("Park Golf Physics Configuration"));
    JsonObject->SetStringField(TEXT("LastModified"), FDateTime::Now().ToString());

    return JsonObject;
}

void AGolfBall::LoadConfigFromJsonObject(TSharedPtr<FJsonObject> JsonObject)
{
    // 기본 볼 물리 설정 로드
    if (JsonObject->HasField(TEXT("BallPhysicsConfig")))
    {
        const TSharedPtr<FJsonObject>* PhysicsObj;
        if (JsonObject->TryGetObjectField(TEXT("BallPhysicsConfig"), PhysicsObj))
        {
            PhysicsConfig.BaseLinearDamping = (*PhysicsObj)->GetNumberField(TEXT("BaseLinearDamping"));
            PhysicsConfig.BaseAngularDamping = (*PhysicsObj)->GetNumberField(TEXT("BaseAngularDamping"));
            PhysicsConfig.RollingFriction = (*PhysicsObj)->GetNumberField(TEXT("RollingFriction"));
            PhysicsConfig.Restitution = (*PhysicsObj)->GetNumberField(TEXT("BounceDamping"));
            PhysicsConfig.AirResistance = (*PhysicsObj)->GetNumberField(TEXT("AirResistance"));
            PhysicsConfig.GravityScale = (*PhysicsObj)->GetNumberField(TEXT("GravityScale"));
            PhysicsConfig.ForwardSpinFactor = (*PhysicsObj)->GetNumberField(TEXT("ForwardSpinFactor"));
            PhysicsConfig.MaxBounceSpeedRatio = (*PhysicsObj)->GetNumberField(TEXT("MaxBounceSpeedRatio"));
            PhysicsConfig.MinBounceFixHeight = (*PhysicsObj)->GetNumberField(TEXT("MinBounceFixHeight"));
            PhysicsConfig.MinPreImpactSpeed = (*PhysicsObj)->GetNumberField(TEXT("MinPreImpactSpeed"));
            PhysicsConfig.TeeShotPowerModify = (*PhysicsObj)->GetNumberField(TEXT("TeeShotPowerModify"));
            PhysicsConfig.SecondShotPowerModify = (*PhysicsObj)->GetNumberField(TEXT("SecondShotPowerModify"));
            PhysicsConfig.MulltiflyGrassCondition = (*PhysicsObj)->GetNumberField(TEXT("MulltiflyGrassCondition"));

            UE_LOG(LogTemp, Log, TEXT("📊 Base physics config loaded"));
        }
    }

    // 파크골프 상수 로드
    if (JsonObject->HasField(TEXT("ParkGolfConstants")))
    {
        const TSharedPtr<FJsonObject>* ConstantsObj;
        if (JsonObject->TryGetObjectField(TEXT("ParkGolfConstants"), ConstantsObj))
        {
            ParkGolfConstants.MIN_SPEED = (*ConstantsObj)->GetNumberField(TEXT("MIN_SPEED"));
            ParkGolfConstants.MAX_SPEED = (*ConstantsObj)->GetNumberField(TEXT("MAX_SPEED"));
            ParkGolfConstants.TYPICAL_SPEED = (*ConstantsObj)->GetNumberField(TEXT("TYPICAL_SPEED"));
            ParkGolfConstants.MIN_LAUNCH_ANGLE = (*ConstantsObj)->GetNumberField(TEXT("MIN_LAUNCH_ANGLE"));
            ParkGolfConstants.MAX_LAUNCH_ANGLE = (*ConstantsObj)->GetNumberField(TEXT("MAX_LAUNCH_ANGLE"));
            ParkGolfConstants.TYPICAL_LAUNCH_ANGLE = (*ConstantsObj)->GetNumberField(TEXT("TYPICAL_LAUNCH_ANGLE"));

            UE_LOG(LogTemp, Log, TEXT("📊 Park golf constants loaded"));
        }
    }

    UE_LOG(LogTemp, Log, TEXT("📊 Config loaded: LinearDamping=%.3f, RollingFriction=%.3f, MinSpeed=%.1f, MaxSpeed=%.1f"),
        PhysicsConfig.BaseLinearDamping, PhysicsConfig.RollingFriction,
        ParkGolfConstants.MIN_SPEED, ParkGolfConstants.MAX_SPEED);
}


// ===== 물리 시스템 =====

void AGolfBall::InitializeUE4PhysicsSystem()
{
    // Custom 프리셋 + PhysicsBody 오브젝트 타입
    BallMesh->SetCollisionProfileName(TEXT("Custom"));
    BallMesh->SetCollisionObjectType(ECC_PhysicsBody);
    BallMesh->SetCollisionResponseToAllChannels(ECR_Block);
    BallMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

    // ★ 추가: WorldDynamic(홀컵 trigger의 오브젝트 타입)에 대해 Overlap으로 설정
    //   trigger 컴포넌트가 WorldDynamic + Query Only이므로
    //   볼이 이 채널을 Block하면 Overlap 이벤트가 발생하지 않음
    BallMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);

    BallMesh->SetUseCCD(true);
    BallMesh->SetNotifyRigidBodyCollision(true);

    ConfigureStatePhysics();
    SetPhysicsState(EPhysicsState::Disabled);

    UE_LOG(LogTemp, Log, TEXT("✅ UE4 optimized physics system initialized"));
}

void AGolfBall::SetBounceFix(bool bISBouncFix)
{
    bBounceFix = bISBouncFix;
}

bool AGolfBall::GetBounceFix()
{
    return bBounceFix;
}

// 3. UE4용 물리 재질 생성
UPhysicalMaterial* AGolfBall::CreateOptimizedPhysicalMaterial()
{
    UPhysicalMaterial* PhysMaterial = NewObject<UPhysicalMaterial>(this);

    // ======================================================
    // 에디터 Ball_Test01 PhysicalMaterial 설정값과 동일하게 맞춤
    // ======================================================

    // 마찰
    PhysMaterial->Friction = 0.25f;   // 마찰
    PhysMaterial->StaticFriction = 0.15f;   // 스태틱 마찰

    // 마찰 혼합 모드: Min (에디터와 동일)
    PhysMaterial->FrictionCombineMode = EFrictionCombineMode::Min;
    PhysMaterial->bOverrideFrictionCombineMode = true;  // 마찰 결합 모드 오버라이드 ✅

    // 복원력
    PhysMaterial->Restitution = 0.75f;      // 복원력

    // 복원 결합 모드: Max (에디터와 동일)
    PhysMaterial->RestitutionCombineMode = EFrictionCombineMode::Max;
    PhysMaterial->bOverrideRestitutionCombineMode = true;  // 복원력 결합 모드 오버라이드 ✅

    // 고급
    PhysMaterial->RaiseMassToPower = 0.75f; // 질량을 제곱으로 줄리기

    // 밀도 (1.15 g/cm³)
    PhysMaterial->Density = 1.15f;

    // 슬립 한계치 (Chaos 전용 필드)
    PhysMaterial->SleepLinearVelocityThreshold = 2.0f;   // 슬립 선형 속도 한계치
    PhysMaterial->SleepAngularVelocityThreshold = 1.5f;   // 슬립 각도 속도 한계치
    PhysMaterial->SleepCounterThreshold = 4;      // 슬립 카운터 한계치

    return PhysMaterial;
}


void AGolfBall::ConfigureStatePhysics()
{
    StatePhysicsMap.Empty();
    StatePhysicsMap.Add(EBallState::Ball_Init, FStatePhysicsSettings(false, false, 5.0f, 5.0f));
    StatePhysicsMap.Add(EBallState::Ball_Ready, FStatePhysicsSettings(false, false, 5.0f, 5.0f));
    StatePhysicsMap.Add(EBallState::Ball_Fly, FStatePhysicsSettings(true, true,
        PhysicsConfig.BaseLinearDamping * 1.0f, PhysicsConfig.BaseAngularDamping * 1.0f));
    StatePhysicsMap.Add(EBallState::Ball_Bound, FStatePhysicsSettings(true, true,
        PhysicsConfig.BaseLinearDamping * 0.5f, PhysicsConfig.BaseAngularDamping * 1.0f));
    StatePhysicsMap.Add(EBallState::Ball_Rolling, FStatePhysicsSettings(true, true,
        PhysicsConfig.BaseLinearDamping * 0.5f, PhysicsConfig.BaseAngularDamping * 2.0f));
    StatePhysicsMap.Add(EBallState::Ball_Stop, FStatePhysicsSettings(false, false, 8.0f, 8.0f));
    StatePhysicsMap.Add(EBallState::Ball_Des, FStatePhysicsSettings(false, false, 8.0f, 8.0f));
}

void AGolfBall::ApplyStatePhysics(EBallState State)
{

    if (!StatePhysicsMap.Contains(State))
    {
        UE_LOG(LogTemp, Warning, TEXT("No physics config for state: %s"), *UEnum::GetValueAsString(State));
        return;
    }

    FStatePhysicsSettings Config = StatePhysicsMap[State];

    // ⭐ 수정: 물리 상태로 직접 매핑
    EPhysicsState RequiredPhysicsState;

    if (State == EBallState::Ball_Des) // ⭐ 핵심 수정: Ball_Init 상태를 특별 처리
    {
        RequiredPhysicsState = EPhysicsState::Disabled; // Ball_Init은 물리/충돌 완전 비활성화
    }
    else if (State == EBallState::Ball_Init) // ⭐ 핵심 수정: Ball_Init 상태를 특별 처리
    {
        RequiredPhysicsState = EPhysicsState::Disabled; // Ball_Init은 물리/충돌 완전 비활성화
    }
    else if (Config.bEnablePhysics)
    {
        RequiredPhysicsState = EPhysicsState::Simulating;
    }
    else if (State == EBallState::Ball_Ready)
    {
        AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
        if (GameMode->IsRangeMode())
        {
            ApplySimpleMesh();
        }
        else
        {
            float HoleDistance = GetHoleDistance();
            bool bShowDueToDistance = (HoleDistance > 1000.0f); // 10m 초과

            //if (bShowDueToDistance)
            //{
            //    ApplyComplexMesh();

            //}
            //else
            //{
            //    ApplySimpleMesh();
            //}

            ApplySimpleMesh();

        }

        RequiredPhysicsState = EPhysicsState::Static;
    }
    else
    {
        RequiredPhysicsState = EPhysicsState::Disabled;
    }

    SetPhysicsState(RequiredPhysicsState);

    // 댐핑 설정 (물리 활성화 후)
    if (Config.bEnablePhysics &&
        State != EBallState::Ball_Bound &&
        State != EBallState::Ball_Rolling)
    {
        GetWorld()->GetTimerManager().SetTimerForNextTick([this, Config]() {
            if (BallMesh && IsValid(BallMesh))
            {
                BallMesh->SetLinearDamping(Config.LinearDamping);
                BallMesh->SetAngularDamping(Config.AngularDamping);
            }
            });
    }

    UE_LOG(LogTemp, Log, TEXT("✅ State physics applied: %s → %s"),
        *UEnum::GetValueAsString(State),
        *UEnum::GetValueAsString(RequiredPhysicsState));
}

void AGolfBall::UpdatePhysicsBasedOnState(float DeltaTime)
{
    // ✅ 안전성 체크 추가
    if (IsValid(GM->BoomLine))
    {
        if (BallMesh->GetComponentVelocity().Size() >= 2000)
            GM->BoomLine->bApply = true;
        else
            GM->BoomLine->bApply = false;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ BoomLine is null in UpdatePhysicsBasedOnState"));
    }

    switch (CurrentBallState)
    {
    case EBallState::Ball_Fly:
        UpdateFlyingPhysics(DeltaTime);
        break;

    case EBallState::Ball_Bound:
        UpdateBouncePhysicsLandtype(DeltaTime);
        break;

    case EBallState::Ball_Rolling:
        UpdateRollingPhysics(DeltaTime);
        break;

    case EBallState::Ball_Stop:


        UpdateStoppedPhysics();

        // ✅ 안전성 체크 추가

        break;
    }
}

void AGolfBall::UpdateFlyingPhysics(float DeltaTime)
{
    FVector CurrentVelocity = BallMesh->GetPhysicsLinearVelocity();
    float Speed = CurrentVelocity.Size();

    if (Speed > VELOCITY_EPSILON)
    {
        float DragCoefficient = PhysicsConfig.AirResistance;
        float ResistanceMagnitude = DragCoefficient * Speed * Speed * DeltaTime * 0.0008f;
        FVector ResistanceForce = -CurrentVelocity.GetSafeNormal() * ResistanceMagnitude;
        BallMesh->AddForce(ResistanceForce);
        // ✅ 최적화: 매 프레임 Log → VeryVerbose (기본 비활성)
        UE_LOG(LogTemp, VeryVerbose, TEXT("UpdateFlyingPhysics - FLY :  %f"), ResistanceMagnitude);
    }

    // ✅ 최적화: 매 프레임 Log 제거
    UE_LOG(LogTemp, VeryVerbose, TEXT("ParkGolf Flying: Speed=%.1fm/s, Height=%.1fm"),
        Speed / 100.0f, GetActorLocation().Z / 100.0f);
}

void AGolfBall::UpdateRollingPhysics(float DeltaTime)
{

    if (!BallMesh->IsSimulatingPhysics())
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("UpdateRollingPhysics: 물리 시뮬레이션이 비활성화됨"));
        return;
    }

    if (DeltaTime <= 0.0f || !FMath::IsFinite(DeltaTime) || DeltaTime > 1.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("UpdateRollingPhysics: 잘못된 DeltaTime=%.6f"), DeltaTime);
        return;
    }

    // ⭐ 물(Water) 지형: 공이 물에 들어가면 마찰력 대신 즉시 정지 처리
    if (CurrentLandType == ELandType::Water)
    {
        UE_LOG(LogTemp, Log, TEXT("💧 UpdateRollingPhysics: 물 지형 감지, 공 즉시 정지"));
        ForceStopBall();
        if (CurrentBallState != EBallState::Ball_Stop)
        {
            SetBallState(EBallState::Ball_Stop);
        }
        return;
    }
    // 현재 물리 속도 가져오기
    FVector CurrentVelocity = FVector::ZeroVector;
    float Speed = 0.0f;

    // ✅ 최적화: try/catch 제거 (UE는 C++ 예외 비활성화, 오버헤드만 발생)
    // IsValidBodyInstance로 안전하게 체크
    if (!BallMesh->GetBodyInstance() || !BallMesh->GetBodyInstance()->IsValidBodyInstance())
    {
        UE_LOG(LogTemp, Error, TEXT("UpdateRollingPhysics: BodyInstance가 유효하지 않음"));
        return;
    }
    CurrentVelocity = BallMesh->GetPhysicsLinearVelocity();
    Speed = CurrentVelocity.Size();

    // 속도 데이터 유효성 검증
    if (CurrentVelocity.ContainsNaN() || FMath::IsNaN(Speed) || !FMath::IsFinite(Speed))
    {
        UE_LOG(LogTemp, Error, TEXT("UpdateRollingPhysics: 잘못된 속도 데이터"));
        BallMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
        BallMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
        return;
    }

    // 속도가 너무 낮으면 완전 정지
    if (Speed < VELOCITY_EPSILON)
    {
        UE_LOG(LogTemp, Log, TEXT("굴림: 속도가 너무 낮음 (%.6f), 정지 처리"), Speed);
        BallMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
        BallMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

        if (CurrentBallState != EBallState::Ball_Stop)
        {
            SetBallState(EBallState::Ball_Stop);
        }
        return;
    }

    // ⭐ 추가: 물에 닿으면 (속도와 무관하게) 즉시 정지
    if (CurrentLandType == ELandType::Water)
    {
        UE_LOG(LogTemp, Log, TEXT("💧 Water 감지 (Rolling): 즉시 정지 처리"));
        ForceStopBall();
        if (CurrentBallState != EBallState::Ball_Stop)
        {
            SetBallState(EBallState::Ball_Stop);
        }
        return;
    }

    // === 바닥 굴림 전용 물리 처리 ===

    // 1. 수평 속도만 추출 (Z축 속도 완전 제거)
    FVector HorizontalVelocity = FVector(CurrentVelocity.X, CurrentVelocity.Y, 0.0f);
    float HorizontalSpeed = HorizontalVelocity.Size();

    UE_LOG(LogTemp, Log, TEXT("굴림: 원래속도=%.1f, 수평속도=%.1f, 수직속도=%.1f"),
        Speed, HorizontalSpeed, CurrentVelocity.Z);

    // 2. 볼을 지면에 정확히 위치시키기
    //if (GetWorld())
    //{
    //    FVector BallLocation = GetActorLocation();
    //    float ActualBallRadius = GetActualBallRadius();

    //    // 지면 감지를 위한 레이캐스트
    //    FVector TraceStart = BallLocation + FVector(0, 0, 5.0f); // 볼 위에서 시작
    //    FVector TraceEnd = BallLocation - FVector(0, 0, ActualBallRadius + 20.0f); // 볼 아래까지

    //    FHitResult GroundHit;
    //    FCollisionQueryParams QueryParams;
    //    QueryParams.AddIgnoredActor(this);
    //    QueryParams.bTraceComplex = true;

    //    if (GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
    //    {
    //        // 목표 위치: 지면 + 볼 반지름
    //        FVector TargetPosition = GroundHit.Location + (GroundHit.Normal * ActualBallRadius);
    //        float HeightDifference = FMath::Abs(BallLocation.Z - TargetPosition.Z);

    //        // 위치 차이가 큰 경우에만 보정 (미세한 진동 방지)
    //        if (HeightDifference > 0.5f) // 1cm 이상 차이
    //        {

    //            /* if (CurrentBallState == EBallState::Ball_Rolling)
    //                 SetActorLocation(TargetPosition, false, nullptr, ETeleportType::None);
    //             else*/
    //            {
    //                // 부드러운 위치 보정 (급격한 점프 방지)
    //                FVector SmoothedPosition = FMath::VInterpTo(BallLocation, TargetPosition, DeltaTime, 10.0f);
    //                SetActorLocation(SmoothedPosition, false, nullptr, ETeleportType::None);
    //            }

    //            UE_LOG(LogTemp, Log, TEXT("지면 보정: %.1fcm 차이, 목표 높이로 이동"), HeightDifference);
    //        }
    //    }
    //}

    // 3. 지형별 마찰력 계산
    float BaseFriction = PhysicsConfig.RollingFriction * FrictionWeight;
    float TerrainMultiplier = 1.9;

    switch (CurrentLandType)
    {
    case ELandType::Green:
        TerrainMultiplier = 1.4f;  // 그린에서 잘 굴림
        break;
    case ELandType::Rough:
        TerrainMultiplier = 1.6f;  // 러프에서 마찰 증가
        break;
    case ELandType::Fairway:
        TerrainMultiplier = 1.2f;  // 페어웨이 보통
        break;
    case ELandType::Sand:
        TerrainMultiplier = 3.8f;  // 모래에서 큰 마찰
        break;
    case ELandType::Water:
        TerrainMultiplier = 5.0f;  // 물에서 급격한 감속
        break;
    default:
        TerrainMultiplier = 1.9f;
        break;
    }

    // 속도에 따른 마찰 조정 (낮은 속도에서 마찰 감소로 자연스러운 굴림)
    float SpeedMS = HorizontalSpeed / 100.0f; // m/s 변환
    float SpeedMultiplier = 1.0f;

    // 기존: 저속에서 마찰을 오히려 키움 (1.9, 1.7, 1.5)
    // 수정: 저속에서 마찰 줄이고(자연스럽게), 고속 기본값 유지
    if (SpeedMS < 0.3f)
        SpeedMultiplier = 0.2f;   // 거의 마찰 없음 → 자연 감속에 맡김
    else if (SpeedMS < 0.5f)
        SpeedMultiplier = 0.3f;   // 1.9 → 0.5 (저속 마찰 감소 → 자연스러운 감속)
    else if (SpeedMS < 1.0f)
        SpeedMultiplier = 0.5f;   // 1.7 → 0.7
    else if (SpeedMS < 2.0f)
        SpeedMultiplier = 0.75f;  // 1.5 → 0.85
    // 2m/s 이상은 SpeedMultiplier = 1.0 (기본값 유지)
    UE_LOG(LogTemp, Log, TEXT(" 마찰력 계산: TerrainMultiplier=%.1f, SpeedMultiplier=%.1f"),
        TerrainMultiplier, SpeedMultiplier);
    // 최종 마찰력 계산 (수평 방향만)
    float EffectiveFriction = BaseFriction * TerrainMultiplier * SpeedMultiplier;
    float FrictionMagnitude = EffectiveFriction * GRAVITY_MAGNITUDE * DeltaTime;

    if (HorizontalSpeed > VELOCITY_EPSILON)
    {
        FVector FrictionForce = -HorizontalVelocity.GetSafeNormal() * FrictionMagnitude;

        // 마찰력 안전성 체크
        if (!FrictionForce.ContainsNaN() && FrictionForce.Size() < 10000.0f) // 비정상적으로 큰 힘 방지
        {
            BallMesh->AddForce(FrictionForce);
        }
    }

    // 4. 경사면 효과 (수평 방향만 적용)
    FVector TerrainNormal = GetTerrainNormal();
    if (!TerrainNormal.IsNearlyZero() && !TerrainNormal.Equals(FVector::UpVector, 0.1f))
    {
        // 경사 방향 계산 (수평 성분만)
        FVector SlopeDirection = FVector(TerrainNormal.X, TerrainNormal.Y, 0.0f).GetSafeNormal();
        float SlopeAngle = FMath::Acos(FMath::Clamp(TerrainNormal.Z, 0.0f, 1.0f));

        if (SlopeAngle > FMath::DegreesToRadians(1.0f)) // 1도 이상의 경사에서만 적용
        {
            // 경사면 중력 효과 (수평 방향만)
            float SlopeForce = FMath::Sin(SlopeAngle) * GRAVITY_MAGNITUDE * 0.5f; // 50% 효과
            FVector SlopeVector = -SlopeDirection * SlopeForce * DeltaTime; // 아래 방향으로

            if (!SlopeVector.ContainsNaN())
            {
                BallMesh->AddForce(SlopeVector);
                UE_LOG(LogTemp, Log, TEXT("경사 효과: 각도=%.1f도, 힘=%.1f"),
                    FMath::RadiansToDegrees(SlopeAngle), SlopeForce);
            }
        }
    }

    // 5. 댐핑 설정 (바닥 굴림에 최적화)
    //float LinearDamping = PhysicsConfig.BaseLinearDamping * 0.6f;  // 기본보다 낮은 댐핑
    //float AngularDamping = PhysicsConfig.BaseAngularDamping * 0.5f; // 회전 유지를 위해 낮은 댐핑

    //BallMesh->SetLinearDamping(LinearDamping);
    //BallMesh->SetAngularDamping(AngularDamping);

    {
        float BaseDamping = PhysicsConfig.BaseLinearDamping;
        float BaseAngular = PhysicsConfig.BaseAngularDamping;

        float DampingMultiplier = 1.0f;
        switch (CurrentLandType)
        {
        case ELandType::Green:   DampingMultiplier = 0.8f;  break;
        case ELandType::Rough:   DampingMultiplier = 2.0f;  break; // ★ 러프: 댐핑 2배
        case ELandType::Fairway: DampingMultiplier = 1.2f;  break;
        case ELandType::Sand:    DampingMultiplier = 4.0f;  break;
        default:                 DampingMultiplier = 1.0f;  break;
        }

        float LinearDamping = FMath::Clamp(BaseDamping * DampingMultiplier, 0.05f, 10.0f);
        float AngularDamping = FMath::Clamp(BaseAngular * DampingMultiplier, 0.05f, 10.0f);

        BallMesh->SetLinearDamping(LinearDamping);
        BallMesh->SetAngularDamping(AngularDamping);
    }

    // 6. 최종 속도 적용 (수평 속도만 유지, Z축은 항상 0)
    FVector FinalVelocity = HorizontalVelocity; // Z 성분은 0

    // 매우 작은 수직 속도라도 완전히 제거
    FinalVelocity.Z = 0.0f;

    // 안전성 체크 후 속도 적용
    if (!FinalVelocity.ContainsNaN())
    {
        FVector Vel = BallMesh->GetPhysicsLinearVelocity();
        if (FMath::Abs(Vel.Z) > 5.0f)  // Z가 5cm/s 초과할 때만 보정 (미세진동 방지)
        {
            Vel.Z = 0.0f;
            BallMesh->SetPhysicsLinearVelocity(Vel);
        }
        else
            BallMesh->SetPhysicsLinearVelocity(FinalVelocity);
    }

    // 7. 각속도도 수평 회전만 허용 (X, Y축 회전만)
    FVector CurrentAngularVel = BallMesh->GetPhysicsAngularVelocityInDegrees();
    FVector ConstrainedAngularVel = FVector(CurrentAngularVel.X, CurrentAngularVel.Y, 0.0f); // Z축 회전 제거
    BallMesh->SetPhysicsAngularVelocityInDegrees(ConstrainedAngularVel);

    // 디버그 로그 (주기적으로)
    static float LastLogTime = 0.0f;
    float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    if (CurrentTime - LastLogTime > 1.0f) // 1초마다 로그
    {
        // ✅ 최적화: Log → VeryVerbose (기본 비활성)
        UE_LOG(LogTemp, Log, TEXT("바닥굴림: 속도=%.1fcm/s, 지형=%s, 마찰=%.2fx%.2f"),
            HorizontalSpeed, *UEnum::GetValueAsString(CurrentLandType),
            TerrainMultiplier, SpeedMultiplier);
        LastLogTime = CurrentTime;
    }
}


void AGolfBall::UpdateBouncePhysicsLandtype(float DeltaTime)
{
    // ⭐ 강화된 안전성 체크 1: 기본 유효성
    if (!BallMesh || !IsValid(BallMesh) || !IsValid(this))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ UpdateBouncePhysics: Invalid objects detected"));
        return;
    }

    // ⭐ 강화된 안전성 체크 2: 물리 시뮬레이션 상태
    if (!BallMesh->IsSimulatingPhysics())
    {
        UE_LOG(LogTemp, Log, TEXT("🔄 UpdateBouncePhysics: Physics not simulating, skipping"));
        return;
    }

    // 안전성 체크: DeltaTime 유효성
    if (DeltaTime <= 0.0f || !FMath::IsFinite(DeltaTime) || DeltaTime > 1.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ UpdateBouncePhysics: Invalid DeltaTime=%.6f, skipping"), DeltaTime);
        return;
    }

    // ✅ 최적화: try/catch 제거 → IsValidBodyInstance 직접 체크 (UE는 C++ 예외 비활성화)
    FVector CurrentVelocity = FVector::ZeroVector;
    float Speed = 0.0f;

    if (!BallMesh->GetBodyInstance() || !BallMesh->GetBodyInstance()->IsValidBodyInstance())
    {
        UE_LOG(LogTemp, Error, TEXT("❌ UpdateBouncePhysics: Invalid BodyInstance"));
        return;
    }
    CurrentVelocity = BallMesh->GetPhysicsLinearVelocity();
    Speed = CurrentVelocity.Size();

    // 속도 데이터 유효성 검증
    if (CurrentVelocity.ContainsNaN() || FMath::IsNaN(Speed) || !FMath::IsFinite(Speed))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ UpdateBouncePhysics: Invalid velocity data - NaN or infinite"));
        if (BallMesh && IsValid(BallMesh))
        {
            BallMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
            BallMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
        }
        return;
    }

    // ★ Ball_Bound 상태(실제 바운스 중)이면 Damping 최소화하고 즉시 반환
//   Chaos 솔버가 Restitution으로 계산한 반발 Z속도를 보존해야 함
//   매 프레임 Damping/Friction을 적용하면 Z속도가 깎여서 바운스가 사라짐
    if (CurrentBallState == EBallState::Ball_Bound)
    {
        BallMesh->SetLinearDamping(0.01f);   // 거의 0 → 반발 Z속도 최대 보존
        BallMesh->SetAngularDamping(0.05f);
        // 마찰/Damping 계산 전부 스킵 → Chaos에 맡김
        return;
    }


    // 낮은 속도에서 처리 (바운스 친화적)
    if (Speed < VELOCITY_EPSILON)
    {
        UE_LOG(LogTemp, Log, TEXT("🛑 UpdateBouncePhysics: Speed too low (%.6f), stopping safely"), Speed);
        if (BallMesh && IsValid(BallMesh))
        {
            BallMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
            BallMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
        }
        if (CurrentBallState != EBallState::Ball_Rolling)
        {
            SetBallState(EBallState::Ball_Rolling);
            UE_LOG(LogTemp, Log, TEXT("ParkGolf: Bound -> Rolling (UpdateBouncePhysicsLandtype)"));
        }
        return;
    }

    // ✅ 최적화: try/catch 제거 → 일반 코드 흐름으로
    {
        float SpeedMS = Speed / 100.0f;
        float SpeedRatio = FMath::Clamp(Speed / 800.0f, 0.01f, 1.0f);  // 더 높은 기준점 (500 -> 800)
        float SpeedSensitivity = FMath::Pow(SpeedRatio, 2.0f);          // 더 급격한 곡선 (1.5 -> 2.0)
        // ⭐ 추가: 힘의 정도에 따른 굴림 특성 차별화
        float PowerInfluence = 1.0f;

        // Ball_Bound에서 온 경우와 Ball_Rolling에서 계속인 경우 구분
        if (CurrentBallState == EBallState::Ball_Bound && SpeedMS > 3.0f)
        {
            // 바운스에서 굴림으로 전환하는 단계 - 아직 빠른 속도
            PowerInfluence = 0.8f; // 마찰과 댐핑 20% 감소
        }
        else if (CurrentBallState == EBallState::Ball_Rolling && SpeedMS > 5.0f)
        {
            // 순수 굴림 상태이지만 아직 빠른 속도
            PowerInfluence = 0.9f; // 마찰과 댐핑 10% 감소
        }


        // 🏀 기본 마찰력 계산 - 낮은 속도에서 매우 낮은 마찰
        float BaseFriction = PhysicsConfig.RollingFriction * FrictionWeight * PowerInfluence;
        float DynamicFriction = BaseFriction * (0.5f + SpeedSensitivity * 0.5f); // 50% ~ 100% 마찰 

        // ⭐ 지형별 민감도 조정 (안전하게)
        float TerrainSensitivityMultiplier = 1.0f;

        // CurrentLandType 안전성 체크
        if (IsValid(this)) // this 포인터 유효성 재확인
        {
            switch (CurrentLandType)
            {
            case ELandType::Green:
                TerrainSensitivityMultiplier = 1.0f;  // 0.5 → 1.0 (기준값)
                break;
            case ELandType::Rough:
                TerrainSensitivityMultiplier = 1.8f;  // 0.7 → 1.8 (거칠게)
                break;
            case ELandType::Fairway:
                TerrainSensitivityMultiplier = 1.2f;  // 0.7 → 1.2
                break;
            case ELandType::Sand:
                TerrainSensitivityMultiplier = 2.5f;  // 1.1 → 2.5
                break;
            case ELandType::Water:
                TerrainSensitivityMultiplier = 5.0f;  // 유지
                break;
            default:
                TerrainSensitivityMultiplier = 1.3f;  // 0.9 → 1.3
                break;
            }

        }

        // 지형 민감도 적용
        DynamicFriction *= TerrainSensitivityMultiplier;

        // 🏀 바운스 친화적 마찰력 적용 - 낮은 속도에서 매우 약한 마찰
        float FrictionMagnitude = 0.0f;

        // ★ 속도 구간별 실제 차별화된 계수 적용 (기존: 전부 0.1f)
        if (Speed < 700.0f)        // 7m/s 이하
        {
            FrictionMagnitude = DynamicFriction * GRAVITY_MAGNITUDE * DeltaTime * 0.8f;
        }
        else if (Speed < 800.0f)   // 8m/s 이하
        {
            FrictionMagnitude = DynamicFriction * GRAVITY_MAGNITUDE * DeltaTime * 1.0f;
        }
        else if (Speed < 900.0f)   // 9m/s 이하
        {
            FrictionMagnitude = DynamicFriction * GRAVITY_MAGNITUDE * DeltaTime * 1.2f;
        }
        else if (Speed < 1200.0f)  // 12m/s 이하
        {
            FrictionMagnitude = DynamicFriction * GRAVITY_MAGNITUDE * DeltaTime * 1.5f;
        }
        else                        // 12m/s 초과
        {
            FrictionMagnitude = DynamicFriction * GRAVITY_MAGNITUDE * DeltaTime * 2.0f;
        }

        // 마찰력 벡터 계산 (안전하게)
        FVector SafeDirection = CurrentVelocity.GetSafeNormal();
        FVector FrictionForce = -SafeDirection * FrictionMagnitude;

        // 마찰력 유효성 최종 체크
        if (FrictionForce.ContainsNaN())
        {
            UE_LOG(LogTemp, Error, TEXT("❌ UpdateBouncePhysics: Invalid friction force calculated"));
            FrictionForce = FVector::ZeroVector;
        }

        // 🏀 바운스 친화적 댐핑 계산 - 낮은 속도에서 매우 낮은 댐핑
        float BaseDamping = PhysicsConfig.BaseLinearDamping;
        if (CurrentBallState == EBallState::Ball_Bound)
        {
            // 바운스 중에는 Damping을 거의 0에 가깝게 → 자연스러운 반발 보존
            BallMesh->SetLinearDamping(0.01f);
            BallMesh->SetAngularDamping(0.05f);
            // 마찰력도 적용하지 않음 (공중에서 마찰 의미 없음)
            return; // ← 나머지 처리 스킵
        }

        // 기존: BaseDamping * (0.3 + SpeedRatio * 0.7) → 0.036 ~ 0.12
        // 수정: 최소값을 0.15로 올리고 고속에서 더 강하게
        float DynamicDamping = FMath::Clamp(
            BaseDamping * (1.5f + SpeedRatio * 2.0f),  // 0.3→1.5, 0.7→2.0
            0.15f, 10.0f);  // 최소 0.15 보장

        // 지형별 조정
        if (CurrentLandType == ELandType::Rough)
            DynamicDamping *= 1.5f;   // 1.2 → 1.5
        else if (CurrentLandType == ELandType::Green)
            DynamicDamping *= 1.0f;   // 0.85 → 1.0 (그린도 기준값)

        // ⭐ 물리 적용 - 이중 안전성 체크와 함께
        if (BallMesh && IsValid(BallMesh) && BallMesh->IsSimulatingPhysics())
        {
            // 댐핑 설정
            BallMesh->SetLinearDamping(DynamicDamping);

            // 마찰력 적용
            if (!FrictionForce.IsNearlyZero())
            {
                BallMesh->AddForce(FrictionForce);
            }

            // 🏀 바운스 상태에서 굴림으로 전환 시 특별 처리 (더 부드럽게)
            if (CurrentBallState == EBallState::Ball_Bound)
            {
                float TransitionDamping = DynamicDamping * 1.0f;  // 더 강하게 감소 (0.7 -> 0.5)
                BallMesh->SetLinearDamping(TransitionDamping);

                // 🏀 미세한 수직 속도 더 천천히 감소 (바운스 지속)
                if (FMath::Abs(CurrentVelocity.Z) > 0.1f && FMath::Abs(CurrentVelocity.Z) < 20.0f) // 범위 확대 (20 -> 50)
                {
                    FVector AdjustedVelocity = CurrentVelocity;
                    AdjustedVelocity.Z *= 1.0f;  // 더 천천히 감소 (0.9 -> 0.98)
                    BallMesh->SetPhysicsLinearVelocity(AdjustedVelocity);
                }
            }

            if (CurrentLandType == ELandType::Green)
            {
                //// 수직 속도 완전 제거
                //FVector HorizontalVelocity = FVector(CurrentVelocity.X, CurrentVelocity.Y, 0.0f);

                //// 그린에서 95% 속도 유지 (현실적)
                //HorizontalVelocity *= 0.95f;

                //// 수평 속도만으로 설정
                //BallMesh->SetPhysicsLinearVelocity(HorizontalVelocity);


                FVector PreservedVelocity = BallMesh->GetPhysicsLinearVelocity();
                PreservedVelocity.X *= 0.97f;
                PreservedVelocity.Y *= 0.97f;
                BallMesh->SetPhysicsLinearVelocity(PreservedVelocity);

            }

        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("⚠️ UpdateBouncePhysics: BallMesh became invalid during execution"));
            return;
        }

        // 디버그 로그 (자주 호출되지 않도록 조건부)
        static float LastLogTime = 0.0f;
        float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

        if (CurrentTime - LastLogTime > 2.0f) // 2초마다만 로그
        {
            UE_LOG(LogTemp, VeryVerbose, TEXT("🎳 Enhanced Rolling: Speed=%.1fcm/s, Friction=%.3f, Terrain=%.1fx, Sensitivity=%.3f"),
                Speed, DynamicFriction, TerrainSensitivityMultiplier, SpeedSensitivity);
            LastLogTime = CurrentTime;
        }
    }
}


void AGolfBall::UpdateStoppedPhysics()
{
    if (BallMesh && BallMesh->IsValidLowLevel())
    {
        FVector CurrentVelocity = BallMesh->GetPhysicsLinearVelocity();
        FVector CurrentAngularVel = BallMesh->GetPhysicsAngularVelocityInDegrees();

        // ✅ 선속도와 각속도 모두 체크
        if (CurrentVelocity.Size() > VELOCITY_EPSILON || CurrentAngularVel.Size() > 0.1f)
        {
            BallMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
            BallMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector, false);

            UE_LOG(LogTemp, Log, TEXT("🛑 Force stopped: Linear=%.2f, Angular=%.2f"),
                CurrentVelocity.Size(), CurrentAngularVel.Size());
        }
    }
}
// 볼스테이트 변경
void AGolfBall::CheckAutoStateTransitions()
{
    float CurrentSpeed = GetBallSpeed();
    float SpeedMS = CurrentSpeed / 100.0f; // m/s 변환

    switch (CurrentBallState)
    {
    case EBallState::Ball_Fly:
    {
        const FVector V = (BallMesh && IsValid(BallMesh))
            ? BallMesh->GetPhysicsLinearVelocity() : FVector::ZeroVector;

        // ① 상승 중이면 절대 Bound로 보내지 않음
        if (V.Z > 50.0f)   // +0.5 m/s 이상 상승
            break;

        // ② 발사 직후 최소 비행 시간 보장
        //    티 높이 2cm < GROUND_CHECK_DISTANCE 5cm 라서
        //    발사 즉시 IsNearGround가 참이 되는 문제 회피
        const float TimeSinceLaunch = GetWorld()->GetTimeSeconds() - LaunchTimeSeconds;
        if (TimeSinceLaunch < MIN_FLIGHT_TIME)
            break;

        if (IsNearGround(GROUND_CHECK_DISTANCE))
        {
            SetBallState(EBallState::Ball_Bound);
            BounceCountOnCurrentTerrain = 0;
            UE_LOG(LogTemp, Log, TEXT("ParkGolf: Fly -> Bound (Speed: %.1f m/s, Vz=%.1f, T+%.3fs)"),
                SpeedMS, V.Z, TimeSinceLaunch);
        }
        break;
    }

    case EBallState::Ball_Bound:
        if (IsNearGround(GROUND_CHECK_DISTANCE))
        {
            const bool bIsRoughTerrain = (CurrentAppliedTerrain == TEXT("Rough"));
            const float RoughBoost = bIsRoughTerrain ? 1.4f : 1.0f;

            float BounceToRollThreshold;
            float BounceToStopThreshold;

            if (SpeedMS > 12.0f)
            {
                BounceToRollThreshold = 80.0f * RoughBoost;
                BounceToStopThreshold = 3.0f;
            }
            else if (SpeedMS > 8.0f)
            {
                BounceToRollThreshold = 100.0f * RoughBoost;
                BounceToStopThreshold = 2.0f;
            }
            else if (SpeedMS > 5.0f)
            {
                BounceToRollThreshold = 120.0f * RoughBoost;
                BounceToStopThreshold = 1.5f;
            }
            else
            {
                BounceToRollThreshold = 150.0f * RoughBoost;
                BounceToStopThreshold = 1.0f;
            }

            // ★ 핵심: Z속도가 양수(위로 이동 중)이면 바운스 중 → 전환 금지
            FVector CurVel = BallMesh->GetPhysicsLinearVelocity();
            if (CurVel.Z > 30.0f)
            {
                break; // 아직 공중 → Bound 유지
            }

            if (CurrentSpeed < BounceToStopThreshold)
            {
                SetBallState(EBallState::Ball_Stop);
                UE_LOG(LogTemp, Log, TEXT("ParkGolf: Bound -> Stop (Speed: %.1f m/s)"), SpeedMS);
            }
            else if (CurrentSpeed < BounceToRollThreshold)
            {
                SetBallState(EBallState::Ball_Rolling);
                UE_LOG(LogTemp, Log,
                    TEXT("ParkGolf: Bound -> Rolling (Speed: %.1f m/s, Threshold: %.1f m/s)"),
                    SpeedMS, BounceToRollThreshold / 100.0f);
            }
        }
        break;

    case EBallState::Ball_Rolling:
        // ⭐ 개선: 굴림에서도 힘에 따른 정지 조건
        float RollingToStopThreshold;

        if (SpeedMS > 3.0f) // 여전히 빠른 굴림
        {
            RollingToStopThreshold = 15.0f; // 0.3m/s 이하에서 정지
        }
        else // 느린 굴림
        {
            RollingToStopThreshold = 6.0f; // 0.15m/s 이하에서 정지
        }

        if (CurrentSpeed < RollingToStopThreshold && IsNearGround(GROUND_CHECK_DISTANCE))
        {
            SetBallState(EBallState::Ball_Stop);
            UE_LOG(LogTemp, Log, TEXT("ParkGolf: Rolling -> Stop (Speed: %.1f m/s)"), SpeedMS);
        }
        break;
    }
}

void AGolfBall::BeginPlay()
{
    Super::BeginPlay();

    // ⭐ UE4 최적화된 물리 재질
    DefaultPhysicalMaterial = CreateOptimizedPhysicalMaterial();
    BallMesh->SetPhysMaterialOverride(DefaultPhysicalMaterial);


    BallMesh->BodyInstance.bOverrideMass = true;
    BallMesh->BodyInstance.SetMassOverride(CachedMass);


    //BallMesh->BodyInstance.SetMassOverride(CachedMass);

    GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());


    // 지형 물리 설정 파일 로드
    LoadTerrainPhysicsConfig();
    UpdatePhysicsParameters();


    //GetWorld()->GetTimerManager().SetTimer(
    //	StateTransitionTimer,
    //	[this]()
    //	{
    //		AdjustBallToGroundLevel();
    //	},
    //	0.1f,
    //		false
    //		);

    // 초기 가시성 설정 - 게임 시작 시 볼 숨김
    SetBallVisibility(false);
    // 원래 콜리젼 설정 저장
    SaveOriginalCollisionSettings();

    LandscapeChecker = ALandscapeChecker::GetLandscapeChecker(GetWorld());
    if (LandscapeChecker)
    {
        UE_LOG(LogTemp, Log, TEXT("✅ AGolfBall LandscapeChecker connected to ball: %s"), *GetName());
        //UpdateCurrentLandType();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ AGolfBall LandscapeChecker not found! Ground type checking disabled for ball: %s"), *GetName());
    }

    LinkedCameraManager = nullptr;

    // ===== 성능 최적화: GroundMarker용 볼 배열 1회 캐시 =====
    // Tick에서 매 프레임 GetAllActorsOfClass 호출을 제거하기 위해 BeginPlay에서 수집
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGolfBall::StaticClass(), CachedGolfBallActors);

    UE_LOG(LogTemp, Log, TEXT("ParkGolfBall: BeginPlay completed with config file system"));
}


void AGolfBall::SetBallColor(const FLinearColor& Color)
{
    if (BallMesh)
    {
        UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(BallMesh->GetMaterial(0), this);
        if (DynamicMaterial)
        {
            DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), Color);
            BallMesh->SetMaterial(0, DynamicMaterial);
            TrailColor = Color; // 트레일 색상도 함께 업데이트 (기존 로직)
            CurrentBallColor = Color; // ⭐ 추가: 내부 변수에 색상 저장
            UE_LOG(LogTemp, Log, TEXT("ParkGolfBall: Set color to %s"), *Color.ToString());
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Failed to create dynamic material for ball color"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("BallMesh is null, cannot set ball color"));
    }
}

void AGolfBall::SetBallState(EBallState NewState)
{
    InternalSetBallState(NewState); // `InternalSetBallState` 호출

}

void AGolfBall::LinkCameraManager(ACameraManager* CameraManager)
{
    if (!IsValid(CameraManager))
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid CameraManager"));
        return;
    }

    LinkedCameraManager = CameraManager;
    NotifyCameraStateChange(CurrentBallState, CurrentBallState);
    UE_LOG(LogTemp, Log, TEXT("✅ Camera linked to ball: %s"), *GetName());
}

void AGolfBall::NotifyCameraStateChange(EBallState PreviousState, EBallState NewState)
{
    if (IsValid(LinkedCameraManager))
    {
        LinkedCameraManager->OnBallStateChangedImmediate(this, PreviousState, NewState);
        UE_LOG(LogTemp, Warning, TEXT("📢 [VERIFY] CameraManager notified successfully"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ [VERIFY] LinkedCameraManager is INVALID!"));
    }
}

void AGolfBall::RequestCameraSync()
{
    UE_LOG(LogTemp, Log, TEXT("🔄 Manual camera sync requested"));
    NotifyCameraStateChange(CurrentBallState, CurrentBallState);
#if WITH_EDITOR

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan,
            TEXT("📷 Camera sync requested"));
    }
#endif
}

void AGolfBall::HandleStateTransition(EBallState PreviousState, EBallState NewState)
{
    // ===== 가시성 처리 추가 =====
    HandleVisibilityOnStateChange(PreviousState, NewState);
    //FCR2BallPosition SensorBallPosition;
        // ✅ 추가: 어떤 경로로 전환되든 Rolling/Stop 진입 시 바운스 카운터 리셋
    // (기존엔 CheckAutoStateTransitions의 1537줄 한 곳에만 있었음 → 21개 전환 지점 전부 커버)
    if (NewState == EBallState::Ball_Rolling || NewState == EBallState::Ball_Stop)
    {
        if (BounceCountOnCurrentTerrain != 0)
        {
            UE_LOG(LogTemp, Log,
                TEXT("🔄 바운스 카운터 리셋: %d → 0 (전환: %s → %s)"),
                BounceCountOnCurrentTerrain,
                *UEnum::GetValueAsString(PreviousState),
                *UEnum::GetValueAsString(NewState));
        }
        BounceCountOnCurrentTerrain = 0;
    }

    if (NewState == EBallState::Ball_Rolling)
    {
        // 최대 굴림 시간 타이머 설정
        GetWorld()->GetTimerManager().SetTimer(
            MaxRollingTimer,
            [this]() {
                UE_LOG(LogTemp, Warning, TEXT("Max rolling time reached, forcing stop"));
                BallMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
                SetBallState(EBallState::Ball_Stop);
            },
            MaxRollingDuration,
            false
        );
    }
    else
    {
        // 다른 상태로 전환시 타이머 클리어
        GetWorld()->GetTimerManager().ClearTimer(MaxRollingTimer);
    }

    switch (NewState)
    {
    case EBallState::Ball_Ready:
        BallMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
        BallMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
        UE_LOG(LogTemp, Log, TEXT("Ball transitioned to Ready state"));
        break;

    case EBallState::Ball_Stop:
        HandleBallStopped();
        break;

    case EBallState::Ball_Fly:
        bWasInAir = true;
        // ✅ Fly 진입: 다음에 지면에 닿으면 즉시 갱신되도록 Dirty
        bLandTypeDirty = true;
        UE_LOG(LogTemp, Log, TEXT("Ball transitioned to Flying state"));
        break;

    case EBallState::Ball_Bound:
        bWasInAir = false;
        // ✅ Bound 진입: 착지 순간 지형 즉시 판정
        bLandTypeDirty = true;
        UE_LOG(LogTemp, Log, TEXT("Ball transitioned to Bound state"));
        break;

    case EBallState::Ball_Rolling:
        // ✅ Rolling 진입: 굴림 시작 지점 지형 즉시 판정
        bLandTypeDirty = true;
        LandTypeLastCheckPos = GetActorLocation(); // 기준점 리셋
        HandleRollingStateEnter();
        break;
    }
}

void AGolfBall::CalculateRoundStat()
{
    //ballstop 이후 작동해야함
    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    if (GameMode)
    {
        AGolfPlayer* OwningPlayer = GameMode->FindPlayer(OwningPlayerIndex);

        if (OwningPlayer)
        {
            TArray<FPlayerInfo> PlayerInfos = GameMode->GameInfo.Players;

            // 랭킹: 동일 점수는 동일 순위, 동점 안정화는 SlotIndex로 결정
            struct FRankEntry
            {
                int32 SlotIndex = -1;
                int32 TotalScore = 0;
            };
            TArray<FRankEntry> RankEntries;
            RankEntries.Reserve(PlayerInfos.Num());
            for (const FPlayerInfo& Info : PlayerInfos)
            {
                FRankEntry Entry;
                Entry.SlotIndex = Info.SlotIndex;
                Entry.TotalScore = Info.TotalScore;
                RankEntries.Add(Entry);
            }

            RankEntries.Sort([](const FRankEntry& A, const FRankEntry& B)
                {
                    if (A.TotalScore != B.TotalScore)
                    {
                        return A.TotalScore < B.TotalScore;
                    }
                    return A.SlotIndex < B.SlotIndex;
                });

            TMap<int32, int32> RankBySlot;
            RankBySlot.Reserve(RankEntries.Num());
            for (int32 i = 0; i < RankEntries.Num(); ++i)
            {
                const int32 CurrentRank = i + 1; // unique ranking (1,2,3...)
                RankBySlot.Add(RankEntries[i].SlotIndex, CurrentRank);
            }

            // 모든 플레이어의 Rank 동기화
            if (GameMode->PlayerManager)
            {
                for (AGolfPlayer* Player : GameMode->PlayerManager->GetPlayers())
                {
                    if (!Player) continue;
                    if (const int32* Rank = RankBySlot.Find(Player->PlayerInfo.SlotIndex))
                    {
                        Player->RoundStat.Rank = *Rank;
                        Player->PlayerInfo.RoundStat.Rank = *Rank;
                    }
                }
            }
            for (FPlayerInfo& Info : GameMode->GameInfo.Players)
            {
                if (const int32* Rank = RankBySlot.Find(Info.SlotIndex))
                {
                    Info.RoundStat.Rank = *Rank;
                }
            }

            //최장 티샷
            if (CheckWasTeeShot())
            {
                FVector BeforePos = OwningPlayer->BEFOREPos;
                FVector CurrentPos = GetActorLocation();
                float Distance = FVector::Dist2D(BeforePos, CurrentPos);

                if (MaxDistanceTeeShot < Distance)
                {
                    MaxDistanceTeeShot = Distance;
                }

                OwningPlayer->RoundStat.MaxDistance = MaxDistanceTeeShot;
            }

            //티샷 평균 거리
            if (TeeShotDistanceArray.Num() > 0)
            {
                float TotalDistance = 0.f;
                for (float Distance : TeeShotDistanceArray)
                {
                    TotalDistance += Distance;
                }

                OwningPlayer->RoundStat.AverageDistanceOfDriver = TotalDistance / TeeShotDistanceArray.Num();
            }

            //페어웨이 안착률
            if (TeeShotSettlementArray.Num() > 0)
            {
                float FairwayCount = 0;

                for (ELandType LandType : TeeShotSettlementArray)
                {
                    if (LandType == ELandType::Fairway)
                    {
                        FairwayCount++;
                    }
                }

                if (FairwayCount > 0)
                    OwningPlayer->RoundStat.FairwayArccuracy = FairwayCount / (float)TeeShotSettlementArray.Num();
            }

            //그린 10m 안착률, 퍼트 수
            if (ShotInfoArray.Num() > 0)
            {
                float GreenSuccessCount = 0.f;
                int32 PuttCount = 0.f;
                for (FShotInfo ShotInfo : ShotInfoArray)
                {
                    if (ShotInfo.StopLocationLandType == ELandType::Green)
                    {
                        if (FVector::Dist2D(ShotInfo.ShotLocation, ShotInfo.StopLocation) > 10.f * M_TO_CM)
                        {
                            GreenSuccessCount++;
                        }
                    }

                    if (FVector::Dist2D(ShotInfo.ShotLocation, GetCurrentHolePosition()) <= 10.f * M_TO_CM)
                    {
                        PuttCount++;
                    }
                }

                if (GreenSuccessCount > 0)
                    OwningPlayer->RoundStat.GreenArccuracy = GreenSuccessCount / ShotInfoArray.Num();

                OwningPlayer->RoundStat.PuttCount = PuttCount;

                FPlayerInfo FoundedPlayerInfo = GM->FindPlayerInfo(OwningPlayer->SlotIndex);
                OwningPlayer->RoundStat.ShotCount = 0;
                for (int32 i = 0; i < GM->CurrentHole; i++)
                {
                    OwningPlayer->RoundStat.ShotCount += FoundedPlayerInfo.ShotCountPerHole[i];
                }
            }
            UE_LOG(LogTemp, Log, TEXT("RoundStat : 랭킹 = %d, 타수 = %d, 평균 드라이버 거리 = %.1f, 최장타거리 = %.1f, 페어웨이 안착률 = %.2f, 그린 적중률 = %.2f, 퍼트 수 합계 = %d")
                , OwningPlayer->RoundStat.Rank,
                OwningPlayer->RoundStat.ShotCount,
                OwningPlayer->RoundStat.AverageDistanceOfDriver,
                OwningPlayer->RoundStat.MaxDistance,
                OwningPlayer->RoundStat.FairwayArccuracy,
                OwningPlayer->RoundStat.GreenArccuracy,
                OwningPlayer->RoundStat.GreenPuttCount);
            OwningPlayer->PlayerInfo.RoundStat = OwningPlayer->RoundStat;
        }

        for (FPlayerInfo& PlayerInfo : GM->GameInfo.Players)
        {
            if (PlayerInfo.SlotIndex == OwningPlayer->SlotIndex)
            {
                PlayerInfo.RoundStat = OwningPlayer->RoundStat;
            }
        }
    }
}

float AGolfBall::GetShotDistance()
{
    AGolfPlayer* OwningPlayer = GM->FindPlayer(OwningPlayerIndex);

    return FVector::Dist2D(OwningPlayer->BEFOREPos, GetActorLocation());
}

void AGolfBall::HandleBallStopped()
{
    if (bOverlapHoleIn)
        return;

    if (bIsTrackingTrajectory)
        StopTrajectoryTracking();

    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    // Avoid overwriting runtime state during active play

    FTimerHandle TH1;
    GetWorld()->GetTimerManager().SetTimer(
        TH1,
        [this, GameMode]() {
            GameMode->BallDistanceWidget->SetRenderOpacity(0.f);
        }, 0.9f, false
    );

    if (GameMode->CurrentGameMode == EGolfGameMode::StrokeMode)
    {
        bool bHoleInResult = CheckHoleIn();
        bool bConcededResult = !bHoleInResult && CheckConcedeDistance();
        bool bOutOfBoundsResult = !bHoleInResult && !bConcededResult && CheckOutOfBounds();

        // 내부 플래그 설정
        if (bHoleInResult)
            SetHoleIn(true);
        else if (bConcededResult) {
            SetConceded(true);
        }

        bIsOutOfBounds = bOutOfBoundsResult;

        if (GameMode->GetCurrentTurnGolfPlayer()->bLastShotOB == false)
        {
            GameMode->GetCurrentTurnGolfPlayer()->bLastShotOB = bIsOutOfBounds;
        }

        UE_LOG(LogTemp, Log, TEXT("🛑 Ball stopped: HoleIn=%s, Conceded=%s, OutOfBounds=%s, BallState=%s"),
            bHoleInResult ? TEXT("True") : TEXT("False"),
            bConcededResult ? TEXT("True") : TEXT("False"),
            bOutOfBoundsResult ? TEXT("True") : TEXT("False"),
            *UEnum::GetValueAsString(CurrentBallState));

        // InGameMode가 수신할 상위 수준 이벤트 브로드캐스트
        if (bHoleInResult)
        {
            OnBallGameFlowEvent.Broadcast(EBallEvent::HoleIn); // HoleIn 이벤트 브로드캐스트
            GameMode->PlayerManager->UpdateGameInfoBallPos();
        }
        else if (bOutOfBoundsResult)
        {
            // ✅ DoublePar 미리 계산 (벌타 추가 전 예상치)
            AGolfPlayer* OwningPlayer = GM->FindPlayer(OwningPlayerIndex);
            int32 HoleIdx = GameMode->CurrentHole - 1;
            int32 ParScore = GameMode->MapInfo.ParScores[HoleIdx];
            int32 ShotsAfterPenalty = OwningPlayer->GetCurrentHoleShotCount() + 2;
            bool  bWillBeDoublePar = (ShotsAfterPenalty >= ParScore * 2 - 1);

            UE_LOG(LogTemp, Warning,
                TEXT("🔢 [OB] 현재 %d타 + 벌타2 = %d타 / DoublePar기준 Par%d×2=%d → %s"),
                OwningPlayer->GetCurrentHoleShotCount(),
                ShotsAfterPenalty, ParScore, ParScore * 2 - 1,
                bWillBeDoublePar ? TEXT("DoublePar!") : TEXT("드롭"));

            if (bWillBeDoublePar)
            {
                // ─────────────────────────────────────────────
                // ✅ DoublePar 확정: 드롭 없이 즉시 HoleOut 처리
                // ─────────────────────────────────────────────

                // 벌타 즉시 추가
                OwningPlayer->IncrementShotCount(); // +1
                OwningPlayer->IncrementShotCount(); // +2
                OwningPlayer->PlayerInfo.ShotCountPerHole[HoleIdx] = ParScore * 2;

                // OB 플래그 해제 → OnEnterResultsState 에서 OB 경로 타지 않음
                OwningPlayer->bLastShotOB = false;
                bIsOutOfBounds = false;

                // HoleOut 처리
                OwningPlayer->SetHoleIn(true);
                SetHoleIn(true);

                // HoleIn 이벤트 브로드캐스트
                // → HandleBallGameFlowEvent 에서 Player_HoleOut 으로 처리됨
                OnBallGameFlowEvent.Broadcast(EBallEvent::HoleIn);
                GameMode->PlayerManager->UpdateGameInfoBallPos();

                UE_LOG(LogTemp, Warning,
                    TEXT("⛳ [OB DoublePar] Player %d → Par%d×2=%d타 양파, 드롭 없이 HoleOut"),
                    OwningPlayerIndex, ParScore, ParScore * 2);
            }
            else
            {
                // ─────────────────────────────────────────────
                // ✅ DoublePar 아님: 기존 OB 드롭 흐름 유지
                // ─────────────────────────────────────────────
                OnBallGameFlowEvent.Broadcast(EBallEvent::OutOfBounds);

                // OB 판정 위젯 1초 후 표시
                FTimerHandle TH;
                GetWorld()->GetTimerManager().SetTimer(TH, [this, GameMode]()
                    {
                        GameMode->ShotResultWidgetInstance->PlayShotResult_OB();
                    }, 1.f, false);
            }
        }
        else if (bConcededResult)
        {
            OnBallGameFlowEvent.Broadcast(EBallEvent::Conceded); // Conceded 이벤트 브로드캐스트
            GameMode->PlayerManager->UpdateGameInfoBallPos();
        }
        else
        {
            OnBallGameFlowEvent.Broadcast(EBallEvent::BallStopped); // BallStopped 이벤트 브로드캐스트
            FTimerHandle TH;
            GetWorld()->GetTimerManager().SetTimer(
                TH,
                [this, GameMode]() {
                    GameMode->ShotResultWidgetInstance->PlayShotResult(CurrentLandType);
                }, 1.f, false
            );

            GameMode->PlayerManager->UpdateGameInfoBallPos();
        }

        //샷 정보 저장
        if (CheckWasTeeShot())
        {
            TeeShotDistanceArray.Add(GetShotDistance());
            TeeShotSettlementArray.Add(CurrentLandType);
        }

        AGolfPlayer* OwningPlayer = GM->FindPlayer(OwningPlayerIndex);
        FShotInfo ShotInfo;
        ShotInfo.ShotLocation = OwningPlayer->BEFOREPos;
        ShotInfo.ShotLocationLandType = LandscapeChecker->GetLandTypeAtLocation(OwningPlayer->BEFOREPos);
        ShotInfo.StopLocation = GetActorLocation();
        ShotInfo.StopLocationLandType = CurrentLandType;
        ShotInfoArray.Add(ShotInfo);

        //TODO: 여기서 크래시남 (이어하기 이후) ShotInfoArray

        CalculateRoundStat();
        GM->InGameScoreBoardStatWidgetInstance->UpdateScoreBoardStats();
        // 실제 상태 변경을 위해 InternalSetBallState 호출 (필요한 경우, 그렇지 않으면 GameMode가 명령하도록 의존)
        InternalSetBallState(EBallState::Ball_Stop); // 볼의 내부 상태를 `Ball_Stop`으로 설정
        if (GameMode->GetCurrentSlot())
            GameMode->GetCurrentSlot()->bBlinking = false;

    }
    else if (GameMode->CurrentGameMode == EGolfGameMode::TrainingMode)
    {
        OnBallGameFlowEvent.Broadcast(EBallEvent::BallStopped); // BallStopped 이벤트 브로드캐스트       
        InternalSetBallState(EBallState::Ball_Stop); // 볼의 내부 상태를 `Ball_Stop`으로 설정
        if (GameMode->GetCurrentSlot())
            GameMode->GetCurrentSlot()->bBlinking = false;

    }
    else if (GameMode->CurrentGameMode == EGolfGameMode::RangeMode)
    {
        // AGolfPlayerController* PlayerController = GameMode->PlayerManager->GetPlayerController();
        // PlayerController->ShowSwingVideoWidget();


        OnBallGameFlowEvent.Broadcast(EBallEvent::BallStopped); // BallStopped 이벤트 브로드캐스트
        InternalSetBallState(EBallState::Ball_Stop); // 볼의 내부 상태를 `Ball_Stop`으로 설정
    }
}

float AGolfBall::CalculateTurnTransitionDelay(bool bHoleIn, bool bOutOfBounds)
{
    if (bHoleIn) return 6.0f;
    if (bOutOfBounds) return 0.0f;

    if (AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
    {
        if (GameMode->CurrentHole >= GameMode->MapInfo.HoleCount)
        {
            return 5.0f;
        }
    }

    return TURN_TRANSITION_DELAY;
}

void AGolfBall::OnComponentEndOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex)
{
    if (OtherComp) OverlapLocked.Remove(OtherComp);
}

bool AGolfBall::IsWorldStaticComponent(const UPrimitiveComponent* Comp)
{
    if (!Comp) return false;

    // 충돌이 완전히 꺼져있으면 제외
    if (Comp->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
        return false;

    // 핵심: Object Type이 WorldStatic 인지
    return (Comp->GetCollisionObjectType() == ECC_WorldStatic);
}


void AGolfBall::OnComponentBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{

    // ★ 진단용: 무조건 찍히는 로그 (게임모드 무관)
    UE_LOG(LogTemp, Warning, TEXT("[DEBUG] OnComponentBeginOverlap CALLED: Other=%s, Comp=%s, GM=%s, Mode=%d"),
        OtherActor ? *OtherActor->GetName() : TEXT("null"),
        OtherComp ? *OtherComp->GetName() : TEXT("null"),
        GM ? TEXT("valid") : TEXT("NULL"),
        GM ? (int32)GM->CurrentGameMode : -1);

    if (OverlapLocked.Contains(OtherComp)) return;
    OverlapLocked.Add(OtherComp);

    const FString Name = OtherActor->GetActorNameOrLabel();    // 런타임에서 안전
    // HoleIn: match "Cup_hole" OR "green_hole" (case-insensitive) + has digit anywhere in name
    auto IsHoleCupActor = [](const FString& N) -> bool {
        bool bMatch = N.Contains(TEXT("green_hole"), ESearchCase::IgnoreCase);
        if (!bMatch) return false;
        for (TCHAR Ch : N) { if (FChar::IsDigit(Ch)) return true; }
        return false;
        };

    switch (GM->CurrentGameMode)
    {
    case EGolfGameMode::StrokeMode:
        UE_LOG(LogTemp, Log, TEXT("[Ball] Overlap with WorldStatic component: %s (Actor: %s)"), *OtherComp->GetName(), *OtherActor->GetName());

        if (!IsHoleCupActor(Name)) return;

        // ★ 컴포넌트 이름에 "trigger"가 포함된 경우에만 홀인 처리
        if (!OtherComp->GetName().Contains(TEXT("trigger"), ESearchCase::IgnoreCase)) break;

        if (OtherActor)
        {
            UE_LOG(LogTemp, Log, TEXT("[Ball] HOLEIN : (Actor: %s)"), *OtherActor->GetName());
            if (auto* SM = GetGameInstance()->GetSubsystem<USoundManager>())
            {
                SM->PlayAtLocation_ById("Effect.Ball.HoleIn", OtherActor->GetActorLocation(), 2.5f);
                bIsHoleIn = true;
                FTimerHandle TH;
                GetWorld()->GetTimerManager().SetTimer(TH,
                    FTimerDelegate::CreateLambda([this]()
                        {
                            SetBallState(EBallState::Ball_Stop);
                        }
                    ), 1.f, false);
            }
        }
        break;

    case EGolfGameMode::TrainingMode:
        UE_LOG(LogTemp, Log, TEXT("[Ball] Overlap with WorldStatic component: %s (Actor: %s)"), *OtherComp->GetName(), *OtherActor->GetName());

        if (!IsHoleCupActor(Name)) return;

        // digit check handled inside IsHoleCupActor

        // (digit guard replaced by IsHoleCupActor)

                // ★ 컴포넌트 이름에 "trigger"가 포함된 경우에만 홀인 처리
        if (!OtherComp->GetName().Contains(TEXT("trigger"), ESearchCase::IgnoreCase)) break;

        if (OtherActor)
        {
            if (auto* SM = GetGameInstance()->GetSubsystem<USoundManager>())
            {
                SM->PlayAtLocation_ById("Effect.Ball.HoleIn", OtherActor->GetActorLocation(), 2.5f);
                bIsHoleIn = true;

                if (GM->GameInfo.bEventHole &&
                    GM->CurrentHole == 1)
                {
                    FTimerHandle TH;
                    GetWorld()->GetTimerManager().SetTimer(
                        TH,
                        [this]() {
                            GM->ResultWidgetInstance->PlayResult(101);
                        }, 1.f, false
                    );

                    FTimerHandle TH2;
                    GetWorld()->GetTimerManager().SetTimer(
                        TH2,
                        [this]() {
                            OnBallGameFlowEvent.Broadcast(EBallEvent::BallStopped); // BallStopped 이벤트 브로드캐스트       
                            InternalSetBallState(EBallState::Ball_Stop); // 볼의 내부 상태를 `Ball_Stop`으로 설정
                            GM->GetCurrentSlot()->bBlinking = false;
                        }, 5.f, false
                    );
                }
                else
                {
                    SM->PlayAtLocation_ById("Effect.Ball.HoleIn", OtherActor->GetActorLocation(), 2.5f);
                    bIsHoleIn = true;

                    FTimerHandle TH;
                    GetWorld()->GetTimerManager().SetTimer(TH,
                        FTimerDelegate::CreateLambda([this]()
                            {
                                OnBallGameFlowEvent.Broadcast(EBallEvent::BallStopped); // BallStopped 이벤트 브로드캐스트       
                                InternalSetBallState(EBallState::Ball_Stop); // 볼의 내부 상태를 `Ball_Stop`으로 설정
                                GM->GetCurrentSlot()->bBlinking = false;
                            }
                        ), 1.f, false);
                }
            }
        }
        break;

    case EGolfGameMode::RangeMode:

        // ★ 컴포넌트 이름에 "trigger"가 포함된 경우에만 홀인 처리
        if (!OtherComp->GetName().Contains(TEXT("trigger"), ESearchCase::IgnoreCase)) break;

        if (Name.Contains(TEXT("holecup")))
        {
            if (OtherActor)
            {
                if (auto* SM = GetGameInstance()->GetSubsystem<USoundManager>())
                {
                    SM->PlayAtLocation_ById("Effect.Ball.HoleIn", OtherActor->GetActorLocation(), 2.5f);
                }
            }
        }
    default:
        break;
    }


}

void AGolfBall::StartTurnTransitionCountdown(float DelayTime)
{
    if (DelayTime <= 0.0f)
    {
        DelayTime = TURN_TRANSITION_DELAY;
    }

    TurnTransitionCountdown = DelayTime;
    TurnTransitionMaxTime = DelayTime;

    GetWorld()->GetTimerManager().SetTimer(
        CountdownUpdateTimer,
        [WeakThis = TWeakObjectPtr<AGolfBall>(this)]()
        {
            if (WeakThis.IsValid() && !WeakThis->bIsBeingDestroyed)
            {
                WeakThis->UpdateTurnTransitionCountdown();
            }
        },
        1.0f,
        true
    );

    GetWorld()->GetTimerManager().SetTimer(
        ResetReadyTimer,
        this,
        &AGolfBall::ResetToReady,
        DelayTime,
        false
    );

    UE_LOG(LogTemp, Log, TEXT("🕐 Turn transition countdown started: %.0f seconds"), DelayTime);
}

void AGolfBall::UpdateTurnTransitionCountdown()
{

    // ⭐ 콜백 시작 시 유효성 체크
    if (bIsBeingDestroyed || !IsValid(this) || !GetWorld())
    {
        // 타이머 정리
        if (GetWorld())
        {
            GetWorld()->GetTimerManager().ClearTimer(CountdownUpdateTimer);
        }
        return;
    }

    TurnTransitionCountdown -= 1.0f;

    if (TurnTransitionCountdown > 0.0f)
    {
        int32 SecondsLeft = FMath::RoundToInt(TurnTransitionCountdown);

        FColor DisplayColor;
        if (SecondsLeft <= 1)
            DisplayColor = FColor::Red;
        else if (SecondsLeft <= 2)
            DisplayColor = FColor::Orange;
        else
            DisplayColor = FColor::Yellow;
#if WITH_EDITOR

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(999, 1.1f, DisplayColor,
                FString::Printf(TEXT("⏰ Next Turn: %d seconds"), SecondsLeft));
        }
#endif
    }
    else
    {
        GetWorld()->GetTimerManager().ClearTimer(CountdownUpdateTimer);
        TurnTransitionCountdown = 0.0f;
    }
}

// ===== 샷 시스템 =====
FVector AGolfBall::CalculateShotVelocity(const FVector& Direction, float SpeedMS) const
{
    if (!Direction.IsNormalized())
    {
        UE_LOG(LogTemp, Error, TEXT("Unnormalized Direction: %s"), *Direction.ToString());
        return FVector::ZeroVector;
    }
    if (SpeedMS <= 0.0f || FMath::IsNaN(SpeedMS))
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid Power: %.2f"), SpeedMS);
        return FVector::ZeroVector;
    }
    return Direction * SpeedMS * (100.0f * 1.0f); // m/s → cm/s
}

FVector AGolfBall::CalculateShotDirectionWithElevation(const FVector& BaseDirection, float YawDegrees)
{
    FVector TerrainNormal = GetTerrainNormal();
    FVector UpVector = TerrainNormal;

    // Yaw 적용: BaseDirection을 Yaw만큼 회전 (수평 방향 조정)
    FVector AdjustedDirection = BaseDirection.GetSafeNormal();
    if (!FMath::IsNearlyZero(YawDegrees))
    {
        // Yaw 회전을 위한 로테이터 생성 (Yaw 축만 사용)
        FRotator YawRotation(0.0f, YawDegrees, 0.0f);

        // BaseDirection을 로컬 공간에서 회전 (지형 노멀을 기준으로)
        FMatrix RotationMatrix = FRotationMatrix::MakeFromZX(UpVector, AdjustedDirection);
        AdjustedDirection = YawRotation.RotateVector(AdjustedDirection);

        // 안전성 검사 (NaN 방지)
        if (AdjustedDirection.ContainsNaN() || !AdjustedDirection.IsNormalized())
        {
            UE_LOG(LogTemp, Warning, TEXT("⚠️ Invalid AdjustedDirection after Yaw rotation. Falling back to original."));
            AdjustedDirection = BaseDirection.GetSafeNormal();
        }
    }

    // 지형에 투영된 ForwardVector 계산
    FVector ForwardVector = FVector::CrossProduct(UpVector, FVector::CrossProduct(AdjustedDirection, UpVector)).GetSafeNormal();

    // 기본 발사 각도 적용 (TYPICAL_LAUNCH_ANGLE 사용)
    float LaunchFactor = ParkGolfConstants.TYPICAL_LAUNCH_ANGLE / 90.0f;
    FVector LaunchDirection = FMath::Lerp(ForwardVector, UpVector, LaunchFactor);

    // 최종 방향 정규화 및 반환
    FVector FinalDirection = LaunchDirection.GetSafeNormal();
    if (FinalDirection.ContainsNaN() || !FinalDirection.IsNormalized())
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Invalid LaunchDirection. Returning fallback."));
        return ForwardVector; // 폴백
    }

    UE_LOG(LogTemp, Verbose, TEXT("🎯 Shot Direction (Yaw=%.2f): %s"), YawDegrees, *FinalDirection.ToString());
    return FinalDirection;
}

FVector AGolfBall::CalculateShotDirectionWithElevation(const FVector& BaseDirection, float LaunchAngleDegrees, float YawDegrees)
{
    // 지면 정보 수집
    FVector TerrainNormal = GetAccurateTerrainNormal();
    FVector UpVector = TerrainNormal; // 지면 법선을 업벡터로 사용
    float GroundSlopeAngle = GetGroundSlopeAngle();

    UE_LOG(LogTemp, Log, TEXT("🏔️ Ground Analysis: SlopeAngle=%.2f°, Normal=%s"),
        GroundSlopeAngle, *TerrainNormal.ToString());

    // 1. Yaw 적용: BaseDirection을 Yaw만큼 회전 (기존 코드와 동일)
    FVector AdjustedDirection = BaseDirection.GetSafeNormal();
    if (!FMath::IsNearlyZero(YawDegrees))
    {
        FRotator YawRotation(0.0f, YawDegrees, 0.0f);
        AdjustedDirection = YawRotation.RotateVector(AdjustedDirection);

        if (AdjustedDirection.ContainsNaN() || !AdjustedDirection.IsNormalized())
        {
            UE_LOG(LogTemp, Warning, TEXT("⚠️ Invalid AdjustedDirection after Yaw rotation. Falling back to original."));
            AdjustedDirection = BaseDirection.GetSafeNormal();
        }
    }

    // 2. 지형에 투영된 ForwardVector 계산 (기존 방식 사용)
    FVector ForwardVector = FVector::CrossProduct(UpVector, FVector::CrossProduct(AdjustedDirection, UpVector)).GetSafeNormal();

    // 3. 지면 경사를 고려한 발사각도 조정
    float EffectiveLaunchAngle = LaunchAngleDegrees + 5;
    if (EffectiveLaunchAngle < 5.0f) {  // 낮은 각도일 때 바운스 강조를 위한 약간의 업 조정
        EffectiveLaunchAngle += 1.0f;  // 1도 추가: 너무 평평하지 않게
    }

    if (GroundSlopeAngle > 1.0f) // 경사가 1도 이상일 때만 보정
    {
        // 샷 방향과 지면 경사의 관계 계산
        FVector HorizontalDirection = FVector(AdjustedDirection.X, AdjustedDirection.Y, 0.0f).GetSafeNormal();
        FVector GroundProjection = FVector(TerrainNormal.X, TerrainNormal.Y, 0.0f).GetSafeNormal();

        // 경사 방향 계산 (지면이 기울어진 방향)
        float DirectionAlignment = FVector::DotProduct(HorizontalDirection, -GroundProjection);

        // 경사 효과 적용
        // 오르막 방향으로 칠 때: 양수 (각도 증가)
        // 내리막 방향으로 칠 때: 음수 (각도 감소)
        float SlopeEffect = GroundSlopeAngle * DirectionAlignment * 1.0f; // 50% 영향도
        EffectiveLaunchAngle += SlopeEffect;

        // 안전 범위로 클램프
        EffectiveLaunchAngle = FMath::Clamp(EffectiveLaunchAngle,
            ParkGolfConstants.MIN_LAUNCH_ANGLE,
            ParkGolfConstants.MAX_LAUNCH_ANGLE * 1.2f);

        UE_LOG(LogTemp, Log, TEXT("🎯 Slope adjustment: %.2f° → %.2f° (effect: %.2f)"),
            LaunchAngleDegrees, EffectiveLaunchAngle, SlopeEffect);
    }

    // 4. 발사 방향 계산 (기존 방식 사용)
    float LaunchFactor = EffectiveLaunchAngle / 90.0f;
    FVector LaunchDirection = FMath::Lerp(ForwardVector, UpVector, LaunchFactor);

    // 5. 최종 방향 정규화 및 검증
    FVector FinalDirection = LaunchDirection.GetSafeNormal();
    if (FinalDirection.ContainsNaN() || !FinalDirection.IsNormalized())
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Invalid LaunchDirection. Returning fallback."));
        return ForwardVector;
    }

    // 디버그 정보
    float ActualLaunchAngle = FMath::RadiansToDegrees(
        FMath::Asin(FMath::Clamp(FinalDirection.Z, -1.0f, 1.0f))
    );

    UE_LOG(LogTemp, Log, TEXT("🚀 Final Launch: Direction=%s, ActualAngle=%.2f°"),
        *FinalDirection.ToString(), ActualLaunchAngle);

    return FinalDirection;
}

float AGolfBall::CalculateExpectedDistance(float SpeedMS, float LaunchAngleDegrees) const
{
    float ClampedSpeed = FMath::Clamp(SpeedMS, ParkGolfConstants.MIN_SPEED, ParkGolfConstants.MAX_SPEED);
    float ClampedAngle = FMath::Clamp(LaunchAngleDegrees, ParkGolfConstants.MIN_LAUNCH_ANGLE, ParkGolfConstants.MAX_LAUNCH_ANGLE);

    float LaunchAngleRad = FMath::DegreesToRadians(ClampedAngle);
    float Gravity = 9.81f;






















    float TheoreticalDistance = (ClampedSpeed * ClampedSpeed * FMath::Sin(2.0f * LaunchAngleRad)) / Gravity;

    float RealWorldFactor = 0.85f - (PhysicsConfig.AirResistance * 10.0f);

    float AngleEfficiency = 1.0f;
    if (ClampedAngle < 3.0f)
    {
        AngleEfficiency = 0.8f + (ClampedAngle / 3.0f) * 0.2f;
    }
    else if (ClampedAngle > 8.0f)
    {
        AngleEfficiency = 1.0f - ((ClampedAngle - 8.0f) / 4.0f) * 0.3f;
    }

    float EstimatedDistance = TheoreticalDistance * RealWorldFactor * AngleEfficiency;

    UE_LOG(LogTemp, Log, TEXT("Distance calc: %.1fm/s @ %.1f° = %.1fm (theory: %.1fm)"),
        ClampedSpeed, ClampedAngle, EstimatedDistance, TheoreticalDistance);

    return EstimatedDistance;
}
bool AGolfBall::CheckTeeShot()
{
    if (GM)
    {
        float BallLocX = FMath::TruncToInt(GetActorLocation().X);
        float BallLocY = FMath::TruncToInt(GetActorLocation().Y);
        float TeeBoxLocX = FMath::TruncToInt(GM->MapInfo.TeePositions[GM->CurrentHole - 1].X);
        float TeeBoxLocY = FMath::TruncToInt(GM->MapInfo.TeePositions[GM->CurrentHole - 1].Y);

        if (BallLocX == TeeBoxLocX && BallLocY == TeeBoxLocY)
        {
            return true;
        }
    }

    return false;
}

bool AGolfBall::CheckWasTeeShot()
{
    if (GM)
    {
        AGolfPlayer* Player = GM->FindPlayer(OwningPlayerIndex);
        float BallLocX = FMath::TruncToInt(Player->BEFOREPos.X);
        float BallLocY = FMath::TruncToInt(Player->BEFOREPos.Y);
        float TeeBoxLocX = FMath::TruncToInt(GM->MapInfo.TeePositions[GM->CurrentHole - 1].X);
        float TeeBoxLocY = FMath::TruncToInt(GM->MapInfo.TeePositions[GM->CurrentHole - 1].Y);

        if (BallLocX == TeeBoxLocX && BallLocY == TeeBoxLocY)
        {
            return true;
        }
    }

    return false;
}


float AGolfBall::GetHoleDistance() const
{
    if (!GM || !IsValid(GM) || !GM->GameInfo.SelectedMap.HolecupPositions.IsValidIndex(GM->CurrentHole - 1))
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ GetHoleDistance: Invalid GM or Hole data"));
        return 999999.0f;  // 무한대 반환 (크로스헤어 숨김)
    }

    FVector HolecupPosition = GM->GameInfo.SelectedMap.HolecupPositions[GM->CurrentHole - 1];
    FVector BallPosition = GetActorLocation();
    float DistanceCm = FVector::Dist(BallPosition, HolecupPosition);

    UE_LOG(LogTemp, VeryVerbose, TEXT("📏 Hole Distance: %.1fm"), DistanceCm / 100.0f);
    return DistanceCm;
}

void AGolfBall::ApplyShot(const FVector& Direction, float PowerPercent)
{
    // PowerPercent를 m/s로 변환 (설정 파일 값 사용)
   // float SpeedMS = ParkGolfConstants.MIN_SPEED + (ParkGolfConstants.MAX_SPEED - ParkGolfConstants.MIN_SPEED) *  FMath::Clamp(PowerPercent / 100.0f, 0.0f, 1.0f);

    float adjustSpeed = 1.0f; // 1.2f
    if (CurrentLandType == ELandType::Sand) adjustSpeed = 0.7f;

    if (CheckTeeShot())
    {
        
        adjustSpeed = PhysicsConfig.TeeShotPowerModify;
        UE_LOG(LogTemp, Log, TEXT("ApplyShot :: TeeShot!--- adjustSpeed = %f "), adjustSpeed);
    }
    else
    {
        adjustSpeed = PhysicsConfig.SecondShotPowerModify;
        UE_LOG(LogTemp, Log, TEXT("ApplyShot :: SecondShot!  --- adjustSpeed = %f "), adjustSpeed);
        
    }

    UE_LOG(LogTemp, Log, TEXT("---------------------- ApplyShot :: adjustSpeed = %f "), adjustSpeed);

    float SpeedMS = PowerPercent * adjustSpeed;  // 10% 감속
    // 기본 각도 사용 (설정 파일 값)
    float DefaultAngle = ParkGolfConstants.TYPICAL_LAUNCH_ANGLE;
    float YawAngle = ParkGolfConstants.TYPICAL_LAUNCH_ANGLE;

    // PlayerController에서 각도 가져오기
    if (AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
    {
        if (GameMode->IsStrokeMode())
            GameMode->StrokeWidgetInstance->ShowPuttingGuidancePanel(false);

        if (GameMode->PlayerManager && GameMode->PlayerManager->GetPlayerController())
        {
            AGolfPlayerController* PlayerController = GameMode->PlayerManager->GetPlayerController();
            DefaultAngle = PlayerController->ShotPitchAngle;
            YawAngle = PlayerController->ShotYawAngle;
        }
    }
    UE_LOG(LogTemp, Log, TEXT("🏌️ Applying shot: %.1fm/s @ %.1f° - yaw %.1f  - direction[%s]"), SpeedMS, DefaultAngle, YawAngle, *Direction.ToString());
    // 새로운 ApplyShotMS 함수 호출
    ApplyShotMS(Direction, SpeedMS, DefaultAngle, YawAngle);
}

void AGolfBall::ApplyShotMS(const FVector& Direction, float SpeedMS, float LaunchAngleDegrees, float YawDegrees)
{
    if (CurrentBallState != EBallState::Ball_Ready)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot apply shot: Ball not ready"));
        return;
    }

    if (!BallMesh || !BallMesh->IsValidLowLevel())
    {
        UE_LOG(LogTemp, Error, TEXT("BallMesh is invalid!"));
        return;
    }

    float OriginalSpeed = SpeedMS;
    float OriginalPitch = LaunchAngleDegrees;
    float OriginalYaw = YawDegrees;

    UE_LOG(LogTemp, Log, TEXT("📍 ApplyShotMS called (before adjustments):"));
    UE_LOG(LogTemp, Log, TEXT("   Speed: %.1f m/s, Pitch: %.1f°, Yaw: %.1f°"),
        SpeedMS, LaunchAngleDegrees, YawDegrees);

    // ⭐ JSON 로드
    LoadShotAdjustmentsFromJSON();

    // ⭐⭐⭐ 핵심: 샷 타입 자동 판단
    EShotType ShotType = DetermineShotType();

    // ⭐⭐⭐ 샷 타입에 따라 조정값 적용
    ApplyShotAdjustments(SpeedMS, LaunchAngleDegrees, YawDegrees, ShotType);

    UE_LOG(LogTemp, Log, TEXT("📍 ApplyShotMS after adjustments:"));
    UE_LOG(LogTemp, Log, TEXT("   Speed: %.1f m/s, Pitch: %.1f°, Yaw: %.1f°"),
        SpeedMS, LaunchAngleDegrees, YawDegrees);

    // ⭐ 기존 코드 계속 (Line 2564부터)
    PreviousBallPosition = GetActorLocation();
    bHasValidOBCrossingPoint = false;
    LastOBCrossingPoint = FVector::ZeroVector;

    ApplyTerrainPhysicsSettings(TEXT("Rough"));

    StartTrajectoryTracking();
    ReloadPhysicsConfig();
    AdjustBallToGroundLevel();

    LastShotStartLocation = GetActorLocation();
    LastShotPower = SpeedMS;

    bJustLaunched = true;
    LaunchTime = GetWorld()->GetTimeSeconds();
    LaunchPosition = GetActorLocation();

    SetPhysicsState(EPhysicsState::Simulating);
    SetBallState(EBallState::Ball_Fly);
    LaunchTimeSeconds = GetWorld()->GetTimeSeconds();

    FVector AdjustedDirection = CalculateShotDirectionWithElevation(
        Direction,
        LaunchAngleDegrees + 0.1f, // 기존 2.0f
        YawDegrees
    );

    FVector ShotVelocity = CalculateShotVelocity(AdjustedDirection, SpeedMS);

    LastShotDirection = AdjustedDirection;

    if (bShowShotArrow)
    {
        // DrawShotDirectionArrow(LastShotStartLocation, AdjustedDirection, SpeedMS, LaunchAngleDegrees);
    }

    LastValidVelocity = ShotVelocity;
    PendingShotVelocity = ShotVelocity;
    PendingShotSpeed = SpeedMS;
    bHasPendingShot = true;

    BallMesh->SetPhysicsLinearVelocity(ShotVelocity);

    GetWorld()->GetTimerManager().SetTimer(
        StateTransitionTimer,
        [this, ShotVelocity, SpeedMS]() {
            ApplyShotVelocityDelayed(ShotVelocity, SpeedMS);
        },
        0.05f,
        false
    );

    GetWorld()->GetTimerManager().SetTimer(
        LaunchGraceTimer,
        [this]() {
            bJustLaunched = false;
            UE_LOG(LogTemp, Log, TEXT("Launch grace period ended"));
        },
        LaunchGracePeriod,
        false
    );

    if (TrailSettings.bShowTrail)
    {
        ClearTrail();
        UE_LOG(LogTemp, Log, TEXT("Ball trail started"));
    }

    if (ShouldShowCrosshair())
    {
        ShowCrosshair();
    }
    else
    {
        HideCrosshair();
    }

    LastBounceImpulseSquared = 0.0f;
    UE_LOG(LogTemp, Log, TEXT("🎯 New Shot - Impulse history reset"));

    UE_LOG(LogTemp, Log, TEXT("✅ Shot setup complete with shot-type adjustments"));
}

// ⭐ 새로 추가: 강제 물리 활성화 및 속도 적용 (디버깅/응급용)
void AGolfBall::ForceApplyShot(const FVector& Direction, float SpeedMS)
{
    UE_LOG(LogTemp, Warning, TEXT("🚨 FORCE APPLYING SHOT: %.1f m/s"), SpeedMS);

    if (!BallMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Cannot force apply shot - BallMesh is null"));
        return;
    }

    // 강제 물리 활성화
    BallMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    BallMesh->SetSimulatePhysics(true);
    BallMesh->SetEnableGravity(true);
    BallMesh->WakeRigidBody();

    // 상태 설정
    SetBallState(EBallState::Ball_Fly);
    CurrentPhysicsState = EPhysicsState::Simulating;
    if (AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
    {


        AGolfPlayerController* PlayerController = GameMode->PlayerManager->GetPlayerController();

        // 속도 계산 및 적용
        FVector AdjustedDirection = CalculateShotDirectionWithElevation(Direction, PlayerController->ShotPitchAngle, PlayerController->ShotYawAngle);
        FVector ShotVelocity = CalculateShotVelocity(AdjustedDirection, SpeedMS);

        // 즉시 적용 (지연 없음)
        BallMesh->SetPhysicsLinearVelocity(ShotVelocity);
        LastValidVelocity = ShotVelocity;
        bHasPendingShot = false;


        // 적용 후 확인
        FVector AppliedVelocity = BallMesh->GetPhysicsLinearVelocity();

        UE_LOG(LogTemp, Warning, TEXT("🚨 Force shot result: Expected=%.1f, Applied=%.1f"),
            ShotVelocity.Size(), AppliedVelocity.Size());

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red,
                FString::Printf(TEXT("🚨 FORCE SHOT: %.1f cm/s"), AppliedVelocity.Size()));
        }
    }
}

void AGolfBall::ApplyShotVelocityDelayed(const FVector& ShotVelocity, float SpeedMS)
{
    if (!BallMesh || !IsValid(BallMesh))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ BallMesh invalid during delayed velocity application"));
        return;
    }

    // 물리 상태 재확인 및 강제 활성화
    if (!BallMesh->IsSimulatingPhysics())
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Physics not active, forcing activation"));
        BallMesh->SetSimulatePhysics(true);
        BallMesh->SetEnableGravity(true);
        BallMesh->WakeRigidBody();
    }

    // 속도 적용
    BallMesh->SetPhysicsLinearVelocity(ShotVelocity);
    //ApplyBackspin(SpeedMS); // 일단 적용안함

    // ⭐ 추가: 탑스핀 적용 (X축 회전으로 forward spin - 구르기 증가)
    //FVector SpinAxis = FVector::CrossProduct(ShotVelocity.GetSafeNormal(), FVector::UpVector);  // 회전 축 계산 (지면 수직)
    //FVector AngularVelocity = SpinAxis * PhysicsConfig.ForwardSpinFactor;  // 강도 적용
    //BallMesh->SetPhysicsAngularVelocityInDegrees(AngularVelocity);

    UE_LOG(LogTemp, Log, TEXT("🚀 Shot applied: Velocity=%.2f m/s, Spin=%.2f deg/s"), SpeedMS, PhysicsConfig.ForwardSpinFactor);

    // 상태 업데이트
    LastValidVelocity = ShotVelocity;
    bHasPendingShot = false;

    // 적용 후 검증
    FVector AppliedVelocity = BallMesh->GetPhysicsLinearVelocity();
    float AppliedSpeed = AppliedVelocity.Size();

    UE_LOG(LogTemp, Log, TEXT("✅ Shot velocity applied: Expected=%.1f cm/s, Actual=%.1f cm/s"),
        ShotVelocity.Size(), AppliedSpeed);

    if (AppliedSpeed < ShotVelocity.Size() * 0.5f)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Velocity application may have failed!"));

        // 재시도
        GetWorld()->GetTimerManager().SetTimerForNextTick([this, ShotVelocity]() {
            if (BallMesh && IsValid(BallMesh))
            {
                BallMesh->SetPhysicsLinearVelocity(ShotVelocity);
                UE_LOG(LogTemp, Warning, TEXT("🔄 Velocity reapplied"));
            }
            });
    }
}

void AGolfBall::ApplyBackspin(float SpeedMS)
{
    float SpeedRatio = FMath::Clamp(SpeedMS / ParkGolfConstants.MAX_SPEED, 0.0f, 1.0f);
    float BackspinStrength = 1.0f + (SpeedRatio * 3.0f); // 3.0f → 1.0f로 강도 감소
    FVector BackspinVector = FVector(-BackspinStrength, 0.0f, 0.0f);
    FVector BackspinRadians = BackspinVector * (PI / 180.0f);

    if (BallMesh && BallMesh->IsValidLowLevel())
    {
        BallMesh->SetPhysicsAngularVelocityInDegrees(BackspinRadians, false);
    }

    UE_LOG(LogTemp, VeryVerbose, TEXT("UE4: Park Golf backspin: %.1f deg/s (Speed: %.1fm/s)"),
        BackspinStrength, SpeedMS);
}

// ===== 궤적 추적 시스템 =====
void AGolfBall::StartTrajectoryTracking()
{
    UE_LOG(LogTemp, Log, TEXT("🛤️ 궤적 추적 시작"));

    bIsTrackingTrajectory = false;
    TrajectoryStartTime = GetWorld()->GetTimeSeconds();
    LastTrajectoryPointTime = TrajectoryStartTime;
    TrajectoryPoints.Empty();
    AddTrajectoryPoint();
}

void AGolfBall::StopTrajectoryTracking()
{
    UE_LOG(LogTemp, Log, TEXT("🛤️ 궤적 추적 종료"));

    bIsTrackingTrajectory = false;
    AddTrajectoryPoint();

    float TotalDistance = GetTotalTrajectoryDistance();
    float MaxHeight = GetMaxTrajectoryHeight();
    float TotalTime = GetWorld()->GetTimeSeconds() - TrajectoryStartTime;

    UE_LOG(LogTemp, Log, TEXT("📊 궤적 완료: 거리=%.1fm, 최고높이=%.1fm, 시간=%.1fs, 포인트=%d개"),
        TotalDistance / 100.0f, MaxHeight / 100.0f, TotalTime, TrajectoryPoints.Num());

}

void AGolfBall::ClearTrajectory()
{
    TrajectoryPoints.Empty();
    bIsTrackingTrajectory = false;
    UE_LOG(LogTemp, Log, TEXT("🗑️ 궤적 데이터 초기화"));
}


void AGolfBall::UpdateTrajectoryTracking(float DeltaTime)
{
    if (!bIsTrackingTrajectory) return;

    float CurrentTime = GetWorld()->GetTimeSeconds();

    if (CurrentTime - LastTrajectoryPointTime >= TrajectorySettings.PointInterval)
    {
        AddTrajectoryPoint();
        LastTrajectoryPointTime = CurrentTime;
    }

    CleanupOldTrajectoryPoints();

    if (TrajectorySettings.bShowTrajectory)
    {
        DrawTrajectory();
    }
}

void AGolfBall::AddTrajectoryPoint()
{
    if (TrajectoryPoints.Num() >= TrajectorySettings.MaxTrajectoryPoints)
    {
        TrajectoryPoints.RemoveAt(0);
    }

    FTrajectoryPoint NewPoint;
    NewPoint.Position = GetActorLocation();
    NewPoint.Speed = GetBallSpeed();
    NewPoint.TimeStamp = GetWorld()->GetTimeSeconds();
    NewPoint.BallState = CurrentBallState;

    TrajectoryPoints.Add(NewPoint);

    UE_LOG(LogTemp, VeryVerbose, TEXT("📍 궤적 포인트 추가: 위치=(%s), 속도=%.1fm/s, 상태=%s"),
        *NewPoint.Position.ToString(), NewPoint.Speed / 100.0f,
        *UEnum::GetValueAsString(NewPoint.BallState));
}

void AGolfBall::CleanupOldTrajectoryPoints()
{
    if (TrajectorySettings.TrajectoryDuration <= 0) return;

    float CurrentTime = GetWorld()->GetTimeSeconds();
    float CutoffTime = CurrentTime - TrajectorySettings.TrajectoryDuration;

    int32 RemoveCount = 0;
    for (int32 i = 0; i < TrajectoryPoints.Num(); i++)
    {
        if (TrajectoryPoints[i].TimeStamp < CutoffTime)
        {
            RemoveCount++;
        }
        else
        {
            break;
        }
    }

    if (RemoveCount > 0)
    {
        TrajectoryPoints.RemoveAt(0, RemoveCount);
        UE_LOG(LogTemp, VeryVerbose, TEXT("🧹 오래된 궤적 포인트 %d개 제거"), RemoveCount);
    }
}

void AGolfBall::DrawTrajectory() const
{
    if (!GetWorld() || TrajectoryPoints.Num() < 2) return;

    for (int32 i = 0; i < TrajectoryPoints.Num() - 1; i++)
    {
        const FTrajectoryPoint& CurrentPoint = TrajectoryPoints[i];
        const FTrajectoryPoint& NextPoint = TrajectoryPoints[i + 1];

        FColor LineColor = FColor::White;

        if (TrajectorySettings.bShowSpeedColors)
        {
            LineColor = GetSpeedBasedColor(CurrentPoint.Speed);
        }
        else
        {
            LineColor = GetStateBasedColor(CurrentPoint.BallState);
        }

        //DrawDebugLine(GetWorld(), CurrentPoint.Position, NextPoint.Position,
        //    LineColor, false, -1.0f, 0, TrajectorySettings.LineThickness);

        if (TrajectorySettings.bShowStateMarkers &&
            CurrentPoint.BallState != NextPoint.BallState)
        {
            FString StateChangeText = FString::Printf(TEXT("%s→%s"),
                *UEnum::GetValueAsString(CurrentPoint.BallState).Right(4),
                *UEnum::GetValueAsString(NextPoint.BallState).Right(4));

        }
    }

    if (TrajectoryPoints.Num() > 0)
    {
        const FTrajectoryPoint& LastPoint = TrajectoryPoints.Last();
        FVector CurrentPosition = GetActorLocation();

        if (!LastPoint.Position.Equals(CurrentPosition, 5.0f))
        {
            FColor RealTimeColor = GetSpeedBasedColor(GetBallSpeed());
            DrawDebugLine(GetWorld(), LastPoint.Position, CurrentPosition,
                RealTimeColor, false, -1.0f, 0, TrajectorySettings.LineThickness + 1.0f);
        }
    }
}

FColor AGolfBall::GetSpeedBasedColor(float Speed) const
{
    float SpeedMS = Speed / 100.0f;

    if (SpeedMS < 2.0f)
        return FColor::Blue;
    else if (SpeedMS < 5.0f)
        return FColor::Green;
    else if (SpeedMS < 10.0f)
        return FColor::Yellow;
    else if (SpeedMS < 15.0f)
        return FColor::Orange;
    else if (SpeedMS < 20.0f)
        return FColor::Red;
    else
        return FColor::Magenta;
}

FColor AGolfBall::GetStateBasedColor(EBallState State) const
{
    switch (State)
    {
    case EBallState::Ball_Ready:
        return FColor::Green;
    case EBallState::Ball_Fly:
        return FColor::Red;
    case EBallState::Ball_Bound:
        return FColor::Yellow;
    case EBallState::Ball_Stop:
        return FColor::Blue;
    default:
        return FColor::White;
    }
}

float AGolfBall::GetTotalTrajectoryDistance() const
{
    if (TrajectoryPoints.Num() < 2) return 0.0f;

    float TotalDistance = 0.0f;

    for (int32 i = 0; i < TrajectoryPoints.Num() - 1; i++)
    {
        float SegmentDistance = FVector::Dist(
            TrajectoryPoints[i].Position,
            TrajectoryPoints[i + 1].Position);
        TotalDistance += SegmentDistance;
    }

    return TotalDistance;
}

float AGolfBall::GetMaxTrajectoryHeight() const
{
    if (TrajectoryPoints.Num() == 0) return 0.0f;

    float MaxHeight = TrajectoryPoints[0].Position.Z;

    for (const FTrajectoryPoint& Point : TrajectoryPoints)
    {
        if (Point.Position.Z > MaxHeight)
        {
            MaxHeight = Point.Position.Z;
        }
    }

    return MaxHeight;
}

void AGolfBall::SetTrajectorySettings(const FTrajectorySettings& NewSettings)
{
    TrajectorySettings = NewSettings;
    UE_LOG(LogTemp, Log, TEXT("⚙️ 궤적 설정 업데이트: 표시=%s, 간격=%.2fs, 지속시간=%.1fs"),
        TrajectorySettings.bShowTrajectory ? TEXT("ON") : TEXT("OFF"),
        TrajectorySettings.PointInterval,
        TrajectorySettings.TrajectoryDuration);
}

// ===== 거리 추적 시스템 =====
void AGolfBall::TrackShotDistance()
{
    // ✅ 최적화: static 로컬 변수 제거 → 헤더의 멤버 변수 사용
    // (static 로컬은 모든 볼 인스턴스가 공유되어 멀티볼 시 버그 발생)

    if (CurrentBallState == EBallState::Ball_Fly && !bTrackingShot)
    {
        ShotStartLocation = GetActorLocation();
        ShotSpeed_Track = LastValidVelocity.Size() / 100.0f;
        ShotAngle_Track = FMath::RadiansToDegrees(FMath::Asin(LastValidVelocity.Z / LastValidVelocity.Size()));
        bTrackingShot = true;

        UE_LOG(LogTemp, Log, TEXT("📍 파크골프 샷 추적 시작: %.1fm/s @ %.1f°"), ShotSpeed_Track, ShotAngle_Track);
    }

    if (CurrentBallState == EBallState::Ball_Stop && bTrackingShot)
    {
        FVector ShotEndLocation = GetActorLocation();
        float ActualDistance = FVector::Dist2D(ShotStartLocation, ShotEndLocation) / 100.0f;

        float ExpectedDistance = CalculateExpectedDistance(ShotSpeed_Track, ShotAngle_Track);
        float Accuracy = (ActualDistance / ExpectedDistance) * 100.0f;

        UE_LOG(LogTemp, Log, TEXT("🎯 파크골프 완료: %.1fm (예상: %.1fm, 정확도: %.1f%%)"),
            ActualDistance, ExpectedDistance, Accuracy);

        FString DistanceEvaluation = EvaluateParkGolfDistance(ActualDistance);
        FString AccuracyEvaluation = EvaluateAccuracy(Accuracy);

        bTrackingShot = false;
    }
}

FString AGolfBall::EvaluateParkGolfDistance(float Distance)
{
    if (Distance < 5.0f)
        return TEXT("퍼팅 거리");
    else if (Distance < 15.0f)
        return TEXT("쇼트 퍼팅");
    else if (Distance < 35.0f)
        return TEXT("숏샷");
    else if (Distance < 60.0f)
        return TEXT("미들샷");
    else if (Distance < 90.0f)
        return TEXT("롱샷");
    else if (Distance < 120.0f)
        return TEXT("파워샷");
    else
        return TEXT("최대 거리!");
}

FString AGolfBall::EvaluateAccuracy(float AccuracyPercent)
{
    if (AccuracyPercent >= 95.0f)
        return TEXT("완벽!");
    else if (AccuracyPercent >= 85.0f)
        return TEXT("정확!");
    else if (AccuracyPercent >= 75.0f)
        return TEXT("양호");
    else if (AccuracyPercent >= 60.0f)
        return TEXT("보통");
    else
        return TEXT("연습 필요");
}

// ===== OB 체크 시스템 =====
bool AGolfBall::CheckOutOfBounds()
{
    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    if (!GameMode)
    {
        UE_LOG(LogTemp, Warning, TEXT("CheckOutOfBounds: GameMode is null"));
        return false;
    }

    if (GameMode->CurrentGameMode != EGolfGameMode::StrokeMode)
        return false;

    int32 CurrentHoleIndex = GameMode->CurrentHole - 1;
    if (!GameMode->MapInfo.OBLines.IsValidIndex(CurrentHoleIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("CheckOutOfBounds: No OB lines for hole %d"), GameMode->CurrentHole);
        return false;
    }

    const TArray<FVector>& OBPoints = GameMode->MapInfo.OBLines[CurrentHoleIndex].Points;

    if (OBPoints.Num() < 3)
    {
        UE_LOG(LogTemp, Warning, TEXT("CheckOutOfBounds: Insufficient OB points (%d) for hole %d"),
            OBPoints.Num(), GameMode->CurrentHole);
        return false;
    }

    FVector BallLocation = GetActorLocation();
    FVector2D BallPos2D(BallLocation.X, BallLocation.Y);
    bool bIsOutside = IsPointOutsidePolygonImproved(BallPos2D, OBPoints);

    // ⭐⭐⭐ 핵심 수정: CheckRealtimeOBCrossing()이 이미 교차점을 저장했다면 덮어쓰지 않음!
    if (bIsOutside && !bHasValidOBCrossingPoint)
    {
        // 실시간 체크에서 놓친 경우에만 폴백 처리 수행
        UE_LOG(LogTemp, Warning, TEXT("🚫 OB detected (fallback - realtime check missed): %s"),
            *BallLocation.ToString());

        LastOBCrossingPoint = BallLocation;
        bHasValidOBCrossingPoint = true;
    }

    return bIsOutside;
}

bool AGolfBall::IsPointOutsidePolygonImproved(const FVector2D& Point, const TArray<FVector>& PolygonPoints) const
{
    int32 NumPoints = PolygonPoints.Num();
    if (NumPoints < 3)
    {
        UE_LOG(LogTemp, Warning, TEXT("유효하지 않은 폴리곤 (점 개수: %d)"), NumPoints);
        return true;
    }

    int32 Crossings = 0;

    float SignedArea = CalculatePolygonSignedArea(PolygonPoints);
    bool IsClockwise = SignedArea < 0;

    UE_LOG(LogTemp, VeryVerbose, TEXT("폴리곤 방향: %s (SignedArea: %.2f)"),
        IsClockwise ? TEXT("시계방향") : TEXT("반시계방향"), SignedArea);

    for (int32 i = 0; i < NumPoints; i++)
    {
        int32 j = (i + 1) % NumPoints;

        FVector2D P1(PolygonPoints[i].X, PolygonPoints[i].Y);
        FVector2D P2(PolygonPoints[j].X, PolygonPoints[j].Y);

        if (IsRayIntersectingSegment(Point, P1, P2))
        {
            Crossings++;
            UE_LOG(LogTemp, VeryVerbose, TEXT("교차 %d: 세그먼트 %d->%d"), Crossings, i, j);
        }
    }

    bool bIsInside = (Crossings % 2) == 1;
    bool bIsOutside = !bIsInside;

    UE_LOG(LogTemp, VeryVerbose, TEXT("교차 횟수: %d, 내부: %s, 외부: %s"),
        Crossings, bIsInside ? TEXT("True") : TEXT("False"), bIsOutside ? TEXT("True") : TEXT("False"));

    return bIsOutside;
}

bool AGolfBall::IsRayIntersectingSegment(const FVector2D& Point, const FVector2D& P1, const FVector2D& P2) const
{
    float MinY = FMath::Min(P1.Y, P2.Y);
    float MaxY = FMath::Max(P1.Y, P2.Y);

    if (Point.Y < MinY || Point.Y >= MaxY)
    {
        return false;
    }

    if (FMath::Abs(P2.Y - P1.Y) < KINDA_SMALL_NUMBER)
    {
        return false;
    }

    float IntersectionX = P1.X + (P2.X - P1.X) * (Point.Y - P1.Y) / (P2.Y - P1.Y);

    return Point.X < IntersectionX;
}

float AGolfBall::CalculatePolygonSignedArea(const TArray<FVector>& PolygonPoints) const
{
    if (PolygonPoints.Num() < 3) return 0.0f;

    float SignedArea = 0.0f;
    int32 NumPoints = PolygonPoints.Num();

    for (int32 i = 0; i < NumPoints; i++)
    {
        int32 j = (i + 1) % NumPoints;
        SignedArea += (PolygonPoints[j].X - PolygonPoints[i].X) * (PolygonPoints[j].Y + PolygonPoints[i].Y);
    }

    return SignedArea * 0.5f;
}


// ===== 유틸리티 함수들 =====
void AGolfBall::UpdatePhysicsParameters()
{
    if (!BallMesh) return;

    float LinearDamping = PhysicsConfig.BaseLinearDamping * (1.0f + FrictionWeight * 2.0f);
    float AngularDamping = PhysicsConfig.BaseAngularDamping * (1.0f + FrictionWeight * 2.0f);

    // ⭐ Bound/Rolling 중엔 지형별 세팅이 항상 우선이어야 하므로 State 플로어 적용 제외
    if (StatePhysicsMap.Contains(CurrentBallState) &&
        CurrentBallState != EBallState::Ball_Bound &&
        CurrentBallState != EBallState::Ball_Rolling)
    {
        FStatePhysicsSettings StateSettings = StatePhysicsMap[CurrentBallState];
        LinearDamping = FMath::Max(LinearDamping, StateSettings.LinearDamping);
        AngularDamping = FMath::Max(AngularDamping, StateSettings.AngularDamping);
    }


    BallMesh->SetLinearDamping(LinearDamping);
    BallMesh->SetAngularDamping(AngularDamping);

    UE_LOG(LogTemp, Log, TEXT("Physics parameters updated: Linear=%.3f, Angular=%.3f"),
        LinearDamping, AngularDamping);
}

void AGolfBall::SetFrictionWeight(float NewWeight)
{
    FrictionWeight = FMath::Clamp(NewWeight, 0.0f, 2.0f);
    UpdatePhysicsParameters();
    UE_LOG(LogTemp, Log, TEXT("SetFrictionWeight: NewWeight=%.2f"), FrictionWeight);
}

void AGolfBall::SetTerrainFriction(float NewFriction)
{
    UE_LOG(LogTemp, VeryVerbose, TEXT("🔄 SetTerrainFriction"));
    PhysicsConfig.RollingFriction = FMath::Clamp(NewFriction, 0.0f, 1.0f);

    // ✅ 볼 PhysMat 교체 하지 않음 — Damping으로만 반영
    if (BallMesh && BallMesh->IsSimulatingPhysics())
    {
        float Damping = FMath::Lerp(0.05f, 0.5f, PhysicsConfig.RollingFriction);
        BallMesh->SetLinearDamping(Damping);
    }

    UE_LOG(LogTemp, Log, TEXT("SetTerrainFriction: %.2f"), PhysicsConfig.RollingFriction);
}
FVector AGolfBall::GetBallVelocity() const
{
    if (BallMesh && BallMesh->IsValidLowLevel())
    {
        if (BallMesh->IsSimulatingPhysics())
        {
            return BallMesh->GetPhysicsLinearVelocity();
        }
        else if (bHasPendingShot)
        {
            // ⭐ 새로 추가: 물리 활성화 전에는 예상 속도 반환
            UE_LOG(LogTemp, VeryVerbose, TEXT("🔄 Returning pending shot velocity"));
            return PendingShotVelocity;
        }
    }

    // 마지막 유효한 속도 반환 (완전 정지가 아닌 경우)
    if (!LastValidVelocity.IsNearlyZero())
    {
        return LastValidVelocity;
    }

    return FVector::ZeroVector;
}
float AGolfBall::GetBallSpeed() const
{
    return GetBallVelocity().Size();
}

bool AGolfBall::IsNearGround(float Distance) const
{
    float ActualBallRadius = GetActualBallRadius();

    // ✅ 최적화: 같은 프레임에서 이미 수행한 지면 LineTrace 재사용
    // const 함수이므로 캐시는 별도 헬퍼로 처리 (여기선 직접 수행)
    FVector Start = GetActorLocation();
    FVector End = Start - FVector(0, 0, ActualBallRadius + Distance);

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    QueryParams.bTraceComplex = true;

    bool bHitGround = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End,
        ECC_WorldStatic, QueryParams);

    if (bHitGround)
    {
        float DistanceToGround = FVector::Dist(Start, HitResult.Location) - ActualBallRadius;
        return DistanceToGround <= Distance;
    }

    return false;
}

bool AGolfBall::IsGroundCollision(AActor* OtherActor, UPrimitiveComponent* OtherComp) const
{
    if (!OtherActor || !OtherComp) return false;

    FString ActorName = OtherActor->GetName();
    FName ProfileName = OtherComp->GetCollisionProfileName();

    bool bIsGround = false;

    if (ProfileName == TEXT("WorldStatic") ||
        ProfileName == TEXT("BlockAll"))
    {
        bIsGround = true;
    }

    if (ActorName.Contains(TEXT("Ground")) ||
        ActorName.Contains(TEXT("green_hole")) ||
        ActorName.Contains(TEXT("holecup")) ||
        ActorName.Contains(TEXT("Floor")) ||
        ActorName.Contains(TEXT("landphysic")) ||
        ActorName.Contains(TEXT("Landscape")) ||
        ActorName.Contains(TEXT("main")) ||
        ActorName.Contains(TEXT("green")) ||
        ActorName.Contains(TEXT("Terrain")))
    {
        bIsGround = true;
    }

    if (OtherActor->IsA(AStaticMeshActor::StaticClass()))
    {
        bIsGround = true;
    }

    return bIsGround;
}

FVector AGolfBall::GetTerrainNormal() const
{

    if (CurrentBallState == EBallState::Ball_Rolling)
    {
        FVector CurrentScale = BallMesh->GetComponentScale();
        float ActualBallRadius = 50.0f * CurrentScale.Z;

        FVector Start = GetActorLocation() - FVector(0, 0, ActualBallRadius);
        FVector End = Start - FVector(0, 0, 10.0f);

        FHitResult HitResult;
        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(this);
        QueryParams.bTraceComplex = true;

        if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End,
            ECC_WorldStatic, QueryParams))
        {
            return HitResult.Normal;
        }

        return FVector::UpVector;
    }
    else
    {
        return GetStabilizedTerrainNormal();
    }


}

void AGolfBall::ApplyAirResistance(float DeltaTime)
{
    FVector CurrentVelocity = BallMesh->GetPhysicsLinearVelocity();
    float Speed = CurrentVelocity.Size();

    if (Speed > VELOCITY_EPSILON)
    {
        float ResistanceMagnitude = PhysicsConfig.AirResistance * Speed * Speed * DeltaTime;
        FVector ResistanceForce = -CurrentVelocity.GetSafeNormal() * ResistanceMagnitude;
        BallMesh->AddForce(ResistanceForce);
    }
}

void AGolfBall::ApplyRollingFriction(float DeltaTime)
{
    FVector CurrentVelocity = BallMesh->GetPhysicsLinearVelocity();
    float Speed = CurrentVelocity.Size();

    if (Speed > VELOCITY_EPSILON)
    {
        float FrictionMagnitude = PhysicsConfig.RollingFriction * FrictionWeight * GRAVITY_MAGNITUDE * DeltaTime;
        FVector FrictionForce = -CurrentVelocity.GetSafeNormal() * FrictionMagnitude;
        BallMesh->AddForce(FrictionForce);
    }
}

void AGolfBall::ApplySlopeEffect(float DeltaTime)
{
    if (!BallMesh) return;

    FVector TerrainNormal = GetTerrainNormal();
    if (TerrainNormal.IsNearlyZero()) return;

    FVector CurrentVelocity = BallMesh->GetPhysicsLinearVelocity();
    float Speed = CurrentVelocity.Size();

    // 🔧 바운스 친화적 수정: 경사면 효과를 속도와 상태에 따라 조정
    float BaseSlopeFactor = FMath::Clamp(Speed / 500.0f, 0.05f, 0.25f);

    // 🔧 바운스 상태에서는 경사면 효과 감소 (바운스 방해 않도록)
    if (CurrentBallState == EBallState::Ball_Bound)
    {
        BaseSlopeFactor *= 0.5f;  // 바운스 중에는 경사면 효과 절반
    }

    FVector SlopeForce = (FVector(0, 0, -1) - TerrainNormal) * GRAVITY_MAGNITUDE * BaseSlopeFactor * DeltaTime;

    // 🔧 추가: 경사가 심한 곳에서는 약간의 불규칙성 추가 (더 자연스러운 움직임)
    float SlopeAngle = FMath::Acos(FVector::DotProduct(TerrainNormal, FVector::UpVector));
    if (SlopeAngle > FMath::DegreesToRadians(15.0f))  // 15도 이상 경사
    {
        FVector RandomForce = FVector(
            FMath::RandRange(-2.0f, 2.0f),
            FMath::RandRange(-2.0f, 2.0f),
            0.0f
        );
        SlopeForce += RandomForce;
    }

    BallMesh->AddForce(SlopeForce);

    UE_LOG(LogTemp, VeryVerbose, TEXT("🏔️ Slope effect: Angle=%.1f°, Force=%.1f, Speed=%.1f"),
        FMath::RadiansToDegrees(SlopeAngle), SlopeForce.Size(), Speed);
}

void AGolfBall::HandleGroundBounce(const FHitResult& Hit)
{
    if (!BallMesh) return;

    FVector CurrentVelocity = BallMesh->GetPhysicsLinearVelocity();
    float IncomingSpeed = CurrentVelocity.Size();

    // 🔧 핵심: 매우 낮은 속도에서는 바운스 하지 않음
    if (IncomingSpeed < 500.0f) // 0.5m/s 이하
    {
        // 바운스 대신 굴림으로 전환
        FVector HorizontalVelocity = FVector(CurrentVelocity.X, CurrentVelocity.Y, 0.0f);
        BallMesh->SetPhysicsLinearVelocity(HorizontalVelocity * 0.9f);
        //  SetBallState(EBallState::Ball_Rolling);
        return;
    }

    // 🔧 개선: 더 현실적인 바운스 계산
    FVector ReflectedVelocity = CurrentVelocity - 2.0f * FVector::DotProduct(CurrentVelocity, Hit.Normal) * Hit.Normal;

    // 🔧 핵심: 매우 강한 댐핑 적용 (랜드스케이프+메쉬 겹침 문제 해결)
    float BounceDamping = PhysicsConfig.Restitution * 0.5f; // 기존 댐핑의 50%로 더 강하게
    ReflectedVelocity *= BounceDamping;

    // 🔧 추가: 수직 성분은 더 강하게 감쇠
    ReflectedVelocity.Z *= 0.6f;

    // 🔧 최소 바운스 높이 제한
    float ZCutoff = (CurrentAppliedTerrain == TEXT("Rough")) ? 4.0f : 8.0f;
    if (FMath::Abs(ReflectedVelocity.Z) < ZCutoff)
    {
        ReflectedVelocity.Z = 0.0f;
    }

    //BallMesh->SetPhysicsLinearVelocity(ReflectedVelocity);

    UE_LOG(LogTemp, Log, TEXT("🏀 Damped bounce: Original=%.1f, Reflected=%.1f, Damping=%.2f"),
        IncomingSpeed, ReflectedVelocity.Size(), BounceDamping);
}


void AGolfBall::LimitGroundBounce(const FHitResult& Hit, const FVector& NormalImpulse)
{
    const float ImpulseSq = NormalImpulse.SizeSquared();

    // 1000 이상일 때만 감속
    if (ImpulseSq >= 1000.0f)
    {
        FVector CurrentVel = BallMesh->GetPhysicsLinearVelocity();
        float CurrentSpeed = CurrentVel.Size();

        // 0. 방향 유지: 정규화된 방향 벡터 추출
        FVector ForwardDir = CurrentVel.GetSafeNormal();  // 현재 이동 방향

        // ⭐ 이전 바운스 값과 비교하여 급격한 증가 감지
        float SpeedReductionFactor = 1.0f;  // 기본: 70% 유지 (30% 감소)

        // ⭐ 수정: 고속일수록 임계 배율 높게 (고속 바운스 보호)
        float CurrentSpeedMS2 = BallMesh->GetPhysicsLinearVelocity().Size() / 100.0f;
        float BounceJumpThreshold = (CurrentSpeedMS2 >= 10.0f) ? 20.0f : 10.0f;  // 5배 → 10~20배

        if (LastBounceImpulseSquared > 0.0f && ImpulseSq >= LastBounceImpulseSquared * BounceJumpThreshold)
        {
            // ⭐ 수정: 고속에서는 강제 굴림 전환 없이 impulse만 기록
            if (CurrentSpeedMS2 < 8.0f)
            {
                SetBallState(EBallState::Ball_Rolling);
                UE_LOG(LogTemp, Log, TEXT("ParkGolf: Bound -> Rolling (LimitGroundBounce 저속)"));
            }
            UE_LOG(LogTemp, Warning, TEXT("⚠️ 이상 바운스 감지 (%.1fm/s): %.2f → %.2f (%.1fx)"),
                CurrentSpeedMS2, LastBounceImpulseSquared, ImpulseSq, ImpulseSq / LastBounceImpulseSquared);
            LastBounceImpulseSquared = ImpulseSq;
        }

        // 1. 속도 감소 적용
        float ReducedSpeed = CurrentSpeed * SpeedReductionFactor;

        // 2. Z축은 추가로 감소 (튐 방지, 선택적)
        //    → 전진 방향은 유지하되, 수직 상승은 억제
        FVector LimitedVel = ForwardDir * ReducedSpeed;
        //   LimitedVel.Z *= 0.6f;  // Z는 60%만 유지 → 위로 튀는 힘 약화

           // 3. 최종 속도 적용
        BallMesh->SetPhysicsLinearVelocity(LimitedVel);

        UE_LOG(LogTemp, Warning, TEXT("LIMITED BOUNCE (%.0f%%, DIR PRESERVED)! "
            "Impulse: %.2f | PrevImpulse: %.2f | Speed: %.2f to %.2f | Dir: (%.2f, %.2f, %.2f)"),
            SpeedReductionFactor * 100.0f, ImpulseSq, LastBounceImpulseSquared,
            CurrentSpeed, LimitedVel.Size(), ForwardDir.X, ForwardDir.Y, ForwardDir.Z);

        // ⭐ 현재 바운스 값을 저장 (다음 비교를 위해)
        LastBounceImpulseSquared = ImpulseSq;
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("Bounce OK - Impulse: %.2f (under 1000)"), ImpulseSq);

        // 작은 바운스도 기록 (연속성 유지)
        LastBounceImpulseSquared = ImpulseSq;
    }
}


// 7. UE4 볼 상태 전환 시 수동 슬리핑 재시작
void AGolfBall::RestartManualSleepCheck()
{
    // 기존 타이머 클리어
    if (GetWorld()->GetTimerManager().IsTimerActive(ManualSleepCheckTimer))
    {
        GetWorld()->GetTimerManager().ClearTimer(ManualSleepCheckTimer);
    }

    // 변수 초기화
    LastSpeedCheckTime = GetWorld()->GetTimeSeconds();
    LastRecordedSpeed = GetBallSpeed();
    LowSpeedFrameCount = 0;

    // 새 타이머 시작 (볼이 움직일 때만)
    if (CurrentBallState == EBallState::Ball_Fly ||
        CurrentBallState == EBallState::Ball_Bound ||
        CurrentBallState == EBallState::Ball_Rolling)
    {
        GetWorld()->GetTimerManager().SetTimer(
            ManualSleepCheckTimer,
            this,
            &AGolfBall::CheckManualSleeping,
            0.1f,  // 0.1초마다
            true   // 반복
        );

        UE_LOG(LogTemp, Log, TEXT("🔄 UE4 Manual sleep check restarted"));
    }
}

// 4. UE4용 수동 슬리핑 시스템
void AGolfBall::CheckManualSleeping()
{
    // ⭐ 즉시 추가: 필수 체크들
    if (!BallMesh || !IsValid(BallMesh) || !BallMesh->IsSimulatingPhysics() || !GetWorld())
    {
        return;
    }

    float CurrentSpeed = GetBallSpeed();
    float CurrentTime = GetWorld()->GetTimeSeconds();

    // ⭐ 즉시 추가: 값 유효성 체크
    if (FMath::IsNaN(CurrentSpeed) || !FMath::IsFinite(CurrentSpeed) ||
        FMath::IsNaN(CurrentTime) || !FMath::IsFinite(CurrentTime))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ CheckManualSleeping: Invalid values detected"));
        return;
    }

    // 나머지 기존 코드 계속...
    const float SleepSpeedThreshold = 1.5f;
    const float SleepTimeThreshold = 1.0f;

    if (CurrentSpeed < SleepSpeedThreshold)
    {
        if (LastRecordedSpeed < SleepSpeedThreshold)
        {
            LowSpeedFrameCount++;

            if ((CurrentTime - LastSpeedCheckTime) > SleepTimeThreshold)
            {
                UE_LOG(LogTemp, Log, TEXT("🛌 Safe sleep triggered: Speed=%.2f"), CurrentSpeed);

                BallMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
                BallMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

                if (CurrentBallState != EBallState::Ball_Stop)
                {
                    SetBallState(EBallState::Ball_Stop);
                }

                // ⭐ 추가: 안전한 타이머 정리
                if (GetWorld()->GetTimerManager().IsTimerActive(ManualSleepCheckTimer))
                {
                    GetWorld()->GetTimerManager().ClearTimer(ManualSleepCheckTimer);
                }
                return;
            }
        }
        else
        {
            LastSpeedCheckTime = CurrentTime;
            LowSpeedFrameCount = 0;
        }
    }
    else
    {
        LowSpeedFrameCount = 0;
        LastSpeedCheckTime = CurrentTime;
    }

    LastRecordedSpeed = CurrentSpeed;
}


// ===== 이벤트 핸들러 =====
void AGolfBall::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, FVector NormalImpulse,
    const FHitResult& Hit)
{

    //// 기존 안전성 체크들...
    if (bIsBeingDestroyed || !IsValid(this) || bIsInCollisionCallback)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ OnHit: Object being destroyed or already in callback, skipping"));
        return;
    }

    if (!OtherActor || !OtherComp || !BallMesh || !IsValid(BallMesh) || !IsValid(this))
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ OnHit: Invalid objects detected, skipping"));
        return;
    }

    bIsInCollisionCallback = true;


    try
    {
        // ⭐ 새로 추가: 지면 충돌 시 법선벡터 시각화
        if (IsGroundCollision(OtherActor, OtherComp))
        {
            const FVector HitNormal = Hit.Normal.GetSafeNormal();

            GEngine->AddOnScreenDebugMessage(
                -1,
                5.0f,
                FColor::Red,
                FString::Printf(TEXT("speed :%.2f"), BallMesh->GetPhysicsLinearVelocity().Size())
            );

            const float PreImpactSpeed = LastLinearVelocity.Size();

            if (PreImpactSpeed >= PhysicsConfig.MinPreImpactSpeed)
            {
                FVector CurrentVel = BallMesh->GetPhysicsLinearVelocity();
                const float PostImpactUpSpeed = CurrentVel.Z;
                const float PostImpactForwardSpeed = CurrentVel.X;
                const float PostImpactRightSpeed = CurrentVel.Y;

                LastLinearVelocity.Z = LastLinearVelocity.Z > 0 ? LastLinearVelocity.Z : LastLinearVelocity.Z * -1;

                const float MaxAllowedUpSpeed = LastLinearVelocity.Z * (PhysicsConfig.MaxBounceSpeedRatio);

                if (bBounceFix)
                {
                    if (PostImpactUpSpeed > PhysicsConfig.MinBounceFixHeight && PostImpactUpSpeed > MaxAllowedUpSpeed)
                    {
                        if (GEngine)
                        {
                            GEngine->AddOnScreenDebugMessage(
                                -1,
                                5.0f,
                                FColor::Yellow,
                                FString::Printf(TEXT("BallBounceFixed!"))
                            );
                        }

                    }
                }
            }

            // 기존 바운스 처리 코드...
            if (CurrentBallState == EBallState::Ball_Fly)
            {


            }
            else if (CurrentBallState == EBallState::Ball_Bound || CurrentBallState == EBallState::Ball_Rolling)
            {

                //    LimitGroundBounce(Hit, NormalImpulse);  // bounce 처리

            //    PendingBounceHit = Hit;
            //    PendingBounceHit.PhysMaterial = PhysMatResolveUtil::ResolveFromHit(Hit, OtherComp);

            //    if (!bHasPendingBounce)
            //    {
            //        bHasPendingBounce = true;

            //        if (GetWorld() && !GetWorld()->GetTimerManager().IsTimerActive(SafeBounceTimer))
            //        {
            //            GetWorld()->GetTimerManager().SetTimerForNextTick([this, Hit, OtherComp]()
            //                {
             //                   ProcessPendingBounce();
            //                }
            //            );
            //        }


            //    }
            }


        }


        //        // ✅ 1회 Resolve로 지형 PhysMat 획득 (기존 4번 중복 호출 → 1번으로 통합)
        UPhysicalMaterial* ResolvedPhysMat = PhysMatResolveUtil::ResolveFromHit(Hit, OtherComp);

        // ✅ 지형 물리 설정 적용
        if (ResolvedPhysMat)
        {
            FString TerrainName = GetTerrainNameFromPhysicalMaterial(ResolvedPhysMat);
            AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
            if (GameMode->IsRangeMode())
            {
                TerrainName = TEXT("Rough"); // ✅ 오타 수정
            }
            else
            {
               // ✅ LandscapeChecker null 체크 추가
                if (IsValid(LandscapeChecker) && LandscapeChecker->bUseMaskTexture)
                {
                    ELandType MaskType = LandscapeChecker->GetLandTypeFromMask(GetActorLocation());
                    if (MaskType == ELandType::Green)   TerrainName = TEXT("Green");
                    else if (MaskType == ELandType::Sand)    TerrainName = TEXT("Bunker");
                    else if (MaskType == ELandType::Fairway) TerrainName = TEXT("FairWay"); // ✅ 오타 수정
                }
            }
 

            UE_LOG(LogTemp, Log,
                TEXT("🌍 OnHit Terrain: [%s] | Ball PhysMat: Friction=%.2f Restitution=%.2f | Terrain PhysMat: Friction=%.2f Restitution=%.2f"),
                *TerrainName,
                DefaultPhysicalMaterial ? DefaultPhysicalMaterial->Friction : 0.f,
                DefaultPhysicalMaterial ? DefaultPhysicalMaterial->Restitution : 0.f,
                ResolvedPhysMat->Friction,
                ResolvedPhysMat->Restitution);

            ApplyTerrainPhysicsSettings(TerrainName, ResolvedPhysMat);

            // ⭐ 추가: 물에 닿으면 즉시 정지 (OnHit 기준, CurrentLandType 캐시와 무관하게 확실)
            if (TerrainName == TEXT("Water"))
            {
                UE_LOG(LogTemp, Warning, TEXT("💧 Water OnHit 감지: 즉시 정지 처리"));
                ForceStopBall();

                // 물리 자체를 멈춰 미끄러짐 방지 (선택: 완전 고정하려면)
                if (BallMesh && IsValid(BallMesh))
                {
                    BallMesh->PutAllRigidBodiesToSleep();
                }

                if (CurrentBallState != EBallState::Ball_Stop)
                {
                    SetBallState(EBallState::Ball_Stop);
                }

                // ⭐ 추가: 물 정지 시 카메라도 고정
                   // ⭐ 추가: 물 정지 시 카메라도 고정 (이미 링크된 CameraManager 사용)
                if (IsValid(LinkedCameraManager))
                {
                    LinkedCameraManager->StopCameraForWater();
                }
            }

            // ⭐ 추가: CheckGroundType은 0.3초/150cm 단위로만 갱신되어 지형 전환 시
//          수십 프레임(최대 0.3초)까지 FrictionWeight/CurrentLandType이 stale 상태로 남음
//          → OnHit에서 지형이 확정된 즉시 같이 동기화
           // SetFrictionWeight(ResolvedPhysMat->Friction);
            FrictionWeight = FMath::Clamp(ResolvedPhysMat->Friction, 0.0f, 2.0f);

            ELandType ResolvedLandType = ELandType::Rough;
            if (TerrainName == TEXT("Green"))        ResolvedLandType = ELandType::Green;
            else if (TerrainName == TEXT("Bunker"))  ResolvedLandType = ELandType::Sand;
            else if (TerrainName == TEXT("FairWay")) ResolvedLandType = ELandType::Fairway;

            if (CurrentLandType != ResolvedLandType)
            {
                CurrentLandType = ResolvedLandType;
            }
        }
        else
        {
            // 수정: ResolvedPhysMat가 유효하면 그걸 넘기고, TerrainName도 PhysMat 이름으로 사용
            if (IsValid(ResolvedPhysMat))
            {
                FString PhysMatName = ResolvedPhysMat->GetName();
                ApplyTerrainPhysicsSettings(PhysMatName, ResolvedPhysMat);
            }
            else
            {
                // PhysMat 자체를 못 찾았을 때만 진짜 Rough 폴백
                ApplyTerrainPhysicsSettings(TEXT("Rough"));
            }
        }



        // ✅ 사운드/파티클: ResolvedPhysMat이 null이면 DefaultPhysicalMaterial로 fallback
        //    PlaySoundByMaterial/SpawnBallParticle은 PM->GetName() 호출하므로 null 절대 불가
        UPhysicalMaterial* SoundPhysMat = ResolvedPhysMat
            ? ResolvedPhysMat
            : DefaultPhysicalMaterial;  // 연습장(StaticMesh, PhysMat 미지정)에서 nullptr 방지

        const float Now = GetWorld()->GetTimeSeconds();
        if (Now - LastHitTime > HitCooldown)
        {
            float ImpulseSize = NormalImpulse.SizeSquared();
            //if (ImpulseSize > 50)

            if (CurrentBallState == EBallState::Ball_Bound)
            {
                UE_LOG(LogTemp, Log, TEXT("NormalImpulse.SizeSquared() : %f"), ImpulseSize);
                // ⭐ 위에서 null-safe로 계산해둔 SoundPhysMat을 사용 (ResolveFromHit()을 다시 호출하면
                //    연습장 등 PhysMat 미지정 StaticMesh에서 nullptr이 나와 사운드가 조용히 스킵됨)
                PlaySoundByMaterial(SoundPhysMat, ImpulseSize);

                // 낮은 속도 충돌에서는 파티클만 생략 (사운드는 그대로 재생)
                const float CurrentSpeed = GetBallSpeed();
                if (CurrentSpeed >= MinSpeedForBounceParticle)
                {
                    SpawnBallParticle(SoundPhysMat);
                }
                else
                {
                    UE_LOG(LogTemp, Log, TEXT("🚫 Skip particle - low speed (%.1f < %.1f)"), CurrentSpeed, MinSpeedForBounceParticle);
                }
            }
        }
        LastHitTime = Now;

        if (ResolvedPhysMat)
        {
            UE_LOG(LogTemp, Log,
                TEXT("🔍 Terrain PhysMat — Friction=%.3f Restitution=%.3f"),
                ResolvedPhysMat->Friction, ResolvedPhysMat->Restitution);
        }
    }
    catch (...)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Exception in OnHit callback"));
    }

    bIsInCollisionCallback = false;
}


bool AGolfBall::IsLandscapeHit(const FHitResult& Hit)
{
    // Hit된 액터가 Landscape인지 확인
    ALandscapeProxy* Landscape = Cast<ALandscapeProxy>(Hit.GetActor());
    return (Landscape != nullptr);
}
FHitResult AGolfBall::CreateStabilizedHitResult(const FHitResult& OriginalHit)
{
    FHitResult StabilizedHit = OriginalHit;

    // 안정화된 지면 노말 적용
    FVector StabilizedNormal = GetStabilizedTerrainNormal();

    // 원래 노말과 비교하여 변화량 로깅
    float AngleDifference = FMath::RadiansToDegrees(
        FMath::Acos(FMath::Clamp(
            FVector::DotProduct(OriginalHit.Normal, StabilizedNormal),
            -1.0f, 1.0f
        ))
    );

    if (AngleDifference > 1.0f) // 1도 이상 차이날 때만 로깅
    {
        UE_LOG(LogTemp, Log, TEXT("🔧 Normal stabilized: %.1f° difference | Original: %s → Stabilized: %s"),
            AngleDifference,
            *OriginalHit.Normal.ToString(),
            *StabilizedNormal.ToString());
    }

    // 안정화된 노말을 Hit 결과에 적용
    StabilizedHit.Normal = StabilizedNormal;
    StabilizedHit.ImpactNormal = StabilizedNormal;

    return StabilizedHit;
}


// ⭐ 새로운 함수: Ball_Bound 상태에서 안정화된 노말을 사용한 바운스
void AGolfBall::HandleConstrainedBounceWithStabilizedNormal(const FHitResult& Hit)
{
    if (!BallMesh)
    {
        return;
    }

    FVector CurrentVelocity = BallMesh->GetPhysicsLinearVelocity();
    float CurrentSpeed = CurrentVelocity.Size();


    // 최소 속도 체크
    //if (CurrentSpeed < 500.0f)
    //{
    //    FVector HorizontalVelocity = FVector(CurrentVelocity.X, CurrentVelocity.Y, 0.0f);
    //    BallMesh->SetPhysicsLinearVelocity(HorizontalVelocity * 0.95f);
    //    SetBallState(EBallState::Ball_Rolling);
    //    return;
    //}

    // ⭐ 핵심 개선: 경계면에서는 완전히 수평으로 전환
    //FVector SmoothedGroundVelocity = CalculateSmoothGroundVelocity(CurrentVelocity, CurrentSpeed);
    //FVector SmoothedGroundVelocity = CalculateZeroGroundVelocity(CurrentVelocity, CurrentSpeed);

    //// 안전성 체크
    //if (IS_VECTOR_VALID(SmoothedGroundVelocity) && !SmoothedGroundVelocity.ContainsNaN())
    //{
    //    BallMesh->SetPhysicsLinearVelocity(SmoothedGroundVelocity);

    //    // 경계면에서는 굴림 상태로 빠르게 전환
    //    if (CurrentSpeed < 500.0f) // 3m/s 이하면 굴림으로 전환
    //    {
    //        SetBallState(EBallState::Ball_Rolling);
    //        UE_LOG(LogTemp, Log, TEXT("ParkGolf: Bound -> Rolling (HandleConstrainedBounceWithStabilizedNormal)"));
    //    }

    //    UE_LOG(LogTemp, Log, TEXT("✅ --------> Ball_Rolling  Smooth ground velocity applied: %.1fcm/s"),
    //        SmoothedGroundVelocity.Size());
    //}
    //else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Invalid smoothed velocity, keeping horizontal only"));
        FVector SafeVelocity = FVector(CurrentVelocity.X, CurrentVelocity.Y, 0.0f) * 0.95f;
        BallMesh->SetPhysicsLinearVelocity(SafeVelocity);
    }
}


// ⭐ 새로운 함수: 부드러운 지면 속도 계산
// ⭐ 개선: 완전히 수평면 기준으로 속도 계산
FVector AGolfBall::CalculateSmoothGroundVelocity(const FVector& OriginalVelocity, float TargetSpeed)
{
    // 1. ⭐ 핵심 수정: 수평면에서의 전진 방향 결정
    FVector HorizontalForwardDirection = FVector::ZeroVector;

    // 1-1. 마지막 샷 방향 우선 (수평 성분만)
    if (!LastShotDirection.IsNearlyZero())
    {
        HorizontalForwardDirection = FVector(LastShotDirection.X, LastShotDirection.Y, 0.0f).GetSafeNormal();
        UE_LOG(LogTemp, Log, TEXT("🎯 Using horizontal LastShotDirection: %s"),
            *HorizontalForwardDirection.ToString());
    }
    else
    {
        // 1-2. 현재 속도의 수평 성분
        FVector HorizontalVel = FVector(OriginalVelocity.X, OriginalVelocity.Y, 0.0f);
        if (!HorizontalVel.IsNearlyZero())
        {
            HorizontalForwardDirection = HorizontalVel.GetSafeNormal();
            UE_LOG(LogTemp, Log, TEXT("🎯 Using horizontal velocity direction: %s"),
                *HorizontalForwardDirection.ToString());
        }
        else
        {
            // 1-3. 폴백: 기본 전진 방향
            HorizontalForwardDirection = FVector(1.0f, 0.0f, 0.0f); // X축 기준
            UE_LOG(LogTemp, Warning, TEXT("⚠️ Using fallback horizontal direction"));
        }
    }

    // 2. ⭐ 수평면에서의 속도 크기 계산 (98% 유지)
    float HorizontalSpeed = TargetSpeed * 0.98f;

    // 3. ⭐ 완전히 수평인 속도 벡터 생성 (Z=0 보장)
    FVector PureHorizontalVelocity = FVector(
        HorizontalForwardDirection.X * HorizontalSpeed,
        HorizontalForwardDirection.Y * HorizontalSpeed,
        0.0f // Z축 완전 제거
    );

    // 4. ⭐ 지형 경사 영향 추가 (수평면에서만)
    FVector TerrainNormal = GetStabilizedTerrainNormal();

    // 지형이 경사진 경우에만 미세 조정
    if (!TerrainNormal.IsNearlyZero() && !TerrainNormal.Equals(FVector::UpVector, 0.1f))
    {
        // 경사 방향 계산 (수평 성분만)
        FVector HorizontalSlopeDirection = FVector(TerrainNormal.X, TerrainNormal.Y, 0.0f);

        if (!HorizontalSlopeDirection.IsNearlyZero())
        {
            HorizontalSlopeDirection = HorizontalSlopeDirection.GetSafeNormal();

            // 경사각 계산
            float SlopeAngle = FMath::RadiansToDegrees(
                FMath::Acos(FMath::Clamp(TerrainNormal.Z, 0.0f, 1.0f))
            );

            // 경사가 2도 이상일 때만 영향 적용
            if (SlopeAngle > 2.0f)
            {
                // 경사 영향도: 최대 5%만 적용
                float SlopeInfluence = FMath::Clamp(SlopeAngle / 45.0f, 0.0f, 0.05f);

                // 경사 방향으로 약간 조정 (수평면에서만)
                FVector AdjustedDirection = FMath::Lerp(
                    HorizontalForwardDirection,
                    -HorizontalSlopeDirection, // 경사 아래 방향
                    SlopeInfluence
                ).GetSafeNormal();

                // 수평 속도 재계산
                PureHorizontalVelocity = FVector(
                    AdjustedDirection.X * HorizontalSpeed,
                    AdjustedDirection.Y * HorizontalSpeed,
                    0.0f
                );

                UE_LOG(LogTemp, Log, TEXT("🏔️ Slope adjusted: Angle=%.1f°, Influence=%.2f%%"),
                    SlopeAngle, SlopeInfluence * 100.0f);
            }
        }
    }

    // 5. ⭐ Ball_Bound 상태에서만 최소한의 지면 접촉력 추가
    if (CurrentBallState == EBallState::Ball_Bound)
    {
        // 지면에 붙어있도록 매우 작은 아래쪽 속도
        PureHorizontalVelocity.Z = -1.0f; // -0.01m/s (지면 접촉 유지)
    }

    // 6. 최종 안전성 검증
    if (!IS_VECTOR_VALID(PureHorizontalVelocity))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Invalid horizontal velocity calculated, using safe fallback"));
        return FVector(HorizontalForwardDirection.X * HorizontalSpeed * 0.9f,
            HorizontalForwardDirection.Y * HorizontalSpeed * 0.9f,
            0.0f);
    }

    // 7. 디버그 로그
    UE_LOG(LogTemp, Log, TEXT("🌍 Pure horizontal velocity: Dir=(%.2f, %.2f, 0.00), Speed=%.1fcm/s"),
        HorizontalForwardDirection.X, HorizontalForwardDirection.Y, PureHorizontalVelocity.Size());

    return PureHorizontalVelocity;
}

// ⭐ 개선: 완전히 수평면 기준으로 속도 계산
FVector AGolfBall::CalculateZeroGroundVelocity(const FVector& OriginalVelocity, float TargetSpeed)
{
    // 1. 수평 방향 결정 (Z축 완전 무시)
    FVector HorizontalDirection = FVector::ZeroVector;

    if (!LastShotDirection.IsNearlyZero())
    {
        // 마지막 샷 방향의 XY만 사용
        HorizontalDirection.X = LastShotDirection.X;
        HorizontalDirection.Y = LastShotDirection.Y;
        HorizontalDirection.Z = 0.0f;
    }
    else
    {
        // 현재 속도의 XY만 사용
        HorizontalDirection.X = OriginalVelocity.X;
        HorizontalDirection.Y = OriginalVelocity.Y;
        HorizontalDirection.Z = 0.0f;
    }

    // 방향 벡터 정규화 (XY 평면에서만)
    float HorizontalLength = FMath::Sqrt(HorizontalDirection.X * HorizontalDirection.X +
        HorizontalDirection.Y * HorizontalDirection.Y);

    if (HorizontalLength < KINDA_SMALL_NUMBER)
    {
        // 폴백: X축 방향
        HorizontalDirection = FVector(1.0f, 0.0f, 0.0f);
    }
    else
    {
        // 수평면에서 정규화
        HorizontalDirection.X /= HorizontalLength;
        HorizontalDirection.Y /= HorizontalLength;
        HorizontalDirection.Z = 0.0f; // 명시적으로 0 설정
    }

    // 2. 수평 속도 크기 계산
    float HorizontalSpeed = TargetSpeed * 0.98f;

    // 3. 완전히 수평인 속도 벡터 생성
    FVector HorizontalVelocity;
    HorizontalVelocity.X = HorizontalDirection.X * HorizontalSpeed;
    HorizontalVelocity.Y = HorizontalDirection.Y * HorizontalSpeed;
    HorizontalVelocity.Z = 0.0f; // 강제로 0

    // 4. Ball_Bound 상태에서만 지면 접촉을 위한 미세한 Z값
   // if (CurrentBallState == EBallState::Ball_Bound)
    {
        HorizontalVelocity.Z = -1.0f; // -0.01m/s
    }

    // 5. 최종 검증
    if (FMath::IsNaN(HorizontalVelocity.X) || FMath::IsNaN(HorizontalVelocity.Y))
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid horizontal velocity, using safe fallback"));
        return FVector(HorizontalSpeed, 0.0f, 0.0f);
    }

    UE_LOG(LogTemp, Log, TEXT("Strict horizontal: (%.2f, %.2f, %.2f), Speed=%.1f"),
        HorizontalVelocity.X, HorizontalVelocity.Y, HorizontalVelocity.Z,
        FMath::Sqrt(HorizontalVelocity.X * HorizontalVelocity.X + HorizontalVelocity.Y * HorizontalVelocity.Y));

    return HorizontalVelocity;
}


void AGolfBall::ResetToReady()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(CountdownUpdateTimer);
        GetWorld()->GetTimerManager().ClearTimer(ResetReadyTimer);
    }

    TurnTransitionCountdown = 0.0f;
    TurnTransitionMaxTime = 0.0f;

    SetBallState(EBallState::Ball_Ready);

    if (bIsTrackingTrajectory)
    {
        StopTrajectoryTracking();
    }

    if (GetWorld())
    {
        DrawDebugString(GetWorld(), GetActorLocation() + FVector(0, 0, 50),
            TEXT("✅ 다음 턴 시작!"),
            nullptr, FColor::Green, 2.0f, false, 2.0f);
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
            TEXT("✅ Next Turn Started!"));
    }

    UE_LOG(LogTemp, Log, TEXT("Ball reset to Ready state after 3-second delay"));
}

// ===== 플레이어 결과 처리 =====
//void AGolfBall::TriggerPlayerResultProcessing(bool bHoleIn, bool bOutOfBounds)
//{
//    if (AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
//    {
//        if (GameMode->PlayerManager)
//        {
//            int32 PlayerIndex = FindOwnerPlayerIndex(GameMode->PlayerManager);
//            if (PlayerIndex != -1)
//            {
//                TArray<AGolfPlayer*> Players = GameMode->PlayerManager->GetPlayers();
//                if (Players.IsValidIndex(PlayerIndex))
//                {
//                    AGolfPlayer* Player = Players[PlayerIndex];
//                    if (Player)
//                    {
//                        Player->UpdateBallPosition(GetActorLocation());
//                        Player->ProcessShotResult(bHoleIn, bOutOfBounds);
//                        UE_LOG(LogTemp, Log, TEXT("✅ Result processing delegated to Player %d"), PlayerIndex);
//                    }
//                }
//            }
//        }
//    }
//}

int32 AGolfBall::FindOwnerPlayerIndex(UGolfPlayerManager* PlayerManager)
{
    if (!PlayerManager) return -1;

    TArray<AGolfBall*> PlayerBalls = PlayerManager->GetPlayerBalls();
    for (int32 i = 0; i < PlayerBalls.Num(); i++)
    {
        if (PlayerBalls[i] == this)
        {
            return i;
        }
    }

    return -1;
}

// ===== Tick 함수 =====
void AGolfBall::Tick(float DeltaTime)
{
    // ⭐ 가장 먼저 파괴 상태 체크
    if (bIsBeingDestroyed || !IsValid(this))
    {
        return;
    }
    // DeltaTime 유효성 체크
    if (DeltaTime <= 0.0f || !FMath::IsFinite(DeltaTime) || DeltaTime > 1.0f)
    {
        return;
    }

    Super::Tick(DeltaTime);

    // ===== 성능 최적화: 프레임 단위 LineTrace 캐시 리셋 =====
    // 같은 프레임 내 IsNearGround/UpdateRollingPhysics 등에서 결과를 재사용
    bFrameGroundCacheValid = false;



    // ================= 볼마커 설정 =============================

    if (GroundMarkerMesh && BallNamePlateComponent)
    {
        if (IsHoleIn())
        {
            GroundMarkerMesh->SetVisibility(false);
            BallNamePlateComponent->SetNamePlateVisible(false);
        }

        if (GM->FindPlayer(OwningPlayerIndex))
        {
            if (GM->FindPlayer(OwningPlayerIndex)->bIsPendingDelete)
            {
                GroundMarkerMesh->SetVisibility(false);
                BallNamePlateComponent->SetNamePlateVisible(false);
            }
        }
    }



    if (GroundMarkerMesh && GroundMarkerMesh->IsVisible())
    {
        FHitResult Hit;
        const FVector Start = GetActorLocation() + FVector(0, 0, 50);
        const FVector End = GetActorLocation() - FVector(0, 0, 1000);

        FCollisionQueryParams Params(SCENE_QUERY_STAT(GroundMarkerTrace), false);
        Params.AddIgnoredActor(this);
        // ✅ 최적화: BeginPlay에서 캐시한 배열 재사용 (매 프레임 GetAllActorsOfClass 제거)
        Params.AddIgnoredActors(CachedGolfBallActors);

        if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
            return;

        // 1) 위치: 지면에 붙이기 + 살짝 띄우기
        const FVector GroundPos = Hit.Location + Hit.ImpactNormal * 0.5f;
        GroundMarkerMesh->SetWorldLocation(GroundPos);

        // 2) 회전: 지면 법선 기반으로 눕히되, Yaw는 고정
        const FVector Up = Hit.ImpactNormal.GetSafeNormal();

        // "고정 yaw"에 해당하는 월드 Forward(수평) 벡터
        const FRotator YawOnly(0.f, MarkerFixedRotation.Yaw, 0.f);
        FVector Forward = YawOnly.Vector(); // (Pitch=0, Roll=0)의 Forward

        // 경사면에 맞게 Forward를 평면에 투영
        Forward = ProjectOnPlane(Forward, Up).GetSafeNormal();
        if (Forward.IsNearlyZero())
        {
            // Up과 거의 평행한 경우(거의 수직 벽 같은 특이 케이스) fallback
            Forward = FVector::CrossProduct(FVector::RightVector, Up).GetSafeNormal();
        }

        const FVector Right = FVector::CrossProduct(Up, Forward).GetSafeNormal();

        // 회전 생성(Forward/Right/Up 정규직교)
        const FMatrix M(Forward, Right, Up, FVector::ZeroVector);
        const FRotator MarkerRot = M.Rotator();

        GroundMarkerMesh->SetWorldRotation(MarkerRot);
    }


    //===========================================================


    if (BallMesh)
    {
        // 다음 물리 스텝 전에 "현재 속도"를 저장 → 충돌 전 속도로 사용
        LastLinearVelocity = BallMesh->GetPhysicsLinearVelocity();
        const float Speed = LastLinearVelocity.Size();
    }

    //===========================================================





    //물리 상태 검증 및 자동 복구
    // ✅ 최적화: static 로컬 → 멤버 변수 (볼 인스턴스별 독립 타이머)
    float CurrentTime = GetWorld()->GetTimeSeconds();

    if (CurrentTime - LastValidationTime > 1.0f) // 1초마다 검증
    {
        ValidateAndFixPhysicsState();
        LastValidationTime = CurrentTime;
    }

    //실시간 에러 감지
    // ✅ 최적화: static 로컬 → 멤버 변수
    float fCurrentTime = GetWorld()->GetTimeSeconds();

    if (BallMesh && BallMesh->IsSimulatingPhysics())
    {
        FVector CurrentAngularVel = BallMesh->GetPhysicsAngularVelocityInDegrees();
        if (CurrentAngularVel.Size() > 0.0f)
        {
            UE_LOG(LogTemp, VeryVerbose, TEXT("🔄 Current Spin: %.2f deg/s"), CurrentAngularVel.Size());
        }
    }

    if (fCurrentTime - LastErrorCheckTime > 1.0f) // 1초마다 체크
    {
        if (BallMesh)
        {
            bool bSimulating = BallMesh->IsSimulatingPhysics();
            ECollisionEnabled::Type CollisionType = BallMesh->GetCollisionEnabled();

            // 에러 조합 감지 및 자동 수정
            if (bSimulating && CollisionType == ECollisionEnabled::NoCollision)
            {
                UE_LOG(LogTemp, Error, TEXT("🚨 AUTO-FIX: Detected invalid physics/collision state"));

                // 자동 수정
                SetPhysicsAndCollisionSafely(true, true);
#if WITH_EDITOR

                if (GEngine)
                {
                    GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red,
                        TEXT("🔧 Auto-fixed physics/collision error"));
                }
#endif
            }
        }

        LastErrorCheckTime = fCurrentTime;
    }

    // ⭐ 즉시 추가: DeltaTime 유효성 체크
    if (DeltaTime <= 0.0f || !FMath::IsFinite(DeltaTime) || DeltaTime > 1.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Tick: Invalid DeltaTime=%.6f, skipping"), DeltaTime);
        return;
    }



    if (!IsValid(this) || !BallMesh) return;

    if (CurrentBallState == EBallState::Ball_Fly ||
        CurrentBallState == EBallState::Ball_Rolling ||
        CurrentBallState == EBallState::Ball_Bound)
    {
        //CheckGroundPenetration();
    }

    if (GetActorLocation().Z < -100000.0f)
    {
        CheckBallOutOfBounds();
    }

    UpdateTrajectoryTracking(DeltaTime);
    TrackShotDistance();
    UpdatePhysicsBasedOnState(DeltaTime);
    CheckAutoStateTransitions();

    if (CurrentBallState == EBallState::Ball_Fly)
    {
        float CurrentSpeed = GetBallSpeed();
        float CurrentHeight = GetActorLocation().Z;
    }


    // ⭐ Trail 업데이트 추가 (기존 코드 뒤에)

    if (LinkedCameraManager)
    {
        // if (LinkedCameraManager->GetCameraModeOption())
        {
            if (TrailSettings.bShowTrail)
            {
                if (CurrentBallState == EBallState::Ball_Fly ||
                    CurrentBallState == EBallState::Ball_Bound ||
                    CurrentBallState == EBallState::Ball_Rolling)
                {
                    UpdateBallTrail(DeltaTime);
                    if (TrailPoints.Num() > 1)
                        DrawBallTrail();
                }

            }
        }

    }


    if (GetBallSpeed() > 10.0f)
    {
        // ✅ 최적화: 매 프레임 GetAuthGameMode+Cast 제거 → BeginPlay에서 캐시한 GM 사용
        if (GM && GM->MiniMapWidget)
        {
            GM->MiniMapWidget->UpdateBallPosition(OwningPlayerIndex, GetActorLocation());
        }
    }

    if (CurrentBallState == EBallState::Ball_Fly ||
        CurrentBallState == EBallState::Ball_Bound ||
        CurrentBallState == EBallState::Ball_Rolling)
    {
        UpdateCrosshairPosition();

        // ✅ 최적화: CheckGroundType 매 프레임 → 거리/상태 기반 조건부 호출
        // 계측 결과: LandscapeChecker::GetLandType avg 131ms / max 348ms (전체 프레임 병목)
        // 전략 1) Ball_Fly 중에는 지형 판정 불필요 → 스킵
        // 전략 2) 상태 전환 시(Bound/Rolling 진입) 즉시 1회 갱신
        // 전략 3) Rolling/Bound 중에는 150cm 이상 이동했을 때만 갱신
        {
            const FVector CurrentPos = GetActorLocation();
            const bool bStateChanged = (LandTypeLastState != CurrentBallState);
            const float MovedDist = FVector::Dist2D(CurrentPos, LandTypeLastCheckPos);
            const bool bFarEnough = (MovedDist >= LandTypeCheckInterval);

            if (bStateChanged)
            {
                bLandTypeDirty = true;
                LandTypeLastState = CurrentBallState;
            }

            // ✅ 추가: 타이머 기반 강제 갱신
            // 거리 조건과 무관하게 0.3초마다 강제 체크
            // 그린 경계에서 멈춰있어도 즉시 감지
            LandTypeForceCheckTimer += DeltaTime;
            if (LandTypeForceCheckTimer >= LANDTYPE_FORCE_CHECK_INTERVAL)
            {
                bLandTypeDirty = true;
                LandTypeForceCheckTimer = 0.0f;
            }

            const bool bShouldCheck = (CurrentBallState != EBallState::Ball_Fly)
                && (CurrentBallState != EBallState::Ball_Stop)   // ✅ Stop 중 불필요한 체크 제거
                && (CurrentBallState != EBallState::Ball_Init)   // ✅ Init 중 불필요한 체크 제거
                && (bLandTypeDirty || bFarEnough);

            if (bShouldCheck)
            {
                CheckGroundType();
                LandTypeLastCheckPos = CurrentPos;
                bLandTypeDirty = false;
                LandTypeForceCheckTimer = 0.0f; // 체크했으면 타이머 리셋
            }
        }

        // ⭐ 추가: 공이 움직이는 동안 실시간 OB 교차점 체크
        CheckRealtimeOBCrossing();

        if (!bIsCinematic)
        {

            // ⭐ 현재 볼 속도 및 속도 관련 정보
            float CurrentBallSpeed = GetBallSpeed();
            // 📊 상세 속도 정보 로깅

          // UE_LOG(LogTemp, Log, TEXT("  Current GetBallSpeed()- Total Speed: %.1f cm/s (%.2f m/s)"),
          //     CurrentBallSpeed, CurrentBallSpeed / 100.0f);

            if (GM->GetCurrentTurnGolfPlayer())
            {
                FVector HolecupPos = GM->MapInfo.HolecupPositions[GM->CurrentHole - 1];
                FVector BallPos = GetActorLocation();
                FVector StartPos = GM->GetCurrentTurnGolfPlayer()->BEFOREPos;

                float Distance = FVector::Dist(BallPos, HolecupPos);
                float Distance2 = FVector::Dist(GM->GetCurrentTurnGolfPlayer()->BEFOREPos, BallPos);

                if (Distance <= 3.f * 100.f)
                {

                    if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
                    {
                        AGolfPlayerController* GPC = Cast<AGolfPlayerController>(PC);
                        // 이벤트카메라 제거
                     //   GPC->ShotCinematicComponent->TryPlayNearCupCinematic(BallPos, HolecupPos, Distance2, GPC->ShotYawAngle, GPC->ShotPower, 0.f, CurrentBallSpeed);
                     //   bIsCinematic = true;
                    }
                }
            }
        }
    }

    // ⭐ 추가: 다음 프레임을 위해 현재 위치를 이전 위치로 저장 (OB 교차점 계산용)
    PreviousBallPosition = GetActorLocation();
}


// ===== 추가 OB 관련 함수들 =====

// ⭐ 새로 추가: 실시간 OB 교차점 체크 (매 프레임마다 호출)
void AGolfBall::CheckRealtimeOBCrossing()
{
    // ⭐⭐⭐ 핵심: 이미 교차점이 저장되어 있으면 더 이상 체크하지 않음
    if (bHasValidOBCrossingPoint)
    {
        return; // 처음 한 번만 저장!
    }

    // 공이 움직이는 중이 아니면 체크하지 않음
    if (CurrentBallState != EBallState::Ball_Fly &&
        CurrentBallState != EBallState::Ball_Bound &&
        CurrentBallState != EBallState::Ball_Rolling)
    {
        return;
    }

    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    if (!GameMode || GameMode->CurrentGameMode != EGolfGameMode::StrokeMode)
    {
        return;
    }

    int32 CurrentHoleIndex = GameMode->CurrentHole - 1;
    if (!GameMode->MapInfo.OBLines.IsValidIndex(CurrentHoleIndex))
    {
        return;
    }

    const TArray<FVector>& OBPoints = GameMode->MapInfo.OBLines[CurrentHoleIndex].Points;
    if (OBPoints.Num() < 3)
    {
        return;
    }

    // 이전 위치가 유효한지 확인
    if (PreviousBallPosition.IsZero())
    {
        return;
    }

    FVector CurrentPosition = GetActorLocation();
    FVector2D PrevPos2D(PreviousBallPosition.X, PreviousBallPosition.Y);
    FVector2D CurrentPos2D(CurrentPosition.X, CurrentPosition.Y);

    // 이전 위치와 현재 위치의 OB 상태 확인
    bool bPreviousWasInside = !IsPointOutsidePolygonImproved(PrevPos2D, OBPoints);
    bool bCurrentIsOutside = IsPointOutsidePolygonImproved(CurrentPos2D, OBPoints);

    // 이전에는 안쪽, 현재는 밖 -> OB 라인을 지나감!
    if (bPreviousWasInside && bCurrentIsOutside)
    {
        // 정확한 교차점 계산
        FVector IntersectionPoint = CalculateOBLineIntersection(
            PreviousBallPosition,
            CurrentPosition,
            OBPoints
        );

        if (!IntersectionPoint.IsZero())
        {
            LastOBCrossingPoint = IntersectionPoint;
            bHasValidOBCrossingPoint = true;

            UE_LOG(LogTemp, Warning, TEXT("🎯 REALTIME OB Crossing detected at: %s"),
                *LastOBCrossingPoint.ToString());
            UE_LOG(LogTemp, Log, TEXT("   Previous pos: %s (Inside)"), *PreviousBallPosition.ToString());
            UE_LOG(LogTemp, Log, TEXT("   Current pos: %s (Outside)"), *CurrentPosition.ToString());
            UE_LOG(LogTemp, Log, TEXT("   Crossing point: %s"), *IntersectionPoint.ToString());

            // 디버그 시각화 (선택사항)
#if WITH_EDITOR
            if (GetWorld())
            {
                // 이전 위치에서 교차점까지 초록선
                DrawDebugLine(GetWorld(), PreviousBallPosition, IntersectionPoint,
                    FColor::Green, false, 10.0f, 0, 5.0f);
                // 교차점에서 현재 위치까지 빨간선
                DrawDebugLine(GetWorld(), IntersectionPoint, CurrentPosition,
                    FColor::Red, false, 10.0f, 0, 5.0f);
                // 교차점에 큰 구체
                DrawDebugSphere(GetWorld(), IntersectionPoint, 20.0f, 12,
                    FColor::Yellow, false, 10.0f, 0, 3.0f);
            }
#endif
        }
        else
        {
            // 교차점 계산 실패시 현재 위치 사용 (폴백)
            LastOBCrossingPoint = CurrentPosition;
            bHasValidOBCrossingPoint = true;

            UE_LOG(LogTemp, Warning, TEXT("⚠️ REALTIME OB detected but intersection failed, using current pos: %s"),
                *LastOBCrossingPoint.ToString());
        }
    }
}


float AGolfBall::FindMinDistanceToOBLine(const FVector2D& Point, const TArray<FVector>& OBPoints) const
{
    float MinDistance = FLT_MAX;

    for (int32 i = 0; i < OBPoints.Num(); i++)
    {
        int32 NextIndex = (i + 1) % OBPoints.Num();
        FVector2D LineStart(OBPoints[i].X, OBPoints[i].Y);
        FVector2D LineEnd(OBPoints[NextIndex].X, OBPoints[NextIndex].Y);

        float Distance = DistancePointToLineSegment(Point, LineStart, LineEnd);
        MinDistance = FMath::Min(MinDistance, Distance);
    }

    return MinDistance;
}

float AGolfBall::DistancePointToLineSegment(const FVector2D& Point, const FVector2D& LineStart, const FVector2D& LineEnd) const
{
    FVector2D LineVec = LineEnd - LineStart;
    FVector2D PointVec = Point - LineStart;

    float LineLength = LineVec.Size();
    if (LineLength < KINDA_SMALL_NUMBER)
    {
        return FVector2D::Distance(Point, LineStart);
    }

    float t = FVector2D::DotProduct(PointVec, LineVec) / (LineLength * LineLength);
    t = FMath::Clamp(t, 0.0f, 1.0f);

    FVector2D ClosestPoint = LineStart + t * LineVec;

    return FVector2D::Distance(Point, ClosestPoint);
}

// ===== 높이 조정 시스템 =====

void AGolfBall::AdjustBallToGroundLevel()
{
    if (!BallMesh || !IsValid(this))
    {
        UE_LOG(LogTemp, Error, TEXT("BallMesh invalid in AdjustBallToGroundLevel"));
        return;
    }

    //if (CurrentBallState == EBallState::Ball_Ready ||
    //    CurrentBallState == EBallState::Ball_Init)
    //    return;

    // 🔧 정확한 볼 반지름 계산
    float ActualBallRadius = GetActualBallRadius();
    FVector CurrentLocation = GetActorLocation();

    UE_LOG(LogTemp, Log, TEXT("🏌️ Adjusting ball position: Current=%s, Radius=%.2fcm"),
        *CurrentLocation.ToString(), ActualBallRadius);

    // 🔧 개선된 지면 감지: 볼 중심에서 아래로 레이캐스트
    FVector TraceStart = CurrentLocation + FVector(0, 0, ActualBallRadius * 2.0f); // 볼 위에서 시작
    FVector TraceEnd = CurrentLocation - FVector(0, 0, ActualBallRadius * 3.0f);   // 볼 아래까지

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;

    for (AGolfBall* Ball : GM->PlayerManager->GetPlayerBalls())
    {
        if (IsValid(Ball))
        {
            QueryParams.AddIgnoredActor(Ball); // AGolfBall* -> AActor* 자동 업캐스트 OK
        }
    }

    QueryParams.AddIgnoredActor(this);
    QueryParams.bTraceComplex = true;
    QueryParams.bReturnPhysicalMaterial = true;

    bool bHitGround = GetWorld()->LineTraceSingleByChannel(
        HitResult, TraceStart, TraceEnd,
        ECC_WorldStatic, QueryParams
    );

    if (bHitGround)
    {
        // 🔧 정확한 위치 계산: 지면 위치 + 볼 반지름
        FVector GroundLocation = HitResult.Location;
        FVector GroundNormal = HitResult.Normal;
        // 지면 법선 방향으로 볼 반지름만큼 위에 위치
        FVector CorrectedPosition = GroundLocation + (GroundNormal * ActualBallRadius);

        // ✅ 티샷일 때 Tee_Height 추가 적용
        if (CheckTeeShot())
        {
            float TeeHeightOffset = 0.0f;

            // PlayerInfo의 Tee_Height 값 가져오기 (단위: mm → cm 변환)
            if (GM && GM->PlayerManager)
            {
                AGolfPlayer* CurrentPlayer = GM->PlayerManager->GetPlayers().IsValidIndex(OwningPlayerIndex)
                    ? GM->PlayerManager->GetPlayers()[OwningPlayerIndex]
                    : nullptr;

                if (CurrentPlayer)
                {
                    // Tee_Height는 mm 단위로 저장됨 → UE는 cm 단위 → /10
                    TeeHeightOffset = (float)CurrentPlayer->PlayerInfo.Tee_Height / 10.0f;
                    UE_LOG(LogTemp, Log, TEXT("🏌️ TeeShot Height Offset: %.2fcm (Tee_Height: %d mm)"),
                        TeeHeightOffset, CurrentPlayer->PlayerInfo.Tee_Height);
                }
            }

            CorrectedPosition.Z += TeeHeightOffset;
        }

        // 🔧 안전 체크: 너무 큰 위치 변화 방지
        float HeightDifference = FMath::Abs(CurrentLocation.Z - CorrectedPosition.Z);
        if (HeightDifference > 100.0f) // 1m 이상 차이나면 제한
        {
            UE_LOG(LogTemp, Warning, TEXT("⚠️ Large height adjustment detected: %.1fcm, limiting"),
                HeightDifference);

            float Direction = (CorrectedPosition.Z > CurrentLocation.Z) ? 1.0f : -1.0f;
            CorrectedPosition.Z = CurrentLocation.Z + (Direction * 100.0f);
        }

        // 위치 설정
        SetActorLocation(CorrectedPosition, false, nullptr, ETeleportType::TeleportPhysics);
        SetActorRotation(FRotator::ZeroRotator);

        UE_LOG(LogTemp, Log, TEXT("✅ Ball positioned: Ground=%.1f, Ball=%.1f (Radius=%.2fcm)"),
            GroundLocation.Z, CorrectedPosition.Z, ActualBallRadius);

    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ No ground found below ball at %s"),
            *CurrentLocation.ToString());

        // 🔧 확장된 범위로 지면 검색
        TryExtendedGroundSearch(CurrentLocation, ActualBallRadius);
    }
}

void AGolfBall::TryExtendedGroundSearch(const FVector& CurrentLocation, float BallRadius)
{
    if (CurrentBallState == EBallState::Ball_Ready)
        return;

    UE_LOG(LogTemp, Warning, TEXT("🔍 Trying extended ground search..."));

    // 더 넓은 범위로 지면 검색
    FVector ExtendedStart = CurrentLocation + FVector(0, 0, 500.0f);  // 5m 위에서
    FVector ExtendedEnd = CurrentLocation - FVector(0, 0, 1000.0f);   // 10m 아래까지

    FHitResult ExtendedHit;
    FCollisionQueryParams ExtendedQuery;

    ExtendedQuery.AddIgnoredActor(this);
    ExtendedQuery.bTraceComplex = false; // 단순 충돌로 더 넓게 검색

    if (GetWorld()->LineTraceSingleByChannel(ExtendedHit, ExtendedStart, ExtendedEnd,
        ECC_WorldStatic, ExtendedQuery))
    {
        FVector SafePosition = ExtendedHit.Location + FVector(0, 0, BallRadius + 5.0f);
        SetActorLocation(SafePosition, false, nullptr, ETeleportType::TeleportPhysics);

        UE_LOG(LogTemp, Warning, TEXT("✅ Extended search success: %s"), *SafePosition.ToString());

        //// 경고 표시
        //if (GetWorld())
        //{
        //    DrawDebugSphere(GetWorld(), SafePosition, BallRadius, 16, FColor::Orange, false, 5.0f);
        //    DrawDebugString(GetWorld(), SafePosition + FVector(0, 0, 50),
        //        TEXT("⚠️ Extended Search Result"), nullptr, FColor::Orange, 5.0f, false);
        //}
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Extended ground search failed! Ball may be in invalid location"));

        // 최후 수단: 기본 높이로 설정
        FVector EmergencyPosition = FVector(CurrentLocation.X, CurrentLocation.Y, 100.0f);
        SetActorLocation(EmergencyPosition);

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
                TEXT("⚠️ Ball placed at emergency position"));
        }
    }
}

void AGolfBall::UpdateFriction()
{
    // 기존 호환성을 위한 함수
    UpdatePhysicsParameters();
}




void AGolfBall::HandleOBDrop()
{
    UE_LOG(LogTemp, Warning, TEXT("🚨 OB 드롭 처리 시작"));

    if (CheckTeeShot())
    {
        UE_LOG(LogTemp, Warning, TEXT("티샷 벌타 드롭 시도, 금지"));
        return;
    }

    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    AGolfPlayerController* PC = Cast<AGolfPlayerController>(
        UGameplayStatics::GetPlayerController(GetWorld(), 0));

    if (!GameMode) return;

    AGolfPlayer* OwningPlayer = GameMode->FindPlayer(OwningPlayerIndex);
    if (!OwningPlayer) return;


    // ─────────────────────────────────────────
    // ✅ Case 2: 양파 아님 → 드롭 위치 계산 후 타이머 등록
    // ─────────────────────────────────────────
    FVector DropPosition = FVector::ZeroVector;

    if (bHasValidOBCrossingPoint)
    {
        DropPosition = CalculateOBDropFromCrossingPoint();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ No valid OB crossing point, using fallback method"));
        DropPosition = CalculateDropPosition();
    }



    // 드롭 타이머 등록 (양파 아닐 때만 여기 도달)
    FTimerHandle DropTimer;
    GetWorldTimerManager().SetTimer(DropTimer, [this, DropPosition, GameMode, PC]()
        {
            // 벌타 추가
            IncrementOwningPlayerShotCount(); // +1
            IncrementOwningPlayerShotCount(); // +2
            UE_LOG(LogTemp, Warning, TEXT("🚨 OB 벌타 2타 추가 완료"));

            // 볼 드롭 실행
            ExecuteDrop(DropPosition);

            // 드롭 사운드
            if (auto* SM = GetGameInstance()->GetSubsystem<USoundManager>())
                SM->PlayAtLocation_ById("Effect.Ball.Drop", DropPosition, 1.0f);

            if (PC && PC->bTerrainGridVisible)
                PC->ToggleTerrainGrid();

            if (GameMode)
            {
                GameMode->GetCurrentSlot()->UpdateStroke(
                    GameMode->FindPlayer(OwningPlayerIndex)->PlayerInfo);
                GameMode->PlayerManager->UpdateGameInfoBallPos();
            }

            UE_LOG(LogTemp, Log, TEXT("✅ OB 드롭 완료: %s"), *DropPosition.ToString());

        }, 1.0f, false);
}


void AGolfBall::ExecuteDrop(const FVector& DropPosition)
{
    // 볼 이동
    if (CurrentBallState == EBallState::Ball_Ready)
        return;


    FTimerHandle DropTimer;
    GetWorldTimerManager().SetTimer(DropTimer, [this, DropPosition]()
        {

            // 물리 상태 초기화
            if (BallMesh)
            {
                BallMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
                BallMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
            }
            SetActorLocation(DropPosition, false, nullptr, ETeleportType::TeleportPhysics);

            // 지면에 정확히 위치시키기
            AdjustBallToGroundLevel();
            GM->FindPlayer(OwningPlayerIndex)->UpdateBallPosition(GetActorLocation());

            // OB 상태 해제
            bIsOutOfBounds = false;
            UE_LOG(LogTemp, Log, TEXT("🏌️ OB 드롭 실행: %s"), *DropPosition.ToString());

        }, 3.0f, false);
}


FVector AGolfBall::CalculateDropPosition() const
{
    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    if (!GameMode)
    {
        UE_LOG(LogTemp, Warning, TEXT("CalculateDropPosition: GameMode is null"));
        return FVector::ZeroVector;
    }

    // 현재 홀의 OB 라인 가져오기
    int32 CurrentHoleIndex = GameMode->CurrentHole - 1;
    if (!GameMode->MapInfo.OBLines.IsValidIndex(CurrentHoleIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("CalculateDropPosition: No OB lines for hole %d"), GameMode->CurrentHole);
        return FVector::ZeroVector;
    }

    const TArray<FVector>& OBPoints = GameMode->MapInfo.OBLines[CurrentHoleIndex].Points;

    if (OBPoints.Num() < 3)
    {
        UE_LOG(LogTemp, Warning, TEXT("CalculateDropPosition: Insufficient OB points"));
        return FVector::ZeroVector;
    }

    // OB 라인에서 가장 가까운 점 찾기
    FVector ClosestOBPoint = FindClosestPointOnOBLine(OBPoints);

    // 인바운드 드롭 위치 계산
    FVector DropPosition = CalculateInBoundsPosition(ClosestOBPoint, OBPoints, OB_DROP_INSET_DISTANCE);

    return DropPosition;
}

FVector AGolfBall::FindClosestPointOnOBLine(const TArray<FVector>& OBPoints) const
{
    FVector BallLocation = GetActorLocation();
    FVector2D BallPos2D(BallLocation.X, BallLocation.Y);

    FVector ClosestPoint = FVector::ZeroVector;
    float MinDistance = FLT_MAX;

    // 모든 OB 라인 세그먼트를 확인하여 가장 가까운 점 찾기
    for (int32 i = 0; i < OBPoints.Num(); i++)
    {
        int32 NextIndex = (i + 1) % OBPoints.Num();

        FVector2D LineStart(OBPoints[i].X, OBPoints[i].Y);
        FVector2D LineEnd(OBPoints[NextIndex].X, OBPoints[NextIndex].Y);

        // 라인 세그먼트에서 볼까지 가장 가까운 점 계산
        FVector2D ClosestPointOnSegment = GetClosestPointOnLineSegment(BallPos2D, LineStart, LineEnd);
        float Distance = FVector2D::Distance(BallPos2D, ClosestPointOnSegment);

        if (Distance < MinDistance)
        {
            MinDistance = Distance;
            // Z값은 원래 OB 포인트의 평균 사용
            float AverageZ = (OBPoints[i].Z + OBPoints[NextIndex].Z) * 0.5f;
            ClosestPoint = FVector(ClosestPointOnSegment.X, ClosestPointOnSegment.Y, AverageZ);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("가장 가까운 OB 점: %s (거리: %.1fcm)"),
        *ClosestPoint.ToString(), MinDistance);

    return ClosestPoint;
}

FVector2D AGolfBall::GetClosestPointOnLineSegment(const FVector2D& Point, const FVector2D& LineStart, const FVector2D& LineEnd) const
{
    FVector2D LineVec = LineEnd - LineStart;
    FVector2D PointVec = Point - LineStart;

    float LineLength = LineVec.Size();
    if (LineLength < KINDA_SMALL_NUMBER)
    {
        return LineStart;
    }

    // 라인 상의 매개변수 t 계산 (0~1 범위로 클램프)
    float t = FVector2D::DotProduct(PointVec, LineVec) / (LineLength * LineLength);
    t = FMath::Clamp(t, 0.0f, 1.0f);

    // 가장 가까운 점 계산
    FVector2D ClosestPoint = LineStart + t * LineVec;

    return ClosestPoint;
}

FVector AGolfBall::CalculateInBoundsPosition(const FVector& ClosestOBPoint, const TArray<FVector>& OBPoints, float InsetDistance) const
{
    FVector BallLocation = GetActorLocation();
    const float BallRadius = GetActualBallRadius();

    UE_LOG(LogTemp, Log, TEXT("🏌️ CalculateInBoundsPosition: Ball=%s, ClosestOB=%s"),
        *BallLocation.ToString(), *ClosestOBPoint.ToString());

    // 1. 인바운드 방향 계산
    FVector2D InwardDirection = CalculateInwardNormal(ClosestOBPoint, OBPoints);

    // 2. 여러 거리에서 안전한 위치 찾기
    TArray<float> SearchDistances = { InsetDistance, InsetDistance * 1.5f, InsetDistance * 2.0f, InsetDistance * 3.0f };
    TArray<float> SideOffsets = { 0.0f, 50.0f, -50.0f, 100.0f, -100.0f }; // 좌우로도 시도

    for (float Distance : SearchDistances)
    {
        for (float SideOffset : SideOffsets)
        {
            // 기본 인바운드 위치 계산
            FVector2D SearchPos2D = FVector2D(ClosestOBPoint.X, ClosestOBPoint.Y) + (InwardDirection * Distance);

            // 좌우 오프셋 적용
            if (FMath::Abs(SideOffset) > 0.1f)
            {
                FVector2D SideDirection = FVector2D(-InwardDirection.Y, InwardDirection.X); // 수직 방향
                SearchPos2D += SideDirection * SideOffset;
            }

            FVector SearchPosition = FVector(SearchPos2D.X, SearchPos2D.Y, ClosestOBPoint.Z);

            // 3. 랜드스케이프 기반 위치 조정
            FVector LandscapePosition = FindLandscapePosition(SearchPosition, BallRadius);

            if (LandscapePosition == FVector::ZeroVector)
            {
                UE_LOG(LogTemp, Log, TEXT("❌ No landscape found at search position"));
                continue; // 랜드스케이프를 찾지 못하면 다음 시도
            }

            // 4. 오브젝트 회피 검사 및 조정
            FVector SafePosition = FindObstacleAvoidancePosition(LandscapePosition, BallRadius);

            // 5. 최종 안전성 검사
            if (IsFinalPositionSafe(SafePosition, BallRadius, OBPoints))
            {
                UE_LOG(LogTemp, Log, TEXT("✅ Safe position found: %s (Distance=%.1f, SideOffset=%.1f)"),
                    *SafePosition.ToString(), Distance, SideOffset);
                return SafePosition;
            }
        }
    }

    UE_LOG(LogTemp, Error, TEXT("❌ Failed to find safe inbounds position"));
    return FVector::ZeroVector;
}

FVector2D AGolfBall::CalculateInwardNormal(const FVector& ClosestOBPoint, const TArray<FVector>& OBPoints) const
{
    // 폴리곤의 중심점 계산
    FVector2D PolygonCenter = FVector2D::ZeroVector;
    for (const FVector& Point : OBPoints)
    {
        PolygonCenter += FVector2D(Point.X, Point.Y);
    }
    PolygonCenter /= OBPoints.Num();

    // OB 점에서 폴리곤 중심으로의 방향 (내부 방향)
    FVector2D ClosestOBPos2D(ClosestOBPoint.X, ClosestOBPoint.Y);
    FVector2D InwardDirection = (PolygonCenter - ClosestOBPos2D).GetSafeNormal();

    return InwardDirection;
}

float AGolfBall::GetGroundHeightAtPosition(const FVector& Position) const
{
    FVector StartTrace = Position + FVector(0, 0, 100.0f); // 위에서 시작
    FVector EndTrace = Position - FVector(0, 0, 200.0f);   // 아래까지

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    QueryParams.bTraceComplex = true;

    if (GetWorld()->LineTraceSingleByChannel(HitResult, StartTrace, EndTrace, ECC_WorldStatic, QueryParams))
    {
        return HitResult.Location.Z;
    }

    // 지면을 찾지 못한 경우 원래 위치의 Z값 사용
    return Position.Z;
}

bool AGolfBall::IsPositionInBounds(const FVector& Position, const TArray<FVector>& OBPoints) const
{
    FVector2D Pos2D(Position.X, Position.Y);

    // IsPointOutsidePolygon을 직접 구현 (const 함수에서 호출 가능하도록)
    int32 NumPoints = OBPoints.Num();
    if (NumPoints < 3) return false; // 유효하지 않은 폴리곤은 외부로 간주

    int32 Crossings = 0;

    for (int32 i = 0; i < NumPoints; i++)
    {
        int32 j = (i + 1) % NumPoints;

        FVector2D P1(OBPoints[i].X, OBPoints[i].Y);
        FVector2D P2(OBPoints[j].X, OBPoints[j].Y);

        // 선분이 테스트 포인트의 Y 레벨과 교차하는지 확인
        if (((P1.Y > Pos2D.Y) != (P2.Y > Pos2D.Y)))
        {
            // 교차점의 X 좌표 계산
            float IntersectionX = P1.X + (P2.X - P1.X) * (Pos2D.Y - P1.Y) / (P2.Y - P1.Y);

            // 교차점이 테스트 포인트의 오른쪽에 있으면 카운트
            if (Pos2D.X < IntersectionX)
            {
                Crossings++;
            }
        }
    }

    // 홀수 번 교차하면 내부, 짝수 번 교차하면 외부
    // 내부이면 true, 외부이면 false 반환
    return (Crossings % 2) == 1;
}


void AGolfBall::ExecutePenaltyDrop(const FVector& DropPosition)
{
    SetActorLocation(DropPosition, false, nullptr, ETeleportType::TeleportPhysics);

    // 물리 상태 초기화
    if (BallMesh)
    {
        BallMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
        BallMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    }

    // 상태를 Ready로 변경
    //SetBallState(EBallState::Ball_Ready);

    // 지면에 정확히 위치시키기
    AdjustBallToGroundLevel();

    // OB 상태 해제
    bIsOutOfBounds = false;

    UE_LOG(LogTemp, Log, TEXT("🏌️ OB 드롭 실행: %s"), *DropPosition.ToString());
}



// ⭐ 새로운 함수: 지속적인 지면 체크
void AGolfBall::CheckGroundPenetration()
{


    //  if (CurrentBallState == EBallState::Ball_Ready 
          //|| CurrentBallState == EBallState::Ball_Stop
    //      || CurrentBallState == EBallState::Ball_Init)
    //      return;

    float ActualBallRadius = GetActualBallRadius();
    FVector BallLocation = GetActorLocation();

    FVector Start = BallLocation;
    FVector End = BallLocation - FVector(0, 0, ActualBallRadius + 10.0f); // 트레이스 거리를 약간 늘려 여유 확보

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    QueryParams.bTraceComplex = true;

    if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_WorldStatic, QueryParams))
    {
        float DistanceToGround = FVector::Dist(BallLocation, HitResult.Location);

        UE_LOG(LogTemp, Log, TEXT("✅ LineTraceSingleByChannel: HIT"));

        // ⭐ 수정: 관통 감지 조건에 미세한 여유를 주거나, 절대적인 기준 적용
        // 예를 들어, 볼 중심이 지면 아래로 1cm 이상 내려갔을 때
        float PenetrationThreshold = 1.0f; // 1cm 이상 관통 시 보정
        if (DistanceToGround < ActualBallRadius - PenetrationThreshold) // 볼 중심에서 지면까지의 거리가 반지름보다 (임계값만큼) 작을 때
        {
            // 또는 HitResult.Location.Z < (BallLocation.Z - ActualBallRadius + PenetrationThreshold) 와 같이 Z값으로 직접 비교
            UE_LOG(LogTemp, Warning, TEXT("⚠️ Ground penetration detected! Distance=%.2fcm, Radius=%.2fcm. Correcting..."),
                DistanceToGround, ActualBallRadius);

            // ⭐ 수정: 보정 위치에 미세한 여유 추가 (예: 0.1cm)
            float CorrectionOffset = 0.1f; // 볼을 지면 위로 0.1cm 더 띄웁니다.
            FVector CorrectedLocation = HitResult.Location + (HitResult.Normal * (ActualBallRadius + CorrectionOffset));
            SetActorLocation(CorrectedLocation);

            // 바운스 처리 (현재 Ball_Fly 상태에서만 바운스하도록 되어 있으므로 유지)
            if (CurrentBallState == EBallState::Ball_Fly)
            {
                HandleGroundBounce(HitResult);
                SetBallState(EBallState::Ball_Bound); // 바운스 후 Bound 상태로 전환
            }
            // 그 외 (Ball_Bound, Ball_Rolling 상태)에서는 물리 재조정만 필요할 수 있습니다.
            // 필요하다면 이곳에 else if 문으로 상태별 추가 처리를 넣을 수 있습니다.

            UE_LOG(LogTemp, Log, TEXT("✅ Position corrected to: %s  -----------checkGroundPenetion------------00000----"), *CorrectedLocation.ToString());
        }
        // ⭐ 추가: 이미 관통하지 않았지만, 볼이 지면에 매우 가깝다면 상태를 Rolling으로 전환 고려
        else if (CurrentBallState == EBallState::Ball_Fly && GetBallSpeed() < MIN_FLYING_SPEED * 1.2f && DistanceToGround <= ActualBallRadius + 5.0f)
        {
            // 비행 속도가 낮고 지면에 매우 가까우면 Rolling으로 바로 전환 고려 (물론 CheckAutoStateTransitions에서도 처리됨)
            // SetBallState(EBallState::Ball_Rolling); // 이 부분은 CheckAutoStateTransitions 로직과 겹칠 수 있으므로 신중하게 적용
        }
    }
    // ⭐ 추가: LineTrace에 히트하지 않았지만, 볼이 지면 아래로 떨어진다면 (안전망)
    else if (BallLocation.Z < -100000.0f) // 이 부분은 틱 함수의 OB 체크와 중복될 수 있습니다.
    {
        // LineTrace에 히트하지 않았고, 볼이 매우 낮은 Z 좌표 아래로 떨어졌을 때
        UE_LOG(LogTemp, Error, TEXT("❌ Ball fell through world detected via CheckGroundPenetration. Z:%.2f. Triggering OB check."), BallLocation.Z);
        CheckBallOutOfBounds(); // 이 함수는 볼을 리셋 위치로 이동시킵니다.
    }
}

// ⭐ 새로운 함수: 볼이 맵 밖으로 떨어졌는지 체크
void AGolfBall::CheckBallOutOfBounds()
{
    FVector CurrentLocation = GetActorLocation();

    if (CurrentLocation.Z < -1000.0f)
    {
        UE_LOG(LogTemp, Error, TEXT("UE4: Ball fell through world!"));

        // 티 위치로 리셋
        if (AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
        {
            if (GameMode->MapInfo.TeePositions.IsValidIndex(GameMode->CurrentHole - 1))
            {
                FVector TeePos = GameMode->MapInfo.TeePositions[GameMode->CurrentHole - 1];
                FVector ResetPos = TeePos + FVector(0, 0, 50);

                SetActorLocation(ResetPos, false, nullptr, ETeleportType::TeleportPhysics);

                if (BallMesh)
                {
                    BallMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
                    BallMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector, false);
                }

                SetBallState(EBallState::Ball_Ready);
                UE_LOG(LogTemp, Warning, TEXT("UE4: Ball reset to tee"));
            }
        }
    }
}
// ⭐ 새로운 함수: 안전한 물리 활성화
void AGolfBall::EnablePhysicsSafely()
{
    if (!BallMesh) return;

    BallMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    BallMesh->SetCollisionProfileName(TEXT("Custom"));

    // ✅ 임시 PhysMat 생성 제거 → DefaultPhysicalMaterial 복원
    if (IsValid(DefaultPhysicalMaterial))
        BallMesh->SetPhysMaterialOverride(DefaultPhysicalMaterial);

    BallMesh->BodyInstance.bOverrideMass = true;
    BallMesh->BodyInstance.SetMassOverride(CachedMass);
    BallMesh->SetSimulatePhysics(true);
    BallMesh->SetEnableGravity(true);
    BallMesh->SetUseCCD(true);
    BallMesh->WakeRigidBody();

    ValidateAndLogPhysicsState();
}

void AGolfBall::ValidateAndLogPhysicsState()
{
    if (!BallMesh) return;

    bool bSimulating = BallMesh->IsSimulatingPhysics();
    bool bGravity = BallMesh->IsGravityEnabled();
    ECollisionEnabled::Type CollisionType = BallMesh->GetCollisionEnabled();

    bool bValid = bSimulating && bGravity && (CollisionType != ECollisionEnabled::NoCollision);

    if (bValid)
    {
        UE_LOG(LogTemp, Log, TEXT("✅ Physics validation passed"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Physics validation failed: Sim=%s, Grav=%s, Col=%s"),
            bSimulating ? TEXT("OK") : TEXT("FAIL"),
            bGravity ? TEXT("OK") : TEXT("FAIL"),
            *UEnum::GetValueAsString(CollisionType));

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
                TEXT("❌ Physics setup failed"));
        }
    }
}


void AGolfBall::CheckGroundType()
{
    if (CurrentBallState == EBallState::Ball_Fly)
        return;

    if (!LandscapeChecker || !IsValid(LandscapeChecker))
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ LandscapeChecker is invalid"));
        return;
    }

    FVector BallLocation = GetActorLocation();
    // ✅ 캐시 무효화 후 재조회
// 이전 위치의 Rough 캐시가 남아있으면 그린 진입 후에도 Rough 반환
    LandscapeChecker->ClearCache();


    ELandType NewLandType = LandscapeChecker->GetLandTypeAtLocation(BallLocation);

    // Unknown이면 갱신 스킵 (트레이스 실패)
    if (NewLandType == ELandType::Unknown)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("⚠️ CheckGroundType: Unknown → 기존 LandType(%s) 유지"),
            *UEnum::GetValueAsString(CurrentLandType));
        return;
    }

    // LandType 변경된 경우에만 갱신
    if (CheckWasTeeShot() || NewLandType != CurrentLandType)
    {
        UE_LOG(LogTemp, Log,
            TEXT("🌿 LandType 변경: %s → %s"),
            *UEnum::GetValueAsString(CurrentLandType),
            *UEnum::GetValueAsString(NewLandType));

        // ✅ 1. CurrentLandType 확정 설정
        CurrentLandType = NewLandType;
        CurrentLandProperties = LandscapeChecker->GetLandPropertiesAtLocation(BallLocation);

        // ✅ 2. 물리 적용 (CurrentLandType 기준)
        ApplyLandTypePhysics();

        // ✅ 3. UI 갱신 (CurrentLandType 기준, 재조회 없음)
        UpdateCurrentLandType();

        // ✅ 4. GameMode별 추가 UI 갱신
        if (GM && (GM->CurrentGameMode == EGolfGameMode::StrokeMode ||
            GM->CurrentGameMode == EGolfGameMode::TrainingMode))
        {
            if (GM->StrokeWidgetInstance)
                GM->StrokeWidgetInstance->SetLandType((int32)CurrentLandType);
        }
    }
}

void AGolfBall::UpdateCurrentLandType()
{
    // ✅ 내부에서 LandscapeChecker 재조회 완전 제거
      //    CheckGroundType()이 이미 CurrentLandType을 설정해줬음
      //    여기선 CurrentLandType 기준으로 UI/물리만 적용

    if (!GM) return;

    // Unknown이면 UI 갱신 스킵 (10번 표시 방지)
    if (CurrentLandType == ELandType::Unknown)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("⚠️ UpdateCurrentLandType: CurrentLandType=Unknown → UI 갱신 스킵"));
        return;
    }

    if (GM->StrokeWidgetInstance)
    {
        int32 LandTypeNum = (int32)CurrentLandType;
        GM->StrokeWidgetInstance->SetLandType(LandTypeNum);

        if (CurrentLandType == ELandType::Sand)
            GM->StrokeWidgetInstance->SetPercentText(30.f);
        else
            GM->StrokeWidgetInstance->SetPercentText(0.f);

        UE_LOG(LogTemp, Log,
            TEXT("UpdateCurrentLandType() : Landtype is %d (%s)"),
            LandTypeNum,
            *UEnum::GetValueAsString(CurrentLandType));
    }
}
void AGolfBall::ApplyLandTypePhysics()
{
    if (!BallMesh) return;

    FHitResult HitResult;
    if (!PerformLineTrace(HitResult)) return;

    // ✅ ResolveFromHit으로 확실하게 PhysMat 획득
    UPhysicalMaterial* PhysMaterial = PhysMatResolveUtil::ResolveFromHit(
        HitResult, HitResult.GetComponent());

    if (PhysMaterial)
    {
        //SetFrictionWeight(PhysMaterial->Friction);
        FrictionWeight = FMath::Clamp(PhysMaterial->Friction, 0.0f, 2.0f);

        // ✅ 지형 이름으로 전체 물리 설정 적용
        FString TerrainName = GetTerrainNameFromPhysicalMaterial(PhysMaterial);
        ApplyTerrainPhysicsSettings(TerrainName, PhysMaterial);

        UE_LOG(LogTemp, Log, TEXT("🌿 ApplyLandTypePhysics: PhysMat=%s → Terrain=%s, Friction=%.3f"),
            *PhysMaterial->GetName(), *TerrainName, PhysMaterial->Friction);
    }
}

ELandType AGolfBall::GetCurrentLandType()
{
    // ✅ 매번 LandscapeChecker 재조회하지 않고 캐시된 값 반환
    return CurrentLandType;
}

void AGolfBall::SkipTurnTransitionCountdown()
{
    if (TurnTransitionCountdown > 0.0f)
    {
        UE_LOG(LogTemp, Log, TEXT("⏭️ Turn transition countdown skipped"));

        // 모든 타이머 클리어
        GetWorld()->GetTimerManager().ClearTimer(CountdownUpdateTimer);
        GetWorld()->GetTimerManager().ClearTimer(ResetReadyTimer);

        // 즉시 다음 턴으로
        ResetToReady();
    }
}



void AGolfBall::SetBallVisibility(bool bVisible, bool bAlsoSetCollision)
{
    if (!BallMesh || !IsValid(BallMesh))
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ BallMesh not valid in SetBallVisibility"));
        return;
    }

    if (bBallForceHidden && bVisible)
    {
        UE_LOG(LogTemp, Log, TEXT("🚫 Ball visibility blocked - force hidden"));
        return;
    }

    bBallCurrentlyVisible = bVisible;
    BallMesh->SetVisibility(bVisible);

    if (bAlsoSetCollision)
    {
        // ⭐ 수정: 물리 시뮬레이션 중이면 충돌 유지
        if (CurrentPhysicsState == EPhysicsState::Simulating)
        {
            UE_LOG(LogTemp, Log, TEXT("🚀 Physics simulating - maintaining collision"));
            return; // 충돌 상태 변경하지 않음
        }

        // 안전하게 충돌 상태 변경
        if (bVisible)
        {
            SetPhysicsState(EPhysicsState::Static);
        }
        else
        {
            SetPhysicsState(EPhysicsState::Disabled);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("👁️ Ball visibility: %s (Physics: %s)"),
        bVisible ? TEXT("ON") : TEXT("OFF"),
        *BallMesh->GetOwner()->GetName());
}

bool AGolfBall::IsBallVisible() const
{
    return bBallCurrentlyVisible && !bBallForceHidden;
}

void AGolfBall::SetBallCollisionEnabled(bool bEnabled)
{
    if (!BallMesh || !IsValid(BallMesh))
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ BallMesh not valid in SetBallCollisionEnabled"));
        return;
    }

    bool bIsSimulatingPhysics = BallMesh->IsSimulatingPhysics();

    if (bEnabled)
    {
        BallMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        BallMesh->SetCollisionProfileName(OriginalCollisionProfileName);
        bBallCollisionEnabled = true;
    }
    else
    {
        if (bIsSimulatingPhysics)
        {
            UE_LOG(LogTemp, Warning, TEXT("⚠️ Cannot disable collision while physics active"));
            BallMesh->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
        }
        else
        {
            BallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
        bBallCollisionEnabled = false;
    }

    // ⭐ 실제 사용: 설정 후 검증
    ValidateAndLogPhysicsState();
}

bool AGolfBall::IsBallCollisionEnabled() const
{
    return bBallCollisionEnabled;
}


void AGolfBall::ShowBallForTeeShot()
{
    UE_LOG(LogTemp, Log, TEXT("🏌️ Showing ball for tee shot"));

    // 강제 숨김 해제
    bBallForceHidden = false;

    SetActorRotation(FRotator::ZeroRotator);
    // 볼 보이기
    SetBallVisibility(true);

}

void AGolfBall::HideBallAfterHoleIn()
{
    UE_LOG(LogTemp, Log, TEXT("🏆 Hiding ball after hole-in"));

    // 강제 숨김 설정 (다음 티샷까지 보이지 않음)
    bBallForceHidden = true;

    // 볼 숨기기
    SetBallVisibility(false);
}

void AGolfBall::UpdateVisibilityBasedOnState()
{
    bool bShouldBeVisible = ShouldBallBeVisible(CurrentBallState);

    UE_LOG(LogTemp, VeryVerbose, TEXT("🔍 Updating visibility: State=%s, ShouldBeVisible=%s"),
        *UEnum::GetValueAsString(CurrentBallState),
        bShouldBeVisible ? TEXT("True") : TEXT("False"));

    SetBallVisibility(bShouldBeVisible);
}

bool AGolfBall::ShouldBallBeVisible(EBallState State) const
{
    // 강제 숨김 상태라면 항상 숨김
    if (bBallForceHidden)
        return false;

    // 상태별 가시성 규칙
    switch (State)
    {
    case EBallState::Ball_Init:
        return false; // 초기화 중에는 숨김

    case EBallState::Ball_Ready:
        return true;  // 준비 상태에서는 보임 (티샷 준비)

    case EBallState::Ball_Fly:
        return true;  // 비행 중에는 보임

    case EBallState::Ball_Bound:
        return true;  // 바운스/굴림 중에는 보임

    case EBallState::Ball_Rolling:
        return true;  // 굴림 중에는 보임

    case EBallState::Ball_Stop:
        return true;  // 정지 상태에서는 보임 (홀인 체크는 별도 처리)

    default:
        return false; // 알 수 없는 상태에서는 숨김
    }
}

void AGolfBall::HandleVisibilityOnStateChange(EBallState PreviousState, EBallState NewState)
{
    UE_LOG(LogTemp, Log, TEXT("🔄 Handling visibility on state change: %s → %s"),
        *UEnum::GetValueAsString(PreviousState),
        *UEnum::GetValueAsString(NewState));

    // 상태별 특별 처리
    switch (NewState)
    {
    case EBallState::Ball_Ready:
        if (PreviousState == EBallState::Ball_Init)
        {
            // 티샷 준비 - 볼 보이기
            ShowBallForTeeShot();
        }
        break;

    case EBallState::Ball_Init:
        // 초기화 상태로 돌아가면 볼 숨기기 (새 홀 시작 등)
        bBallForceHidden = true; // 강제 숨김 해제
        SetBallVisibility(false);

        break;

    default:
        // 일반적인 상태 변경에서는 규칙에 따라 가시성 결정
        UpdateVisibilityBasedOnState();
        break;
    }
}

// ===== 새 홀 시작 시 볼 가시성 리셋 함수 =====
void AGolfBall::ResetForNewHole()
{
    UE_LOG(LogTemp, Log, TEXT("🆕 Resetting ball for new hole"));

    // 강제 숨김 해제
    bBallForceHidden = false;

    // 초기 상태로 설정 (숨김)
    SetBallState(EBallState::Ball_Init);
    SetBallVisibility(false);

    // 물리 초기화
    if (BallMesh)
    {
        BallMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
        BallMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    }

    UE_LOG(LogTemp, Log, TEXT("✅ Ball reset complete - ready for new hole"));
}

void AGolfBall::PrepareForTeeShot()
{
    UE_LOG(LogTemp, Log, TEXT("🏌️ Preparing ball for tee shot"));

    // 1. 강제 숨김 해제
    bBallForceHidden = false;

    // 2. Ready 상태로 설정 (자동으로 볼이 보임)
    SetBallState(EBallState::Ball_Ready);

    // 3. 볼 위치 조정 (지면에 정확히 배치)
    AdjustBallToGroundLevel();

    // 4. 물리 초기화
    if (BallMesh)
    {
        BallMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
        BallMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

    }

    UE_LOG(LogTemp, Log, TEXT("✅ Ball prepared for tee shot"));
}

void AGolfBall::SaveOriginalCollisionSettings()
{
    // 볼의 원래 콜리젼 설정을 저장
    OriginalCollisionType = BallMesh->GetCollisionEnabled();
    OriginalCollisionProfileName = BallMesh->GetCollisionProfileName();
}
void AGolfBall::RestoreOriginalCollisionSettings()
{
    // 저장된 원래 콜리젼 설정을 복원
    BallMesh->SetCollisionEnabled(static_cast<ECollisionEnabled::Type>(OriginalCollisionType));
    BallMesh->SetCollisionProfileName(OriginalCollisionProfileName);
}

void AGolfBall::UpdateCollisionBasedOnVisibility()
{
    // 가시성 상태에 따라 콜리젼 자동 업데이트
    bool bShouldHaveCollision = IsBallVisible();
    SetBallCollisionEnabled(bShouldHaveCollision);
}

// 새로운 함수 추가
void AGolfBall::ForceEnableCollisionForShot()
{
    if (!BallMesh) return;

    // 샷 실행 중에는 반드시 충돌 활성화
    bBallCollisionEnabled = true;
    BallMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    BallMesh->SetCollisionProfileName(OriginalCollisionProfileName);

    UE_LOG(LogTemp, Log, TEXT("🏌️ Collision forced ON for shot execution"));
}

// GolfBall.cpp에 추가
void AGolfBall::LogPhysicsState(const FString& Context)
{
    if (!BallMesh) return;

    UE_LOG(LogTemp, Log, TEXT("🔍 Physics State [%s]: Simulate=%s, Collision=%s, Gravity=%s, State=%s"),
        *Context,
        BallMesh->IsSimulatingPhysics() ? TEXT("ON") : TEXT("OFF"),
        *UEnum::GetValueAsString(BallMesh->GetCollisionEnabled()),
        BallMesh->IsGravityEnabled() ? TEXT("ON") : TEXT("OFF"),
        *UEnum::GetValueAsString(CurrentBallState));
}






void AGolfBall::SetPhysicsAndCollisionSafely(bool bEnablePhysics, bool bEnableCollision)
{
    if (!BallMesh || !IsValid(BallMesh))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ BallMesh invalid in SetPhysicsAndCollisionSafely"));
        return;
    }

    // 재귀 호출 방지
    if (bIsChangingPhysicsState)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Physics state change already in progress, skipping"));
        return;
    }

    bIsChangingPhysicsState = true;

    UE_LOG(LogTemp, Log, TEXT("🔧 Setting Physics=%s, Collision=%s"),
        bEnablePhysics ? TEXT("ON") : TEXT("OFF"),
        bEnableCollision ? TEXT("ON") : TEXT("OFF"));




    try
    {
        // ⭐ 핵심: 올바른 순서로 설정
        if (bEnablePhysics && bEnableCollision)
        {
            // 활성화: 충돌 먼저, 물리 나중에
            BallMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            BallMesh->SetCollisionProfileName(OriginalCollisionProfileName);

            // 짧은 지연 후 물리 활성화 (언리얼 엔진 내부 처리 시간 확보)
           // BallMesh->SetSimulatePhysics(false); // 일단 끄고
            GetWorld()->GetTimerManager().SetTimerForNextTick([this]() {
                if (BallMesh && IsValid(BallMesh))
                {
                    BallMesh->SetSimulatePhysics(true);
                    BallMesh->SetEnableGravity(true);
                    BallMesh->SetUseCCD(true);
                    BallMesh->WakeRigidBody();

                    UE_LOG(LogTemp, Log, TEXT("✅ Physics activated with delay"));
                    LogCurrentPhysicsState(TEXT("After Delayed Activation"));
                }
                bIsChangingPhysicsState = false;
                });
            return; // 여기서 리턴하여 아래 코드 실행 방지
        }
        else if (!bEnablePhysics && !bEnableCollision)
        {
            // 비활성화: 물리 먼저, 충돌 나중에
            BallMesh->SetSimulatePhysics(false);
            BallMesh->SetEnableGravity(false);
            BallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
        else if (!bEnablePhysics && bEnableCollision)
        {
            // 물리만 끄고 충돌은 유지 (Ready 상태)

            BallMesh->SetSimulatePhysics(false);
            BallMesh->SetEnableGravity(false);
            BallMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        }
        else // bEnablePhysics && !bEnableCollision
        {
            // ⭐ 이 조합은 에러 원인! 물리만 켜고 충돌 끄기 불가
            UE_LOG(LogTemp, Error, TEXT("❌ Invalid combination: Physics=ON, Collision=OFF. Using PhysicsOnly instead."));
            BallMesh->SetSimulatePhysics(true);
            BallMesh->SetEnableGravity(true);
            BallMesh->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly); // NoCollision 대신 PhysicsOnly
        }
    }
    catch (...)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Exception in SetPhysicsAndCollisionSafely"));
    }

    bIsChangingPhysicsState = false;
    LogCurrentPhysicsState(TEXT("After SetPhysicsAndCollisionSafely"));
}

void AGolfBall::LogCurrentPhysicsState(const FString& Context)
{
    if (!BallMesh) return;

    bool bSimulating = BallMesh->IsSimulatingPhysics();
    bool bGravity = BallMesh->IsGravityEnabled();
    ECollisionEnabled::Type CollisionType = BallMesh->GetCollisionEnabled();

    UE_LOG(LogTemp, Log, TEXT("📊 [%s] Physics: %s, Gravity: %s, Collision: %s, State: %s"),
        *Context,
        bSimulating ? TEXT("✅") : TEXT("❌"),
        bGravity ? TEXT("✅") : TEXT("❌"),
        *UEnum::GetValueAsString(CollisionType),
        *UEnum::GetValueAsString(CurrentBallState));

    // 에러 조합 감지
    if (bSimulating && CollisionType == ECollisionEnabled::NoCollision)
    {
        UE_LOG(LogTemp, Error, TEXT("🚨 DETECTED ERROR COMBINATION: Physics=ON but Collision=NoCollision"));

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
                TEXT("🚨 Physics/Collision Error Detected!"));
        }
    }
}

void AGolfBall::ValidateAndFixPhysicsState()
{
    if (!BallMesh || bIsChangingPhysicsState) return;

    bool bActualSimulating = BallMesh->IsSimulatingPhysics();
    ECollisionEnabled::Type ActualCollision = BallMesh->GetCollisionEnabled();

    // 에러 조합 감지
    bool bHasError = (bActualSimulating && ActualCollision == ECollisionEnabled::NoCollision);

    if (bHasError)
    {
        UE_LOG(LogTemp, Error, TEXT("🚨 DETECTED PHYSICS ERROR - Auto fixing..."));

        // 에러 상황에 따른 자동 복구
        if (CurrentBallState == EBallState::Ball_Fly || CurrentBallState == EBallState::Ball_Bound)
        {
            // 움직이는 중이면 시뮬레이션 상태로 복구
            SetPhysicsState(EPhysicsState::Simulating);
        }
        else if (CurrentBallState == EBallState::Ball_Ready)
        {
            // 준비 상태면 정적 상태로 복구
            SetPhysicsState(EPhysicsState::Static);
        }
        else
        {
            // 기타 상태면 비활성화
            SetPhysicsState(EPhysicsState::Disabled);
        }

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
                TEXT("🔧 Physics error auto-fixed"));
        }
    }
}


void AGolfBall::SetBallMassSafely(float NewMass)
{
    if (!BallMesh || !IsValid(BallMesh))
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ BallMesh invalid, only caching mass: %.3fkg"), NewMass);
        CachedMass = NewMass;
        return;
    }

    // 질량 값 검증
    float ValidMass = FMath::Clamp(NewMass, 0.001f, 10.0f); // 1g ~ 10kg 범위
    CachedMass = ValidMass;

    // 질량 설정 (물리 상태 관계없이 설정 가능)
    //BallMesh->SetMassOverrideInKg(NAME_None, ValidMass);
    //BallMesh->BodyInstance.SetMassOverride(ValidMass);
    BallMesh->BodyInstance.bOverrideMass = true;
    BallMesh->BodyInstance.SetMassOverride(ValidMass);
    bMassValidated = true;

    UE_LOG(LogTemp, Log, TEXT("✅ Ball mass set safely: %.3fkg (Physics: %s)"),
        ValidMass, BallMesh->IsSimulatingPhysics() ? TEXT("ON") : TEXT("OFF"));
}


void AGolfBall::LogShotDebugInfo() const
{
    if (!BallMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ BallMesh is null"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("=== Shot Debug Info ==="));
    UE_LOG(LogTemp, Warning, TEXT("Ball State: %s"), *UEnum::GetValueAsString(CurrentBallState));
    UE_LOG(LogTemp, Warning, TEXT("Physics State: %s"), *UEnum::GetValueAsString(CurrentPhysicsState));
    UE_LOG(LogTemp, Warning, TEXT("Is Simulating Physics: %s"), BallMesh->IsSimulatingPhysics() ? TEXT("✅ YES") : TEXT("❌ NO"));
    UE_LOG(LogTemp, Warning, TEXT("Has Pending Shot: %s"), bHasPendingShot ? TEXT("✅ YES") : TEXT("❌ NO"));

    FVector CurrentVelocity = BallMesh->IsSimulatingPhysics() ? BallMesh->GetPhysicsLinearVelocity() : FVector::ZeroVector;
    float CurrentSpeed = CurrentVelocity.Size();

    UE_LOG(LogTemp, Warning, TEXT("Physics Velocity: %s (%.1f cm/s)"), *CurrentVelocity.ToString(), CurrentSpeed);
    UE_LOG(LogTemp, Warning, TEXT("Last Valid Velocity: %s (%.1f cm/s)"), *LastValidVelocity.ToString(), LastValidVelocity.Size());

    if (bHasPendingShot)
    {
        UE_LOG(LogTemp, Warning, TEXT("Pending Shot Velocity: %s (%.1f cm/s)"), *PendingShotVelocity.ToString(), PendingShotVelocity.Size());
        UE_LOG(LogTemp, Warning, TEXT("Pending Shot Speed: %.1f m/s"), PendingShotSpeed);
    }

    UE_LOG(LogTemp, Warning, TEXT("GetBallSpeed() returns: %.1f cm/s"), GetBallSpeed());
    UE_LOG(LogTemp, Warning, TEXT("==================="));

#if WITH_EDITOR

    // 화면에도 표시
    if (GEngine)
    {
        FString DebugText = FString::Printf(
            TEXT("🔍 Ball Debug\nState: %s\nPhysics: %s\nSpeed: %.1f cm/s\nPending: %s"),
            *UEnum::GetValueAsString(CurrentBallState).Right(6),
            BallMesh->IsSimulatingPhysics() ? TEXT("ON") : TEXT("OFF"),
            GetBallSpeed(),
            bHasPendingShot ? TEXT("YES") : TEXT("NO")
        );

        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, DebugText);
    }
#endif
}



float AGolfBall::GetCurrentSpeedDebug() const
{
    if (!BallMesh || !IsValid(BallMesh))
        return -1.0f; // BallMesh 문제

    if (!BallMesh->IsSimulatingPhysics())
    {
        if (bHasPendingShot)
            return PendingShotVelocity.Size(); // 대기 중인 속도
        else
            return -2.0f; // 물리 비활성화
    }

    return BallMesh->GetPhysicsLinearVelocity().Size(); // 실제 물리 속도
}

// ⭐ 새로 추가: Trail 업데이트 함수
void AGolfBall::UpdateBallTrail(float DeltaTime)
{
    float CurrentTime = GetWorld()->GetTimeSeconds();
    float CurrentSpeed = GetBallSpeed();

    // 최소 속도 이상일 때만 트레일 포인트 추가
    if (CurrentSpeed >= TrailSettings.MinSpeedForTrail &&
        CurrentTime - LastTrailUpdateTime >= TrailSettings.TrailUpdateInterval)
    {
        AddTrailPoint();
        LastTrailUpdateTime = CurrentTime;
    }

    // 오래된 포인트들 제거 및 페이드 업데이트
    CleanupOldTrailPoints();
}

// ⭐ 새로 추가: Trail 포인트 추가
void AGolfBall::AddTrailPoint()
{
    // 최대 포인트 수 제한
    if (TrailPoints.Num() >= TrailSettings.MaxTrailPoints)
    {
        TrailPoints.RemoveAt(0);
    }

    FVector CurrentPosition = GetActorLocation();
    float CurrentSpeed = GetBallSpeed();
    float CurrentTime = GetWorld()->GetTimeSeconds();

    // 새 트레일 포인트 추가
    FTrailPoint NewPoint(CurrentPosition, CurrentSpeed, CurrentTime, 1.0f);
    TrailPoints.Add(NewPoint);

    UE_LOG(LogTemp, VeryVerbose, TEXT("🌟 Trail point added: Pos=%s, Speed=%.1f"),
        *CurrentPosition.ToString(), CurrentSpeed);
}

// ⭐ 새로 추가: 오래된 포인트 정리 및 페이드 업데이트
void AGolfBall::CleanupOldTrailPoints()
{
    if (TrailPoints.Num() == 0) return;

    float CurrentTime = GetWorld()->GetTimeSeconds();
    float CutoffTime = CurrentTime - TrailSettings.TrailDuration;

    // 페이드 효과 업데이트 및 오래된 포인트 제거
    for (int32 i = TrailPoints.Num() - 1; i >= 0; i--)
    {
        FTrailPoint& Point = TrailPoints[i];

        if (Point.TimeStamp < CutoffTime)
        {
            // 완전히 오래된 포인트 제거
            TrailPoints.RemoveAt(i);
        }
        else if (TrailSettings.bUseFadeEffect)
        {
            // 페이드 효과 계산:
            // 최신 포인트(Age=0) → Alpha 0.3
            // 오래된 포인트(Age=Duration) → Alpha 0.0 으로 선형 감소
            float Age = CurrentTime - Point.TimeStamp;
            float FadeRatio = 1.0f - (Age / TrailSettings.TrailDuration); // 1.0 ~ 0.0
            Point.Alpha = FMath::Clamp(FadeRatio * 0.3f, 0.0f, 0.3f);   // 0.3 ~ 0.0
        }
    }
}

// ⭐ 새로 추가: Trail 그리기 (실시간 렌더링)
void AGolfBall::DrawBallTrail() const
{
    if (!GetWorld() || TrailPoints.Num() < 2) return;

    // 연속된 포인트들을 라인으로 연결
    for (int32 i = 0; i < TrailPoints.Num() - 1; i++)
    {
        const FTrailPoint& CurrentPoint = TrailPoints[i];
        const FTrailPoint& NextPoint = TrailPoints[i + 1];

        // 투명도가 거의 0이면 스킵
        if (CurrentPoint.Alpha < 0.05f && NextPoint.Alpha < 0.05f)
            continue;

        // 색상 계산
        FLinearColor LineColor = TrailSettings.TrailBaseColor;

        if (TrailSettings.bUseSpeedBasedColors)
        {
            LineColor = GetTrailColorForSpeed(CurrentPoint.Speed);
        }

        // 페이드 효과 적용
        float AvgAlpha = (CurrentPoint.Alpha + NextPoint.Alpha) * 0.5f;
        LineColor = GetTrailColorWithFade(LineColor, AvgAlpha);

        // 라인 두께 계산 (페이드에 따라 두께도 변화)
        float LineThickness = TrailSettings.TrailThickness * AvgAlpha;

        // 라인 그리기
        DrawDebugLine(
            GetWorld(),
            CurrentPoint.Position,
            NextPoint.Position,
            LineColor.ToFColor(true),
            false,
            -1.0f, // 지속 시간 (매 프레임 다시 그리므로 -1)
            0,
            LineThickness
        );
    }

    // 현재 볼 위치와 마지막 트레일 포인트 연결
    if (TrailPoints.Num() > 0)
    {
        const FTrailPoint& LastPoint = TrailPoints.Last();
        FVector CurrentPosition = GetActorLocation();

        if (!LastPoint.Position.Equals(CurrentPosition, 5.0f))
        {
            FLinearColor CurrentColor = TrailSettings.bUseSpeedBasedColors ?
                GetTrailColorForSpeed(GetBallSpeed()) : TrailSettings.TrailBaseColor;

            // 볼 근처 구간도 동일하게 Alpha 0.3 적용
            CurrentColor = GetTrailColorWithFade(CurrentColor, 0.3f);

            DrawDebugLine(
                GetWorld(),
                LastPoint.Position,
                CurrentPosition,
                CurrentColor.ToFColor(true),
                false,
                -1.0f,
                0,
                TrailSettings.TrailThickness * 0.5f
            );
        }
    }
}

// ⭐ 새로 추가: 속도별 색상 계산
FLinearColor AGolfBall::GetTrailColorForSpeed(float Speed) const
{
    float SpeedMS = Speed / 100.0f; // cm/s -> m/s
    return FLinearColor::Red;
    /*
    // 속도별 색상 그라데이션
    if (SpeedMS < 2.0f)
        return FLinearColor::Blue;      // 느림 - 파랑
    else if (SpeedMS < 5.0f)
        return FLinearColor::Green;     // 보통 - 초록
    else if (SpeedMS < 10.0f)
        return FLinearColor::Yellow;    // 빠름 - 노랑
    else if (SpeedMS < 15.0f)
        return FLinearColor::Gray;    // 매우 빠름 - 주황
    else if (SpeedMS < 20.0f)
        return FLinearColor::Red;       // 아주 빠름 - 빨강
    else
        return FLinearColor::Black;   // 극한 속도 - 마젠타
        */
}

// ⭐ 새로 추가: 페이드 효과가 적용된 색상 계산
FLinearColor AGolfBall::GetTrailColorWithFade(const FLinearColor& BaseColor, float Alpha) const
{
    FLinearColor FadedColor = BaseColor;
    FadedColor.A = Alpha; // 투명도 적용

    // 페이드될 때 색상도 살짝 어둡게
    if (Alpha < 1.0f)
    {
        float DarkenFactor = 0.5f + (Alpha * 0.5f); // 30% ~ 100% 밝기
        FadedColor.R *= DarkenFactor;
        FadedColor.G *= DarkenFactor;
        FadedColor.B *= DarkenFactor;
    }

    return FadedColor;
}

// ⭐ 새로 추가: Trail 제어 함수들
void AGolfBall::SetTrailVisible(bool bVisible)
{
    TrailSettings.bShowTrail = bVisible;

    if (!bVisible)
    {
        ClearTrail();
    }

    UE_LOG(LogTemp, Log, TEXT("🌟 Ball trail visibility: %s"), bVisible ? TEXT("ON") : TEXT("OFF"));

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan,
            FString::Printf(TEXT("🌟 Trail: %s"), bVisible ? TEXT("ON") : TEXT("OFF")));
    }
}
void AGolfBall::ClearTrail()
{
    TrailPoints.Empty();
    LastTrailUpdateTime = 0.0f;

    UE_LOG(LogTemp, Log, TEXT("🧹 Ball trail cleared"));
}

void AGolfBall::SetTrailSettings(const FBallTrailSettings& NewSettings)
{
    TrailSettings = NewSettings;

    // 최대 포인트 수가 줄어들었다면 기존 포인트들 정리
    if (TrailPoints.Num() > TrailSettings.MaxTrailPoints)
    {
        int32 ExcessPoints = TrailPoints.Num() - TrailSettings.MaxTrailPoints;
        TrailPoints.RemoveAt(0, ExcessPoints);
    }

    UE_LOG(LogTemp, Log, TEXT("🎨 Trail settings updated: Duration=%.1fs, Thickness=%.1f"),
        TrailSettings.TrailDuration, TrailSettings.TrailThickness);
}

bool AGolfBall::CheckHoleIn()
{
    if (bIsHoleIn)
    {
        return true;
    }

    if (!GetWorld())
    {
        return false;
    }

    //0. 볼이 홀컵 overlap되면, bIsHoleIn을 true로 바꾸면 댐

    //AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    //if (!GameMode || !GameMode->MapInfo.HolecupPositions.IsValidIndex(GameMode->CurrentHole - 1))
    //{
    //    UE_LOG(LogTemp, Error, TEXT("❌ Invalid GameMode or Holecup position in CheckHoleIn"));
    //    return false;
    //}

    //FVector HolecupPos = GameMode->MapInfo.HolecupPositions[GameMode->CurrentHole - 1];
    //FVector BallPos = GetActorLocation();
    //float Distance = FVector::Dist(BallPos, HolecupPos);

    //// 홀컵 반경 내 (예: 10cm)로 설정
    //bool bHoleIn = Distance <= 10.0f;
    //if (bHoleIn)
    //{
    //    bIsHoleIn = true;
    //    UE_LOG(LogTemp, Log, TEXT("🏆 Ball %s is in the hole! Distance=%.1f cm"), *GetName(), Distance);
    //}

    return bIsHoleIn;
}

bool AGolfBall::CheckConcedeDistance() const
{
    if (bIsConceded || bIsHoleIn)
    {
        return true;
    }

    if (!GetWorld())
    {
        return false;
    }

    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    if (!GameMode || !GameMode->MapInfo.HolecupPositions.IsValidIndex(GameMode->CurrentHole - 1))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Invalid GameMode or Holecup position in CheckConcedeDistance"));
        return false;
    }

    FVector HolecupPos = GameMode->MapInfo.HolecupPositions[GameMode->CurrentHole - 1];
    FVector BallPos = GetActorLocation();
    float Distance = FVector::Dist(BallPos, HolecupPos);

    float ConcedeDistanceOption = GameMode->GameInfo.GameOptions.Concede_Distance;
    float DistanceOfConcede = ConcedeDistanceOption <= 0 ? 0 : 100 + (ConcedeDistanceOption - 1.0f) * 50.f;

    bool bConceded = Distance <= DistanceOfConcede;
    if (bConceded)
    {
        UE_LOG(LogTemp, Log, TEXT("👍 Ball %s is within concede distance: %.1f cm"), *GetName(), Distance);
    }

    return bConceded;
}


void AGolfBall::SetHoleIn(bool bHoleIn)
{
    bIsHoleIn = bHoleIn;
    if (bIsHoleIn)
    {
        bIsConceded = false; // 홀인이면 컨시드 무효
        UE_LOG(LogTemp, Log, TEXT("🏆 Ball %s set to HoleIn"), *GetName());
    }
}


void AGolfBall::SetConceded(bool bConceded)
{
    if (!bIsHoleIn) // 홀인이 아니어야 컨시드 가능
    {
        bIsConceded = bConceded;
        if (bIsConceded)
        {
            IncrementOwningPlayerShotCount();
            UE_LOG(LogTemp, Log, TEXT("👍 Ball %s set to Conceded"), *GetName());
        }
    }
}


// GolfBall.cpp (TriggerPlayerResultProcessing 함수 내부)
// ⭐ 수정: bIsConcededResult 매개변수 추가 및 전달
//void AGolfBall::TriggerPlayerResultProcessing(bool bHoleIn, bool bOutOfBounds, bool bIsConcededResult)
//{
//    if (AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
//    {
//        if (GameMode->PlayerManager)
//        {
//            int32 PlayerIndex = FindOwnerPlayerIndex(GameMode->PlayerManager);
//            if (PlayerIndex != -1)
//            {
//                TArray<AGolfPlayer*> Players = GameMode->PlayerManager->GetPlayers();
//                if (Players.IsValidIndex(PlayerIndex))
//                {
//                    AGolfPlayer* Player = Players[PlayerIndex];
//                    if (Player)
//                    {
//                        Player->UpdateBallPosition(GetActorLocation());
//                        // ⭐ 수정: bIsConcededResult 매개변수 추가
//                        Player->ProcessShotResult(bHoleIn, bOutOfBounds, bIsConcededResult);
//                        UE_LOG(LogTemp, Log, TEXT("✅ Result processing delegated to Player %d"), PlayerIndex);
//                    }
//                }
//            }
//        }
//    }
//}

// 기존 SetBallState를 InternalSetBallState로 이름 변경하여 명확성 확보
void AGolfBall::InternalSetBallState(EBallState NewState, bool bForceUpdate)
{
    if (bIsBeingDestroyed || !IsValid(this))
    {
        return;
    }

    if (CurrentBallState == NewState && !bForceUpdate)
        return;


    if (NewState == EBallState::Ball_Stop || NewState == EBallState::Ball_Ready)
        HideCrosshair();


    EBallState PreviousState = CurrentBallState;
    CurrentBallState = NewState; // 실제 볼 상태 변경
    OnBallStateChangedInternal.Broadcast(NewState, OwningPlayerIndex); // 내부 델리게이트 브로드캐스트
    ApplyStatePhysics(NewState);
    HandleStateTransition(PreviousState, NewState);
    NotifyCameraStateChange(PreviousState, NewState); // CameraManager는 여전히 내부 볼 상태 변경을 직접 수신

    UE_LOG(LogTemp, Log, TEXT("🎾 Ball state: %s → %s"),
        *UEnum::GetValueAsString(PreviousState),
        *UEnum::GetValueAsString(NewState));
}


bool AGolfBall::PerformLineTrace(FHitResult& OutHit)
{
    UWorld* World = GetWorld();
    if (!World) return false;

    FVector Start = GetActorLocation();
    FVector End = Start - FVector(0, 0, 40.0f);

    FCollisionQueryParams Params(SCENE_QUERY_STAT(BallGroundTrace), true);
    Params.AddIgnoredActor(this);
    Params.bReturnPhysicalMaterial = true;  // ✅ 핵심 수정

    return World->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, Params);
}

// ⭐ 새로 추가: 소유 플레이어의 샷 카운트를 증가시키는 함수 구현
void AGolfBall::IncrementOwningPlayerShotCount()
{
    // 현재 월드의 GameMode를 가져옵니다.
    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    if (!GameMode)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ IncrementOwningPlayerShotCount: GameMode is null."));
        return;
    }

    // GameMode에서 PlayerManager를 가져옵니다.
    UGolfPlayerManager* PlayerManager = GameMode->PlayerManager;
    if (!PlayerManager)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ IncrementOwningPlayerShotCount: PlayerManager is null."));
        return;
    }

    // OwningPlayerIndex를 사용하여 해당 플레이어를 찾습니다.
    if (PlayerManager->GetPlayers().IsValidIndex(OwningPlayerIndex))
    {
        AGolfPlayer* OwningPlayer = PlayerManager->GetPlayers()[OwningPlayerIndex];
        if (IsValid(OwningPlayer))
        {
            // 플레이어의 샷 카운트 증가 함수를 호출합니다.
            OwningPlayer->IncrementShotCount();
            UE_LOG(LogTemp, Log, TEXT("✅ Player %d's shot count incremented by GolfBall."), OwningPlayerIndex);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("❌ IncrementOwningPlayerShotCount: Owning Player %d is invalid."), OwningPlayerIndex);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ IncrementOwningPlayerShotCount: Owning Player Index %d is invalid."), OwningPlayerIndex);
    }
}

// ⭐ 새로 추가: GetBallColor 함수 구현
FLinearColor AGolfBall::GetBallColor() const
{
    return CurrentBallColor;
}


void AGolfBall::SafeHandleBounce(const FHitResult& Hit)
{
    if (!BallMesh || !IsValid(BallMesh) || bIsBeingDestroyed)
    {
        return;
    }

    // ✅ Bound/Fly 상태에서만 반사 처리 — 다른 모든 전환 경로에도 동일 적용
    if (CurrentBallState != EBallState::Ball_Bound &&
        CurrentBallState != EBallState::Ball_Fly)
    {
        UE_LOG(LogTemp, Verbose,
            TEXT("⏭️ SafeHandleBounce 스킵: 상태=%s"),
            *UEnum::GetValueAsString(CurrentBallState));
        return;
    }

    try
    {
        FVector CurrentVelocity = BallMesh->GetPhysicsLinearVelocity();
        float IncomingSpeed = CurrentVelocity.Size();

        // 안전성 체크
        if (CurrentVelocity.ContainsNaN() || FMath::IsNaN(IncomingSpeed))
        {
            UE_LOG(LogTemp, Error, TEXT("❌ SafeHandleBounce: Invalid velocity data"));
            return;
        }

        // 🏀 바운스 처리 (기존 로직과 동일하지만 안전하게)
        if (IncomingSpeed < 25.0f) // 0.25m/s 이하
        {
            FVector HorizontalVelocity = FVector(CurrentVelocity.X, CurrentVelocity.Y, 0.0f);
            FVector SafeVelocity = HorizontalVelocity * 0.95f;

            if (IS_VECTOR_VALID(SafeVelocity))
            {
                BallMesh->SetPhysicsLinearVelocity(SafeVelocity);
            }

            UE_LOG(LogTemp, Log, TEXT("🏀 Safe: Very low speed, smooth transition"));
            return;
        }

        // 반사 속도 계산
        FVector ReflectedVelocity = CurrentVelocity - 2.0f * FVector::DotProduct(CurrentVelocity, Hit.Normal) * Hit.Normal;

        // 댐핑 적용
        float BounceDamping = PhysicsConfig.Restitution;
        // ✅ 러프에서 초반 바운스(1~3회)는 댐핑을 약하게 적용 → 더 활발하게 튕김
        if (CurrentAppliedTerrain == TEXT("Rough"))
        {
            BounceCountOnCurrentTerrain++;

            if (BounceCountOnCurrentTerrain <= 10)
            {
                // 초반 바운스: 댐핑을 더 약하게 (반발력 보존)
                BounceDamping = FMath::Min(BounceDamping * 1.15f, 0.85f);
                UE_LOG(LogTemp, Log,
                    TEXT("🏀 러프 초반 바운스 #%d: Damping 보강 → %.2f"),
                    BounceCountOnCurrentTerrain, BounceDamping);
            }
        }
        ReflectedVelocity *= BounceDamping;

        // ✅ 수정: 러프에서는 컷오프 낮춰서 작은 바운스도 유지
        float ZCutoff = (CurrentAppliedTerrain == TEXT("Rough")) ? 4.0f : 8.0f;
        if (FMath::Abs(ReflectedVelocity.Z) < ZCutoff)
        {
            ReflectedVelocity.Z = 0.0f;
        }

        // 최종 안전성 체크 후 적용
        if (IS_VECTOR_VALID(ReflectedVelocity))
        {
            BallMesh->SetPhysicsLinearVelocity(ReflectedVelocity);
            UE_LOG(LogTemp, Log, TEXT("🏀 Safe bounce applied: %.1f -> %.1f"),
                IncomingSpeed, ReflectedVelocity.Size());
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("❌ SafeHandleBounce: Invalid reflected velocity"));
        }
    }
    catch (...)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Exception in SafeHandleBounce"));
    }
}

void AGolfBall::SpawnBallParticle(UPhysicalMaterial* PM)
{

    if (!PM)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ SpawnBallParticle: PM is null, skip"));
        return;
    }

    FString PMName = PM->GetName();
    FString ParticleName = "";

    if (PMName.Equals(TEXT("Rough"), ESearchCase::IgnoreCase))     ParticleName = "Bounce_Normal";
    else if (PMName.Equals(TEXT("Bunker"), ESearchCase::IgnoreCase))    ParticleName = "Bounce_Normal";
    else if (PMName.Equals(TEXT("Mat"), ESearchCase::IgnoreCase))       ParticleName = "Bounce_Normal";
    else if (PMName.Equals(TEXT("Net"), ESearchCase::IgnoreCase))     ParticleName = "Bounce_Normal";
    else if (PMName.Equals(TEXT("Green"), ESearchCase::IgnoreCase))     ParticleName = "Bounce_Normal";
    else if (PMName.Equals(TEXT("Leaves"), ESearchCase::IgnoreCase))    ParticleName = "Bounce_leaves";
    else if (PMName.Equals(TEXT("Leavese"), ESearchCase::IgnoreCase))   ParticleName = "Bounce_Normal";
    else if (PMName.Equals(TEXT("Water"), ESearchCase::IgnoreCase))      ParticleName = "Bounce_Water";
    else if (PMName.Equals(TEXT("Bark"), ESearchCase::IgnoreCase))     ParticleName = "Hit_Wood";

    if (GM)
    {
        if (!ParticleName.IsEmpty())
            GM->BallParticleManager->SpawnParticle(GetWorld(), *ParticleName, GetActorLocation());
        else
            UE_LOG(LogTemp, Warning, TEXT("Ball Particle Name is InValid"));
    }
    else
        UE_LOG(LogTemp, Error, TEXT("GM is null from AGolfBall::SpawnBallParticle()"));
}

void AGolfBall::PlaySoundByMaterial(UPhysicalMaterial* PM, float ImpulseSize)
{
    if (!PM)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ SpawnBallParticle: PM is null, skip"));
        return;
    }
    FString PMName = PM->GetName();
    FString SoundId = "";

    // ⭐ 프로젝트 다른 지형 판별 함수(GetTerrainNameFromPhysicalMaterial)와 동일하게 Contains()로 통일.
    //    기존엔 Equals()(완전일치)라서 실제 에셋 이름이 "PM_Fairway_01"처럼 접두/접미가 붙으면 매칭 자체가 안 됐음.
    if (PMName.Contains(TEXT("Rough")))         SoundId = "Effect.Ball.Material.Normal";
    else if (PMName.Contains(TEXT("Fair")))     SoundId = "Effect.Ball.Material.Normal";  // ⭐ Fairway 매핑 누락 - 추가
    else if (PMName.Contains(TEXT("Bunker")))   SoundId = "Effect.Ball.Material.Send";
    else if (PMName.Contains(TEXT("Mat")))      SoundId = "Effect.Ball.Material.Mat";
    else if (PMName.Contains(TEXT("Net")))      SoundId = "Effect.Ball.Material.Net";
    else if (PMName.Contains(TEXT("Green")))    SoundId = "Effect.Ball.Material.Normal";
    else if (PMName.Contains(TEXT("Grass")))    SoundId = "Effect.Ball.Material.Normal";  // ⭐ Grass 매핑 누락 - 추가
    else if (PMName.Contains(TEXT("Leavese")))  SoundId = "Effect.Ball.Material.Leaves"; //오타난 머터리얼이 존재
    else if (PMName.Contains(TEXT("Leaves")))   SoundId = "Effect.Ball.Material.Leaves";
    else if (PMName.Contains(TEXT("Road")))     SoundId = "Effect.Ball.Material.Road";
    else if (PMName.Contains(TEXT("Water")))    SoundId = "Effect.Ball.Material.Water";
    else if (PMName.Contains(TEXT("Bark")))     SoundId = "Effect.Ball.Material.Wood";
    else if (PMName.Contains(TEXT("steel")))    SoundId = "Effect.Ball.Material.Steel";
    else if (PMName.Contains(TEXT("Tee")))      SoundId = "Effect.Ball.Material.Normal";  // ⭐ TeeBox 매핑 누락 - 추가
    else if (PMName.Contains(TEXT("Holecup")))  SoundId = "Effect.Ball.Material.Holecup";

    // ⭐ 위 목록에 없는 새 지형이 추가돼도 완전히 무음이 되지 않도록 기본 사운드로 폴백
    if (SoundId.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ PlaySoundByMaterial: 매핑 없는 PhysMat [%s] → 기본 사운드로 폴백"), *PMName);
        SoundId = "Effect.Ball.Material.Normal";
    }

    UE_LOG(LogTemp, Log, TEXT("Ball Sounds: %s"), *PMName);

    if (GM)
    {
        if (auto* SM = GetGameInstance()->GetSubsystem<USoundManager>())
        {
            if (!SoundId.IsEmpty())
            {
                SM->PlayAtLocation_ById(*SoundId, GetActorLocation(), SM->MapImpulseToVolume(ImpulseSize));
                UE_LOG(LogTemp, Log, TEXT("GM SOUND PLSY ::PlaySoundByMaterial()  -------------->[%s] %s  - %f"), *PMName, *SoundId, ImpulseSize);
            }
            else
                UE_LOG(LogTemp, Log, TEXT("GM SoundID------Empty()"));
        }
    }
    else
        UE_LOG(LogTemp, Error, TEXT("GM is null from AGolfBall::PlaySoundByMaterial()"));
}

UPhysicalMaterial* AGolfBall::GetPhysMatBelow_ThroughEmpty(
    float TraceUp,
    float TraceDown,
    ECollisionChannel Channel,
    bool bTraceComplex)
{
    UWorld* World = GetWorld();
    if (!World) return nullptr;

    const FVector Loc = GetActorLocation();
    const FVector Start = Loc + FVector(0, 0, TraceUp);
    const FVector End = Loc - FVector(0, 0, TraceDown);

    FCollisionQueryParams Params(TEXT("PhysMatQuery"), bTraceComplex, this);
    Params.bReturnPhysicalMaterial = true;

    TArray<FHitResult> Hits;
    if (World->LineTraceMultiByChannel(Hits, Start, End, Channel, Params))
    {
        for (const FHitResult& H : Hits)
        {
            // 콜리전은 있었지만 물리재질이 비어있으면 "통과" 처리하고 다음 히트로
            if (UPhysicalMaterial* PM = H.PhysMaterial.Get())
            {
                return PM; // 첫 번째로 물리재질이 설정된 표면만 인정
            }
        }
    }
    return nullptr; // 끝까지 내려가도 없으면 실패
}

void AGolfBall::ProcessPendingBounce()
{
    if (!IsValid(this) || bIsBeingDestroyed || !bHasPendingBounce)
    {
        bHasPendingBounce = false;
        return;
    }

    try
    {
        if (BallMesh && IsValid(BallMesh) && BallMesh->IsSimulatingPhysics())
        {
            // ✅ 핵심 수정: Ball_Bound 상태에서만 바운스 반사 적용
            // Ball_Rolling/Ball_Stop 상태에서 OnHit이 들어와도
            // 반사 공식을 적용하면 구르는 속도가 비정상적으로 깎여서
            // 일찍 멈춰버리는 문제(바운드 후 바로 Stop) 발생
            if (CurrentBallState == EBallState::Ball_Bound ||
                CurrentBallState == EBallState::Ball_Fly)
            {
                SafeHandleBounce(PendingBounceHit);
            }
            else
            {
                UE_LOG(LogTemp, Verbose,
                    TEXT("⏭️ ProcessPendingBounce 스킵: 현재 상태=%s (Bound 아님)"),
                    *UEnum::GetValueAsString(CurrentBallState));
            }
        }
    }
    catch (...)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Exception in ProcessPendingBounce"));
    }

    bHasPendingBounce = false;
}

// 지형 이름 추출 함수
FString AGolfBall::GetTerrainNameFromPhysicalMaterial(UPhysicalMaterial* PhysMaterial) const
{
    if (!PhysMaterial)
    {
        return TEXT("Rough"); // 기본값
    }

    FString MaterialName = PhysMaterial->GetName();

    // 물리 재질 이름을 지형 이름으로 매핑
    if (MaterialName.Contains(TEXT("Rough"))) return TEXT("Rough");
    else if (MaterialName.Contains(TEXT("Fair")))   return TEXT("FairWay");
    else if (MaterialName.Contains(TEXT("Bunker"))) return TEXT("Bunker");
    else if (MaterialName.Contains(TEXT("Green"))) return TEXT("Green");
    //else if (MaterialName.Contains(TEXT("Mat"))) return TEXT("Mat");
    else if (MaterialName.Contains(TEXT("Net"))) return TEXT("Net");
    else if (MaterialName.Contains(TEXT("Grass"))) return TEXT("Grass");
    else if (MaterialName.Contains(TEXT("Leaves"))) return TEXT("Leaves");
    else if (MaterialName.Contains(TEXT("Leavese"))) return TEXT("Leavese"); // 오타 버전
    else if (MaterialName.Contains(TEXT("Road"))) return TEXT("Road");
    else if (MaterialName.Contains(TEXT("Water"))) return TEXT("Water");
    else if (MaterialName.Contains(TEXT("Bark"))) return TEXT("Bark");
    // ✅ Grass → TerrainSettings에 등록된 "Grass" 키로 반환 (Rough fallback 제거)
    else if (MaterialName.Contains(TEXT("Grass")))   return TEXT("Grass");
    // ✅ Tee → TeeBox 매핑 추가
    else if (MaterialName.Contains(TEXT("Tee")))     return TEXT("TeeBox");

    UE_LOG(LogTemp, Warning, TEXT("⚠️ GetTerrainName: 매핑 없음 [%s] → Rough"), *MaterialName);
    return TEXT("Rough");
}

// 지형별 물리 설정 적용
void AGolfBall::ApplyTerrainPhysicsSettings(const FString& TerrainName, UPhysicalMaterial* TerrainPhysMat)
{
    // ★ JSON에 정의 안 된 지형이면 → 에디터 PhysMat 값을 그대로 사용
    if (!TerrainPhysicsConfig.TerrainSettings.Contains(TerrainName))
    {
        if (IsValid(TerrainPhysMat))
        {
            UE_LOG(LogTemp, Log,
                TEXT("ℹ️ Terrain '%s' not in JSON config → using Editor PhysMat values (Friction=%.3f, Restitution=%.3f)"),
                *TerrainName, TerrainPhysMat->Friction, TerrainPhysMat->Restitution);

            CurrentAppliedTerrain = TerrainName;

            // 볼 PhysMat에 에디터 PhysMat 값을 그대로 복사
            if (IsValid(DefaultPhysicalMaterial))
            {
                DefaultPhysicalMaterial->Friction = TerrainPhysMat->Friction;
                DefaultPhysicalMaterial->Restitution = TerrainPhysMat->Restitution;
            }

            // PhysicsConfig에도 기록 (Tick/UI 참조용) — Damping은 JSON 기본값 사용
            PhysicsConfig.RollingFriction = TerrainPhysMat->Friction;
            PhysicsConfig.Restitution = TerrainPhysMat->Restitution;

            if (BallMesh && BallMesh->IsSimulatingPhysics())
            {
                BallMesh->SetLinearDamping(PhysicsConfig.BaseLinearDamping);
                BallMesh->SetAngularDamping(PhysicsConfig.BaseAngularDamping);
            }
        }
        else
        {
            // 에디터 PhysMat도 없으면 그제서야 Rough로 폴백
            UE_LOG(LogTemp, Warning,
                TEXT("⚠️ Terrain '%s' not in JSON config AND no valid PhysMat → fallback to Rough"),
                *TerrainName);
            ApplyTerrainPhysicsSettings(TEXT("Rough"), TerrainPhysMat);
        }
        return;
    }

    FTerrainPhysicsSettings Settings = TerrainPhysicsConfig.TerrainSettings[TerrainName];
    CurrentTerrainSettings = Settings;
    CurrentAppliedTerrain = TerrainName;

    float CurrentSpeedMS = BallMesh->IsSimulatingPhysics()
        ? BallMesh->GetPhysicsLinearVelocity().Size() / 100.0f : 0.0f;

    if (TerrainName == TEXT("Green") || TerrainName == TEXT("Road"))
    {
        // 기존: MaxBounce = 0.80f / 0.65f
        float MaxBounce = (CurrentSpeedMS >= 10.0f) ? 0.95f : 0.85f;  // ← 상향
        Settings.Restitution = FMath::Clamp(Settings.Restitution, 0.15f, MaxBounce);
        Settings.RollingFriction *= 0.99f;
    }
    else if (TerrainName == TEXT("FairWay"))
    {
        // 기존: MaxBounce = 0.70f / 0.50f
        float MaxBounce = (CurrentSpeedMS >= 10.0f) ? 0.85f : 0.70f;  // ← 상향
        Settings.Restitution = FMath::Clamp(Settings.Restitution, 0.10f, MaxBounce);
        Settings.RollingFriction = FMath::Clamp(Settings.RollingFriction * 0.95f, 0.2f, 0.8f);
    }
    else if (TerrainName == TEXT("Rough"))
    {
        // 기존: MaxBounce = 0.78f / 0.55f
        float MaxBounce = (CurrentSpeedMS >= 10.0f) ? 0.90f : 0.78f;  // ← 상향
        Settings.Restitution = FMath::Clamp(Settings.Restitution, 0.25f, MaxBounce);
        Settings.RollingFriction = FMath::Clamp(Settings.RollingFriction * 0.95f, 0.25f, 0.8f);
    }


    else if (TerrainName == TEXT("Bunker"))
    {
        Settings.Restitution = FMath::Min(Settings.Restitution * 0.5f, 0.1f);
        Settings.RollingFriction *= 0.99f;
    }
    else if (TerrainName == TEXT("Leaves"))
    {
        Settings.Restitution = FMath::Min(Settings.Restitution * 1.05f, 0.8f);
        Settings.RollingFriction *= 0.55f;
    }
    else if (TerrainName == TEXT("Net"))
    {
        Settings.Restitution = FMath::Min(Settings.Restitution * 0.5f, 0.1f);
        Settings.RollingFriction *= 0.5f;
        UE_LOG(LogTemp, Log, TEXT("🌍-------- HIT Net----->"));
    }
    else if (TerrainName == TEXT("Bark"))
    {
        Settings.Restitution = FMath::Min(Settings.Restitution * 0.5f, 0.1f);
        Settings.RollingFriction *= 0.5f;
        UE_LOG(LogTemp, Log, TEXT("🌍-------- HIT BARK------>"));
    }

    if (TerrainName != TEXT("Bunker") && TerrainName != TEXT("Water") &&
        TerrainName != TEXT("Road") && TerrainName != TEXT("Bark") && TerrainName != TEXT("Net"))
    {
        float GameOptionGreenSpeed = GM->GameInfo.GameOptions.Green_Speed - 1;
        float MulltiplySpeed = 1.0f + -(GameOptionGreenSpeed * PhysicsConfig.MulltiflyGrassCondition);
        Settings.RollingFriction *= MulltiplySpeed;
    }

    ApplyPhysicsSettingsFromTerrain(Settings);

    // ⭐ 추가: 복원력(Restitution)을 실제 지형 PhysMaterial에 반영
    //    → 지금까지는 RollingFriction만 실제 적용되고 Restitution은 죽은 값이었음
    // ★ 지형 PhysMat에 JSON Friction/Restitution 직접 기록
    //   Multiply 모드에서 볼 PhysMat = 1.0이므로 지형값이 최종값
//    if (IsValid(TerrainPhysMat))
//    {
//        TerrainPhysMat->Friction = Settings.RollingFriction;
//        TerrainPhysMat->Restitution = Settings.Restitution;
//    }
}

// 물리 설정을 실제로 적용하는 함수   
void AGolfBall::ApplyPhysicsSettingsFromTerrain(const FTerrainPhysicsSettings& Settings)
{
    if (!BallMesh || !IsValid(BallMesh)) return;

    // ✅ PhysicsConfig 내부 상태 기록 (Tick/UI 참조용)
    PhysicsConfig.RollingFriction = Settings.RollingFriction;
    PhysicsConfig.Restitution = Settings.Restitution;
    PhysicsConfig.BaseLinearDamping = Settings.LinearDamping;
    PhysicsConfig.BaseAngularDamping = Settings.AngularDamping;
    PhysicsConfig.AirResistance = Settings.AirResistance;

    // ★ TerrainPhysics.json 값을 볼 PhysMat에 직접 덮어씀
    //    CombineMode = Override 이므로 지형 PhysMat은 완전히 무시되고
    //    이 값이 충돌 시 Chaos 솔버에 최종 Friction/Restitution으로 전달됨
    if (IsValid(DefaultPhysicalMaterial))
    {
        DefaultPhysicalMaterial->Friction = Settings.RollingFriction;
        DefaultPhysicalMaterial->Restitution = Settings.Restitution;
        // BallMesh에 이미 SetPhysMaterialOverride(DefaultPhysicalMaterial)가 적용돼 있으므로
        // 포인터가 같은 오브젝트 → 별도 SetPhysMaterialOverride 재호출 불필요
    }

    // ✅ Damping은 기존대로 매 프레임 Tick에서도 쓰이므로 직접 설정
    if (BallMesh->IsSimulatingPhysics())
    {
        BallMesh->SetLinearDamping(Settings.LinearDamping);
        BallMesh->SetAngularDamping(Settings.AngularDamping);
    }

    UE_LOG(LogTemp, Log,
        TEXT("🌍 [JSON→PhysMat] Terrain [%s] → Friction=%.3f Restitution=%.3f LinearDamp=%.3f AngularDamp=%.3f"),
        *Settings.TerrainName,
        Settings.RollingFriction, Settings.Restitution,
        Settings.LinearDamping, Settings.AngularDamping);
}

// 지형 물리 설정 파일 로드
bool AGolfBall::LoadTerrainPhysicsConfig(const FString& FilePath)
{
    FString ActualPath = FilePath.IsEmpty() ? GetDefaultConfigFilePath() : FilePath;

    // 파일명을 지형 설정용으로 변경
    if (FilePath.IsEmpty())
    {
        FString ProjectDir = FPaths::ProjectSavedDir();
        FString ConfigDir = FPaths::Combine(ProjectDir, TEXT("Config"));
        ActualPath = FPaths::Combine(ConfigDir, TEXT("TerrainPhysics.json"));
    }

    if (LoadConfigFromJson(ActualPath))
    {
        UE_LOG(LogTemp, Log, TEXT("✅ Terrain physics config loaded from: %s"), *ActualPath);
        return true;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("❌ Failed to load terrain config from: %s, creating default"), *ActualPath);
        if (SaveTerrainPhysicsConfig(ActualPath))
        {
            UE_LOG(LogTemp, Log, TEXT("📄 Created default terrain config file: %s"), *ActualPath);
        }
        return false;
    }
}

// 지형 물리 설정 파일 저장
bool AGolfBall::SaveTerrainPhysicsConfig(const FString& FilePath)
{
    FString ActualPath = FilePath.IsEmpty() ? GetDefaultConfigFilePath() : FilePath;

    if (FilePath.IsEmpty())
    {
        FString ProjectDir = FPaths::ProjectSavedDir();
        FString ConfigDir = FPaths::Combine(ProjectDir, TEXT("Config"));
        ActualPath = FPaths::Combine(ConfigDir, TEXT("TerrainPhysics.json"));
    }

    TSharedPtr<FJsonObject> JsonObject = CreateDefaultTerrainConfigJson();

    // 현재 지형 설정들을 JSON에 저장
    SaveTerrainConfigToJsonObject(JsonObject);

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    if (FFileHelper::SaveStringToFile(OutputString, *ActualPath))
    {
        UE_LOG(LogTemp, Log, TEXT("💾 Terrain physics config saved to: %s"), *ActualPath);
        return true;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to save terrain config to: %s"), *ActualPath);
        return false;
    }
}

// JSON 객체에서 지형 설정 로드
void AGolfBall::LoadTerrainConfigFromJsonObject(TSharedPtr<FJsonObject> JsonObject)
{
    if (!JsonObject->HasField(TEXT("TerrainPhysicsConfig")))
    {
        return;
    }

    const TSharedPtr<FJsonObject>* TerrainConfigObj;
    if (JsonObject->TryGetObjectField(TEXT("TerrainPhysicsConfig"), TerrainConfigObj))
    {
        for (auto& Pair : (*TerrainConfigObj)->Values)
        {
            FString TerrainName = Pair.Key;

            if (Pair.Value->Type == EJson::Object)
            {
                TSharedPtr<FJsonObject> TerrainObj = Pair.Value->AsObject();

                FTerrainPhysicsSettings Settings;
                Settings.TerrainName = TerrainName;
                Settings.RollingFriction = TerrainObj->GetNumberField(TEXT("RollingFriction"));
                Settings.Restitution = TerrainObj->GetNumberField(TEXT("Restitution"));   // BounceDamping → Restitution
                Settings.LinearDamping = TerrainObj->GetNumberField(TEXT("LinearDamping"));
                Settings.AngularDamping = TerrainObj->GetNumberField(TEXT("AngularDamping"));
                Settings.AirResistance = TerrainObj->GetNumberField(TEXT("AirResistance"));

                TerrainPhysicsConfig.TerrainSettings.Add(TerrainName, Settings);

                UE_LOG(LogTemp, Log, TEXT("📊 Loaded terrain config: %s (Friction=%.3f, Bounce=%.3f)"),
                    *TerrainName, Settings.RollingFriction, Settings.Restitution);
            }
        }
    }
}

// JSON 객체에 지형 설정 저장
void AGolfBall::SaveTerrainConfigToJsonObject(TSharedPtr<FJsonObject> JsonObject)
{
    TSharedPtr<FJsonObject> TerrainConfigObj = MakeShareable(new FJsonObject);

    for (auto& Pair : TerrainPhysicsConfig.TerrainSettings)
    {
        TSharedPtr<FJsonObject> TerrainObj = MakeShareable(new FJsonObject);
        FTerrainPhysicsSettings Settings = Pair.Value;

        TerrainObj->SetNumberField(TEXT("RollingFriction"), Settings.RollingFriction);
        TerrainObj->SetNumberField(TEXT("Restitution"), Settings.Restitution);
        TerrainObj->SetNumberField(TEXT("LinearDamping"), Settings.LinearDamping);
        TerrainObj->SetNumberField(TEXT("AngularDamping"), Settings.AngularDamping);
        TerrainObj->SetNumberField(TEXT("AirResistance"), Settings.AirResistance);
        TerrainObj->SetStringField(TEXT("TerrainName"), Settings.TerrainName);

        TerrainConfigObj->SetObjectField(Pair.Key, TerrainObj);
    }

    JsonObject->SetObjectField(TEXT("TerrainPhysicsConfig"), TerrainConfigObj);
}

// 기본 지형 설정 JSON 생성
TSharedPtr<FJsonObject> AGolfBall::CreateDefaultTerrainConfigJson() const
{
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

    // 기본 볼 물리 설정 (기존 코드)
    TSharedPtr<FJsonObject> PhysicsObj = MakeShareable(new FJsonObject);
    PhysicsObj->SetNumberField(TEXT("BaseLinearDamping"), PhysicsConfig.BaseLinearDamping);
    PhysicsObj->SetNumberField(TEXT("BaseAngularDamping"), PhysicsConfig.BaseAngularDamping);
    PhysicsObj->SetNumberField(TEXT("RollingFriction"), PhysicsConfig.RollingFriction);
    PhysicsObj->SetNumberField(TEXT("Restitution"), PhysicsConfig.Restitution);
    PhysicsObj->SetNumberField(TEXT("AirResistance"), PhysicsConfig.AirResistance);
    PhysicsObj->SetNumberField(TEXT("GravityScale"), PhysicsConfig.GravityScale);
    PhysicsObj->SetNumberField(TEXT("ForwardSpinFactor"), PhysicsConfig.ForwardSpinFactor);
    JsonObject->SetObjectField(TEXT("BallPhysicsConfig"), PhysicsObj);

    // 파크골프 상수 (기존 코드)
    TSharedPtr<FJsonObject> ConstantsObj = MakeShareable(new FJsonObject);
    ConstantsObj->SetNumberField(TEXT("MIN_SPEED"), ParkGolfConstants.MIN_SPEED);
    ConstantsObj->SetNumberField(TEXT("MAX_SPEED"), ParkGolfConstants.MAX_SPEED);
    ConstantsObj->SetNumberField(TEXT("TYPICAL_SPEED"), ParkGolfConstants.TYPICAL_SPEED);
    ConstantsObj->SetNumberField(TEXT("MIN_LAUNCH_ANGLE"), ParkGolfConstants.MIN_LAUNCH_ANGLE);
    ConstantsObj->SetNumberField(TEXT("MAX_LAUNCH_ANGLE"), ParkGolfConstants.MAX_LAUNCH_ANGLE);
    ConstantsObj->SetNumberField(TEXT("TYPICAL_LAUNCH_ANGLE"), ParkGolfConstants.TYPICAL_LAUNCH_ANGLE);
    JsonObject->SetObjectField(TEXT("ParkGolfConstants"), ConstantsObj);

    // 메타데이터
    JsonObject->SetStringField(TEXT("ConfigVersion"), TEXT("2.0")); // 버전 업
    JsonObject->SetStringField(TEXT("Description"), TEXT("Park Golf Physics Configuration with Terrain Settings"));
    JsonObject->SetStringField(TEXT("LastModified"), FDateTime::Now().ToString());

    return JsonObject;
}

// 특정 지형 설정 가져오기
FTerrainPhysicsSettings AGolfBall::GetTerrainPhysicsSettings(const FString& TerrainName) const
{
    if (TerrainPhysicsConfig.TerrainSettings.Contains(TerrainName))
    {
        return TerrainPhysicsConfig.TerrainSettings[TerrainName];
    }

    // 기본값 반환
    return TerrainPhysicsConfig.TerrainSettings[TEXT("Green")];
}

// 특정 지형 설정 변경
void AGolfBall::SetTerrainPhysicsSettings(const FString& TerrainName, const FTerrainPhysicsSettings& Settings)
{
    TerrainPhysicsConfig.TerrainSettings.Add(TerrainName, Settings);

    // 현재 적용된 지형이라면 즉시 적용
    if (CurrentAppliedTerrain == TerrainName)
    {
        ApplyPhysicsSettingsFromTerrain(Settings);
    }

    UE_LOG(LogTemp, Log, TEXT("🔧 Terrain settings updated: %s"), *TerrainName);
}


void AGolfBall::LogCurrentTerrainSettings() const
{
    UE_LOG(LogTemp, Warning, TEXT("=== Current Terrain Settings ==="));
    UE_LOG(LogTemp, Warning, TEXT("Applied Terrain: %s"), *CurrentAppliedTerrain);
    UE_LOG(LogTemp, Warning, TEXT("Rolling Friction: %.3f"), CurrentTerrainSettings.RollingFriction);
    UE_LOG(LogTemp, Warning, TEXT("Bounce Damping: %.3f"), CurrentTerrainSettings.Restitution);
    UE_LOG(LogTemp, Warning, TEXT("Linear Damping: %.3f"), CurrentTerrainSettings.LinearDamping);
    UE_LOG(LogTemp, Warning, TEXT("Angular Damping: %.3f"), CurrentTerrainSettings.AngularDamping);
    UE_LOG(LogTemp, Warning, TEXT("Air Resistance: %.3f"), CurrentTerrainSettings.AirResistance);
    UE_LOG(LogTemp, Warning, TEXT("==============================="));
#if WITH_EDITOR
    if (GEngine)
    {
        FString DebugText = FString::Printf(
            TEXT("Current Terrain: %s\nFriction: %.2f | Bounce: %.2f"),
            *CurrentAppliedTerrain,
            CurrentTerrainSettings.RollingFriction,
            CurrentTerrainSettings.Restitution
        );

        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, DebugText);
    }
#endif
}


// 1. 지면 경사 정보를 구하는 개선된 함수
FVector AGolfBall::GetAccurateTerrainNormal() const
{
    if (!BallMesh) return FVector::UpVector;

    FVector BallLocation = GetActorLocation();
    float ActualBallRadius = GetActualBallRadius();

    // 더 정밀한 지면 감지를 위해 여러 방향으로 트레이스
    TArray<FVector> TraceDirections = {
        FVector(0, 0, -1),           // 아래
        FVector(50, 0, -1),          // 앞쪽
        FVector(-50, 0, -1),         // 뒤쪽
        FVector(0, 50, -1),          // 오른쪽
        FVector(0, -50, -1)          // 왼쪽
    };

    FVector AverageNormal = FVector::UpVector;
    int32 ValidHits = 0;

    for (const FVector& Direction : TraceDirections)
    {
        FVector Start = BallLocation + FVector(0, 0, 10.0f);
        FVector End = Start + Direction.GetSafeNormal() * (ActualBallRadius + 30.0f);

        FHitResult HitResult;
        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(this);
        QueryParams.bTraceComplex = true;

        if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_WorldStatic, QueryParams))
        {
            AverageNormal += HitResult.Normal;
            ValidHits++;
        }
    }

    if (ValidHits > 0)
    {
        AverageNormal = AverageNormal.GetSafeNormal();
        return AverageNormal;
    }

    return FVector::UpVector;
}

// 2. 지면 경사각을 계산하는 새로운 함수
float AGolfBall::GetGroundSlopeAngle() const
{
    FVector GroundNormal = GetAccurateTerrainNormal();

    // 수직벡터와 지면 법선 사이의 각도 계산
    float SlopeAngleRad = FMath::Acos(FMath::Clamp(FVector::DotProduct(GroundNormal, FVector::UpVector), -1.0f, 1.0f));
    float SlopeAngleDeg = FMath::RadiansToDegrees(SlopeAngleRad);

    return SlopeAngleDeg;
}
// 3. 지면 경사 방향을 계산하는 함수
FVector AGolfBall::GetGroundSlopeDirection() const
{
    FVector GroundNormal = GetAccurateTerrainNormal();

    // 지면 법선을 수평면에 투영하여 경사 방향 계산
    FVector SlopeDirection = FVector(GroundNormal.X, GroundNormal.Y, 0.0f).GetSafeNormal();

    // 경사가 올라가는 방향을 반환 (법선과 반대)
    return -SlopeDirection;
}

// 4. 개선된 발사 방향 계산 함수
FVector AGolfBall::CalculateShotDirectionWithTerrainSlope(const FVector& BaseDirection, float LaunchAngleDegrees, float YawDegrees)
{
    // 지면 정보 수집
    FVector TerrainNormal = GetAccurateTerrainNormal();
    float GroundSlopeAngle = GetGroundSlopeAngle();
    FVector GroundSlopeDirection = GetGroundSlopeDirection();

    UE_LOG(LogTemp, Log, TEXT("🏔️ Ground Analysis: SlopeAngle=%.2f°, Normal=%s"),
        GroundSlopeAngle, *TerrainNormal.ToString());

    // 1. Yaw 회전 적용 (수평 방향 조정)
    FVector AdjustedDirection = BaseDirection.GetSafeNormal();
    if (!FMath::IsNearlyZero(YawDegrees))
    {
        FRotator YawRotation(0.0f, YawDegrees, 0.0f);
        AdjustedDirection = YawRotation.RotateVector(AdjustedDirection);
    }

    // 2. 지면에 평행한 전진 방향 계산
    FVector GroundForward = FVector::CrossProduct(
        FVector::CrossProduct(AdjustedDirection, TerrainNormal),
        TerrainNormal
    ).GetSafeNormal();

    // 3. 지면 경사를 고려한 발사각도 조정
    float EffectiveLaunchAngle = LaunchAngleDegrees;

    // 경사면에서의 각도 보정
    if (GroundSlopeAngle > 1.0f) // 경사가 1도 이상일 때
    {
        // 샷 방향과 경사 방향의 관계 계산
        float SlopeInfluence = FVector::DotProduct(AdjustedDirection, GroundSlopeDirection);

        // 경사 위로 치는 경우 각도 증가, 경사 아래로 치는 경우 각도 감소
        float AngleAdjustment = GroundSlopeAngle * SlopeInfluence * 0.7f; // 70% 영향도
        EffectiveLaunchAngle += AngleAdjustment;

        // 안전 범위로 클램프
        EffectiveLaunchAngle = FMath::Clamp(EffectiveLaunchAngle,
            ParkGolfConstants.MIN_LAUNCH_ANGLE,
            ParkGolfConstants.MAX_LAUNCH_ANGLE * 1.5f);

        UE_LOG(LogTemp, Log, TEXT("🎯 Slope-adjusted launch angle: %.2f° → %.2f° (slope influence: %.2f)"),
            LaunchAngleDegrees, EffectiveLaunchAngle, SlopeInfluence);
    }

    // 4. 지면을 기준으로 한 발사 방향 계산
    float LaunchFactor = FMath::Sin(FMath::DegreesToRadians(EffectiveLaunchAngle));
    float HorizontalFactor = FMath::Cos(FMath::DegreesToRadians(EffectiveLaunchAngle));

    // 지면에 수직인 업 벡터와 지면에 평행한 전진 벡터를 조합
    FVector LaunchDirection = (GroundForward * HorizontalFactor) + (TerrainNormal * LaunchFactor);
    LaunchDirection = LaunchDirection.GetSafeNormal();

    // 5. 최종 안전성 검증
    if (LaunchDirection.ContainsNaN() || !LaunchDirection.IsNormalized())
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Invalid launch direction, using fallback"));
        return CalculateFallbackDirection(AdjustedDirection, LaunchAngleDegrees);
    }

    // 디버그 정보
    float ActualLaunchAngle = FMath::RadiansToDegrees(
        FMath::Acos(FVector::DotProduct(LaunchDirection, FVector(LaunchDirection.X, LaunchDirection.Y, 0.0f).GetSafeNormal()))
    );

    UE_LOG(LogTemp, Log, TEXT("🚀 Final Launch: Direction=%s, ActualAngle=%.2f°"),
        *LaunchDirection.ToString(), ActualLaunchAngle);

    return LaunchDirection;
}

// 5. 폴백 발사 방향 계산 (에러 발생 시)
FVector AGolfBall::CalculateFallbackDirection(const FVector& Direction, float LaunchAngle) const
{
    float LaunchFactor = LaunchAngle / 90.0f;
    return FMath::Lerp(
        FVector(Direction.X, Direction.Y, 0.0f).GetSafeNormal(),
        FVector::UpVector,
        LaunchFactor
    ).GetSafeNormal();
}
// 1. 진행방향 앞의 지면 법선을 구하는 함수
FVector AGolfBall::GetForwardTerrainNormal(const FVector& Direction, float ForwardDistance) const
{
    if (!GetWorld()) return FVector::UpVector;

    FVector BallLocation = GetActorLocation();
    float ActualBallRadius = GetActualBallRadius();

    // 진행방향으로 ForwardDistance만큼 앞의 위치 계산
    FVector ForwardDirection = FVector(Direction.X, Direction.Y, 0.0f).GetSafeNormal(); // 수평 방향만
    FVector ForwardPosition = BallLocation + (ForwardDirection * ForwardDistance);

    UE_LOG(LogTemp, Log, TEXT("🔍 Checking forward terrain: Current=%s, Forward=%s (%.1fcm ahead)"),
        *BallLocation.ToString(), *ForwardPosition.ToString(), ForwardDistance);

    // 앞 지점에서 아래로 트레이스하여 지면 찾기
    FVector TraceStart = ForwardPosition + FVector(0, 0, 50.0f); // 위에서 시작
    FVector TraceEnd = ForwardPosition - FVector(0, 0, 100.0f);  // 아래까지

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    QueryParams.bTraceComplex = true;

    if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
    {
        FVector ForwardTerrainNormal = HitResult.Normal;

        UE_LOG(LogTemp, Log, TEXT("✅ Forward terrain found: Normal=%s, Height=%.1fcm"),
            *ForwardTerrainNormal.ToString(), HitResult.Location.Z);

        // 디버그 시각화
        //if (GetWorld())
        //{
        //    DrawDebugLine(GetWorld(), TraceStart, HitResult.Location, FColor::Yellow, false, 2.0f, 0, 2.0f);
        //    DrawDebugSphere(GetWorld(), HitResult.Location, 5.0f, 8, FColor::Green, false, 2.0f);

        //    // 법선 벡터 표시
        //    FVector NormalEnd = HitResult.Location + (ForwardTerrainNormal * 30.0f);
        //    DrawDebugLine(GetWorld(), HitResult.Location, NormalEnd, FColor::Cyan, false, 2.0f, 0, 3.0f);
        //}

        return ForwardTerrainNormal;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ No forward terrain found, using current position"));

        // 앞 지면을 찾지 못하면 현재 위치의 지면 사용
        return GetAccurateTerrainNormal();
    }
}

// 2. 앞 지면을 고려한 발사 방향 계산 함수
FVector AGolfBall::CalculateShotDirectionWithForwardTerrain(const FVector& BaseDirection, float LaunchAngleDegrees, float YawDegrees)
{
    // 진행방향 10cm 앞의 지면 법선 구하기
    FVector ForwardTerrainNormal = GetForwardTerrainNormal(BaseDirection, 10.0f);

    // 현재 위치와 앞 지면의 높이 차이 계산
    FVector CurrentPosition = GetActorLocation();
    FVector ForwardDirection = FVector(BaseDirection.X, BaseDirection.Y, 0.0f).GetSafeNormal();
    FVector ForwardPosition = CurrentPosition + (ForwardDirection * 10.0f);

    // 앞 지면의 높이를 구하기 위한 트레이스
    FVector TraceStart = ForwardPosition + FVector(0, 0, 50.0f);
    FVector TraceEnd = ForwardPosition - FVector(0, 0, 100.0f);

    FHitResult ForwardHit;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    QueryParams.bTraceComplex = true;

    float HeightDifference = 0.0f;
    if (GetWorld()->LineTraceSingleByChannel(ForwardHit, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
    {
        HeightDifference = ForwardHit.Location.Z - CurrentPosition.Z;
    }

    UE_LOG(LogTemp, Log, TEXT("🏔️ Forward terrain analysis: HeightDiff=%.1fcm, Normal=%s"),
        HeightDifference, *ForwardTerrainNormal.ToString());

    // 1. Yaw 회전 적용
    FVector AdjustedDirection = BaseDirection.GetSafeNormal();
    if (!FMath::IsNearlyZero(YawDegrees))
    {
        FRotator YawRotation(0.0f, YawDegrees, 0.0f);
        AdjustedDirection = YawRotation.RotateVector(AdjustedDirection);
    }

    // 2. 앞 지면을 기준으로 한 전진 방향 계산
    FVector ForwardVector = FVector::CrossProduct(ForwardTerrainNormal,
        FVector::CrossProduct(AdjustedDirection, ForwardTerrainNormal)).GetSafeNormal();

    // 3. 높이 차이를 고려한 발사각도 조정
    float EffectiveLaunchAngle = LaunchAngleDegrees;

    // 앞 지면이 높으면 각도 증가, 낮으면 각도 감소
    if (FMath::Abs(HeightDifference) > 2.0f) // 2cm 이상 차이날 때
    {
        float AngleAdjustment = FMath::Atan2(HeightDifference, 10.0f) * (180.0f / PI); // 10cm 거리 기준
        EffectiveLaunchAngle += AngleAdjustment * 0.8f; // 80% 영향도

        // 안전 범위로 클램프
        EffectiveLaunchAngle = FMath::Clamp(EffectiveLaunchAngle,
            ParkGolfConstants.MIN_LAUNCH_ANGLE,
            ParkGolfConstants.MAX_LAUNCH_ANGLE * 1.2f);

        UE_LOG(LogTemp, Log, TEXT("🎯 Angle adjusted for height diff: %.2f° → %.2f° (diff: %.1fcm)"),
            LaunchAngleDegrees, EffectiveLaunchAngle, HeightDifference);
    }

    // 4. 마찰 방지를 위한 추가 높이 조정
    // 지면에 너무 가깝게 발사되는 것을 방지하기 위해 최소 발사각도 보장
    float MinSafeAngle = 2.0f; // 최소 2도
    if (EffectiveLaunchAngle < MinSafeAngle)
    {
        EffectiveLaunchAngle = MinSafeAngle;
        UE_LOG(LogTemp, Log, TEXT("🛡️ Minimum safe angle applied: %.2f°"), MinSafeAngle);
    }

    // 5. 최종 발사 방향 계산
    float LaunchFactor = EffectiveLaunchAngle / 90.0f;
    FVector LaunchDirection = FMath::Lerp(ForwardVector, ForwardTerrainNormal, LaunchFactor);

    // 6. 최종 검증 및 정규화
    FVector FinalDirection = LaunchDirection.GetSafeNormal();
    if (FinalDirection.ContainsNaN() || !FinalDirection.IsNormalized())
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Invalid launch direction, using fallback"));
        return CalculateFallbackDirection(AdjustedDirection, EffectiveLaunchAngle);
    }

    // 실제 발사각도 계산 (디버깅용)
    float ActualLaunchAngle = FMath::RadiansToDegrees(
        FMath::Asin(FMath::Clamp(FinalDirection.Z, -1.0f, 1.0f))
    );

    UE_LOG(LogTemp, Log, TEXT("🚀 Forward-terrain launch: Direction=%s, ActualAngle=%.2f°"),
        *FinalDirection.ToString(), ActualLaunchAngle);

    // 디버그 시각화
    //if (GetWorld())
    //{
    //    FVector BallPos = GetActorLocation();
    //    FVector LaunchEnd = BallPos + (FinalDirection * 200.0f); // 2m 길이로 표시
    //    DrawDebugLine(GetWorld(), BallPos, LaunchEnd, FColor::Magenta, false, 3.0f, 0, 4.0f);

    //    // 발사각도 정보 표시
    //    DrawDebugString(GetWorld(), BallPos + FVector(0, 0, 80),
    //        FString::Printf(TEXT("Launch: %.1f° (adj: %.1f°)"), ActualLaunchAngle, EffectiveLaunchAngle),
    //        nullptr, FColor::Magenta, 3.0f, false);
    //}

    return FinalDirection;
}


// 3. 화살표 그리기 함수 구현
void AGolfBall::DrawShotDirectionArrow(const FVector& StartLocation, const FVector& Direction, float Power, float LaunchAngle)
{
    if (!GetWorld()) return;

    // 화살표 길이 계산 (파워에 비례)
    float ArrowLength = (Power / 20.0f) * ShotArrowScale; // 20m/s 기준으로 정규화
    ArrowLength = FMath::Clamp(ArrowLength, 50.0f, 500.0f); // 최소 50cm, 최대 5m

    // 화살표 끝점 계산
    FVector ArrowEnd = StartLocation + (Direction * ArrowLength);

    // 파워에 따른 색상 결정
    FLinearColor ArrowColor = GetShotPowerColor(Power);

    // 메인 화살표 그리기
    //DrawDebugDirectionalArrow(
    //    GetWorld(),
    //    StartLocation + FVector(0, 0, 5.0f), // 시작점을 5cm 위로
    //    ArrowEnd + FVector(0, 0, 5.0f),      // 끝점도 5cm 위로
    //    30.0f,                               // 화살표 머리 크기
    //    ArrowColor.ToFColor(true),
    //    false,                               // 지속적
    //    ShotArrowDuration,                   // 지속 시간
    //    0,                                   // 우선순위
    //    ShotArrowThickness                   // 두께
    //);

    // 추가 시각화: 파워 표시 링
    DrawShotPowerIndicator(StartLocation, Power);

    // 발사각도 표시
    DrawShotAngleIndicator(StartLocation, Direction, LaunchAngle);

    // 예상 착지점 표시
    DrawEstimatedLandingPoint(StartLocation, Direction, Power, LaunchAngle);

    UE_LOG(LogTemp, Log, TEXT("Shot arrow drawn: Power=%.1fm/s, Length=%.1fcm, Angle=%.1f°"),
        Power, ArrowLength, LaunchAngle);
}

// 4. 파워에 따른 색상 계산
FLinearColor AGolfBall::GetShotPowerColor(float Power)
{
    // 파워를 0-1로 정규화 (0-25m/s 기준)
    float NormalizedPower = FMath::Clamp(Power / 25.0f, 0.0f, 1.0f);

    if (NormalizedPower < 0.3f) // 약한 샷 (0-30%)
    {
        return FLinearColor::Green; // 초록색
    }
    else if (NormalizedPower < 0.6f) // 보통 샷 (30-60%)
    {
        // 초록에서 노랑으로 그라데이션
        float Ratio = (NormalizedPower - 0.3f) / 0.3f;
        return FLinearColor::LerpUsingHSV(FLinearColor::Green, FLinearColor::Yellow, Ratio);
    }
    else if (NormalizedPower < 0.8f) // 강한 샷 (60-80%)
    {
        // 노랑에서 주황으로 그라데이션
        float Ratio = (NormalizedPower - 0.6f) / 0.2f;
        return FLinearColor::LerpUsingHSV(FLinearColor::Yellow,
            FLinearColor(1.0f, 0.5f, 0.0f, 1.0f), Ratio); // 주황색
    }
    else // 매우 강한 샷 (80-100%)
    {
        // 주황에서 빨강으로 그라데이션
        float Ratio = (NormalizedPower - 0.8f) / 0.2f;
        return FLinearColor::LerpUsingHSV(FLinearColor(1.0f, 0.5f, 0.0f, 1.0f),
            FLinearColor::Red, Ratio);
    }
}

// 5. 파워 표시 링 그리기
void AGolfBall::DrawShotPowerIndicator(const FVector& StartLocation, float Power)
{
    if (!GetWorld()) return;

    // 파워에 비례한 링 크기
    float RingRadius = (Power / 25.0f) * 100.0f; // 최대 1m 반지름
    RingRadius = FMath::Clamp(RingRadius, 20.0f, 150.0f);

    // 파워 링 그리기 (지면에)
    DrawDebugCircle(
        GetWorld(),
        StartLocation,
        RingRadius,
        32,                                    // 세그먼트 수
        GetShotPowerColor(Power).ToFColor(true),
        false,                                 // 지속적
        ShotArrowDuration,                     // 지속 시간
        0,                                     // 우선순위
        1.0f,                                  // 두께
        FVector(0, 1, 0),                      // Y축
        FVector(1, 0, 0)                       // X축
    );

    // 파워 텍스트 표시
    FString PowerText = FString::Printf(TEXT("%.1fm/s"), Power);
    DrawDebugString(
        GetWorld(),
        StartLocation + FVector(0, 0, 30.0f),
        PowerText,
        nullptr,
        GetShotPowerColor(Power).ToFColor(true),
        ShotArrowDuration,
        false,
        1.5f // 텍스트 크기
    );
}

// 6. 발사각도 표시
void AGolfBall::DrawShotAngleIndicator(const FVector& StartLocation, const FVector& Direction, float LaunchAngle)
{
    if (!GetWorld() || LaunchAngle < 1.0f) return;

    // 수평선 그리기
    FVector HorizontalDir = FVector(Direction.X, Direction.Y, 0.0f).GetSafeNormal();
    FVector HorizontalEnd = StartLocation + (HorizontalDir * 80.0f);

    DrawDebugLine(
        GetWorld(),
        StartLocation + FVector(0, 0, 5.0f),
        HorizontalEnd + FVector(0, 0, 5.0f),
        FColor::White,
        false,
        ShotArrowDuration,
        0,
        0.5f
    );

    // 각도 호 그리기 (간단한 선들로 근사)
    int32 ArcSegments = FMath::Max(3, (int32)(LaunchAngle / 5.0f));
    for (int32 i = 0; i <= ArcSegments; i++)
    {
        float CurrentAngle = (float(i) / ArcSegments) * FMath::DegreesToRadians(LaunchAngle);
        float NextAngle = (float(i + 1) / ArcSegments) * FMath::DegreesToRadians(LaunchAngle);

        if (i < ArcSegments)
        {
            FVector ArcPoint1 = StartLocation + FVector(
                HorizontalDir.X * FMath::Cos(CurrentAngle) * 60.0f,
                HorizontalDir.Y * FMath::Cos(CurrentAngle) * 60.0f,
                FMath::Sin(CurrentAngle) * 60.0f + 5.0f
            );

            FVector ArcPoint2 = StartLocation + FVector(
                HorizontalDir.X * FMath::Cos(NextAngle) * 60.0f,
                HorizontalDir.Y * FMath::Cos(NextAngle) * 60.0f,
                FMath::Sin(NextAngle) * 60.0f + 5.0f
            );

            DrawDebugLine(
                GetWorld(),
                ArcPoint1,
                ArcPoint2,
                FColor::Cyan,
                false,
                ShotArrowDuration,
                0,
                0.5f
            );
        }
    }

    // 각도 텍스트 표시
    FString AngleText = FString::Printf(TEXT("%.1f°"), LaunchAngle);
    FVector AngleTextPos = StartLocation + FVector(
        HorizontalDir.X * 40.0f,
        HorizontalDir.Y * 40.0f,
        15.0f
    );

    DrawDebugString(
        GetWorld(),
        AngleTextPos,
        AngleText,
        nullptr,
        FColor::Cyan,
        ShotArrowDuration,
        false,
        1.0f
    );
}

void AGolfBall::DrawEstimatedLandingPoint(const FVector& StartLocation, const FVector& Direction, float Power, float LaunchAngle)
{
    if (!GetWorld()) return;

    // 간단한 포물선 계산으로 예상 착지점 계산
    float EstimatedDistance = CalculateExpectedDistance(Power, LaunchAngle);

    // 착지점 위치 계산 (수평 방향으로)
    FVector HorizontalDir = FVector(Direction.X, Direction.Y, 0.0f).GetSafeNormal();
    FVector EstimatedLandingPos = StartLocation + (HorizontalDir * EstimatedDistance * 100.0f); // m를 cm로 변환

    // 지면 높이에 맞춰 조정
    FVector AdjustedLandingPos = GetGroundAdjustedPosition(EstimatedLandingPos);

    // 착지점 마커 그리기
    DrawDebugSphere(
        GetWorld(),
        AdjustedLandingPos + FVector(0, 0, 10.0f),
        15.0f, // 반지름
        12,    // 세그먼트
        FColor::Orange,
        false,
        ShotArrowDuration,
        0,
        0.5f
    );

    // 착지점까지의 점선 그리기
    int32 DashCount = FMath::Max(5, (int32)(EstimatedDistance / 10.0f)); // 10m마다 점
    for (int32 i = 1; i < DashCount; i++)
    {
        float Ratio = float(i) / DashCount;
        FVector DashPoint = FMath::Lerp(StartLocation, AdjustedLandingPos, Ratio);
        DashPoint.Z += 20.0f; // 20cm 위로

        DrawDebugSphere(
            GetWorld(),
            DashPoint,
            3.0f,
            8,
            FColor::Yellow,
            false,
            ShotArrowDuration,
            0,
            1.0f
        );
    }

    // 거리 텍스트 표시
    FString DistanceText = FString::Printf(TEXT("~%.0fm"), EstimatedDistance);
    DrawDebugString(
        GetWorld(),
        AdjustedLandingPos + FVector(0, 0, 40.0f),
        DistanceText,
        nullptr,
        FColor::Orange,
        ShotArrowDuration,
        false,
        1.2f
    );
}

// 8. 지면 높이 조정된 위치 계산
FVector AGolfBall::GetGroundAdjustedPosition(const FVector& Position)
{
    if (!GetWorld()) return Position;

    FVector Start = Position + FVector(0, 0, 200.0f); // 2m 위에서
    FVector End = Position - FVector(0, 0, 200.0f);   // 2m 아래까지

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    QueryParams.bTraceComplex = true;

    if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_WorldStatic, QueryParams))
    {
        return HitResult.Location;
    }

    return Position; // 지면을 찾지 못하면 원래 위치 반환
}

// 9. 화살표 표시 토글 함수들

void AGolfBall::SetShotArrowVisible(bool bVisible)
{
    bShowShotArrow = bVisible;
    UE_LOG(LogTemp, Log, TEXT("Shot arrow visibility: %s"), bVisible ? TEXT("ON") : TEXT("OFF"));
}

void AGolfBall::SetShotArrowDuration(float Duration)
{
    ShotArrowDuration = FMath::Clamp(Duration, 1.0f, 30.0f);
    UE_LOG(LogTemp, Log, TEXT("Shot arrow duration set to: %.1fs"), ShotArrowDuration);
}


void AGolfBall::DrawGroundNormalVector(const FHitResult& Hit)
{
    if (!GetWorld()) return;

    // 충돌 지점
    FVector HitLocation = Hit.ImpactPoint;

    // 지면 법선벡터 (Hit.Normal 또는 Hit.ImpactNormal 사용)
    FVector GroundNormal = Hit.Normal;

    // 법선벡터가 유효한지 확인
    if (GroundNormal.IsNearlyZero())
    {
        GroundNormal = Hit.ImpactNormal;
    }

    if (GroundNormal.IsNearlyZero())
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Invalid ground normal vector"));
        return;
    }

    // 법선벡터 정규화
    GroundNormal = GroundNormal.GetSafeNormal();

    // ⭐ 속도 정보 계산
    FVector CurrentVelocity = GetBallVelocity();
    float TotalSpeed = CurrentVelocity.Size();
    float SpeedMS = TotalSpeed / 100.0f; // cm/s -> m/s 변환

    // 속도 성분 분리
    FVector HorizontalVelocity = FVector(CurrentVelocity.X, CurrentVelocity.Y, 0.0f);
    float HorizontalSpeed = HorizontalVelocity.Size();
    float VerticalSpeed = FMath::Abs(CurrentVelocity.Z);

    // 입사각 계산 (지면 법선과 속도벡터 사이의 각도)
    float IncidenceAngle = 0.0f;
    if (!CurrentVelocity.IsNearlyZero())
    {
        float DotProduct = FVector::DotProduct(-CurrentVelocity.GetSafeNormal(), GroundNormal);
        IncidenceAngle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(DotProduct, 0.0f, 1.0f)));
    }

    // 지면 경사각 계산
    float SlopeAngle = FMath::RadiansToDegrees(
        FMath::Acos(FMath::Clamp(FVector::DotProduct(GroundNormal, FVector::UpVector), 0.0f, 1.0f))
    );

    // 법선벡터의 끝점 계산 (50cm 길이)
    float NormalLength = 50.0f;
    FVector NormalEnd = HitLocation + (GroundNormal * NormalLength);

    // 법선벡터를 파란색 선으로 그리기
    DrawDebugLine(
        GetWorld(),
        HitLocation,
        NormalEnd,
        FColor::Blue,
        false,           // 지속적이지 않음
        5.0f,           // 5초간 표시 (속도 정보 읽을 시간 확보)
        0,              // 우선순위
        3.0f            // 선 두께
    );

    // 법선벡터 끝에 작은 구체로 방향 표시
    DrawDebugSphere(
        GetWorld(),
        NormalEnd,
        3.0f,           // 반지름 3cm
        8,              // 세그먼트 수
        FColor::Blue,
        false,          // 지속적이지 않음
        5.0f           // 5초간 표시
    );

    // 충돌 지점에 빨간 점으로 표시
    DrawDebugSphere(
        GetWorld(),
        HitLocation,
        2.0f,           // 반지름 2cm
        8,              // 세그먼트 수
        FColor::Red,
        false,          // 지속적이지 않음
        5.0f           // 5초간 표시
    );

    // ⭐ 속도 벡터 시각화 (마젠타색으로 표시)
    if (!CurrentVelocity.IsNearlyZero())
    {
        // 속도 벡터를 적절한 크기로 스케일링 (최대 100cm)
        float VelocityScale = FMath::Min(100.0f, TotalSpeed * 0.1f);
        FVector VelocityEnd = HitLocation + (CurrentVelocity.GetSafeNormal() * VelocityScale);

        DrawDebugLine(
            GetWorld(),
            HitLocation,
            VelocityEnd,
            FColor::Magenta,
            false,
            5.0f,
            0,
            2.0f
        );

        // 속도 벡터 끝에 화살표 표시
        DrawDebugSphere(
            GetWorld(),
            VelocityEnd,
            2.0f,
            6,
            FColor::Magenta,
            false,
            5.0f
        );
    }

    // ⭐ 확장된 디버그 정보 텍스트 (속도 정보 포함)
    FString DebugText = FString::Printf(
        TEXT("Ground Collision Info\n")
        TEXT("▼ 속도 정보 ▼\n")
        TEXT("총 속도: %.1f cm/s (%.2f m/s)\n")
        TEXT("수평 속도: %.1f cm/s\n")
        TEXT("수직 속도: %.1f cm/s\n")
        TEXT("입사각: %.1f°\n")
        TEXT("▼ 지면 정보 ▼\n")
        TEXT("지면 경사: %.1f°\n")
        TEXT("법선벡터: (%.2f, %.2f, %.2f)\n")
        TEXT("▼ 볼 상태 ▼\n")
        TEXT("상태: %s"),
        TotalSpeed, SpeedMS,
        HorizontalSpeed / 100.0f,
        VerticalSpeed / 100.0f,
        IncidenceAngle,
        SlopeAngle,
        GroundNormal.X, GroundNormal.Y, GroundNormal.Z,
        *UEnum::GetValueAsString(CurrentBallState).Right(6) // 상태명의 뒷부분만
    );

    DrawDebugString(
        GetWorld(),
        HitLocation + FVector(0, 0, 30.0f), // 충돌 지점 위 30cm에 텍스트
        DebugText,
        nullptr,
        FColor::White,
        5.0f,           // 5초간 표시
        false,          // 그림자 없음
        1.0f            // 텍스트 크기
    );

    // 추가: 지면 평면 시각화 (옵션)
    DrawGroundPlaneVisualization(HitLocation, GroundNormal);

    // 추가: 속도 성분별 시각화
    DrawVelocityComponents(HitLocation, CurrentVelocity, GroundNormal);

    UE_LOG(LogTemp, Log, TEXT("🔵 Ground Normal: %s, Slope: %.1f°, Speed: %.1fm/s, Incidence: %.1f°"),
        *GroundNormal.ToString(), SlopeAngle, SpeedMS, IncidenceAngle);
}


// 추가 함수: 지면 평면 시각화
void AGolfBall::DrawGroundPlaneVisualization(const FVector& HitLocation, const FVector& GroundNormal)
{
    if (!GetWorld()) return;

    // 지면에 수직인 두 벡터를 구해서 평면을 표현
    FVector Right = FVector::CrossProduct(GroundNormal, FVector::ForwardVector);
    if (Right.IsNearlyZero())
    {
        Right = FVector::CrossProduct(GroundNormal, FVector::RightVector);
    }
    Right = Right.GetSafeNormal();

    FVector Forward = FVector::CrossProduct(Right, GroundNormal).GetSafeNormal();

    // 지면 평면을 나타내는 십자가 그리기
    float PlaneSize = 40.0f; // 40cm

    // 가로선
    FVector PlaneStart1 = HitLocation + (Right * PlaneSize);
    FVector PlaneEnd1 = HitLocation - (Right * PlaneSize);

    // 세로선
    FVector PlaneStart2 = HitLocation + (Forward * PlaneSize);
    FVector PlaneEnd2 = HitLocation - (Forward * PlaneSize);

    // 연한 초록색으로 지면 평면 표시
    DrawDebugLine(GetWorld(), PlaneStart1, PlaneEnd1, FColor::Green, false, 5.0f, 0, 2.0f);
    DrawDebugLine(GetWorld(), PlaneStart2, PlaneEnd2, FColor::Green, false, 5.0f, 0, 2.0f);

    // 평면의 모서리를 연결하는 사각형
    TArray<FVector> PlaneCorners = {
        HitLocation + (Right * PlaneSize * 0.7f) + (Forward * PlaneSize * 0.7f),
        HitLocation - (Right * PlaneSize * 0.7f) + (Forward * PlaneSize * 0.7f),
        HitLocation - (Right * PlaneSize * 0.7f) - (Forward * PlaneSize * 0.7f),
        HitLocation + (Right * PlaneSize * 0.7f) - (Forward * PlaneSize * 0.7f)
    };

    for (int32 i = 0; i < 4; i++)
    {
        FVector Start = PlaneCorners[i];
        FVector End = PlaneCorners[(i + 1) % 4];
        DrawDebugLine(GetWorld(), Start, End, FColor(0, 255, 0, 100), false, 5.0f, 0, 1.0f);
    }
}

void AGolfBall::DrawVelocityComponents(const FVector& HitLocation, const FVector& Velocity, const FVector& GroundNormal)
{
    if (!GetWorld() || Velocity.IsNearlyZero()) return;

    // 속도 벡터를 지면 법선 기준으로 분해
    FVector NormalComponent = FVector::DotProduct(Velocity, GroundNormal) * GroundNormal;
    FVector TangentialComponent = Velocity - NormalComponent;

    float VelocityScale = 0.05f; // 스케일 조정 (너무 길지 않게)

    // 전체 속도 벡터 (마젠타)
    FVector TotalVelocityEnd = HitLocation + (Velocity * VelocityScale);
    DrawDebugLine(GetWorld(), HitLocation, TotalVelocityEnd, FColor::Magenta, false, 5.0f, 0, 3.0f);
    DrawDebugSphere(GetWorld(), TotalVelocityEnd, 2.0f, 6, FColor::Magenta, false, 5.0f);

    // 법선 성분 (지면에 수직한 속도) - 빨간색
    if (!NormalComponent.IsNearlyZero())
    {
        FVector NormalEnd = HitLocation + (NormalComponent * VelocityScale);
        DrawDebugLine(GetWorld(), HitLocation, NormalEnd, FColor::Red, false, 5.0f, 0, 2.5f);
        DrawDebugSphere(GetWorld(), NormalEnd, 1.5f, 6, FColor::Red, false, 5.0f);

        // 법선 성분 정보
        DrawDebugString(GetWorld(), NormalEnd + FVector(0, 0, 10),
            FString::Printf(TEXT("법선: %.1fcm/s"), NormalComponent.Size()),
            nullptr, FColor::Red, 5.0f, false, 0.8f);
    }

    // 접선 성분 (지면에 평행한 속도) - 노란색
    if (!TangentialComponent.IsNearlyZero())
    {
        FVector TangentialEnd = HitLocation + (TangentialComponent * VelocityScale);
        DrawDebugLine(GetWorld(), HitLocation, TangentialEnd, FColor::Yellow, false, 5.0f, 0, 2.5f);
        DrawDebugSphere(GetWorld(), TangentialEnd, 1.5f, 6, FColor::Yellow, false, 5.0f);

        // 접선 성분 정보
        DrawDebugString(GetWorld(), TangentialEnd + FVector(0, 0, 10),
            FString::Printf(TEXT("접선: %.1fcm/s"), TangentialComponent.Size()),
            nullptr, FColor::Yellow, 5.0f, false, 0.8f);
    }

    // 총 속도 정보
    DrawDebugString(GetWorld(), TotalVelocityEnd + FVector(0, 0, 10),
        FString::Printf(TEXT("총속도: %.1fcm/s"), Velocity.Size()),
        nullptr, FColor::Magenta, 5.0f, false, 0.8f);
}

// 추가 함수: 속도별 색상 코드 표시
void AGolfBall::DrawSpeedColorLegend(const FVector& BaseLocation)
{
    if (!GetWorld()) return;

    FVector LegendStart = BaseLocation + FVector(100, 0, 50); // 충돌지점에서 1m 옆, 50cm 위

    // ⭐ 수정: TArray 초기화 리스트 대신 개별 추가 방식 사용
    TArray<TPair<FString, FColor>> SpeedColors;
    SpeedColors.Add(TPair<FString, FColor>(TEXT("0-2 m/s: 매우 느림"), FColor::Blue));
    SpeedColors.Add(TPair<FString, FColor>(TEXT("2-5 m/s: 느림"), FColor::Green));
    SpeedColors.Add(TPair<FString, FColor>(TEXT("5-10 m/s: 보통"), FColor::Yellow));
    SpeedColors.Add(TPair<FString, FColor>(TEXT("10-15 m/s: 빠름"), FColor::Orange));
    SpeedColors.Add(TPair<FString, FColor>(TEXT("15+ m/s: 매우 빠름"), FColor::Red));

    for (int32 i = 0; i < SpeedColors.Num(); i++)
    {
        FVector LegendPos = LegendStart + FVector(0, 0, -i * 15.0f); // 15cm 간격

        // 색상 구체
        DrawDebugSphere(GetWorld(), LegendPos, 3.0f, 8, SpeedColors[i].Value, false, 5.0f);

        // 텍스트
        DrawDebugString(GetWorld(), LegendPos + FVector(10, 0, 0),
            SpeedColors[i].Key, nullptr, FColor::White, 5.0f, false, 0.7f);
    }

    // 범례 제목
    DrawDebugString(GetWorld(), LegendStart + FVector(0, 0, 20),
        TEXT("=== 속도 범례 ==="), nullptr, FColor::Cyan, 5.0f, false, 1.0f);
}

// 새로운 함수 추가
void AGolfBall::ResetToDefaultPhysicalMaterial()
{
    // 1. PhysicalMaterial 생성 및 JSON 값 적용

    if (BallMesh && IsValid(DefaultPhysicalMaterial))
    {
        BallMesh->SetPhysMaterialOverride(DefaultPhysicalMaterial);
        UE_LOG(LogTemp, Log, TEXT("🔄 Ball PhysMat reset to default (Friction=%.2f, Restitution=%.2f)"),
            DefaultPhysicalMaterial->Friction, DefaultPhysicalMaterial->Restitution);
    }


    UE_LOG(LogTemp, Log, TEXT("🔄 Reset to default physical material"));
}


void AGolfBall::HandlePenaltyDrop()
{
    UE_LOG(LogTemp, Warning, TEXT("🚨 벌타 드롭 처리 시작"));

    if (CheckTeeShot())
    {
        UE_LOG(LogTemp, Warning, TEXT("티샷 벌타 드롭 시도, 금지"));
        return;
    }

    // 드롭 위치 계산
    FVector DropPosition = CalculatePenaltyDropPosition();
    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    AGolfPlayerController* PC = Cast<AGolfPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
    if (DropPosition != FVector::ZeroVector)
    {
        UBallDropMarkerLibrary::UpdateDropBillboardMarker(GM->DropMarker, DropPosition);
        LinkedCameraManager->ChangeCameraMode(ECameraMode::Stop);
        GM->ReadyBillboard->Billboard->SetVisibility(false);

        AGolfPlayer* OwningPlayer = GM->FindPlayer(OwningPlayerIndex);
        OwningPlayer->SetPlayerState(EPlayerState::Player_Init);

        // 3초 후 드롭 실행 (애니메이션/이펙트 시간 확보)
        FTimerHandle DropTimer;
        GetWorldTimerManager().SetTimer(DropTimer, [this, DropPosition, GameMode, PC]()
            {
                ExecutePenaltyDrop(DropPosition);
                IncrementOwningPlayerShotCount(); // 1벌타 추가

                LinkedCameraManager->ChangeCameraMode(ECameraMode::Ready);

                AGolfPlayer* OwningPlayer = GM->FindPlayer(OwningPlayerIndex);
                OwningPlayer->SetPlayerState(EPlayerState::Player_Ready);

                // 드롭 이펙트 재생
               // VisualizePenaltyDrop(DropPosition);

                UE_LOG(LogTemp, Log, TEXT("✅ 벌타 드롭 완료: %s"), *DropPosition.ToString());

                if (auto* SM = GetGameInstance()->GetSubsystem<USoundManager>())
                {
                    SM->PlayAtLocation_ById("Effect.Ball.Drop", DropPosition, 1.0f);
                }
                if (PC->bTerrainGridVisible)
                    PC->ToggleTerrainGrid();
                GameMode->GetCurrentSlot()->UpdateStroke(GameMode->FindPlayer(OwningPlayerIndex)->PlayerInfo);
                UBallDropMarkerLibrary::UpdateDropBillboardMarker(GM->DropMarker, FVector::ZeroVector);

            }, 1.5f, false);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ 벌타 드롭 위치 계산 실패"));

        // 실패 시 기본 티박스로 돌아가기
        if (GameMode->MapInfo.TeePositions.IsValidIndex(GameMode->CurrentHole - 1))
        {
            FVector TeePos = GameMode->MapInfo.TeePositions[GameMode->CurrentHole - 1];
            ExecutePenaltyDrop(TeePos + FVector(0, 0, 50));
            IncrementOwningPlayerShotCount(); // 1벌타 추가
            if (PC->bTerrainGridVisible)
                PC->ToggleTerrainGrid();
            GameMode->GetCurrentSlot()->UpdateStroke(GameMode->FindPlayer(OwningPlayerIndex)->PlayerInfo);
        }
    }


}

FVector AGolfBall::GetCurrentHolePosition() const
{
    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    if (!GameMode)
    {
        UE_LOG(LogTemp, Warning, TEXT("GetCurrentHolePosition: GameMode is null"));
        return FVector::ZeroVector;
    }

    int32 CurrentHoleIndex = GameMode->CurrentHole - 1;
    if (!GameMode->MapInfo.HolecupPositions.IsValidIndex(CurrentHoleIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("GetCurrentHolePosition: Invalid hole index %d"), CurrentHoleIndex);
        return FVector::ZeroVector;
    }

    return GameMode->MapInfo.HolecupPositions[CurrentHoleIndex];
}

// 볼에서 홀컵까지의 방향벡터 (3D)
FVector AGolfBall::GetDirectionToHole() const
{
    FVector HolePosition = GetCurrentHolePosition();
    if (HolePosition == FVector::ZeroVector)
    {
        return FVector::ZeroVector;
    }

    FVector BallPosition = GetActorLocation();
    return HolePosition - BallPosition;
}


// 수평면에서의 방향벡터 (Z축 제외)
FVector AGolfBall::GetDirectionToHole2D() const
{
    FVector Direction = GetDirectionToHole();
    Direction.Z = 0.0f; // Z축 성분 제거

    if (Direction.IsNearlyZero())
    {
        return FVector::ForwardVector; // 기본값
    }

    return Direction.GetSafeNormal();
}



FVector AGolfBall::CalculatePenaltyDropPosition() const
{
    return CalculatePenaltyDropPositionInternal(true); // useHoleDirectionPriority = true
}

FVector AGolfBall::CalculatePenaltyDropPositionInternal(bool bUseHoleDirectionPriority) const
{
    FVector CurrentPosition = GetActorLocation();

    // =========================================================================
    // [공통] TourSpline 방향 벡터 사전 계산
    //   - InGameMode->TourActor->GetTourSpline() 에서 볼 위치에 가장 가까운
    //     스플라인 포인트를 찾고, 볼→스플라인 방향 벡터(ToSplineDir)를 구한다.
    //   - 이후 PriorityDirections 를 구성할 때,
    //     Dot(candidate, ToSplineDir) >= 0 인 방향만 포함시켜
    //     "페어웨이(코스) 쪽 방향"으로만 드롭 위치를 탐색한다.
    // =========================================================================
    FVector ToSplineDir = FVector::ZeroVector;   // 스플라인 방향 (유효하면 채워짐)
    bool    bHasSplineDir = false;

    if (AInGameMode* SplineGM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
    {
        if (SplineGM->TourActor)
        {
            if (USplineComponent* Spline = SplineGM->TourActor->GetTourSpline())
            {
                // 볼 위치에서 가장 가까운 스플라인 입력 키
                const float ClosestKey =
                    Spline->FindInputKeyClosestToWorldLocation(CurrentPosition);

                // 해당 포인트의 월드 위치
                const FVector ClosestPt =
                    Spline->GetLocationAtSplineInputKey(ClosestKey, ESplineCoordinateSpace::World);

                // 볼 → 스플라인 포인트 방향 (XY 평면, 정규화)
                FVector Delta = ClosestPt - CurrentPosition;
                Delta.Z = 0.0f;

                if (!Delta.IsNearlyZero())
                {
                    ToSplineDir = Delta.GetSafeNormal();
                    bHasSplineDir = true;

                    UE_LOG(LogTemp, Log,
                        TEXT("🔵 [PenaltyDrop] TourSpline 방향 계산 완료: ClosestPt=%s, ToSplineDir=%s"),
                        *ClosestPt.ToString(), *ToSplineDir.ToString());
                }
                else
                {
                    UE_LOG(LogTemp, Warning,
                        TEXT("⚠️ [PenaltyDrop] 볼이 스플라인 위에 있어 방향 계산 불가 → 방향 필터 없이 탐색"));
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("⚠️ [PenaltyDrop] TourSpline이 nullptr → 방향 필터 없이 탐색"));
            }
        }
    }

    // =========================================================================
    // 후보 방향 리스트에서 스플라인 방향 쪽 방향만 선택하는 람다
    //   Dot(dir, ToSplineDir) >= -DotThreshold 이면 "스플라인 방향 쪽" 으로 간주
    //   bHasSplineDir 가 false 이면 필터 없이 모두 허용
    // =========================================================================
    const float DotThreshold = -0.1f;  // 약 ±96도 이내 → 거의 반대쪽만 제외
    auto IsSplineSideDir = [&](const FVector& Dir) -> bool
        {
            if (!bHasSplineDir) return true;  // 스플라인 정보 없으면 모든 방향 허용
            return FVector::DotProduct(Dir, ToSplineDir) >= DotThreshold;
        };

    // =========================================================================
    // 홀컵 방향 우선 드롭 탐색
    // =========================================================================
    if (bUseHoleDirectionPriority)
    {
        // DirectionToHole: 홀컵 반대 방향 (볼이 날아온 방향 근처)
        FVector DirectionToHole = -GetDirectionToHole2D();

        UE_LOG(LogTemp, Log, TEXT("🏌️ PenaltyDrop: Ball=%s, HoleOppDir=%s"),
            *CurrentPosition.ToString(), *DirectionToHole.ToString());

        if (!DirectionToHole.IsNearlyZero())
        {
            // --- 후보 방향 생성 (기존과 동일한 각도 우선순위) ---
            TArray<FVector> AllCandidates;
            AllCandidates.Add(DirectionToHole.RotateAngleAxis(90.0f, FVector::UpVector)); // ±90 (측면)
            AllCandidates.Add(DirectionToHole.RotateAngleAxis(-90.0f, FVector::UpVector));
            AllCandidates.Add(DirectionToHole.RotateAngleAxis(60.0f, FVector::UpVector)); // ±60
            AllCandidates.Add(DirectionToHole.RotateAngleAxis(-60.0f, FVector::UpVector));
            AllCandidates.Add(DirectionToHole.RotateAngleAxis(30.0f, FVector::UpVector)); // ±30
            AllCandidates.Add(DirectionToHole.RotateAngleAxis(-30.0f, FVector::UpVector));
            AllCandidates.Add(DirectionToHole);                                            //   0 (정방향)

            // --- 스플라인 방향 필터 적용 ---
            TArray<FVector> PriorityDirections;
            TArray<FVector> FallbackDirections; // 필터 탈락 방향 (2차 탐색용)

            for (const FVector& Candidate : AllCandidates)
            {
                if (IsSplineSideDir(Candidate))
                {
                    PriorityDirections.Add(Candidate);
                }
                else
                {
                    FallbackDirections.Add(Candidate);
                    UE_LOG(LogTemp, Verbose,
                        TEXT("   🚫 스플라인 반대쪽 방향 제외: %s (Dot=%.2f)"),
                        *Candidate.ToString(),
                        FVector::DotProduct(Candidate, ToSplineDir));
                }
            }

            UE_LOG(LogTemp, Log,
                TEXT("   방향 필터 결과: 통과=%d개 / 제외=%d개"),
                PriorityDirections.Num(), FallbackDirections.Num());

            // --- 1차: 스플라인 방향 쪽 탐색 ---
            const TArray<float> SearchDistances = { 100.0f, 150.0f, 200.0f };

            for (const FVector& Dir : PriorityDirections)
            {
                for (float Dist : SearchDistances)
                {
                    FVector GroundPos = GetGroundLevelPosition(CurrentPosition + Dir * Dist);
                    if (IsPositionSafe(GroundPos))
                    {
                        const float Dot = FMath::Clamp(
                            FVector::DotProduct(DirectionToHole, Dir), -1.0f, 1.0f);

                        UE_LOG(LogTemp, Log,
                            TEXT("✅ [스플라인 방향] 드롭 위치 발견: %s (각도=%.0f°, 거리=%.0fcm)"),
                            *GroundPos.ToString(),
                            FMath::RadiansToDegrees(FMath::Acos(Dot)),
                            Dist);
                        UE_LOG(LogTemp, Log, TEXT("📍 홀컵까지: %.1fm"),
                            FVector::Dist(GroundPos, GetCurrentHolePosition()) / 100.0f);

                        return GroundPos;
                    }
                }
            }

            UE_LOG(LogTemp, Warning,
                TEXT("⚠️ 스플라인 방향 쪽에서 드롭 위치 못 찾음 → 반대쪽 방향(fallback) 탐색"));

            // --- 2차: 스플라인 반대쪽 방향 (부득이한 경우) ---
            for (const FVector& Dir : FallbackDirections)
            {
                for (float Dist : SearchDistances)
                {
                    FVector GroundPos = GetGroundLevelPosition(CurrentPosition + Dir * Dist);
                    if (IsPositionSafe(GroundPos))
                    {
                        UE_LOG(LogTemp, Warning,
                            TEXT("⚠️ [반대쪽 fallback] 드롭 위치 발견: %s (거리=%.0fcm)"),
                            *GroundPos.ToString(), Dist);
                        return GroundPos;
                    }
                }
            }

            UE_LOG(LogTemp, Warning,
                TEXT("⚠️ 홀컵 방향 탐색 전체 실패 → 동거리 fallback 사용"));
        }
        else
        {
            UE_LOG(LogTemp, Warning,
                TEXT("⚠️ 홀컵 방향 결정 불가 → 동거리 fallback 사용"));
        }
    }

    // ===== FALLBACK: 동거리 위치 찾기 =====
    return FindSafePositionAtEqualDistance();
}

FVector AGolfBall::FindSafePositionAtEqualDistance() const
{
    FVector CurrentPosition = GetActorLocation();
    FVector HolePosition = GetCurrentHolePosition();

    // 홀컵까지의 거리 계산 (드롭할 위치는 같은 거리 유지)
    float DistanceToHole = FVector::Dist(CurrentPosition, HolePosition);

    UE_LOG(LogTemp, Log, TEXT("🔄 동거리 드롭 위치 탐색: 홀컵까지 %.1fm"),
        DistanceToHole / 100.0f);

    // 현재 위치에서 홀컵으로의 방향
    FVector DeltaToHole = (HolePosition - CurrentPosition).GetSafeNormal();

    // 사거리 방향: 홀컵과의 방향에 수직인 축을 중심으로 180도 회전
    FVector AwayDirection = -DeltaToHole;

    // 여러 각도로 원형 탐색 (홀컵 반대쪽 방향 우선)
    TArray<float> SearchAngles;

    // 1순위: 정확히 반대 방향 (180도)
    SearchAngles.Add(180.0f);

    // 2순위: 좌우 (±135도)
    SearchAngles.Add(135.0f);
    SearchAngles.Add(-135.0f);

    // 3순위: 좌우 (±90도)
    SearchAngles.Add(90.0f);
    SearchAngles.Add(-90.0f);

    // 4순위: 좌우 (±45도)
    SearchAngles.Add(45.0f);
    SearchAngles.Add(-45.0f);

    // 각 각도에서 거리 우선으로 탐색
    TArray<float> SearchDistances = { 0.0f, 50.0f, 100.0f, 150.0f, 200.0f, 250.0f };

    for (float Angle : SearchAngles)
    {
        FVector SearchDirection = AwayDirection.RotateAngleAxis(Angle, FVector::UpVector);

        for (float DistanceOffset : SearchDistances)
        {
            // 홀컵과의 거리 ± DistanceOffset 범위 내에서 탐색
            float TargetDistance = DistanceToHole + DistanceOffset;
            if (TargetDistance < 0.0f) continue; // 음수 거리는 무시

            FVector TestPosition = CurrentPosition + (SearchDirection * DistanceOffset);
            FVector GroundPosition = GetGroundLevelPosition(TestPosition);

            if (IsPositionSafe(GroundPosition))
            {
                float ActualDistanceToHole = FVector::Dist(GroundPosition, HolePosition);

                UE_LOG(LogTemp, Log, TEXT("✅ 동거리 드롭 위치 발견: %s (각도: %.0f°, 오프셋: %.0fcm)"),
                    *GroundPosition.ToString(),
                    Angle,
                    DistanceOffset);

                UE_LOG(LogTemp, Log, TEXT("📊 원래 거리: %.1fm → 새 거리: %.1fm"),
                    DistanceToHole / 100.0f,
                    ActualDistanceToHole / 100.0f);

                return GroundPosition;
            }
        }
    }

    // ===== 최후의 fallback: 기존 안전 위치 함수 사용 =====
    UE_LOG(LogTemp, Error, TEXT("❌ 동거리 위치도 찾지 못함, 기본 안전 위치 사용"));
    return GetSafeDropPositionDefault();
}

// 기존 로직 (완전히 새로운 구현)
FVector AGolfBall::GetSafeDropPositionDefault() const
{
    FVector CurrentPosition = GetActorLocation();
    FVector TestPosition = CurrentPosition + (FVector::UpVector * 50.0f);

    // 가장 가까운 안전한 위치를 radial search로 찾기
    for (float Radius = 0.0f; Radius <= 500.0f; Radius += 50.0f)
    {
        for (float AngleDeg = 0.0f; AngleDeg < 360.0f; AngleDeg += 45.0f)
        {
            float AngleRad = FMath::DegreesToRadians(AngleDeg);
            FVector Offset(
                FMath::Cos(AngleRad) * Radius,
                FMath::Sin(AngleRad) * Radius,
                0.0f
            );

            FVector SearchPos = CurrentPosition + Offset;
            FVector GroundPosition = GetGroundLevelPosition(SearchPos);

            if (IsPositionSafe(GroundPosition))
            {
                UE_LOG(LogTemp, Log, TEXT("🟢 기본 드롭 위치 발견: %s (반경: %.0fcm)"),
                    *GroundPosition.ToString(), Radius);
                return GroundPosition;
            }
        }
    }

    // 진짜 마지막 수단: 그냥 현재 위치의 지형 높이만 조정
    UE_LOG(LogTemp, Warning, TEXT("⚠️ 안전한 드롭 위치를 찾을 수 없음, 현재 위치에서 높이만 조정"));
    FVector FallbackPosition = GetGroundLevelPosition(CurrentPosition);
    return FallbackPosition;
}


FVector AGolfBall::FindSafeDropPosition(const FVector& CurrentPosition, const TArray<FVector>& SearchDirections) const
{
    for (const FVector& Direction : SearchDirections)
    {
        // 여러 거리에서 시도
        for (float Distance = 50.0f; Distance <= PENALTY_DROP_SEARCH_RADIUS; Distance += 50.0f)
        {
            FVector TestPosition = CurrentPosition + (Direction * Distance);
            FVector GroundPosition = GetGroundLevelPosition(TestPosition);

            if (IsPositionSafe(GroundPosition))
            {
                UE_LOG(LogTemp, Log, TEXT("✅ 전체 검색에서 안전한 위치 발견: %s"), *GroundPosition.ToString());
                return GroundPosition;
            }
        }
    }

    UE_LOG(LogTemp, Error, TEXT("❌ 모든 방향에서 안전한 드롭 위치를 찾지 못함"));
    return FVector::ZeroVector;
}

bool AGolfBall::IsPositionSafe(const FVector& Position, float SafeRadius) const
{
    if (!GetWorld()) return false;

    // 1. OB 라인 안쪽 체크 (가장 중요한 조건)
    if (!IsPositionInBounds(Position))
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("❌ OB 라인 밖: %s"), *Position.ToString());
        return false;
    }

    // 2. 지면 체크 (유효한 지면이 있는지)
    FVector GroundCheckStart = Position + FVector(0, 0, 100.0f);
    FVector GroundCheckEnd = Position - FVector(0, 0, 100.0f);

    FHitResult GroundHit;
    FCollisionQueryParams GroundParams;
    GroundParams.AddIgnoredActor(this);
    GroundParams.bTraceComplex = true;

    if (!GetWorld()->LineTraceSingleByChannel(GroundHit, GroundCheckStart, GroundCheckEnd, ECC_WorldStatic, GroundParams))
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("❌ 지면 없음: %s"), *Position.ToString());
        return false;
    }

    // 3. 장애물 체크 (구체 충돌 검사)
    FCollisionShape SphereShape = FCollisionShape::MakeSphere(SafeRadius);
    FCollisionQueryParams ObstacleParams;
    ObstacleParams.AddIgnoredActor(this);
    ObstacleParams.bTraceComplex = false; // 성능상 단순 충돌 사용

    // 볼 위치에서 장애물 검사
    FVector CheckPosition = Position + FVector(0, 0, SafeRadius); // 볼 반지름만큼 위로

    if (GetWorld()->OverlapAnyTestByChannel(CheckPosition, FQuat::Identity, ECC_WorldStatic, SphereShape, ObstacleParams))
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("❌ 장애물 감지: %s"), *Position.ToString());
        return false;
    }

    // 4. 경사도 체크 (너무 가파른 경사면 제외)
    float SlopeAngle = FMath::RadiansToDegrees(
        FMath::Acos(FMath::Clamp(FVector::DotProduct(GroundHit.Normal, FVector::UpVector), 0.0f, 1.0f))
    );

    if (SlopeAngle > 30.0f) // 30도 이상 경사면은 제외
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("❌ 경사도 초과: %.1f도"), SlopeAngle);
        return false;
    }

    // 5. 물 체크 (LandscapeChecker 활용)
    if (LandscapeChecker && IsValid(LandscapeChecker))
    {
        ELandType LandType = LandscapeChecker->GetLandTypeAtLocation(Position);
        if (LandType == ELandType::Water)
        {
            UE_LOG(LogTemp, VeryVerbose, TEXT("❌ 물 위치: %s"), *Position.ToString());
            return false;
        }
    }

    // 6. OB 라인과의 안전 거리 체크 (추가 안전성)
    float DistanceToOBLine = GetDistanceToNearestOBLine(Position);
    if (DistanceToOBLine < 50.0f) // OB 라인에서 50cm 이상 떨어져야 함
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("❌ OB 라인 너무 근접: %.1fcm"), DistanceToOBLine);
        return false;
    }

    UE_LOG(LogTemp, VeryVerbose, TEXT("✅ 안전한 위치: %s (OB거리: %.1fcm)"), *Position.ToString(), DistanceToOBLine);
    return true;
}

FVector AGolfBall::GetGroundLevelPosition(const FVector& Position) const
{
    if (!GetWorld()) return Position;

    FVector Start = Position + FVector(0, 0, 100.0f);
    FVector End = Position - FVector(0, 0, 200.0f);

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    QueryParams.bTraceComplex = true;

    if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_WorldStatic, QueryParams))
    {
        return HitResult.Location + FVector(0, 0, UNREAL_SPHERE_RADIUS);
    }

    return Position; // 지면을 찾지 못하면 원래 위치 반환
}
bool AGolfBall::IsPositionInBounds(const FVector& Position) const
{
    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    if (!GameMode)
    {
        UE_LOG(LogTemp, Warning, TEXT("IsPositionInBounds: GameMode is null"));
        return false; // GameMode가 없으면 안전하지 않다고 가정
    }

    int32 CurrentHoleIndex = GameMode->CurrentHole - 1;
    if (!GameMode->MapInfo.OBLines.IsValidIndex(CurrentHoleIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("IsPositionInBounds: No OB lines for hole %d"), GameMode->CurrentHole);
        return true; // OB 라인이 없으면 모든 위치가 유효하다고 가정
    }

    const TArray<FVector>& OBPoints = GameMode->MapInfo.OBLines[CurrentHoleIndex].Points;

    if (OBPoints.Num() < 3)
    {
        UE_LOG(LogTemp, Warning, TEXT("IsPositionInBounds: Insufficient OB points (%d) for hole %d"),
            OBPoints.Num(), GameMode->CurrentHole);
        return true; // 유효하지 않은 OB 라인이면 모든 위치가 유효하다고 가정
    }

    FVector2D Pos2D(Position.X, Position.Y);

    // 기존의 IsPointOutsidePolygonImproved 함수 활용 (반대 결과 반환)
    return !IsPointOutsidePolygonImproved(Pos2D, OBPoints);
}


float AGolfBall::GetDistanceToNearestOBLine(const FVector& Position) const
{
    TArray<FVector> OBPoints = GetCurrentHoleOBPoints();

    if (OBPoints.Num() < 2)
    {
        return FLT_MAX; // OB 라인이 없으면 무한대 거리 반환
    }

    float MinDistance = FLT_MAX;

    // 모든 OB 라인 세그먼트에 대해 최단 거리 계산
    for (int32 i = 0; i < OBPoints.Num(); i++)
    {
        int32 NextIndex = (i + 1) % OBPoints.Num();

        float DistanceToSegment = CalculateDistanceToOBLineSegment(
            Position,
            OBPoints[i],
            OBPoints[NextIndex]
        );

        MinDistance = FMath::Min(MinDistance, DistanceToSegment);
    }

    return MinDistance;
}

TArray<FVector> AGolfBall::GetCurrentHoleOBPoints() const
{
    TArray<FVector> EmptyArray;

    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    if (!GameMode)
    {
        return EmptyArray;
    }

    int32 CurrentHoleIndex = GameMode->CurrentHole - 1;
    if (!GameMode->MapInfo.OBLines.IsValidIndex(CurrentHoleIndex))
    {
        return EmptyArray;
    }

    return GameMode->MapInfo.OBLines[CurrentHoleIndex].Points;
}

float AGolfBall::CalculateDistanceToOBLineSegment(const FVector& Point, const FVector& LineStart, const FVector& LineEnd) const
{
    // 3D 포인트를 2D로 변환 (Z축 무시)
    FVector2D P(Point.X, Point.Y);
    FVector2D A(LineStart.X, LineStart.Y);
    FVector2D B(LineEnd.X, LineEnd.Y);

    // 라인 벡터
    FVector2D AB = B - A;
    FVector2D AP = P - A;

    // 라인 길이의 제곱
    float ABLengthSquared = AB.SizeSquared();

    if (ABLengthSquared < KINDA_SMALL_NUMBER)
    {
        // 라인 세그먼트가 점에 가까우면 시작점까지의 거리 반환
        return FVector2D::Distance(P, A);
    }

    // 투영 비율 계산 (0~1 범위로 클램프)
    float t = FMath::Clamp(FVector2D::DotProduct(AP, AB) / ABLengthSquared, 0.0f, 1.0f);

    // 라인 세그먼트 상의 가장 가까운 점
    FVector2D ClosestPoint = A + t * AB;

    // 포인트에서 가장 가까운 점까지의 거리
    return FVector2D::Distance(P, ClosestPoint);
}


bool AGolfBall::CheckObstacleAtPosition(const FVector& Position, float CheckRadius) const
{
    if (!GetWorld()) return false;

    FCollisionShape SphereShape = FCollisionShape::MakeSphere(CheckRadius);
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    QueryParams.bTraceComplex = false;

    return GetWorld()->OverlapAnyTestByChannel(Position, FQuat::Identity, ECC_WorldStatic, SphereShape, QueryParams);
}

void AGolfBall::VisualizePenaltyDrop(const FVector& DropPosition) const
{
    if (!GetWorld()) return;

    // 드롭 위치에 이펙트 표시
    DrawDebugSphere(GetWorld(), DropPosition, 20.0f, 16, FColor::Orange, false, 10.0f, 0, 2.0f);

    // 안전 반경 표시
    DrawDebugCircle(GetWorld(), DropPosition, PENALTY_DROP_SAFE_RADIUS, 32, FColor::Green, false, 10.0f, 0, 1.0f,
        FVector(0, 1, 0), FVector(1, 0, 0));

    // OB 안전 거리 표시 (다른 색상으로)
    DrawDebugCircle(GetWorld(), DropPosition, PENALTY_DROP_OB_SAFETY_MARGIN, 16, FColor::Blue, false, 10.0f, 0, 1.0f,
        FVector(0, 1, 0), FVector(1, 0, 0));

    // 드롭 텍스트 표시
    DrawDebugString(GetWorld(), DropPosition + FVector(0, 0, 50.0f),
        TEXT("PENALTY DROP"), nullptr, FColor::Orange, 10.0f, false, 1.5f);

    // OB 라인까지의 거리 표시
    float DistanceToOB = GetDistanceToNearestOBLine(DropPosition);
    FString DistanceText = FString::Printf(TEXT("OB Distance: %.0fcm"), DistanceToOB);
    DrawDebugString(GetWorld(), DropPosition + FVector(0, 0, 30.0f),
        DistanceText, nullptr, FColor::Blue, 10.0f, false, 1.0f);

    // 원래 위치에서 드롭 위치로 점선 표시
    FVector CurrentPosition = GetActorLocation();
    int32 DashCount = 10;
    for (int32 i = 0; i < DashCount; i++)
    {
        if (i % 2 == 0) // 점선 효과
        {
            float Ratio1 = float(i) / DashCount;
            float Ratio2 = float(i + 1) / DashCount;
            FVector Start = FMath::Lerp(CurrentPosition, DropPosition, Ratio1);
            FVector End = FMath::Lerp(CurrentPosition, DropPosition, Ratio2);
            DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, 10.0f, 0, 2.0f);
        }
    }

    // 현재 홀의 OB 라인도 시각화 (참고용)
    TArray<FVector> OBPoints = GetCurrentHoleOBPoints();
    if (OBPoints.Num() > 2)
    {
        for (int32 i = 0; i < OBPoints.Num(); i++)
        {
            int32 NextIndex = (i + 1) % OBPoints.Num();
            FVector LineStart = OBPoints[i] + FVector(0, 0, 10.0f); // 지면에서 10cm 위
            FVector LineEnd = OBPoints[NextIndex] + FVector(0, 0, 10.0f);
            DrawDebugLine(GetWorld(), LineStart, LineEnd, FColor::Yellow, false, 10.0f, 0, 3.0f);
        }
    }
}

// 헬퍼 함수: 원형 검색 방향 생성
TArray<FVector> AGolfBall::GenerateCircularSearchDirections() const
{
    TArray<FVector> Directions;

    for (int32 i = 0; i < PENALTY_DROP_MAX_ATTEMPTS; i++)
    {
        float Angle = (float(i) / PENALTY_DROP_MAX_ATTEMPTS) * 360.0f;
        float RadianAngle = FMath::DegreesToRadians(Angle);

        FVector Direction = FVector(
            FMath::Cos(RadianAngle),
            FMath::Sin(RadianAngle),
            0.0f
        );

        Directions.Add(Direction);
    }

    return Directions;
}


FVector AGolfBall::GetStabilizedTerrainNormal() const
{
    if (!GetWorld())
        return FVector::UpVector;

    FVector BallLocation = GetActorLocation();
    float ActualBallRadius = GetActualBallRadius();

    // 기본 지면 트레이스
    FVector TraceStart = BallLocation + FVector(0, 0, ActualBallRadius);
    FVector TraceEnd = BallLocation - FVector(0, 0, ActualBallRadius + 20.0f);

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    QueryParams.bTraceComplex = true;

    if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
    {
        // Landscape 섹션 경계면 분석
        FLandscapeSectionBoundaryDetector::FLandscapeHitInfo HitInfo =
            FLandscapeSectionBoundaryDetector::AnalyzeLandscapeHit(HitResult, GetWorld(), true);

        // 디버그 시각화 (개발 중에만)
#if WITH_EDITOR
        //if (bShowGroundNormalDebug)
        //{
        //    FLandscapeSectionBoundaryDetector::DebugVisualizeSectionBoundary(GetWorld(), HitInfo, 2.0f);
        //}
#endif

        // 안정화된 노말 반환
        return HitInfo.Normal;
    }

    return FVector::UpVector;
}


// ⭐ 새로운 함수: 원래 노말과 안정화된 노말 비교 시각화
void AGolfBall::DrawNormalComparison(const FHitResult& Hit)
{
    if (!GetWorld()) return;

    FVector HitLocation = Hit.ImpactPoint;
    FVector OriginalNormal = Hit.Normal;
    FVector StabilizedNormal = GetStabilizedTerrainNormal();

    // 원래 노말 (빨간색)
    FVector OriginalEnd = HitLocation + (OriginalNormal * 40.0f);
    DrawDebugLine(GetWorld(), HitLocation, OriginalEnd, FColor::Red, false, 3.0f, 0, 2.0f);
    DrawDebugSphere(GetWorld(), OriginalEnd, 2.0f, 8, FColor::Red, false, 3.0f);

    // 안정화된 노말 (초록색)
    FVector StabilizedEnd = HitLocation + (StabilizedNormal * 45.0f);
    DrawDebugLine(GetWorld(), HitLocation, StabilizedEnd, FColor::Green, false, 3.0f, 0, 3.0f);
    DrawDebugSphere(GetWorld(), StabilizedEnd, 2.5f, 8, FColor::Green, false, 3.0f);

    // 각도 차이 계산 및 표시
    float AngleDifference = FMath::RadiansToDegrees(
        FMath::Acos(FMath::Clamp(
            FVector::DotProduct(OriginalNormal, StabilizedNormal),
            -1.0f, 1.0f
        ))
    );

    // 디버그 텍스트
    FString ComparisonText = FString::Printf(
        TEXT("Normal Comparison\n")
        TEXT("Original: (%.2f, %.2f, %.2f)\n")
        TEXT("Stabilized: (%.2f, %.2f, %.2f)\n")
        TEXT("Angle Diff: %.1f°"),
        OriginalNormal.X, OriginalNormal.Y, OriginalNormal.Z,
        StabilizedNormal.X, StabilizedNormal.Y, StabilizedNormal.Z,
        AngleDifference
    );

    DrawDebugString(GetWorld(), HitLocation + FVector(0, 0, 60.0f),
        ComparisonText, nullptr, FColor::White, 3.0f, false, 0.9f);

    // 범례
    DrawDebugString(GetWorld(), HitLocation + FVector(50, 0, 40),
        TEXT("Red: Original Normal"), nullptr, FColor::Red, 3.0f, false, 0.8f);
    DrawDebugString(GetWorld(), HitLocation + FVector(50, 0, 25),
        TEXT("Green: Stabilized Normal"), nullptr, FColor::Green, 3.0f, false, 0.8f);
}


void AGolfBall::HandleBoundaryBounceWithDirectionCorrection(const FHitResult& Hit)
{
    if (!BallMesh || CurrentBallState != EBallState::Ball_Bound)
    {
        return;
    }

    FVector CurrentVelocity = BallMesh->GetPhysicsLinearVelocity();
    float CurrentSpeed = CurrentVelocity.Size();

    UE_LOG(LogTemp, Log, TEXT("🔧 Boundary direction correction - Speed: %.1fcm/s"), CurrentSpeed);

    // 최소 속도 체크
    if (CurrentSpeed < 500.0f)
    {
        FVector HorizontalVelocity = FVector(CurrentVelocity.X, CurrentVelocity.Y, 0.0f);
        BallMesh->SetPhysicsLinearVelocity(HorizontalVelocity * 0.95f);
        SetBallState(EBallState::Ball_Rolling);
        UE_LOG(LogTemp, Log, TEXT("ParkGolf: Bound -> Rolling (HandleBoundaryBounceWithDirectionCorrection)"));
        return;
    }

    // ⭐ 개선: 더 강력한 수평 보정 적용
    FVector CorrectedVelocity = CalculateStrictHorizontalVelocity(CurrentVelocity, CurrentSpeed);

    // 안전성 체크 및 적용
    if (IS_VECTOR_VALID(CorrectedVelocity))
    {
        BallMesh->SetPhysicsLinearVelocity(CorrectedVelocity);
        UE_LOG(LogTemp, Log, TEXT("✅ Strict horizontal correction applied: %.1fcm/s"),
            CorrectedVelocity.Size());
    }
}

FVector AGolfBall::CalculateStrictHorizontalVelocity(const FVector& OriginalVelocity, float TargetSpeed)
{
    // 1. 현재 수평 방향 유지
    FVector HorizontalDirection = FVector(OriginalVelocity.X, OriginalVelocity.Y, 0.0f).GetSafeNormal();

    // 2. 방향이 유효하지 않으면 마지막 샷 방향 사용
    if (HorizontalDirection.IsNearlyZero() && !LastShotDirection.IsNearlyZero())
    {
        HorizontalDirection = FVector(LastShotDirection.X, LastShotDirection.Y, 0.0f).GetSafeNormal();
    }

    // 3. 여전히 유효하지 않으면 기본 방향 사용
    if (HorizontalDirection.IsNearlyZero())
    {
        HorizontalDirection = FVector::ForwardVector;
    }

    // 4. ⭐ 완전히 수평인 속도 생성 (Z축 절대 불허)
    float NewSpeed = TargetSpeed * 0.96f; // 4% 감속으로 안정화
    FVector StrictHorizontalVelocity = FVector(
        HorizontalDirection.X * NewSpeed,
        HorizontalDirection.Y * NewSpeed,
        0.0f // Z축 완전 제거
    );

    // 5. 지면 접촉을 위한 미세한 아래쪽 힘 (선택적)
    if (CurrentBallState == EBallState::Ball_Bound)
    {
        // 지면에 붙어서 굴러가도록 미세한 아래쪽 속도
        StrictHorizontalVelocity.Z = -1.0f; // -0.01m/s
    }

    UE_LOG(LogTemp, Log, TEXT("⬇️ Strict horizontal velocity: Direction=%s, Z=%.1f"),
        *HorizontalDirection.ToString(), StrictHorizontalVelocity.Z);

    return StrictHorizontalVelocity;
}

// ⭐ 새로운 함수: 전진방향으로 속도 벡터 보정 (위쪽 방향 문제 수정)
FVector AGolfBall::CalculateForwardCorrectedVelocity(
    const FVector& OriginalVelocity,
    const FVector& StabilizedNormal,
    float TargetSpeed)
{
    // 1. ⭐ 핵심 수정: 마지막 샷 방향을 우선 사용 (전진방향 유지)
    FVector ForwardDirection = FVector::ZeroVector;

    // 1-1. 마지막 샷 방향 사용 (가장 신뢰할 수 있는 전진방향)
    if (!LastShotDirection.IsNearlyZero())
    {
        ForwardDirection = FVector(LastShotDirection.X, LastShotDirection.Y, 0.0f).GetSafeNormal();
        UE_LOG(LogTemp, Log, TEXT("🎯 Using LastShotDirection: %s"), *ForwardDirection.ToString());
    }
    // 1-2. 차선책: 현재 속도의 수평 성분 사용
    else
    {
        FVector HorizontalVelocity = FVector(OriginalVelocity.X, OriginalVelocity.Y, 0.0f);
        if (!HorizontalVelocity.IsNearlyZero())
        {
            ForwardDirection = HorizontalVelocity.GetSafeNormal();
            UE_LOG(LogTemp, Warning, TEXT("⚠️ Using current velocity direction: %s"), *ForwardDirection.ToString());
        }
        else
        {
            // 최후 수단: 기본 전진방향
            ForwardDirection = FVector::ForwardVector;
            UE_LOG(LogTemp, Error, TEXT("❌ Using fallback ForwardVector"));
        }
    }

    // 2. ⭐ 핵심 수정: 수평면에서만 전진방향 계산
    // FVector ForwardDirection = HorizontalDirection; // 직접 사용 (지면 투영 생략) // ❌ Remove this line

    // 3. ⭐ 수직 성분 처리 개선
    float OriginalVerticalSpeed = OriginalVelocity.Z;
    float OriginalHorizontalSpeed = FVector(OriginalVelocity.X, OriginalVelocity.Y, 0.0f).Size();

    // 4. ⭐ 핵심: 수평 속도 우선, 수직 속도는 제한적으로만 사용
    float NewHorizontalSpeed = TargetSpeed * 0.90f; // 전체 속도의 90%를 수평으로
    float MaxVerticalSpeed = TargetSpeed * 0.30f;    // 전체 속도의 30%까지만 수직으로

    // 5. 새로운 수평 속도 벡터
    FVector NewHorizontalVelocity = ForwardDirection * NewHorizontalSpeed;

    // 6. ⭐ 수직 성분 강제 제한
    float NewVerticalSpeed = 0.0f;

    // Ball_Bound 상태에서는 약간의 수직 성분만 허용 (바운스 효과)
    if (CurrentBallState == EBallState::Ball_Bound && OriginalVerticalSpeed > 0.0f)
    {
        // 원래 수직 속도가 양수(위쪽)이면 약간만 유지, 음수면 0으로
        NewVerticalSpeed = FMath::Clamp(OriginalVerticalSpeed * 0.3f, 0.0f, MaxVerticalSpeed);
    }

    // 만약 원래 속도가 아래쪽이면 굴림으로 전환
    if (OriginalVerticalSpeed < -50.0f) // -0.5m/s 이하로 아래쪽
    {
        NewVerticalSpeed = 0.0f; // 완전히 수평으로
    }

    // 7. 최종 속도 벡터 (수평 위주)
    FVector CorrectedVelocity = NewHorizontalVelocity + FVector(0, 0, NewVerticalSpeed);

    // 8. ⭐ 안전 체크: Z 성분이 너무 크면 강제로 제한
    if (CorrectedVelocity.Z > MaxVerticalSpeed)
    {
        CorrectedVelocity.Z = MaxVerticalSpeed;
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Vertical speed clamped to %.1f"), MaxVerticalSpeed);
    }

    // 9. ⭐ 최종 안전장치: 속도가 위쪽으로 향하는지 체크
    float PitchAngle = FMath::RadiansToDegrees(FMath::Asin(CorrectedVelocity.Z / CorrectedVelocity.Size()));
    if (PitchAngle > 15.0f) // 15도 이상 위쪽이면 강제 보정
    {
        // 15도로 제한
        float MaxPitchRad = FMath::DegreesToRadians(15.0f);
        float HorizontalMagnitude = CorrectedVelocity.Size() * FMath::Cos(MaxPitchRad);
        float VerticalMagnitude = CorrectedVelocity.Size() * FMath::Sin(MaxPitchRad);

        CorrectedVelocity = ForwardDirection * HorizontalMagnitude + FVector(0, 0, VerticalMagnitude);

        UE_LOG(LogTemp, Warning, TEXT("⚠️ Pitch angle limited: %.1f° → 15°"), PitchAngle);
    }

    UE_LOG(LogTemp, Log, TEXT("🔧 Direction-corrected velocity: Speed=%.1f, Pitch=%.1f°, Direction=%s"),
        CorrectedVelocity.Size(),
        FMath::RadiansToDegrees(FMath::Asin(CorrectedVelocity.Z / CorrectedVelocity.Size())),
        *ForwardDirection.ToString());

    return CorrectedVelocity;
}

// ⭐ 새로운 함수: 지면에 정렬된 전진방향 계산 (위쪽 방향 방지)
FVector AGolfBall::CalculateGroundAlignedForwardDirection(
    const FVector& DesiredDirection,
    const FVector& GroundNormal)
{
    // 1. ⭐ 핵심 수정: 수평면에서만 작업 (Z축 무시)
    FVector HorizontalDesired = FVector(DesiredDirection.X, DesiredDirection.Y, 0.0f).GetSafeNormal();
    FVector HorizontalNormal = FVector(GroundNormal.X, GroundNormal.Y, 0.0f).GetSafeNormal();

    // 2. 기본적으로 원하는 수평 방향 사용
    FVector ProjectedDirection = HorizontalDesired;

    // 3. ⭐ 지면 경사만 고려 (수직 성분은 배제)
    float SlopeAngle = FMath::RadiansToDegrees(
        FMath::Acos(FMath::Clamp(FVector::DotProduct(GroundNormal, FVector::UpVector), 0.0f, 1.0f))
    );

    if (SlopeAngle > 2.0f) // 2도 이상 경사
    {
        // ⭐ 수정: 경사 방향도 수평면에서만 계산
        FVector SlopeDirection = GetGroundSlopeDirection();
        SlopeDirection.Z = 0.0f; // Z축 강제 제거
        SlopeDirection = SlopeDirection.GetSafeNormal();

        if (!SlopeDirection.IsNearlyZero())
        {
            // 경사 영향을 매우 약하게 적용 (10% 이하)
            float SlopeInfluence = FMath::Clamp(SlopeAngle / 45.0f, 0.0f, 0.1f); // 최대 10% 영향
            ProjectedDirection = FMath::Lerp(HorizontalDesired, SlopeDirection, SlopeInfluence).GetSafeNormal();
        }
    }

    // 4. ⭐ 최종 안전장치: Z 성분 완전 제거
    ProjectedDirection.Z = 0.0f;
    ProjectedDirection = ProjectedDirection.GetSafeNormal();

    // 5. 최종 검증
    if (ProjectedDirection.IsNearlyZero() || ProjectedDirection.ContainsNaN())
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Invalid projected direction, using horizontal desired"));
        return HorizontalDesired;
    }

    UE_LOG(LogTemp, Log, TEXT("🧭 Ground aligned direction: Input=%s, Output=%s (Z=%.3f)"),
        *DesiredDirection.ToString(), *ProjectedDirection.ToString(), ProjectedDirection.Z);

    return ProjectedDirection;
}

FVector AGolfBall::CalculateHorizontalCorrectedVelocity(const FVector& OriginalVelocity, float TargetSpeed)
{
    // 1. ⭐ 핵심: 마지막 샷 방향을 우선적으로 사용
    FVector ForwardDirection = FVector::ZeroVector;

    // 마지막 샷 방향이 있으면 그것을 사용 (가장 신뢰할 수 있음)
    if (!LastShotDirection.IsNearlyZero())
    {
        ForwardDirection = FVector(LastShotDirection.X, LastShotDirection.Y, 0.0f).GetSafeNormal();
        UE_LOG(LogTemp, Log, TEXT("🎯 Using LastShotDirection for boundary correction: %s"), *ForwardDirection.ToString());
    }
    else
    {
        // 현재 속도 방향 사용 (차선책)
        FVector HorizontalVel = FVector(OriginalVelocity.X, OriginalVelocity.Y, 0.0f);
        if (!HorizontalVel.IsNearlyZero())
        {
            ForwardDirection = HorizontalVel.GetSafeNormal();
            UE_LOG(LogTemp, Warning, TEXT("⚠️ Using original velocity direction: %s"), *ForwardDirection.ToString());
        }
        else
        {
            ForwardDirection = FVector::ForwardVector;
            UE_LOG(LogTemp, Error, TEXT("❌ Using fallback ForwardVector"));
        }
    }

    // 2. ⭐ 방향 검증: 뒤쪽으로 가는지 체크
    if (!LastShotDirection.IsNearlyZero())
    {
        FVector LastHorizontal = FVector(LastShotDirection.X, LastShotDirection.Y, 0.0f).GetSafeNormal();
        float Alignment = FVector::DotProduct(ForwardDirection, LastHorizontal);

        // 방향이 90도 이상 어긋나면 마지막 샷 방향 강제 사용
        if (Alignment < 0.0f)
        {
            ForwardDirection = LastHorizontal;
            UE_LOG(LogTemp, Warning, TEXT("⚠️ Direction corrected to LastShotDirection due to misalignment"));
        }
    }

    // 3. 속도 크기 결정 (95% 유지)
    float NewSpeed = TargetSpeed * 0.95f;

    // 4. 완전히 수평인 새 속도 생성
    FVector NewVelocity = ForwardDirection * NewSpeed;

    // 5. Ball_Bound 상태에서만 최소한의 바운스 추가
    if (CurrentBallState == EBallState::Ball_Bound && OriginalVelocity.Z > 0.0f)
    {
        // 5도 이하의 아주 작은 위쪽 각도만 허용
        float MaxBounceAngle = 5.0f;
        float MaxVerticalSpeed = NewSpeed * FMath::Sin(FMath::DegreesToRadians(MaxBounceAngle));
        float MinimalBounce = FMath::Min(OriginalVelocity.Z * 0.1f, MaxVerticalSpeed);
        NewVelocity.Z = MinimalBounce;
    }

    UE_LOG(LogTemp, Log, TEXT("✅ Horizontal correction complete: Speed=%.1f, Direction=%s, Z=%.1f"),
        NewVelocity.Size(), *ForwardDirection.ToString(), NewVelocity.Z);

    return NewVelocity;
}

void AGolfBall::DebugVelocityDirection(const FString& Context, const FVector& Velocity)
{
    if (Velocity.Size() < 1.0f) return;

    float PitchAngle = FMath::RadiansToDegrees(FMath::Asin(
        FMath::Clamp(Velocity.Z / Velocity.Size(), -1.0f, 1.0f)
    ));

    float YawAngle = FMath::RadiansToDegrees(FMath::Atan2(Velocity.Y, Velocity.X));

    FColor DebugColor = FColor::White;
    if (PitchAngle > 10.0f) DebugColor = FColor::Red;    // 위쪽
    else if (PitchAngle < -10.0f) DebugColor = FColor::Blue; // 아래쪽
    else DebugColor = FColor::Green; // 수평

    UE_LOG(LogTemp, Log, TEXT("🎯 %s: Speed=%.1f, Pitch=%.1f°, Yaw=%.1f°"),
        *Context, Velocity.Size(), PitchAngle, YawAngle);

    // 시각적 디버그 (에디터에서만)
    if (GetWorld())
    {
        FVector BallPos = GetActorLocation();
        FVector ArrowEnd = BallPos + (Velocity.GetSafeNormal() * 100.0f);

        DrawDebugDirectionalArrow(GetWorld(), BallPos, ArrowEnd, 20.0f, DebugColor,
            false, 2.0f, 0, 3.0f);

        DrawDebugString(GetWorld(), BallPos + FVector(0, 0, 30.0f),
            FString::Printf(TEXT("%s: %.1f°"), *Context, PitchAngle),
            nullptr, DebugColor, 2.0f, false, 1.0f);
    }
}


// ⭐ 새로 추가: 굴림 상태 진입 처리 (서서히 멈추는 기능 추가)
void AGolfBall::HandleRollingStateEnter()
{
    if (!IsValid(this) || !GetWorld()) return;

    // 기존 굴림 타이머가 있다면 클리어
    GetWorld()->GetTimerManager().ClearTimer(MaxRollingTimer);
    GetWorld()->GetTimerManager().ClearTimer(GradualStopTimer); // 새 타이머도 클리어

    // 점진적 멈춤 변수 초기화
    bIsGraduallystopping = false;
    GradualStopStartTime = 0.0f;
    GradualStopDuration = 3.0f; // 3초에 걸쳐 서서히 멈춤
    InitialRollingSpeed = 0.0f;

    // 최대 굴림 시간 타이머 설정
    FTimerDelegate RollingTimeoutDelegate;
    RollingTimeoutDelegate.BindWeakLambda(this, [this]()
        {
            if (!IsValid(this) || bIsBeingDestroyed)
            {
                UE_LOG(LogTemp, Warning, TEXT("Rolling timeout: Object invalid, skipping"));
                return;
            }

            if (!BallMesh || !IsValid(BallMesh))
            {
                UE_LOG(LogTemp, Error, TEXT("Rolling timeout: BallMesh invalid"));
                return;
            }

            UE_LOG(LogTemp, Warning, TEXT("Max rolling time reached, starting gradual stop"));
            StartGradualStop();
        });

    GetWorld()->GetTimerManager().SetTimer(
        MaxRollingTimer,
        RollingTimeoutDelegate,
        MaxRollingDuration,
        false
    );

    UE_LOG(LogTemp, Log, TEXT("Rolling state entered, timeout set for %.1fs"), MaxRollingDuration);
}

// ⭐ 새로 추가: 점진적 정지 시작
void AGolfBall::StartGradualStop()
{
    if (!BallMesh || !IsValid(BallMesh) || bIsGraduallyStop) return;

    // 현재 속도 저장
    FVector CurrentVelocity = BallMesh->GetPhysicsLinearVelocity();
    InitialRollingSpeed = CurrentVelocity.Size();

    if (InitialRollingSpeed < 50.0f) // 이미 충분히 느리면 즉시 정지
    {
        ForceStopBall();
        return;
    }

    // 점진적 정지 시작
    bIsGraduallyStop = true;
    GradualStopStartTime = GetWorld()->GetTimeSeconds();

    UE_LOG(LogTemp, Log, TEXT("Starting gradual stop from %.1fcm/s over %.1fs"),
        InitialRollingSpeed, GradualStopDuration);

    // 점진적 정지 타이머 설정 (0.1초마다 업데이트)
    FTimerDelegate GradualStopDelegate;
    GradualStopDelegate.BindWeakLambda(this, [this]()
        {
            UpdateGradualStop();
        });

    GetWorld()->GetTimerManager().SetTimer(
        GradualStopTimer,
        GradualStopDelegate,
        0.1f, // 0.1초마다 업데이트
        true  // 반복
    );
}

// ⭐ 새로 추가: 점진적 정지 업데이트
void AGolfBall::UpdateGradualStop()
{
    if (!IsValid(this) || !BallMesh || !IsValid(BallMesh) || !bIsGraduallyStop)
    {
        GetWorld()->GetTimerManager().ClearTimer(GradualStopTimer);
        return;
    }

    float CurrentTime = GetWorld()->GetTimeSeconds();
    float ElapsedTime = CurrentTime - GradualStopStartTime;

    // 진행률 계산 (0.0 ~ 1.0)
    float Progress = FMath::Clamp(ElapsedTime / GradualStopDuration, 0.0f, 1.0f);

    // 점진적 감속을 위한 곡선 적용 (EaseOut 효과)
    float EaseOutProgress = 1.0f - FMath::Pow(1.0f - Progress, 3.0f);

    // 목표 속도 계산 (초기 속도에서 0으로 감소)
    float TargetSpeed = InitialRollingSpeed * (1.0f - EaseOutProgress);

    // 현재 속도와 방향 가져오기
    FVector CurrentVelocity = BallMesh->GetPhysicsLinearVelocity();
    FVector HorizontalVelocity = FVector(CurrentVelocity.X, CurrentVelocity.Y, 0.0f);

    if (!HorizontalVelocity.IsNearlyZero())
    {
        // 방향 유지하면서 속도만 조절
        FVector NewVelocity = HorizontalVelocity.GetSafeNormal() * TargetSpeed;
        NewVelocity.Z = 0.0f; // Z축은 항상 0

        try
        {
            BallMesh->SetPhysicsLinearVelocity(NewVelocity);
        }
        catch (...)
        {
            UE_LOG(LogTemp, Error, TEXT("Exception during gradual stop velocity update"));
        }

        UE_LOG(LogTemp, VeryVerbose, TEXT("Gradual stop: Progress=%.1f%%, Speed=%.1f->%.1fcm/s"),
            Progress * 100.0f, CurrentVelocity.Size(), TargetSpeed);
    }

    // 완료 체크
    if (Progress >= 1.0f || TargetSpeed < 10.0f) // 진행률 100% 또는 속도가 10cm/s 이하
    {
        CompleteGradualStop();
    }
}

// ⭐ 새로 추가: 점진적 정지 완료
void AGolfBall::CompleteGradualStop()
{
    UE_LOG(LogTemp, Log, TEXT("Gradual stop completed"));

    // 타이머 정리
    GetWorld()->GetTimerManager().ClearTimer(GradualStopTimer);

    // 완전 정지
    ForceStopBall();

    // 상태 변경
    if (CurrentBallState == EBallState::Ball_Rolling)
    {
        SetBallState(EBallState::Ball_Stop);
    }

    // 플래그 리셋
    bIsGraduallyStop = false;
}

// ⭐ 새로 추가: 강제 정지 (기존 코드 분리)
void AGolfBall::ForceStopBall()
{
    if (!BallMesh || !IsValid(BallMesh)) return;

    try
    {
        BallMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
        BallMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
        UE_LOG(LogTemp, Log, TEXT("Ball force stopped"));
    }
    catch (...)
    {
        UE_LOG(LogTemp, Error, TEXT("Exception during force stop"));
    }
}

// ⭐ 새로 추가: 모든 상태 관련 타이머 안전하게 정리 (수정)
void AGolfBall::ClearAllStateTimers()
{
    if (!GetWorld()) return;

    FTimerManager& TimerManager = GetWorld()->GetTimerManager();

    // 개별 타이머들 정리
    if (TimerManager.IsTimerActive(MaxRollingTimer))
    {
        TimerManager.ClearTimer(MaxRollingTimer);
    }

    if (TimerManager.IsTimerActive(GradualStopTimer))
    {
        TimerManager.ClearTimer(GradualStopTimer);
    }

    // 점진적 정지 상태 리셋
    bIsGraduallyStop = false;

    UE_LOG(LogTemp, VeryVerbose, TEXT("State timers cleared"));
}


// 새로 추가: 랜드스케이프 위치 찾기
FVector AGolfBall::FindLandscapePosition(const FVector& SearchPosition, float BallRadius) const
{
    if (!GetWorld()) return FVector::ZeroVector;

    // ⭐ 수정: 더 넓은 범위에서 트레이스
    FVector TraceStart = SearchPosition + FVector(0, 0, 200.0f); // 20m 위에서 시작
    FVector TraceEnd = SearchPosition - FVector(0, 0, 300.0f);   // 30m 아래까지

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    Params.bTraceComplex = true;
    Params.bReturnPhysicalMaterial = true; // ⭐ 추가: PhysicalMaterial 정보도 가져오기

    TArray<FHitResult> HitResults;
    if (GetWorld()->LineTraceMultiByChannel(HitResults, TraceStart, TraceEnd, ECC_WorldStatic, Params))
    {
        // ⭐ 개선: 랜드스케이프를 더 정확하게 찾기
        for (const FHitResult& Hit : HitResults)
        {
            if (!IsValid(Hit.GetActor())) continue;
            if (Cast<AGolfBall>(Hit.GetActor())) continue;

            if (IsGroundActor(Hit.GetActor()))
                return Hit.Location + (Hit.Normal * BallRadius);
        }

        // 2순위: 컴포넌트 이름으로 랜드스케이프 찾기
        for (const FHitResult& Hit : HitResults)
        {
            if (!IsValid(Hit.GetActor())) continue;

            if (Hit.Component.IsValid())
            {
                FString ComponentName = Hit.Component->GetName().ToLower();
                if (ComponentName.Contains(TEXT("landscape")))
                {
                    FVector LandscapePos = Hit.Location + (Hit.Normal * BallRadius);
                    UE_LOG(LogTemp, Log, TEXT("🌿 Found landscape component: %s at %s"),
                        *Hit.Component->GetName(), *LandscapePos.ToString());
                    return LandscapePos;
                }
            }
        }

        // 3순위: 액터 이름으로 지면 추정 (나무 제외)
        for (const FHitResult& Hit : HitResults)
        {
            if (!IsValid(Hit.GetActor())) continue;

            FString ActorName = Hit.GetActor()->GetName().ToLower();

            // ⭐ 나무나 장애물 제외
            if (ActorName.Contains(TEXT("tree")) ||
                ActorName.Contains(TEXT("rock")) ||
                ActorName.Contains(TEXT("prop")) ||
                ActorName.Contains(TEXT("foliage")) ||
                ActorName.Contains(TEXT("SM_")) ||
                ActorName.Contains(TEXT("sm_"))) // StaticMesh 프리팹들
            {
                UE_LOG(LogTemp, VeryVerbose, TEXT("⚠️ Skipping obstacle: %s"), *ActorName);
                continue;
            }

            // 지면으로 추정되는 액터 (landphysic 스태틱메시 포함)
            if (ActorName.Contains(TEXT("ground")) ||
                ActorName.Contains(TEXT("terrain")) ||
                ActorName.Contains(TEXT("floor")) ||
                ActorName.Contains(TEXT("landphysic")))
            {
                FVector GroundPos = Hit.Location + (Hit.Normal * BallRadius);
                UE_LOG(LogTemp, Log, TEXT("🏔️ Found ground-like: %s at %s"),
                    *Hit.GetActor()->GetName(), *GroundPos.ToString());
                return GroundPos;
            }
        }

        // ⭐ 4순위: PhysicalMaterial 체크 (지면 타입 확인)
        for (const FHitResult& Hit : HitResults)
        {
            if (!IsValid(Hit.GetActor())) continue;

            // 나무 등 제외
            FString ActorName = Hit.GetActor()->GetName().ToLower();
            if (ActorName.Contains(TEXT("tree")) ||
                ActorName.Contains(TEXT("foliage")) ||
                ActorName.Contains(TEXT("SM_")) ||
                ActorName.Contains(TEXT("prop")))
            {
                continue;
            }

            if (Hit.PhysMaterial.IsValid())
            {
                FString MatName = Hit.PhysMaterial->GetName().ToLower();

                // 지면 타입의 PhysicalMaterial 찾기
                if (MatName.Contains(TEXT("grass")) ||
                    MatName.Contains(TEXT("rough")) ||
                    MatName.Contains(TEXT("fairway")) ||
                    MatName.Contains(TEXT("green")) ||
                    MatName.Contains(TEXT("sand")) ||
                    MatName.Contains(TEXT("bunker")))
                {
                    FVector GroundPos = Hit.Location + (Hit.Normal * BallRadius);
                    UE_LOG(LogTemp, Log, TEXT("🌱 Found ground by PhysMat: %s at %s"),
                        *Hit.PhysMaterial->GetName(), *GroundPos.ToString());
                    return GroundPos;
                }
            }
        }

        // ⭐ 최종: 가장 낮은 위치 찾기 (기존 가장 높은 위치 대신)
        // 지면은 보통 아래쪽에 있음
        if (HitResults.Num() > 0)
        {
            const FHitResult* LowestHit = nullptr;
            float LowestZ = FLT_MAX;

            for (const FHitResult& Hit : HitResults)
            {
                if (!IsValid(Hit.GetActor())) continue;

                FString ActorName = Hit.GetActor()->GetName().ToLower();

                // 나무 등 명백한 장애물 제외
                if (ActorName.Contains(TEXT("tree")) ||
                    ActorName.Contains(TEXT("SM_")) ||
                    ActorName.Contains(TEXT("foliage")))
                {
                    continue;
                }

                // ⭐ 검색 위치보다 너무 높은 곳은 제외 (나무 위 방지)
                if (Hit.Location.Z > SearchPosition.Z + 150.0f) // 5m 이상 위는 제외
                {
                    UE_LOG(LogTemp, VeryVerbose, TEXT("⚠️ Too high, skipping: %s (Z: %.1f)"),
                        *ActorName, Hit.Location.Z);
                    continue;
                }

                if (Hit.Location.Z < LowestZ)
                {
                    LowestZ = Hit.Location.Z;
                    LowestHit = &Hit;
                }
            }

            if (LowestHit)
            {
                FVector LowestPos = LowestHit->Location + (LowestHit->Normal * BallRadius);
                UE_LOG(LogTemp, Warning, TEXT("⚠️ Using LOWEST hit as fallback: %s at %s"),
                    *LowestHit->GetActor()->GetName(), *LowestPos.ToString());
                return LowestPos;
            }
        }
    }

    UE_LOG(LogTemp, Error, TEXT("❌ No suitable ground found at search position: %s"),
        *SearchPosition.ToString());
    return FVector::ZeroVector;
}

// 새로 추가: 오브젝트 회피 위치 찾기
FVector AGolfBall::FindObstacleAvoidancePosition(const FVector& LandscapePosition, float BallRadius) const
{
    if (!GetWorld()) return LandscapePosition;

    const float SafeRadius = BallRadius * 2.5f; // 볼 반지름의 2.5배로 안전 거리 설정
    const float MaxAvoidanceDistance = 300.0f;  // 최대 3m까지 회피 시도

    // 초기 위치에서 오브젝트 충돌 검사
    if (!HasObstacleCollision(LandscapePosition, SafeRadius))
    {
        UE_LOG(LogTemp, Log, TEXT("✅ No obstacles at landscape position"));
        return LandscapePosition; // 충돌이 없으면 그대로 사용
    }

    UE_LOG(LogTemp, Warning, TEXT("⚠️ Obstacles detected, finding avoidance position"));

    // 여러 방향으로 회피 위치 탐색
    TArray<FVector> AvoidanceDirections;

    // 8방향 + 추가 16방향 = 24방향으로 탐색
    for (int32 i = 0; i < 24; i++)
    {
        float Angle = (float(i) / 24.0f) * 360.0f;
        float RadianAngle = FMath::DegreesToRadians(Angle);

        FVector Direction = FVector(
            FMath::Cos(RadianAngle),
            FMath::Sin(RadianAngle),
            0.0f
        );
        AvoidanceDirections.Add(Direction);
    }

    // 각 방향으로 회피 시도
    TArray<float> AvoidanceDistances = { 50.0f, 100.0f, 150.0f, 200.0f, 250.0f, 300.0f };

    for (float Distance : AvoidanceDistances)
    {
        for (const FVector& Direction : AvoidanceDirections)
        {
            FVector AvoidancePos = LandscapePosition + (Direction * Distance);

            // 새로운 위치에서 랜드스케이프 높이 재조정
            FVector AdjustedPos = AdjustToLandscapeHeight(AvoidancePos, BallRadius);

            if (AdjustedPos == FVector::ZeroVector)
            {
                continue; // 랜드스케이프를 찾지 못하면 다음 시도
            }

            // 오브젝트 충돌 검사
            if (!HasObstacleCollision(AdjustedPos, SafeRadius))
            {
                UE_LOG(LogTemp, Log, TEXT("✅ Found obstacle-free position: %s (Distance=%.1f)"),
                    *AdjustedPos.ToString(), Distance);
                return AdjustedPos;
            }
        }
    }

    UE_LOG(LogTemp, Error, TEXT("❌ Could not find obstacle-free position, returning original"));
    return LandscapePosition; // 회피 위치를 찾지 못하면 원래 위치 반환
}

// 새로 추가: 특정 위치에서 랜드스케이프 높이 조정
FVector AGolfBall::AdjustToLandscapeHeight(const FVector& Position, float BallRadius) const
{
    if (!GetWorld()) return FVector::ZeroVector;

    FVector TraceStart = Position + FVector(0, 0, 500.0f);
    FVector TraceEnd = Position - FVector(0, 0, 500.0f);

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    Params.bTraceComplex = true;

    TArray<FHitResult> HitResults;
    if (GetWorld()->LineTraceMultiByChannel(HitResults, TraceStart, TraceEnd, ECC_WorldStatic, Params))
    {
        // 랜드스케이프 우선 검색
        for (const FHitResult& Hit : HitResults)
        {
            if (!IsValid(Hit.GetActor())) continue;
            if (Cast<AGolfBall>(Hit.GetActor())) continue;

            if (IsGroundActor(Hit.GetActor()))
                return Hit.Location + (Hit.Normal * BallRadius);
        }

        // ✅ 지면 액터를 못 찾으면 "가장 높은 것"이 아니라
        //    검색 위치보다 아래에 있는 것 중 가장 높은 것을 선택
        //    (기존 로직은 네트/천막/타겟 위로 볼을 올려버림)
        const FHitResult* Best = nullptr;
        for (const FHitResult& Hit : HitResults)
        {
            if (!IsValid(Hit.GetActor())) continue;
            if (Cast<AGolfBall>(Hit.GetActor())) continue;
            if (Hit.Location.Z > Position.Z + 10.0f) continue;   // 볼보다 위는 지면이 아님
            if (!Best || Hit.Location.Z > Best->Location.Z) Best = &Hit;
        }
        if (Best)
        {
            UE_LOG(LogTemp, Warning, TEXT("⚠️ 지면 Tag 없음, 최상단 하부 히트 사용: %s"),
                *Best->GetActor()->GetName());
            return Best->Location + (Best->Normal * BallRadius);
        }
    }

    return FVector::ZeroVector;
}

// 새로 추가: 오브젝트 충돌 검사
bool AGolfBall::HasObstacleCollision(const FVector& Position, float CheckRadius) const
{
    if (!GetWorld()) return false;

    FCollisionShape SphereShape = FCollisionShape::MakeSphere(CheckRadius);
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    QueryParams.bTraceComplex = false;

    TArray<FOverlapResult> OverlapResults;
    if (GetWorld()->OverlapMultiByChannel(OverlapResults, Position, FQuat::Identity,
        ECC_WorldStatic, SphereShape, QueryParams))
    {
        for (const FOverlapResult& Overlap : OverlapResults)
        {
            if (!IsValid(Overlap.GetActor())) continue;

            // 랜드스케이프는 장애물이 아님
            if (Overlap.GetActor()->IsA(ALandscape::StaticClass()) ||
                Overlap.GetActor()->IsA(ALandscapeProxy::StaticClass()))
            {
                continue;
            }

            FString ActorName = Overlap.GetActor()->GetName().ToLower();

            // ⭐ 개선: 더 많은 장애물 타입 감지
            if (ActorName.Contains(TEXT("tree")) ||
                ActorName.Contains(TEXT("rock")) ||
                ActorName.Contains(TEXT("prop")) ||
                ActorName.Contains(TEXT("foliage")) ||
                ActorName.Contains(TEXT("fence")) ||
                ActorName.Contains(TEXT("wall")) ||
                ActorName.Contains(TEXT("bark")) ||
                ActorName.Contains(TEXT("bush")) ||
                ActorName.Contains(TEXT("sm_")) || // StaticMesh 프리팹
                Overlap.GetActor()->Tags.Contains(TEXT("Obstacle")))
            {
                UE_LOG(LogTemp, Log, TEXT("🚫 Obstacle detected: %s (Type: %s)"),
                    *Overlap.GetActor()->GetName(),
                    *Overlap.GetActor()->GetClass()->GetName());
                return true;
            }

            // 홀컵/깃발 등 게임 오브젝트는 제외
            if (ActorName.Contains(TEXT("hole")) ||
                ActorName.Contains(TEXT("flag")) ||
                ActorName.Contains(TEXT("pin")) ||
                ActorName.Contains(TEXT("cup")))
            {
                continue;
            }

            // ⭐ 추가: 컴포넌트가 Foliage인지 체크
            if (Overlap.Component.IsValid())
            {
                FString CompName = Overlap.Component->GetName().ToLower();
                if (CompName.Contains(TEXT("foliage")) ||
                    CompName.Contains(TEXT("instancedstatic")))
                {
                    UE_LOG(LogTemp, Log, TEXT("🌳 Foliage obstacle: %s"), *CompName);
                    return true;
                }
            }
        }
    }

    return false;
}

// 새로 추가: 최종 위치 안전성 검사
bool AGolfBall::IsFinalPositionSafe(const FVector& Position, float BallRadius, const TArray<FVector>& OBPoints) const
{
    // 1. OB 라인 내부 확인
    if (!IsPositionInBounds(Position, OBPoints))
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("❌ Position is out of bounds"));
        return false;
    }

    // 2. 랜드스케이프 존재 확인
    if (AdjustToLandscapeHeight(Position, BallRadius) == FVector::ZeroVector)
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("❌ No valid landscape at position"));
        return false;
    }

    // 3. 오브젝트 충돌 재확인
    if (HasObstacleCollision(Position, BallRadius * 2.0f))
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("❌ Final obstacle check failed"));
        return false;
    }

    // 4. OB 라인과의 안전 거리 확인
    float DistanceToOB = GetDistanceToNearestOBLine(Position);
    if (DistanceToOB < 30.0f) // 30cm 이상 떨어져야 함
    {
        UE_LOG(LogTemp, Log, TEXT("❌ Too close to OB line: %.1fcm"), DistanceToOB);
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("✅ Final position is safe: OB distance=%.1fcm"), DistanceToOB);
    return true;
}


// ⭐ 새로 추가: OB 교차점에서 120cm 안쪽 드롭 위치 계산
FVector AGolfBall::CalculateOBDropFromCrossingPoint()
{
    if (!bHasValidOBCrossingPoint)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ No valid OB crossing point"));
        return FVector::ZeroVector;
    }

    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    if (!GameMode)
    {
        return FVector::ZeroVector;
    }

    int32 CurrentHoleIndex = GameMode->CurrentHole - 1;
    if (!GameMode->MapInfo.OBLines.IsValidIndex(CurrentHoleIndex))
    {
        return FVector::ZeroVector;
    }

    const TArray<FVector>& OBPoints = GameMode->MapInfo.OBLines[CurrentHoleIndex].Points;

    UE_LOG(LogTemp, Log, TEXT("🎯 Calculating drop from OB crossing: %s"),
        *LastOBCrossingPoint.ToString());

    // 1. OB 교차점에서 가장 가까운 OB 라인 세그먼트 찾기
    FVector ClosestPointOnOB = FVector::ZeroVector;
    FVector InwardNormal = FVector::ZeroVector;
    float MinDistance = FLT_MAX;

    for (int32 i = 0; i < OBPoints.Num(); i++)
    {
        int32 NextIndex = (i + 1) % OBPoints.Num();

        FVector LineStart = OBPoints[i];
        FVector LineEnd = OBPoints[NextIndex];

        // 라인 세그먼트에서 가장 가까운 점 찾기
        FVector ClosestPoint = GetClosestPointOnLineSegment3D(LastOBCrossingPoint, LineStart, LineEnd);
        float Distance = FVector::Dist(LastOBCrossingPoint, ClosestPoint);

        if (Distance < MinDistance)
        {
            MinDistance = Distance;
            ClosestPointOnOB = ClosestPoint;

            // 라인의 안쪽 방향 계산 (OB 라인에 수직, 코스 안쪽)
            FVector LineDirection = (LineEnd - LineStart).GetSafeNormal();
            FVector ToCenter = GetPolygonCenter(OBPoints) - ClosestPoint;
            ToCenter.Z = 0.0f; // 수평면에서만

            // 라인에 수직인 방향 중 코스 안쪽 방향 선택
            FVector Perpendicular1 = FVector::CrossProduct(LineDirection, FVector::UpVector);
            FVector Perpendicular2 = -Perpendicular1;

            // 코스 중심 방향과 더 가까운 수직 방향 선택
            float Dot1 = FVector::DotProduct(Perpendicular1, ToCenter.GetSafeNormal());
            float Dot2 = FVector::DotProduct(Perpendicular2, ToCenter.GetSafeNormal());

            InwardNormal = (Dot1 > Dot2) ? Perpendicular1 : Perpendicular2;
            InwardNormal = InwardNormal.GetSafeNormal();
        }
    }

    if (InwardNormal.IsNearlyZero())
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Could not determine inward direction"));
        return FVector::ZeroVector;
    }

    UE_LOG(LogTemp, Log, TEXT("📍 Closest OB point: %s, Inward: %s"),
        *ClosestPointOnOB.ToString(), *InwardNormal.ToString());

    // 2. 120cm 안쪽 위치 계산
    const float DropDistance = 120.0f; // 120cm
    FVector BaseDropPosition = ClosestPointOnOB + (InwardNormal * DropDistance);

    // 3. 랜드스케이프 높이에 맞추기
    float BallRadius = GetActualBallRadius();
    FVector LandscapePosition = FindLandscapePosition(BaseDropPosition, BallRadius);

    if (LandscapePosition == FVector::ZeroVector)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ No landscape found at drop position"));
        return FVector::ZeroVector;
    }

    // 4. 장애물 회피 검사 및 최종 위치 결정
    FVector SafePosition = FindSafeDropPositionWithObstacleAvoidance(
        LandscapePosition,
        InwardNormal,
        BallRadius,
        OBPoints
    );

    if (SafePosition != FVector::ZeroVector)
    {
        UE_LOG(LogTemp, Log, TEXT("✅ Safe drop position found: %s"), *SafePosition.ToString());

        // 디버그 시각화
      //  VisualizeOBDrop(ClosestPointOnOB, SafePosition, InwardNormal);

        return SafePosition;
    }

    UE_LOG(LogTemp, Error, TEXT("❌ Could not find safe drop position"));
    return FVector::ZeroVector;
}

// ⭐ 새로 추가: 3D 라인 세그먼트에서 가장 가까운 점
FVector AGolfBall::GetClosestPointOnLineSegment3D(
    const FVector& Point,
    const FVector& LineStart,
    const FVector& LineEnd) const
{
    FVector LineVec = LineEnd - LineStart;
    FVector PointVec = Point - LineStart;

    float LineLength = LineVec.Size();
    if (LineLength < KINDA_SMALL_NUMBER)
    {
        return LineStart;
    }

    float t = FVector::DotProduct(PointVec, LineVec) / (LineLength * LineLength);
    t = FMath::Clamp(t, 0.0f, 1.0f);

    return LineStart + t * LineVec;
}

// ⭐ 새로 추가: 폴리곤 중심점 계산
FVector AGolfBall::GetPolygonCenter(const TArray<FVector>& Points) const
{
    if (Points.Num() == 0) return FVector::ZeroVector;

    FVector Center = FVector::ZeroVector;
    for (const FVector& Point : Points)
    {
        Center += Point;
    }
    return Center / Points.Num();
}

// ⭐ 새로 추가: 장애물 회피 기능이 강화된 안전한 드롭 위치 찾기
FVector AGolfBall::FindSafeDropPositionWithObstacleAvoidance(
    const FVector& InitialPosition,
    const FVector& InwardDirection,
    float BallRadius,
    const TArray<FVector>& OBPoints) const
{
    // 1. 초기 위치가 안전한지 먼저 체크
    if (IsDropPositionSafe(InitialPosition, BallRadius, OBPoints))
    {
        UE_LOG(LogTemp, Log, TEXT("✅ Initial position is safe"));
        return InitialPosition;
    }

    UE_LOG(LogTemp, Warning, TEXT("⚠️ Initial position has obstacles, searching for safe spot"));

    // 2. 여러 방향과 거리로 안전한 위치 탐색
    TArray<FVector> SearchDirections;

    // 기본 안쪽 방향
    SearchDirections.Add(InwardDirection);

    // 안쪽 방향 기준으로 좌우 방향들
    for (int32 Angle = 30; Angle <= 150; Angle += 30)
    {
        FVector RightDir = InwardDirection.RotateAngleAxis(Angle, FVector::UpVector);
        FVector LeftDir = InwardDirection.RotateAngleAxis(-Angle, FVector::UpVector);
        SearchDirections.Add(RightDir);
        SearchDirections.Add(LeftDir);
    }

    // 여러 거리에서 시도
    TArray<float> SearchDistances = { 0.0f, 30.0f, 60.0f, 90.0f, 120.0f, 150.0f, 200.0f };

    for (float Distance : SearchDistances)
    {
        for (const FVector& Direction : SearchDirections)
        {
            FVector TestPosition = InitialPosition + (Direction * Distance);

            // 랜드스케이프 높이 재조정
            FVector AdjustedPosition = AdjustToLandscapeHeight(TestPosition, BallRadius);

            if (AdjustedPosition == FVector::ZeroVector)
            {
                continue;
            }

            // 안전성 검사
            if (IsDropPositionSafe(AdjustedPosition, BallRadius, OBPoints))
            {
                UE_LOG(LogTemp, Log, TEXT("✅ Safe position found at distance %.1fcm"), Distance);
                return AdjustedPosition;
            }
        }
    }

    UE_LOG(LogTemp, Error, TEXT("❌ No safe position found after extensive search"));
    return FVector::ZeroVector;
}

// ⭐ 새로 추가: 드롭 위치 안전성 종합 검사
bool AGolfBall::IsDropPositionSafe(
    const FVector& Position,
    float BallRadius,
    const TArray<FVector>& OBPoints) const
{
    // 1. OB 라인 내부 확인
    if (!IsPositionInBounds(Position, OBPoints))
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("❌ Position is OB"));
        return false;
    }

    // 2. OB 라인과 최소 거리 확인 (50cm 이상)
    float DistanceToOB = GetDistanceToNearestOBLine(Position);
    if (DistanceToOB < 50.0f)
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("❌ Too close to OB: %.1fcm"), DistanceToOB);
        return false;
    }

    // 3. 장애물 충돌 검사 (볼 반지름의 3배 영역)
    if (HasObstacleCollision(Position, BallRadius * 3.0f))
    {
        UE_LOG(LogTemp, Log, TEXT("❌ Obstacles detected"));
        return false;
    }

    // 4. 지형 경사도 검사 (30도 이하)
    FVector GroundNormal = GetGroundNormalAtPosition(Position);
    float SlopeAngle = FMath::RadiansToDegrees(
        FMath::Acos(FMath::Clamp(FVector::DotProduct(GroundNormal, FVector::UpVector), 0.0f, 1.0f))
    );

    if (SlopeAngle > 30.0f)
    {
        UE_LOG(LogTemp, Log, TEXT("❌ Slope too steep: %.1f°"), SlopeAngle);
        return false;
    }

    // 5. 물 체크
    if (LandscapeChecker && IsValid(LandscapeChecker))
    {
        ELandType LandType = LandscapeChecker->GetLandTypeAtLocation(Position);
        if (LandType == ELandType::Water)
        {
            UE_LOG(LogTemp, Log, TEXT("❌ Position in water"));
            return false;
        }
    }

    return true;
}

// ⭐ 새로 추가: 특정 위치의 지면 법선 구하기
FVector AGolfBall::GetGroundNormalAtPosition(const FVector& Position) const
{
    if (!GetWorld()) return FVector::UpVector;

    FVector TraceStart = Position + FVector(0, 0, 50.0f);
    FVector TraceEnd = Position - FVector(0, 0, 100.0f);

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    QueryParams.bTraceComplex = true;

    if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
    {
        return HitResult.Normal;
    }

    return FVector::UpVector;
}

// ⭐ 새로 추가: OB 드롭 시각화
void AGolfBall::VisualizeOBDrop(
    const FVector& OBPoint,
    const FVector& DropPoint,
    const FVector& InwardDirection) const
{
    if (!GetWorld()) return;

    const float Duration = 20.0f; // 20초간 표시

    // OB 교차점 표시 (빨간색)
    DrawDebugSphere(GetWorld(), OBPoint, 15.0f, 12, FColor::Red, false, Duration);
    DrawDebugString(GetWorld(), OBPoint + FVector(0, 0, 80),
        TEXT("OB Crossing Point"), nullptr, FColor::Red, Duration, false, 1.5f);

    // 드롭 위치 표시 (초록색)
    DrawDebugSphere(GetWorld(), DropPoint, 25.0f, 16, FColor::Green, false, Duration);
    DrawDebugString(GetWorld(), DropPoint + FVector(0, 0, 80),
        TEXT("DROP HERE"), nullptr, FColor::Green, Duration, false, 2.0f);

    // ⭐ 추가: 드롭 위치 높이 표시
    DrawDebugString(GetWorld(), DropPoint + FVector(0, 0, 50),
        FString::Printf(TEXT("Z: %.1f"), DropPoint.Z),
        nullptr, FColor::Cyan, Duration, false, 1.2f);

    // 안쪽 방향 화살표
    FVector ArrowEnd = OBPoint + (InwardDirection * 150.0f);
    DrawDebugDirectionalArrow(GetWorld(), OBPoint, ArrowEnd, 30.0f,
        FColor::Yellow, false, Duration, 0, 3.0f);

    // OB점에서 드롭 위치까지 선
    DrawDebugLine(GetWorld(), OBPoint, DropPoint, FColor::Orange, false, Duration, 0, 3.0f);

    // 거리 표시
    float Distance = FVector::Dist(OBPoint, DropPoint);
    FVector MidPoint = (OBPoint + DropPoint) * 0.5f + FVector(0, 0, 30);
    DrawDebugString(GetWorld(), MidPoint,
        FString::Printf(TEXT("Distance: %.0fcm"), Distance),
        nullptr, FColor::White, Duration, false, 1.3f);

}

// ============================================================================
// OB 라인 교차점 계산 함수들
// ============================================================================

// 두 선분의 교차점을 계산 (2D)
bool AGolfBall::LineSegmentIntersection2D(
    const FVector2D& P1, const FVector2D& P2,
    const FVector2D& P3, const FVector2D& P4,
    FVector2D& OutIntersection) const
{
    float x1 = P1.X, y1 = P1.Y;
    float x2 = P2.X, y2 = P2.Y;
    float x3 = P3.X, y3 = P3.Y;
    float x4 = P4.X, y4 = P4.Y;

    float Denominator = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);

    // 평행하거나 일치하는 경우
    if (FMath::Abs(Denominator) < KINDA_SMALL_NUMBER)
    {
        return false;
    }

    float t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / Denominator;
    float u = -((x1 - x2) * (y1 - y3) - (y1 - y2) * (x1 - x3)) / Denominator;

    // 두 선분이 교차하는지 확인 (0 <= t <= 1 and 0 <= u <= 1)
    if (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f)
    {
        OutIntersection.X = x1 + t * (x2 - x1);
        OutIntersection.Y = y1 + t * (y2 - y1);
        return true;
    }

    return false;
}

// 볼 궤적과 OB 라인의 교차점 계산
FVector AGolfBall::CalculateOBLineIntersection(
    const FVector& PrevPos,
    const FVector& CurrentPos,
    const TArray<FVector>& OBPoints) const
{
    FVector2D BallSegmentStart(PrevPos.X, PrevPos.Y);
    FVector2D BallSegmentEnd(CurrentPos.X, CurrentPos.Y);

    FVector ClosestIntersection = FVector::ZeroVector;
    float ClosestDistance = FLT_MAX;

    // 모든 OB 라인 세그먼트와 교차점 확인
    for (int32 i = 0; i < OBPoints.Num(); i++)
    {
        int32 NextIndex = (i + 1) % OBPoints.Num();

        FVector2D OBSegmentStart(OBPoints[i].X, OBPoints[i].Y);
        FVector2D OBSegmentEnd(OBPoints[NextIndex].X, OBPoints[NextIndex].Y);

        FVector2D Intersection2D;
        if (LineSegmentIntersection2D(
            BallSegmentStart, BallSegmentEnd,
            OBSegmentStart, OBSegmentEnd,
            Intersection2D))
        {
            // 교차점 발견! Z 좌표는 OB 라인의 두 점을 보간하여 계산
            float t = 0.0f;
            float SegmentLength = FVector2D::Distance(OBSegmentStart, OBSegmentEnd);
            if (SegmentLength > KINDA_SMALL_NUMBER)
            {
                t = FVector2D::Distance(OBSegmentStart, Intersection2D) / SegmentLength;
            }

            float InterpolatedZ = FMath::Lerp(OBPoints[i].Z, OBPoints[NextIndex].Z, t);
            FVector Intersection3D(Intersection2D.X, Intersection2D.Y, InterpolatedZ);

            // 이전 위치에서 가장 가까운 교차점을 찾기
            float DistanceFromPrevPos = FVector::Dist(PrevPos, Intersection3D);
            if (DistanceFromPrevPos < ClosestDistance)
            {
                ClosestDistance = DistanceFromPrevPos;
                ClosestIntersection = Intersection3D;
            }

            UE_LOG(LogTemp, Log, TEXT("📍 OB Line intersection found at: %s (segment %d-%d)"),
                *Intersection3D.ToString(), i, NextIndex);
        }
    }

    if (!ClosestIntersection.IsZero())
    {
        UE_LOG(LogTemp, Warning, TEXT("✅ Closest OB intersection: %s"),
            *ClosestIntersection.ToString());
    }

    return ClosestIntersection;
}


void AGolfBall::ShowCrosshair()
{
    if (bIsBeingDestroyed || !ShouldShowCrosshair()) return;

    if (!CrosshairActor)
    {
        CreateCrosshairActor();  // 액터 생성
    }

    if (CrosshairActor && IsValid(CrosshairActor))
    {
        CrosshairActor->SetActorHiddenInGame(false);
        CrosshairActor->SetActorEnableCollision(false);
        bCrosshairActive = true;

        UpdateCrosshairPosition();  // 즉시 위치 동기화

        UE_LOG(LogTemp, Log, TEXT("🎯 Crosshair SHOWN: Hole distance = %.1fm"), GetHoleDistance() / 100.0f);
    }
}

void AGolfBall::HideCrosshair()
{
    if (CrosshairActor && IsValid(CrosshairActor))
    {
        CrosshairActor->SetActorHiddenInGame(true);
        bCrosshairActive = false;
        UE_LOG(LogTemp, Log, TEXT("🎯 Crosshair HIDDEN"));
    }
}

// ⭐ 추가: 크로스헤어 표시 조건 체크
bool AGolfBall::ShouldShowCrosshair() const
{
    // 홀컵 거리가 10m(1000cm) 초과일 때만 표시
    float HoleDistance = GetHoleDistance();
    bool bShowDueToDistance = (HoleDistance > 1000.0f); // 10m 초과

    if (!bShowDueToDistance)
    {
        UE_LOG(LogTemp, Log, TEXT("🎯 Crosshair HIDDEN: Hole distance = %.1fm (≤10m)"), HoleDistance / 100.0f);
    }

    return bShowDueToDistance;
}


void AGolfBall::CreateCrosshairActor()
{
    if (CrosshairActor || bIsBeingDestroyed || !CrosshairBPClass) return;

    UWorld* World = GetWorld();
    if (!World) return;

    FVector SpawnLoc = GetActorLocation();
    FRotator SpawnRot = GetActorRotation();

    CrosshairActor = World->SpawnActor<AActor>(CrosshairBPClass, SpawnLoc, SpawnRot);
    if (CrosshairActor)
    {
        CrosshairActor->SetActorLocation(SpawnLoc + FVector(0, 0, 0.0f));
        CrosshairActor->SetActorEnableCollision(false);

        GetWorldTimerManager().SetTimer(
            CrosshairUpdateTimer,
            this,
            &AGolfBall::UpdateCrosshairPosition,
            0.05f,
            true);

        UE_LOG(LogTemp, Log, TEXT("Crosshair SPAWNED"));
    }
}

void AGolfBall::DestroyCrosshairActor()
{
    if (!CrosshairActor || bIsBeingDestroyed) return;

    // 타이머 정리 (안전)
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(CrosshairUpdateTimer);
    }

    // 객체가 유효한지 다시 확인
    if (IsValid(CrosshairActor))
    {
        UE_LOG(LogTemp, Log, TEXT("Crosshair Actor destroyed safely"));
        CrosshairActor->Destroy();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("CrosshairActor already invalid - skipped destroy"));
    }

    CrosshairActor = nullptr;
    bCrosshairActive = false;
}


void AGolfBall::UpdateCrosshairPosition()
{
    // 1. 파괴 중이면 즉시 종료
    if (bIsBeingDestroyed) return;

    // 2. 액터 유효성 + 월드 존재 체크
    if (!CrosshairActor || !IsValid(CrosshairActor) || !GetWorld())
    {
        DestroyCrosshairActor();
        return;
    }

    FVector BallLoc = GetActorLocation();
    CrosshairActor->SetActorLocation(BallLoc + FVector(0, 0, 0.0f));
    //CrosshairActor->SetActorRotation(GetActorRotation());
}

void AGolfBall::ApplyComplexMesh()
{
    if (!BallMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ BallMesh is null"));
        return;
    }

    if (!ComplexBallMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ ComplexBallMesh not loaded!"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("🔄 Applying COMPLEX Mesh: %s"),
        *ComplexBallMesh->GetName());

    UE_LOG(LogTemp, Warning, TEXT("------Ball Constructor 1"));

    // 물리 중이면 잠시 정지
    const bool bWasSimulating = BallMesh->IsSimulatingPhysics();
    if (bWasSimulating)
    {
        BallMesh->SetSimulatePhysics(false);
    }

    UE_LOG(LogTemp, Warning, TEXT("------Ball Constructor 2 - change Mesh"));
    // 메쉬 교체

    BallMesh->SetStaticMesh(ComplexBallMesh);

    UE_LOG(LogTemp, Warning, TEXT("------Ball Constructor 3 - Collision setting "));
    // 스케일 및 콜리전 재설정
    float ReadyScale = 1.0f; // 필요시 조정 가능
    BallMesh->SetWorldScale3D(FVector(ReadyScale));

    BallMesh->SetCollisionProfileName(TEXT("Custom"));
    BallMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    BallMesh->SetNotifyRigidBodyCollision(true);
    BallMesh->SetGenerateOverlapEvents(true);

    UE_LOG(LogTemp, Warning, TEXT("------Ball Constructor 4 - Physic setting"));

    // 다시 물리 활성화 (Ball_Ready 상태이므로 물리는 비활성)
    BallMesh->SetSimulatePhysics(false);
    BallMesh->SetEnableGravity(false);

    UE_LOG(LogTemp, Log, TEXT("🎾 Initial Mesh set: %s"),
        BallMesh->GetStaticMesh() ?
        *BallMesh->GetStaticMesh()->GetName() : TEXT("None"));

}


void AGolfBall::ApplySimpleMesh()
{
    if (!BallMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ BallMesh is null"));
        return;
    }

    if (!SimpleBallMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ SimpleBallMesh not loaded!"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("🔄 Applying simple Mesh: %s"),
        *SimpleBallMesh->GetName());

    UE_LOG(LogTemp, Warning, TEXT("------Ball Constructor 1"));

    // 물리 중이면 잠시 정지
    const bool bWasSimulating = BallMesh->IsSimulatingPhysics();
    if (bWasSimulating)
    {
        BallMesh->SetSimulatePhysics(false);
    }

    UE_LOG(LogTemp, Warning, TEXT("------Ball Constructor 2 - change Mesh"));
    // 메쉬 교체

    BallMesh->SetStaticMesh(SimpleBallMesh);

    UE_LOG(LogTemp, Warning, TEXT("------Ball Constructor 3 - Collision setting "));
    // 스케일 및 콜리전 재설정
    float ReadyScale = 1.0f; // 필요시 조정 가능
    BallMesh->SetWorldScale3D(FVector(ReadyScale));

    BallMesh->SetCollisionProfileName(TEXT("Custom"));
    BallMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    BallMesh->SetNotifyRigidBodyCollision(true);
    BallMesh->SetGenerateOverlapEvents(true);

    UE_LOG(LogTemp, Warning, TEXT("------Ball Constructor 4 - Physic setting"));

    // 다시 물리 활성화 (Ball_Ready 상태이므로 물리는 비활성)
    BallMesh->SetSimulatePhysics(false);
    BallMesh->SetEnableGravity(false);

    UE_LOG(LogTemp, Log, TEXT("🎾 Initial Mesh set: %s"),
        BallMesh->GetStaticMesh() ?
        *BallMesh->GetStaticMesh()->GetName() : TEXT("None"));

}

// ⭐⭐⭐ 함수 1: adminConfig.json 경로 반환
FString AGolfBall::GetShotAdjustmentsConfigPath() const
{
    // [ProjectRoot]/Saved/Config/adminConfig.json
    FString ConfigDir = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("Config")
    );

    return FPaths::Combine(ConfigDir, TEXT("BallPhysics.json"));
}

// 함수 2: 샷 타입 판단 (⭐ 새로운 함수)
EShotType AGolfBall::DetermineShotType()
{

    // 거리가 허용 범위 내면 티샷
    if (CheckTeeShot())
    {
        CurrentShotType = EShotType::TeeShot;
        UE_LOG(LogTemp, Log, TEXT("🟢 Tee Shot detected "));
        return EShotType::TeeShot;
    }
    else
    {
        CurrentShotType = EShotType::NormalShot;
        UE_LOG(LogTemp, Log, TEXT("🔵 Normal Shot detected "));
        return EShotType::NormalShot;
    }
}

// ⭐⭐⭐ 함수 2: JSON 파일에서 샷 조정값 로드
void AGolfBall::LoadShotAdjustmentsFromJSON()
{
    FString ConfigPath = GetShotAdjustmentsConfigPath();

    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*ConfigPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Shot adjustments config not found: %s"), *ConfigPath);
        UE_LOG(LogTemp, Warning, TEXT("   Using default values"));
        CurrentShotAdjustments = FShotAdjustments();
        return;
    }

    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *ConfigPath))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to read shot adjustments config: %s"), *ConfigPath);
        CurrentShotAdjustments = FShotAdjustments();
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("📄 Loading shot adjustments from: %s"), *ConfigPath);

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to parse shot adjustments JSON"));
        CurrentShotAdjustments = FShotAdjustments();
        return;
    }

    if (!JsonObject->HasField(TEXT("shotAdjustments")))
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ 'shotAdjustments' field not found in JSON"));
        CurrentShotAdjustments = FShotAdjustments();
        return;
    }

    TSharedPtr<FJsonObject> ShotAdjustmentsObj = JsonObject->GetObjectField(TEXT("shotAdjustments"));
    if (!ShotAdjustmentsObj.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to get shotAdjustments object"));
        CurrentShotAdjustments = FShotAdjustments();
        return;
    }

    // ⭐ 기본 조정값 로드 (하위 호환성)
    CurrentShotAdjustments.SpeedMultiplier =
        ShotAdjustmentsObj->HasField(TEXT("speedMultiplier"))
        ? ShotAdjustmentsObj->GetNumberField(TEXT("speedMultiplier"))
        : 1.0f;

    CurrentShotAdjustments.SpeedOffset =
        ShotAdjustmentsObj->HasField(TEXT("speedOffset"))
        ? ShotAdjustmentsObj->GetNumberField(TEXT("speedOffset"))
        : 0.0f;

    CurrentShotAdjustments.PitchAngleOffset =
        ShotAdjustmentsObj->HasField(TEXT("pitchAngleOffset"))
        ? ShotAdjustmentsObj->GetNumberField(TEXT("pitchAngleOffset"))
        : 0.0f;

    CurrentShotAdjustments.YawAngleOffset =
        ShotAdjustmentsObj->HasField(TEXT("yawAngleOffset"))
        ? ShotAdjustmentsObj->GetNumberField(TEXT("yawAngleOffset"))
        : 0.0f;

    // ⭐ 티샷 조정값 로드
    if (ShotAdjustmentsObj->HasField(TEXT("teeShot")))
    {
        TSharedPtr<FJsonObject> TeeShotObj = ShotAdjustmentsObj->GetObjectField(TEXT("teeShot"));
        if (TeeShotObj.IsValid())
        {
            CurrentShotAdjustments.TeeSpeedMultiplier =
                TeeShotObj->HasField(TEXT("speedMultiplier"))
                ? TeeShotObj->GetNumberField(TEXT("speedMultiplier"))
                : 1.0f;

            CurrentShotAdjustments.TeeSpeedOffset =
                TeeShotObj->HasField(TEXT("speedOffset"))
                ? TeeShotObj->GetNumberField(TEXT("speedOffset"))
                : 0.0f;

            CurrentShotAdjustments.TeePitchAngleOffset =
                TeeShotObj->HasField(TEXT("pitchAngleOffset"))
                ? TeeShotObj->GetNumberField(TEXT("pitchAngleOffset"))
                : 0.0f;

            CurrentShotAdjustments.TeeYawAngleOffset =
                TeeShotObj->HasField(TEXT("yawAngleOffset"))
                ? TeeShotObj->GetNumberField(TEXT("yawAngleOffset"))
                : 0.0f;

            UE_LOG(LogTemp, Log, TEXT("   ✅ Tee Shot adjustments loaded"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("   ⚠️ 'teeShot' field not found, using defaults"));
    }

    // ⭐ 일반샷 조정값 로드
    if (ShotAdjustmentsObj->HasField(TEXT("normalShot")))
    {
        TSharedPtr<FJsonObject> NormalShotObj = ShotAdjustmentsObj->GetObjectField(TEXT("normalShot"));
        if (NormalShotObj.IsValid())
        {
            CurrentShotAdjustments.NormalSpeedMultiplier =
                NormalShotObj->HasField(TEXT("speedMultiplier"))
                ? NormalShotObj->GetNumberField(TEXT("speedMultiplier"))
                : 1.0f;

            CurrentShotAdjustments.NormalSpeedOffset =
                NormalShotObj->HasField(TEXT("speedOffset"))
                ? NormalShotObj->GetNumberField(TEXT("speedOffset"))
                : 0.0f;

            CurrentShotAdjustments.NormalPitchAngleOffset =
                NormalShotObj->HasField(TEXT("pitchAngleOffset"))
                ? NormalShotObj->GetNumberField(TEXT("pitchAngleOffset"))
                : 0.0f;

            CurrentShotAdjustments.NormalYawAngleOffset =
                NormalShotObj->HasField(TEXT("yawAngleOffset"))
                ? NormalShotObj->GetNumberField(TEXT("yawAngleOffset"))
                : 0.0f;

            UE_LOG(LogTemp, Log, TEXT("   ✅ Normal Shot adjustments loaded"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("   ⚠️ 'normalShot' field not found, using defaults"));
    }

    // 로그 출력
    UE_LOG(LogTemp, Log, TEXT("✅ All shot adjustments loaded successfully:"));
    UE_LOG(LogTemp, Log, TEXT("   [TEE SHOT]"));
    UE_LOG(LogTemp, Log, TEXT("      Speed: ×%.2f %+.2f m/s"),
        CurrentShotAdjustments.TeeSpeedMultiplier, CurrentShotAdjustments.TeeSpeedOffset);
    UE_LOG(LogTemp, Log, TEXT("      Angle: %+.1f° (Pitch), %+.1f° (Yaw)"),
        CurrentShotAdjustments.TeePitchAngleOffset, CurrentShotAdjustments.TeeYawAngleOffset);
    UE_LOG(LogTemp, Log, TEXT("   [NORMAL SHOT]"));
    UE_LOG(LogTemp, Log, TEXT("      Speed: ×%.2f %+.2f m/s"),
        CurrentShotAdjustments.NormalSpeedMultiplier, CurrentShotAdjustments.NormalSpeedOffset);
    UE_LOG(LogTemp, Log, TEXT("      Angle: %+.1f° (Pitch), %+.1f° (Yaw)"),
        CurrentShotAdjustments.NormalPitchAngleOffset, CurrentShotAdjustments.NormalYawAngleOffset);
}

// ⭐⭐⭐ 함수 3: 조정값 적용
void AGolfBall::ApplyShotAdjustments(float& OutSpeed, float& OutPitchAngle, float& OutYawAngle, EShotType ShotType)
{
    float OriginalSpeed = OutSpeed;
    float OriginalPitchAngle = OutPitchAngle;
    float OriginalYawAngle = OutYawAngle;

    // ⭐ 샷 타입에 따라 다른 조정값 선택
    float SpeedMultiplier;
    float SpeedOffset;
    float PitchAngleOffset;
    float YawAngleOffset;

    if (ShotType == EShotType::TeeShot)
    {
        SpeedMultiplier = CurrentShotAdjustments.TeeSpeedMultiplier;
        SpeedOffset = CurrentShotAdjustments.TeeSpeedOffset;
        PitchAngleOffset = CurrentShotAdjustments.TeePitchAngleOffset;
        YawAngleOffset = CurrentShotAdjustments.TeeYawAngleOffset;

        UE_LOG(LogTemp, Log, TEXT("🟢 Applying TEE SHOT adjustments:"));
        UE_LOG(LogTemp, Log, TEXT("   Multiplier: %.2f, Offset: %.2f"), SpeedMultiplier, SpeedOffset);
    }
    else
    {
        SpeedMultiplier = CurrentShotAdjustments.NormalSpeedMultiplier;
        SpeedOffset = CurrentShotAdjustments.NormalSpeedOffset;
        PitchAngleOffset = CurrentShotAdjustments.NormalPitchAngleOffset;
        YawAngleOffset = CurrentShotAdjustments.NormalYawAngleOffset;

        UE_LOG(LogTemp, Log, TEXT("🔵 Applying NORMAL SHOT adjustments:"));
        UE_LOG(LogTemp, Log, TEXT("   Multiplier: %.2f, Offset: %.2f"), SpeedMultiplier, SpeedOffset);
    }

    SpeedMultiplier *= CurrentShotAdjustments.SpeedMultiplier;
    SpeedOffset += CurrentShotAdjustments.SpeedOffset;

    // 스피드 조정
    OutSpeed *= SpeedMultiplier;
    OutSpeed += SpeedOffset;
    OutSpeed = FMath::Clamp(OutSpeed, 1.0f, 50.0f);

    PitchAngleOffset += CurrentShotAdjustments.PitchAngleOffset;
    YawAngleOffset += CurrentShotAdjustments.YawAngleOffset;
    // 각도 조정
    OutPitchAngle += PitchAngleOffset;
    OutYawAngle += YawAngleOffset;

    OutPitchAngle = FMath::Clamp(OutPitchAngle, 0.5f, 45.0f);
    OutYawAngle = FMath::Clamp(OutYawAngle, -45.0f, 45.0f);

    // 로그 출력
    UE_LOG(LogTemp, Log, TEXT("   Speed: %.2f → %.2f m/s (%+.2f)"),
        OriginalSpeed, OutSpeed, OutSpeed - OriginalSpeed);
    UE_LOG(LogTemp, Log, TEXT("   Pitch Angle: %.2f → %.2f° (%+.2f)"),
        OriginalPitchAngle, OutPitchAngle, OutPitchAngle - OriginalPitchAngle);
    UE_LOG(LogTemp, Log, TEXT("   Yaw Angle: %.2f → %.2f° (%+.2f)"),
        OriginalYawAngle, OutYawAngle, OutYawAngle - OriginalYawAngle);
}