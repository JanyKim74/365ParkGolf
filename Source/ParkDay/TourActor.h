#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TourActor.generated.h"

class USplineComponent;
class UCameraComponent;
class AInGameMode;
class APlayerController;

UENUM(BlueprintType)
enum class ETourState : uint8
{
	Idle,
	BlendingIn,
	Lifting,
	PreMoveHold,
	Moving,
	WaitingAtEnd,
	BlendingOut
};

UCLASS()
class PARKDAY_API ATourActor : public AActor
{
	GENERATED_BODY()

public:
	ATourActor();

	UFUNCTION(BlueprintCallable, Category = "Tour")
		void StartTourByHoleIndex(int32 HoleIndex);

	UFUNCTION(BlueprintCallable, Category = "Tour")
		bool StartTourWithSpline(USplineComponent* InSpline);

	UFUNCTION(BlueprintCallable, Category = "Tour")
		void StopTour(bool bRestoreCamera = true);

	UFUNCTION(BlueprintPure, Category = "Tour")
		USplineComponent* GetTourSpline() const { return TourSpline.Get(); }

	/**
	 * 지정한 홀 인덱스의 스플라인을 찾아 TourSpline 으로 설정만 합니다.
	 * 투어 카메라 이동은 시작하지 않으며, 드롭 위치 계산 등 게임 로직에서
	 * GetTourSpline() 으로 참조할 수 있도록 사전 등록하는 용도입니다.
	 *
	 * @param HoleIndex  설정할 홀 번호 (1-based)
	 * @return 스플라인을 성공적으로 찾아 설정했으면 true
	 */
	UFUNCTION(BlueprintCallable, Category = "Tour")
		bool SetSplineForHole(int32 HoleIndex);
	UFUNCTION(BlueprintCallable, Category = "Tour")
		void StopTourAndRestoreNow();
protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	// ---------------------------------------------------------------------
	// Find Spline Policy (Actor Name 기반)
	// ---------------------------------------------------------------------

	/** 레벨에 배치된 스플라인 액터 이름 Prefix (예: "aim_hole") */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tour|FindSpline")
		FString HoleSplineActorNamePrefix = TEXT("aim_hole");

	/** 인덱스를 이름에 붙일 때 0 패딩 자릿수 (예: 2면 01,02..) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tour|FindSpline")
		int32 HoleIndexZeroPadDigits = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tour|FindSpline")
		TSubclassOf<AActor> SplineOwnerFilterClass;

	// ---------------------------------------------------------------------
	// Components
	// ---------------------------------------------------------------------
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tour|Components")
		UCameraComponent* TourCamera = nullptr;

	// ---------------------------------------------------------------------
	// Camera
	// ---------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tour|Camera")
		float BlendInTime = 0.0f;

	/** 복귀 시 "되돌아가는 느낌" 없애려면 0 권장 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tour|Camera")
		float BlendOutTime = 0.0f;

	// ---------------------------------------------------------------------
	// Move
	// ---------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tour|Move")
		float MoveSpeed = 700.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tour|Move")
		float LookAheadDistance = 150.f;

	/** 최종 목표 아래보기 각도(양수 권장). 리프트 중 0 -> 이 값으로 블렌딩 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tour|Move")
		float DownPitchDegrees = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tour|Move")
		float StartDistance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tour|Move")
		float EndDistanceTolerance = 5.f;

	/** 스플라인(바닥) 기준으로 떠 있을 높이(cm). 리프트 완료 후 Z는 이 값 기준으로 "고정" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tour|Move")
		float FloatHeight = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tour|Move")
		float LiftUpSpeed = 400.f;

	/** 공중 고정 Z 도달 후, 출발 전 멈춤 시간(초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tour|Move")
		float PreMoveHoldSeconds = 1.33f;

	/** 출발 시 가속을 부드럽게 만드는 시간(초). 0이면 즉시 최고속 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tour|Move")
		float StartAccelSeconds = 1.25f;

	/** 마지막 도착 직전에 감속을 시작할 거리(cm). 0이면 감속 없음 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tour|Move")
		float DecelDistance = 600.f;

	/** 감속 구간 최소 속도(cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tour|Move")
		float MinEndSpeed = 90.f;

	// ---------------------------------------------------------------------
	// End
	// ---------------------------------------------------------------------
	/*마지막 도착 후 기다리는 시갅*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tour|End")
		float WaitAtEndSeconds = 3.f;

	/*화전 전환 시 페이드 인 지속시간 (초)*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tour|Fade")
		float FadeInDuration = 0.6f;
	/*화전 전환 시 페이드 아웃 지속시간 (초)*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tour|Fade")
		float FadeOutDuration = 0.6f;

private:
	/** PitchBlendAlpha=0이면 수평(피치0), 1이면 DownPitchDegrees 적용 */
	void UpdateLook(float UseDistanceAlongSpline, float UseFixedZ, float PitchBlendAlpha);

	bool ResolveSplineByHoleIndex(int32 HoleIndex, USplineComponent*& OutSpline) const;

	void ApplyViewTargetToSelf();
	void RestoreViewTarget();

	void UpdateLift(float DeltaSeconds);
	void UpdatePreMoveHold(float DeltaSeconds);
	void UpdateMove(float DeltaSeconds);

	void SetState(ETourState NewState);
	void ResetTourInternal();   // 카메라/페이드 건드리지 않고 변수만 초기화
	bool bTransitionBusy = false; // (선택) 페이드/전환 중 재진입 방지



private:
	UPROPERTY(Transient)
		TWeakObjectPtr<USplineComponent> TourSpline;

	UPROPERTY(Transient)
		APlayerController* CachedPC = nullptr;

	UPROPERTY(Transient)
		AActor* OriginalViewTarget = nullptr;

	UPROPERTY(Transient)
		ETourState State = ETourState::Idle;

	// Spline 진행 거리
	float DistanceAlongSpline = 0.f;

	// Lift / Fixed Z
	float LiftStartZ = 0.f;
	float CurrentLiftZ = 0.f;
	float FixedTourZ = 0.f;

	// PreMove Hold
	float PreMoveHoldRemain = 0.f;

	// Start Accel
	float MoveElapsedSeconds = 0.f;

	// End Wait
	float WaitRemain = 0.f;

private:
	AInGameMode* GM = nullptr;
};