
#include "GolfPlayer.h"

#include "BallParticleManager.h"
#include "GolfBall.h"
#include "InGameMode.h"
#include "GolfGameMode.h"
#include "GolfPlayerManager.h"
#include "StrokeWidget.h"
#include "GolfPlayerController.h"
#include "PlayerInfoSlotWidget.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "ParticleManager.h"
#include "ReadyBillboard.h"
#include "SoundManager.h"
#include "Components/BillboardComponent.h"
#include "Widgets/DistanceWidget.h"
#include "Widgets/ResultWidget.h"
#include "CameraManager.h"
#include "ParkDay/Utils/BallDropMarkerLibrary.h"
#include "ParkDay/BallDropMarkerActor.h"
#include "ParkDay/BallNamePlateComponent.h"
#include "ParkDay/GolfPlayerController.h"
#include "ShotCinematicComponent.h"
#include "ParkDay/Widgets/BallDistanceWidget.h"
#include "ParkDay/Widgets/InGamePlayerSelectWidget.h"
#include "ParkDay/Widgets/Menu/PlayerSelectWidget.h"

AGolfPlayer::AGolfPlayer()
{
    PlayerIndex = 0;
    CurrentPlayerState = EPlayerState::Player_Des; // ⭐ 초기 상태를 Player_Des로 설정
    bIsHoleIn = false;
    DoubleParThreshold = 2;
}

void AGolfPlayer::SetPlayerInfo(const FPlayerInfo& Info)
{
    AInGameMode* GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    PlayerInfo = Info;
    if (PlayerInfo.ShotCountPerHole.Num() < GM->MaxHoleCount)
    {
        PlayerInfo.ShotCountPerHole.SetNumZeroed(GM->MaxHoleCount);
    }
    UE_LOG(LogTemp, Log, TEXT("PlayerInfo set: %s, PlayerIndex: %d, HoleCount: %d"), *PlayerInfo.ID, PlayerIndex, PlayerInfo.HoleCount);
}


void AGolfPlayer::SetPlayerState(EPlayerState NewState)
{
    if (CurrentPlayerState == NewState)
    {
        UE_LOG(LogTemp, Verbose, TEXT("Player %d state already %s, skipping re-entry"),
            PlayerIndex, *UEnum::GetValueAsString(NewState));
        return;
    }
    //if (!CanTransitionToState(NewState))
    //{
    //    UE_LOG(LogTemp, Error, TEXT("❌ Invalid state transition: %s → %s for Player %d"),
    //        *UEnum::GetValueAsString(CurrentPlayerState),
    //        *UEnum::GetValueAsString(NewState),
    //        PlayerIndex);
    //    return;
    //}

    EPlayerState OldState = CurrentPlayerState;
    CurrentPlayerState = NewState;

    OnPlayerStateChanged(OldState, NewState);
    // ⭐ 델리게이트 브로드캐스트
    OnPlayerStateChangedDelegate.Broadcast(PlayerIndex, NewState);

}

void AGolfPlayer::UpdateBallPosition(FVector NewPosition)
{
    PlayerInfo.BeforePosX = PlayerInfo.BallPosX;
    PlayerInfo.BeforePosY = PlayerInfo.BallPosY;
    PlayerInfo.BeforePosZ = PlayerInfo.BallPosZ;

    PlayerInfo.BallPosX = NewPosition.X;
    PlayerInfo.BallPosY = NewPosition.Y;
    PlayerInfo.BallPosZ = NewPosition.Z;

    if (AInGameMode* GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
    {
		GM->FindPlayerInfoPtr(SlotIndex)->BeforePosX = PlayerInfo.BeforePosX;
		GM->FindPlayerInfoPtr(SlotIndex)->BeforePosY = PlayerInfo.BeforePosY;
		GM->FindPlayerInfoPtr(SlotIndex)->BeforePosZ = PlayerInfo.BeforePosZ;

		GM->FindPlayerInfoPtr(SlotIndex)->BallPosX = NewPosition.X;
	    GM->FindPlayerInfoPtr(SlotIndex)->BallPosY = NewPosition.Y;
		GM->FindPlayerInfoPtr(SlotIndex)->BallPosZ = NewPosition.Z;

		UE_LOG(LogTemp, VeryVerbose,
			TEXT("✅ Player %d ball position synced: (%.0f, %.0f, %.0f)"),
			PlayerIndex, NewPosition.X, NewPosition.Y, NewPosition.Z);
    }

    if (AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
        GameMode->SaveGameInfoToJSON();
}

void AGolfPlayer::PrepareShot(FVector Direction, float Power)
{
    if (AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
    {
        if (AGolfPlayerController* PC = Cast<AGolfPlayerController>(GameMode->PlayerControllerClass))
        {
            FRotator Rot = Direction.Rotation();
            Rot.Pitch = PC->ShotPitchAngle;
            Rot.Yaw = PC->ShotYawAngle;
            ShotDirection = Rot.Vector().GetSafeNormal();
        }
        else
        {
            ShotDirection = Direction.GetSafeNormal();
        }
    }
    else
    {
        ShotDirection = Direction.GetSafeNormal();
    }
    ShotPower = Power;
    UE_LOG(LogTemp, Log, TEXT("PrepareShot: PlayerIndex=%d, ShotDirection=%s (Pitch=%f) (Yaw = %f), ShotPower=%f"),
        PlayerIndex, *ShotDirection.ToString(), ShotDirection.Rotation().Pitch, ShotDirection.Rotation().Yaw, ShotPower);
}

void AGolfPlayer::ExecuteShot()
{
    if (CurrentPlayerState != EPlayerState::Player_Ready)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ ExecuteShot: Player %d not in Ready state, current state=%s"),
            PlayerIndex, *UEnum::GetValueAsString(CurrentPlayerState));
        return;
    }

    if (AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
    {
        AGolfBall* Ball = GameMode->PlayerManager->GetPlayerBalls()[PlayerIndex];
        if (Ball)
        {
            if (Ball->GetBallState() != EBallState::Ball_Ready)
            {
                UE_LOG(LogTemp, Warning, TEXT("ExecuteShot: Ball not ready, state=%s"), *UEnum::GetValueAsString(Ball->GetBallState()));
                return;
            }
            UE_LOG(LogTemp, Log, TEXT("ExecuteShot: Applying shot to ball at %s, Direction=%s, Power=%f"),
                *Ball->GetActorLocation().ToString(), *ShotDirection.ToString(), ShotPower);
            Ball->ApplyShot(ShotDirection, ShotPower);
            BEFOREPos = Ball->GetActorLocation();

            SetPlayerState(EPlayerState::Player_Shot);
            if (GameMode->CurrentGameMode == EGolfGameMode::StrokeMode)
				IncrementShotCount();

            if (GameMode->StrokeWidgetInstance)
            GameMode->StrokeWidgetInstance->ShowAimInfo(false);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Failed to find ball for PlayerIndex: %d"), PlayerIndex);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to cast to AGolfGameMode"));
    }
}

void AGolfPlayer::SetPlayerInfoToGameInfo()
{
    AInGameMode* GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());

    if (GM->GameInfo.Players.IsValidIndex(PlayerIndex))
    {
        GM->GameInfo.Players[PlayerIndex] = PlayerInfo;  // 직접 접근
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("SetPlayerInfoToGameInfo() GM is null"));
    }
}

