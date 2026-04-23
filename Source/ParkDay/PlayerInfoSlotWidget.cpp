#include "PlayerInfoSlotWidget.h"

#include "GolfBall.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"
#include "InGameMode.h"
#include "GolfPlayer.h"
#include "GolfPlayerManager.h"

bool UPlayerInfoSlotWidget::Initialize()
{
    bool bSuccess = Super::Initialize();
    if (!bSuccess)
    {
        return false;
    }
    return true;
}

void UPlayerInfoSlotWidget::NativeConstruct()
{
    Super::NativeConstruct();
    GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    if (!GM)
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerInfoSlotWidget : GameMode is null"));
    }

    // ⭐ 홀아웃 이미지 미리 로드 및 캐싱
    if (!CachedHoleOutTexture)
    {
        CachedHoleOutTexture = Cast<UTexture2D>(
            StaticLoadObject(UTexture2D::StaticClass(), nullptr,
                TEXT("/Game/TextPopupsAndSpells/Chance/holeout"))
            );

        if (CachedHoleOutTexture)
        {
            UE_LOG(LogTemp, Log, TEXT("✅ HoleOut texture cached successfully"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("❌ Failed to load HoleOut texture from: /Game/TextPopupsAndSpells/Chance/holeout"));
        }
    }

    // ⭐ Image_HoleOut 위젯 바인딩 확인
    if (!Image_HoleOut)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Image_HoleOut is not bound! Please bind it in UMG Designer."));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("✅ Image_HoleOut is properly bound"));
        // 초기에는 숨김
        Image_HoleOut->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UPlayerInfoSlotWidget::NativeTick(const FGeometry& Geometry, float InDeltaTime)
{
    Super::NativeTick(Geometry, InDeltaTime);

    float DistanceToHole = 0;

    if (TextBlock_Distance_On)
    {
        // 홀인 또는 컨시드 상태 확인
        bool bIsPlayerHoleIn = false;

        if (GM && GM->PlayerManager && GM->PlayerManager->GetPlayers().IsValidIndex(OwningPlayerIndex))
        {
            AGolfPlayer* Player = GM->PlayerManager->GetPlayers()[OwningPlayerIndex];
            if (Player)
            {
                // ⭐ 수정: 홀인(IsHoleIn) 또는 컨시드(Player_HoleOut) 모두 "HOLE IN" 표시
                bIsPlayerHoleIn = Player->IsHoleIn() || Player->GetPlayerState() == EPlayerState::Player_HoleOut;
            }
        }

        // 홀인/컨시드 상태면 거리 및 이미지 표시
        if (bIsPlayerHoleIn)
        {
            // ⭐ 수정: Off 상태(대기 중)는 거리 0으로 표시
            if (TextBlock_Distance_Off)
            {
                TextBlock_Distance_Off->SetText(FText::FromString(TEXT("HOLE IN")));
            }

            // On 상태(현재 턴)는 "HOLE IN" 표시
            if (TextBlock_Distance_On)
            {
                TextBlock_Distance_On->SetText(FText::FromString(TEXT("0")));
            }

            // ⭐ Image_HoleOut에 홀아웃 이미지 표시
            if (GM)
            {
                if (GM->IsStrokeMode())
                {
                    if (Image_HoleOut)
                    {
                        Image_HoleOut->SetVisibility(ESlateVisibility::Visible);

                        // ⭐ 캐시된 홀아웃 이미지 사용
                        if (CachedHoleOutTexture)
                        {
                            Image_HoleOut->SetBrushFromTexture(CachedHoleOutTexture);
                            Image_HoleOut->SetRenderOpacity(1.0f);
                        }
                        else
                        {
                            UE_LOG(LogTemp, Error, TEXT("❌ CachedHoleOutTexture is null in NativeTick"));
                        }
                    }
                }
            }
        }
        else
        {
            // ⭐ 홀인이 아닐 때는 Image_HoleOut 숨김
            if (Image_HoleOut)
            {
                Image_HoleOut->SetVisibility(ESlateVisibility::Collapsed);
            }

            // ⭐ 수정: 각 슬롯은 자신이 담당하는 플레이어(OwningPlayerIndex)의 거리를 표시
            if (GM->PlayerManager)
            {
                if (GM->CurrentHole - 1 < GM->MaxHoleCount)
                {
                    if (GM->PlayerManager->GetPlayerBalls().IsValidIndex(OwningPlayerIndex) &&
                        GM->GameInfo.SelectedMap.HolecupPositions.IsValidIndex(GM->CurrentHole - 1))
                    {
                        DistanceToHole = FVector::Dist(GM->PlayerManager->GetPlayerBalls()[OwningPlayerIndex]->GetActorLocation(),
                            GM->GameInfo.SelectedMap.HolecupPositions[GM->CurrentHole - 1]);
                    }
                }
            }

            if (DistanceToHole < 0.1f)
            {
                TextBlock_Distance_On->SetText(FText::FromString(TEXT("0m")));
                if (TextBlock_Distance_Off)
                {
                    TextBlock_Distance_Off->SetText(FText::FromString(TEXT("0m")));
                }
            }
            else
            {
                TextBlock_Distance_On->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), DistanceToHole / 100.0f)));
                if (TextBlock_Distance_Off)
                {
                    TextBlock_Distance_Off->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), DistanceToHole / 100.0f)));
                }
            }
        }
    }

    // ⭐⭐⭐ 런타임 추가 플레이어 깜빡임 애니메이션 ⭐⭐⭐
    if (bIsRuntimeBlinking)
    {
        RuntimeBlinkElapsed += InDeltaTime;

        // 지정된 시간 후 깜빡임 중지
        if (RuntimeBlinkElapsed >= RuntimeBlinkDuration)
        {
            bIsRuntimeBlinking = false;
            SetRenderOpacity(0.7f); // 반투명 상태 유지
            UE_LOG(LogTemp, Log, TEXT("✨ Runtime player blink animation finished"));
        }
        else
        {
            // 사인파 깜빡임
            const float Phase = 2.0f * PI * RuntimeBlinkFrequency * RuntimeBlinkElapsed;
            const float T01 = 0.5f * (FMath::Sin(Phase) + 1.0f); // 0..1
            const float OpacityValue = FMath::Lerp(RuntimeMinOpacity, RuntimeMaxOpacity, T01);
            SetRenderOpacity(OpacityValue);
        }
    }

    // 기존 Chance 이미지 블링크 처리
    if (bBlinking && Image_Chance)
    {
        Elapsed += InDeltaTime;
        const float Phase = 2.f * PI * BlinkFrequency * Elapsed;
        const float T01 = 0.5f * (FMath::Sin(Phase) + 1.0f);
        const float Opacity = FMath::Lerp(MinOpacity, MaxOpacity, T01);
        Image_Chance->SetRenderOpacity(Opacity);
    }
}

