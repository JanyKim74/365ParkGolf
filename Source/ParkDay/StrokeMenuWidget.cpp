// StrokeMenuWidget.cpp
#include "StrokeMenuWidget.h"
#include "Components/Button.h" // UButton을 사용하기 위해 포함 (헤더에 이미 있지만, .cpp에서도 명시적으로 포함하는 것이 좋습니다)
#include "Components/TextBlock.h"
#include "InGameMode.h"
#include "Widgets/InGameMenuPopup.h"
#include "GolfPlayerManager.h"
#include "GolfPlayer.h"
#include "Widgets/InGamePlayerSelectWidget.h"
#include "GolfPlayerController.h"
#include "PlayerInfoSlotWidget.h"
#include "ParkDay/TourActor.h"
#include "ParkDay/Utils/UtilLibrary.h"
#include "ParkDay/Widgets/CameraModePopupWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "StrokeMenuButtonWidget.h"

UStrokeMenuWidget::UStrokeMenuWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    ApplyButtonStyle(false);
}

void UStrokeMenuWidget::SetClickEventType(int32 iValue)
{
}

void UStrokeMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    // 모든 버튼의 클릭 이벤트를 바인딩합니다.
    BindButtonEvents();

    //if (TextBlock_MulliganCount)
    //{
    //    if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(TextBlock_MulliganCount->Slot))
    //    {
    //        CanvasSlot->SetZOrder(10);  // 숫자 클수록 위
    //    }
    //}

    // ⭐ 생성 시점에 이미 Visible 상태일 수 있으니 한 번 갱신
    UpdateMulliganCountText();
}

void UStrokeMenuWidget::SetVisibility(ESlateVisibility InVisibility)
{
    Super::SetVisibility(InVisibility);

    // ⭐ 메뉴가 다시 보일 때마다(플레이어/홀이 바뀐 뒤일 수 있으므로) 최신 값으로 갱신
    if (InVisibility == ESlateVisibility::Visible)
    {
        UpdateMulliganCountText();
    }
}

void UStrokeMenuWidget::UpdateMulliganCountText()
{
    if (!TextBlock_MulliganCount)
        return;

    if (!GM)
        GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());

    if (!GM)
        return;

    AGolfPlayer* Player = GM->GetCurrentTurnGolfPlayer();
    if (!Player)
        return;

    const int32 MaxMulligan = GM->GameInfo.GameOptions.Mulligan_Count;      // 게임 옵션에 설정된 최대 멀리건 횟수
    const int32 UsedMulligan = Player->PlayerInfo.MulliganCount;            // 현재 플레이어가 이미 사용한 횟수
    const int32 RemainingMulligan = FMath::Max(0, MaxMulligan - UsedMulligan);

    if (MaxMulligan <= -1)
    {
        TextBlock_MulliganCount->SetText(FText::FromString("Infinity"));
    }
    else
    {
        // "남은 / 총" 형식으로 표기 (예: "2 / 3")
        TextBlock_MulliganCount->SetText(FText::FromString(
            FString::Printf(TEXT("%d / %d"), RemainingMulligan, MaxMulligan)));
    }

}

// ⭐ UUserWidget 내부에 있는 실제 UButton을 찾아 반환하는 헬퍼 함수
UButton* UStrokeMenuWidget::GetButtonFromUserWidget(UUserWidget* UserWidget, FName ButtonNameInBlueprintWidget)
{
    if (!UserWidget)
    {
        return nullptr;
    }
    // GetWidgetFromName을 사용하여 블루프린트 위젯 내부의 UButton 인스턴스를 찾습니다.
    // 이 UButton은 블루프린트 위젯에서 'Is Variable'로 체크되어 변수 이름을 가져야 합니다.
    return Cast<UButton>(UserWidget->GetWidgetFromName(ButtonNameInBlueprintWidget));
}

void UStrokeMenuWidget::ShowScoreBoard()
{

}