void AGolfPlayer::IncrementShotCount()
{
    if (AInGameMode* GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
    {
		int32 HoleIndex = GM->CurrentHole - 1;
		if (PlayerInfo.ShotCountPerHole.IsValidIndex(HoleIndex))
		{
			PlayerInfo.ShotCountPerHole[HoleIndex]++;
			PlayerInfo.ShotCount++;
                //여기서 gm의 gameinfo의 playerinfo에 샷 카운트를 증가시키는데, 플레이어 삭제 시 json(gameinfo)의 playerinfo는 미리 삭제되서
                    //array 에러로 크래시가 난다. 따라서, slotindex로 찾아서 증감 시키도록 함

            if (FPlayerInfo* FoundedPlayerInfo = GM->FindPlayerInfoPtr(SlotIndex))
            {
                FoundedPlayerInfo->ShotCount++;
                FoundedPlayerInfo->ShotCountPerHole[HoleIndex]++;

                //GM->GameInfo.Players[PlayerIndex].ShotCount++;
                //GM->GameInfo.Players[PlayerIndex].ShotCountPerHole[GM->CurrentHole - 1]++;

                UE_LOG(LogTemp, Log, TEXT("--GAMEMODE - Player %d: Shot count for hole %d incremented to %d"),
                    PlayerIndex, GM->CurrentHole, FoundedPlayerInfo->ShotCountPerHole[HoleIndex]);
            }
        }
        UE_LOG(LogTemp, Log, TEXT("Player %d: Shot count for hole %d incremented to %d"),
            PlayerIndex, PlayerInfo.HoleCount, PlayerInfo.ShotCountPerHole[HoleIndex]);
 
    }
}

int32 AGolfPlayer::GetCurrentHoleShotCount() const
{
    AInGameMode* GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    int32 HoleIndex = GM->CurrentHole - 1;
    if (PlayerInfo.ShotCountPerHole.IsValidIndex(HoleIndex))
    {
        return PlayerInfo.ShotCountPerHole[HoleIndex];
    }
    return 0;
}

void AGolfPlayer::ResetShotCountForHole(int32 HoleIndex)
{
    if (PlayerInfo.ShotCountPerHole.IsValidIndex(HoleIndex - 1))
    {
        PlayerInfo.ShotCountPerHole[HoleIndex - 1] = 0;
        UE_LOG(LogTemp, Log, TEXT("Player %d: Shot count reset for hole %d"), PlayerIndex, HoleIndex);
    }
}

FString AGolfPlayer::GetPlayerStateString() const
{
    return UEnum::GetValueAsString(CurrentPlayerState);
}

bool AGolfPlayer::CanTransitionToState(EPlayerState NewState) const
{
    switch (CurrentPlayerState)
    {
    case EPlayerState::Player_Des:
        return NewState == EPlayerState::Player_Init; // ⭐ Player_Des -> Player_Init
    case EPlayerState::Player_Init:
        return NewState == EPlayerState::Player_Ready || NewState == EPlayerState::Player_Des; // ⭐ Player_Init -> Player_Ready or Player_Des
    case EPlayerState::Player_Ready:
        return NewState == EPlayerState::Player_Shot || NewState == EPlayerState::Player_Des;
    case EPlayerState::Player_Shot:
        return NewState == EPlayerState::Player_Results;
    case EPlayerState::Player_Results:
        // ⭐ 수정: Results -> HoleOut 또는 Des/Init 가능
        return NewState == EPlayerState::Player_Des || NewState == EPlayerState::Player_Init || NewState == EPlayerState::Player_HoleOut;
    case EPlayerState::Player_HoleOut:
        return NewState == EPlayerState::Player_Des || NewState == EPlayerState::Player_Init;
    default:
        return false;
    }
}

void AGolfPlayer::OnPlayerStateChanged(EPlayerState OldState, EPlayerState NewState)
{
    UE_LOG(LogTemp, Log, TEXT("🎮 Player %d State: %s → %s"),
        PlayerIndex,
        *UEnum::GetValueAsString(OldState),
        *UEnum::GetValueAsString(NewState));

    switch (NewState)
    {
    case EPlayerState::Player_Ready:
        OnEnterReadyState();
        break;
    case EPlayerState::Player_Shot:
        OnEnterShotState();
        break;
    case EPlayerState::Player_Results:
        OnEnterResultsState();
        break;
    case EPlayerState::Player_Init:
        OnEnterInitState(); // ⭐ 새로운 상태 처리 함수
        break;
    case EPlayerState::Player_Des:
        OnEnterDesState(); // ⭐ 새로운 상태 처리 함수
        break;

    case EPlayerState::Player_HoleOut: // ⭐ 추가
        OnEnterHoleOutState();
        break;
    }
}

void AGolfPlayer::OnEnterInitState() // ⭐ 추가
{
    UE_LOG(LogTemp, Log, TEXT("🟡 Player %d entered Init state"), PlayerIndex);
    if (GEngine)
    {
        // ⭐ 골프공 숨기기
        if (AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
        {
            if (GameMode->CurrentGameMode == EGolfGameMode::StrokeMode || GameMode->CurrentGameMode == EGolfGameMode::TrainingMode)
            {
                GameMode->StrokeWidgetInstance->UpdateShotBallSpeedAndAngle();
            }

            //if (GameMode->PlayerManager && GameMode->PlayerManager->GetPlayerBalls().IsValidIndex(PlayerIndex))
            //{
            //    AGolfBall* Ball = GameMode->PlayerManager->GetPlayerBalls()[PlayerIndex];
            //    if (Ball) {
            //        Ball->SetHoleIn(false);
            //        Ball->SetConceded(false);
            //    }
            //}
        }
        //bIsHoleIn = false;      
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow,
            FString::Printf(TEXT("🟡 Player %d Init"), PlayerIndex));
    }
}

void AGolfPlayer::OnEnterDesState() // ⭐ 추가
{
    UE_LOG(LogTemp, Log, TEXT("⚪ Player %d entered Des state"), PlayerIndex);
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::White,
            FString::Printf(TEXT("⚪ Player %d Des"), PlayerIndex));
    }

    // ⭐ 골프공 숨기기
    if (AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
    {
        if (GameMode->PlayerManager && GameMode->PlayerManager->GetPlayerBalls().IsValidIndex(PlayerIndex))
        {
            if (AGolfBall* Ball = GameMode->FindBall(PlayerIndex))
            {
                if (Ball)
                {
                    const int32 HoleIdx = GameMode->CurrentHole - 1;
                    const bool bHasShot = PlayerInfo.ShotCountPerHole.IsValidIndex(HoleIdx) && PlayerInfo.ShotCountPerHole[HoleIdx] > 0;

                    if (bHasShot && !Ball->IsHoleIn())
                    {
                        Ball->SetBallForceHidden(false);
                        Ball->SetBallVisibility(true, false);
                    }
                    else
                    {
                        Ball->SetBallForceHidden(true);
                        Ball->SetBallVisibility(false, false);
                    }
                    UE_LOG(LogTemp, Log, TEXT("✅ Player %d ball hidden in Des state"), PlayerIndex);
                }
            }
        }
    }
}

void AGolfPlayer::OnEnterReadyState()
{
    UE_LOG(LogTemp, Log, TEXT("🟢 Player %d ready for shot"), PlayerIndex);

    // ✅ 1단계: World 검증
    UWorld* World = GetWorld();
    if (!World || !IsValid(World))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ World is null in OnEnterReadyState"));
        return;
    }

    // ✅ 2단계: GameMode 검증
    AInGameMode* GameMode = Cast<AInGameMode>(World->GetAuthGameMode());
    if (!GameMode || !IsValid(GameMode))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ GameMode is null for Player %d"), PlayerIndex);
        return;
    }

    if (GameMode)
    {
        GameMode->UpdateAllPlayerInfoSlots();
        if (GameMode->BallDistanceWidget)
        {
            GameMode->BallDistanceWidget->SetRenderOpacity(0.f);
            UE_LOG(LogTemp, Log, TEXT("===================GolfPlayer----BallDistance = SetRenderOpacity(0.f)"));
        }

        UE_LOG(LogTemp, Warning, TEXT("-------------------------------------------- AutoTee trying opposite avoidance direction for Player %d"), PlayerIndex);
        if(GameMode->CheckFirstShot())
        GameMode->SetAutoTeeHeight();
    }

    // ✅ 3단계: PlayerManager 검증
    if (!GameMode->PlayerManager || !IsValid(GameMode->PlayerManager))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ PlayerManager is null"));
        return;
    }

    // ✅ 4단계: PlayerBalls 배열 검증
    TArray<AGolfBall*> PlayerBalls = GameMode->PlayerManager->GetPlayerBalls();
    if (!PlayerBalls.IsValidIndex(PlayerIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Invalid PlayerIndex %d, PlayerBalls.Num()=%d"),
            PlayerIndex, PlayerBalls.Num());
        return;
    }

    // ✅ 5단계: Ball 검증
    AGolfBall* Ball = PlayerBalls[PlayerIndex];
    if (!Ball || !IsValid(Ball))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Ball is null for PlayerIndex %d"), PlayerIndex);
        return;
    }

    //GameMode->StrokeWidgetInstance->UpdateShotBallSpeedAndAngle();
    //GameMode->StrokeWidgetInstance->WBP_Distance->UpdateShotDistance();

    if (!GameMode->IsRangeMode())
    {
        GameMode->UpdateBallNamePlateAndMarker();
	    GameMode->StrokeWidgetInstance->WBP_Distance->TextBlock_UpDown_Angle->SetText(FText::FromString("0.0"));
    	GameMode->StrokeWidgetInstance->WBP_Distance->TextBlock_LeftRight_Angle->SetText(FText::FromString("0.0"));
    	GameMode->StrokeWidgetInstance->WBP_Distance->TextBlock_BallSpeed->SetText(FText::FromString("0.0"));

        GameMode->StrokeWidgetInstance->UpdateMulliganTexture();

    }

    // ✅ 6단계: 안전한 Ball 조작
    try
    {
        // 현재 슬롯 이미지 처리 (안전하게)
        if (GameMode->GetCurrentSlot() && GameMode->GetCurrentSlot()->Image_Chance && GameMode->IsStrokeMode())
        {
            GameMode->GetCurrentSlot()->Image_Chance->SetVisibility(ESlateVisibility::Collapsed);
        }
        UBallDropMarkerLibrary::UpdateDropBillboardMarker(GameMode->DropMarker, FVector::ZeroVector);

        // 홀컵 위치 검증 후 거리 계산
        if (GameMode->MapInfo.HolecupPositions.IsValidIndex(GameMode->CurrentHole - 1))
        {
            FVector HolecupPos = GameMode->MapInfo.HolecupPositions[GameMode->CurrentHole - 1];
            FVector BallPos = Ball->GetActorLocation();
            float Distance = FVector::Dist(BallPos, HolecupPos) / 100.0f;
            float Height = HolecupPos.Z * 0.01f - BallPos.Z * 0.01f;

            // Ball 가시성 설정
            Ball->SetBallForceHidden(false);
            Ball->SetBallVisibility(true, true);

            if (Ball->bIsCinematic)
            {
                if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
                {
                    AGolfPlayerController* GPC = Cast<AGolfPlayerController>(PC);

                    GPC->ShotCinematicComponent->StopCinematic(0.f);
                }
            }


            UE_LOG(LogTemp, Log, TEXT("✅ Player %d ball visible, Distance=%.1fm, Height=%.1fm"),
                PlayerIndex, Distance, Height);

            // ✅ 7단계: TeeAnim 안전 처리
            SafeHandleTeeAnimation(GameMode);

            // ✅ 8단계: StrokeWidget 안전 처리

            if (!GameMode->IsRangeMode())
            {
                SafeHandleStrokeWidget(GameMode, Ball, Distance, Height);


                if (GameMode->IsStrokeMode())
                {
                    const FVector  BeforeLocation = Ball->LinkedCameraManager->GetActorLocation();
                    const FRotator BeforeRotator = Ball->LinkedCameraManager->GetActorRotation();
                    if (GameMode->GameInfo.GameOptions.Camera_Mode == 1)
                    {
                        Ball->LinkedCameraManager->SetFixedCameraPosition(BeforeLocation, BeforeRotator);
                        Ball->LinkedCameraManager->bUsePartialFixedMode = true;
                        Ball->LinkedCameraManager->SetUseFixedModeInReady(false);
                        Ball->LinkedCameraManager->ChangeCameraMode(ECameraMode::Fixed);
                    }
                    else
                    {
                        Ball->LinkedCameraManager->SetFixedCameraPosition(BeforeLocation, BeforeRotator);
                        Ball->LinkedCameraManager->bUsePartialFixedMode = false;
                        Ball->LinkedCameraManager->SetUseFixedModeInReady(false);
                        Ball->LinkedCameraManager->ChangeCameraMode(ECameraMode::Ready);
                    }

                    // 현재 플레이어가 Ready 상태가 되면 미니맵 요소들 표시
                    if (GameMode->MiniMapWidget)
                    {
                        GameMode->MiniMapWidget->ShowPlayerElements(PlayerIndex);
                    }

                    // PlayerController에게 AimActor 초기화 지시
                    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
                    {
                        if (AGolfPlayerController* GolfPC = Cast<AGolfPlayerController>(PC))
                        {
                            // ⭐ 수정된 부분: 첫 샷 전용 에임 위치 찾기
                            FVector OptimalAimPosition;

                           // if (GetCurrentHoleShotCount() == 0)
                            if(GameMode->CheckFirstShot())
                            {
                                // 첫 샷: 티-홀 방향 50미터 지점에서 OB 회피 위치 찾기
                                OptimalAimPosition = FindFirstShotAimPosition();
                                FString EndAnnouncement = PlayerInfo.NickName + TEXT("님 차례입니다! 티샷 하세요!");
                                GameMode->Speak(EndAnnouncement);

                                UE_LOG(LogTemp, Log, TEXT("🎯 First shot aim position set: %s"),
                                    *OptimalAimPosition.ToString());
                                GameMode->PlayerManager->SetSensorClub(CR2CLUB_DRIVER);

                                FTimerHandle TH;
                                GetWorld()->GetTimerManager().SetTimer(TH, [this, GameMode]() {
                                    AGolfPlayerController* PC = Cast<AGolfPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0)); 
                                    PC->GetAimActor()->SetAimVisibility(true);
                                    }, 0.5f, false);
                            }
                            else
                            {
                                // 첫 샷이 아님: 기존 로직 사용
                                OptimalAimPosition = FindAimPosition();
                                
                                if (Distance < 10.0f) // 10미터
                                {

                                    FTimerHandle TH;
                                    GetWorld()->GetTimerManager().SetTimer(TH, [this, GameMode]() {
                                        //GameMode->StrokeWidgetInstance->DisplayPuttingGuidance();
                                        GameMode->StrokeWidgetInstance->DisplayPuttingGuidanceWithAutoHide(10.0f);
                                        }, 0.5f, false);

                                    GameMode->PlayerManager->SetSensorClub(CR2CLUB_PUTTER);
                                }
                                else
                                {
                                    GameMode->PlayerManager->SetSensorClub(CR2CLUB_IRON7);
                                }
                                
                                UE_LOG(LogTemp, Log, TEXT("🎯 Regular shot aim position set: %s"),
                                    *OptimalAimPosition.ToString());

                                FString EndAnnouncement = PlayerInfo.NickName + TEXT("님 차례입니다!");
                                GameMode->Speak(EndAnnouncement);
                          
                            }

                            // PlayerController에 에임 위치 설정
                           // GolfPC->SetAimToExactPosition(OptimalAimPosition);
                            GolfPC->SetAimToPosition(OptimalAimPosition);
                            GolfPC->UpdateMiniMapForCurrentPlayerOnly();
                        }
                        
                    }

                    // ✅ 9단계: 기회 표시 안전 처리
                    SafeHandleChanceDisplay(GameMode, Ball);

                    GameMode->GameInfo.LatestUseMulliganPlayerIndex = -1;

                    GameMode->MiniMapWidget->InitTip2();
                    GameMode->MiniMapWidget->SetBallToHoleLineVisible(true);
                    GameMode->MiniMapWidget->SetBallToHoleLineColor(FLinearColor::Yellow);
                    GameMode->MiniMapWidget->SetShowOnlyCurrentPlayerLine(true);
                    GameMode->StrokeWidgetInstance->WBP_Distance->UpdateShotDistanceText(0.0f);
               
                    
					if (FVector::Dist(GameMode->GetCurrentTurnGolfBall()->GetActorLocation(),
						GameMode->MapInfo.HolecupPositions[GameMode->CurrentHole - 1]) <= 1000.f)
					{
                        FTimerHandle TH;
                        GetWorld()->GetTimerManager().SetTimer(TH, [this, GameMode]() {
                            AGolfPlayerController* PC = Cast<AGolfPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
                            PC->SetAimToExactPosition(GameMode->MapInfo.HolecupPositions[GameMode->CurrentHole - 1]);
                            PC->ToggleTerrainGrid();
                        }, 0.5f, false);
					}

                    for (AGolfBall* GolfBall : GameMode->PlayerManager->GetPlayerBalls())
                    {
                        if (GolfBall->IsHoleIn())
                            GolfBall->SetBallVisibility(false);
                    }

                    GameMode->MiniMapWidget->UpdateTip2();

                    GameMode->StrokeWidgetInstance->PositionCanvasPanelAboveHole();
                    GameMode->UpdateBallNamePlateAndMarker();
                    GameMode->InGamePlayerSelectWidget->PlayerSelect->UpdateButtonStatus();
                }
                else if (GameMode->CurrentGameMode == EGolfGameMode::TrainingMode)
                {
                }

                GameMode->ReadyBillboard->Billboard->SetSprite(GameMode->ReadyBillboard->NoReadyImage);
                GameMode->ReadyBillboard->Billboard->SetVisibility(true);

#if WITH_EDITOR
				/*              FCR2BallPosition position;
							  GameMode->PlayerManager->OnSensorBallReady(position);*/
#endif
            }
            
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("❌ Invalid HolecupPositions for hole %d"), GameMode->CurrentHole);
        }
    }
    catch (...)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Exception in OnEnterReadyState for Player %d"), PlayerIndex);
        return;
    }


    PlayerInfo.bIsHoleout = false;

    if (GameMode->IsStrokeMode())
    {
        for (FPlayerInfo& Player : GameMode->GameInfo.Players)
        {
            if (Player.SlotIndex == PlayerInfo.SlotIndex)
                Player.bIsHoleout = false;
        }

        BallSpeed = 0.0f;
        ShotYawAngle = 0.0f;
        
        GameMode->SaveGameInfoToJSON();
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
            FString::Printf(TEXT("🟢 Player %d Ready"), PlayerIndex));
    }
}