// ⭐⭐⭐ 새로 추가: 런타임 추가 플레이어 스타일 설정 함수 ⭐⭐⭐
void UPlayerInfoSlotWidget::SetRuntimeAddedStyle(bool bIsRuntime)
{
    if (bIsRuntime)
    {
        UE_LOG(LogTemp, Log, TEXT("✨ Setting runtime-added player style for Player %d"), OwningPlayerIndex);

        // 방법 1: 3초간 깜빡임 애니메이션 시작
        bIsRuntimeBlinking = true;
        RuntimeBlinkElapsed = 0.0f;

        // 방법 2: 배경에 노란색 틴트 적용
        if (Image_Player_Background_Off)
        {
            Image_Player_Background_Off->SetColorAndOpacity(
                FLinearColor(1.0f, 1.0f, 0.6f, 0.9f) // 연한 노란색
            );
        }
        if (Image_Player_Background_On)
        {
            Image_Player_Background_On->SetColorAndOpacity(
                FLinearColor(1.0f, 1.0f, 0.6f, 0.9f)
            );
        }

        // 방법 3 (옵션): "GUEST" 라벨 표시
        if (TextBlock_GuestLabel)
        {
            TextBlock_GuestLabel->SetVisibility(ESlateVisibility::Visible);
            TextBlock_GuestLabel->SetText(FText::FromString(TEXT("GUEST")));
            TextBlock_GuestLabel->SetColorAndOpacity(
                FSlateColor(FLinearColor(1.0f, 0.65f, 0.0f, 1.0f)) // 주황색
            );

            UE_LOG(LogTemp, Log, TEXT("✅ Guest label displayed"));
        }

        // 방법 4 (옵션): 게스트 배지 아이콘 표시
        if (Image_GuestBadge)
        {
            Image_GuestBadge->SetVisibility(ESlateVisibility::Visible);

            // 게스트 아이콘 로드 (경로는 프로젝트에 맞게 수정)
            UTexture2D* GuestIcon = Cast<UTexture2D>(
                StaticLoadObject(UTexture2D::StaticClass(), nullptr,
                    TEXT("/Game/UMG/Resources/Images/Icons/GuestIcon"))
                );

            if (GuestIcon)
            {
                Image_GuestBadge->SetBrushFromTexture(GuestIcon);
                UE_LOG(LogTemp, Log, TEXT("✅ Guest badge icon set"));
            }
            else
            {
                // 아이콘 로드 실패 시 색상만 설정
                Image_GuestBadge->SetColorAndOpacity(
                    FLinearColor(1.0f, 0.65f, 0.0f, 1.0f)
                );
                UE_LOG(LogTemp, Warning, TEXT("⚠️ Guest icon not found, using color only"));
            }
        }

        // 방법 5 (옵션): 테두리 표시
        if (Image_RuntimeBorder)
        {
            Image_RuntimeBorder->SetVisibility(ESlateVisibility::Visible);
            Image_RuntimeBorder->SetColorAndOpacity(
                FLinearColor(1.0f, 0.84f, 0.0f, 1.0f) // 금색 테두리
            );

            UE_LOG(LogTemp, Log, TEXT("✅ Runtime border displayed"));
        }

        UE_LOG(LogTemp, Log, TEXT("🎨 Runtime-added player style applied successfully"));
    }
    else
    {
        // 일반 플레이어 스타일로 복원
        bIsRuntimeBlinking = false;
        SetRenderOpacity(1.0f);

        if (Image_Player_Background_Off)
        {
            Image_Player_Background_Off->SetColorAndOpacity(FLinearColor::White);
        }
        if (Image_Player_Background_On)
        {
            Image_Player_Background_On->SetColorAndOpacity(FLinearColor::White);
        }

        if (TextBlock_GuestLabel)
        {
            TextBlock_GuestLabel->SetVisibility(ESlateVisibility::Collapsed);
        }
        if (Image_GuestBadge)
        {
            Image_GuestBadge->SetVisibility(ESlateVisibility::Collapsed);
        }
        if (Image_RuntimeBorder)
        {
            Image_RuntimeBorder->SetVisibility(ESlateVisibility::Collapsed);
        }

        UE_LOG(LogTemp, Log, TEXT("🔄 Normal player style restored for Player %d"), OwningPlayerIndex);
    }
}

