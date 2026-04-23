// BallSweepTraceSubsystem.h
// 목적: 액터(예: 골프공)에 직접 컴포넌트를 붙이지 않고,
//       월드 전역에서 "이전 위치 → 현재 위치" 경로를 구체 스윕으로 검사.
//       Overlap 이벤트가 불가능한 메쉬(SplineMesh/HISM 등)도 "지나갔다"를 안정적으로 감지.
//
// 사용 흐름:
// 1) 월드가 시작되면 Subsystem이 활성화됨(자동).
// 2) 감시할 액터(공 등)가 스폰/활성화될 때 RegisterActor로 등록.
// 3) 서브시스템이 SampleInterval 간격으로 각 액터 경로를 스윕.
// 4) 히트 시: FX/사운드 재생, 델리게이트 브로드캐스트, 상세 로그/디버그 표시.
//
// 주의:
// - UE 4.26에서 UWorldSubsystem 사용 가능.
// - Tick 대신 Timer 기반 샘플링(성능/제어 용이).
// - 모듈명은 YOURMODULE로 가정. 실제 프로젝트 모듈명으로 바꿔줘.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BallSweepTraceSubsystem.generated.h"

// 로깅 카테고리
DECLARE_LOG_CATEGORY_EXTERN(LogBallSweepSys, Log, All);

// 외부 바인딩용: 서브시스템이 히트를 감지하면 호출되는 이벤트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSweepHitGlobal, AActor*, TrackedActor, const FHitResult&, Hit);

// 감시 설정(액터별 커스텀 세팅)
USTRUCT(BlueprintType)
struct FBallSweepTraceConfig
{
    GENERATED_BODY()

    // 스윕 구체 반지름(액터 크기보다 살짝 크게)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TraceRadius = 10.f;

    // 너무 느리면 검사 스킵(속도 기준, cm/s)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinSpeedToCheck = 5.f;

    // 샘플링 간격(초). 0이면 서브시스템 전역 SampleInterval 사용
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OverrideSampleInterval = 0.f;

    // 긴 경로 세분화
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxSubsteps = 8;

    // 세그먼트 최대 길이
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxStepDistance = 100.f;

    // 태그 필터(비어있으면 모두 허용)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FName> AcceptTagFilters;

    // 오브젝트 타입 쿼리 옵션
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bQueryWorldStatic = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bQueryWorldDynamic = true;

    // 피드백 쿨다운(초) — 연속 트리거 억제
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FeedbackCooldown = 0.2f;

    // (옵션) FX/사운드 — 액터별 지정 가능
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    class UParticleSystem* ImpactFX = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    class USoundBase* ImpactSound = nullptr;

    // 디버그 드로잉
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bDrawDebug = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DebugPersistTime = 1.5f;
};

// 내부 상태(액터별)
USTRUCT()
struct FBallSweepTraceState
{
    GENERATED_BODY()

    // 마지막 샘플 위치
    FVector PrevLocation = FVector::ZeroVector;

    // 마지막 피드백 시간(쿨다운)
    double LastFeedbackTime = -DBL_MAX;

    // 개별 타이머(개별 간격을 쓰는 경우)
    FTimerHandle TimerHandle;

    // 등록 시점의 월드 유효성 체크용(옵션)
    TWeakObjectPtr<AActor> Owner;
};


UCLASS()
class PARKDAY_API UBallSweepTraceSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    UBallSweepTraceSubsystem();

    // 서브시스템 전역 샘플링 간격(초). 개별 설정 Override가 0이면 이 값을 사용.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sweep|Config")
    float SampleInterval = 0.05f;

    // 전역 이벤트(옵션): 누가 맞았는지와 Hit 정보를 브로드캐스트
    UPROPERTY(BlueprintAssignable, Category="Sweep|Event")
    FOnSweepHitGlobal OnSweepHitGlobal;

    // —— 등록/해제 API ————————————————————————————————

    // 감시 대상 등록(필수)
    UFUNCTION(BlueprintCallable, Category="Sweep|API")
    void RegisterActor(AActor* Actor, const FBallSweepTraceConfig& Config);

    // 등록 해제(권장: 소멸/비활성 시)
    UFUNCTION(BlueprintCallable, Category="Sweep|API")
    void UnregisterActor(AActor* Actor);

    // 등록 여부 조회
    UFUNCTION(BlueprintCallable, Category="Sweep|API")
    bool IsRegistered(AActor* Actor) const;

protected:
    // 라이프사이클
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

private:
    // 액터 → (설정, 상태)
    UPROPERTY()
    TMap<TWeakObjectPtr<AActor>, FBallSweepTraceConfig> TrackedConfigs;

    TMap<TWeakObjectPtr<AActor>, FBallSweepTraceState> TrackedStates;

    // 전역 타이머(개별 Override가 없을 때 공용으로 사용)
    FTimerHandle GlobalTimerHandle;

    // 타이머 콜백(전역)
    void TickAll();

    // 타이머 콜백(개별)
    void TickOne(AActor* Actor);

    // 단일 액터 샘플링 & 경로 스윕
    void SampleAndTrace(AActor* Actor, FBallSweepTraceConfig& Cfg, FBallSweepTraceState& St);

    // 경로를 세분화하여 스윕
    void SweepPath(UWorld* World, const FVector& From, const FVector& To, const FBallSweepTraceConfig& Cfg, FBallSweepTraceState& St, AActor* TrackedActor);

    // 실제 스윕(한 세그먼트)
    bool SweepSegment(UWorld* World, const FVector& From, const FVector& To, const FBallSweepTraceConfig& Cfg, FHitResult& OutHit, AActor* IgnoredActor) const;

    // 태그 필터
    bool PassTagFilter(const TArray<FName>& Filters, const UPrimitiveComponent* Comp) const;

    // 쿼리 파라미터 구성
    void BuildQueryParams(FCollisionQueryParams& OutQ, FCollisionObjectQueryParams& OutO, const FBallSweepTraceConfig& Cfg, AActor* IgnoredActor) const;

    // 피드백 처리(FX/사운드/이벤트/디버그/쿨다운 갱신)
    void HandleSweepHit(const FHitResult& Hit, FBallSweepTraceConfig& Cfg, FBallSweepTraceState& St, AActor* TrackedActor);
};
