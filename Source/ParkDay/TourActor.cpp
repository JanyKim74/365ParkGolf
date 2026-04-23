#include "TourActor.h"

#include "Camera/CameraComponent.h"
#include "Components/SplineComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

// 프로젝트 경로는 실제 구조에 맞게 유지/수정하세요.
#include "ParkDay/InGameMode.h"
#include "ParkDay/Utils/UtilLibrary.h"

ATourActor::ATourActor()
{
	PrimaryActorTick.bCanEverTick = true;

	TourCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TourCamera"));
	RootComponent = TourCamera;
}

void ATourActor::BeginPlay()
{
	Super::BeginPlay();
	GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
	SetState(ETourState::Idle);
}

void ATourActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	switch (State)
	{
	case ETourState::Idle:
		break;

	case ETourState::BlendingIn:
		SetState(ETourState::Lifting);
		break;

	case ETourState::Lifting:
		UpdateLift(DeltaSeconds);
		break;

	case ETourState::PreMoveHold:
		UpdatePreMoveHold(DeltaSeconds);
		break;

	case ETourState::Moving:
		UpdateMove(DeltaSeconds);
		break;
	case ETourState::WaitingAtEnd:
		WaitRemain -= DeltaSeconds;
		if (WaitRemain <= 0.f)
		{
			StopTourAndRestoreNow(); // ✅ 한 군데로 통일
		}
		break;

	case ETourState::BlendingOut:
		break;

	default:
		break;
	}
}

void ATourActor::StartTourByHoleIndex(int32 HoleIndex)
{
	USplineComponent* FoundSpline = nullptr;
	if (!ResolveSplineByHoleIndex(HoleIndex, FoundSpline) || !FoundSpline)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ATourActor] StartTourByHoleIndex failed. HoleIndex=%d"), HoleIndex);
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[ATourActor] StartTourByHoleIndex . StartTourByHoleIndex=%d"), HoleIndex);
	// 기존 프로젝트 흐름 유지(페이드 + UI 숨김)
	UUtilLibrary::FadeIn(GetWorld(), FadeInDuration, FFadeCallback::CreateLambda([this, FoundSpline]()
		{
			if (GM && GM->StrokeWidgetInstance)
			{
				//GM->StrokeWidgetInstance->SetVisibility(ESlateVisibility::Hidden);
				GM->StrokeWidgetInstance->HideAll();
			}

			StartTourWithSpline(FoundSpline);
			UUtilLibrary::FadeOut(GetWorld(), FadeOutDuration);
		}));
}

void ATourActor::ResetTourInternal()
{
	TourSpline = nullptr;

	DistanceAlongSpline = 0.f;
	LiftStartZ = 0.f;
	CurrentLiftZ = 0.f;
	FixedTourZ = 0.f;

	PreMoveHoldRemain = 0.f;
	MoveElapsedSeconds = 0.f;
	WaitRemain = 0.f;

	SetState(ETourState::Idle);
}

bool ATourActor::StartTourWithSpline(USplineComponent* InSpline)
{
	if (!InSpline)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ATourActor] StartTourWithSpline failed. InSpline is null."));
		return false;
	}

	ResetTourInternal();

	TourSpline = InSpline;

	CachedPC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!CachedPC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ATourActor] No PlayerController."));
		TourSpline = nullptr;
		return false;
	}
	OriginalViewTarget = CachedPC->GetViewTarget();

	USplineComponent* Spline = TourSpline.Get();
	if (!Spline)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ATourActor] TourSpline invalid."));
		return false;
	}

	// 시작 지점: 스플라인 첫 포인트(0)에서 시작
	DistanceAlongSpline = 0.f;
	const FVector StartGroundLoc = Spline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);

	// 고정 Z는 시작지점의 바닥Z + FloatHeight로 1회 계산 후 고정
	FixedTourZ = StartGroundLoc.Z + FloatHeight;

	// 리프트 시작 Z는 시작지점 바닥 Z
	LiftStartZ = StartGroundLoc.Z;
	CurrentLiftZ = LiftStartZ;

	// 리프트 중 Pitch 블렌딩이므로, 시작은 수평(Alpha=0)
	SetActorLocation(FVector(StartGroundLoc.X, StartGroundLoc.Y, CurrentLiftZ));
	UpdateLook(DistanceAlongSpline, FixedTourZ, 0.0f);

	// 프리 홀드/이동/대기 변수 초기화
	PreMoveHoldRemain = 0.f;
	MoveElapsedSeconds = 0.f;
	WaitRemain = 0.f;



	ApplyViewTargetToSelf();
	SetState(ETourState::BlendingIn);
	return true;
}

