#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Canvas.h"
#include "GolfMiniMap.generated.h"

class AGolfBall;
class AInGameMode;
class UGolfPlayerManager; // ⭐ 추가: UGolfPlayerManager 전방 선언

/**
 * 골프 미니맵 위젯 - UE4 버전
 * 홀컵 위치와 티박스 위치를 기준으로 200x400 크기의 미니맵 표시
 * 에임 정보, 플래그, 거리, 높이 정보 포함
 * 실제 맵을 캡처해서 배경 이미지로 사용
 */
UCLASS()
class PARKDAY_API UGolfMiniMap : public UUserWidget
{
    GENERATED_BODY()

public:
    UGolfMiniMap(const FObjectInitializer& ObjectInitializer);

    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;



    // 미니맵 설정
    UFUNCTION(BlueprintCallable, Category = "MiniMap")
    void InitializeMiniMap(const FVector& TeePosition, const FVector& HolecupPosition);

    // ⭐ 수정: UpdateBallPosition 이제 플레이어 인덱스를 받음
    UFUNCTION(BlueprintCallable, Category = "MiniMap")
    void UpdateBallPosition(int32 PlayerIndex, const FVector& BallPosition);

    // ⭐ 수정: UpdateAimDirection 이제 플레이어 인덱스를 받음
    UFUNCTION(BlueprintCallable, Category = "MiniMap")
    void UpdateAimDirection(int32 PlayerIndex, const FVector& AimDirection);

    UFUNCTION(BlueprintCallable, Category = "MiniMap")
    void SetCurrentHole(int32 HoleNumber);

    UFUNCTION(BlueprintCallable, Category = "MiniMap")
    void UpdateDistanceAndElevation(float DistanceToHole, float ElevationDifference);

    // 맵 캡처 기능 (기존과 동일)
    UFUNCTION(BlueprintCallable, Category = "MiniMap Capture")
    void CaptureMapBackground();

    UFUNCTION(BlueprintCallable, Category = "MiniMap Capture")
    void SetCaptureHeight(float Height);

    UFUNCTION(BlueprintCallable, Category = "MiniMap Capture")
    void RefreshMapBackground();

    // 미니맵 설정값들 (기존과 동일)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap Settings")
    float MapWidth;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap Settings")
    float MapHeight;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap Settings")
    float VerticalViewPercentage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap Settings")
    float MapScale;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap Settings")
    float AimLineLength;

    // === MiniMap flip options ===
    // 좌우/상하 뒤집기 (기본: 둘 다 활성화 → 180° 회전과 동일)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap Settings")
    bool bFlipMapHorizontally = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap Settings")
    bool bFlipMapVertically = true;

    // 런타임에 뒤집기 설정 변경
    UFUNCTION(BlueprintCallable, Category = "MiniMap Settings")
    void SetMiniMapFlip(bool bFlipX, bool bFlipY);

    // 맵 캡처 설정 (기존과 동일)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap Capture Settings")
    float CaptureHeight;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap Capture Settings")
    float CaptureOrthoWidth;

    // ⭐ Orthographic 캡처 반경에 곱해지는 여유 배율. 1.0 = 계산값 그대로(꽉 차게), 클수록 더 넓게 캡처되어(=더 축소되어) 보임.
    //    Perspective→Orthographic 전환 후 화면이 이전보다 확대되어 보이면 이 값을 1.0보다 크게 올리세요. (예: 1.3 = 약 30% 여유)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap Capture Settings")
    float CaptureViewMargin = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap Capture Settings")
    int32 RenderTargetResolution;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap Capture Settings")
    bool bAutoRefreshCapture;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap Capture Settings")
    float RefreshInterval;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap Capture Settings")
    class UMaterialInterface* CaptureMaterial;

    // 미니맵 표시/숨김 애니메이션 (기존과 동일)
    UFUNCTION(BlueprintCallable, Category = "MiniMap Animation")
    void ShowMiniMapWithAnimation(bool bShow, float AnimationDuration = 0.3f);

    // 볼 궤적 표시 (기존과 동일)
    UFUNCTION(BlueprintCallable, Category = "MiniMap Trajectory")
    void ShowBallTrajectory(const TArray<FVector>& tTrajectoryPoints);