void UStrokeMenuWidget::BindButtonEvents()
{
    // 각 UUserWidget 변수에서 실제 UButton을 찾아서 클릭 이벤트를 바인딩합니다.
    // "ActualButtonNameInsideBP" 부분은 해당 블루프린트 위젯 내부에 있는 UButton의 변수 이름입니다.
    // 예: WBP_InGame_Menu_Button 블루프린트 안에 "MyButton"이라는 이름의 UButton이 있다면,
    // GetButtonFromUserWidget(WBP_InGame_Menu_Button, TEXT("MyButton"))

    // ⭐ 중요: 아래의 TEXT("Button") 부분은 WBP_InGame_Menu_Button 블루프린트 위젯 내부에 있는 UButton의 실제 이름으로 변경해야 합니다.
    // 예를 들어, 블루프린트 위젯의 Button 컴포넌트 이름이 'Button_Internal'이라면, TEXT("Button_Internal")이 됩니다.
    // 만약 블루프린트 위젯이 UButton 자체라면, 캐스팅만으로도 충분합니다.

    UButton* CurrentButton = nullptr;

    CurrentButton = GetButtonFromUserWidget(WBP_InGame_Menu_Grid, TEXT("Button_Menu")); // ⭐ "Button"을 실제 블루프린트 위젯 내의 버튼 이름으로 변경
    if (CurrentButton) CurrentButton->OnClicked.AddDynamic(this, &UStrokeMenuWidget::OnButtonShowGrid);

    CurrentButton = GetButtonFromUserWidget(WBP_InGame_Menu_ScoreCard, TEXT("Button_Menu")); // ⭐ 실제 이름으로 변경
    if (CurrentButton) CurrentButton->OnClicked.AddDynamic(this, &UStrokeMenuWidget::OnButtonShowScoreCard);

    CurrentButton = GetButtonFromUserWidget(WBP_InGame_Menu_PlayerAdd, TEXT("Button_Menu")); // ⭐ 실제 이름으로 변경
    if (CurrentButton) CurrentButton->OnClicked.AddDynamic(this, &UStrokeMenuWidget::OnButtonPlayerControl);

    CurrentButton = GetButtonFromUserWidget(Button_mulligan, TEXT("Button_Menu")); // ⭐ 실제 이름으로 변경
    if (CurrentButton) CurrentButton->OnClicked.AddDynamic(this, &UStrokeMenuWidget::OnButtonMulligan);

    CurrentButton = GetButtonFromUserWidget(WBP_InGame_Menu_NextHole, TEXT("Button_Menu")); // ⭐ 실제 이름으로 변경
    if (CurrentButton) CurrentButton->OnClicked.AddDynamic(this, &UStrokeMenuWidget::OnButtonNextHole);

    CurrentButton = GetButtonFromUserWidget(WBP_InGame_Menu_PenaltyDrop, TEXT("Button_Menu")); // ⭐ 실제 이름으로 변경
    if (CurrentButton) CurrentButton->OnClicked.AddDynamic(this, &UStrokeMenuWidget::OnButtonPaneltyDrop);

    //CurrentButton = GetButtonFromUserWidget(WBP_InGame_Menu_Button_6, TEXT("Button_Menu")); // ⭐ 실제 이름으로 변경
    //if (CurrentButton) CurrentButton->OnClicked.AddDynamic(this, &UStrokeMenuWidget::OnButtonSwingMotion);

    CurrentButton = GetButtonFromUserWidget(WBP_InGame_Menu_Preview, TEXT("Button_Menu")); // ⭐ 실제 이름으로 변경
    if (CurrentButton) CurrentButton->OnClicked.AddDynamic(this, &UStrokeMenuWidget::OnButtonTourCamera);

    //CurrentButton = GetButtonFromUserWidget(WBP_InGame_Menu_Button_8, TEXT("Button_Menu")); // ⭐ 실제 이름으로 변경
    //if (CurrentButton) CurrentButton->OnClicked.AddDynamic(this, &UStrokeMenuWidget::OnButtonCameraMode);

    CurrentButton = GetButtonFromUserWidget(WBP_InGame_Menu_SkipTurn, TEXT("Button_Menu")); // ⭐ 실제 이름으로 변경
    if (CurrentButton) CurrentButton->OnClicked.AddDynamic(this, &UStrokeMenuWidget::OnButtonNextPlayer);

    //CurrentButton = GetButtonFromUserWidget(WBP_InGame_Menu_Button_10, TEXT("Button_Menu")); // ⭐ 실제 이름으로 변경
    //if (CurrentButton) CurrentButton->OnClicked.AddDynamic(this, &UStrokeMenuWidget::OnButtonUseOK);

    CurrentButton = GetButtonFromUserWidget(WBP_InGame_Menu_Button_ExitRound, TEXT("Button_Menu")); // ⭐ 실제 이름으로 변경
    if (CurrentButton) CurrentButton->OnClicked.AddDynamic(this, &UStrokeMenuWidget::OnButtonRoundExit);

}

