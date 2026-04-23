#include "PuttingGuide.h"
#include "Kismet/GameplayStatics.h"
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "Engine/StaticMeshActor.h"
#include "DrawDebugHelpers.h"
#include "InGameMode.h"

APuttingGuide::APuttingGuide()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void APuttingGuide::BeginPlay()
{
	Super::BeginPlay();
}

FPuttingGuideResult APuttingGuide::AnalyzePutting(
	FVector BallLocation,
	FVector HoleLocation,
	float MeasurementSpacing,
	float MaxDistance)
{
	FPuttingGuideResult Result;
	Result.LeftHeights.Empty();
	Result.RightHeights.Empty();
	Result.LeftMeasurePoints.Empty();
	Result.RightMeasurePoints.Empty();

	// 볼의 Z축 높이 기준점
	float BallBaseHeightCM = BallLocation.Z;

	UE_LOG(LogTemp, Log, TEXT("[PuttingGuide] ===== 퍼팅 분석 시작 ====="));
	UE_LOG(LogTemp, Log, TEXT("[PuttingGuide] 볼 위치: (%.2f, %.2f, %.2f) cm"),
		BallLocation.X, BallLocation.Y, BallLocation.Z);
	UE_LOG(LogTemp, Log, TEXT("[PuttingGuide] 홀 위치: (%.2f, %.2f, %.2f) cm"),
		HoleLocation.X, HoleLocation.Y, HoleLocation.Z);

	// 1단계: 볼에서 홀까지의 방향 벡터 (수평면에서만)
	FVector HoleDirection = GetHorizontalDirection(HoleLocation - BallLocation);

	if (HoleDirection.IsNearlyZero())
	{
		UE_LOG(LogTemp, Error, TEXT("[PuttingGuide] ERROR: 볼과 홀의 위치가 같습니다!"));
		Result.PuttingGuidanceText = TEXT("ERROR: Ball and Hole same location");
		return Result;
	}

	// ✅ 중요: Forward/Left/Right 벡터 계산
	FVector ForwardVector = HoleDirection;  // 홀로 가는 방향
	FVector RightVector = FVector(-HoleDirection.Y, HoleDirection.X, 0.0f).GetSafeNormal();
	FVector LeftVector = -RightVector;

	UE_LOG(LogTemp, Log, TEXT("[PuttingGuide] 홀 방향: (%.2f, %.2f)"),
		HoleDirection.X, HoleDirection.Y);
	UE_LOG(LogTemp, Log, TEXT("[PuttingGuide] 좌측 방향: (%.2f, %.2f)"),
		LeftVector.X, LeftVector.Y);
	UE_LOG(LogTemp, Log, TEXT("[PuttingGuide] 우측 방향: (%.2f, %.2f)"),
		RightVector.X, RightVector.Y);

	// ✅ 2단계: 측정 시작 (수정됨)
	UE_LOG(LogTemp, Log, TEXT("[PuttingGuide] ===== 높이값 측정 시작 ====="));

	float MeasureForwardDistance = MeasurementSpacing;  // 1m부터 시작
	int32 MeasureIndex = 1;

	while (MeasureForwardDistance <= MaxDistance)
	{
		// ✅ 좌측 측정: 앞(ForwardVector) + 좌(LeftVector 50cm)
		FVector LeftMeasurePoint = BallLocation
			+ (ForwardVector * MeasureForwardDistance)
			+ (LeftVector * 50.0f);  // 고정: 50cm 좌측

		float LeftHeightCM = GetTerrainHeightCM(LeftMeasurePoint);

		if (LeftHeightCM > -999999.0f)
		{
			float LeftHeightDiffCM = LeftHeightCM - BallBaseHeightCM;
			Result.LeftHeights.Add(LeftHeightDiffCM);
			Result.LeftMeasurePoints.Add(LeftMeasurePoint);
			Result.LeftMeasureCount++;

			UE_LOG(LogTemp, Log,
				TEXT("[PuttingGuide] 좌측[%d] 거리: %.0fcm (%.1fm) 앞 좌50cm | 높이차: %+.2fcm"),
				MeasureIndex,
				MeasureForwardDistance,
				MeasureForwardDistance / 100.0f,
				LeftHeightDiffCM);
		}

		// ✅ 우측 측정: 앞(ForwardVector) + 우(RightVector 50cm)
		FVector RightMeasurePoint = BallLocation
			+ (ForwardVector * MeasureForwardDistance)
			+ (RightVector * 50.0f);  // 고정: 50cm 우측

		float RightHeightCM = GetTerrainHeightCM(RightMeasurePoint);

		if (RightHeightCM > -999999.0f)
		{
			float RightHeightDiffCM = RightHeightCM - BallBaseHeightCM;
			Result.RightHeights.Add(RightHeightDiffCM);
			Result.RightMeasurePoints.Add(RightMeasurePoint);
			Result.RightMeasureCount++;

			UE_LOG(LogTemp, Log,
				TEXT("[PuttingGuide] 우측[%d] 거리: %.0fcm (%.1fm) 앞 우50cm | 높이차: %+.2fcm"),
				MeasureIndex,
				MeasureForwardDistance,
				MeasureForwardDistance / 100.0f,
				RightHeightDiffCM);
		}

		MeasureForwardDistance += MeasurementSpacing;
		MeasureIndex++;
	}

	Result.TotalMeasureCount = Result.LeftMeasureCount + Result.RightMeasureCount;

	UE_LOG(LogTemp, Log, TEXT("[PuttingGuide] 총 측정 개수 | 좌측: %d, 우측: %d, 합계: %d"),
		Result.LeftMeasureCount,
		Result.RightMeasureCount,
		Result.TotalMeasureCount);

	// ✅ 공과 홀컵 거리 계산 및 저장
	float BallToHoleDistanceCM = FVector::Dist(BallLocation, HoleLocation);
	Result.BallToHoleDistanceCM = BallToHoleDistanceCM;
	Result.MaxDistanceCM = MaxDistance;

	UE_LOG(LogTemp, Log, TEXT("[PuttingGuide] 📏 거리 정보:"));
	UE_LOG(LogTemp, Log, TEXT("[PuttingGuide]   공-홀 거리: %.2fcm (%.2fm)"),
		BallToHoleDistanceCM, BallToHoleDistanceCM / 100.0f);
	UE_LOG(LogTemp, Log, TEXT("[PuttingGuide]   최대 측정거리: %.0fcm (%.1fm)"),
		MaxDistance, MaxDistance / 100.0f);

	// 3단계: 평균 계산
	CalculateAverages(Result);

	// ✅ 추가: 최대 측정거리 저장
	Result.MaxDistanceCM = MaxDistance;

	// 4단계: 편차 계산 및 가이드 결정
	CalculateGuidance(Result);

	// ===== 5단계: 컵 단위 기반 GuidanceWorldPosition 계산 =====
	// 홀컵 위치 기준으로, 공->홀 직선의 직각 방향으로 컵 오프셋만큼 이동한 월드 좌표
	// 패널은 홀컵이 아닌 이 좌표를 투영해서 배치함
	{
		// 컵 단위 -> 오프셋 거리 (cm) 매핑
		// 직진:0  반컵:10  한컵:20  두컵:40  세컵:60  네컵:80
		float OffsetCM = 0.0f;
		const FString& CupText = Result.PuttingGuidanceText;

		if (CupText.Contains(TEXT("반컵")))        OffsetCM = 10.0f;
		else if (CupText.Contains(TEXT("한컵")))   OffsetCM = 20.0f;
		else if (CupText.Contains(TEXT("두컵")))   OffsetCM = 40.0f;
		else if (CupText.Contains(TEXT("세컵")))   OffsetCM = 60.0f;
		else if (CupText.Contains(TEXT("네컵")))   OffsetCM = 80.0f;
		// 직진 또는 직진 포함 텍스트: OffsetCM = 0.0f 유지

		Result.GuidanceOffsetCM = OffsetCM;

		if (OffsetCM < 0.1f || Result.Direction == TEXT("STRAIGHT"))
		{
			// 직진: 홀컵 위치 그대로
			Result.GuidanceWorldPosition = HoleLocation;
		}
		else
		{
			// 공->홀 방향의 직각 벡터 재계산
			FVector HDir = GetHorizontalDirection(HoleLocation - BallLocation);
			FVector RVec = FVector(-HDir.Y, HDir.X, 0.0f).GetSafeNormal();
			FVector LVec = -RVec;

			// 방향에 따라 홀컵에서 오프셋 적용
			FVector OffsetVec = (Result.Direction == TEXT("LEFT")) ? LVec : RVec;
			Result.GuidanceWorldPosition = HoleLocation + (OffsetVec * OffsetCM);
			// Z는 홀컵 Z 유지 (지형 위 기준)
			Result.GuidanceWorldPosition.Z = HoleLocation.Z;
		}

		UE_LOG(LogTemp, Warning,
			TEXT("[PuttingGuide] GuidanceWorldPosition: (%.1f, %.1f, %.1f) | Dir=%s | Offset=%.0fcm"),
			Result.GuidanceWorldPosition.X,
			Result.GuidanceWorldPosition.Y,
			Result.GuidanceWorldPosition.Z,
			*Result.Direction,
			OffsetCM);
	}

	return Result;
}

