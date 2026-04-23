#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PuttingGuide.generated.h"

/**
 * 퍼팅 가이드 시스템
 *
 * 볼에서 홀까지의 직선에서 1미터씩 좌우 높이값을 측정하여
 * 퍼팅 방향을 계산합니다.
 *
 * 로직:
 * 1. 볼→홀 직선 기준, 좌우 1m, 2m, 3m... 높이값 측정
 * 2. 좌측 모든 높이값을 미터로 나눔 (평균 계산)
 * 3. 우측 모든 높이값을 미터로 나눔 (평균 계산)
 * 4. 좌측평균 - 우측평균 = 편차
 * 5. 편차 < 0 → 좌측, 편차 > 0 → 우측으로 보정
 */

USTRUCT(BlueprintType)
struct FPuttingGuideResult
{
	GENERATED_BODY()

		// ===== 측정 데이터 =====
		// 좌측 높이값 배열 (센티미터)
		UPROPERTY(BlueprintReadOnly, Category = "Putting")
		TArray<float> LeftHeights;

	// 우측 높이값 배열 (센티미터)
	UPROPERTY(BlueprintReadOnly, Category = "Putting")
		TArray<float> RightHeights;

	// ===== 분석 결과 =====
	// 좌측 높이값 합 / 좌측 측정 개수 = 좌측 평균
	UPROPERTY(BlueprintReadOnly, Category = "Putting")
		float LeftAverageHeightCM = 0.0f;

	// 우측 높이값 합 / 우측 측정 개수 = 우측 평균
	UPROPERTY(BlueprintReadOnly, Category = "Putting")
		float RightAverageHeightCM = 0.0f;

	// 좌측평균 - 우측평균 = 편차
	// 음수 = 좌측이 낮음 (우측이 높음) → 좌측으로 쏨
	// 양수 = 좌측이 높음 (우측이 낮음) → 우측으로 쏨
	UPROPERTY(BlueprintReadOnly, Category = "Putting")
		float HeightDifferenceCM = 0.0f;

	// ===== 퍼팅 가이드 =====
	// 실제 보정값 (센티미터)
	// "LEFT 10cm" 또는 "RIGHT 5cm" 형식
	UPROPERTY(BlueprintReadOnly, Category = "Putting")
		FString PuttingGuidanceText;

	// 방향 (LEFT, RIGHT, STRAIGHT)
	UPROPERTY(BlueprintReadOnly, Category = "Putting")
		FString Direction;

	// 보정값 절댓값 (센티미터)
	UPROPERTY(BlueprintReadOnly, Category = "Putting")
		float AdjustmentCM = 0.0f;

	// ===== 메타 데이터 =====
	UPROPERTY(BlueprintReadOnly, Category = "Putting")
		int32 LeftMeasureCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Putting")
		int32 RightMeasureCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Putting")
		int32 TotalMeasureCount = 0;

	// 디버그용 측정 포인트
	UPROPERTY(BlueprintReadOnly, Category = "Putting")
		TArray<FVector> LeftMeasurePoints;

	UPROPERTY(BlueprintReadOnly, Category = "Putting")
		TArray<FVector> RightMeasurePoints;

	UPROPERTY(BlueprintReadOnly, Category = "Putting")
		float MaxDistanceCM = 0.0f;  // 최대 측정거리 (cm)

	UPROPERTY(BlueprintReadOnly, Category = "Putting")
		float AverageSlope = 0.0f;  // 평균 기울기 (무차원)

	UPROPERTY(BlueprintReadOnly, Category = "Putting")
		float BallToHoleDistanceCM = 0.0f;  // 공과 홀컵 거리

		// ===== 패널 배치용 월드 좌표 =====
	// 컵 단위 계산 결과를 3D 월드 좌표로 저장
	// 직진: 홀컵 위치 그대로
	// 좌/우: 홀컵에서 공->홀 직선의 직각 방향으로 컵 단위 거리만큼 이동한 좌표
	//   반컵=10cm, 한컵=20cm, 두컵=40cm, 세컵=60cm, 네컵=80cm
	UPROPERTY(BlueprintReadOnly, Category = "Putting")
		FVector GuidanceWorldPosition = FVector::ZeroVector;

	// 컵 단위 오프셋 거리 (cm): 패널 배치에 사용된 실제 오프셋
	UPROPERTY(BlueprintReadOnly, Category = "Putting")
		float GuidanceOffsetCM = 0.0f;
};