FSlateBrush UStrokeMenuWidget::MakeImageBrush(UTexture2D* Texture, FVector2D DesiredSize)
{
    FSlateBrush Brush;
    Brush.DrawAs = ESlateBrushDrawType::Image;       // 이미지로 그리기
    Brush.Tiling = ESlateBrushTileType::NoTile;
    Brush.Mirroring = ESlateBrushMirrorType::NoMirror;
    Brush.ImageSize = (DesiredSize.IsNearlyZero())
        ? FVector2D(Texture ? Texture->GetSizeX() : 64, Texture ? Texture->GetSizeY() : 64)
        : DesiredSize;
    Brush.SetResourceObject(Texture);
    return Brush;
}

void UStrokeMenuWidget::ApplyButtonStyle(bool bOn)
{
    if (!WBP_InGame_Menu_ScoreCard) return;

    UTexture2D* BaseTex = bOn ? OnImage : OffImage;
    // 안전장치: 텍스처 없으면 그대로 두기
    if (!BaseTex)
    {
        // 최소한 클릭은 되도록 스타일은 유지
        return;
    }

    FVector2D Size(BaseTex->GetSizeX(), BaseTex->GetSizeY());
    FSlateBrush NormalBrush = MakeImageBrush(BaseTex, Size);
    FSlateBrush HoveredBrush = NormalBrush;
    FSlateBrush PressedBrush = NormalBrush;

    FButtonStyle NewStyle;
    NewStyle.SetNormal(NormalBrush);
    NewStyle.SetHovered(HoveredBrush);
    NewStyle.SetPressed(PressedBrush);
    NewStyle.SetDisabled(NormalBrush);

    NewStyle.SetNormalPadding(FMargin(0));
    NewStyle.SetPressedPadding(FMargin(0));

    // 스타일 교체

    //if (UButton* FoundButton = Cast<UButton>(WBP_InGame_Menu_Grid->GetWidgetFromName(TEXT("Button_Menu"))))
    //{
    //    FoundButton->SetStyle(NewStyle);
    //}
}

void UStrokeMenuWidget::LockClick()
{
    UUtilLibrary::LockButtonForSeconds(GetButtonFromUserWidget(WBP_InGame_Menu_Grid, TEXT("Button_Menu")), GetWorld(), 0.33f);
    UUtilLibrary::LockButtonForSeconds(GetButtonFromUserWidget(WBP_InGame_Menu_ScoreCard, TEXT("Button_Menu")), GetWorld(), 0.33f);
    UUtilLibrary::LockButtonForSeconds(GetButtonFromUserWidget(WBP_InGame_Menu_PlayerAdd, TEXT("Button_Menu")), GetWorld(), 0.33f);
    UUtilLibrary::LockButtonForSeconds(GetButtonFromUserWidget(Button_mulligan, TEXT("Button_Menu")), GetWorld(), 0.33f);
    UUtilLibrary::LockButtonForSeconds(GetButtonFromUserWidget(WBP_InGame_Menu_NextHole, TEXT("Button_Menu")), GetWorld(), 0.33f);
    UUtilLibrary::LockButtonForSeconds(GetButtonFromUserWidget(WBP_InGame_Menu_PenaltyDrop, TEXT("Button_Menu")), GetWorld(), 0.33f);
    //   UUtilLibrary::LockButtonForSeconds(GetButtonFromUserWidget(WBP_InGame_Menu_Button_6, TEXT("Button_Menu")), GetWorld(), 0.33f);
    UUtilLibrary::LockButtonForSeconds(GetButtonFromUserWidget(WBP_InGame_Menu_Preview, TEXT("Button_Menu")), GetWorld(), 0.33f);
    //   UUtilLibrary::LockButtonForSeconds(GetButtonFromUserWidget(WBP_InGame_Menu_Button_8, TEXT("Button_Menu")), GetWorld(), 0.33f);
    UUtilLibrary::LockButtonForSeconds(GetButtonFromUserWidget(WBP_InGame_Menu_SkipTurn, TEXT("Button_Menu")), GetWorld(), 0.33f);
    //   UUtilLibrary::LockButtonForSeconds(GetButtonFromUserWidget(WBP_InGame_Menu_Button_10, TEXT("Button_Menu")), GetWorld(), 0.33f);
}