void ATourActor::StopTour(bool bRestoreCamera)
{
	if (bRestoreCamera)
	{
		RestoreViewTarget();
	}


	TourSpline = nullptr;
	CachedPC = nullptr;
	OriginalViewTarget = nullptr;

	State = ETourState::Idle;

	DistanceAlongSpline = 0.f;
	LiftStartZ = 0.f;
	CurrentLiftZ = 0.f;
	FixedTourZ = 0.f;

	PreMoveHoldRemain = 0.f;
	MoveElapsedSeconds = 0.f;
	WaitRemain = 0.f;
}

void ATourActor::UpdateLook(float UseDistanceAlongSpline, float UseFixedZ, float PitchBlendAlpha)
{
	USplineComponent* Spline = TourSpline.Get();
	if (!Spline) return;

	const float Len = Spline->GetSplineLength();

	const float CurD = FMath::Clamp(UseDistanceAlongSpline, 0.f, Len);
	const float LookD = FMath::Clamp(CurD + LookAheadDistance, 0.f, Len);

	const FVector CurGround = Spline->GetLocationAtDistanceAlongSpline(CurD, ESplineCoordinateSpace::World);
	const FVector LookGround = Spline->GetLocationAtDistanceAlongSpline(LookD, ESplineCoordinateSpace::World);

	const FVector CurLoc(CurGround.X, CurGround.Y, UseFixedZ);
	const FVector LookLoc(LookGround.X, LookGround.Y, UseFixedZ);

	FVector Dir = (LookLoc - CurLoc);
	if (Dir.SizeSquared() < 1.f)
	{
		const FVector Tangent = Spline->GetTangentAtDistanceAlongSpline(CurD, ESplineCoordinateSpace::World);
		Dir = FVector(Tangent.X, Tangent.Y, 0.f);
	}

	FRotator LookRot = Dir.Rotation();

	const float Alpha = FMath::Clamp(PitchBlendAlpha, 0.f, 1.f);
	LookRot.Pitch -= FMath::Abs(DownPitchDegrees) * Alpha;

	SetActorRotation(LookRot);
}

bool ATourActor::ResolveSplineByHoleIndex(int32 HoleIndex, USplineComponent*& OutSpline) const
{
	OutSpline = nullptr;
	UWorld* World = GetWorld();
	if (!World) return false;

	// 찾고자 하는 기본 이름 (예: "HoleSpline_1")
	const FString IndexStr = FString::Printf(TEXT("%0*d"), HoleIndexZeroPadDigits, HoleIndex);
	const FString TargetBaseName = HoleSplineActorNamePrefix + IndexStr;

	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(
		World,
		SplineOwnerFilterClass.Get() ? SplineOwnerFilterClass.Get() : AActor::StaticClass(),
		Actors
	);

	for (AActor* A : Actors)
	{
		if (!A) continue;

		const FString ActorName = A->GetName();

		// 1단계: 일단 "HoleSpline_1"로 시작하는지 확인
		if (ActorName.StartsWith(TargetBaseName, ESearchCase::IgnoreCase))
		{
			// 2단계: "HoleSpline_1" 뒤에 바로 숫자가 붙어있는지 확인
			// TargetBaseName의 길이만큼 인덱스를 건너뛰어 다음 문자를 체크합니다.
			if (ActorName.Len() > TargetBaseName.Len())
			{
				TCHAR NextChar = ActorName[TargetBaseName.Len()];

				// 다음 문자가 숫자라면(0~9), 이는 HoleSpline_19 같은 다른 숫자의 일부임
				if (FChar::IsDigit(NextChar))
				{
					continue; // 다른 숫자이므로 무시
				}
			}

			// 여기까지 왔다면 정확히 "HoleSpline_1"이거나 뒤에 숫자가 아닌 문자(예: _, C)가 붙은 것임
			USplineComponent* Spline = A->FindComponentByClass<USplineComponent>();
			if (Spline)
			{
				OutSpline = Spline;
				return true;
			}
		}
	}

	return false;
}

void ATourActor::ApplyViewTargetToSelf()
{
	if (!CachedPC) return;
	CachedPC->SetViewTargetWithBlend(this, BlendInTime);
}