    UFUNCTION(BlueprintCallable, Category = "MiniMap Trajectory")
    void ClearBallTrajectory();

    // 거리 정보 표시 (기존과 동일)
    UFUNCTION(BlueprintCallable, Category = "MiniMap Info")
    void UpdateDistanceInfo();

    // 미니맵 확대/축소 (기존과 동일)
    UFUNCTION(BlueprintCallable, Category = "MiniMap Zoom")
    void SetZoomLevel(float ZoomLevel);

    virtual void BeginDestroy() override;

    // 캡처 최적화 관련 변수들 (기존과 동일)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    float MinCaptureInterval = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    bool bEnableAdaptiveCapture = true;

    // OB 라인 관련 함수들 (기존과 동일)
    UFUNCTION(BlueprintCallable, Category = "MiniMap OB")
    void UpdateOBLines(const TArray<FVector>& OBPoints);

    UFUNCTION(BlueprintCallable, Category = "MiniMap OB")
    void ClearOBLines();

    UFUNCTION(BlueprintCallable, Category = "MiniMap OB")
    void SetOBLinesVisible(bool bVisible);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap OB")
    FLinearColor OBLineColor = FLinearColor::Red;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap OB")
    float OBLineThickness = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap OB")
    bool bShowOBLines = true;

    // 좌표 매칭 디버그 함수들 (기존과 동일)
    UFUNCTION(BlueprintCallable, Category = "Debug Coordinates")
    void DebugCoordinateMatching();

    UFUNCTION(BlueprintCallable, Category = "Debug Coordinates")
    void TestCoordinateMapping();

    UFUNCTION(BlueprintCallable, Category = "Debug Coordinates")
    void FixCoordinateAlignment();

    // 미니맵 스케일 조정 함수 추가 (기존과 동일)
    UFUNCTION(BlueprintCallable, Category = "MiniMap Scale")
    void AdjustMiniMapScale(float ScaleFactor);

    UFUNCTION(BlueprintCallable, Category = "MiniMap Scale")
    void SetMiniMapScale(float NewScale) { MapScale = FMath::Clamp(NewScale, 0.1f, 5.0f); AdjustMiniMapScale(1.0f); }

    UFUNCTION(BlueprintCallable, Category = "MiniMap Scale")
    float GetCurrentMiniMapScale() const { return MapScale; }


    // 편의 함수들 (기존과 동일)
    UFUNCTION(BlueprintCallable, Category = "MiniMap Control")
    void SetMiniMapZoomLevel(int32 ZoomLevel);

    UFUNCTION(BlueprintCallable, Category = "MiniMap Control")
    void SetCaptureQuality(int32 QualityLevel);

    UFUNCTION(BlueprintCallable, Category = "MiniMap Control")
    void AutoFitToHole();

    UFUNCTION(BlueprintCallable, Category = "MiniMap Control")
    void ResetMiniMapSettings();

    UFUNCTION(BlueprintCallable, Category = "MiniMap Control")
    void SetPerformanceMode(bool bHighPerformance);

    UFUNCTION(BlueprintCallable, Category = "MiniMap Debug")
    void ShowDebugInfo(bool bShow);

    // ⭐ 새로 추가: 새로운 플레이어 볼과 에임 라인 추가/제거 함수
    UFUNCTION(BlueprintCallable, Category = "MiniMap Players")
    void AddPlayerToMiniMap(int32 PlayerIndex, const FVector& InitialBallPosition, const FLinearColor& BallColor);

    UFUNCTION(BlueprintCallable, Category = "MiniMap Players")
    void RemovePlayerFromMiniMap(int32 PlayerIndex);


    // 미니맵 클릭 관련 함수들
    UFUNCTION(BlueprintCallable, Category = "MiniMap Click")
    FVector MapPositionToWorldPosition(const FVector2D& MapPosition) const;

    UFUNCTION(BlueprintCallable, Category = "MiniMap Click")
    bool IsPointInMiniMapBounds(const FVector2D& MapPosition) const;

    UFUNCTION(BlueprintCallable, Category = "MiniMap Click")
    void OnMiniMapClicked(const FVector2D& ClickPosition);

    // 편의 함수들
    UFUNCTION(BlueprintCallable, Category = "MiniMap Click")
    FVector GetWorldPositionFromMiniMapClick(float MapX, float MapY) const;

