#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "PuttingGuide.h"
#include "StrokeWidget.generated.h"

class AInGameMode; // ⭐ AInGameMode에 대한 전방 선언 추가

class UVerticalBox;
class UOverlay;
class UImage;

UCLASS()
class PARKDAY_API UStrokeWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& InGeometry, float InDeltaTime) override;
    virtual void NativeDestruct() override;

    // 버튼 클릭 이벤트
    UFUNCTION()
        void OnMulliganButtonClicked();

    UFUNCTION()
        void OnMenuButtonClicked();

    void UpdateMulliganTexture();

    UFUNCTION()
        void OnOKButtonClicked();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PlayerTurnCard")
    TArray<TSoftObjectPtr<UTexture2D>> TurnCards;

    void HideAll();
    void ShowAll();

    void HideUI();
    void ShowUI();

    // 맵 정보 업데이트 함수
    UFUNCTION(BlueprintCallable, Category = "UI")
        void UpdateMapInfo(int32 HoleNumber, int32 Par, float CourseLength);

    // 에임 정보 업데이트
    UFUNCTION(BlueprintCallable, Category = "UI")
        void UpdateAimInfo(float Distance, float Height);

    // ⭐ 추가: 특정 화면 위치에 에임 정보창을 표시하는 함수
    UFUNCTION(BlueprintCallable, Category = "UI")
        void ShowAimInfoAtLocation(FVector2D ScreenPosition, bool bVisible);

    void ShowAimInfo(bool bVisible);
    void SetLandType(int32 nType);

    void HideAllChildren(class UCanvasPanel* Canvas);
    void ShowAllChildren(class UCanvasPanel* Canvas);
    
    UPROPERTY(meta =(BindWidget))   UCanvasPanel* CanvasPanel_Tour;
    UPROPERTY(meta =(BindWidget))   UCanvasPanel* CanvasPanel_CourseInfo;
    UPROPERTY(meta =(BindWidget))   UVerticalBox* VerticalBox_PlayerList;
    UPROPERTY(meta =(BindWidget))   UCanvasPanel* CanvasPanel_Notice;
    UPROPERTY(meta =(BindWidget))   UOverlay* Overlay_BallInfo;
    UPROPERTY(meta =(BindWidget))   UButton* Button_Tour_Stop;
    UPROPERTY(meta =(BindWidget))   UImage* Image_Player_number;

    UFUNCTION() void OnClickedTourStop();

    UPROPERTY(meta = (BindWidget), BlueprintReadWrite, Category = "UI Bindings") // BlueprintReadWrite 추가
        class UCanvasPanel* CanvasPanel_Tip_1;

    UPROPERTY(meta = (BindWidget), BlueprintReadWrite, Category = "UI Bindings") // BlueprintReadWrite 추가
        class UCanvasPanel* CanvasPanel_Minimap;

    UPROPERTY(meta = (BindWidget))
        class UDistanceWidget* WBP_Distance;

    FTimerHandle WidgetHideTimer;

    UPROPERTY(meta = (BindWidget))
        class UCanvasPanel* CanvasPanel_PlayerTurn;  // 또는 UUserWidget* 혹은 UCanvasPanelSlot* 등

    UPROPERTY(meta = (BindWidget), BlueprintReadWrite, Category = "UI Bindings") // BlueprintReadWrite 추가
        class UTextBlock* turn_Name;

    UPROPERTY(meta = (BindWidget), BlueprintReadWrite, Category = "UI Bindings") // BlueprintReadWrite 추가
        class UTextBlock* TextBlock_PinDistance;

    // ⭐⭐⭐ PuttingGuide 관련 UI 및 타이머 추가
    UPROPERTY(meta = (BindWidget), BlueprintReadWrite, Category = "UI Bindings")
        class UCanvasPanel* Canvas_PuttingGuid;

    UPROPERTY(meta = (BindWidget), BlueprintReadWrite, Category = "UI Bindings")
        class UTextBlock* TextBlock_Guid;

    // PuttingGuide 타이머 (자동 숨김 기능용)
    FTimerHandle PuttingGuidanceHideTimer;

    // PuttingGuide 분석 결과 표시 함수
    UFUNCTION(BlueprintCallable, Category = "Golf|Putting")
        void DisplayPuttingGuidance();

    // PuttingGuide 텍스트 업데이트 함수
    UFUNCTION(BlueprintCallable, Category = "Golf|Putting")
        void UpdatePuttingGuidanceText(const FString& GuidanceText);

    // PuttingGuide 패널 위치 설정 함수
    UFUNCTION(BlueprintCallable, Category = "Golf|Putting")
        void PositionPuttingGuidancePanel(FVector2D ScreenPosition);

    // PuttingGuide 패널 표시/숨김
    UFUNCTION(BlueprintCallable, Category = "Golf|Putting")
        void ShowPuttingGuidancePanel(bool bShow);

    // PuttingGuide 자동 숨김 기능
    UFUNCTION(BlueprintCallable, Category = "Golf|Putting")
        void DisplayPuttingGuidanceWithAutoHide(float HideDuration = 5.0f);

    void ShowCanvasAndHideAfterDelay(const FString& ActorName);
    void HidePlayerTurnCanvasWidget();
    UFUNCTION()
        void UseMulliganWrapper();

    UFUNCTION()
        void UseOKWrapper();

    // UMG에서 바인딩할 위젯
    UPROPERTY(meta = (BindWidget), BlueprintReadWrite, Category = "UI Bindings") // BlueprintReadWrite 추가
        class UTextBlock* TextBlock_CourseInfo_Name;

    UPROPERTY(meta = (BindWidget), BlueprintReadWrite, Category = "UI Bindings") // BlueprintReadWrite 추가
        class UTextBlock* TextBlock_CourseInfo_OutCourse;

    UPROPERTY(meta = (BindWidget), BlueprintReadWrite, Category = "UI Bindings") // BlueprintReadWrite 추가
        class UTextBlock* TextBlock_CourseInfo_ParIndex;

    UPROPERTY(meta = (BindWidget), BlueprintReadWrite, Category = "UI Bindings") // BlueprintReadWrite 추가
        class UTextBlock* TextBlock_CourseInfo_ParCount;

    UPROPERTY(meta = (BindWidget), BlueprintReadWrite, Category = "UI Bindings") // BlueprintReadWrite 추가
        class UTextBlock* TextBlock_CourseInfo_Distance;

    //UPROPERTY(meta = (BindWidget), BlueprintReadWrite, Category = "UI Bindings") // BlueprintReadWrite 추가
        //class UTextBlock* TextBlock_BallLocation;

    UPROPERTY(meta = (BindWidget), BlueprintReadWrite, Category = "UI Bindings") // BlueprintReadWrite 추가
        class UTextBlock* TextBlock_Tip1_Distance;

    UPROPERTY(meta = (BindWidget), BlueprintReadWrite, Category = "UI Bindings") // BlueprintReadWrite 추가
        class UTextBlock* TextBlock_Tip1_Height;

    UPROPERTY(meta = (BindWidget), BlueprintReadWrite, Category = "UI Bindings") // BlueprintReadWrite 추가
        class UImage* Image_CourseMap;
    UPROPERTY(meta = (BindWidget), BlueprintReadWrite, Category = "UI Bindings") // BlueprintReadWrite 추가
        UTextBlock* TextBlock_Percent;


    UPROPERTY(meta = (BindWidget), BlueprintReadWrite, Category = "UI Bindings") // BlueprintReadWrite 추가
        class UImage* Image_BG;

    UFUNCTION()
        void SetPercentText(float Percent);
    UPROPERTY(meta = (BindWidget), BlueprintReadWrite, Category = "UI Bindings") // BlueprintReadWrite 추가
        class UImage* Image_BallLocation_1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Land Type Textures")
        TMap<int32, UTexture2D*> LandTypeTextures;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Land Type Textures")
        TMap<int32, FString> LandTypeNames;

