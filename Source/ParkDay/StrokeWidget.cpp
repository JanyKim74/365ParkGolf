#include "StrokeWidget.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetLayoutLibrary.h" 
#include "Components/CanvasPanel.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Kismet/GameplayStatics.h"
#include "InGameMode.h"
#include "Components/CanvasPanelSlot.h" // UCanvasPanelSlot을 사용하기 위해 필요합니다.
#include "GolfPlayerController.h"
#include "GolfPlayer.h"
#include "Widgets/InGameMenuPopup.h"
#include "GolfPlayerManager.h"
#include "SoundManager.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Utils/LoadTexture2DFromFileAsync.h"
#include "Utils/UtilLibrary.h"
#include "Widgets/DistanceWidget.h"
#include "PuttingGuide.h"  // 최상단 include에 추가 필요
#include "DrawDebugHelpers.h"
#include "ParkDay/TourActor.h"


void UStrokeWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 버튼 이벤트 바인딩
    if (Button_Muligan)
    {
        Button_Muligan->OnClicked.AddDynamic(this, &UStrokeWidget::OnMulliganButtonClicked);
    }

    if (Button_OK)
    {
        Button_OK->OnClicked.AddDynamic(this, &UStrokeWidget::OnOKButtonClicked);
    }

    if (Button_Menu)
    {
        Button_Menu->OnClicked.AddDynamic(this, &UStrokeWidget::OnMenuButtonClicked);
    }

    if (Button_Tour_Stop)
    {
        Button_Tour_Stop->OnClicked.AddDynamic(this, &UStrokeWidget::OnClickedTourStop);
    }

    // 시작 시 숨기기
    if (CanvasPanel_Tip_1)
    {
        CanvasPanel_Tip_1->SetVisibility(ESlateVisibility::Hidden);
    }

    // 시작 시 숨기기
    if (Canvas_PuttingGuid)
    {
        Canvas_PuttingGuid->SetVisibility(ESlateVisibility::Hidden);
    }

    if (CanvasPanel_PlayerTurn)
        HidePlayerTurnCanvasWidget();

    // ✅ NativeConstruct에서는 LoadObject 호출 안 함!
    // Tick에서 안전하게 로드



    UE_LOG(LogTemp, Log, TEXT("✅ StrokeWidget NativeConstruct completed"));
}


// ⭐ 새로운 Tick 함수 - 첫 프레임에만 실행
void UStrokeWidget::NativeTick(const FGeometry& InGeometry, float InDeltaTime)
{
    Super::NativeTick(InGeometry, InDeltaTime);

    // ✅ 한 번만 로드
    if (!bTexturesLoaded)
    {
        LoadLandTypeTextures();
    }
}


void UStrokeWidget::NativeDestruct()
{
    // 위젯 소멸 시 퍼팅 가이드 타이머 반드시 정리
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PuttingGuidanceHideTimer);
    }

    Super::NativeDestruct();
}

void UStrokeWidget::LoadLandTypeTextures()
{
    // ✅ 게임 스레드 체크
    if (!IsInGameThread())
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ LoadLandTypeTextures called from non-game thread, skipping"));
        return;
    }

    // ✅ 이미 로드된 경우 스킵
    if (bTexturesLoaded)
    {
        UE_LOG(LogTemp, Verbose, TEXT("📝 Textures already loaded, skipping"));
        return;
    }

    // ✅ 동기식 로드 (게임 스레드에서 안전)
    const FString BasePath = TEXT("/Game/UMG/Resources/Images/InGame/");

    // 텍스처 로드
    LandTypeTextures.Add(0, LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("tee.tee"))));
    LandTypeTextures.Add(1, LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("rough.rough"))));
    LandTypeTextures.Add(2, LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("green.green"))));
    LandTypeTextures.Add(3, LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("green.green"))));
    LandTypeTextures.Add(4, LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("rough.rough"))));
    LandTypeTextures.Add(5, LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("bunker.bunker"))));
    LandTypeTextures.Add(6, LoadObject<UTexture2D>(nullptr, *(BasePath + TEXT("water.water"))));

    // 이름 로드
    LandTypeNames.Add(0, TEXT("Tee"));
    LandTypeNames.Add(1, TEXT("Grass"));
    LandTypeNames.Add(2, TEXT("Fairway"));
    LandTypeNames.Add(3, TEXT("Green"));
    LandTypeNames.Add(4, TEXT("Rough"));
    LandTypeNames.Add(5, TEXT("Bunker"));
    LandTypeNames.Add(6, TEXT("Water"));

    // ✅ 로딩 완료 표시
    bTexturesLoaded = true;

    // ✅ 결과 로그
    int32 LoadedCount = 0;
    for (const auto& Pair : LandTypeTextures)
    {
        if (Pair.Value)
        {
            LoadedCount++;
        }
    }

    UE_LOG(LogTemp, Log, TEXT("✅ Loaded %d/%d land type textures"), LoadedCount, LandTypeTextures.Num());
}

UTexture2D* UStrokeWidget::GetLandTypeTexture(int32 Type)
{
    // ✅ 텍스처가 로드되지 않았으면 재시도
    if (!bTexturesLoaded)
    {
        LoadLandTypeTextures();
    }

    // ✅ 안전한 조회
    if (UTexture2D* const* TexturePtr = LandTypeTextures.Find(Type))
    {
        return *TexturePtr;
    }

    UE_LOG(LogTemp, Warning, TEXT("⚠️ Land type texture not found: %d"), Type);
    return nullptr;

}

FString UStrokeWidget::GetLandTypeName(int32 Type)
{
    // ✅ 텍스처가 로드되지 않았으면 재시도
    if (!bTexturesLoaded)
    {
        LoadLandTypeTextures();
    }

    // ✅ 안전한 조회
    if (FString* const NamePtr = LandTypeNames.Find(Type))
    {
        return *NamePtr;
    }

    return TEXT("Unknown");
}

