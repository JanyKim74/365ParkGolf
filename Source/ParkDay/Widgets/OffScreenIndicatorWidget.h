#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OffscreenIndicatorWidget.generated.h"

class UImage;
class APlayerController;

UCLASS()
class PARKDAY_API UOffscreenIndicatorWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    // 매 프레임(또는 일정 주기) 호출
    void UpdateForTarget(APlayerController* PC, const FVector& TargetWorldLoc);
    UFUNCTION(BlueprintCallable, Category="Indicator")
		void SetAllowedViewTarget(AActor* InViewTarget);

	UFUNCTION(BlueprintCallable, Category = "Indicator")
		void ClearAllowedViewTarget();
public:
    // 화면 가장자리 마진(px)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Indicator")
    float EdgePadding = 70.f;

    // 화살표 이미지 기본 방향 보정
    // - 이미지가 기본적으로 ↑(위)를 보고 있으면 90
    // - 기본적으로 →(오른쪽)이면 0
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Indicator")
    float ImageBasisCorrectionDeg = 90.f;

    // 위치 스무딩(클수록 빠르게 따라감)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Indicator|Smoothing")
    float PositionSmoothSpeed = 12.f; // 8~18 추천

    // 방향 벡터 스무딩(뒤에서 민감함 완화)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Indicator|Smoothing")
    float DirSmoothSpeed = 10.f; // 5~12 추천

    // 각도 스무딩(클수록 빠르게 따라감)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Indicator|Smoothing")
    float AngleSmoothSpeed = 10.f; // 6~14 추천

    // 뒤/앞 판정 히스테리시스
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Indicator|Behind")
    float EnterBehindDot = -0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Indicator|Behind")
    float ExitBehindDot = 0.15f;

protected:
    UPROPERTY(meta=(BindWidget))
    UImage* Image_Arrow = nullptr;

    // OffscreenIndicatorWidget.h
private:
    TWeakObjectPtr<AActor> AllowedViewTarget;  // ✅ 최초 카메라(고정)
    bool bAllowedViewTargetInitialized = false;

private:
    bool CalcEdgeClampedScreenPoint(
        const FVector2D& ScreenPos,
        const FVector2D& ViewSize,
        float InEdgePadding,
        FVector2D& OutClamped,
        float& OutAngleDeg) const;

private:
    // Behind hysteresis state
    bool bWasBehind = false;

    // Smoothing state
    bool bHasSmoothedPos = false;
    FVector2D SmoothedPos = FVector2D::ZeroVector;

    bool bHasSmoothedDir = false;
    FVector2D SmoothedDir2D = FVector2D(1.f, 0.f);

    bool bHasSmoothedAngle = false;
    float SmoothedAngle = 0.f;
};
