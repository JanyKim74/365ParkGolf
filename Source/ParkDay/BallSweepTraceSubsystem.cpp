// BallSweepTraceSubsystem.cpp

#include "BallSweepTraceSubsystem.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY(LogBallSweepSys);

UBallSweepTraceSubsystem::UBallSweepTraceSubsystem()
{
    // 기본 전역 샘플링 간격 (개별 Override가 없으면 사용)
    SampleInterval = 0.05f;
}

void UBallSweepTraceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UE_LOG(LogBallSweepSys, Log, TEXT("[Initialize] Subsystem 초기화. Global SampleInterval=%.3fs"), SampleInterval);

    // 전역 타이머는 “등록된 액터가 있고, 개별 오버라이드가 없는 경우에만” 의미가 있음.
    // 최초에는 등록 액터 수가 0이므로 타이머는 등록 시점에 세팅함.
}

void UBallSweepTraceSubsystem::Deinitialize()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(GlobalTimerHandle);

        // 액터별 타이머 정리
        for (auto& Pair : TrackedStates)
        {
            World->GetTimerManager().ClearTimer(Pair.Value.TimerHandle);
        }
    }

    TrackedConfigs.Empty();
    TrackedStates.Empty();

    UE_LOG(LogBallSweepSys, Log, TEXT("[Deinitialize] Subsystem 종료"));
    Super::Deinitialize();
}

void UBallSweepTraceSubsystem::RegisterActor(AActor* Actor, const FBallSweepTraceConfig& Config)
{
    if (!Actor || !GetWorld())
    {
        UE_LOG(LogBallSweepSys, Warning, TEXT("[RegisterActor] Actor 또는 World가 유효하지 않음"));
        return;
    }

    TWeakObjectPtr<AActor> Key(Actor);
    if (TrackedConfigs.Contains(Key))
    {
        UE_LOG(LogBallSweepSys, Log, TEXT("[RegisterActor] 이미 등록됨: %s"), *Actor->GetName());
        // 설정 업데이트 허용(원하면 생략 가능)
        TrackedConfigs[Key] = Config;
        return;
    }

    // 설정/상태 추가
    TrackedConfigs.Add(Key, Config);

    FBallSweepTraceState NewState;
    NewState.Owner = Actor;
    NewState.PrevLocation = Actor->GetActorLocation();
    TrackedStates.Add(Key, NewState);

    UE_LOG(LogBallSweepSys, Log, TEXT("[RegisterActor] 등록: %s  Prev=%s"), *Actor->GetName(), *NewState.PrevLocation.ToString());

    // 개별 인터벌이 지정된 경우: 개별 타이머
    const bool bUsePerActorTimer = (Config.OverrideSampleInterval > 0.f);
    if (bUsePerActorTimer)
    {
        GetWorld()->GetTimerManager().SetTimer(
            TrackedStates[Key].TimerHandle,
            FTimerDelegate::CreateUObject(this, &UBallSweepTraceSubsystem::TickOne, Actor),
            Config.OverrideSampleInterval,
            true);

        UE_LOG(LogBallSweepSys, Log, TEXT("[RegisterActor] 개별 타이머 시작: %s (Interval=%.3fs)"),
            *Actor->GetName(), Config.OverrideSampleInterval);
    }
    else
    {
        // 전역 타이머가 돌고 있지 않으면 시작
        if (!GetWorld()->GetTimerManager().IsTimerActive(GlobalTimerHandle))
        {
            GetWorld()->GetTimerManager().SetTimer(
                GlobalTimerHandle, this, &UBallSweepTraceSubsystem::TickAll, SampleInterval, true);

            UE_LOG(LogBallSweepSys, Log, TEXT("[RegisterActor] 전역 타이머 시작 (Interval=%.3fs)"), SampleInterval);
        }
    }
}

void UBallSweepTraceSubsystem::UnregisterActor(AActor* Actor)
{
    if (!Actor || !GetWorld()) return;

    TWeakObjectPtr<AActor> Key(Actor);

    // 개별 타이머 삭제
    if (FBallSweepTraceState* St = TrackedStates.Find(Key))
    {
        GetWorld()->GetTimerManager().ClearTimer(St->TimerHandle);
    }

    TrackedStates.Remove(Key);
    TrackedConfigs.Remove(Key);

    UE_LOG(LogBallSweepSys, Log, TEXT("[UnregisterActor] 해제: %s"), *Actor->GetName());

    // 남은 대상이 없고 전역 타이머가 돌고 있으면 중지
    if (TrackedConfigs.Num() == 0 && GetWorld()->GetTimerManager().IsTimerActive(GlobalTimerHandle))
    {
        GetWorld()->GetTimerManager().ClearTimer(GlobalTimerHandle);
        UE_LOG(LogBallSweepSys, Log, TEXT("[UnregisterActor] 전역 타이머 중지 (대상 0)"));
    }
}

bool UBallSweepTraceSubsystem::IsRegistered(AActor* Actor) const
{
    return Actor && TrackedConfigs.Contains(TWeakObjectPtr<AActor>(Actor));
}

// ——— 타이머 콜백 ————————————————————————————————————