void APuttingGuide::CalculateAverages(FPuttingGuideResult& Result)
{
	UE_LOG(LogTemp, Log, TEXT("[PuttingGuide] ===== 평균값 계산 ====="));

	// 좌측 평균 = 좌측 높이값 합 / 좌측 측정 개수
	if (Result.LeftMeasureCount > 0)
	{
		float LeftSum = 0.0f;
		for (float Height : Result.LeftHeights)
		{
			LeftSum += Height;
		}
		Result.LeftAverageHeightCM = LeftSum / Result.LeftMeasureCount;

		UE_LOG(LogTemp, Log,
			TEXT("[PuttingGuide] 좌측 평균: %.2fcm (합: %.2fcm / 개수: %d)"),
			Result.LeftAverageHeightCM,
			LeftSum,
			Result.LeftMeasureCount);
	}

	// 우측 평균 = 우측 높이값 합 / 우측 측정 개수
	if (Result.RightMeasureCount > 0)
	{
		float RightSum = 0.0f;
		for (float Height : Result.RightHeights)
		{
			RightSum += Height;
		}
		Result.RightAverageHeightCM = RightSum / Result.RightMeasureCount;

		UE_LOG(LogTemp, Log,
			TEXT("[PuttingGuide] 우측 평균: %.2fcm (합: %.2fcm / 개수: %d)"),
			Result.RightAverageHeightCM,
			RightSum,
			Result.RightMeasureCount);
	}
}