// 각 버튼 클릭 이벤트 핸들러 구현
// 그리드보기
void UStrokeMenuWidget::OnButtonShowGrid()
{
    FTimerHandle TH;
    TWeakObjectPtr<UStrokeMenuWidget> WeakThis(this);
    UUtilLibrary::LockButtonForSeconds(GetButtonFromUserWidget(WBP_InGame_Menu_Grid, TEXT("Button_Menu")), GetWorld(), 0.33f);
    UE_LOG(LogGameMode, Log, TEXT(" --- UStrokeMenuWidget::OnButtonShowGrid"));
    GetWorld()->GetTimerManager().SetTimer(TH, [WeakThis]()
        {
            if (WeakThis->GM)
            {
                if (WeakThis->GM->GetCurrentTurnGolfPlayer()->GetPlayerState() == EPlayerState::Player_Ready)
                {
                    WeakThis->bIsOnGird = !WeakThis->bIsOnGird;
                    WeakThis->ApplyButtonStyle(WeakThis->bIsOnGird);

                    if (AGolfPlayerController* PC = Cast<AGolfPlayerController>(UGameplayStatics::GetPlayerController(WeakThis->GetWorld(), 0)))
                    {
                        PC->ToggleTerrainGrid();
                    }
                }
                WeakThis->GM->StrokeMenuWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);

            }
        },
        0.25f, false);
}
// 스코어카드 보기
void UStrokeMenuWidget::OnButtonShowScoreCard()
{
    UUtilLibrary::LockButtonForSeconds(GetButtonFromUserWidget(WBP_InGame_Menu_ScoreCard, TEXT("Button_Menu")), GetWorld(), 0.33f);

    FTimerHandle TH;
    TWeakObjectPtr<UStrokeMenuWidget> WeakThis(this);

    GetWorld()->GetTimerManager().SetTimer(TH, [WeakThis]()
        {
            if (WeakThis->GM)
            {
                WeakThis->GM->StrokeMenuWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
                WeakThis->GM->InGameScoreBoardWidgetInstance->SetVisibility(ESlateVisibility::Visible);
            }
        },
        0.25f, false);
}

//플레이어 추가/삭제
void UStrokeMenuWidget::OnButtonPlayerControl()
{
    UUtilLibrary::LockButtonForSeconds(GetButtonFromUserWidget(WBP_InGame_Menu_PlayerAdd, TEXT("Button_Menu")), GetWorld(), 0.33f);

    FTimerHandle TH;
    TWeakObjectPtr<UStrokeMenuWidget> WeakThis(this);

    GetWorld()->GetTimerManager().SetTimer(TH, [WeakThis]()
        {
            if (WeakThis->GM)
            {
                if (WeakThis->GM->IsStrokeMode())
                    if (WeakThis->GM->GetCurrentGameState() == EGameState::Game_Play)
                        if (WeakThis->GM->GetCurrentTurnGolfPlayer()->GetPlayerState() == EPlayerState::Player_Ready)
                            WeakThis->GM->InGamePlayerSelectWidget->SetVisibility(ESlateVisibility::Visible);
            }
        },
        0.25f, false);


}
//멀리건
void UStrokeMenuWidget::OnButtonMulligan()
{
    UUtilLibrary::LockButtonForSeconds(GetButtonFromUserWidget(Button_mulligan, TEXT("Button_Menu")), GetWorld(), 0.33f);

    FTimerHandle TH;
    TWeakObjectPtr<UStrokeMenuWidget> WeakThis(this);

    GetWorld()->GetTimerManager().SetTimer(TH, [WeakThis]()
        {
            if (WeakThis->GM)
            {
                WeakThis->GM->StrokeMenuWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
                WeakThis->GM->InGamePopupWidgetInstance->UpdatePopupForUseMulligan();
            }
        },
        0.25f, false);

}