void UPlayerInfoSlotWidget::UpdateStroke(const FPlayerInfo PlayerInfo)
{
    int32 CurrentShotCount = PlayerInfo.ShotCountPerHole[GM->CurrentHole - 1];
    TextBlock_Stroke_On->SetText(FText::FromString(FString::Printf(TEXT("%d"), CurrentShotCount + 1)));
    TextBlock_Stroke_Off->SetText(FText::FromString(FString::Printf(TEXT("%d"), CurrentShotCount + 1)));
}

void UPlayerInfoSlotWidget::SetChance(bool Blinking, int32 Score)
{
    if (GM)
    {
        if (GM->IsStrokeMode())
        {
            Score++;
            int32 FinalScore = (Score - GM->GameInfo.SelectedMap.ParScores[GM->CurrentHole - 1]);

            if (GM->ChanceTextureMap.Contains(FinalScore))
            {
                if (GM->ChanceTextureMap[FinalScore] != nullptr)
                {
                    bBlinking = Blinking;

                    Image_Chance->SetBrushFromTexture(GM->ChanceTextureMap[FinalScore], false);

                    if (!Blinking)
                    {
                        Image_Chance->SetVisibility(ESlateVisibility::Collapsed);
                        StopBlink();
                    }
                    else
                    {
                        Image_Chance->SetVisibility(ESlateVisibility::HitTestInvisible);
                        StartBlink();
                    }
                }
                else
                {
                    Image_Chance->SetVisibility(ESlateVisibility::Collapsed);
                    bBlinking = false;
                    UE_LOG(LogTemp, Error, TEXT("PlayerInfoSlotWidget::ChangeChanceImage : Images array index not valid"));
                }
            }
        }
    }
}