void UStrokeWidget::InitializeLandTypeTextures()
{
    // ⭐ BeginPlay에서만 호출 (게임 스레드 보장)
    if (!IsInGameThread())
    {
        return;
    }

    // 동기식 로드 (간단하고 안전)
    const FString BasePath = TEXT("/Game/UMG/Resources/Images/InGame/");

    struct FTextureData
    {
        int32 Type;
        FString Path;
    };

    TArray<FTextureData> TexturesToLoad = {
        {0, BasePath + TEXT("tee")},
        {1, BasePath + TEXT("rough")},
        {2, BasePath + TEXT("green")},
    };

    for (const auto& Data : TexturesToLoad)
    {
        UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *Data.Path);
        if (Texture)
        {
            LandTypeTextureMap.Add(Data.Type, Texture);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("⚠️ Failed to load texture: %s"), *Data.Path);
        }
    }
}

//void UStrokeWidget::SetPositionTip1()
//{
//    AInGameMode* GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
//    APlayerController* PC = GetOwningPlayer();
//    if (!GM || !PC) return;
//
//    const int32 HoleIdx = FMath::Clamp(GM->CurrentHole - 1, 0, GM->GameInfo.SelectedMap.HolecupPositions.Num() - 1);
//    FVector TargetWorld = GM->GameInfo.SelectedMap.HolecupPositions[HoleIdx];
//
//    // 카메라 뒤면 숨기기
//    const FVector CamLoc = PC->PlayerCameraManager->GetCameraLocation();
//    const FVector CamFwd = PC->PlayerCameraManager->GetActorForwardVector();
//    if (FVector::DotProduct(CamFwd, TargetWorld - CamLoc) <= 0.f)
//    {
//        CanvasPanel_Tip_1->SetVisibility(ESlateVisibility::Collapsed);
//        return;
//    }
//
//    FVector2D ScreenPos;
//    const bool bProjected =
//        UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
//            PC, TargetWorld + FVector(0.f, 0.f, 240.f), ScreenPos, /*bPlayerViewportRelative=*/ true);
//
//    if (!bProjected)
//    {
//        CanvasPanel_Tip_1->SetVisibility(ESlateVisibility::Collapsed);
//        return;
//    }
//    else
//    {
//        CanvasPanel_Tip_1->SetVisibility(ESlateVisibility::HitTestInvisible);
//    }
//
//    //UpdateAimInfo(FVector::Dist(TargetWorld, GM->GetCurrentTurnGolfBall()->GetActorLocation()) / 100.f, (TargetWorld.Z - GM->GetCurrentTurnGolfBall()->GetActorLocation().Z) / 100.f);
//
//    if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(CanvasPanel_Tip_1->Slot))
//    {
//        CanvasSlot->SetPosition(ScreenPos);
//    }
//}

void UStrokeWidget::UpdateMapInfo(int32 HoleNumber, int32 Par, float CourseLength)
{
    AInGameMode* GameMode = Cast<AInGameMode>(UGameplayStatics::GetGameMode(GetWorld()));

    // GameMode null 체크 추가
    if (!GameMode || !GameMode->StrokeWidgetInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("GameMode가 null입니다 - UpdateMapInfo에서"));
        return;
    }


    if (TextBlock_CourseInfo_Name)
    {
        TextBlock_CourseInfo_Name->SetText(FText::FromString(GameMode->MapInfo.MapName));
    }
    if (TextBlock_CourseInfo_ParIndex)
    {
        TextBlock_CourseInfo_ParIndex->SetText(FText::FromString(FString::Printf(TEXT("%d"), HoleNumber)));
    }
    if (TextBlock_CourseInfo_ParCount)
    {
        TextBlock_CourseInfo_ParCount->SetText(FText::FromString(FString::Printf(TEXT("%d"), Par)));
    }
    if (TextBlock_CourseInfo_Distance)
    {
        TextBlock_CourseInfo_Distance->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), CourseLength / 100.0f)));
    }
    if (Image_CourseMap)
    {
        // Saved 폴더 기준 경로 생성
        FString ImagePath = FPaths::ProjectSavedDir() / TEXT("CourseMap/") +
            GameMode->GameInfo.SelectedMap.MapName + TEXT("/image1.png");

        FString Err;
        if (UTexture2D* Tex = ULoadTexture2DFromFileAsync::LoadTexture2DFromFileSync(ImagePath, &Err))
        {
            Image_CourseMap->SetBrushFromTexture(Tex);
            UE_LOG(LogTemp, Log, TEXT("✅ Loaded external CourseMap Image: %s"), *ImagePath);
        }
    }
}


void UStrokeWidget::UpdateAimInfo(float Distance, float Height)
{

    PositionCanvasPanelAboveHole();
    //   DrawDebugCanvasPosition();  // 디버그용

    if (TextBlock_Tip1_Distance)
    {
        TextBlock_Tip1_Distance->SetText(FText::FromString(FString::Printf(TEXT("%.1f m"), Distance)));
    }
    if (TextBlock_Tip1_Height)
    {
        TextBlock_Tip1_Height->SetText(FText::FromString(FString::Printf(TEXT("%.2f m"), Height)));
        if (Height == 0)
        {
            TextBlock_Tip1_Height->SetText(FText::FromString(FString::Printf(TEXT("%.0f m"), Height)));
            TextBlock_Tip1_Height->SetColorAndOpacity(FSlateColor(FLinearColor::White));
            return;
        }
        FSlateColor TextColor = Height > 0.f ? FSlateColor(FLinearColor::Red) : FSlateColor(FLinearColor(0, 0.6, 1.0, 1.0));

        TextBlock_Tip1_Height->SetColorAndOpacity(TextColor);
    }
}

void UStrokeWidget::HideAll()
{
    CanvasPanel_Tour->SetVisibility(ESlateVisibility::Visible);
    CanvasPanel_CourseInfo->SetVisibility(ESlateVisibility::Hidden);
    CanvasPanel_Minimap->SetVisibility(ESlateVisibility::Hidden);
    CanvasPanel_Notice->SetVisibility(ESlateVisibility::Hidden);
    CanvasPanel_PlayerTurn->SetVisibility(ESlateVisibility::Hidden);
    ShowAimInfo(false);
    VerticalBox_PlayerList->SetVisibility(ESlateVisibility::Hidden);
    Button_Muligan->SetVisibility(ESlateVisibility::Hidden);
    Button_OK->SetVisibility(ESlateVisibility::Hidden);
    Overlay_BallInfo->SetVisibility(ESlateVisibility::Hidden);
    WBP_Distance->SetVisibility(ESlateVisibility::Hidden);
    ShowPuttingGuidancePanel(false);
    Image_BG->SetVisibility(ESlateVisibility::Hidden);
}