//void UStrokeMenuWidget::UseMulliganWrapper()
//{
//    GM->StrokeWidgetInstance->OnMulliganButtonClicked();
//}

//다음홀로이동
void UStrokeMenuWidget::OnButtonNextHole()
{
    UUtilLibrary::LockButtonForSeconds(GetButtonFromUserWidget(WBP_InGame_Menu_NextHole, TEXT("Button_Menu")), GetWorld(), 0.33f);

    FTimerHandle TH;
    TWeakObjectPtr<UStrokeMenuWidget> WeakThis(this);

    GetWorld()->GetTimerManager().SetTimer(TH, [WeakThis]()
        {
            if (WeakThis->GM)
            {
                WeakThis->GM->StrokeMenuWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
                WeakThis->GM->InGamePopupWidgetInstance->UpdatePopupForNextHole();
                WeakThis->GM->HideChanceWidget();
            }
        },
        0.25f, false);

    if (AGolfPlayerController* PC = Cast<AGolfPlayerController>(UGameplayStatics::GetPlayerController(WeakThis->GetWorld(), 0)))
    {
        if (PC->bTerrainGridVisible)
            PC->ToggleTerrainGrid();
    }
}

// 벌타드롭
void UStrokeMenuWidget::OnButtonPaneltyDrop()
{
    UUtilLibrary::LockButtonForSeconds(GetButtonFromUserWidget(WBP_InGame_Menu_PenaltyDrop, TEXT("Button_Menu")), GetWorld(), 0.33f);

    FTimerHandle TH;
    TWeakObjectPtr<UStrokeMenuWidget> WeakThis(this);

    GetWorld()->GetTimerManager().SetTimer(TH, [WeakThis]()
        {
            if (WeakThis->GM)
            {
                WeakThis->GM->StrokeMenuWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
            }
        },
        0.25f, false);

    if (GM)
    {
        AGolfPlayer* Player = GM->GetCurrentTurnGolfPlayer();

        GM->HandlePanelltyDropLogic();

        if (Player)
        {
            int32 CurrentScore = GM->MapInfo.ParScores[GM->CurrentHole - 1] - Player->PlayerInfo.ShotCountPerHole[GM->CurrentHole - 1];

            GM->GetCurrentSlot()->UpdateStroke(Player->PlayerInfo);

            if (GM->GetCurrentTurnGolfPlayer()->CheckChance())
            {
                GM->ShowChanceWidget(CurrentScore);
            }
            GM->HideChanceWidget(); //강제로 없앰
        }

        UE_LOG(LogTemp, Log, TEXT("Menu Button 6 Clicked!"));
    }
}
// 스윙모션 보기
void UStrokeMenuWidget::OnButtonSwingMotion()
{
    OnMenuButtonClicked.Broadcast(7);
    //  UUtilLibrary::LockButtonForSeconds(GetButtonFromUserWidget(WBP_InGame_Menu_Button_6, TEXT("Button_Menu")), GetWorld(), 0.33f);

    FTimerHandle TH;
    TWeakObjectPtr<UStrokeMenuWidget> WeakThis(this);

    GetWorld()->GetTimerManager().SetTimer(TH, [WeakThis]()
        {
            if (WeakThis->GM)
            {
                WeakThis->GM->StrokeMenuWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
            }
        },
        0.25f, false);
    if (GM->GameInfo.GameOptions.SwingMotion)
        GM->ShowSwingMotion(true);
    UE_LOG(LogTemp, Log, TEXT("Menu Button SwingMotion !"));
}
//둘러보기
void UStrokeMenuWidget::OnButtonTourCamera()
{
    UUtilLibrary::LockButtonForSeconds(GetButtonFromUserWidget(WBP_InGame_Menu_Preview, TEXT("Button_Menu")), GetWorld(), 0.33f);

    FTimerHandle TH;
    TWeakObjectPtr<UStrokeMenuWidget> WeakThis(this);

    GetWorld()->GetTimerManager().SetTimer(TH, [WeakThis]()
        {
            if (WeakThis->GM)
            {
                WeakThis->GM->StrokeMenuWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
                WeakThis->GM->HideChanceWidget();
            }
        },
        0.25f, false);

    if (GM->GetCurrentTurnGolfPlayer()->GetPlayerState() == EPlayerState::Player_Ready)
    {
        int32 TargetSublevel = GM->GameInfo.SelectedMap.Sublevel;
        int32 RealIndex = GM->GetPhysicalHoleNum(GM->GameInfo.CurrentHole, TargetSublevel);
        GM->TourActor->StartTourByHoleIndex(RealIndex);
    }
}
//카메라 모드
void UStrokeMenuWidget::OnButtonCameraMode()
{
    //   UUtilLibrary::LockButtonForSeconds(GetButtonFromUserWidget(WBP_InGame_Menu_Button_8, TEXT("Button_Menu")), GetWorld(), 0.33f);

    FTimerHandle TH;
    TWeakObjectPtr<UStrokeMenuWidget> WeakThis(this);

    GetWorld()->GetTimerManager().SetTimer(TH, [WeakThis]()
        {
            if (WeakThis->GM)
            {
                WeakThis->GM->CameraModePopupWidget->SetVisibility(ESlateVisibility::Visible);
            }
        },
        0.25f, false);
}