void UBallSweepTraceSubsystem::TickAll()
{
    UWorld* World = GetWorld();
    if (!World) return;

    // 전역 인터벌로 도는 대상들만 처리(개별 타이머 대상은 TickOne에서 따로 돎)
    for (auto& Pair : TrackedConfigs)
    {
        AActor* Actor = Pair.Key.Get();
        if (!Actor) continue;

        const FBallSweepTraceConfig& Cfg = Pair.Value;
        if (Cfg.OverrideSampleInterval > 0.f) continue; // 개별 타이머 대상 스킵

        FBallSweepTraceState* St = TrackedStates.Find(Pair.Key);
        if (!St) continue;

        SampleAndTrace(Actor, const_cast<FBallSweepTraceConfig&>(Cfg), *St);
    }
}

void UBallSweepTraceSubsystem::TickOne(AActor* Actor)
{
    if (!Actor) return;

    TWeakObjectPtr<AActor> Key(Actor);
    FBallSweepTraceConfig* Cfg = TrackedConfigs.Find(Key);
    FBallSweepTraceState* St   = TrackedStates.Find(Key);

    if (!Cfg || !St) return;

    SampleAndTrace(Actor, *Cfg, *St);
}

// ——— 핵심 로직 ————————————————————————————————————

void UBallSweepTraceSubsystem::SampleAndTrace(AActor* Actor, FBallSweepTraceConfig& Cfg, FBallSweepTraceState& St)
{
    if (!Actor) return;

    UWorld* World = GetWorld();
    if (!World) return;

    const FVector Curr = Actor->GetActorLocation();
    const FVector Move = Curr - St.PrevLocation;
    const float   Dist = Move.Size();

    // 속도 임계치 체크
    float Dt = Cfg.OverrideSampleInterval > 0.f ? Cfg.OverrideSampleInterval : SampleInterval;
    Dt = (Dt > 0.f) ? Dt : FApp::GetDeltaTime(); // 방어적: 0인 경우 Tick Delta 사용
    const float Speed = (Dt > KINDA_SMALL_NUMBER) ? (Dist / Dt) : 0.f;

    if (Cfg.MinSpeedToCheck > 0.f && Speed < Cfg.MinSpeedToCheck)
    {
        UE_LOG(LogBallSweepSys, Verbose, TEXT("[Sample] %s 속도(%.1f) < MinSpeed(%.1f) → 스킵"),
            *Actor->GetName(), Speed, Cfg.MinSpeedToCheck);
        St.PrevLocation = Curr; // 위치 갱신(누적 오차 방지)
        return;
    }

    if (Dist <= KINDA_SMALL_NUMBER)
    {
        // 정지
        St.PrevLocation = Curr;
        return;
    }

    UE_LOG(LogBallSweepSys, Verbose, TEXT("[Sample] %s 경로 스윕: From=%s  To=%s  Dist=%.1f  Speed=%.1f"),
        *Actor->GetName(), *St.PrevLocation.ToString(), *Curr.ToString(), Dist, Speed);

    // 경로 스윕
    SweepPath(World, St.PrevLocation, Curr, Cfg, St, Actor);

    // 다음 프레임 대비
    St.PrevLocation = Curr;
}

void UBallSweepTraceSubsystem::SweepPath(UWorld* World, const FVector& From, const FVector& To,
                                         const FBallSweepTraceConfig& Cfg, FBallSweepTraceState& St, AActor* TrackedActor)
{
    const float TotalDist = FVector::Distance(From, To);

    // 세분화 개수 계산
    int32 NumSteps = FMath::Max(1, FMath::CeilToInt(TotalDist / FMath::Max(1.f, Cfg.MaxStepDistance)));
    NumSteps = FMath::Min(NumSteps, Cfg.MaxSubsteps);

    UE_LOG(LogBallSweepSys, Verbose, TEXT("[SweepPath] %s TotalDist=%.1f → Substeps=%d (MaxStep=%.1f)"),
        *TrackedActor->GetName(), TotalDist, NumSteps, Cfg.MaxStepDistance);

    FVector SegmentStart = From;

    for (int32 Step = 1; Step <= NumSteps; ++Step)
    {
        const float Alpha = (float)Step / (float)NumSteps;
        const FVector SegmentEnd = FMath::Lerp(From, To, Alpha);

        FHitResult Hit;
        const bool bHit = SweepSegment(World, SegmentStart, SegmentEnd, Cfg, Hit, TrackedActor);

        if (Cfg.bDrawDebug)
        {
            const FColor Col = bHit ? FColor::Red : FColor::Green;
            DrawDebugLine(World, SegmentStart, SegmentEnd, Col, false, Cfg.DebugPersistTime, 0, 2.f);
        }

        if (bHit)
        {
            HandleSweepHit(Hit, const_cast<FBallSweepTraceConfig&>(Cfg), St, TrackedActor);
            // 정책: 첫 히트만 처리하고 종료 (원하면 계속 검사로 변경 가능)
            return;
        }

        SegmentStart = SegmentEnd;
    }
}