FVector AGolfPlayer::FindAimPosition()
{
    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    if (!GameMode)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ FindAimPosition: GameMode is null"));
        return FVector::ZeroVector;
    }

    AGolfBall* Ball = GameMode->PlayerManager->GetPlayerBalls()[PlayerIndex];
    if (!Ball)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ FindAimPosition: Ball is null for Player %d"), PlayerIndex);
        return FVector::ZeroVector;
    }

    FVector BallPos = Ball->GetActorLocation();
    if (!GameMode->MapInfo.HolecupPositions.IsValidIndex(GameMode->CurrentHole - 1))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ FindAimPosition: Invalid HolecupPositions for hole %d"), GameMode->CurrentHole);
        return FVector::ZeroVector;
    }

    FVector HoleCupPos = GameMode->MapInfo.HolecupPositions[GameMode->CurrentHole - 1];
    float DistanceMeters = FVector::Dist(BallPos, HoleCupPos) / 100.0f; // cm to meters

    FVector AimPos = HoleCupPos; // 기본적으로 홀컵 위치

    if (GetCurrentHoleShotCount() == 0)
    {
        FVector BallLocation = Ball->GetActorLocation();
        FVector HolecupLocation = HoleCupPos;
        FVector TeeLocation = GameMode->GetCurrentTeeLocation();
        FRotator TeeRotation = GameMode->TeeRotationArray[GameMode->CurrentHole - 1];
        if (GameMode->CheckFirstShot() && GameMode->IsStrokeMode())
        {
            UE_LOG(LogTemp, Log, TEXT("✅================SetTargetBall()  Camera target set with immediate sync"));
            AimPos = TeeLocation + TeeRotation.Vector() * 5000.f;

        }

        // 첫 샷(티 샷): 기본 홀컵 위치 사용 (요청에 따라 별도 처리 없음, 홀컵으로 유지)
        UE_LOG(LogTemp, Log, TEXT("FindAimPosition: First shot (tee shot) for Player %d, aiming at hole cup"), PlayerIndex);
        return AimPos;
    }

    // 첫 샷이 아닌 경우: 홀컵을 에임 포지션으로 설정
    UE_LOG(LogTemp, Log, TEXT("FindAimPosition: Non-first shot for Player %d, initial aim at hole cup"), PlayerIndex);

    // OB 라인 확인 로직: BallPos에서 HoleCupPos로의 경로를 샘플링하여 OB PhysMat 확인
    bool bHitsOB = false;
    const int32 SampleCount = 10; // 경로 샘플링 포인트 수 (조정 가능)
    FVector Direction = (HoleCupPos - BallPos).GetSafeNormal();
    float StepSize = FVector::Dist(BallPos, HoleCupPos) / SampleCount;

    for (int32 i = 1; i <= SampleCount; ++i) // 시작점 제외
    {
        FVector SamplePos = BallPos + Direction * (StepSize * i);
        UPhysicalMaterial* PhysMat = Ball->GetPhysMatBelow_ThroughEmpty(10.0f, 500.0f, ECC_Visibility, true);
        if (PhysMat && PhysMat->GetName().Contains("OB")) // OB PhysMat 이름에 "OB" 포함 가정 (코드 기반으로 유추)
        {
            bHitsOB = true;
            UE_LOG(LogTemp, Warning, TEXT("FindAimPosition: OB detected at sample point %d for Player %d"), i, PlayerIndex);
            break;
        }
    }

    // OB에 걸리고, 남은 거리가 50m 초과 시 회피 처리
    if (bHitsOB && DistanceMeters > 50.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("FindAimPosition: Avoiding OB for Player %d (Distance: %.1fm)"), PlayerIndex, DistanceMeters);

        // 회피 로직: 경로 방향의 수직 벡터로 약간偏移 (오른쪽으로 10%偏移 가정, 필요 시 조정)
        FVector PerpDir = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
        float OffsetAmount = 0.1f; //偏移 비율 (10%)
        FVector AvoidDir = (Direction + PerpDir * OffsetAmount).GetSafeNormal();

        // 새로운 에임 위치: 원래 거리만큼 회피 방향으로 설정
        AimPos = BallPos + AvoidDir * FVector::Dist(BallPos, HoleCupPos);

        // 회피 후 다시 OB 확인 (간단 재귀적 확인, 필요 시 반복 루프로 변경)
        bool bStillHitsOB = false;
        for (int32 i = 1; i <= SampleCount; ++i)
        {
            FVector SamplePos = BallPos + AvoidDir * (StepSize * i);
            UPhysicalMaterial* PhysMat = Ball->GetPhysMatBelow_ThroughEmpty(10.0f, 500.0f, ECC_Visibility, true);
            if (PhysMat && PhysMat->GetName().Contains("OB"))
            {
                bStillHitsOB = true;
                break;
            }
        }

        if (bStillHitsOB)
        {
            // 여전히 OB라면 반대 방향으로 시도
            AvoidDir = (Direction - PerpDir * OffsetAmount).GetSafeNormal();
            AimPos = BallPos + AvoidDir * FVector::Dist(BallPos, HoleCupPos);
            UE_LOG(LogTemp, Warning, TEXT("FindAimPosition: Still hits OB, trying opposite avoidance direction for Player %d"), PlayerIndex);
        }
    }

    return AimPos;
}