void APuttingGuide::CalculateGuidance(FPuttingGuideResult& Result)
{
	UE_LOG(LogTemp, Log, TEXT("[PuttingGuide] ===== 편차 계산 및 가이드 결정 ====="));

	float AbsDifference = FMath::Abs(Result.LeftAverageHeightCM - Result.RightAverageHeightCM);
	Result.HeightDifferenceCM = AbsDifference;

	UE_LOG(LogTemp, Log,
		TEXT("[PuttingGuide] 좌측평균: %+.2fcm | 우측평균: %+.2fcm | 높이차: %.2fcm"),
		Result.LeftAverageHeightCM,
		Result.RightAverageHeightCM,
		AbsDifference);

	const float DeadZone = 1.0f;

	if (AbsDifference < DeadZone)
	{
		Result.Direction = TEXT("STRAIGHT");
		Result.AdjustmentCM = 0.0f;

		// ✅ 한글 변환
		Result.PuttingGuidanceText = GetDirectionInKorean(Result.Direction);

		UE_LOG(LogTemp, Log, TEXT("[PuttingGuide] 방향: STRAIGHT (편차 < %.1fcm)"), DeadZone);
	}
	else
	{
		bool bLeftIsHigher = Result.LeftAverageHeightCM > Result.RightAverageHeightCM;

		if (bLeftIsHigher)
		{
			Result.Direction = TEXT("LEFT");
		}
		else
		{
			Result.Direction = TEXT("RIGHT");
		}

		Result.AdjustmentCM = AbsDifference;
		// ✅ 1미터 단위 기울기 계산 (cm/m)
		float SlopePerMeter = (AbsDifference / FMath::Max(Result.MaxDistanceCM, 1.0f)) * 100.0f;

		// ✅ 거리를 미터로 변환
		float DistanceInMeter = Result.BallToHoleDistanceCM / 100.0f;

		// ✅ 실제 높이차 = 기울기 × 거리
		float ActualHeightDiff = SlopePerMeter * DistanceInMeter;
		UE_LOG(LogTemp, Log, TEXT("[PuttingGuide] 📊 거리 기반 컵 계산:"));
		UE_LOG(LogTemp, Log, TEXT("[PuttingGuide]   최대거리 편차: %.2fcm"), AbsDifference);
		UE_LOG(LogTemp, Log, TEXT("[PuttingGuide]   최대거리: %.0fcm (%.1fm)"), Result.MaxDistanceCM, Result.MaxDistanceCM / 100.0f);
		UE_LOG(LogTemp, Log, TEXT("[PuttingGuide]   1미터 기울기: %.2fcm/m"), SlopePerMeter);
		UE_LOG(LogTemp, Log, TEXT("[PuttingGuide]   실제 거리: %.2fm"), DistanceInMeter);
		UE_LOG(LogTemp, Log, TEXT("[PuttingGuide]   실제 높이차: %.2fcm"), ActualHeightDiff);

		float Maxheight = ActualHeightDiff * DistanceInMeter; // 미터당 평균높이차를 곱
		UE_LOG(LogTemp, Log, TEXT("[PuttingGuide]   최대 높이고리: %.2fcm   --------------------"), Maxheight);
		FString CupText = GetCupUnitFromHeight(Maxheight);
		FString DirectionKorean = GetDirectionInKorean(Result.Direction);

		Result.PuttingGuidanceText = FString::Printf(
			TEXT("%s %s"),
			*DirectionKorean,
			*CupText);

		UE_LOG(LogTemp, Warning,
			TEXT("[PuttingGuide] 방향: %s | 편차: %.2fcm | 실제높이차: %.2fcm | 컵단위: %s"),
			*DirectionKorean,
			AbsDifference,
			ActualHeightDiff,
			*CupText);
	}

	UE_LOG(LogTemp, Warning, TEXT("[PuttingGuide] ===== 최종 가이드: %s ====="),
		*Result.PuttingGuidanceText);
}