    UFUNCTION(BlueprintCallable, Category = "MiniMap Click")
    void SetMiniMapClickEnabled(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "MiniMap Click")
    void TestCoordinateConversion();

    // 클릭 이벤트 델리게이트
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMiniMapClicked, FVector, WorldPosition);

    UPROPERTY(BlueprintAssignable, Category = "MiniMap Events")
    FOnMiniMapClicked OnMiniMapClickedEvent;


    // 클릭 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap Click")
    bool bEnableMiniMapClick = true;

    UFUNCTION(BlueprintCallable, Category = "MiniMap Click")
    void HandleStrokeModeClick(const FVector& WorldPosition);

    UFUNCTION(BlueprintCallable, Category = "MiniMap Click")
    void HandleTrainingModeClick(const FVector& WorldPosition);

    // ⭐ Training Mode 전용 함수들
    UFUNCTION(BlueprintCallable, Category = "Training Mode")
    void MoveBallToPosition(const FVector& WorldPosition);

    UFUNCTION(BlueprintCallable, Category = "Training Mode")
    bool IsValidBallPlacement(const FVector& WorldPosition) const;

    UFUNCTION(BlueprintCallable, Category = "Training Mode")
    void ResetTrainingBall();

    // ⭐ Stroke Mode 전용 함수들
    UFUNCTION(BlueprintCallable, Category = "Stroke Mode")
    void SetAimDirection(const FVector& WorldPosition);

    UFUNCTION(BlueprintCallable, Category = "Stroke Mode")
    void UpdateCameraAim(const FVector& TargetDirection);

    // ⭐ OB 지역 체크 함수 추가
    UFUNCTION(BlueprintCallable, Category = "MiniMap OB")
    bool IsPointInOBArea(const FVector& WorldPoint) const;

    UFUNCTION(BlueprintCallable, Category = "MiniMap OB")
    bool IsClickPositionValid(const FVector& WorldPosition) const;

    UFUNCTION(BlueprintCallable, Category = "MiniMap OB")
    void ShowOBWarning(const FVector& ClickedPosition);
    // 명확성을 위한 인바운드 체크 함수 추가
    UFUNCTION(BlueprintCallable, Category = "MiniMap OB")
    bool IsPointInBounds(const FVector& WorldPoint) const;

