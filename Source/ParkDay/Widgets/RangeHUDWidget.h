#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ParkDay/GolfBall.h"
#include "ParkDay/GolfDataStructures.h"
#include "RangeHUDWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnChangedDrivingCheckBoxState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnChangedApproachCheckBoxState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnChangedPuttingCheckBoxState);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAddShotStat, FShotStat, ShotStat);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBallStop, FShotStat, ShotStat);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnModeChange);

class AInGameMode;
class UButton;
class UTextBlock;
class UImage;
class UWrapBox;
class UCanvasPanel;
class UCheckBox;
class URangeHUDStatLineWidget;

constexpr float CM_TO_M = 0.01f;
constexpr float M_TO_CM = 100.f;

UCLASS()
class PARKDAY_API URangeHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    FOnChangedApproachCheckBoxState OnChangedApproachCheckBoxStateDele;
    FOnChangedDrivingCheckBoxState OnChangedDrivingCheckBoxStateDele;
    FOnChangedPuttingCheckBoxState OnChangedPuttingCheckBoxStateDele;
    FOnAddShotStat OnAddShotStatDele;
    FOnBallStop OnBallStopDele;
    FOnModeChange OnModeChangeDele;

    // ---------- Lifecycle ----------
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    void ValidateAndReBindWidgets();

    virtual int32 NativePaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled
    ) const override;

    // ---------- Bind Widgets ----------
    UPROPERTY(meta=(BindWidget), BlueprintReadWrite) UButton* Button_ResetShotInfo = nullptr;
    UPROPERTY(meta=(BindWidget), BlueprintReadWrite) UButton* Button_SwingMotion   = nullptr;
    UPROPERTY(meta=(BindWidget), BlueprintReadWrite) UButton* Button_CameraMode    = nullptr;
    UPROPERTY(meta=(BindWidget), BlueprintReadWrite) UButton* Button_Menu          = nullptr;

    // ✅ 오직 이 패널만 기준으로 사용
    UPROPERTY(meta=(BindWidget), BlueprintReadWrite) UCanvasPanel* CanvasPanel_Left = nullptr;

    UPROPERTY(meta=(BindWidget), BlueprintReadWrite) UImage* Image_CameraMode = nullptr;
    UPROPERTY(meta=(BindWidget), BlueprintReadWrite) UWrapBox* WrapBox_Menu = nullptr;
    UPROPERTY(meta=(BindWidget), BlueprintReadWrite) UTextBlock* TextBlock_ShotInfo_Distance = nullptr;
    UPROPERTY(meta=(BindWidget), BlueprintReadWrite) UTextBlock* TextBlock_ShotInfo_BallSpeed = nullptr;
    UPROPERTY(meta=(BindWidget), BlueprintReadWrite) UTextBlock* TextBlock_ShotInfo_LeftRightAngle = nullptr;
    UPROPERTY(meta=(BindWidget), BlueprintReadWrite) UTextBlock* TextBlock_ShotInfo_EscapeAngle = nullptr;

    UPROPERTY(meta=(BindWidget), BlueprintReadWrite) UTextBlock* TextBlock_Range = nullptr;
    UPROPERTY(meta=(BindWidget), BlueprintReadWrite) UCanvasPanel* CanvasPanel_Modes = nullptr;
    UPROPERTY(meta=(BindWidget), BlueprintReadWrite) UCheckBox* CheckBox_DrivingMode = nullptr;
    UPROPERTY(meta=(BindWidget), BlueprintReadWrite) UCheckBox* CheckBox_ApproachMode = nullptr;
    UPROPERTY(meta=(BindWidget), BlueprintReadWrite) UCheckBox* CheckBox_PuttingMode = nullptr;
    UPROPERTY(meta=(BindWidget), BlueprintReadWrite) UCheckBox* CheckBox_Mode = nullptr;
    UPROPERTY(meta=(BindWidget), BlueprintReadWrite) UButton* Button_Stat = nullptr;
    UPROPERTY(meta=(BindWidget), BlueprintReadWrite) UButton* Button_LeftArrow = nullptr;
    UPROPERTY(meta=(BindWidget), BlueprintReadWrite) UButton* Button_RightArrow = nullptr;
    UPROPERTY(meta=(BindWidget), BlueprintReadWrite) UCanvasPanel* CanvasPanel_DIstance = nullptr;

    // 타겟 마커
    UPROPERTY(meta=(BindWidget), BlueprintReadWrite) UImage* Image_Target = nullptr;

    UPROPERTY(meta = (BindWidget))
    class UTimerWidget* WBP_Timer;

    UPROPERTY()
    URangeHUDStatLineWidget* AverageLine = nullptr;

    void SetTextForTextBlock(UTextBlock& TextBlock, FString Text);

    UFUNCTION() void UpdateAverageLine();

    UFUNCTION() void HandleOnChangedDrivingModeCheckBoxState(bool bIsChecked);
    UFUNCTION() void HandleOnChangedApproachModeCheckBoxState(bool bIsChecked);
    UFUNCTION() void HandleOnChangedPuttingModeCheckBoxState(bool bIsChecked);
    UFUNCTION() void HandleOnPressedLeftArrowButton();
    UFUNCTION() void HandleOnPressedRightArrowButton();
    UFUNCTION() void HandleOnPressedStatButton();

    void ChangePracticeModeState(EPracticeMode ChangeMode);
    void SetVisibilityModes(bool bIsVisible);

    UPROPERTY()
    FShotStat CurrentShotStat;

    UPROPERTY()
    int32 ShotCount = 0;

    UPROPERTY()
    float ApproachModeDistance = 10000.f;

    UPROPERTY()
    float PuttingModeDistance = 3000.f;

    UFUNCTION() void HandleOnChangedCheckBoxMode(bool bIsChecked);

    // ---------- 외부 데이터 ----------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style") TArray<FLinearColor> StrokeColors;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style", meta=(ClampMin="0.5")) float LineThickness = 2.0f;

    // 세로축(진행방향) 길이: 0m~FieldLengthMeters → 패널 Y(아래→위)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Axis", meta=(ClampMin="1.0"))
    float FieldLengthMeters = 120.f;

    // 좌우 총 길이(m) → 패널 X 좌/우 매핑(중앙 기준)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Axis|Lateral")
    float LateralTotalMeters = 60.f;

    // 픽셀 반폭(0이면 패널 절반 자동 사용)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Axis|Lateral")
    float LateralHalfWidthPixels = 0.f;

    // 좌우 반전 필요 시
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Axis|Lateral")
    bool bInvertLateralX = false;

    // 최근/과거 라인 색상
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style") FLinearColor RecentStrokeColor = FLinearColor::Red;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style") FLinearColor PastStrokeColor   = FLinearColor::White;

    // 히스토리 보관 개수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="History", meta=(ClampMin="1"))
    int32 MaxShotHistory = 10;

    // 샘플링 간격
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sampling", meta=(ClampMin="0.05"))
    float CheckInterval = 0.5f;

    // 디버그
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug") bool  bDebugLogPaint = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug", meta=(ClampMin="0.05")) float DebugLogIntervalSec = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug") bool  bDebugDrawGizmos = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debug", meta=(ClampMin="1.0")) float DebugMarkerPx = 8.f;

    // 버튼/카메라
    UPROPERTY(BlueprintReadWrite, EditAnywhere) UTexture2D* CameraModeTexture_Move = nullptr;
    UPROPERTY(BlueprintReadWrite, EditAnywhere) UTexture2D* CameraModeTexture_Fix  = nullptr;
    UPROPERTY() bool bFlipCameraMode = true;

    // ---------- UI 이벤트 ----------
    UFUNCTION() void Init();
    UFUNCTION() void OnMenuButtonClicked();
    UFUNCTION() void HandlePlayerState(int32 PlayerIndex, EPlayerState PlayerState);
    UFUNCTION() void HandleBallEvent(EBallEvent BallEvent);
    UFUNCTION() void HandleResetButtonClicked();
    UFUNCTION() void HandleCameraModeButtonClicked();
    UFUNCTION() void HandleSwingMotionButtonClicked();
    UFUNCTION() void SetEnableButtons(bool bIsEnable);
    // Approach 모드 타겟 UI(Image_Target) 표시/위치 갱신
    void RefreshApproachTargetWidget();
    UPROPERTY()
    FVector PuttingModeStartPoint;

    void UpdateShotBallSpeedAndAngle();
    void UpdateShotDistance();
    // Approach 모드 타겟 UI 마커
    void UpdateApproachTargetMarker();