// ✅ Helper 함수: 높이 편차를 컵 단위로 변환 
// 거리당 최대 높이 계산 
FString APuttingGuide::GetCupUnitFromHeight(float HeightDifferenceCM) const
{
	// 컵 크기: 약 10.7cm
	// 반컵: 5.35cm
	// 컵 크기 (골프 규정)
	const float CupDiameter = 10.7f;  // cm

	// 실제 높이차를 컵 단위로 변환
	float CupUnits = HeightDifferenceCM / CupDiameter;
	UE_LOG(LogTemp, Warning, TEXT("[PuttingGuide] ===== CUPUNIT- %f  = ( %f / 10.7f )====="), CupUnits, HeightDifferenceCM);
	// 컵 단위로 분류
	if (CupUnits < 0.5f)
	{
		return TEXT("직진");
	}
	else if (CupUnits < 1.0f)
	{
		return TEXT("반컵");
	}
	else if (CupUnits < 2.0f)
	{
		return TEXT("한컵");
	}
	else if (CupUnits < 3.0f)
	{
		return TEXT("두컵");
	}
	else if (CupUnits < 4.0f)
	{
		return TEXT("세컵");
	}
	else if (CupUnits < 5.0f)
	{
		return TEXT("네컵");
	}
	else
	{
		return TEXT("네컵 이상");
	}
}
void APuttingGuide::PrintPuttingGuidanceResult(const FPuttingGuideResult& Result)
{
	UE_LOG(LogTemp, Warning, TEXT(""));
	UE_LOG(LogTemp, Warning, TEXT("╔════════════════════════════════════════╗"));
	UE_LOG(LogTemp, Warning, TEXT("║       PUTTING GUIDANCE RESULT          ║"));
	UE_LOG(LogTemp, Warning, TEXT("╠════════════════════════════════════════╣"));

	UE_LOG(LogTemp, Warning, TEXT("║ [측정 정보]"));
	UE_LOG(LogTemp, Warning, TEXT("║   좌측 측정 개수: %d"), Result.LeftMeasureCount);
	UE_LOG(LogTemp, Warning, TEXT("║   우측 측정 개수: %d"), Result.RightMeasureCount);
	UE_LOG(LogTemp, Warning, TEXT("║   전체 측정 개수: %d"), Result.TotalMeasureCount);

	UE_LOG(LogTemp, Warning, TEXT("║"));
	UE_LOG(LogTemp, Warning, TEXT("║ [높이값 데이터]"));
	UE_LOG(LogTemp, Warning, TEXT("║   좌측 높이값 개수: %d"), Result.LeftHeights.Num());
	if (Result.LeftHeights.Num() > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("║   좌측 높이값: "));
		for (int32 i = 0; i < Result.LeftHeights.Num(); ++i)
		{
			UE_LOG(LogTemp, Warning, TEXT("║     [%d] %+.2fcm"),
				i + 1, Result.LeftHeights[i]);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("║"));
	UE_LOG(LogTemp, Warning, TEXT("║   우측 높이값 개수: %d"), Result.RightHeights.Num());
	if (Result.RightHeights.Num() > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("║   우측 높이값: "));
		for (int32 i = 0; i < Result.RightHeights.Num(); ++i)
		{
			UE_LOG(LogTemp, Warning, TEXT("║     [%d] %+.2fcm"),
				i + 1, Result.RightHeights[i]);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("║"));
	UE_LOG(LogTemp, Warning, TEXT("║ [분석 결과]"));
	UE_LOG(LogTemp, Warning, TEXT("║   좌측 평균: %+.2fcm"), Result.LeftAverageHeightCM);
	UE_LOG(LogTemp, Warning, TEXT("║   우측 평균: %+.2fcm"), Result.RightAverageHeightCM);
	UE_LOG(LogTemp, Warning, TEXT("║   편차 (좌-우): %+.2fcm"), Result.HeightDifferenceCM);

	UE_LOG(LogTemp, Warning, TEXT("║"));
	UE_LOG(LogTemp, Warning, TEXT("║ [퍼팅 가이드]"));
	UE_LOG(LogTemp, Warning, TEXT("║   방향: %s"), *Result.Direction);
	UE_LOG(LogTemp, Warning, TEXT("║   보정값: %.0fcm"), Result.AdjustmentCM);
	UE_LOG(LogTemp, Warning, TEXT("║   가이드: %s"), *Result.PuttingGuidanceText);

	UE_LOG(LogTemp, Warning, TEXT("╚════════════════════════════════════════╝"));
	UE_LOG(LogTemp, Warning, TEXT(""));
}