public:
    // UI 컴포넌트들 - 기본
    UPROPERTY(meta = (BindWidget))
    class UCanvasPanel* MiniMapCanvas;

    UPROPERTY(meta = (BindWidget))
    class UImage* Image_Minimap;

    UPROPERTY(meta = (BindWidget))
    class UImage* BackgroundImage;

    UPROPERTY(meta = (BindWidget))
    class UImage* TeeImage;

    UPROPERTY(meta = (BindWidget))
    class UImage* HolecupImage;

    UPROPERTY(meta = (BindWidget))
    class UImage* AimLineImage;

    // ⭐ 수정: BallImage 및 AimLineImage 단일 변수 제거
    // UPROPERTY(meta = (BindWidget)) class UImage* BallImage;
    // UPROPERTY(meta = (BindWidget)) class UImage* AimLineImage;

    UPROPERTY(meta = (BindWidget))
    class UImage* FlagImage;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* HoleNumberText;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* DistanceText;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* ElevationText;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* WindInfoText;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* ParInfoText;

    UPROPERTY(meta = (BindWidget))
    class UCanvasPanel* OBLinesCanvas;


    // 맵 캡처 관련 컴포넌트들 (기존과 동일)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MiniMap Capture")
    class AActor* CaptureActor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MiniMap Capture")
    class USceneCaptureComponent2D* SceneCaptureComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MiniMap Capture")
    class UTextureRenderTarget2D* MapRenderTarget;

    // 미니맵 데이터
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MiniMap Data")
    FVector TeeWorldPosition;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MiniMap Data")
    FVector HolecupWorldPosition;

    // ⭐ 수정: BallWorldPosition 및 CurrentAimDirection 단일 변수 제거
    // FVector BallWorldPosition;
    // FVector CurrentAimDirection;

    // ⭐ 추가: 여러 플레이어 볼 위치 및 에임 방향, 색상을 위한 TMap
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MiniMap Data")
    TMap<int32, FVector> PlayerBallWorldPositions;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MiniMap Data")
    TMap<int32, FVector> PlayerAimDirections;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap Colors")
    TMap<int32, FLinearColor> PlayerBallColors; // 플레이어별 볼 색상


    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MiniMap Data")
    FVector MapCenterWorldPosition;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MiniMap Data")
    float WorldToMapScale;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MiniMap Data")
    int32 CurrentHoleNumber;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MiniMap Data")
    float CurrentDistance;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MiniMap Data")
    float CurrentElevation;

    // 색상 설정 (기존과 동일)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap Colors")
    FLinearColor TeeColor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap Colors")
    FLinearColor HolecupColor;

    // ⭐ 수정: BallColor 단일 변수 제거
    // UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap Colors") FLinearColor BallColor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap Colors")
    FLinearColor AimLineColor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap Colors")
    FLinearColor FlagColor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap Colors")
    FLinearColor BackgroundColor;

    // 궤적 표시용 이미지들 (기존과 동일)
    UPROPERTY(meta = (BindWidget))
    class UCanvasPanel* TrajectoryCanvas;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* DistanceToHoleText;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* ShotDistanceText;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trajectory")
    TArray<FVector> CurrentTrajectory;

    UPROPERTY()
    TArray<UImage*> TrajectoryPoints;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap Settings")
    float CurrentZoomLevel = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap Settings")
    float MinZoomLevel = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap Settings")
    float MaxZoomLevel = 3.0f;


    // 게임 모드 참조 (기존과 동일)
    UPROPERTY()
    AInGameMode* GameMode;

    // ⭐ 추가: PlayerManager 참조
    UPROPERTY()
    UGolfPlayerManager* PlayerManager;

    // 미니맵 경계값들 (기존과 동일)
    float MinWorldX, MaxWorldX;
    float MinWorldY, MaxWorldY;

    bool bIsInitialized;
    bool bCaptureInitialized;

    // 자동 새로고침 타이머 (기존과 동일)
    FTimerHandle RefreshTimerHandle;

    // 궤적 표시 함수들 (기존과 동일)
    void CreateTrajectoryPoint(const FVector& WorldPosition);
    void ClearTrajectoryPoints();

    // 거리 계산 (기존과 동일)
    float CalculateDistanceToHole() const;
    float CalculateLastShotDistance() const;

    // 캡처 최적화 관련 변수들 (기존과 동일)
    FDateTime LastCaptureTime;
    bool bCaptureScheduled = false;
    FTimerHandle CaptureDelayTimer;

    // OB 라인 관리 (기존과 동일)
    UPROPERTY()
    TArray<UImage*> OBLineSegments;

    TArray<FVector> CurrentOBPoints;

    // OB 라인 관련 헬퍼 함수들 (기존과 동일)
    void CreateOBLineSegments(const TArray<FVector>& OBPoints);
    void ClearOBLineSegments();
    UImage* CreateOBLineSegment(const FVector2D& StartPos, const FVector2D& EndPos);
    void UpdateOBLineSegmentTransform(UImage* LineImage, const FVector2D& StartPos, const FVector2D& EndPos);

    // ── OB 마스크 오버레이 (OB 바깥 반투명 어둡게) ──────────────────────
    // UpdateOBLines() 호출 시 자동으로 갱신됨. Tick 에서 호출 금지.
    void UpdateOBMaskOverlay();
    void ClearOBMaskOverlay();

    // OB 마스크 오버레이 UImage (MiniMapCanvas 위에 동적 생성)
    UPROPERTY()
    UImage* OBMaskOverlayImage = nullptr;

    // OB 마스크 텍스처 (런타임 생성, PF_B8G8R8A8)
    UPROPERTY()
    UTexture2D* OBMaskTexture = nullptr;

    // OB 바깥 영역 불투명도 (0=완전투명 ~ 1=완전불투명, 기본 0.55)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap OB")
    float OBOutsideDarkness = 0.55f;

    // OB 바깥 영역 색상 (기본 검정)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap OB")
    FLinearColor OBOutsideColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // 마스크 활성화 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap OB")
    bool bShowOBMaskOverlay = true;

    // 좌표 매칭 정보 저장 (기존과 동일)
    bool bDebugCoordinates = false;
    TArray<FVector> DebugWorldPoints;
    TArray<FVector2D> DebugMapPoints;
    void UpdateImprovedCaptureCamera();
    void RefreshUIElementPositions();
    FVector CalculateImprovedMapCenter() const;
    float CalculateImprovedWorldToMapScale() const;

    // ⭐ 축척(WorldToMapScale) ↔ 캡처 반경 동기화를 위한 공용 헬퍼
    // 티/홀컵/OB 라인이 화면 밖으로 잘리지 않기 위한 "최소 필요 캡처 반경"(cm)을 계산
    float CalculateMinRequiredCaptureRadius() const;
    // MapWidth/MapHeight 비율을 유지하는 렌더타겟 해상도 계산 (BaseResolution을 높이 기준으로 사용)
    FIntPoint CalculateRenderTargetSize(int32 BaseResolution) const;


    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coordinate Matching", meta = (AllowPrivateAccess = "true"))
    float CaptureWorldSize = 2000.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coordinate Matching", meta = (AllowPrivateAccess = "true"))
    FVector CaptureWorldCenter = FVector::ZeroVector;

    FTimerHandle CoordinateVerificationTimer;
    FTimerHandle AlignmentFixTimer;

    void TestCoordinateMappingDetailed();



    // UE4 위젯 이벤트 오버라이드
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    // 클릭 위치 검증
    bool IsClickOnMiniMapArea(const FVector2D& LocalClickPosition) const;


    // ⭐ Training Mode 관련 설정값들
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training Mode Settings")
    bool bAllowBallMovement = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training Mode Settings")
    float MinDistanceFromHole = 100.0f; // 홀컵에서 최소 거리 (cm)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training Mode Settings")
    float MaxDistanceFromHole = 50000.0f; // 홀컵에서 최대 거리 (cm)

    // ⭐ Stroke Mode 관련 설정값들
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stroke Mode Settings")
    bool bAllowAimAdjustment = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stroke Mode Settings")
    float AimSensitivity = 1.0f;

    UPROPERTY(meta = (BindWidget), BlueprintReadWrite, Category = "UI Bindings") // BlueprintReadWrite 추가
        UTextBlock* TextBlock_Tip2_Distance_Pick;
    UPROPERTY(meta = (BindWidget), BlueprintReadWrite, Category = "UI Bindings") // BlueprintReadWrite 추가
        UTextBlock* TextBlock_Tip2_Distance_PickToHole;
    UPROPERTY(meta = (BindWidget), BlueprintReadWrite, Category = "UI Bindings") // BlueprintReadWrite 추가
        UTextBlock* TextBlock_Tip2_Height;
    UPROPERTY(meta = (BindWidget), BlueprintReadWrite, Category = "UI Bindings") // BlueprintReadWrite 추가
        UCanvasPanel* CanvasPanel_Tip_2;

    UFUNCTION()
    void MoveToMouseTip2(FVector2D ViewportPos);
    UFUNCTION()
    void UpdateTip2();
    UFUNCTION()
    void InitTip2();


    // 좌표 변환 함수들 (기존과 동일)
    FVector2D WorldToMapPosition(const FVector& WorldPosition) const;

    UFUNCTION()
    void CalculateHoleRotationAngle();

    UFUNCTION()
    FVector2D RotatePoint(const FVector2D& Point, float AngleRadians) const;



    // UI 업데이트 함수들 (이제 PlayerIndex를 받음)
    void UpdateImagePosition(class UImage* Image, const FVector& WorldPosition, const FLinearColor& Color, int32 PlayerIndex = -1); // ⭐ 수정
    void UpdateAimLinePosition(int32 PlayerIndex); // ⭐ 수정


    // 블루프린트에서 호출할 수 있는 함수들
    UFUNCTION(BlueprintCallable, Category = "MiniMap|Ball To Hole Line")
    void SetBallToHoleLineVisible(bool bVisible);

    UFUNCTION(BlueprintCallable, Category = "MiniMap|Ball To Hole Line")
    void SetBallToHoleLineColor(FLinearColor NewColor);

    UFUNCTION(BlueprintCallable, Category = "MiniMap|Ball To Hole Line")
    void SetShowOnlyCurrentPlayerLine(bool bOnlyCurrentPlayer);

    // 볼-홀컵 라인 관련 설정 함수들
    UFUNCTION(BlueprintCallable, Category = "MiniMap|Ball To Hole Line")
    void EnableBallToHoleLine(bool bEnable);

    UFUNCTION(BlueprintCallable, Category = "MiniMap|Ball To Hole Line")
    void SetBallToHoleLineThickness(float Thickness);

    UFUNCTION(BlueprintCallable, Category = "MiniMap|Ball To Hole Line")
    bool IsBallToHoleLineVisible() const { return bShowBallToHoleLine; }

    UFUNCTION(BlueprintCallable, Category = "MiniMap|Ball To Hole Line")
    void RefreshAllBallToHoleLines();

    UFUNCTION(BlueprintCallable, Category = "Debug AimActor")
    void ShowAimActorMapPosition(int32 PlayerIndex);

    UFUNCTION(BlueprintCallable, Category = "MiniMap AimActor")
    void UpdateAimActorPosition(int32 PlayerIndex, const FVector& AimActorPosition);

    void HideAllAimActorExceptCurrent();

    void DebugAimLineStatus();

    UFUNCTION(BlueprintCallable, Category = "MiniMap AimActor")
    void UpdateBallToAimLinePosition(int32 PlayerIndex);

    void DebugAimActorLines(int32 PlayerIndex);