void UStrokeWidget::ShowAll()
{
    CanvasPanel_Tour->SetVisibility(ESlateVisibility::Hidden);
    CanvasPanel_CourseInfo->SetVisibility(ESlateVisibility::Visible);
    CanvasPanel_Minimap->SetVisibility(ESlateVisibility::Visible);
    CanvasPanel_Notice->SetVisibility(ESlateVisibility::Visible);
    CanvasPanel_PlayerTurn->SetVisibility(ESlateVisibility::Visible);
    ShowAimInfo(true);
    VerticalBox_PlayerList->SetVisibility(ESlateVisibility::Visible);
    Button_Muligan->SetVisibility(ESlateVisibility::Visible);
    Button_OK->SetVisibility(ESlateVisibility::Visible);
    Overlay_BallInfo->SetVisibility(ESlateVisibility::Visible);
    WBP_Distance->SetVisibility(ESlateVisibility::Visible);
    Image_BG->SetVisibility(ESlateVisibility::Visible);
}


void UStrokeWidget::HideUI()
{

    CanvasPanel_Minimap->SetVisibility(ESlateVisibility::Hidden);
    CanvasPanel_Notice->SetVisibility(ESlateVisibility::Hidden);
    CanvasPanel_PlayerTurn->SetVisibility(ESlateVisibility::Hidden);
    ShowAimInfo(false);
    Image_BG->SetVisibility(ESlateVisibility::Hidden);

    Button_Muligan->SetVisibility(ESlateVisibility::Hidden);
    Button_OK->SetVisibility(ESlateVisibility::Hidden);
    Overlay_BallInfo->SetVisibility(ESlateVisibility::Hidden);
    WBP_Distance->SetVisibility(ESlateVisibility::Hidden);
    ShowPuttingGuidancePanel(false);
}

void UStrokeWidget::ShowUI()
{

    CanvasPanel_Minimap->SetVisibility(ESlateVisibility::Visible);
    CanvasPanel_Notice->SetVisibility(ESlateVisibility::Visible);
    CanvasPanel_PlayerTurn->SetVisibility(ESlateVisibility::Visible);
    ShowAimInfo(true);

    Button_Muligan->SetVisibility(ESlateVisibility::Visible);
    Button_OK->SetVisibility(ESlateVisibility::Visible);
    Overlay_BallInfo->SetVisibility(ESlateVisibility::Visible);
    WBP_Distance->SetVisibility(ESlateVisibility::Visible);
    Image_BG->SetVisibility(ESlateVisibility::Visible);
}

void UStrokeWidget::ShowAimInfo(bool bVisible)
{
    if (!CanvasPanel_Tip_1) return;

    if (!bVisible)
    {
        // 숨김은 즉시 처리 + 대기 중인 표시 타이머가 있으면 취소
        if (GetWorld())
        {
            GetWorld()->GetTimerManager().ClearTimer(AimInfoShowTimer);
        }
        CanvasPanel_Tip_1->SetVisibility(ESlateVisibility::Collapsed);
        HideAllChildren(CanvasPanel_Tip_1);
        return;
    }

    // 표시는 1초 딜레이 후 처리
    if (GetWorld())
    {
        // 이미 대기 중인 타이머가 있으면 중복 실행 방지를 위해 클리어 후 재등록
        GetWorld()->GetTimerManager().ClearTimer(AimInfoShowTimer);
        GetWorld()->GetTimerManager().SetTimer(
            AimInfoShowTimer,
            [this]()
            {
                if (!IsValid(this) || !CanvasPanel_Tip_1) return;
                CanvasPanel_Tip_1->SetVisibility(ESlateVisibility::Visible);
                ShowAllChildren(CanvasPanel_Tip_1);
            },
            2.0f,   // 1초 딜레이
            false   // 반복 없음
        );
    }
}