void ATourActor::RestoreViewTarget()
{
	if (!CachedPC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tour] RestoreViewTarget failed: CachedPC null"));
		return;
	}
	if (!OriginalViewTarget)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tour] RestoreViewTarget failed: OriginalViewTarget null"));
		return;
	}

	UUtilLibrary::FadeIn(GetWorld(), FadeInDuration, FFadeCallback::CreateLambda([this]()
		{
			CachedPC->SetViewTargetWithBlend(OriginalViewTarget, BlendOutTime);

			if (GM && GM->StrokeWidgetInstance)
			{
				GM->StrokeWidgetInstance->SetVisibility(ESlateVisibility::Visible);
			}
			UUtilLibrary::FadeOut(GetWorld(), FadeOutDuration);
		}));

}

void ATourActor::UpdateLift(float DeltaSeconds)
{
	USplineComponent* Spline = TourSpline.Get();
	if (!Spline)
	{
		StopTour(true);
		return;
	}

	// 리프트는 시작 Distance(0) 위치에서 수행 (XY는 시작점 유지)
	const FVector GroundLoc = Spline->GetLocationAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::World);

	CurrentLiftZ = FMath::FInterpConstantTo(CurrentLiftZ, FixedTourZ, DeltaSeconds, LiftUpSpeed);
	SetActorLocation(FVector(GroundLoc.X, GroundLoc.Y, CurrentLiftZ));

	// 리프트 진행률에 따라 Pitch: 수평(0) -> DownPitchDegrees로 블렌딩
	float PitchAlpha = 1.0f;
	const float Denom = (FixedTourZ - LiftStartZ);
	if (Denom > KINDA_SMALL_NUMBER)
	{
		PitchAlpha = FMath::Clamp((CurrentLiftZ - LiftStartZ) / Denom, 0.f, 1.f);
		PitchAlpha = FMath::SmoothStep(0.f, 1.f, PitchAlpha);
	}
	UpdateLook(DistanceAlongSpline, FixedTourZ, PitchAlpha);

	// 리프트 완료
	if (FMath::IsNearlyEqual(CurrentLiftZ, FixedTourZ, 0.5f))
	{
		CurrentLiftZ = FixedTourZ;

		// 공중 도달 후 출발 전 멈춤
		PreMoveHoldRemain = FMath::Max(0.f, PreMoveHoldSeconds);
		if (PreMoveHoldRemain > 0.f)
		{
			SetState(ETourState::PreMoveHold);
		}
		else
		{
			MoveElapsedSeconds = 0.f;
			SetState(ETourState::Moving);
		}
	}
}

void ATourActor::UpdatePreMoveHold(float DeltaSeconds)
{
	PreMoveHoldRemain -= DeltaSeconds;
	if (PreMoveHoldRemain <= 0.f)
	{
		PreMoveHoldRemain = 0.f;
		MoveElapsedSeconds = 0.f; // 출발 가속 초기화
		SetState(ETourState::Moving);
		return;
	}

	// 홀드 중에도 최종 Pitch 유지(Alpha=1)
	UpdateLook(DistanceAlongSpline, FixedTourZ, 1.0f);
}
void ATourActor::UpdateMove(float DeltaSeconds)
{
	USplineComponent* Spline = TourSpline.Get();
	if (!Spline)
	{
		StopTour(true);
		return;
	}

	const float SplineLen = Spline->GetSplineLength();
	const float EndThreshold = FMath::Max(EndDistanceTolerance, 1.0f);

	const float Remaining = FMath::Max(0.f, SplineLen - DistanceAlongSpline);

	// 1) 출발 스무스 가속(0->1)
	MoveElapsedSeconds += DeltaSeconds;

	float StartSpeedScale = 1.f;
	if (StartAccelSeconds > KINDA_SMALL_NUMBER)
	{
		const float A = FMath::Clamp(MoveElapsedSeconds / StartAccelSeconds, 0.f, 1.f);
		StartSpeedScale = FMath::SmoothStep(0.f, 1.f, A);
		StartSpeedScale = FMath::Max(StartSpeedScale, 0.02f);
	}

	const float TargetSpeed = MoveSpeed * StartSpeedScale;

	// 2) 감속 알파(1->0) : Remaining이 0에 가까울수록 0
	float DecelAlpha = 1.f;
	const bool bInDecel = (DecelDistance > KINDA_SMALL_NUMBER) && (Remaining <= DecelDistance);
	if (bInDecel)
	{
		float A = FMath::Clamp(Remaining / DecelDistance, 0.f, 1.f); // 1->0
		DecelAlpha = FMath::SmoothStep(0.f, 1.f, A);
	}

	// 3) 최종 속도: 감속 구간에서는 MinEndSpeed까지 "연속적으로" 내려감(0으로는 안 감)
	float EffectiveSpeed = TargetSpeed;
	if (bInDecel)
	{
		// DecelAlpha=1이면 TargetSpeed, 0이면 MinEndSpeed
		EffectiveSpeed = MinEndSpeed + (TargetSpeed - MinEndSpeed) * DecelAlpha;
	}
	else
	{
		// 감속 구간 밖에서 필요하면 최소 속도 보장(원치 않으면 삭제 가능)
		EffectiveSpeed = FMath::Max(EffectiveSpeed, MinEndSpeed);
	}

	// 4) 이번 프레임 이동량
	const float Step = EffectiveSpeed * DeltaSeconds;

	// 5) 도착 처리(멈춤/재가속/스냅 이슈 방지)
	if (Remaining <= EndThreshold || Step >= Remaining)
	{
		DistanceAlongSpline = SplineLen;

		const FVector GroundEnd = Spline->GetLocationAtDistanceAlongSpline(SplineLen, ESplineCoordinateSpace::World);
		SetActorLocation(FVector(GroundEnd.X, GroundEnd.Y, FixedTourZ));
		UpdateLook(SplineLen, FixedTourZ, 1.0f);

		WaitRemain = FMath::Max(0.f, WaitAtEndSeconds);
		SetState(ETourState::WaitingAtEnd);
		return;
	}

	// 6) 이동
	DistanceAlongSpline = FMath::Clamp(DistanceAlongSpline + Step, 0.f, SplineLen);

	const FVector GroundLoc = Spline->GetLocationAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::World);
	SetActorLocation(FVector(GroundLoc.X, GroundLoc.Y, FixedTourZ));
	UpdateLook(DistanceAlongSpline, FixedTourZ, 1.0f);
}