private:
    // ⭐ 추가: 각 플레이어 볼 및 에임 라인 위젯을 저장할 TMap

    TMap<int32, UImage*> PlayerBallImages;

    TMap<int32, UImage*> PlayerBallToHoleLineImages; // 각 플레이어의 볼-홀컵 라인 이미지

    TMap<int32, FSlateBrush> PlayerBallBrushes;

    TArray<UTexture2D*> PlayerBallTextures;

    // AimActor 관련

    TMap<int32, UImage*> PlayerAimActorImages; // 각 플레이어의 AimActor 아이콘


    TMap<int32, FVector> PlayerAimActorPositions; // 각 플레이어의 AimActor 위치


    TMap<int32, UImage*> PlayerBallToAimLineImages; // 볼 → AimActor 라인

    TMap<int32, UImage*> PlayerAimToHoleLineImages; // AimActor → 홀컵 라인


    FVector CalculateMapCenter() const;
    float CalculateWorldToMapScale() const;



    void UpdateFlagPosition();
    void UpdateInfoTexts();
    void UpdateMiniMapBounds();

    // 맵 캡처 관련 함수들 (기존과 동일)
    void CreateCaptureComponents();
    void UpdateCaptureCamera();
    void ApplyCapturedTexture();

    // 그림자 비활성화 함수
    void DisableShadowsForCapture();

    // Background image flip 적용(UMG 렌더 변환)
    void ApplyBackgroundFlipTransform() const;


    // ⭐ OB 관련 설정값들 추가

    bool bEnableOBCheck = true;

    bool bShowOBWarnings = true;

    float OBWarningDuration = 3.0f;


    UPROPERTY()
    float HoleRotationAngle = 0.0f;  // 홀 방향 회전 각도 (라디안)



    // 볼-홀컵 라인 설정

    bool bShowBallToHoleLine = true; // 볼-홀컵 라인 표시 여부
    FLinearColor BallToHoleLineColor = FLinearColor(0.0f, 1.0f, 0.0f, 0.8f); // 녹색, 약간 투명
    float BallToHoleLineThickness = 2.0f; // 라인 두께
    bool bShowOnlyCurrentPlayerLine = false; // true면 현재 플레이어만, false면 모든 플레이어
    FTimerHandle DelayedCaptureTimer;


    int32 MaxPlayerCount = 6;

    // 함수 선언 추가
    UFUNCTION(BlueprintCallable, Category = "MiniMap Textures")
    void LoadAllPlayerBallTextures();

    UFUNCTION(BlueprintCallable, Category = "MiniMap Textures")
    UTexture2D* GetPlayerBallTexture(int32 PlayerIndex) const;

    UFUNCTION(BlueprintCallable, Category = "MiniMap Textures")
    bool AreAllTexturesLoaded() const;


    // AimActor 관련 함수들
    UFUNCTION(BlueprintCallable, Category = "MiniMap AimActor")
    void CreateBallToAimLineForPlayer(int32 PlayerIndex);

    UFUNCTION(BlueprintCallable, Category = "MiniMap AimActor")
    void CreateAimToHoleLineForPlayer(int32 PlayerIndex);


    UFUNCTION(BlueprintCallable, Category = "MiniMap AimActor")
    void UpdateAimToHoleLinePosition(int32 PlayerIndex);
    // 함수 선언

    UFUNCTION(BlueprintCallable, Category = "MiniMap AimActor")
    void CreateAimActorForPlayer(int32 PlayerIndex);

    UFUNCTION(BlueprintCallable, Category = "MiniMap AimActor")
    void RemoveAimActorForPlayer(int32 PlayerIndex);

    UFUNCTION(BlueprintCallable, Category = "MiniMap AimActor")
    void SetAimActorVisible(bool bVisible);

    void SetAimLineColors(FLinearColor BallToAimColor, FLinearColor AimToHoleColor);

    // 라인 업데이트 헬퍼 함수들
    void UpdateLinePosition(UImage* LineImage, const FVector2D& StartPos, const FVector2D& EndPos,
        const FLinearColor& LineColor, int32 PlayerIndex);

    int32 GetLoadedTextureCount() const;

    UFUNCTION(BlueprintCallable, Category = "MiniMap Click")
    void SetAimToWorldPosition(const FVector& WorldPosition);

    FVector ApplyHolecupDistanceLimit(const FVector& ClickedPosition);

    void SetTip2DefaultValues();

    FTimerHandle Tip2InitTimer;

    void CreateCaptureComponentsInline();

    void SetTip2SafeDefaults();

    FVector ApplyAimPositionLimit(const FVector& OriginalAimPosition);

