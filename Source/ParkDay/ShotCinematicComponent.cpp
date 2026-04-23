#include "ShotCinematicComponent.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "ParkDay/GolfPlayerController.h"
#include "ShotCinematicCameraActor.h"

UShotCinematicComponent::UShotCinematicComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // Tick 사용 안 함
}

void UShotCinematicComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureCameraActor();
}

bool UShotCinematicComponent::PassConditions(
	float ShotStartDistanceCm,
	float EndDistanceCm,
	float ShotYawDeltaDeg,
	float BallSpeedCmPerSec) const
{
	UE_LOG(LogTemp, Log, TEXT("ShotStartDistanceCm = %.1f"), ShotStartDistanceCm);
	UE_LOG(LogTemp, Log, TEXT("EndDistanceCm = %.1f"), EndDistanceCm);
	UE_LOG(LogTemp, Log, TEXT("ShotYawDeltaDeg = %.1f"), ShotYawDeltaDeg);
	UE_LOG(LogTemp, Log, TEXT("BallSpeedCmPerSec = %.1f"), BallSpeedCmPerSec);

	// 1) 볼이 홀컵에서 10m 이상에서 샷
	if (ShotStartDistanceCm < MinStartDistanceCm)
		return false;

	// 2) 샷 후 볼이 홀컵 3m 이내로 진입
	if (EndDistanceCm > MaxEndDistanceCm)
		return false;

	// 3) 좌우각 +-1도 이내
	if (FMath::Abs(ShotYawDeltaDeg) > MaxAbsYawDeg)
		return false;


	// 볼 상태 확인: Ball_rolling 상태일 때만 카메라 배치

	// 4) 볼 속도 >= 남은거리 * 0.3 (단위: cm/s, cm)
	//float RequiredSpeed = ShotStartDistanceCm * SpeedDistanceRatio;
	//UE_LOG(LogTemp, Log, TEXT("RequiredSpeed = %.1f"), RequiredSpeed);
	//// 기존 코드 유지: BallSpeedCmPerSec가 m/s로 들어오는 상황을 고려해 *100 비교
	//if (BallSpeedCmPerSec * 100.f < RequiredSpeed)
	//	return false;
	//// 남은거리 30미터이면 15ms 보다크면 빠른속도이므로 제외
	//float RequiredSpeedMax = ShotStartDistanceCm * 0.7f;
	//UE_LOG(LogTemp, Log, TEXT("RequiredSpeedMax = %.1f"), RequiredSpeedMax);
	//if (BallSpeedCmPerSec * 100.f > RequiredSpeedMax)
	//	return false;

	return true;
}

bool UShotCinematicComponent::EnsureCameraActor()
{
	if (CinematicCamera && !CinematicCamera->IsPendingKillPending())
		return true;

	UWorld* World = GetWorld();
	if (!World)
		return false;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = GetOwner();

	CinematicCamera = World->SpawnActor<AShotCinematicCameraActor>(AShotCinematicCameraActor::StaticClass(), Params);
	if (!CinematicCamera)
		return false;

	CinematicCamera->SetCinematicActive(false);
	return true;
}