void UStrokeWidget::OnOKButtonClicked()
{
    if (USoundManager* SM = GetGameInstance()->GetSubsystem<USoundManager>())
    {
        SM->Play2D_ById(TEXT("Effect.UI.Click"));
    }
    if (AGolfPlayerController* PC = Cast<AGolfPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
    {
        PC->ToggleTerrainGrid();
    }
}


void UStrokeWidget::SetPercentText(float Percent)
{
    if (Percent == 0.0f)
        TextBlock_Percent->SetVisibility(ESlateVisibility::Collapsed);
    else
    {
        TextBlock_Percent->SetText(FText::FromString(FString::Printf(TEXT("%.0f%%"), Percent)));
        TextBlock_Percent->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
}

void UStrokeWidget::UpdateShotBallSpeedAndAngle()
{
    WBP_Distance->UpdateSensorTextData();
}

void UStrokeWidget::OnMulliganButtonClicked()
{
    if (AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
    {
        if (GameMode->IsTrainingMode())
            return;

        int32 LastShotPlayerSlotIndex = GameMode->LatestShotSlotIndex;

        UE_LOG(LogTemp, Log, TEXT("OnMulliganButtonClicked() --- LastShotPlayerSlotIndex - %d"), LastShotPlayerSlotIndex);

        AGolfPlayer* Player = GameMode->FindPlayerSlotIndex(LastShotPlayerSlotIndex);
        AGolfBall* Ball = Player ? GameMode->FindBall(Player->PlayerIndex) : nullptr;
        if (!Player || !Ball)
        {
            UE_LOG(LogTemp, Error, TEXT("OnMulliganButtonClicked() --- Invalid player/ball for slot index %d"), LastShotPlayerSlotIndex);
            return;
        }

        int32 CurrentHoleNumber = GameMode->CurrentHole - 1;
        int32 PlayerShotCountCurrentHole = Player->PlayerInfo.ShotCountPerHole[CurrentHoleNumber];
        int32 PlayerUseMulliganCount = Player->PlayerInfo.MulliganCount;
        int32 MulliganUseLimit = GameMode->GameInfo.GameOptions.Mulligan_Count;
        int32 LatestUseMulliganPlayerIndex = GameMode->GameInfo.LatestUseMulliganPlayerIndex;

        // 현제 플레이어 티샷 조건제거
        if (!Player->EnableMulligan())
        {
            GameMode->InGamePopupWidgetInstance->ChangeDescription(FText::FromString(TEXT("한홀에 한번만 사용가능합니다.")));
            GameMode->InGamePopupWidgetInstance->OnClickedPopupConfirmDele.Unbind();
        }
        else if (MulliganUseLimit != -1 && MulliganUseLimit - PlayerUseMulliganCount <= 0)
        {
            GameMode->InGamePopupWidgetInstance->ChangeDescription(FText::FromString(TEXT("사용 횟수가 부족합니다.")));
            GameMode->InGamePopupWidgetInstance->OnClickedPopupConfirmDele.Unbind();
        }
        else if (GameMode->GetCurrentTurnGolfPlayer()->GetPlayerState() != EPlayerState::Player_Ready)
        {
            GameMode->InGamePopupWidgetInstance->ChangeDescription(FText::FromString(TEXT("지금은 멀리건 사용이 불가능합니다.")));
            GameMode->InGamePopupWidgetInstance->OnClickedPopupConfirmDele.Unbind();
        }
        else if (Player->IsHoleIn() || Ball->IsHoleIn() || Ball->IsConceded())
        {
            GameMode->InGamePopupWidgetInstance->ChangeDescription(FText::FromString(TEXT("홀 인 플레이어 멀리건 사용 불가능")));
            GameMode->InGamePopupWidgetInstance->OnClickedPopupConfirmDele.Unbind();
        }
        else if (Ball->CheckTeeShot())
        {
            GameMode->InGamePopupWidgetInstance->ChangeDescription(FText::FromString(TEXT("티 샷 멀리건 사용 불가")));
            GameMode->InGamePopupWidgetInstance->OnClickedPopupConfirmDele.Unbind();
        }
        else if (Player->bIsPendingDelete)
        {
            GameMode->InGamePopupWidgetInstance->ChangeDescription(FText::FromString(TEXT("삭제 된 플레이어 사용 불가")));
            GameMode->InGamePopupWidgetInstance->OnClickedPopupConfirmDele.Unbind();
        }
        else
        {
            GameMode->InGamePopupWidgetInstance->ChangeDescription(FText::FromString(TEXT("멀리건을 사용 하시겠습니까?")));
            GameMode->InGamePopupWidgetInstance->OnClickedPopupConfirmDele.BindUObject(this, &UStrokeWidget::UseMulliganWrapper);
            if (auto* SM = GetGameInstance()->GetSubsystem<USoundManager>())
            {
                SM->Play2D_ById("Voice.Q.UseMulligan");
            }
        }

        GameMode->ShowInGameMenuPopup();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("GameMode is null"));
    }
}

void UStrokeWidget::UseOKWrapper()
{
    if (AGolfPlayer* Player = Cast<AInGameMode>(GetWorld()->GetAuthGameMode())->PlayerManager->LatestShotPlayer)
    {
        if (auto* SM = GetGameInstance()->GetSubsystem<USoundManager>())
        {
            SM->Play2D_ById("Voice.Q.UseOK");
        }
    }
    else
        UE_LOG(LogTemp, Error, TEXT("Player Is Null"));
}

static FSlateBrush MakeBrush(UObject* Res, FVector2D Size)
{
    FSlateBrush B;
    B.SetResourceObject(Res);
    B.ImageSize = Size;
    B.DrawAs = ESlateBrushDrawType::Image; // 또는 Box
    return B;
}

void UStrokeWidget::UpdateMulliganTexture()
{
    if (AInGameMode* GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
    {
        if (GM->LatestShotSlotIndex == 0)
        {
            GM->LatestShotSlotIndex = GM->PlayerManager->GetPlayers()[0]->SlotIndex;
        }
        if (AGolfPlayer* Player = GM->FindPlayerSlotIndex(GM->LatestShotSlotIndex))
        {
            int32 RemainMulliganCount = GM->GameInfo.GameOptions.Mulligan_Count - Player->PlayerInfo.MulliganCount;

            if (GM->GameInfo.GameOptions.Mulligan_Count == -1)
                RemainMulliganCount = -1;

            if (GM->MulliganTextureMap.Contains(RemainMulliganCount))
            {
                UTexture2D* MulliganCountTexture = GM->MulliganTextureMap[RemainMulliganCount];
                FButtonStyle BS = Button_Muligan->GetStyle();

                BS.SetNormal(MakeBrush(MulliganCountTexture, BS.Normal.GetImageSize()));
                BS.SetHovered(MakeBrush(MulliganCountTexture, BS.Hovered.GetImageSize()));
                BS.SetPressed(MakeBrush(MulliganCountTexture, BS.Pressed.GetImageSize()));
                BS.SetDisabled(MakeBrush(MulliganCountTexture, BS.Disabled.GetImageSize()));
                BS.Disabled.TintColor = FSlateColor(FLinearColor(0.2f, 0.2f, 0.2f, 1.0f));
                Button_Muligan->SetStyle(BS);
            }
        }
    }
}

void UStrokeWidget::UseMulliganWrapper()
{
    if (AGolfPlayer* Player = Cast<AGolfPlayerController>(GetWorld()->GetFirstPlayerController())->GetCurrentGolfPlayer())
    {
        Player->UseMulligan();
    }
    else
        UE_LOG(LogTemp, Error, TEXT("Player Is Null"));
}

void UStrokeWidget::OnMenuButtonClicked()
{
    // 메뉴 버튼 클릭 시 로직 (예: 메뉴 화면 표시)
    UE_LOG(LogTemp, Log, TEXT("Menu Button Clicked"));
    // 초기에는 메뉴를 숨김
   // ShowStrokeMenu();
    if (USoundManager* SM = GetGameInstance()->GetSubsystem<USoundManager>())
    {
        SM->Play2D_ById(TEXT("Effect.UI.Click"));
    }

    if (AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
        GameMode->ShowStrokeMenu();

}

// StrokeWidget.cpp
void UStrokeWidget::ShowAimInfoAtLocation(FVector2D ScreenPosition, bool bVisible)
{
    if (CanvasPanel_Tip_1)
    {
        UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(CanvasPanel_Tip_1->Slot);
        if (CanvasSlot)
        {
            FVector2D WidgetSize = CanvasPanel_Tip_1->GetDesiredSize(); // 위젯의 예상 크기

            // ⭐ 핵심 수정 부분: UMG 디자이너에서 피벗을 (0.5, 0.5)로 설정했다고 가정한 코드
            // ScreenPosition은 위젯의 "중앙"이 될 위치를 나타냅니다.
            // CanvasPanelSlot::SetPosition은 위젯의 "좌상단" 위치를 요구하므로,
            // WidgetSize의 절반을 빼서 좌상단 위치를 계산합니다.
            FVector2D FinalPosition = ScreenPosition + (WidgetSize * 2.0f);
            FinalPosition.X -= 20;
            FinalPosition.Y -= 50;

            CanvasSlot->SetPosition(FinalPosition);

            //CanvasPanel_Tip_1->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
           // ShowAllChildren(CanvasPanel_Tip_1);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("❌ CanvasPanel_Tip_1's slot is not a UCanvasPanelSlot. Check UMG structure."));
        }
    }
}

void UStrokeWidget::SetLandType(int32 nType)
{
    // UE_LOG(LogTemp, Log, TEXT("UStrokeWidget - SetLandType - [%d]"), nType);

     // ✅ 텍스처 조회
    UTexture2D* Texture = GetLandTypeTexture(nType);
    FString TypeName = GetLandTypeName(nType);

    if (Image_BallLocation_1 && Texture)
    {
        Image_BallLocation_1->SetBrushFromTexture(Texture, true);
        // UE_LOG(LogTemp, Log, TEXT("✅ Land type %d (%s) applied"), nType, *TypeName);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Cannot set land type %d - Image or Texture is null"), nType);
    }
}

// 위젯을 3초 후 숨기도록 설정
void UStrokeWidget::ShowCanvasAndHideAfterDelay(const FString& ActorName)
{
    UTexture2D* LoadedTexture = nullptr;
    float RemainingDistance = 0.0f; // 남은 거리를 담을 변수
    float RemainingAimDist = 0.0f; // 에임 거리를 담을 변수
    float fAimHeight = 0.0f;
    //이미지 가져오기
    if (TurnCards.Num() > 0)
    {
        if (AInGameMode* GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
        {
            int32 CurrentTurnIndex = GM->GetCurrentTurnGolfPlayer()->PlayerIndex;
            if (TurnCards.IsValidIndex(CurrentTurnIndex))
                LoadedTexture = TurnCards[CurrentTurnIndex].LoadSynchronous();

            // TODO: 플레이어 객체나 게임모드에서 실제 남은 거리 값을 가져오세요.

                // 현재 볼 위치 가져오기
            TArray<AGolfBall*> PlayerBalls = GM->PlayerManager->GetPlayerBalls();
            if (!PlayerBalls.IsValidIndex(GM->CurrentPlayerIndex))
                return;

            AGolfBall* CurrentBall = PlayerBalls[GM->CurrentPlayerIndex];

            RemainingDistance = FVector::Dist(CurrentBall->GetActorLocation(), GM->MapInfo.HolecupPositions[GM->CurrentHole]);
            // 일단은 예시 값으로 처리하겠습니다.
            AGolfPlayerController* PC = Cast<AGolfPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
            RemainingAimDist = FVector::Dist(CurrentBall->GetActorLocation(), PC->GetAimActor()->GetActorLocation());
            fAimHeight = CurrentBall->GetActorLocation().Z - PC->GetAimActor()->GetActorLocation().Z;
        }
    }
    if (CanvasPanel_PlayerTurn)
    {
        UE_LOG(LogTemp, Log, TEXT("ShowCanvasAndHideAfterDelay -   "));

        if (auto* SM = GetGameInstance()->GetSubsystem<USoundManager>())
        {
            auto* GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());

            SM->Play2D_ById("Effect.Turn");

            FTimerHandle H;
            GetWorld()->GetTimerManager().SetTimer(H, [this, SM, GM]()
                {
                    SM->PlayTTS_Turn_ById("Voice.Turn", GM->GetCurrentTurnGolfPlayer()->PlayerIndex);
                }, 1.0f, false);
        }
        if (LoadedTexture)
            Image_Player_number->SetBrushFromTexture(LoadedTexture);
        // --- 거리 텍스트 출력 부분 추가 ---
        if (TextBlock_PinDistance)
        {
            // FString::Printf를 사용하여 소수점 자리수나 단위를 포맷팅합니다.
            // 예: "123.4m"
            FString DistanceStr = FString::Printf(TEXT("%.1fm"), RemainingDistance * 0.01f);
            TextBlock_PinDistance->SetText(FText::FromString(DistanceStr));
        }
        // --------------------------------
        CanvasPanel_PlayerTurn->SetVisibility(ESlateVisibility::Visible);
        ShowAllChildren(CanvasPanel_PlayerTurn);
        turn_Name->SetText(FText::FromString(ActorName));
        GetWorld()->GetTimerManager().SetTimer(WidgetHideTimer, this, &UStrokeWidget::HidePlayerTurnCanvasWidget, 1.7f, false);
    }

}

void UStrokeWidget::HidePlayerTurnCanvasWidget()
{
    UE_LOG(LogTemp, Log, TEXT("HideCanvasWidget -   "));
    if (CanvasPanel_PlayerTurn)
    {
        CanvasPanel_PlayerTurn->SetVisibility(ESlateVisibility::Collapsed);  // 또는 RemoveFromParent()
        HideAllChildren(CanvasPanel_PlayerTurn);
    }
}


void UStrokeWidget::HideAllChildren(UCanvasPanel* Canvas)
{
    if (!Canvas) return;
    UE_LOG(LogTemp, Log, TEXT("HideAllChildren -   %s"), *Canvas->GetName());
    int32 ChildCount = Canvas->GetChildrenCount();
    for (int32 i = 0; i < ChildCount; ++i)
    {
        UWidget* ChildWidget = Canvas->GetChildAt(i);
        if (ChildWidget)
        {
            ChildWidget->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}


void UStrokeWidget::ShowAllChildren(UCanvasPanel* Canvas)
{
    if (!Canvas) return;

    UE_LOG(LogTemp, Log, TEXT("ShowAllChildren -    %s"), *Canvas->GetName());
    int32 ChildCount = Canvas->GetChildrenCount();
    for (int32 i = 0; i < ChildCount; ++i)
    {
        UWidget* ChildWidget = Canvas->GetChildAt(i);
        if (ChildWidget)
        {
            ChildWidget->SetVisibility(ESlateVisibility::Visible); // 또는 SelfHitTestInvisible
        }
    }
}


void UStrokeWidget::PositionCanvasPanelAboveHole()
{
    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    // GameMode, Player 유효성 체크 ... (기존 코드 유지)
    if (!GameMode || !GameMode->PlayerManager) return;
    AGolfPlayer* Player = GameMode->PlayerManager->GetPlayers()[GameMode->CurrentPlayerIndex];
    if (Player->GetPlayerState() != EPlayerState::Player_Ready) return;

    // 위젯 유효성 체크
    if (!IsValid(CanvasPanel_Tip_1)) return;
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!IsValid(PlayerController)) return;

    // 1. 홀컵 월드 좌표 계산
    int32 HoleIndex = GameMode->CurrentHole - 1;
    if (!GameMode->MapInfo.HolecupPositions.IsValidIndex(HoleIndex))
    {
        CanvasPanel_Tip_1->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    FVector HolecupWorldPos = GameMode->MapInfo.HolecupPositions[HoleIndex];
    FVector TargetWorldPos = HolecupWorldPos + FVector(0.f, 0.f, HolecupHeightOffset);

    // 2. 카메라 후방 체크 (Dot Product)
    FVector CameraLoc = PlayerController->PlayerCameraManager->GetCameraLocation();
    FVector ToHole = (TargetWorldPos - CameraLoc).GetSafeNormal();
    FVector CameraForward = PlayerController->PlayerCameraManager->GetActorForwardVector();

    // 내적값이 0보다 작으면(카메라 뒤) 숨김
    if (FVector::DotProduct(CameraForward, ToHole) <= 0.f)
    {
        CanvasPanel_Tip_1->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    // 3. 월드 좌표 -> 위젯 스크린 좌표 변환 (핵심)
    FVector2D ScreenPos;
    bool bProjected = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
        PlayerController,
        TargetWorldPos,
        ScreenPos,
        true // bPlayerViewportRelative: 뷰포트 기준 좌표 (DPI 스케일링 포함)
    );

    if (!bProjected)
    {
        CanvasPanel_Tip_1->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    // 4. Canvas Slot 위치 설정
    if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(CanvasPanel_Tip_1->Slot))
    {
        // 위젯의 현재 크기 가져오기
        FVector2D WidgetSize = CanvasPanel_Tip_1->GetDesiredSize();

        // [수정] 스크린 좌표(ScreenPos)는 홀컵의 중앙점입니다.
        // 위젯의 정중앙이 홀컵에 오게 하려면, 위젯 크기의 절반만큼 왼쪽/위로 이동해야 합니다.
        FVector2D FinalPosition = ScreenPos;

        // X축: 위젯 절반만큼 왼쪽으로 이동 (정중앙 정렬)
        FinalPosition.X -= (WidgetSize.X * 0.1f);

        FinalPosition.X -= 122;

        // Y축: 위젯 높이만큼 위로 이동 (홀컵 위에 얹기)
        FinalPosition.Y -= WidgetSize.Y;

        // 추가 오프셋 (헤더의 CanvasPanelOffset 무시하고 직접 조정 권장)
        // 예: 홀컵보다 50픽셀 더 위로 띄우기
        FinalPosition.Y -= 50.0f;

        if (FinalPosition.Y < 90.0f) FinalPosition.Y = 90.0f;

        // 3. 최종 적용
        CanvasSlot->SetPosition(FinalPosition);

        // 위치가 튀는 것을 방지하기 위해 사이즈가 유효할 때만 보이기
        if (WidgetSize.X > 1.0f)
        {
            CanvasPanel_Tip_1->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
    }
}

void UStrokeWidget::DrawDebugCanvasPosition()
{
    if (!IsValid(CanvasPanel_Tip_1))
        return;

    if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(CanvasPanel_Tip_1->Slot))
    {
        FVector2D CurrentPos = CanvasSlot->GetPosition();
        FVector2D WidgetSize = CanvasPanel_Tip_1->GetDesiredSize();

        FString DebugMsg = FString::Printf(
            TEXT("📍 CanvasPanel_Tip_1\n")
            TEXT("  Position: (%.0f, %.0f)\n")
            TEXT("  Size: (%.0f, %.0f)\n")
            TEXT("  Pivot: (%.1f, %.1f)\n")
            TEXT("  Right Edge: %.0f\n")
            TEXT("  Bottom Edge: %.0f"),
            CurrentPos.X, CurrentPos.Y,
            WidgetSize.X, WidgetSize.Y,
            CanvasPanelPivot.X, CanvasPanelPivot.Y,
            CurrentPos.X + WidgetSize.X,
            CurrentPos.Y + WidgetSize.Y
        );

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 0.016f, FColor::Yellow, DebugMsg);
        }
    }
}

void UStrokeWidget::DisplayPuttingGuidance()
{
    /**
  * PuttingGuide 분석을 실행하고 결과를 UI에 표시합니다.
  */

  // 1. PuttingGuide 액터 찾기
    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    APuttingGuide* PuttingGuide = GameMode ? GameMode->PuttingGuideActor : nullptr;


    if (!PuttingGuide)
    {
        UE_LOG(LogTemp, Warning, TEXT("[StrokeWidget] APuttingGuide actor not found"));
        return;
    }

    // 2. Game Mode에서 볼과 홀 정보 획득

    if (!GameMode)
    {
        UE_LOG(LogTemp, Warning, TEXT("[StrokeWidget] AInGameMode not found"));
        return;
    }

    // ⭐ 중요: 아래 코드는 당신의 Game Mode 구조에 맞게 수정해야 합니다.
    // Game Mode에서 위치 정보를 제공하는 함수가 있는지 확인하세요.
    // 예시:
    // FVector BallLocation = GameMode->GetCurrentBallLocation();
    // FVector HoleLocation = GameMode->GetCurrentHoleLocation();

    // 임시: 더미 데이터 (실제로는 GameMode에서 가져와야 함)
    FVector BallLocation = GameMode->GetCurrentTurnGolfBall()->GetActorLocation();
    FVector HoleLocation = GameMode->MapInfo.HolecupPositions[GameMode->CurrentHole - 1];
    float fDist = FVector::Dist(BallLocation, HoleLocation);
    // 3. 퍼팅 분석 실행
    FPuttingGuideResult Result = PuttingGuide->AnalyzePutting(
        BallLocation,
        HoleLocation,
        50.0f,   // 1m 간격
        fDist   // 최대 10m
    );

    // 4. 텍스트 업데이트 

    UpdatePuttingGuidanceText(Result.PuttingGuidanceText);

    // 5. 패널 표시
    ShowPuttingGuidancePanel(false);

    // 6. 위치 설정
    PositionPuttingGuidancePanelAtHole(HoleLocation, Result);

    UE_LOG(LogTemp, Warning, TEXT("[StrokeWidget] Putting Guidance: %s"),
        *Result.PuttingGuidanceText);

}


void UStrokeWidget::UpdatePuttingGuidanceText(const FString& GuidanceText)
{
    /**
  * TextBlock_Guid에 텍스트를 설정합니다.
  */

    if (!TextBlock_Guid)
    {
        UE_LOG(LogTemp, Warning, TEXT("[StrokeWidget] TextBlock_Guid is not bound"));
        return;
    }

    // 텍스트 설정
    TextBlock_Guid->SetText(FText::FromString(GuidanceText));

    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    FString EndAnnouncement = GuidanceText + TEXT(" 보고 치세요");


    // 색상 설정 (방향에 따라)
    if (GuidanceText.Contains(TEXT("LEFT")))
    {
        EndAnnouncement = GuidanceText + TEXT(" 보고 치세요");
        TextBlock_Guid->SetColorAndOpacity(FLinearColor::Yellow);
    }
    else if (GuidanceText.Contains(TEXT("RIGHT")))
    {
        EndAnnouncement = GuidanceText + TEXT(" 보고 치세요");
        TextBlock_Guid->SetColorAndOpacity(FLinearColor::Yellow);
    }
    else if (GuidanceText.Contains(TEXT("STRAIGHT")))
    {
        EndAnnouncement = TEXT(" 똑바로 보고 치세요");
        TextBlock_Guid->SetColorAndOpacity(FLinearColor::Yellow);
    }

    GameMode->Speak(EndAnnouncement);

    UE_LOG(LogTemp, Log, TEXT("[StrokeWidget] Updated TextBlock_Guid: %s"), *GuidanceText);
}

void UStrokeWidget::PositionPuttingGuidancePanel(FVector2D ScreenPosition)
{
    /**
     * Canvas_PuttingGuid 패널을 화면의 특정 위치에 배치합니다.
     */

    if (!Canvas_PuttingGuid)
    {
        UE_LOG(LogTemp, Warning, TEXT("[StrokeWidget] Canvas_PuttingGuid is not bound"));
        return;
    }

    // Canvas Panel의 슬롯 가져오기
    UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Canvas_PuttingGuid->Slot);

    if (CanvasSlot)
    {
        // 위치 설정
        CanvasSlot->SetPosition(ScreenPosition);
        CanvasSlot->SetOffsets(FMargin(ScreenPosition.X, ScreenPosition.Y, 0, 0));

        UE_LOG(LogTemp, Log, TEXT("[StrokeWidget] Canvas_PuttingGuid positioned at (%.0f, %.0f)"),
            ScreenPosition.X, ScreenPosition.Y);
    }
}

void UStrokeWidget::ShowPuttingGuidancePanel(bool bShow)
{
    /**
     * Canvas_PuttingGuid 패널을 표시하거나 숨깁니다.
     */

    if (!Canvas_PuttingGuid)
    {
        UE_LOG(LogTemp, Warning, TEXT("[StrokeWidget] Canvas_PuttingGuid is not bound"));
        return;
    }

    if (bShow)
    {
        Canvas_PuttingGuid->SetVisibility(ESlateVisibility::Visible);
        UE_LOG(LogTemp, Log, TEXT("[StrokeWidget] Canvas_PuttingGuid shown"));
    }
    else
    {
        Canvas_PuttingGuid->SetVisibility(ESlateVisibility::Hidden);
        UE_LOG(LogTemp, Log, TEXT("[StrokeWidget] Canvas_PuttingGuid hidden"));
    }
}

void UStrokeWidget::DisplayPuttingGuidanceWithAutoHide(float HideDuration)
{
    /**
       * PuttingGuide를 표시하고 지정된 시간 후 자동으로 숨깁니다.
       *
       * @param HideDuration 숨길 때까지의 시간 (초)
       */

       // 1. 분석 및 표시
    DisplayPuttingGuidance();

    // 2. 기존 타이머가 있으면 취소
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(PuttingGuidanceHideTimer);

        TWeakObjectPtr<UStrokeWidget> WeakThis(this);
        GetWorld()->GetTimerManager().SetTimer(
            PuttingGuidanceHideTimer,
            [WeakThis]()
            {
                if (WeakThis.IsValid())
                {
                    WeakThis->ShowPuttingGuidancePanel(false);
                }
            },
            HideDuration,
            false
        );

        UE_LOG(LogTemp, Log, TEXT("[StrokeWidget] DisplayPuttingGuidanceWithAutoHide: %.1f seconds"),
            HideDuration);
    }
}

void UStrokeWidget::PositionPuttingGuidancePanelAtHole(
    FVector HoleWorldLocation,
    const FPuttingGuideResult& Result)
{
    /**
     * ✅ 개선된 패널 배치 로직
     *
     * 기존: 홀컵 좌표를 투영 후 픽셀 오프셋 계산 (부정확)
     * 변경: Result.GuidanceWorldPosition (컵 단위 오프셋이 적용된 월드 좌표)을
     *       직접 2D 스크린 좌표로 변환하여 패널 배치
     *
     * - 직진:  홀컵 위치 그대로
     * - 좌/우: 홀컵에서 공->홀 직각 방향으로 이동한 좌표
     *          반컵=10cm, 한컵=20cm, 두컵=40cm, 세컵=60cm, 네컵=80cm
     */

    if (!Canvas_PuttingGuid)
    {
        UE_LOG(LogTemp, Warning, TEXT("[StrokeWidget] Canvas_PuttingGuid is not valid"));
        return;
    }

    APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
    if (!PlayerController || !PlayerController->PlayerCameraManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("[StrokeWidget] PlayerController not found"));
        return;
    }

    // Step 1: 카메라 뒤에 있는지 체크
    FVector CameraLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
    FRotator CameraRot = PlayerController->PlayerCameraManager->GetCameraRotation();
    FVector CameraForward = FRotationMatrix(CameraRot).GetScaledAxis(EAxis::X);

    // GuidanceWorldPosition이 유효한지 확인 (ZeroVector면 홀컵 사용)
    FVector TargetWorldPos = Result.GuidanceWorldPosition.IsNearlyZero()
        ? HoleWorldLocation
        : Result.GuidanceWorldPosition;

    float ForwardDist = FVector::DotProduct(TargetWorldPos - CameraLocation, CameraForward);
    if (ForwardDist <= 0.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("[StrokeWidget] GuidanceWorldPosition is behind the camera!"));
        Canvas_PuttingGuid->SetVisibility(ESlateVisibility::Hidden);
        return;
    }

    // Step 2: GuidanceWorldPosition을 2D 스크린 좌표로 변환
    //   ProjectWorldLocationToWidgetPosition: DPI 스케일 자동 보정된 슬레이트 좌표 반환
    FVector2D ScreenPosition;
    bool bProjected = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
        PlayerController,
        TargetWorldPos,
        ScreenPosition,
        true  // bPlayerViewportRelative
    );

    if (!bProjected)
    {
        UE_LOG(LogTemp, Warning, TEXT("[StrokeWidget] ProjectWorldLocationToWidgetPosition failed"));
        Canvas_PuttingGuid->SetVisibility(ESlateVisibility::Hidden);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[StrokeWidget] GuidanceWorldPosition projected: (%.2f, %.2f) | Offset=%.0fcm | Dir=%s"),
        ScreenPosition.X, ScreenPosition.Y,
        Result.GuidanceOffsetCM,
        *Result.Direction);

    // Step 3: Viewport 크기 (DPI 보정)
    int32 ViewportX, ViewportY;
    PlayerController->GetViewportSize(ViewportX, ViewportY);
    float DPIScale = UWidgetLayoutLibrary::GetViewportScale(GetWorld());
    ViewportSize.X = (float)ViewportX / DPIScale;
    ViewportSize.Y = (float)ViewportY / DPIScale;

    // Step 4: 패널 중앙 정렬 (앵커 좌상단 기준)
    FVector2D AdjustedScreenPosition;
    AdjustedScreenPosition.X = ScreenPosition.X - (PuttingGuidancePanelSize.X * 0.5f);
    AdjustedScreenPosition.Y = ScreenPosition.Y - (PuttingGuidancePanelSize.Y * 0.5f) + HolecupUIOffsetY;

    // Step 5: 화면 경계 클램핑
    AdjustedScreenPosition.X = FMath::Clamp(AdjustedScreenPosition.X, 0.0f, ViewportSize.X - PuttingGuidancePanelSize.X);
    AdjustedScreenPosition.Y = FMath::Clamp(AdjustedScreenPosition.Y, 0.0f, ViewportSize.Y - PuttingGuidancePanelSize.Y);

    UE_LOG(LogTemp, Log, TEXT("[StrokeWidget] Final Position: (%.0f, %.0f) | Viewport: (%.0f, %.0f)"),
        AdjustedScreenPosition.X, AdjustedScreenPosition.Y,
        ViewportSize.X, ViewportSize.Y);

    // Step 6: 패널 위치 설정
    PositionPuttingGuidancePanel(AdjustedScreenPosition);

    UE_LOG(LogTemp, Log, TEXT("[StrokeWidget] ===== Panel Positioned Successfully ====="));
}
float UStrokeWidget::CalculateDistanceScale(float DistanceMeter) const
{
    /**
     * 거리에 따른 스케일 계산
     *
     * 거리 1m: 1.0 (100%)
     * 거리 2m: 0.7 (70%)
     * 거리 3m: 0.5 (50%)
     * 거리 4m: 0.4 (40%)
     * 거리 5m: 0.3 (30%)
     * 거리 10m: 0.1 (10%)
     *
     * 공식: Scale = 1.0 / (1.0 + Distance)
     */

     // 최소값 1m (가까운 거리)
    float ClampedDistance = FMath::Max(DistanceMeter, 1.0f);

    // Scale = 1.0 / (1.0 + ClampedDistance)
    float Scale = 1.0f / (1.0f + ClampedDistance);

    // 범위 제한: 0.1 ~ 1.0
    Scale = FMath::Clamp(Scale, 0.1f, 1.0f);
    Scale *= 5.0f;

    return Scale;
}

void UStrokeWidget::OnClickedTourStop()
{
    UUtilLibrary::LockButtonForSeconds(Button_Tour_Stop, GetWorld(), 0.25f);
    if (AInGameMode* GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
    {
        UUtilLibrary::FadeIn(GetWorld(), 0.5f, FFadeCallback::CreateLambda([this, GM]
            {
                GM->TourActor->StopTourAndRestoreNow();
                UUtilLibrary::FadeOut(GetWorld(), 0.5f);
            }
        ));
    }
}