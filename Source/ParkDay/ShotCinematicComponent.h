#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShotCinematicComponent.generated.h"

class AShotCinematicCameraActor;

/**
 * 샷 연출 카메라 컴포넌트 (UE4.26)
 * - TryPlayNearCupCinematic() : 조건 만족 시 연출 카메라로 전환
 * - StopCinematic() : 외부에서 원하는 타이밍에 원래 뷰로 복귀
 *
 * NOTE:
 *  - Tick 사용 안 함 (요청사항)
 *  - Timer 사용 안 함 (요청사항)
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PARKDAY_API UShotCinematicComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UShotCinematicComponent();

	virtual void BeginPlay() override;

	/** 조건 검사 */
	bool PassConditions(
		float ShotStartDistanceCm,
		float EndDistanceCm,
		float ShotYawDeltaDeg,
		float BallSpeedCmPerSec) const;

	/** 카메라 액터 준비 */
	bool EnsureCameraActor();

	/** 연출 카메라 Transform 계산 */
	FTransform ComputeCinematicTransform(
		const FVector& BallLocationCm,
		const FVector& HoleLocationCm,
		float ShotYawDeltaDeg) const;

	/**
	 * 연출 카메라 실행 (조건 만족 시)
	 * @param HoldTimeSec : 더 이상 사용하지 않음(호환을 위해 남겨둠)
	 */
	UFUNCTION(BlueprintCallable, Category="Cinematic|Shot")
	bool TryPlayNearCupCinematic(
		const FVector& BallLocationCm,
		const FVector& HoleLocationCm,
		float ShotStartDistanceCm,
		float ShotYawDeltaDeg,
		float BallSpeedCmPerSec,
		float BlendTimeSec = 0.35f,
		float CrentBallSpeed = 0.0f,
		float HoldTimeSec = 0.0f);

	/** 외부에서 호출해서 원래 뷰로 복귀 */
	UFUNCTION(BlueprintCallable, Category="Cinematic|Shot")
	void StopCinematic(float BlendTimeSec = 0.25f);

	/** 현재 연출 중인지 */
	UFUNCTION(BlueprintCallable, Category="Cinematic|Shot")
	bool IsCinematicActive() const { return bCinematicActive; }

public:
	// ---- 튜닝 파라미터 ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic|Shot")
	float MinStartDistanceCm = 1000.f; // 30m

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic|Shot")
	float MaxEndDistanceCm = 300.f; // 3m

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic|Shot")
	float MaxAbsYawDeg = 1.0f; // +-1도

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic|Shot")
	float SpeedDistanceRatio = 0.3f;

	/**
	 * 홀컵 "뒤쪽"으로 물러나는 비율
	 * - BasePos = Hole + Dir(Ball->Hole) * (Dist * BehindFraction)
	 * - Dist=2000cm, BehindFraction=0.20 => 홀에서 400cm 뒤쪽
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic|Shot")
	float BehindFraction = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic|Shot")
	float HeightOffsetCm = 250.f;

	// (호환용으로 남겨둠: 현재 뒤쪽 연출에서는 사용 안 함)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic|Shot")
	float SideOffsetFraction = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic|Shot")
	float MinSideOffsetCm = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic|Shot")
	float MaxSideOffsetCm = 150.f;

private:
	UPROPERTY(Transient)
	AShotCinematicCameraActor* CinematicCamera = nullptr;

	UPROPERTY(Transient)
	AActor* PreviousViewTarget = nullptr;

	UPROPERTY(Transient)
	bool bCinematicActive = false;

	// ⭐ 새로 추가: 최소 카메라 거리 (cm)

	float MinCameraDistanceCm = 200.0f;  // 2미터
};
