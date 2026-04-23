#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "BallNamePlateComponent.generated.h"

class UWidgetComponent;
class UBallNamePlateWidget;

UCLASS(ClassGroup=(UI), meta=(BlueprintSpawnableComponent))
class PARKDAY_API UBallNamePlateComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UBallNamePlateComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

public:
	UFUNCTION(BlueprintCallable, Category="BallNamePlate")
	UBallNamePlateWidget* GetNamePlateWidget() const;

	UFUNCTION(BlueprintCallable, Category="BallNamePlate")
	void SetWidgetClass(TSubclassOf<UUserWidget> InNamePlateWidgetClass);

	UFUNCTION(BlueprintCallable, Category="BallNamePlate")
	void SetPlayerNameText(const FText& InName);

	UFUNCTION(BlueprintCallable, Category="BallNamePlate")
	void SetPlayerNameString(const FString& InName);

	UFUNCTION(BlueprintCallable, Category="BallNamePlate")
	void SetNamePlateVisible(bool bIsVisible);

protected:
	void EnsureWidgetCreated();
	void ApplyPendingNameIfAny();

	bool ScreenToWorldAtBallDepth(
		class APlayerController* PC,
		const FVector2D& ScreenPos,
		const FVector& BallWorldPos,
		FVector& OutWorldPos) const;

	void UpdateWorldPositionByScreenOffset();
	void UpdateAutoVisibility();

	/** ✅ 거리 기반 스케일을 “위젯(CanvasPanel_NamePlate) RenderScale”로 적용 */
	void UpdateDistanceScale();

	/** ✅ 스케일 계산만 수행 (오프셋에도 같은 비율 적용하기 위해 분리) */
	float ComputeDistanceScale() const;

	void ApplyFinalVisibility();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BallNamePlate")
	TSubclassOf<UUserWidget> NamePlateWidgetClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BallNamePlate")
	UWidgetComponent* WidgetComponent = nullptr;

	/** 화면상 공 기준 오프셋(픽셀). 이 값도 거리 스케일 비율로 같이 줄어듦 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BallNamePlate|ScreenOffset")
	FVector2D ScreenOffsetPx = FVector2D(0.f, -60.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BallNamePlate|Culling")
	float ScreenMarginPx = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BallNamePlate|Common")
	int32 CameraPlayerIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BallNamePlate|Visibility")
	bool bStartVisible = true;

	// ---------------------------------------------------------------------
	// Distance Scale
	// ---------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BallNamePlate|DistanceScale")
	bool bScaleByDistance = true;

	/** 이 거리(cm)까지는 Scale=1.0 고정 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BallNamePlate|DistanceScale", meta=(EditCondition="bScaleByDistance"))
	float HoldDistance = 600.f;

	/** 이 거리(cm)에 도달하면 MinScale (이후 고정) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BallNamePlate|DistanceScale", meta=(EditCondition="bScaleByDistance"))
	float MinScaleDistance = 3000.f;

	/** 최대로 줄어드는 비율 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BallNamePlate|DistanceScale", meta=(EditCondition="bScaleByDistance", ClampMin="0.1", ClampMax="1.0"))
	float MinScale = 0.4f;

protected:
	UPROPERTY(Transient)
	FText PendingNameText;

	bool bWidgetReady = false;
	bool bManualVisible = true;
	bool bAutoVisible = true;

	/** ✅ 현재 적용 스케일(위젯 + 오프셋 공통) */
	float CurrentScale = 1.0f;
};