void APuttingGuide::DrawDebugVisualization(
	const FVector& BallLocation,
	const FVector& HoleLocation,
	const FPuttingGuideResult& Result,
	float DebugDisplayDuration)
{
	if (!GetWorld())
	{
		return;
	}

	// 볼 위치 표시
	DrawDebugSphere(GetWorld(), BallLocation, 25.0f, 12, FColor::Green, false, DebugDisplayDuration, 0, 2.0f);
	DrawDebugString(GetWorld(), BallLocation + FVector(0, 0, 150.0f), TEXT("BALL"), nullptr, FColor::Green, DebugDisplayDuration);

	// 홀 위치 표시
	DrawDebugSphere(GetWorld(), HoleLocation, 25.0f, 12, FColor::Blue, false, DebugDisplayDuration, 0, 2.0f);
	DrawDebugString(GetWorld(), HoleLocation + FVector(0, 0, 150.0f), TEXT("HOLE"), nullptr, FColor::Blue, DebugDisplayDuration);

	// 볼→홀 직선
	DrawDebugLine(GetWorld(), BallLocation, HoleLocation, FColor::Cyan, false, DebugDisplayDuration, 0, 2.0f);

	// 좌측 측정 포인트들
	for (int32 i = 0; i < Result.LeftMeasurePoints.Num(); ++i)
	{
		FVector Point = Result.LeftMeasurePoints[i];
		float HeightDiff = Result.LeftHeights[i];

		// 측정 포인트
		FColor Color = (HeightDiff < 0) ? FColor::Red : FColor::Yellow;
		DrawDebugSphere(GetWorld(), Point, 15.0f, 8, Color, false, DebugDisplayDuration, 0, 1.5f);

		// 높이 차이 표시 선
		FVector TopPoint = Point + FVector(0, 0, HeightDiff);
		DrawDebugLine(GetWorld(), Point, TopPoint, Color, false, DebugDisplayDuration, 0, 1.0f);

		// 번호 표시
		DrawDebugString(GetWorld(), Point + FVector(0, 0, 100.0f),
			*FString::Printf(TEXT("L%d"), i + 1), nullptr, Color, DebugDisplayDuration);
	}

	// 우측 측정 포인트들
	for (int32 i = 0; i < Result.RightMeasurePoints.Num(); ++i)
	{
		FVector Point = Result.RightMeasurePoints[i];
		float HeightDiff = Result.RightHeights[i];

		// 측정 포인트
		FColor Color = (HeightDiff < 0) ? FColor::Red : FColor::Cyan;
		DrawDebugSphere(GetWorld(), Point, 15.0f, 8, Color, false, DebugDisplayDuration, 0, 1.5f);

		// 높이 차이 표시 선
		FVector TopPoint = Point + FVector(0, 0, HeightDiff);
		DrawDebugLine(GetWorld(), Point, TopPoint, Color, false, DebugDisplayDuration, 0, 1.0f);

		// 번호 표시
		DrawDebugString(GetWorld(), Point + FVector(0, 0, 100.0f),
			*FString::Printf(TEXT("R%d"), i + 1), nullptr, Color, DebugDisplayDuration);
	}

	// 최종 가이드 표시
	FVector GuidanceTextPos = BallLocation + FVector(0, 0, 300.0f);
	FColor GuidanceColor = FColor::White;
	if (Result.Direction == TEXT("LEFT"))
	{
		GuidanceColor = FColor::Red;
	}
	else if (Result.Direction == TEXT("RIGHT"))
	{
		GuidanceColor = FColor::Blue;
	}
	else
	{
		GuidanceColor = FColor::Green;
	}

	DrawDebugString(GetWorld(), GuidanceTextPos, *Result.PuttingGuidanceText,
		nullptr, GuidanceColor, DebugDisplayDuration, true);
}