void AGolfPlayer::SpawnShotParticle()
{
    if (AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
    {
        if (AGolfBall* Ball = Cast<AGolfBall>(GameMode->PlayerManager->GetPlayerBalls()[GameMode->CurrentPlayerIndex]))
        {
            FString ParticleName = "";
            FString PhyMatName = "";

            PhyMatName = Ball->GetPhysMatBelow_ThroughEmpty(10.f, 500.f, ECC_Visibility, true)->GetName();

            if (PhyMatName == "Bunker")    ParticleName = "Shot_Bunker";
            else if (PhyMatName == "Dirt")    ParticleName = "Shot_Dirt";
            else if (PhyMatName == "Rough")    ParticleName = "Shot_Grass";
            else if (PhyMatName == "Grass")     ParticleName = "Shot_Grass";
			else if (PhyMatName == "Leaves")     ParticleName = "Shot_Grass";
            else if (PhyMatName == "Leavese")   ParticleName = "Shot_Grass";
            else                                ParticleName = "Bounce_Normal";

            if (!ParticleName.IsEmpty())
                GameMode->BallParticleManager->SpawnParticle(GetWorld(), *ParticleName, Ball->GetActorLocation());
            else
                UE_LOG(LogTemp, Warning, TEXT("Shot ParticleName is InValid"));

            UE_LOG(LogTemp, Log, TEXT("Shot Particle : %s"), *PhyMatName);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Shot Particle Ball is null"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Shot Particle GM is null"));
    }
}

void AGolfPlayer::OnEnterShotState()
{
    if (bIsContinue)
        bIsContinue = false;

    if (AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
    {
        if (GameMode->BallDistanceWidget)
        {
            GameMode->BallDistanceWidget->SetRenderOpacity(1.f);
            UE_LOG(LogTemp, Log, TEXT("===================GolfPlayer----BallDistance = SetRenderOpacity(1.f)"));
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("===================GolfPlayer----BallDistance = BallDistanceWidget == NULL"));
        }
       
        if (AGolfBall* Ball = GameMode->GetCurrentTurnGolfBall())
        {
            Ball->bIsCinematic = false;
            GameMode->LatestShotSlotIndex = SlotIndex;
            GameMode->GameInfo.LatestShotPlayerSlotIndex = SlotIndex;

            UE_LOG(LogTemp, Log, TEXT("LatestShotPlayerSlotIndex : %d"), SlotIndex);
            if (auto* SM = GetGameInstance()->GetSubsystem<USoundManager>())
            {
                SM->PlayAtLocation_ById("Effect.Ball.Shot", Ball->GetActorLocation(), 1.f);

	            if (GameMode->GetCurrentTurnGolfPlayer()->GetCurrentHoleShotCount() == 0 || GameMode->GetCurrentGameMode() == EGolfGameMode::RangeMode)   //첫 티 샷만
	                GameMode->BallParticleManager->SpawnParticle(GetWorld(), TEXT("Shot_Tee"), Ball->GetActorLocation());

	            AGolfPlayer* Player = GameMode->GetCurrentTurnGolfPlayer();
	            if (Player->GetSecsorShotPower() >= 25.f)
	            {
	                GameMode->BallParticleManager->SpawnParticle(GetWorld(), TEXT("Shot_Boom"), Ball->GetActorLocation());
                    SM->PlayAtLocation_ById("Effect.Ball.Boom", Ball->GetActorLocation(), 1.f);
	            }

                if (GameMode->CurrentGameMode == EGolfGameMode::StrokeMode)
				{
					if (GameMode->GetCurrentSlot())
					{
						GameMode->GetCurrentSlot()->Image_Chance->SetVisibility(ESlateVisibility::Collapsed);
					}
				}
            }
        }

        bDropAlready = false;

        GameMode->bClickedMinimap = false;
        SpawnShotParticle();
        if (GetWorld()->GetTimerManager().IsTimerActive(GameMode->PlayerManager->TH))
        {
            GetWorld()->GetTimerManager().ClearTimer(GameMode->PlayerManager->TH);
            GameMode->ReadyBillboard->Billboard->SetVisibility(false);
        }

        GameMode->ReadyBillboard->Billboard->SetVisibility(false);

        if (GameMode->CurrentGameMode == EGolfGameMode::StrokeMode || GameMode->CurrentGameMode == EGolfGameMode::TrainingMode)
        {
            GameMode->StrokeWidgetInstance->ShowAimInfo(false);
            UpdateBallSpeedAndAngles();
            GameMode->StrokeWidgetInstance->UpdateShotBallSpeedAndAngle();

            if (AGolfPlayerController* PC = Cast<AGolfPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
            {
                if (GameMode->GameInfo.GameOptions.SwingMotion)
                    //PC->WaitForCaptureReady();
                    if (BallSpeed > 20.0f && ShotYawAngle > -1.0f && ShotYawAngle < 1.0f)
                    PC->TriggerSwingRecording();
            }
        }    
        else  // rangemode
        {
            UpdateBallSpeedAndAngles();
            if (AGolfPlayerController* PC = Cast<AGolfPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
            {
                if (GameMode->GameInfo.GameOptions.RangeSwingMotion)
                    //PC->WaitForCaptureReady();      
                   // if (BallSpeed > 10.0f)
                    PC->TriggerSwingRecording();
            }
        }
       

        if (GameMode->TeeAnimInstance)
        {
            AActor* TeeAnimActor = GameMode->TeeAnimInstance;
            USkeletalMeshComponent* SKM = TeeAnimActor->FindComponentByClass<USkeletalMeshComponent>();
            if (SKM)
            {
                if (UAnimSingleNodeInstance* Inst = SKM->GetSingleNodeInstance())
                {
                    Inst->SetPlaying(true);
                }
            }
            else
                UE_LOG(LogTemp, Error, TEXT("TeeAnim SKM is null"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("TeeAnimArray is InValid Index"));
        }


        if (GameMode)
        {         
            // ⭐ 요청사항: SetAutoTeeHeight() 를 1초 후에 호출

            FTimerHandle TeeDelayHandle;
            GetWorld()->GetTimerManager().SetTimer(
                TeeDelayHandle,
                [this]()
                {
                    if (AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
                    {
                        UE_LOG(LogTemp, Log, TEXT("SetAutoTeeHeight() delayed 1s call - Player %d"), PlayerIndex);
                        GameMode->SetAutoTeeHeight();
                    }
                },
                1.0f,
                    false
                    );
        }


        bLastShotOB = false;
    }


    UE_LOG(LogTemp, Log, TEXT("🏌️ Player %d shot in progress"), PlayerIndex);
}

void AGolfPlayer::OnEnterResultsState()
{
    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());

    if (GameMode->CurrentGameMode == EGolfGameMode::StrokeMode || GameMode->CurrentGameMode == EGolfGameMode::TrainingMode)
    {
        UpdateShotDistance();
        UE_LOG(LogTemp, Log, TEXT("📊 Player %d entered Results state"), PlayerIndex);

        if (!bShotResultProcessed)
        {
            if (bLastShotHoleIn)
            {
                HandleHoleInResult();
            }
            else if (bLastShotOB)
            {
                ProcessOBResult();
            }
            else
            {
                HandleNormalResult();
            }
            //GameMode->StrokeWidgetInstance->TextBlock_ShotInfo_Distance->SetText()
            bShotResultProcessed = true;
            DisplayShotResult();
        }

        // 조건 추가해야함
        //ShotPitchAngle = PlayerController->ShotPitchAngle;
        //ShotYawAngle = PlayerController->ShotYawAngle
        if (BallSpeed > 20.0f && ShotYawAngle > -1.0f && ShotYawAngle < 1.0f)
        {
            UE_LOG(LogTemp, Log, TEXT("📊 Player bALL SPEED [ %f ]"), BallSpeed);
            UGolfPlayerManager* PlayerManager = GameMode->PlayerManager;
            AGolfBall* Ball = PlayerManager->GetPlayerBalls()[PlayerIndex];
            if (GameMode->GameInfo.GameOptions.SwingMotion && !Ball->CheckOutOfBounds())
                GameMode->ShowSwingMotion(true);
        }



    }
    else if (GameMode->CurrentGameMode == EGolfGameMode::RangeMode)
    {
        if (AGolfPlayerController* PC = Cast<AGolfPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
        {
            UpdateShotDistance();
           // if (BallSpeed > 10.0f)
            {
                if (GameMode->GameInfo.GameOptions.RangeSwingMotion)
                    GameMode->ShowSwingMotion(true);
                   // PC->ShowSwingClipWidget();
                   // PC->ShowSwingVideoWidget();
            }

            
        }
        UE_LOG(LogTemp, Log, TEXT("📊 Player %d entered Results state"), PlayerIndex);
    }

}

// GolfPlayer.cpp(AGolfPlayer::OnPlayerStateChanged 내부에서 호출되도록)
void AGolfPlayer::OnEnterHoleOutState()
{
    UE_LOG(LogTemp, Log, TEXT("🎉 Player %d entered HoleOut state!"), PlayerIndex);
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow,
            FString::Printf(TEXT("🎉 Player %d HoleOut!"), PlayerIndex));
    }
    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());

    if (GameMode)
    {
        if (bIsRuntimeAdded || bIsPendingDelete)
            return;

        // 현재 플레이어의 해당 홀 최종 스코어 계산
        int32 CurrentPlayerFinalScore = GetCurrentHoleShotCount(); // AGolfPlayer에서 현재 홀의 타수를 가져옴
        int32 RelativeScore = 0;
        // GameMode에서 현재 홀의 파(Par) 점수를 가져와 이미지 인덱스 계산
        if (bIsContinue == false)
        {
            if (GameMode->MapInfo.ParScores.IsValidIndex(GameMode->CurrentHole - 1))
            {
                int32 ParScore = GameMode->MapInfo.ParScores[GameMode->CurrentHole - 1];
                RelativeScore = CurrentPlayerFinalScore - ParScore; // 파 대비 상대적인 스코어 (-1: 버디, 0: 파, +1: 보기)

                RelativeScore = FMath::Clamp(RelativeScore, -ParScore + 1, ParScore);
                CurrentPlayerFinalScore = FMath::Clamp(CurrentPlayerFinalScore, 1, ParScore * 2); // 최소/최대 클램프

                PlayerInfo.ShotCount += CurrentPlayerFinalScore;
                if (!PlayerInfo.HoleScores.IsValidIndex(GameMode->CurrentHole - 1))
                {
                    PlayerInfo.HoleScores.Add(RelativeScore);
                    UE_LOG(LogTemp, Log, TEXT("PlayerInfo.HoleScores Num : %d"), PlayerInfo.HoleScores.Num());
                }

                PlayerInfo.ShotCountPerHole[GameMode->CurrentHole - 1] = CurrentPlayerFinalScore;
                PlayerInfo.TotalScore += RelativeScore;
            }
            else
            {
                int32 ParScore = GameMode->MapInfo.ParScores[GameMode->CurrentHole - 1];

                UE_LOG(LogTemp, Warning, TEXT("⚠️ Par score not found for current hole %d. Using raw shot count %d for result image."),
                    GameMode->CurrentHole, CurrentPlayerFinalScore);
                CurrentPlayerFinalScore = FMath::Clamp(CurrentPlayerFinalScore, 1, ParScore * 2); // 최소/최대 클램프
            }
        }
	
        if (!GameMode->bSetNextHole)
        {
	        if (CheckDoublePar())
	        {
	        	int32 CurrentHole = GameMode->CurrentHole - 1;
                int32 ParScore = GameMode->MapInfo.ParScores[GameMode->CurrentHole - 1];
                PlayerInfo.ShotCountPerHole[CurrentHole] = ParScore * 2;
	        	if (PlayerInfo.HoleScores.IsValidIndex(CurrentHole))
	        		PlayerInfo.HoleScores[CurrentHole] = RelativeScore;
	        	else
	        		PlayerInfo.HoleScores.Add(RelativeScore);
                FTimerHandle TH;
                GetWorld()->GetTimerManager().SetTimer(TH, [this, GameMode, RelativeScore]()
                    {
                        GameMode->ResultWidgetInstance->PlayResult(100);
                    }, 0.8f, false);
            }
	        else
	        {
                if (GameMode->StrokeWidgetInstance)
                    GameMode->StrokeWidgetInstance->SetLandType(2);
	        	//GameMode->SpawnHoleInParticle();
	        	FTimerHandle TH;
	        	GetWorld()->GetTimerManager().SetTimer(TH,[this,GameMode,RelativeScore]()
					{
						GameMode->ResultWidgetInstance->PlayResult(RelativeScore);
					},0.8f, false);

 	        }
        }

        PlayerInfo.bIsHoleout = true;
        SetPlayerInfo(PlayerInfo);
        SetPlayerInfoToGameInfo();
        GameMode->SaveGameInfoToJSON();
        if (GameMode->InGameScoreBoardWidgetInstance)
            GameMode->InGameScoreBoardWidgetInstance->UpdateScoreBoard();
        GameMode->UpdateBallNamePlateAndMarker();
        UE_LOG(LogTemp, Log, TEXT("✅ Showing player %d's result widget with score %d."), PlayerIndex, CurrentPlayerFinalScore);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("AInGameMode is null in OnEnterHoleOutState. Cannot control result widget."));
    }

}