void UPlayerInfoSlotWidget::StartBlink()
{
    bBlinking = true;
    Elapsed = 0.f;
    if (Image_Chance) Image_Chance->SetVisibility(ESlateVisibility::Visible);
    if (Image_Chance) Image_Chance->SetRenderOpacity(1.f);
}

void UPlayerInfoSlotWidget::StopBlink()
{
    bBlinking = false;
    if (Image_Chance) Image_Chance->SetRenderOpacity(0.f);
}

void UPlayerInfoSlotWidget::SetPlayerInfo(const FPlayerInfo& InPlayerInfo, int32 CurrentHoleIndex, int32 PlayerIdx, float DistanceToHole)
{
    // OwningPlayerIndex should be set by the creator; do not overwrite here

    // 디버그 로그 (문제 해결 후 제거 가능)
    UE_LOG(LogTemp, Log, TEXT("📊 PlayerInfoSlot: Setting Player %d info, Distance: %.1fm"), PlayerIdx, DistanceToHole / 100.0f);


    if (TextBlock_Name_Off)
    {
        TextBlock_Name_Off->SetText(FText::FromString(InPlayerInfo.NickName));
    }
    if (TextBlock_Name_On)
    {
        TextBlock_Name_On->SetText(FText::FromString(InPlayerInfo.NickName));
    }

    if (TextBlock_UnderPar_Off)
    {
        TextBlock_UnderPar_Off->SetText(FText::Format(NSLOCTEXT("PlayerSlot", "ScoreFormat", " {0}"), FText::AsNumber(InPlayerInfo.TotalScore)));
    }
    if (TextBlock_UnderPar_On)
    {
        TextBlock_UnderPar_On->SetText(FText::Format(NSLOCTEXT("PlayerSlot", "ScoreFormat", " {0}"), FText::AsNumber(InPlayerInfo.TotalScore)));
    }

    if (TextBlock_Stroke_Off)
    {
        if (InPlayerInfo.ShotCountPerHole.IsValidIndex(CurrentHoleIndex - 1) && InPlayerInfo.ShotCountPerHole[CurrentHoleIndex - 1] > 0)
        {
            int32 IncrementCount = InPlayerInfo.bIsHoleout ? 0 : 1;
            TextBlock_Stroke_Off->SetText(FText::Format(NSLOCTEXT("PlayerSlot", "ShotCountFormat", "{0}"), FText::AsNumber(InPlayerInfo.ShotCountPerHole[CurrentHoleIndex - 1] + IncrementCount)));
        }
        else
        {
            TextBlock_Stroke_Off->SetText(FText::FromString(TEXT("0")));
        }
    }
    if (TextBlock_Stroke_On)
    {
        if (InPlayerInfo.ShotCountPerHole.IsValidIndex(CurrentHoleIndex - 1))
        {
            int32 IncrementCount = InPlayerInfo.bIsHoleout ? 0 : 1;
            TextBlock_Stroke_On->SetText(FText::Format(NSLOCTEXT("PlayerSlot", "ShotCountFormat", "{0}"), FText::AsNumber(InPlayerInfo.ShotCountPerHole[CurrentHoleIndex - 1] + IncrementCount)));
        }
        else
        {
            TextBlock_Stroke_On->SetText(FText::FromString(TEXT("0")));
        }
    }

    // 홀인 또는 컨시드 상태 확인
    bool bIsPlayerHoleIn = false;
    if (GM && GM->PlayerManager && GM->PlayerManager->GetPlayers().IsValidIndex(PlayerIdx))
    {
        AGolfPlayer* Player = GM->PlayerManager->GetPlayers()[PlayerIdx];
        if (Player)
        {
            // ⭐ 수정: 홀인(IsHoleIn) 또는 컨시드(Player_HoleOut) 모두 "HOLE IN" 표시
            bIsPlayerHoleIn = Player->IsHoleIn() || Player->GetPlayerState() == EPlayerState::Player_HoleOut;
        }
    }

    // 거리 정보 업데이트 - 홀인/컨시드 체크
    if (bIsPlayerHoleIn)
    {
        // ⭐ 수정: Off 상태(대기 중)는 거리 0으로 표시
        if (TextBlock_Distance_Off)
        {
            TextBlock_Distance_Off->SetText(FText::FromString(TEXT("HOLE IN")));
        }

        // On 상태(현재 턴)는 "HOLE IN" 표시
        if (TextBlock_Distance_On)
        {
            TextBlock_Distance_On->SetText(FText::FromString(TEXT("0")));
        }


        // ⭐ Image_HoleOut에 홀아웃 이미지 표시
        if (Image_HoleOut)
        {
            if (GM->IsStrokeMode())
            {
                Image_HoleOut->SetVisibility(ESlateVisibility::Visible);

                // ⭐ 캐시된 홀아웃 이미지 사용
                if (CachedHoleOutTexture)
                {
                    Image_HoleOut->SetBrushFromTexture(CachedHoleOutTexture);
                    Image_HoleOut->SetRenderOpacity(1.0f);
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("❌ CachedHoleOutTexture is null in SetPlayerInfo"));
                }
            }
        }
    }
    else
    {
        // 홀아웃이 아닌 경우 거리 표시
        if (TextBlock_Distance_Off)
        {
            if (DistanceToHole < 0.1f)
            {
                TextBlock_Distance_Off->SetText(FText::FromString(TEXT("0")));
            }
            else
            {
                TextBlock_Distance_Off->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), DistanceToHole / 100.0f)));
            }
        }

        if (TextBlock_Distance_On)
        {
            if (DistanceToHole < 0.1f)
            {
                TextBlock_Distance_On->SetText(FText::FromString(TEXT("0")));
            }
            else
            {
                TextBlock_Distance_On->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), DistanceToHole / 100.0f)));
            }
        }
    }

    // 배경 이미지 설정
    const int32 ImageIndex = (DisplayIndex != INDEX_NONE) ? DisplayIndex : PlayerIdx;

    if (Image_Player_Background_Off)
    {
        FString ImagePath = FString::Printf(TEXT("/Game/UMG/Resources/Images/InGame_365/small_p%d"), ImageIndex + 1);
        UTexture2D* BackgroundTexture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *ImagePath));

        if (BackgroundTexture)
        {
            Image_Player_Background_Off->SetBrushFromTexture(BackgroundTexture);
            UE_LOG(LogTemp, Log, TEXT("✅ Player %d background image set from %s"), ImageIndex, *ImagePath);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("❌ Failed to load background texture for Player %d from %s"), ImageIndex, *ImagePath);
        }
    }

    if (Image_Player_Background_On)
    {
        FString ImagePath = FString::Printf(TEXT("/Game/UMG/Resources/Images/InGame/big_p%d"), ImageIndex + 1);
        UTexture2D* BackgroundTexture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *ImagePath));

        if (BackgroundTexture)
        {
            Image_Player_Background_On->SetBrushFromTexture(BackgroundTexture);
            UE_LOG(LogTemp, Log, TEXT("✅ Player %d background image set from %s"), ImageIndex, *ImagePath);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("❌ Failed to load background texture for Player %d from %s"), ImageIndex, *ImagePath);
        }
    }
}