FString APuttingGuide::GetFormattedGuidanceString(const FPuttingGuideResult& Result) const
{
	FString Output;

	Output += TEXT("\n╔════════════════════════════════════════╗\n");
	Output += TEXT("║       PUTTING GUIDANCE                 ║\n");
	Output += TEXT("╠════════════════════════════════════════╣\n");

	// 좌측 평균
	Output += FString::Printf(
		TEXT("║ LEFT AVG:   %+7.2fcm                    ║\n"),
		Result.LeftAverageHeightCM);

	// 우측 평균
	Output += FString::Printf(
		TEXT("║ RIGHT AVG:  %+7.2fcm                    ║\n"),
		Result.RightAverageHeightCM);

	// 편차
	Output += FString::Printf(
		TEXT("║ DIFFERENCE: %+7.2fcm                    ║\n"),
		Result.HeightDifferenceCM);

	Output += TEXT("║                                        ║\n");

	// 가이드
	Output += FString::Printf(
		TEXT("║ GUIDANCE:   %-35s║\n"),
		*Result.PuttingGuidanceText);

	Output += TEXT("╚════════════════════════════════════════╝\n");

	return Output;
}

float APuttingGuide::GetTerrainHeightCM(FVector InLocation)
{
	if (!GetWorld())
	{
		return -1000000.0f;
	}

	// ✅ Cup_hole + 숫자 이름 패턴 체크 (예: "Cup_hole1", "Cup_hole12")
	auto IsCupHoleActor = [](const AActor* Actor) -> bool
	{
		if (!Actor) return false;
		const FString& Name = Actor->GetName();
		if (!Name.StartsWith(TEXT("Cup_hole"))) return false;

		const int32 PrefixLen = 8; // "Cup_hole".Len()
		for (int32 i = PrefixLen; i < Name.Len(); ++i)
		{
			if (!FChar::IsDigit(Name[i]))
				return false;
		}
		return true;
	};

	// ✅ Landphysic StaticMeshActor 체크
	auto IsLandphysicActor = [](const AActor* Actor) -> bool
	{
		if (!Actor) return false;
		if (!Actor->IsA<AStaticMeshActor>()) return false;
		const FString& Name = Actor->GetName();
		return Name.Contains(TEXT("Landphysic")) || Name.Contains(TEXT("landphysic"));
	};

	FVector StartTrace = InLocation + FVector(0, 0, 10000.0f);
	FVector EndTrace = InLocation - FVector(0, 0, 10000.0f);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.bTraceComplex = true;  // 정확한 메시 검사

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);

	TArray<FHitResult> HitResults;
	bool bHit = GetWorld()->LineTraceMultiByObjectType(
		HitResults,
		StartTrace,
		EndTrace,
		ObjectQueryParams,
		QueryParams
	);

	if (!bHit)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PuttingGuide] GetTerrainHeightCM: No hit at (%.1f, %.1f)"),
			InLocation.X, InLocation.Y);
		return -1000000.0f;
	}

	bool  bFoundTerrain = false;
	float BestTerrainZ = 0.0f;

	for (const FHitResult& Hit : HitResults)
	{
		if (!Hit.GetComponent()) continue;

		AActor* HitActor = Hit.GetActor();
		FString  ComponentClass = Hit.GetComponent()->GetClass()->GetName();

		// ✅ 유효 지형 판별
		bool bIsValidTerrain =
			// 1) 랜드스케이프 컴포넌트/액터 클래스 이름 체크
			ComponentClass.Contains(TEXT("Landscape")) ||
			(HitActor && HitActor->GetClass()->GetName().Contains(TEXT("Landscape"))) ||
			// 2) Cup_hole 홀컵 액터
			IsCupHoleActor(HitActor) ||
			// 3) Landphysic StaticMesh
			IsLandphysicActor(HitActor);

		if (!bIsValidTerrain) continue;

		// ✅ Cup_hole 액터 히트 시 InGameMode 홀컵 Z값 사용
		float HitZ = Hit.ImpactPoint.Z;
		if (IsCupHoleActor(HitActor))
		{
			if (AInGameMode* GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
			{
				int32 HoleIdx = GM->CurrentHole - 1;
				if (GM->MapInfo.HolecupPositions.IsValidIndex(HoleIdx))
				{
					HitZ = GM->MapInfo.HolecupPositions[HoleIdx].Z;
					UE_LOG(LogTemp, Log,
						TEXT("[PuttingGuide] ⛳ Cup_hole hit -> HolecupPositions[%d].Z = %.1f"),
						HoleIdx, HitZ);
				}
			}
		}

		// ✅ InLocation.Z 에 가장 가까운 히트 선택
		if (!bFoundTerrain ||
			FMath::Abs(HitZ - InLocation.Z) < FMath::Abs(BestTerrainZ - InLocation.Z))
		{
			BestTerrainZ = HitZ;
			bFoundTerrain = true;

			UE_LOG(LogTemp, Log,
				TEXT("[PuttingGuide] ✅ Terrain Hit - Actor:[%s] Comp:[%s] Z=%.1f"),
				HitActor ? *HitActor->GetName() : TEXT("null"),
				*ComponentClass,
				HitZ);
		}
	}

	if (!bFoundTerrain)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[PuttingGuide] ⚠️ No valid terrain among %d hits at (%.1f, %.1f)"),
			HitResults.Num(), InLocation.X, InLocation.Y);
		return -1000000.0f;
	}

	return BestTerrainZ;
}