bool UBallSweepTraceSubsystem::SweepSegment(UWorld* World, const FVector& From, const FVector& To,
                                            const FBallSweepTraceConfig& Cfg, FHitResult& OutHit, AActor* IgnoredActor) const
{
    FCollisionQueryParams Q(SCENE_QUERY_STAT(BallSweepSys), /*bTraceComplex=*/false, IgnoredActor);
    Q.bReturnPhysicalMaterial = true;
    Q.bFindInitialOverlaps = false;

    FCollisionObjectQueryParams O;
    if (Cfg.bQueryWorldStatic)  { O.AddObjectTypesToQuery(ECC_WorldStatic); }
    if (Cfg.bQueryWorldDynamic) { O.AddObjectTypesToQuery(ECC_WorldDynamic); }
    // 필요 시 O.AddObjectTypesToQuery(ECC_PhysicsBody) 등

    const FQuat Rot = FQuat::Identity;
    const FCollisionShape Shape = FCollisionShape::MakeSphere(Cfg.TraceRadius);

    const bool bHit = World->SweepSingleByObjectType(OutHit, From, To, Rot, O, Shape, Q);

    if (bHit)
    {
        if (!OutHit.Component.IsValid())
        {
            UE_LOG(LogBallSweepSys, Verbose, TEXT("[SweepSegment] Hit했지만 Component 무효"));
            return false;
        }

        // 태그 필터
        if (!PassTagFilter(Cfg.AcceptTagFilters, OutHit.Component.Get()))
        {
            UE_LOG(LogBallSweepSys, Verbose, TEXT("[SweepSegment] 태그 불일치 → 무시 (Comp=%s)"),
                *OutHit.Component->GetName());
            return false;
        }

        //UE_LOG(LogBallSweepSys, Log, TEXT("[SweepSegment] HIT Comp=%s Actor=%s Impact=%s Normal=%s"),
        //    *OutHit.Component->GetName(),
        //    OutHit.GetActor() ? *OutHit.GetActor()->GetName() : TEXT("None"),
        //    *OutHit.ImpactPoint.ToString(),
        //    *OutHit.ImpactNormal.ToString());
    }

    return bHit;
}

bool UBallSweepTraceSubsystem::PassTagFilter(const TArray<FName>& Filters, const UPrimitiveComponent* Comp) const
{
    if (Filters.Num() == 0) return true; // 필터 비어있으면 모두 허용

    for (const FName& Tag : Filters)
    {
        if (Comp && Comp->ComponentHasTag(Tag))
        {
            return true;
        }
    }
    return false;
}

void UBallSweepTraceSubsystem::BuildQueryParams(FCollisionQueryParams& OutQ, FCollisionObjectQueryParams& OutO,
                                                const FBallSweepTraceConfig& Cfg, AActor* IgnoredActor) const
{
    OutQ = FCollisionQueryParams(SCENE_QUERY_STAT(BallSweepSys), /*bTraceComplex=*/false, IgnoredActor);
    OutQ.bReturnPhysicalMaterial = true;
    OutQ.bFindInitialOverlaps = false;

    OutO = FCollisionObjectQueryParams();
    if (Cfg.bQueryWorldStatic)  { OutO.AddObjectTypesToQuery(ECC_WorldStatic); }
    if (Cfg.bQueryWorldDynamic) { OutO.AddObjectTypesToQuery(ECC_WorldDynamic); }
}

void UBallSweepTraceSubsystem::HandleSweepHit(const FHitResult& Hit, FBallSweepTraceConfig& Cfg,
                                              FBallSweepTraceState& St, AActor* TrackedActor)
{
    // 쿨다운
    if (Cfg.FeedbackCooldown > 0.f)
    {
        const double Now = FPlatformTime::Seconds();
        if ((Now - St.LastFeedbackTime) < Cfg.FeedbackCooldown)
        {
            UE_LOG(LogBallSweepSys, Verbose, TEXT("[HandleHit] 쿨다운 진행중 → 스킵 (%s)"), *TrackedActor->GetName());
            return;
        }
        St.LastFeedbackTime = Now;
    }

    // 위치 선택: 임팩트 포인트가 0이면 액터 현재 위치 사용(방어적)
    FVector ImpactPt = FVector(Hit.ImpactPoint);
    const FVector Where = (ImpactPt.IsNearlyZero() && TrackedActor)
        ? TrackedActor->GetActorLocation()
        : ImpactPt;



    // FX
    if (Cfg.ImpactFX)
    {
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Cfg.ImpactFX, FTransform(Where));
        UE_LOG(LogBallSweepSys, Verbose, TEXT("[HandleHit] FX Spawn @ %s  (Actor=%s)"),
            *Where.ToString(), *GetNameSafe(TrackedActor));
    }

    // 사운드
    if (Cfg.ImpactSound)
    {
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), Cfg.ImpactSound, Where);
        UE_LOG(LogBallSweepSys, Verbose, TEXT("[HandleHit] Sound Play @ %s  (Actor=%s)"),
            *Where.ToString(), *GetNameSafe(TrackedActor));
    }

    // 전역 이벤트 브로드캐스트(옵션)
    OnSweepHitGlobal.Broadcast(TrackedActor, Hit);
}
