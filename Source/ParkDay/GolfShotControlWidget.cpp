#include "GolfShotControlWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/CanvasPanel.h"
#include "GolfPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

UGolfShotControlWidget::UGolfShotControlWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // 기본값 설정
    MinShotPower = 1.0f;
    MaxShotPower = 50.0f;
    MinShotAngle = 0.0f;
    MaxShotAngle = 45.0f;
    MinShotYaw = -45.0f;  // 🔥 새로 추가: 좌로 45도
    MaxShotYaw = 45.0f;   // 🔥 새로 추가: 우로 45도
    PowerStep = 1.0f;
    AngleStep = 1.0f;
    YawStep = 1.0f;       // 🔥 새로 추가

    CurrentShotPower = 25.0f;
    CurrentShotAngle = 3.0f;
    CurrentShotYaw = 0.0f;    // 🔥 새로 추가: 기본값 직진
    PredictedDistance = 0.0f;
    PredictedFlightTime = 0.0f;

    bIsVisible = false;
    VisibilityAnimationTime = 0.0f;

    // 포인터 초기화
    PlayerController = nullptr;
    ShotControlPanel = nullptr;
    PowerBar = nullptr;
    PowerSlider = nullptr;
    PowerValueText = nullptr;
    PowerLabelText = nullptr;
    AngleBar = nullptr;
    AngleSlider = nullptr;
    AngleValueText = nullptr;
    AngleLabelText = nullptr;

    // 🔥 새로 추가: Yaw 관련 포인터 초기화
    YawBar = nullptr;
    YawSlider = nullptr;
    YawValueText = nullptr;
    YawLabelText = nullptr;

    ShotButton = nullptr;
    CancelButton = nullptr;
}

void UGolfShotControlWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // PlayerController 참조 획득
    PlayerController = Cast<AGolfPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));

    // 슬라이더 이벤트 바인딩
    if (PowerSlider)
    {
        PowerSlider->OnValueChanged.AddDynamic(this, &UGolfShotControlWidget::OnPowerSliderChanged);
        PowerSlider->SetMinValue(MinShotPower);
        PowerSlider->SetMaxValue(MaxShotPower);
        PowerSlider->SetStepSize(PowerStep);
        PowerSlider->SetValue(CurrentShotPower);
    }

    if (AngleSlider)
    {
        AngleSlider->OnValueChanged.AddDynamic(this, &UGolfShotControlWidget::OnAngleSliderChanged);
        AngleSlider->SetMinValue(MinShotAngle);
        AngleSlider->SetMaxValue(MaxShotAngle);
        AngleSlider->SetStepSize(AngleStep);
        AngleSlider->SetValue(CurrentShotAngle);
    }

    // 🔥 새로 추가: Yaw 슬라이더 이벤트 바인딩
    if (YawSlider)
    {
        YawSlider->OnValueChanged.AddDynamic(this, &UGolfShotControlWidget::OnYawSliderChanged);
        YawSlider->SetMinValue(MinShotYaw);
        YawSlider->SetMaxValue(MaxShotYaw);
        YawSlider->SetStepSize(YawStep);
        YawSlider->SetValue(CurrentShotYaw);
    }

    // 버튼 이벤트 바인딩
    if (ShotButton)
    {
        ShotButton->OnClicked.AddDynamic(this, &UGolfShotControlWidget::OnShotButtonClicked);
    }

    if (CancelButton)
    {
        CancelButton->OnClicked.AddDynamic(this, &UGolfShotControlWidget::OnCancelButtonClicked);
    }

    // 라벨 텍스트 설정
    if (PowerLabelText)
    {
        PowerLabelText->SetText(FText::FromString(TEXT("Shot Power")));
    }

    if (AngleLabelText)
    {
        AngleLabelText->SetText(FText::FromString(TEXT("Launch Angle")));
    }

    // 🔥 새로 추가: Yaw 라벨 텍스트 설정
    if (YawLabelText)
    {
        YawLabelText->SetText(FText::FromString(TEXT("Direction")));
    }

    // 초기 UI 업데이트
    UpdatePowerDisplay();
    UpdateAngleDisplay();
    UpdateYawDisplay();  // 🔥 새로 추가
   // UpdateShotPreview();

    // 초기에는 숨김
    ShowShotControl(false);
}

void UGolfShotControlWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    // 애니메이션 업데이트
    if (bIsVisible)
    {
        VisibilityAnimationTime += InDeltaTime;
        // 실시간 업데이트는 슬라이더 변경 시에만 수행
    }
}

void UGolfShotControlWidget::SetShotPower(float Power)
{
    CurrentShotPower = FMath::Clamp(Power, MinShotPower, MaxShotPower);

    if (PowerSlider)
    {
        PowerSlider->SetValue(CurrentShotPower);
    }

    UpdatePowerDisplay();
    // UpdateShotPreview();
}