bool AGolfPlayer::CheckLastHoleOut()
{
    if (IsHoleIn())
        return false;

    bool bIsLastHoleOut = true;

    AInGameMode* GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    if (GM)
    {
		for (AGolfPlayer* Player : GM->PlayerManager->GetPlayers())
		{
            if (Player->PlayerIndex != PlayerIndex)
            {
                if (!Player->IsHoleIn())
                    bIsLastHoleOut = false;
            }
		}
    }

    return bIsLastHoleOut;
}


void AGolfPlayer::ProcessOBResult()
{
    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    if (!GameMode) return;

    UE_LOG(LogTemp, Warning, TEXT("🚨 [ProcessOBResult] Player %d OB 결과 처리"), PlayerIndex);

    int32 HoleIdx = GameMode->CurrentHole - 1;
    if (!GameMode->MapInfo.ParScores.IsValidIndex(HoleIdx)) return;

    int32 ParScore = GameMode->MapInfo.ParScores[HoleIdx];
    int32 CurrentShots = GetCurrentHoleShotCount();
    int32 RelativeScore = CurrentShots - ParScore;

    // ✅ 결과 위젯 표시 (BallStopped와 동일하게)
    FTimerHandle TH;
    GetWorld()->GetTimerManager().SetTimer(TH, [this, GameMode, RelativeScore]()
        {
            if (GameMode->ResultWidgetInstance)
                GameMode->ResultWidgetInstance->PlayResult(RelativeScore);
        }, 0.3f, false);

    UE_LOG(LogTemp, Warning, TEXT("⛳ [OB] Player %d - Shots:%d Par:%d Score:%+d"),
        PlayerIndex, CurrentShots, ParScore, RelativeScore);
}
void AGolfPlayer::HandleHoleInResult()
{
    UE_LOG(LogTemp, Log, TEXT("🏆 Player %d achieved HOLE IN!"), PlayerIndex);
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Blue,
            FString::Printf(TEXT("🏆 Player %d: HOLE IN!"), PlayerIndex));
    }
}

void AGolfPlayer::HandleNormalResult()
{
    UE_LOG(LogTemp, Log, TEXT("⛳ Player %d completed normal shot"), PlayerIndex);

    if (AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
    {
        if (GameMode->MapInfo.HolecupPositions.IsValidIndex(GameMode->CurrentHole - 1))
        {
            FVector HolecupPos = GameMode->MapInfo.HolecupPositions[GameMode->CurrentHole - 1];
            FVector BallPos = FVector(PlayerInfo.BallPosX, PlayerInfo.BallPosY, PlayerInfo.BallPosZ);
            float Distance = FVector::Dist(BallPos, HolecupPos) / 100.0f;

            UE_LOG(LogTemp, Log, TEXT("📏 Player %d distance to hole: %.1fm"), PlayerIndex, Distance);
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
                    FString::Printf(TEXT("📏 P%d: %.1fm to hole"), PlayerIndex, Distance));
            }
        }
    }
}

void AGolfPlayer::DisplayShotResult()
{
    FString ResultMessage;
    FColor ResultColor = FColor::White;

    if (bLastShotHoleIn)
    {
        ResultMessage = FString::Printf(TEXT("🏆 Player %d: HOLE IN! (Score: %d)"), PlayerIndex, CurrentHoleScore);
        ResultColor = FColor::Blue;
    }
    else if (bLastShotOB)
    {
        ResultMessage = FString::Printf(TEXT("🚨 Player %d: OUT OF BOUNDS (Score: %d)"), PlayerIndex, CurrentHoleScore);
        ResultColor = FColor::Red;
    }
    else
    {
        ResultMessage = FString::Printf(TEXT("⛳ Player %d: Shot completed (Score: %d)"), PlayerIndex, CurrentHoleScore);
        ResultColor = FColor::Green;
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, ResultColor, ResultMessage);
    }

    UE_LOG(LogTemp, Log, TEXT("%s"), *ResultMessage);
}

void AGolfPlayer::UpdateBallSpeedAndAngles()
{
    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    UGolfPlayerManager* PlayerManager = GameMode->PlayerManager;
    AGolfPlayerController* PlayerController = Cast<AGolfPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
    AGolfBall* Ball = PlayerManager->GetPlayerBalls()[PlayerIndex];

    BallSpeed = PlayerController->ShotPower;
    ShotPitchAngle = PlayerController->ShotPitchAngle;
    ShotYawAngle = PlayerController->ShotYawAngle;
}

void AGolfPlayer::UpdateShotDistance()
{
    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    UGolfPlayerManager* PlayerManager = GameMode->PlayerManager;
    AGolfPlayerController* PlayerController = Cast<AGolfPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
    AGolfBall* Ball = PlayerManager->GetPlayerBalls()[PlayerIndex];

    ShotDistance = FVector::Dist(BEFOREPos, Ball->BallMesh->GetComponentLocation()) / 100.f;
}

float AGolfPlayer::GetSecsorShotPower()
{
    return ShotPower;
}

bool AGolfPlayer::CheckDoublePar() const
{
    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    if (!GameMode || !GameMode->MapInfo.ParScores.IsValidIndex(GameMode->CurrentHole - 1))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Invalid GameMode or Par score in CheckDoublePar"));
        return false;
    }

    int32 ParScore = GameMode->MapInfo.ParScores[GameMode->CurrentHole - 1];
    int32 CurrentShots = GetCurrentHoleShotCount();
    int32 DoubleParScore = ParScore * 2 - 1;

    if (GameMode->GetCurrentTurnGolfBall())
    {
        if (GameMode->GetCurrentTurnGolfBall()->IsHoleIn() || GameMode->GetCurrentTurnGolfBall()->IsConceded())
        {
            DoubleParScore += 1;
        }
    }

    bool bDoublePar = CurrentShots >= DoubleParScore;
    if (bDoublePar)
    {
        UE_LOG(LogTemp, Log, TEXT("⛳ Player %d reached double par: Shots=%d, DoublePar=%d"),
            PlayerIndex, CurrentShots, DoubleParScore);
    }

    return bDoublePar;
}

void AGolfPlayer::SetHoleIn(bool bHoleIn)
{
    bIsHoleIn = bHoleIn;
    PlayerInfo.bIsHoleout = bHoleIn;

    if (bIsHoleIn)
    {
        UE_LOG(LogTemp, Log, TEXT("🏆 Player %d set to HoleIn"), PlayerIndex);
       // SetPlayerState(EPlayerState::Player_HoleOut);
    }
}