void UPlayerInfoSlotWidget::HideAllStateDisplay()
{
    if (TextBlock_Name_On) TextBlock_Name_On->SetVisibility(ESlateVisibility::Collapsed);
    if (TextBlock_UnderPar_On) TextBlock_UnderPar_On->SetVisibility(ESlateVisibility::Collapsed);
    if (TextBlock_Stroke_On) TextBlock_Stroke_On->SetVisibility(ESlateVisibility::Collapsed);
    if (TextBlock_Stroke_On_st) TextBlock_Stroke_On_st->SetVisibility(ESlateVisibility::Collapsed);
    if (TextBlock_Distance_On) TextBlock_Distance_On->SetVisibility(ESlateVisibility::Collapsed);
    if (TextBlock_Distance_On_m) TextBlock_Distance_On_m->SetVisibility(ESlateVisibility::Collapsed);
    if (Image_Player_Background_On) Image_Player_Background_On->SetVisibility(ESlateVisibility::Collapsed);
    if (Image_Chance) Image_Chance->SetVisibility(ESlateVisibility::Collapsed);
    if (Image_HoleOut) Image_HoleOut->SetVisibility(ESlateVisibility::Collapsed);

    if (TextBlock_Name_Off) TextBlock_Name_Off->SetVisibility(ESlateVisibility::Collapsed);
    if (TextBlock_UnderPar_Off) TextBlock_UnderPar_Off->SetVisibility(ESlateVisibility::Collapsed);
    if (TextBlock_Stroke_Off) TextBlock_Stroke_Off->SetVisibility(ESlateVisibility::Collapsed);
    if (TextBlock_Stroke_Off_st) TextBlock_Stroke_Off_st->SetVisibility(ESlateVisibility::Collapsed);
    if (TextBlock_Distance_Off) TextBlock_Distance_Off->SetVisibility(ESlateVisibility::Collapsed);
    if (TextBlock_Distance_Off_m) TextBlock_Distance_Off_m->SetVisibility(ESlateVisibility::Collapsed);
    if (Image_Player_Background_Off) Image_Player_Background_Off->SetVisibility(ESlateVisibility::Collapsed);
}