void UGolfShotControlWidget::SetShotAngle(float Angle)
{
    CurrentShotAngle = FMath::Clamp(Angle, MinShotAngle, MaxShotAngle);

    if (AngleSlider)
    {
        AngleSlider->SetValue(CurrentShotAngle);
    }

    UpdateAngleDisplay();
    // UpdateShotPreview();
}

// 🔥 새로 추가: Yaw 설정 함수
void UGolfShotControlWidget::SetShotYaw(float Yaw)
{
    CurrentShotYaw = FMath::Clamp(Yaw, MinShotYaw, MaxShotYaw);

    if (YawSlider)
    {
        YawSlider->SetValue(CurrentShotYaw);
    }

    UpdateYawDisplay();
    // UpdateShotPreview();
}

void UGolfShotControlWidget::UpdateShotPreview()
{
    CalculateShotPreview();
    UpdatePreviewDisplay();
    UpdateClubSuggestion();  // 🔥 클럽 추천 업데이트
}

void UGolfShotControlWidget::ShowShotControl(bool bShow)
{
    bIsVisible = bShow;
    VisibilityAnimationTime = 0.0f;

    if (ShotControlPanel)
    {
        // 🔥 변수명 충돌 해결
        ShotControlPanel->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
    }

    // PlayerController에 현재 값 전달
    if (bShow && PlayerController)
    {
        PlayerController->ShotPower = CurrentShotPower;
        PlayerController->ShotPitchAngle = CurrentShotAngle;
        PlayerController->ShotYawAngle = CurrentShotYaw;  // 🔥 수정: Yaw 값 전달
    }
}

void UGolfShotControlWidget::ExecuteShot()
{
    if (PlayerController)
    {
        // 설정된 값을 PlayerController에 적용
        PlayerController->ShotPower = CurrentShotPower;
        PlayerController->ShotPitchAngle = CurrentShotAngle;
        PlayerController->ShotYawAngle = CurrentShotYaw;  // 🔥 수정: Yaw 값 전달

        // 샷 실행
        PlayerController->ExecuteShot();

        // UI 숨김
        ShowShotControl(false);
    }
}

void UGolfShotControlWidget::OnPowerSliderChanged(float Value)
{
    SetShotPower(Value);

    // PlayerController에 실시간 업데이트
    if (PlayerController)
    {
        PlayerController->ShotPower = CurrentShotPower;
    }
}

void UGolfShotControlWidget::OnAngleSliderChanged(float Value)
{
    SetShotAngle(Value);

    // PlayerController에 실시간 업데이트
    if (PlayerController)
    {
        PlayerController->ShotPitchAngle = CurrentShotAngle;
    }
}

// 🔥 새로 추가: Yaw 슬라이더 변경 이벤트 핸들러
void UGolfShotControlWidget::OnYawSliderChanged(float Value)
{
    SetShotYaw(Value);

    // PlayerController에 실시간 업데이트
    if (PlayerController)
    {
        PlayerController->ShotYawAngle = CurrentShotYaw;
    }
}

void UGolfShotControlWidget::OnShotButtonClicked()
{
    ExecuteShot();
}

void UGolfShotControlWidget::OnCancelButtonClicked()
{
    ShowShotControl(false);
}

void UGolfShotControlWidget::CalculateShotPreview()
{
    // 포물선 운동 계산 (Yaw는 방향만 결정하므로 거리 계산에는 영향 없음)
    PredictedDistance = CalculateMaxDistance(CurrentShotPower, CurrentShotAngle);
    PredictedFlightTime = CalculateFlightTime(CurrentShotPower, CurrentShotAngle);
}

void UGolfShotControlWidget::UpdatePowerDisplay()
{
    // 파워 바 업데이트
    if (PowerBar)
    {
        float PowerPercent = (CurrentShotPower - MinShotPower) / (MaxShotPower - MinShotPower);
        PowerBar->SetPercent(PowerPercent);

        // 파워에 따른 색상 변경
        FLinearColor BarColor;
        if (PowerPercent < 0.3f)
        {
            BarColor = FLinearColor::Green;  // 약한 파워
        }
        else if (PowerPercent < 0.7f)
        {
            BarColor = FLinearColor::Yellow; // 중간 파워
        }
        else
        {
            BarColor = FLinearColor::Red;    // 강한 파워
        }
        PowerBar->SetFillColorAndOpacity(BarColor);
    }

    // 파워 텍스트 업데이트
    if (PowerValueText)
    {
        FString PowerText = FString::Printf(TEXT("%.1f m/s"), CurrentShotPower);
        PowerValueText->SetText(FText::FromString(PowerText));
    }
}