UCLASS()
class PARKDAY_API APuttingGuide : public AActor
{
	GENERATED_BODY()

public:
	APuttingGuide();

	virtual void BeginPlay() override;

	/**
	 * 퍼팅 가이드 분석 실행
	 *
	 * @param BallLocation 볼의 위치
	 * @param HoleLocation 홀의 위치
	 * @param MeasurementSpacing 측정 간격 (기본값: 100cm = 1m)
	 * @param MaxDistance 최대 측정 거리 (기본값: 1000cm = 10m)
	 * @return 퍼팅 가이드 결과
	 *
	 * 예시:
	 * FPuttingGuideResult Result = PuttingGuide->AnalyzePutting(
	 *     Ball->GetActorLocation(),
	 *     Hole->GetActorLocation(),
	 *     100.0f,  // 1m 간격
	 *     1000.0f  // 최대 10m
	 * );
	 * // Result.PuttingGuidanceText = "RIGHT 10cm"
	 */
	UFUNCTION(BlueprintCallable, Category = "Golf|Putting")
		FPuttingGuideResult AnalyzePutting(
			FVector BallLocation,
			FVector HoleLocation,
			float MeasurementSpacing = 100.0f,
			float MaxDistance = 1000.0f
		);

	/**
	 * 퍼팅 가이드 결과를 로그로 상세히 출력
	 */
	UFUNCTION(BlueprintCallable, Category = "Golf|Putting")
		void PrintPuttingGuidanceResult(const FPuttingGuideResult& Result);

	/**
	 * 디버그 시각화를 그립니다
	 */
	UFUNCTION(BlueprintCallable, Category = "Golf|Putting")
		void DrawDebugVisualization(
			const FVector& BallLocation,
			const FVector& HoleLocation,
			const FPuttingGuideResult& Result,
			float DebugDisplayDuration = 5.0f
		);

	/**
	 * 퍼팅 가이드 결과를 포맷된 문자열로 반환
	 */
	UFUNCTION(BlueprintCallable, Category = "Golf|Putting")
		FString GetFormattedGuidanceString(const FPuttingGuideResult& Result) const;

	/**
	 * 퍼팅 가이드를 float 값으로 반환 (음수/양수로 방향 표현)
	 *
	 * @param Result 퍼팅 가이드 분석 결과
	 * @return float 보정값 (센티미터 단위)
	 *   - 음수: 좌측으로 보정 (예: -10.0 = LEFT 10cm)
	 *   - 양수: 우측으로 보정 (예: +10.0 = RIGHT 10cm)
	 *   - 0.0: 일직선 (STRAIGHT)
	 *
	 * 예시:
	 *   Result.HeightDifference = +5.0cm
	 *   → GetAdjustmentAsFloat() = +5.0f (RIGHT 5cm)
	 *
	 *   Result.HeightDifference = -5.0cm
	 *   → GetAdjustmentAsFloat() = -5.0f (LEFT 5cm)
	 */
	UFUNCTION(BlueprintCallable, Category = "Golf|Putting")
		float GetAdjustmentAsFloat(const FPuttingGuideResult& Result) const;

private:
	/**
	 * 지면의 높이값을 구합니다 (LineTrace를 이용)
	 *
	 * @param InLocation 측정할 위치
	 * @return Z축 높이값 (센티미터 단위)
	 */
	float GetTerrainHeightCM(FVector InLocation);

	/**
	 * 벡터를 정규화하고 Z축을 제거합니다 (2D 평면)
	 */
	FVector GetHorizontalDirection(FVector Direction);

	/**
	 * 계산 로직의 핵심 부분
	 * 좌측 높이값들의 합 / 좌측 측정 개수 = 좌측 평균
	 */
	void CalculateAverages(FPuttingGuideResult& Result);

	/**
	 * 편차 계산 및 가이드 결정
	 * 좌측평균 - 우측평균 = 편차
	 */
	void CalculateGuidance(FPuttingGuideResult& Result);


	/**
	 * 높이 편차를 컵 단위로 변환
	 * @param HeightDifferenceCM 높이 차이 (cm)
	 * @return 컵 단위 문자열 ("반컵", "1컵", "2컵" 등)
	 */
	FString GetCupUnitFromHeight(float HeightDifferenceCM) const;

	/**
 * 방향을 한글로 변환
 * @param Direction "LEFT", "RIGHT", "STRAIGHT"
 * @return "왼쪽", "오른쪽", "직진"
 */
	FString GetDirectionInKorean(const FString& Direction) const;


};