//이게 먼저임 (볼 상태 바뀔때 호출 됨)
void AGolfPlayer::ProcessShotResult(bool bHoleIn, bool bOutOfBounds, bool bIsConceded)
{
    UE_LOG(LogTemp, Log, TEXT("🎯 Player %d processing shot result: HoleIn=%s, OB=%s, Conceded=%s"),
        PlayerIndex, bHoleIn ? TEXT("Yes") : TEXT("No"), bOutOfBounds ? TEXT("Yes") : TEXT("No"), bIsConceded ? TEXT("Yes") : TEXT("No"));

    bLastShotHoleIn = bHoleIn;
    bShotResultProcessed = false;

    if (AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
    {
        if (GameMode->CurrentGameMode == EGolfGameMode::StrokeMode || GameMode->CurrentGameMode == EGolfGameMode::TrainingMode)
        {
            if (GameMode->PlayerManager && GameMode->PlayerManager->GetPlayerBalls().IsValidIndex(PlayerIndex))
            {
                AGolfBall* Ball = GameMode->PlayerManager->GetPlayerBalls()[PlayerIndex];
                if (IsValid(Ball))
                {
                    if (bHoleIn)
                    {
                        SetHoleIn(true); // 플레이어 상태를 HoleOut으로 변경
                        Ball->SetHoleIn(true); // 볼의 HoleIn 플래그 설정
                        Ball->SetPhysicsState(EPhysicsState::Simulating);
                    }
                    else if (bIsConceded) // ⭐ 수정: AGolfBall에서 넘어온 bIsConceded 값을 직접 사용
                    {
                        SetHoleIn(true); // 플레이어 상태를 HoleOut으로 변경 (컨시드도 홀아웃으로 간주)
                        Ball->SetConceded(true); // 볼의 Conceded 플래그 설정
                        UE_LOG(LogTemp, Log, TEXT("✅ Player %d is conceded."), PlayerIndex);
                    }
                    else if (CheckDoublePar())  //여긴 매번 호출됨
                    {
                        int32 CurrentHole = GameMode->CurrentHole - 1;
                        int32 ParScore = GameMode->GameInfo.SelectedMap.ParScores[CurrentHole];
                        PlayerInfo.ShotCountPerHole[CurrentHole] = ParScore * 2;
                        if (PlayerInfo.HoleScores.IsValidIndex(CurrentHole))
                            PlayerInfo.HoleScores[CurrentHole] = ParScore;
                        else
                            PlayerInfo.HoleScores.Add(ParScore);
                        SetHoleIn(true); // 더블 파로 인한 홀아웃 처리
                        Ball->SetHoleIn(true); // 볼의 HoleIn 플래그 설정
                    }
                }
            }
        }
    }
    SetPlayerState(EPlayerState::Player_Results);
}

void AGolfPlayer::TakeShot(FVector Direction, float Power)
{
    UE_LOG(LogTemp, Warning, TEXT("샷을 할 수 없습니다: 공이 준비 상태가 아님"));
}


void AGolfPlayer::UseMulligan()
{
    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());

    // ✅ 추가: 게임모드 검증
    if (!GameMode)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ UseMulligan: GameMode is null"));
        return;
    }

    int32 LatestPlayerSlotIndex = GameMode->LatestShotSlotIndex;

    // 슬롯 인덱스로 직전 플레이어 찾기
    AGolfPlayer* BeforePlayer = nullptr;

    for (AGolfPlayer* Player : GameMode->PlayerManager->GetPlayers())
    {
        if (LatestPlayerSlotIndex == Player->SlotIndex)
            BeforePlayer = Player;
    }

    AGolfBall* BeforeBall = nullptr;

    if (BeforePlayer)
    {
        BeforeBall = GameMode->FindBall(BeforePlayer->PlayerIndex);
    }


    AGolfBall* CurrentBall = GameMode->GetCurrentTurnGolfBall();
    AGolfPlayerController* PC = Cast<AGolfPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));

    if (BeforePlayer && BeforeBall && CurrentBall)
    {
        //기존에 실행중이던 찬스 지움

		if (GameMode->GetCurrentSlot())
		{
			GameMode->GetCurrentSlot()->SetChance(false, BeforePlayer->PlayerInfo.ShotCountPerHole[GameMode->CurrentHole - 1]);
			GameMode->ParticleManager->StopChanceFX();
		}

        //직전에 친 사람이 자신일 경우
        if (BeforePlayer->PlayerIndex == GameMode->CurrentPlayerIndex)
        {
            UE_LOG(LogTemp, Log, TEXT("🔄 Mulligan Case 1: Player %d using mulligan on own shot"), PlayerIndex);

            //state
            SetPlayerState(EPlayerState::Player_Des);
            CurrentBall->SetBallState(EBallState::Ball_Des);

            // ✅ 개선: BEFOREPos 유효성 검증
            if (BEFOREPos.IsNearlyZero())
            {
                UE_LOG(LogTemp, Warning, TEXT("⚠️ BEFOREPos is zero vector, using current ball location"));
                BEFOREPos = CurrentBall->GetActorLocation();
            }
            CurrentBall->SetBallForceHidden(false);
            CurrentBall->SetBallVisibility(true, true);
            CurrentBall->SetActorLocation(BEFOREPos);

            FVector CurrentHolecupPos = GameMode->MapInfo.HolecupPositions[GameMode->CurrentHole - 1];
            FVector CurrentBallPos = CurrentBall->GetActorLocation();
            float CurrentBallHoleDistance = FVector::Dist(CurrentBallPos, CurrentHolecupPos) / 100.0f;
            float CurrentHeight = CurrentHolecupPos.Z * 0.01f - CurrentBallPos.Z * 0.01f;

            GameMode->StrokeWidgetInstance->UpdateAimInfo(CurrentBallHoleDistance, CurrentHeight);

            PlayerInfo.OnceHoleMulligan = true;
            PlayerInfo.MulliganCount++;

            // ✅ 게임모드 동기화
            GameMode->GameInfo.Players[PlayerIndex].OnceHoleMulligan = true;
            GameMode->GameInfo.Players[PlayerIndex].MulliganCount++;

            PlayerInfo.ShotCountPerHole[GameMode->CurrentHole - 1] =
                FMath::Max(0, PlayerInfo.ShotCountPerHole[GameMode->CurrentHole - 1] - (bLastShotOB ? 3 : 1));
            GameMode->GameInfo.Players[PlayerIndex].ShotCountPerHole[GameMode->CurrentHole - 1] =
                FMath::Max(0, GameMode->GameInfo.Players[PlayerIndex].ShotCountPerHole[GameMode->CurrentHole - 1] - (bLastShotOB ? 3 : 1));

            UE_LOG(LogTemp, Log, TEXT("⚠️ Mulligan -  Player - Hole %d count:[%d]"), GameMode->CurrentHole - 1, GameMode->GameInfo.Players[PlayerIndex].ShotCountPerHole[GameMode->CurrentHole - 1]);

            GameMode->InGameScoreBoardWidgetInstance->UpdateMulliganUse();
 
            //같은 ready일 경우 찬스 안나옴
            if (BeforePlayer->GetPlayerState() == EPlayerState::Player_Ready)
            {
                SafeHandleChanceDisplay(GameMode, BeforeBall);
            }
            BeforePlayer->SetPlayerState(EPlayerState::Player_Ready);
            BeforeBall->SetBallState(EBallState::Ball_Ready);

            if (GameMode->CheckFirstShot())
                GameMode->PlayerManager->SetSensorClub(CR2CLUB_DRIVER);
            else
            {
                FVector HolecupPos = GameMode->MapInfo.HolecupPositions[GameMode->CurrentHole - 1];
                FVector BallPos = CurrentBall->GetActorLocation();
                float Distance = FVector::Dist(BallPos, HolecupPos) / 100.0f;
                if (Distance < 10.0f) // 10미터
                {
                    GameMode->PlayerManager->SetSensorClub(CR2CLUB_PUTTER);
                }
                else
                {
                    GameMode->PlayerManager->SetSensorClub(CR2CLUB_IRON7);
                }
            }              



            GameMode->UpdateMiniMapAimLine();
            GameMode->StrokeWidgetInstance->UpdateMulliganTexture();

            GameMode->GetCurrentSlot()->UpdateStroke(PlayerInfo);
            // Force UI refresh after mulligan (case 1)
            GameMode->UpdateAllPlayerInfoSlots();
            GameMode->HighlightCurrentPlayerSlot(GameMode->CurrentPlayerIndex);
            if (PC && PC->bTerrainGridVisible)
            {
                FTimerHandle TH;
                GetWorld()->GetTimerManager().SetTimer(TH, [this, GameMode]() {
                    AGolfPlayerController* PCt = Cast<AGolfPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
                    if (PCt)
                    {                        
                        PCt->ToggleTerrainGrid();
                    }
                    }, 0.5f, false);
            }

            bLastShotOB = false;

            //티샷일경우
            if (CurrentBall->CheckWasTeeShot())
            {
                //최근 티샷 정보 하나 삭제
                if (CurrentBall->TeeShotDistanceArray.Num() > 0)
                {
                    CurrentBall->TeeShotDistanceArray.Pop();
                    CurrentBall->TeeShotSettlementArray.Pop();
                }
            }
            else
            {
                CurrentBall->ShotInfoArray.RemoveAt(BeforeBall->ShotInfoArray.Num() - 1);
            }

            CurrentBall->CalculateRoundStat();
            PlayerInfo.HoleMulligans[GameMode->CurrentHole - 1] = true;
            GameMode->FindPlayerInfoPtr(SlotIndex)->HoleMulligans[GameMode->CurrentHole - 1] = true;
            UpdateBallPosition(CurrentBall->GetActorLocation());
            GameMode->UpdateBallNamePlateAndMarker();

            UE_LOG(LogTemp, Log, TEXT("✅ Case 1 Mulligan Complete - Player %d"), PlayerIndex);
            return;
        }

        // ✅ 개선: HoleIn 체크 후 로그 추가
        if (BeforePlayer->IsHoleIn())
        {
            UE_LOG(LogTemp, Warning, TEXT("⚠️ Cannot use mulligan: BeforePlayer %d already holed in"),
                BeforePlayer->PlayerIndex);
            return;
        }

        UE_LOG(LogTemp, Log, TEXT("🔄 Mulligan Case 2: Player %d using mulligan on Player %d's shot"),
            PlayerIndex, BeforePlayer->PlayerIndex);

        if (BeforeBall->CheckWasTeeShot())
        {
            SetPlayerState(EPlayerState::Player_Des);
        }
        CurrentBall->SetBallState(EBallState::Ball_Des);

        // ✅ 개선: BEFOREPos 유효성 검증
        if (BeforePlayer->BEFOREPos.IsNearlyZero())
        {
            UE_LOG(LogTemp, Warning, TEXT("⚠️ BeforePlayer's BEFOREPos is zero, using ball location"));
            BeforePlayer->BEFOREPos = BeforeBall->GetActorLocation();
        }
        BeforeBall->SetActorLocation(BeforePlayer->BEFOREPos);

        FVector CurrentHolecupPos = GameMode->MapInfo.HolecupPositions[GameMode->CurrentHole - 1];
        FVector CurrentBallPos = CurrentBall->GetActorLocation();
        float CurrentBallHoleDistance = FVector::Dist(CurrentBallPos, CurrentHolecupPos) / 100.0f;
        float CurrentHeight = CurrentHolecupPos.Z * 0.01f - CurrentBallPos.Z * 0.01f;

        GameMode->StrokeWidgetInstance->UpdateAimInfo(CurrentBallHoleDistance, CurrentHeight);

        FVector BeforeHolecupPos = GameMode->MapInfo.HolecupPositions[GameMode->CurrentHole - 1];
        FVector BeforeBallPos = BeforeBall->GetActorLocation();
        float BeforeBallHoleDistance = FVector::Dist(BeforeHolecupPos, BeforeBallPos) / 100.0f;
        float BeforeHeight = BeforeHolecupPos.Z * 0.01f - BeforeBallPos.Z * 0.01f;

        GameMode->StrokeWidgetInstance->UpdateAimInfo(BeforeBallHoleDistance, BeforeHeight);

        GameMode->CurrentPlayerIndex = BeforePlayer->PlayerIndex;


        // ✅ 개선: 카메라 관리자 유효성 검증
        if (CurrentBall->LinkedCameraManager)
        {
            CurrentBall->LinkedCameraManager->SetTargetBall(BeforeBall);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("⚠️ CurrentBall's LinkedCameraManager is null"));
        }

        GameMode->FindPlayerInfoPtr(BeforePlayer->SlotIndex)->ShotCountPerHole[GameMode->CurrentHole - 1] =
            FMath::Max(0, GameMode->FindPlayerInfoPtr(BeforePlayer->SlotIndex)->ShotCountPerHole[GameMode->CurrentHole - 1] - (BeforePlayer->bLastShotOB ? 3 : 1));

        BeforePlayer->PlayerInfo.ShotCountPerHole[GameMode->CurrentHole - 1] = GameMode->FindPlayerInfoPtr(BeforePlayer->SlotIndex)->ShotCountPerHole[GameMode->CurrentHole - 1];

        BeforePlayer->PlayerInfo.MulliganCount++;
        GameMode->FindPlayerInfoPtr(BeforePlayer->SlotIndex)->MulliganCount++;  // ✅ 추가
        BeforePlayer->PlayerInfo.OnceHoleMulligan = true;                         // ✅ 추가
        GameMode->FindPlayerInfoPtr(BeforePlayer->SlotIndex)->OnceHoleMulligan = true;  // ✅ 추가
        BeforePlayer->bLastShotOB = false;
       

        //바로 위에서 점수 계산하고 ready로 바꾸도록 변경


        if (BeforePlayer->GetPlayerState() == EPlayerState::Player_Ready)
        {
            SafeHandleChanceDisplay(GameMode, BeforeBall);
        }
        BeforePlayer->SetPlayerState(EPlayerState::Player_Ready);
        BeforeBall->SetBallState(EBallState::Ball_Ready);
        BeforeBall->SetBallForceHidden(false);
        BeforeBall->SetBallVisibility(true, true);

        CurrentBall->SetBallForceHidden(false);
        CurrentBall->SetBallVisibility(true, false);


        if (GameMode->CheckFirstShot())
            GameMode->PlayerManager->SetSensorClub(CR2CLUB_DRIVER);
        else
        {
            FVector HolecupPos = GameMode->MapInfo.HolecupPositions[GameMode->CurrentHole - 1];
            FVector BallPos = CurrentBall->GetActorLocation();
            float Distance = FVector::Dist(BallPos, HolecupPos) / 100.0f;
            if (Distance < 10.0f) // 10미터
            {
                GameMode->PlayerManager->SetSensorClub(CR2CLUB_PUTTER);
            }
            else
            {
                GameMode->PlayerManager->SetSensorClub(CR2CLUB_IRON7);
            }
        }

        GameMode->StrokeWidgetInstance->UpdateMulliganTexture();
        GameMode->GetCurrentSlot()->UpdateStroke(BeforePlayer->PlayerInfo);
        // Force UI refresh after mulligan (case 2)
        GameMode->UpdateAllPlayerInfoSlots();
        GameMode->HighlightCurrentPlayerSlot(GameMode->CurrentPlayerIndex);

        GameMode->UpdateMiniMapAimLine();

        if (PC && PC->bTerrainGridVisible)
        {
            FTimerHandle TH;
            GetWorld()->GetTimerManager().SetTimer(TH, [this, GameMode]() {
                AGolfPlayerController* PCt = Cast<AGolfPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
                if (PCt)
                {
                    PCt->ToggleTerrainGrid();
                }
                }, 0.5f, false);
        }


        //티샷일경우
        if (BeforeBall->CheckWasTeeShot())
        {
            //최근 티샷 정보 하나 삭제
            BeforeBall->TeeShotDistanceArray.Pop();
            BeforeBall->TeeShotSettlementArray.Pop();
        }
        else
        {
            BeforeBall->ShotInfoArray.RemoveAt(BeforeBall->ShotInfoArray.Num() - 1);
        }

        BeforePlayer->PlayerInfo.HoleMulligans[GameMode->CurrentHole - 1] = true;
        GameMode->FindPlayerInfoPtr(BeforePlayer->SlotIndex)->HoleMulligans[GameMode->CurrentHole - 1] = true;
        GameMode->UpdateBallNamePlateAndMarker();
        BeforeBall->CalculateRoundStat();
        UpdateBallPosition(BeforeBall->GetActorLocation());
        UE_LOG(LogTemp, Log, TEXT("✅ Case 2 Mulligan Complete - BeforePlayer %d"), BeforePlayer->PlayerIndex);

        // Ensure ready state/UI are applied to the actual current turn player

        return;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ UseMulligan Failed - Invalid pointers"));
        UE_LOG(LogTemp, Error, TEXT("   BeforePlayer: %s"), BeforePlayer ? TEXT("Valid") : TEXT("nullptr"));
        UE_LOG(LogTemp, Error, TEXT("   BeforeBall: %s"), BeforeBall ? TEXT("Valid") : TEXT("nullptr"));
        UE_LOG(LogTemp, Error, TEXT("   CurrentBall: %s"), CurrentBall ? TEXT("Valid") : TEXT("nullptr"));
        UE_LOG(LogTemp, Error, TEXT("   LatestPlayerSlotIndex: %d"), LatestPlayerSlotIndex);

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red,
                TEXT("❌ Mulligan Failed: Invalid players or balls"));
        }
        return;  // ✅ 중요: 조기 반환
    }

    // Case 1/2 handle ready state explicitly; avoid overriding with wrong player
}