void UGolfShotControlWidget::UpdateAngleDisplay()
{
    // 각도 바 업데이트
    if (AngleBar)
    {
        float AnglePercent = (CurrentShotAngle - MinShotAngle) / (MaxShotAngle - MinShotAngle);
        AngleBar->SetPercent(AnglePercent);

        // 각도에 따른 색상 변경
        FLinearColor BarColor;
        if (CurrentShotAngle < 15.0f)
        {
            BarColor = FLinearColor::Blue;   // 낮은 탄도
        }
        else if (CurrentShotAngle < 30.0f)
        {
            BarColor = FLinearColor::Green;  // 중간 탄도
        }
        else
        {
            BarColor = FLinearColor::Yellow; // 높은 탄도
        }
        AngleBar->SetFillColorAndOpacity(BarColor);
    }

    // 각도 텍스트 업데이트
    if (AngleValueText)
    {
        FString AngleText = FString::Printf(TEXT("%.1f°"), CurrentShotAngle);
        AngleValueText->SetText(FText::FromString(AngleText));
    }
}

// 🔥 새로 추가: Yaw 디스플레이 업데이트 함수
void UGolfShotControlWidget::UpdateYawDisplay()
{
    // Yaw 바 업데이트
    if (YawBar)
    {
        // Yaw는 -45 ~ +45 범위이므로, 0을 중심으로 정규화
        float YawPercent = (CurrentShotYaw - MinShotYaw) / (MaxShotYaw - MinShotYaw);
        YawBar->SetPercent(YawPercent);

        // Yaw에 따른 색상 변경
        FLinearColor BarColor;
        float AbsYaw = FMath::Abs(CurrentShotYaw);

        if (AbsYaw < 10.0f)
        {
            BarColor = FLinearColor::Green;   // 직진에 가까움
        }
        else if (AbsYaw < 25.0f)
        {
            BarColor = FLinearColor::Yellow;  // 중간 정도 방향
        }
        else
        {
            BarColor = FLinearColor::Red;     // 큰 방향 변경
        }
        YawBar->SetFillColorAndOpacity(BarColor);
    }

    // Yaw 텍스트 업데이트
    if (YawValueText)
    {
        FString YawText;
        if (CurrentShotYaw > 0)
        {
            YawText = FString::Printf(TEXT("%.1f° R"), CurrentShotYaw);  // Right
        }
        else if (CurrentShotYaw < 0)
        {
            YawText = FString::Printf(TEXT("%.1f° L"), FMath::Abs(CurrentShotYaw));  // Left
        }
        else
        {
            YawText = FString::Printf(TEXT("0.0° Straight"));  // Straight
        }
        YawValueText->SetText(FText::FromString(YawText));
    }
}

void UGolfShotControlWidget::UpdatePreviewDisplay()
{

}

void UGolfShotControlWidget::UpdateClubSuggestion()
{

}

FString UGolfShotControlWidget::GetRecommendedClub(float Power, float Angle) const
{
    float DistanceYards = (CalculateMaxDistance(Power, Angle) / 91.44f);

    // 거리와 각도에 따른 클럽 추천
    if (DistanceYards > 220.0f)
    {
        return TEXT("Driver");
    }
    else if (DistanceYards > 180.0f)
    {
        return TEXT("3 Wood");
    }
    else if (DistanceYards > 160.0f)
    {
        return TEXT("5 Iron");
    }
    else if (DistanceYards > 140.0f)
    {
        return TEXT("7 Iron");
    }
    else if (DistanceYards > 120.0f)
    {
        return TEXT("9 Iron");
    }
    else if (Angle > 35.0f)
    {
        return TEXT("Sand Wedge");
    }
    else
    {
        return TEXT("Pitching Wedge");
    }
}

FLinearColor UGolfShotControlWidget::GetClubColor(const FString& ClubName) const
{
    if (ClubName.Contains(TEXT("Driver")))
    {
        return FLinearColor::Red;        // 드라이버 - 빨강
    }
    else if (ClubName.Contains(TEXT("Wood")))
    {
        return FLinearColor(1.0f, 0.5f, 0.0f, 1.0f);  // 우드 - 주황
    }
    else if (ClubName.Contains(TEXT("Iron")))
    {
        return FLinearColor::Blue;       // 아이언 - 파랑
    }
    else if (ClubName.Contains(TEXT("Wedge")))
    {
        return FLinearColor::Green;      // 웨지 - 초록
    }
    else
    {
        return FLinearColor::White;      // 기본 - 흰색
    }
}

float UGolfShotControlWidget::CalculateMaxDistance(float Power, float Angle) const
{
    // 포물선 운동 공식: R = (v^2 * sin(2θ)) / g
    const float Gravity = 980.0f; // cm/s^2
    float AngleRadians = FMath::DegreesToRadians(Angle);
    float PowerCmPerSec = Power * 100.0f; // m/s를 cm/s로 변환

    float Distance = (PowerCmPerSec * PowerCmPerSec * FMath::Sin(2.0f * AngleRadians)) / Gravity;

    return Distance; // cm 단위
}

float UGolfShotControlWidget::CalculateFlightTime(float Power, float Angle) const
{
    // 비행 시간 공식: t = (2 * v * sin(θ)) / g
    const float Gravity = 9.8f; // m/s^2
    float AngleRadians = FMath::DegreesToRadians(Angle);

    float FlightTime = (2.0f * Power * FMath::Sin(AngleRadians)) / Gravity;

    return FlightTime; // 초 단위
}