protected:
    // 내부 함수들
    void UpdateBallToHoleLinePosition(int32 PlayerIndex);
    void CreateBallToHoleLineForPlayer(int32 PlayerIndex);
    void RemoveBallToHoleLineForPlayer(int32 PlayerIndex);
    void ClearAllBallToHoleLines();

    void SetBallTexture(UTexture2D* NewTexture);

    void CreateAimLineForPlayer(int32 PlayerIndex);
    void EnsureAimLineExists(int32 PlayerIndex);



    // AimActor 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap AimActor")
    UTexture2D* AimActorTexture;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap AimActor")
    FLinearColor AimActorColor = FLinearColor::Yellow;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap AimActor")
    float AimActorIconSize = 14.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap AimActor")
    bool bShowAimActor = true;
    // 라인 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap AimActor")
    FLinearColor BallToAimLineColor = FLinearColor::Yellow;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap AimActor")
    FLinearColor AimToHoleLineColor = FLinearColor(0.0f, 1.0f, 1.0f, 0.8f); // 시안색

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap AimActor")
    float AimLineThickness = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap AimActor")
    bool bShowOnlyCurrentPlayerAimLines = false;


    FVector FindGroundPosition(const FVector& TargetPosition, AActor* IgnoreActor);
    bool IsValidGroundPosition(const FVector& Position) const;
    bool IsLandscapeHit(const FHitResult& HitResult) const;

    UPROPERTY()
    UMaterialInstanceDynamic* MapDynamicMaterial = nullptr;


    // GolfMinimap.h에 추가할 함수 선언
public:
    // 현재 플레이어만 표시하는 기능 추가
    UFUNCTION(BlueprintCallable, Category = "MiniMap Players")
    void ShowOnlyCurrentPlayer(int32 CurrentPlayerIndex);

    UFUNCTION(BlueprintCallable, Category = "MiniMap Players")
    void HideAllPlayersExcept(int32 PlayerIndex);

    UFUNCTION(BlueprintCallable, Category = "MiniMap Players")
    void HidePlayerElements(int32 PlayerIndex);

    UFUNCTION(BlueprintCallable, Category = "MiniMap Players")
    void ShowPlayerElements(int32 PlayerIndex);

    // 플레이어 전환 시 호출되는 함수
    UFUNCTION(BlueprintCallable, Category = "MiniMap Players")
    void OnPlayerTurnChanged(int32 NewCurrentPlayerIndex, int32 PreviousPlayerIndex);

private:
    // 현재 플레이어 추적
    int32 LastDisplayedPlayerIndex = -1;
};