bool AGolfPlayer::CheckChance()
{
    FVector BallLocation = FVector::ZeroVector;
    FVector HolecupLocation = FVector::ZeroVector;

    AInGameMode* GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    if (GM)
    {
        BallLocation = GM->GetCurrentTurnGolfBall()->GetActorLocation();
        HolecupLocation = GM->MapInfo.HolecupPositions[GM->CurrentHole - 1];
    }
    float HolecupDistance = FVector::Dist(BallLocation, HolecupLocation);

    if (HolecupDistance > 10 * 100.f)
    {
        return false;
    }

    return true;
}

void AGolfPlayer::UseOK()
{
    if (AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
    {
        // 이전 플레이어를 홀 아웃 처리
        AGolfPlayer* BeforePlayer = GameMode->FindPlayerSlotIndex(GameMode->LatestShotSlotIndex);
        AGolfBall* BeforeBall = BeforePlayer ? GameMode->FindBall(BeforePlayer->PlayerIndex) : nullptr;

        if (BeforePlayer && BeforePlayer->bIsHoleIn)
            return;

        if (BeforePlayer && BeforeBall)
        {
            SetPlayerState(EPlayerState::Player_HoleOut);

            BeforeBall->SetBallState(EBallState::Ball_Des);
            UE_LOG(LogTemp, Log, TEXT("OK 사용: 공 홀아웃 처리"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Ball or Player is null"));
        }
    }
}

// ⭐ 새로 추가: 타이머에 의해 호출될 홀아웃 위젯 숨김 함수 구현
void AGolfPlayer::HideHoleOutWidgetTimed()
{
    UE_LOG(LogTemp, Log, TEXT("⏳ Hiding player result widget for Hole Out due to timer for Player %d."), PlayerIndex);
}

// GolfPlayer.cpp에 구현
void AGolfPlayer::SafeHandleTeeAnimation(AInGameMode* GameMode)
{
    if (!GameMode || GameMode->CurrentHole <= 0) return;

    int32 CurrentHoleIndex = GameMode->CurrentHole - 1;
    if (!GameMode->TeeAnimInstance) return;

    AActor* TeeAnimActor = GameMode->TeeAnimInstance;
    if (!TeeAnimActor || !IsValid(TeeAnimActor)) return;

    USkeletalMeshComponent* SKM = TeeAnimActor->FindComponentByClass<USkeletalMeshComponent>();
    if (!SKM || !IsValid(SKM)) return;

    UAnimSingleNodeInstance* Inst = SKM->GetSingleNodeInstance();
    if (Inst && IsValid(Inst))
    {
        Inst->SetPlaying(false);
        Inst->SetPosition(0.f, false);
    }
}

void AGolfPlayer::SafeHandleStrokeWidget(AInGameMode* GameMode, AGolfBall* Ball, float Distance, float Height)
{
    if (!GameMode || !Ball) return;

    bool bIsStrokeOrTraining = (GameMode->CurrentGameMode == EGolfGameMode::StrokeMode ||
        GameMode->CurrentGameMode == EGolfGameMode::TrainingMode);

    if (!bIsStrokeOrTraining) return;

    // ✅ StrokeWidgetInstance 안전 검증
    if (!GameMode->StrokeWidgetInstance || !IsValid(GameMode->StrokeWidgetInstance))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ StrokeWidgetInstance is null"));
        return;
    }

    try
    {
        //GameMode->StrokeWidgetInstance->SetPositionTip1();
        GameMode->StrokeWidgetInstance->ShowAimInfo(true);
        //GameMode->StrokeWidgetInstance->WBP_Distance->SetVisibility(ESlateVisibility::Collapsed);
        GameMode->StrokeWidgetInstance->UpdateAimInfo(Distance, Height);

        if (GameMode->IsStrokeMode())
        {
            //이미 홀 아웃인 경우 출력 안함 (이어하기)
            if (!IsHoleIn() && !PlayerInfo.bIsHoleout)
            {
             
                GameMode->StrokeWidgetInstance->ShowCanvasAndHideAfterDelay(PlayerInfo.NickName);
            }
        }

        Ball->UpdateCurrentLandType();
        
        if (Ball->CheckTeeShot())
        {
            GameMode->StrokeWidgetInstance->SetLandType(0);
        }
        else
			GameMode->StrokeWidgetInstance->SetLandType((int32)Ball->GetCurrentLandType());

        GameMode->HighlightCurrentPlayerSlot(PlayerIndex);
        GameMode->StrokeWidgetInstance->UpdateMulliganTexture(); // 이전 크래시 원인

        UE_LOG(LogTemp, Log, TEXT("✅ StrokeWidget updated successfully"));
    }
    catch (...)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Exception in SafeHandleStrokeWidget"));
    }
}

void AGolfPlayer::SafeHandleChanceDisplay(AInGameMode* GameMode, AGolfBall* Ball)
{
    if (!GameMode || !Ball) return;

    if (bIsContinue) return;

    // GameInfo와 HolecupPositions 검증
    if (!GameMode->GameInfo.SelectedMap.HolecupPositions.IsValidIndex(GameMode->CurrentHole - 1))
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Invalid HolecupPositions for chance display"));
        return;
    }

    FVector CurrentHolecupPosition = GameMode->GameInfo.SelectedMap.HolecupPositions[GameMode->CurrentHole - 1];
    const float DistSqr = FVector::DistSquared(Ball->GetActorLocation(), CurrentHolecupPosition);
    float RadiusMeter = 15.f * 100.f;

    if (DistSqr <= RadiusMeter * RadiusMeter)
    {
        int32 PlayerCurrentShotCount = PlayerInfo.ShotCountPerHole[GameMode->CurrentHole - 1];

        // 타이머 설정 (안전하게)
        UWorld* World = GetWorld();
        if (World)
        {
            FTimerHandle H1, H2;

            World->GetTimerManager().SetTimer(H1,
                FTimerDelegate::CreateLambda([GameMode, PlayerCurrentShotCount]()
                    {
                        if (GameMode && IsValid(GameMode) && GameMode->ParticleManager)
                        {
                            GameMode->ParticleManager->PlayChanceFX(PlayerCurrentShotCount + 1);
                        }
                    }), 1.5f, false);

            World->GetTimerManager().SetTimer(H2,
                FTimerDelegate::CreateLambda([GameMode, PlayerCurrentShotCount]()
                    {
                        if (GameMode && IsValid(GameMode) && GameMode->GetCurrentSlot())
                        {
                            GameMode->GetCurrentSlot()->SetChance(true, PlayerCurrentShotCount);
                        }
                    }), 3.7f, false);
        }
    }
}