void UPlayerInfoSlotWidget::UpdatePlayerStateDisplay(EPlayerState NewState)
{
    if (bIsRuntimeAdded)
        return;

    // 홀아웃(홀인/컨시드) 상태 특별 처리
    if (NewState == EPlayerState::Player_HoleOut)
    {
        // ⭐ 수정: Off 상태는 거리 0으로 표시
        if (TextBlock_Distance_Off)
        {
            TextBlock_Distance_Off->SetText(FText::FromString(TEXT("HOLE IN")));
        }

        // On 상태는 "HOLE IN" 표시
        if (TextBlock_Distance_On)
        {
            TextBlock_Distance_On->SetText(FText::FromString(TEXT("0")));
        }


        // ⭐ Image_HoleOut에 홀아웃 이미지 표시
        if (Image_HoleOut)
        {
            if (GM)
            {
                if (GM->IsStrokeMode())
                {
                    Image_HoleOut->SetVisibility(ESlateVisibility::Visible);

                    // ⭐ 캐시된 홀아웃 이미지 사용
                    if (CachedHoleOutTexture)
                    {
                        Image_HoleOut->SetBrushFromTexture(CachedHoleOutTexture);
                        Image_HoleOut->SetRenderOpacity(1.0f);
                    }
                    else
                    {
                        UE_LOG(LogTemp, Error, TEXT("❌ CachedHoleOutTexture is null in UpdatePlayerStateDisplay"));
                    }
                }
            }
        }

        // Off UI 표시
        if (TextBlock_Name_Off) TextBlock_Name_Off->SetVisibility(ESlateVisibility::Visible);
        if (TextBlock_UnderPar_Off) TextBlock_UnderPar_Off->SetVisibility(ESlateVisibility::Visible);
        if (TextBlock_Stroke_Off) TextBlock_Stroke_Off->SetVisibility(ESlateVisibility::Visible);
        if (TextBlock_Stroke_Off_st) TextBlock_Stroke_Off_st->SetVisibility(ESlateVisibility::Visible);
        if (TextBlock_Distance_Off) TextBlock_Distance_Off->SetVisibility(ESlateVisibility::Visible);
        if (TextBlock_Distance_Off_m) TextBlock_Distance_Off_m->SetVisibility(ESlateVisibility::Visible);
        if (Image_Player_Background_Off) Image_Player_Background_Off->SetVisibility(ESlateVisibility::Visible);
        if (Image_HoleOut)
        {
            if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Image_HoleOut->Slot))
            {
                // 위치 (X, Y)
                CanvasSlot->SetPosition(FVector2D(350, 5));
                //CanvasSlot->SetSize(FVector2D(200.f, 100.f));
            }
        }

        // On UI 숨김
        if (TextBlock_Name_On) TextBlock_Name_On->SetVisibility(ESlateVisibility::Collapsed);
        if (TextBlock_UnderPar_On) TextBlock_UnderPar_On->SetVisibility(ESlateVisibility::Collapsed);
        if (TextBlock_Stroke_On) TextBlock_Stroke_On->SetVisibility(ESlateVisibility::Collapsed);
        if (TextBlock_Stroke_On_st) TextBlock_Stroke_On_st->SetVisibility(ESlateVisibility::Collapsed);
        if (TextBlock_Distance_On) TextBlock_Distance_On->SetVisibility(ESlateVisibility::Collapsed);
        if (TextBlock_Distance_On_m) TextBlock_Distance_On_m->SetVisibility(ESlateVisibility::Collapsed);
        if (Image_Player_Background_On) Image_Player_Background_On->SetVisibility(ESlateVisibility::Collapsed);

        // ⭐ 수정: Image_HoleOut은 홀아웃 이미지를 표시하므로 Collapsed 하지 않음

        if (CurrentTurnIndicator)
        {
            CurrentTurnIndicator->SetVisibility(ESlateVisibility::Collapsed);
        }

        return;
    }

    // 현재 턴 인디케이터 로직
    if (NewState == EPlayerState::Player_Ready || NewState == EPlayerState::Player_Shot)
    {
        if (TextBlock_Name_On) TextBlock_Name_On->SetVisibility(ESlateVisibility::Visible);
        if (TextBlock_UnderPar_On) TextBlock_UnderPar_On->SetVisibility(ESlateVisibility::Visible);
        if (TextBlock_Stroke_On) TextBlock_Stroke_On->SetVisibility(ESlateVisibility::Visible);
        if (TextBlock_Stroke_On_st) TextBlock_Stroke_On_st->SetVisibility(ESlateVisibility::Visible);
        if (TextBlock_Distance_On) TextBlock_Distance_On->SetVisibility(ESlateVisibility::Visible);
        if (TextBlock_Distance_On_m) TextBlock_Distance_On_m->SetVisibility(ESlateVisibility::Visible);
        if (Image_Player_Background_On) Image_Player_Background_On->SetVisibility(ESlateVisibility::Visible);
        if (Image_HoleOut)
        {
            if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Image_HoleOut->Slot))
            {
                // 위치 (X, Y)
                CanvasSlot->SetPosition(FVector2D(428, 12));
                //CanvasSlot->SetSize(FVector2D(200.f, 100.f));
            }
        }

        if (TextBlock_Name_Off) TextBlock_Name_Off->SetVisibility(ESlateVisibility::Collapsed);
        if (TextBlock_UnderPar_Off) TextBlock_UnderPar_Off->SetVisibility(ESlateVisibility::Collapsed);
        if (TextBlock_Stroke_Off) TextBlock_Stroke_Off->SetVisibility(ESlateVisibility::Collapsed);
        if (TextBlock_Stroke_Off_st) TextBlock_Stroke_Off_st->SetVisibility(ESlateVisibility::Collapsed);
        if (TextBlock_Distance_Off) TextBlock_Distance_Off->SetVisibility(ESlateVisibility::Collapsed);
        if (TextBlock_Distance_Off_m) TextBlock_Distance_Off_m->SetVisibility(ESlateVisibility::Collapsed);
        if (Image_Player_Background_Off) Image_Player_Background_Off->SetVisibility(ESlateVisibility::Collapsed);
    }
    else
    {
        if (TextBlock_Name_On) TextBlock_Name_On->SetVisibility(ESlateVisibility::Collapsed);
        if (TextBlock_UnderPar_On) TextBlock_UnderPar_On->SetVisibility(ESlateVisibility::Collapsed);
        if (TextBlock_Stroke_On) TextBlock_Stroke_On->SetVisibility(ESlateVisibility::Collapsed);
        if (TextBlock_Stroke_On_st) TextBlock_Stroke_On_st->SetVisibility(ESlateVisibility::Collapsed);
        if (TextBlock_Distance_On) TextBlock_Distance_On->SetVisibility(ESlateVisibility::Collapsed);
        if (TextBlock_Distance_On_m) TextBlock_Distance_On_m->SetVisibility(ESlateVisibility::Collapsed);
        if (Image_Player_Background_On) Image_Player_Background_On->SetVisibility(ESlateVisibility::Collapsed);

        // ⭐ 홀아웃 상태가 아닐 때만 Image_HoleOut과 Image_Chance를 숨김
        if (NewState != EPlayerState::Player_HoleOut)
        {
            if (Image_HoleOut) Image_HoleOut->SetVisibility(ESlateVisibility::Collapsed);
            if (Image_Chance) Image_Chance->SetVisibility(ESlateVisibility::Collapsed);
        }

        if (TextBlock_Name_Off) TextBlock_Name_Off->SetVisibility(ESlateVisibility::Visible);
        if (TextBlock_UnderPar_Off) TextBlock_UnderPar_Off->SetVisibility(ESlateVisibility::Visible);
        if (TextBlock_Stroke_Off) TextBlock_Stroke_Off->SetVisibility(ESlateVisibility::Visible);
        if (TextBlock_Stroke_Off_st) TextBlock_Stroke_Off_st->SetVisibility(ESlateVisibility::Visible);
        if (TextBlock_Distance_Off) TextBlock_Distance_Off->SetVisibility(ESlateVisibility::Visible);
        if (TextBlock_Distance_Off_m) TextBlock_Distance_Off_m->SetVisibility(ESlateVisibility::Visible);
        if (Image_Player_Background_Off) Image_Player_Background_Off->SetVisibility(ESlateVisibility::Visible);
        if (Image_HoleOut)
        {
            if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Image_HoleOut->Slot))
            {
                // 위치 (X, Y)
                CanvasSlot->SetPosition(FVector2D(350, 5));
                //CanvasSlot->SetSize(FVector2D(200.f, 100.f));
            }
        }
    }

    if (CurrentTurnIndicator)
    {
        CurrentTurnIndicator->SetVisibility(NewState == EPlayerState::Player_Ready ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}

void UPlayerInfoSlotWidget::UpdateNickName(FString NickName)
{
    TextBlock_Name_Off->SetText(FText::FromString(NickName));
    TextBlock_Name_On->SetText(FText::FromString(NickName));
}
