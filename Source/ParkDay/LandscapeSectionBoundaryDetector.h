// LandscapeSectionBoundaryDetector.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "LandscapeInfo.h"
#include "LandscapeDataAccess.h"
#include "DrawDebugHelpers.h"

/**
 * UE4 Landscape 섹션 경계면 감지 및 노말값 스무딩 클래스
 */
class PARKDAY_API FLandscapeSectionBoundaryDetector
{
public:
    struct FLandscapeHitInfo
    {
        FVector Normal;
        FVector Location;
        bool bIsSectionBoundary;
        float NormalVariance;
        TArray<FVector> SampleNormals;
        ULandscapeComponent* HitComponent;
        FIntPoint ComponentCoordinates;
        FVector2D LocalCoordinates;

        FLandscapeHitInfo()
        {
            Normal = FVector::UpVector;
            Location = FVector::ZeroVector;
            bIsSectionBoundary = false;
            NormalVariance = 0.0f;
            HitComponent = nullptr;
            ComponentCoordinates = FIntPoint::ZeroValue;
            LocalCoordinates = FVector2D::ZeroVector;
        }
    };
    // ✅ 9단계: 기회 표시 안전 처리
private:
    // 섹션 경계면 감지 임계값
    static const float SECTION_BOUNDARY_THRESHOLD;  // 노말 변화 임계값 (15도)
    static const float NORMAL_VARIANCE_THRESHOLD;   // 분산 임계값 (10도)
    static const int32 SAMPLE_COUNT;                 // 샘플링 개수 (9개)
    static const float SAMPLE_RADIUS;                // 샘플링 반지름 (20cm)

public:
    /**
     * 메인 함수: Landscape 충돌 시 섹션 경계면 감지 및 노말 스무딩
     */
    static FLandscapeHitInfo AnalyzeLandscapeHit(
        const FHitResult& Hit,
        UWorld* World,
        bool bApplySmoothing = true
    );

    /**
     * 섹션 경계면 감지
     */
    static bool IsSectionBoundary(
        const FVector& HitLocation,
        ULandscapeComponent* Component,
        UWorld* World,
        float& OutNormalVariance
    );

    /**
     * 다중 샘플링을 통한 노말값 스무딩
     */
    static FVector GetSmoothedNormal(
        const FVector& CenterLocation,
        UWorld* World,
        float SampleRadius = 20.0f,
        int32 SampleCount = 9
    );

    /**
     * Landscape 컴포넌트 경계 체크
     */
    static bool IsNearComponentBoundary(
        const FVector& WorldLocation,
        ULandscapeComponent* Component,
        float BoundaryThreshold = 50.0f  // 50cm 이내
    );

    /**
     * 노말 벡터들의 분산 계산
     */
    static float CalculateNormalVariance(const TArray<FVector>& Normals);

    /**
     * 가중 평균을 통한 노말 스무딩 (거리 기반 가중치)
     */
    static FVector GetWeightedAverageNormal(
        const TArray<FVector>& Normals,
        const TArray<float>& Weights
    );

    /**
     * 샘플 위치 생성 (원형 패턴)
     */
    static TArray<FVector> GenerateSamplePositions(
        const FVector& CenterLocation,
        float Radius,
        int32 Count
    );

    /**
     * 특정 위치에서 노말값 가져오기
     */
    static FVector GetNormalAtPosition(
        const FVector& WorldLocation,
        UWorld* World
    );

    /**
     * 노말 벡터를 합리적 범위로 제한
     */
    static FVector ClampNormalToReasonableRange(const FVector& Normal);

    /**
     * 컴포넌트 좌표 계산
     */
    static void CalculateComponentCoordinates(
        const FVector& WorldLocation,
        ULandscapeComponent* Component,
        FLandscapeHitInfo& HitInfo
    );

    /**
     * 디버그 시각화
     */
    static void DebugVisualizeSectionBoundary(
        UWorld* World,
        const FLandscapeHitInfo& HitInfo,
        float Duration = 5.0f
    );

    static FVector GetNormalAtPositionImproved(
        const FVector& WorldLocation,
        UWorld* World
    );

    static bool CheckAdjacentComponents(
        const FVector& HitLocation,
        ULandscapeComponent* Component
    );

    static float CalculateHeightVariance(
        const FVector& CenterLocation,
        UWorld* World
    );

    static void LogDetailedBoundaryInfo(
        const FVector& HitLocation,
        ULandscapeComponent* Component,
        const FLandscapeHitInfo& HitInfo
    );


    /**
 * 개선된 인접 컴포넌트 체크 (8방향, 완화된 기준)
 */
    static bool CheckAdjacentComponentsImproved(
        const FVector& HitLocation,
        ULandscapeComponent* Component
    );

    /**
     * 노말 변화 패턴 감지 (새로운 감지 방법)
     */
    static bool DetectNormalChangePattern(const TArray<FVector>& SampleNormals);

    /**
     * 개선된 컴포넌트 경계 체크 (완화된 임계값)
     */
    static bool IsNearComponentBoundaryImproved(
        const FVector& WorldLocation,
        ULandscapeComponent* Component,
        float BoundaryThreshold = 50.0f  // 기본값을 50cm로 증가
    );
};