protected:

    UPROPERTY(meta = (BindWidget), BlueprintReadWrite, Category = "UI Bindings") // BlueprintReadWrite 추가
        class UButton* Button_Muligan;

    UPROPERTY(meta = (BindWidget), BlueprintReadWrite, Category = "UI Bindings") // BlueprintReadWrite 추가
        class UButton* Button_OK;

    UPROPERTY(meta = (BindWidget), BlueprintReadWrite, Category = "UI Bindings") // BlueprintReadWrite 추가
        UButton* Button_Menu;

    // 위치 조정용 멤버 변수
    UPROPERTY(EditAnywhere, Category = "UI Config")
        FVector2D CanvasPanelPivot = FVector2D(0.5f, 0.5f);  // 피벗 포인트 (중심)

    UPROPERTY(EditAnywhere, Category = "UI Config")
        FVector2D CanvasPanelOffset = FVector2D(-50.f, -100.f);  // 화면상 오프셋 (위로 100px)

    UPROPERTY(EditAnywhere, Category = "UI Config")
        float HolecupHeightOffset = 250.f;  // 홀컵에서 얼마나 위로 표시할지 (cm)


public:
    void UpdateShotBallSpeedAndAngle();
    // 홀컵 위에 UI를 배치하는 함수
    void PositionCanvasPanelAboveHole();

    void DrawDebugCanvasPosition();

    // 플레이어 정보 슬롯 (UMG에서 Child Widget으로 추가)
    //UPROPERTY(meta = (BindWidget), BlueprintReadWrite, Category = "UI Bindings") // BlueprintReadWrite 추가
    //   class UUserWidget* PlayerInfoSlot;


         // ⭐ 텍스처 접근 함수
    UFUNCTION(BlueprintCallable, Category = "UI")
        UTexture2D* GetLandTypeTexture(int32 Type);

    UFUNCTION(BlueprintCallable, Category = "UI")
        FString GetLandTypeName(int32 Type);

    // 홀컵 위치 기반 배치
    UFUNCTION(BlueprintCallable, Category = "UI")
        void PositionPuttingGuidancePanelAtHole(
            FVector HoleWorldLocation,
            const FPuttingGuideResult& Result);  // Result 구조체 전달

    // 조정 가능한 변수들
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Golf|Putting")
        FVector2D PuttingGuidancePanelSize = FVector2D(137.0f, 80.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Golf|Putting")
        float HolecupUIOffsetY = -50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Golf|Putting")
        FVector2D ViewportSize = FVector2D(1920.0f, 1080.0f);


    // ⭐ 좌우 편차 테스트 함수
    UFUNCTION(BlueprintCallable, Category = "Debug|Putting")
        void TestPuttingGuidancePosition(float TestRightDistance = 1000.0f);

    // 테스트 모드 활성화
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Putting")
        bool bDebugPositionTest = false;

    // 테스트 호수 저장
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Putting")
        float TestHoleDistance = 1000.0f;

    // 여러 좌우 거리 테스트
    UFUNCTION(BlueprintCallable, Category = "Debug|Putting")
        void TestMultipleLateralDistances();


private:
    // ⭐ 게임 스레드에서만 호출
    void LoadLandTypeTextures();

    // ⭐ 로딩 상태
    bool bTexturesLoaded = false;

    UPROPERTY()
        TMap<int32, UTexture2D*> LandTypeTextureMap;

    void InitializeLandTypeTextures();

    float CalculateDistanceScale(float DistanceMeter) const;

};