FTransform UShotCinematicComponent::ComputeCinematicTransform(
	const FVector& BallLocationCm,
	const FVector& HoleLocationCm,
	float ShotYawDeltaDeg) const
{
	// =========================================================
	// 카메라 배치 기준:
	//   공→홀컵 진행선의 좌측 또는 우측 측면에서 바라보는 구도
	//
	//         카메라(좌측)
	//              |
	//   공 ────────+──────→ 홀컵
	//              |
	//         카메라(우측)
	//
	//   - 기준점: 공-홀컵 선분 위의 랜덤 지점 (공쪽 20%~홀컵쪽 80%)
	//   - 측면 거리: 300~500cm (3~5m)
	//   - 좌/우 랜덤 선택
	//   - 조준 목표: 홀컵 위치 + 100cm 위
	// =========================================================

	// ── 기준 방향 벡터 ──────────────────────────────────────
	const FVector ToCup = HoleLocationCm - BallLocationCm;

	// 공→홀컵 수평 방향 (정규화)
	FVector Forward = FVector(ToCup.X, ToCup.Y, 0.f);
	if (Forward.SizeSquared() > KINDA_SMALL_NUMBER)
		Forward.Normalize();
	else
		Forward = FVector::ForwardVector;

	// 진행선의 오른쪽 수직 벡터 (시계방향 90°)
	// Forward = (Fx, Fy) → Right = (Fy, -Fx)
	const FVector Right = FVector(Forward.Y, -Forward.X, 0.f);

	// ── 1) 좌/우 랜덤 선택 ───────────────────────────────────
	// +1 = 오른쪽, -1 = 왼쪽
	const float Side = (FMath::RandBool()) ? 1.0f : -1.0f;

	// ── 2) 기준점: 공-홀컵 선분 위 랜덤 지점 ────────────────
	// t=0: 공 위치, t=1: 홀컵 위치
	// 공쪽 20% ~ 홀컵쪽 80% 사이에서 랜덤
	const float T = FMath::FRandRange(1.4f, 1.8f);
	const FVector LinePoint = BallLocationCm + ToCup * T;

	// ── 3) 측면 거리 랜덤: 3m ~ 5m ──────────────────────────
	const float SideDistCm = FMath::FRandRange(500.0f, 700.0f);

	// ── 4) 카메라 위치: 기준점에서 좌/우로 이동 ─────────────
	FVector CamPos = FVector(LinePoint.X, LinePoint.Y, HoleLocationCm.Z)
		+ Right * Side * SideDistCm
		+ FVector::UpVector * HeightOffsetCm;

	// ── 5) 조준 목표: 홀컵 100cm 위 ─────────────────────────
	const FVector LookAtTarget = HoleLocationCm + FVector::UpVector * 100.0f;
	const FRotator CamRot = (LookAtTarget - CamPos).Rotation();

	UE_LOG(LogTemp, Log,
		TEXT("[ShotCinematic] 🎬 Side=%s T=%.2f SideDist=%.0fcm Height=%.0fcm | CamPos=(%.0f,%.0f,%.0f)"),
		Side > 0.f ? TEXT("RIGHT") : TEXT("LEFT"),
		T, SideDistCm, HeightOffsetCm,
		CamPos.X, CamPos.Y, CamPos.Z);

	return FTransform(CamRot, CamPos, FVector::OneVector);
}

bool UShotCinematicComponent::TryPlayNearCupCinematic(
	const FVector& BallLocationCm,
	const FVector& HoleLocationCm,
	float ShotStartDistanceCm,
	float ShotYawDeltaDeg,
	float BallSpeedCmPerSec,
	float BlendTimeSec,
	float CrentBallSpeed,
	float HoldTimeSec /*unused*/)
{

	UE_LOG(LogTemp, Log, TEXT("🎯 [ShotCinematic] ShotStartDistanceCm -[%f] BallSpeedCmPerSec -[%f]-HoldTimeSec-[%f]---CurrentSpeed -[%f] .")
		, ShotStartDistanceCm, BallSpeedCmPerSec, HoldTimeSec, CrentBallSpeed);

	if (CrentBallSpeed > 500.0f) return false;
	if (CrentBallSpeed < 100.0f) return false;

	UWorld* World = GetWorld();
	if (!World)
		return false;

	if (!EnsureCameraActor())
		return false;

	const float EndDistanceCm = FVector::Dist(BallLocationCm, HoleLocationCm);
	const bool bIsCheck = PassConditions(ShotStartDistanceCm, EndDistanceCm, ShotYawDeltaDeg, BallSpeedCmPerSec);
	if (!bIsCheck)
		return false;

	AGolfPlayerController* PC = Cast<AGolfPlayerController>(GetOwner());
	if (!PC)
		return false;

	// 이미 연출 중이면 중복 방지: 이전 상태 정리 후 진행
	if (bCinematicActive)
	{
		StopCinematic(0.0f);
	}

	// 이전 뷰 저장
	PreviousViewTarget = PC->GetViewTarget();

	// 카메라 위치/회전 계산 후 적용
	const FTransform CamXf = ComputeCinematicTransform(BallLocationCm, HoleLocationCm, ShotYawDeltaDeg);
	CinematicCamera->SetActorTransform(CamXf);
	CinematicCamera->SetCinematicActive(true);

	// 전환
	PC->SetViewTargetWithBlend(CinematicCamera, BlendTimeSec);

	// Timer 기반 자동 복귀 제거 (요청사항)
	if (HoldTimeSec > 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ShotCinematic] HoldTimeSec is deprecated. Call StopCinematic() to return."));
	}

	bCinematicActive = true;
	return true;
}

void UShotCinematicComponent::StopCinematic(float BlendTimeSec)
{
	UWorld* World = GetWorld();
	if (!World)
		return;

	if (!bCinematicActive)
		return;

	AGolfPlayerController* PC = Cast<AGolfPlayerController>(GetOwner());
	if (!PC)
		return;

	// 원래 뷰로 복귀
	if (PreviousViewTarget && !PreviousViewTarget->IsPendingKillPending())
	{
		PC->SetViewTargetWithBlend(PreviousViewTarget, BlendTimeSec);
	}

	// 연출 카메라 비활성
	if (CinematicCamera)
	{
		CinematicCamera->SetCinematicActive(false);
	}

	PreviousViewTarget = nullptr;
	bCinematicActive = false;
}