void ATourActor::SetState(ETourState NewState)
{
	State = NewState;
}

// ============================================================================
// SetSplineForHole
//   투어 카메라 이동 없이, 지정 홀의 스플라인을 TourSpline 에만 등록합니다.
//   홀 전환 시 드롭 위치 계산(CalculatePenaltyDropPositionInternal) 등
//   게임 로직이 GetTourSpline() 을 통해 올바른 스플라인을 참조할 수 있게
//   홀 초기화(OnEnterHoleInit) 단계에서 미리 호출해 주세요.
//
//   사용 예 (InGameMode.cpp):
//       if (TourActor)
//           TourActor->SetSplineForHole(CurrentHole);
// ============================================================================
bool ATourActor::SetSplineForHole(int32 HoleIndex)
{
	USplineComponent* FoundSpline = nullptr;

	if (!ResolveSplineByHoleIndex(HoleIndex, FoundSpline) || !FoundSpline)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[ATourActor] SetSplineForHole: 홀 %d 의 스플라인을 찾지 못했습니다."),
			HoleIndex);

		// 이전 홀 스플라인 참조를 안전하게 초기화
		TourSpline = nullptr;
		return false;
	}

	TourSpline = FoundSpline;

	UE_LOG(LogTemp, Log,
		TEXT("[ATourActor] SetSplineForHole: 홀 %d 스플라인 설정 완료 → %s"),
		HoleIndex,
		*FoundSpline->GetOwner()->GetName());

	return true;
}

void ATourActor::StopTourAndRestoreNow()
{
	// 투어 중이 아니라도, 뷰 복구만 안전하게 시도할 수 있음
	// (단, CachedPC/OriginalViewTarget이 유효해야 함)
	if (!CachedPC || !OriginalViewTarget)
	{
		// 혹시라도 상태만 정리
		ResetTourInternal();
		return;
	}

	GM->StrokeWidgetInstance->ShowAll();

	// ✅ 1) 상태머신 즉시 정지 (Tick에서 UpdateLift/Move 못 타게)
	State = ETourState::Idle;

	// ✅ 2) 투어 경로 참조 끊기 (원치 않으면 유지해도 되지만 보통 끊는 게 안전)
	TourSpline = nullptr;

	// ✅ 3) 원래 카메라로 복귀 (기존 FadeIn/Out + UI 복구 로직 재사용)
	RestoreViewTarget();

	// ✅ 4) 내부 변수 리셋(선택)
	// RestoreViewTarget()은 비동기 페이드 람다이긴 하지만,
	// 여기서 투어 내부 상태를 정리해도 문제 없음(다음 투어 시작 안정성 ↑)
	ResetTourInternal();
}