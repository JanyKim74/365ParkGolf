// StrokeMenuWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h" // 여전히 UButton을 사용할 수 있도록 포함
#include "Components/TextBlock.h" // ⭐ TextBlock_MulliganCount 사용을 위해 전방선언 대신 직접 include
#include "Delegates/DelegateCombinations.h"
#include "StrokeMenuWidget.generated.h"

class AInGameMode;
class UUserWidget;

// 메뉴 버튼 클릭 이벤트를 외부에 알리기 위한 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStrokeMenuButtonClicked, int32, ButtonIndex);

/**
 * @brief 스트로크 메뉴에 사용될 12개의 버튼을 포함하는 위젯입니다.
 */
UCLASS()
class PARKDAY_API UStrokeMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 생성자
    UStrokeMenuWidget(const FObjectInitializer& ObjectInitializer);

    // 메뉴 버튼 클릭 이벤트를 외부에 노출
    UPROPERTY(BlueprintAssignable, Category = "UI Events")
    FOnStrokeMenuButtonClicked OnMenuButtonClicked;

    UFUNCTION(BlueprintCallable, Category = "UI")
    void SetClickEventType(int32 iValue);

    // 12개의 메뉴 버튼에 대한 참조 (블루프린트 위젯 자체)
    // ⭐ UButton* 대신 UUserWidget* 로 변경합니다.
    UPROPERTY(meta = (BindWidget))
    UUserWidget* WBP_InGame_Menu_Grid;
    UPROPERTY(meta = (BindWidget))
    UUserWidget* WBP_InGame_Menu_ScoreCard;
    UPROPERTY(meta = (BindWidget))
    UUserWidget* WBP_InGame_Menu_PlayerAdd;
    UPROPERTY(meta = (BindWidget))
    UUserWidget* Button_mulligan;
    UPROPERTY(meta = (BindWidget))
    UUserWidget* WBP_InGame_Menu_NextHole;
    UPROPERTY(meta = (BindWidget))
    UUserWidget* WBP_InGame_Menu_PenaltyDrop;
    //UPROPERTY(meta = (BindWidget))
    //    UUserWidget* WBP_InGame_Menu_Button_6;
    UPROPERTY(meta = (BindWidget))
    UUserWidget* WBP_InGame_Menu_Preview;
    //UPROPERTY(meta = (BindWidget))
    //    UUserWidget* WBP_InGame_Menu_Button_8;
    UPROPERTY(meta = (BindWidget))
    UUserWidget* WBP_InGame_Menu_SkipTurn;
    //UPROPERTY(meta = (BindWidget))
    //    UUserWidget* WBP_InGame_Menu_Button_10;
    UPROPERTY(meta = (BindWidget))
    UUserWidget* WBP_InGame_Menu_Button_ExitRound;

    // ⭐ 현재 플레이어의 남은 멀리건 개수를 표시
    UPROPERTY(meta = (BindWidget))
    UTextBlock* TextBlock_MulliganCount;

public:
    //UFUNCTION()
    //void UseMulliganWrapper();

    // 블루프린트 위젯 내부의 실제 UButton을 가져오는 헬퍼 함수
    // 'ButtonNameInBlueprintWidget'은 해당 블루프린트 위젯 내부에 있는 UButton의 UMG 변수 이름입니다.
    UButton* GetButtonFromUserWidget(UUserWidget* UserWidget, FName ButtonNameInBlueprintWidget);
    UFUNCTION()
    FSlateBrush MakeImageBrush(UTexture2D* Texture, FVector2D DesiredSize);
    UFUNCTION()
    void ApplyButtonStyle(bool bOn);

    UPROPERTY(EditAnywhere, Category = "UI")
    UTexture2D* OnImage;

    UPROPERTY(EditAnywhere, Category = "UI")
    UTexture2D* OffImage;

    // ⭐ 메뉴가 Visible로 바뀔 때마다(재생성 없이 토글되는 구조라) 멀리건 개수를 새로 갱신하기 위해 오버라이드
    // (public이어야 함 - UWidget::SetVisibility 원본이 public이고, 외부에서 StrokeMenuWidgetInstance->SetVisibility(...)로 호출하는 기존 코드가 많음)
    virtual void SetVisibility(ESlateVisibility InVisibility) override;

protected:
    // 위젯 초기화 시 호출됩니다. (Blueprint에서 위젯이 생성될 때)
    virtual void NativeConstruct() override;

private:
    AInGameMode* GM;

    UPROPERTY()
    bool bIsOnGird = false;




    // 각 버튼 클릭 이벤트를 처리할 함수 (UFUNCTION으로 선언)
    UFUNCTION() void OnButtonShowGrid();
    UFUNCTION() void OnButtonShowScoreCard();
    UFUNCTION() void OnButtonPlayerControl();
    UFUNCTION() void OnButtonMulligan();
    UFUNCTION() void OnButtonNextHole();
    UFUNCTION() void OnButtonPaneltyDrop();
    UFUNCTION() void OnButtonSwingMotion();
    UFUNCTION() void OnButtonTourCamera();
    UFUNCTION() void OnButtonCameraMode();
    UFUNCTION() void OnButtonNextPlayer();
    UFUNCTION() void OnButtonUseOK();
    UFUNCTION() void OnButtonRoundExit();

    void LockClick();

    // 버튼 클릭 이벤트 바인딩을 위한 헬퍼 함수
    void BindButtonEvents();


    UFUNCTION()
    void ShowScoreBoard();

    // ⭐ 현재 턴 플레이어의 남은 멀리건 개수를 TextBlock_MulliganCount에 반영
    void UpdateMulliganCountText();
};