FVector AGolfPlayer::FindFirstShotAimPosition()
{
    UE_LOG(LogTemp, Log, TEXT("🎯 Finding first shot aim position for Player %d"), PlayerIndex);

    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    if (!GameMode)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ FindFirstShotAimPosition: GameMode is null"));
        return FVector::ZeroVector;
    }

    // 현재 홀의 티와 홀컵 위치 가져오기
    int32 CurrentHoleIndex = GameMode->CurrentHole - 1;

    if (!GameMode->MapInfo.TeePositions.IsValidIndex(CurrentHoleIndex) ||
        !GameMode->MapInfo.HolecupPositions.IsValidIndex(CurrentHoleIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Invalid hole index for tee/holecup positions: %d"), CurrentHoleIndex);
        return FVector::ZeroVector;
    }

    FVector TeePosition = GameMode->MapInfo.TeePositions[CurrentHoleIndex];
    FVector HolecupPosition = GameMode->MapInfo.HolecupPositions[CurrentHoleIndex];

    // ⭐⭐⭐ 새로 추가: TeeRotation 가져오기
    FRotator TeeRotation = FRotator::ZeroRotator;
    if (GameMode->TeeRotationArray.IsValidIndex(CurrentHoleIndex))
    {
        TeeRotation = GameMode->TeeRotationArray[CurrentHoleIndex];
        UE_LOG(LogTemp, Log, TEXT("✅ TeeRotation for hole %d: %s"),
            GameMode->CurrentHole, *TeeRotation.ToString());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ TeeRotation not found for hole %d, using tee-to-hole direction"),
            GameMode->CurrentHole);
        // Fallback: 티에서 홀컵 방향 계산
        FVector Direction = (HolecupPosition - TeePosition).GetSafeNormal();
        TeeRotation = Direction.Rotation();
    }

    // 현재 볼 위치 (티 근처)
    AGolfBall* Ball = GameMode->PlayerManager->GetPlayerBalls()[PlayerIndex];
    if (!Ball)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Ball is null for Player %d"), PlayerIndex);
        return FVector::ZeroVector;
    }

    FVector BallLocation = Ball->GetActorLocation();

    // 첫 샷인지 확인
    if (GetCurrentHoleShotCount() > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Not first shot for Player %d, shot count: %d"),
            PlayerIndex, GetCurrentHoleShotCount());
        return HolecupPosition;
    }

    UE_LOG(LogTemp, Log, TEXT("✅ First shot confirmed for Player %d"), PlayerIndex);

    // ⭐⭐⭐ 수정: TeeRotation을 사용하여 안전한 에임 위치 찾기
    FVector OptimalAimPosition = FindSafeAimPositionFromTee(
        BallLocation,           // 공 위치 사용 (티 근처)
        HolecupPosition,
        TeeRotation,            // ⭐ TeeRotation 전달
        5000.0f                 // 50미터
    );

    UE_LOG(LogTemp, Log, TEXT("🎯 First shot aim position found: %s (TeeDir: %s)"),
        *OptimalAimPosition.ToString(), *TeeRotation.Vector().ToString());

    return OptimalAimPosition;
}

FVector AGolfPlayer::FindSafeAimPositionFromTee(
    const FVector& StartPosition,
    const FVector& HolecupPosition,
    const FRotator& TeeRotation,    // ⭐ 추가
    float TargetDistance)
{
    UE_LOG(LogTemp, Log, TEXT("🔍 Finding safe aim position from tee..."));

    // ⭐⭐⭐ 수정: TeeRotation을 사용한 방향 계산
    FVector TeeDirection = TeeRotation.Vector();
    TeeDirection.Z = 0.0f; // 수평 방향만
    TeeDirection.Normalize();

    UE_LOG(LogTemp, Log, TEXT("📐 Using TeeRotation direction: %s (Yaw: %.1f°)"),
        *TeeDirection.ToString(), TeeRotation.Yaw);

    // 홀컵까지의 실제 거리
    float ActualHoleDistance = FVector::Dist(StartPosition, HolecupPosition);

    // ⭐ 티 방향으로 50미터가 홀컵 거리보다 크면 홀컵으로 제한
    float AimDistance = FMath::Min(TargetDistance, ActualHoleDistance * 0.95f);

    if (AimDistance < TargetDistance)
    {
        UE_LOG(LogTemp, Log, TEXT("🎯 Aim distance limited: %.1fm → %.1fm (95%% of hole distance)"),
            TargetDistance / 100.0f, AimDistance / 100.0f);
    }

    // ⭐⭐⭐ 수정: 티 방향으로 계산된 거리만큼 전방
    FVector InitialTargetPosition = StartPosition + (TeeDirection * AimDistance);

    UE_LOG(LogTemp, Log, TEXT("🎯 Initial target position: %s (%.1fm from ball in TeeRotation direction)"),
        *InitialTargetPosition.ToString(), AimDistance / 100.0f);

    // OB 충돌 검사
    bool bHitsOB = CheckOBCollisionAlongPath(StartPosition, InitialTargetPosition);

    if (!bHitsOB)
    {
        UE_LOG(LogTemp, Log, TEXT("✅ Direct path is clear, using initial target"));
        return InitialTargetPosition;
    }

    UE_LOG(LogTemp, Warning, TEXT("⚠️ Direct path hits OB, finding avoidance position"));

    // ⭐⭐⭐ 수정: OB 회피 시에도 TeeDirection 기준으로 계산
    FVector AvoidancePosition = CalculateOBAvoidancePosition(
        StartPosition,
        HolecupPosition,
        TeeDirection,   // ⭐ TeeDirection 사용
        AimDistance
    );

    return AvoidancePosition;
}

bool AGolfPlayer::CheckOBCollisionAlongPath(const FVector& StartPos, const FVector& EndPos, int32 SampleCount)
{
    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    if (!GameMode)
    {
        return false;
    }

    AGolfBall* Ball = GameMode->PlayerManager->GetPlayerBalls()[PlayerIndex];
    if (!Ball)
    {
        return false;
    }

    FVector Direction = (EndPos - StartPos).GetSafeNormal();
    float PathDistance = FVector::Dist(StartPos, EndPos);
    float StepSize = PathDistance / SampleCount;

    UE_LOG(LogTemp, VeryVerbose, TEXT("🔍 Checking OB collision along path: %d samples, step size: %.1fcm"),
        SampleCount, StepSize);

    for (int32 i = 1; i <= SampleCount; ++i)
    {
        FVector SamplePosition = StartPos + Direction * (StepSize * i);

        // 게임모드의 OB 영역 체크 사용
        if (GameMode->IsPointInOBArea(SamplePosition))
        {
            UE_LOG(LogTemp, Warning, TEXT("🚫 OB detected at sample %d: %s"), i, *SamplePosition.ToString());
            return true;
        }        
    }

    UE_LOG(LogTemp, Log, TEXT("✅ Path is clear of OB"));
    return false;
}

FVector AGolfPlayer::CalculateOBAvoidancePosition(
    const FVector& StartPos,
    const FVector& HolecupPos,
    const FVector& ProblemDirection,  // TeeDirection이 전달됨
    float Distance)
{
    UE_LOG(LogTemp, Log, TEXT("🔄 Calculating OB avoidance on STRAIGHT LINE (TeeDirection only)..."));

    // ⭐ 1m(100cm) 단위 선형 후진: Distance → 100cm (직선상만!)
    for (float TestDistance = Distance; TestDistance >= 100.0f; TestDistance -= 100.0f)
    {
        FVector TestPosition = StartPos + (ProblemDirection * TestDistance);

        // 경로 전체 OB 검사 (CheckOBCollisionAlongPath 사용)
        if (!CheckOBCollisionAlongPath(StartPos, TestPosition, 10))  // 10 샘플로 정밀 검사
        {
            UE_LOG(LogTemp, Log, TEXT("✅ Found safe STRAIGHT position at %.1fm (1m step)"),
                TestDistance / 100.0f);
            return TestPosition;
        }

        // 5m 간격 로그 (성능 위해)
        if (FMath::IsNearlyEqual(FMath::Fmod(TestDistance, 500.0f), 0.0f))
        {
            UE_LOG(LogTemp, VeryVerbose, TEXT("⏳ Testing straight %.1fm... (OB hit)"), TestDistance / 100.0f);
        }
    }

    // 볼 위치 자체 체크 (최후)
    if (!CheckOBCollisionAlongPath(StartPos, StartPos, 1))  // 0m 경로 (항상 true지만)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Fallback to ball position (0m straight)"));
        return StartPos;
    }

    // 최종 fallback: 10m 직선
    FVector FallbackPosition = StartPos + (ProblemDirection * 1000.0f);
    UE_LOG(LogTemp, Error, TEXT("❌ No safe straight found, fallback to 10m straight: %s"),
        *FallbackPosition.ToString());
    return FallbackPosition;
}


bool AGolfPlayer::EnableMulligan()
{
    // 1. 이미 이 홀에서 사용했는지 확인
    AInGameMode* GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());

    if (PlayerInfo.HoleMulligans[GM->CurrentHole - 1])
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Mulligan already used on hole %d for Player %d"),
            PlayerInfo.HoleCount, PlayerIndex);
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("✅ Mulligan enabled for Player %d"),
        PlayerIndex);
    return true;
}