private:
    AInGameMode* GM = nullptr;

    // 샷 기록
    TArray<FShotPath> ShotPaths;
    TArray<FShotStat> ShotStats;
    FTimerHandle      CheckTimerHandle;

    void AddShotStat();

    // --- 좌표계: 오직 CanvasPanel_Left ---
    bool GetCanvasLeftGeometry(struct FGeometry& OutGeo) const;

    // (샷 히스토리와 무관) 시작점/목표점 기반 월드 → 패널 로컬 변환
    FVector2D WorldToCanvasLocalBasis(const FVector& StartWorld, const FVector& EndWorld, const FVector& WorldLoc, const FGeometry& CanvasGeo) const;

    // ✅ 현재 PracticeMode에 따라 라인 기준축(Start/End)을 결정
    bool GetLineBasisForMode(int32 ShotIdx, FVector& OutStartWorld, FVector& OutEndWorld) const;



    // 샘플링/그리기
    void StartShotCapture();
    void StopShotCapture();
    void SampleBallPosition();
    int32 DrawShotLines(const FGeometry& RootGeo, FSlateWindowElementList& OutDrawElements, int32 BaseLayerId) const;
    

    void SetTargetMarkerVisible(bool bVisible);

    // 유틸
    FVector GetCurrentBallLocation() const;
    FVector GetCurrentCupLocation() const;
    void RequestRedraw();
    void TrimShotHistory();

    // 디버그
    static FString DumpGeometry(const FGeometry& G);
    static FString DumpVec(const FVector& V);
    static FString DumpPt(const FVector2D& P);
    void DrawCross(FSlateWindowElementList& OutDrawElements, const FGeometry& TargetGeo,
                   const FVector2D& P, const FLinearColor& C, float SizePx, int32& LayerId) const;

    static double GLastPaintLogTimeSec;

    // --- 샷별 Basis(투영 축) 기록: ShotPaths와 인덱스 동기화 ---
    struct FShotBasis
    {
        FVector Start = FVector::ZeroVector;
        FVector End = FVector::ZeroVector;
    };

    TArray<FShotBasis> ShotBases;

    // 현재 PracticeMode에 따른 Basis(Start/End) 계산 (샷 기록 시점에 고정하기 위함)
    bool GetLineBasisForCurrentMode(FVector& OutStartWorld, FVector& OutEndWorld) const;
};
