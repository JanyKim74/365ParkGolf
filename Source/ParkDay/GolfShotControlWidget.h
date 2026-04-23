#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/CanvasPanel.h"
#include "GolfShotControlWidget.generated.h"

class AGolfPlayerController;

/**
 * 골프 샷 조절 UI 위젯 - Yaw 컨트롤 추가
 * 상승각도(Pitch), 좌우 방향(Yaw), 파워를 조절하는 기능
 */
UCLASS()
class PARKDAY_API UGolfShotControlWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UGolfShotControlWidget(const FObjectInitializer& ObjectInitializer);

    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // 샷 파라미터 설정
    UFUNCTION(BlueprintCallable, Category = "Shot Control")
        void SetShotPower(float Power);

    UFUNCTION(BlueprintCallable, Category = "Shot Control")
        void SetShotAngle(float Angle);

    UFUNCTION(BlueprintCallable, Category = "Shot Control")
        void SetShotYaw(float Yaw);  // 🔥 새로 추가

    UFUNCTION(BlueprintCallable, Category = "Shot Control")
        void UpdateShotPreview();

    // UI 표시/숨김
    UFUNCTION(BlueprintCallable, Category = "Shot Control")
        void ShowShotControl(bool bShow);

    // 샷 실행
    UFUNCTION(BlueprintCallable, Category = "Shot Control")
        void ExecuteShot();

    // 샷 설정값들
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Settings")
        float MinShotPower;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Settings")
        float MaxShotPower;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Settings")
        float MinShotAngle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Settings")
        float MaxShotAngle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Settings")  // 🔥 새로 추가
        float MinShotYaw;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Settings")  // 🔥 새로 추가
        float MaxShotYaw;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Settings")
        float PowerStep;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Settings")
        float AngleStep;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Settings")  // 🔥 새로 추가
        float YawStep;

protected:
    // UI 컴포넌트들 - 메인 패널
    UPROPERTY(meta = (BindWidget))
        class UCanvasPanel* ShotControlPanel;

    // UI 컴포넌트들 - 파워 조절
    UPROPERTY(meta = (BindWidget))
        class UProgressBar* PowerBar;

    UPROPERTY(meta = (BindWidget))
        class USlider* PowerSlider;

    UPROPERTY(meta = (BindWidget))
        class UTextBlock* PowerValueText;

    UPROPERTY(meta = (BindWidget))
        class UTextBlock* PowerLabelText;

    // UI 컴포넌트들 - 각도 조절 (Pitch)
    UPROPERTY(meta = (BindWidget))
        class UProgressBar* AngleBar;

    UPROPERTY(meta = (BindWidget))
        class USlider* AngleSlider;

    UPROPERTY(meta = (BindWidget))
        class UTextBlock* AngleValueText;

    UPROPERTY(meta = (BindWidget))
        class UTextBlock* AngleLabelText;

    // UI 컴포넌트들 - 방향 조절 (Yaw) 🔥 새로 추가
    UPROPERTY(meta = (BindWidget))
        class UProgressBar* YawBar;

    UPROPERTY(meta = (BindWidget))
        class USlider* YawSlider;

    UPROPERTY(meta = (BindWidget))
        class UTextBlock* YawValueText;

    UPROPERTY(meta = (BindWidget))
        class UTextBlock* YawLabelText;

    // UI 컴포넌트들 - 버튼
    UPROPERTY(meta = (BindWidget))
        class UButton* ShotButton;

    UPROPERTY(meta = (BindWidget))
        class UButton* CancelButton;

    // 현재 샷 파라미터
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shot State")
        float CurrentShotPower;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shot State")
        float CurrentShotAngle;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shot State")  // 🔥 새로 추가
        float CurrentShotYaw;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shot State")
        float PredictedDistance;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shot State")
        float PredictedFlightTime;

private:
    // 이벤트 핸들러
    UFUNCTION()
        void OnPowerSliderChanged(float Value);

    UFUNCTION()
        void OnAngleSliderChanged(float Value);

    UFUNCTION()  // 🔥 새로 추가
        void OnYawSliderChanged(float Value);

    UFUNCTION()
        void OnShotButtonClicked();

    UFUNCTION()
        void OnCancelButtonClicked();

    // 미리보기 계산
    void CalculateShotPreview();

    // UI 업데이트
    void UpdatePowerDisplay();
    void UpdateAngleDisplay();
    void UpdateYawDisplay();  // 🔥 새로 추가
    void UpdatePreviewDisplay();
    void UpdateClubSuggestion();

    // 물리 계산
    float CalculateMaxDistance(float Power, float Angle) const;
    float CalculateFlightTime(float Power, float Angle) const;

    // 클럽 추천 시스템
    FString GetRecommendedClub(float Power, float Angle) const;
    FLinearColor GetClubColor(const FString& ClubName) const;

    // PlayerController 참조
    UPROPERTY()
        AGolfPlayerController* PlayerController;

    // 애니메이션 상태
    bool bIsVisible;
    float VisibilityAnimationTime;
};