FVector APuttingGuide::GetHorizontalDirection(FVector Direction)
{
	// Z축 제거 후 2D로 정규화
	FVector HorizontalDir(Direction.X, Direction.Y, 0.0f);
	return HorizontalDir.GetSafeNormal();
}
float APuttingGuide::GetAdjustmentAsFloat(const FPuttingGuideResult& Result) const
{
	/**
	 * 편차를 float 값으로 변환
	 *
	 * 로직:
	 * HeightDifference = 좌측평균 - 우측평균
	 *
	 * 양수면 우측(양수), 음수면 좌측(음수)으로 반환
	 *
	 * 예시 1:
	 *   좌측 평균: +2.85cm
	 *   우측 평균: -2.15cm
	 *   편차: 2.85 - (-2.15) = +5.0cm
	 *   → 좌측이 5cm 높음
	 *   → 우측으로 보정 필요
	 *   → 반환값: +5.0f (양수)
	 *
	 * 예시 2:
	 *   좌측 평균: -2.5cm
	 *   우측 평균: +2.5cm
	 *   편차: -2.5 - 2.5 = -5.0cm
	 *   → 우측이 5cm 높음
	 *   → 좌측으로 보정 필요
	 *   → 반환값: -5.0f (음수)
	 */

	const float DeadZone = 1.0f; // 1cm 미만은 0으로 처리

	if (FMath::Abs(Result.HeightDifferenceCM) < DeadZone)
	{
		// STRAIGHT 케이스: 보정 불필요
		UE_LOG(LogTemp, Log, TEXT("[PuttingGuide] GetAdjustmentAsFloat: STRAIGHT (0.0f)"));
		return 0.0f;
	}

	// HeightDifference를 그대로 반환
	// 양수: RIGHT (우측으로 보정)
	// 음수: LEFT (좌측으로 보정)

	if (Result.HeightDifferenceCM > 0.0f)
	{
		UE_LOG(LogTemp, Log, TEXT("[PuttingGuide] GetAdjustmentAsFloat: RIGHT +%.2fcm"),
			Result.HeightDifferenceCM);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[PuttingGuide] GetAdjustmentAsFloat: LEFT %.2fcm"),
			Result.HeightDifferenceCM);
	}

	return Result.HeightDifferenceCM;
}

FString APuttingGuide::GetDirectionInKorean(const FString& Direction) const
{
	if (Direction == TEXT("LEFT"))
	{
		return TEXT("왼쪽");
	}
	else if (Direction == TEXT("RIGHT"))
	{
		return TEXT("오른쪽");
	}
	else if (Direction == TEXT("STRAIGHT"))
	{
		return TEXT("직진");
	}

	return TEXT("알 수 없음");  // Fallback
}