// 플레이어 순서넘기기
void UStrokeMenuWidget::OnButtonNextPlayer()
{
    UUtilLibrary::LockButtonForSeconds(GetButtonFromUserWidget(WBP_InGame_Menu_SkipTurn, TEXT("Button_Menu")), GetWorld(), 0.33f);

    FTimerHandle TH;
    TWeakObjectPtr<UStrokeMenuWidget> WeakThis(this);

    GetWorld()->GetTimerManager().SetTimer(TH, [WeakThis]()
        {
            if (WeakThis->GM)
            {
                WeakThis->GM->StrokeMenuWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
                WeakThis->GM->InGamePopupWidgetInstance->UpdatePopupForNextPlayer();
                WeakThis->GM->HideChanceWidget();
            }
        },
        0.25f, false);
}
// OK 사용하기
void UStrokeMenuWidget::OnButtonUseOK()
{
    // UUtilLibrary::LockButtonForSeconds(GetButtonFromUserWidget(WBP_InGame_Menu_Button_10, TEXT("Button_Menu")), GetWorld(), 0.33f);

    FTimerHandle TH;
    TWeakObjectPtr<UStrokeMenuWidget> WeakThis(this);

    GetWorld()->GetTimerManager().SetTimer(TH, [WeakThis]()
        {
            if (WeakThis->GM)
            {
                WeakThis->GM->StrokeMenuWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
                WeakThis->GM->GetCurrentTurnGolfPlayer()->UseOK();

            }
        },
        0.25f, false);
}
//라운드 종료
void UStrokeMenuWidget::OnButtonRoundExit()
{
    UUtilLibrary::LockButtonForSeconds(GetButtonFromUserWidget(WBP_InGame_Menu_Button_ExitRound, TEXT("Button_Menu")), GetWorld(), 0.33f);

    FTimerHandle TH;
    TWeakObjectPtr<UStrokeMenuWidget> WeakThis(this);

    GetWorld()->GetTimerManager().SetTimer(TH, [WeakThis]()
        {
            if (WeakThis->GM)
            {
                WeakThis->GM->InGamePopupWidgetInstance->UpdatePopupForEndRound();

            }
        },
        0.25f, false);
}