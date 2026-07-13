#include "GolfPlayerManager.h"
#include "TerraParkGameInstance.h"
#include "CameraManager.h"     // CameraManager 접근을 위해 필요 (이미 포함되어 있을 수 있음)
#include "Camera/CameraComponent.h"
#include "Engine/World.h"      // GetWorld() 등 유틸리티
#include "Kismet/KismetMathLibrary.h"  // FMath 유틸리티
#include "BallParticleManager.h"
#include "BallSweepTraceSubsystem.h"
#include "BoomLine.h"
#include "GolfPlayer.h"
#include "GolfPlayerController.h"
#include "GolfBall.h"
#include "InGameMode.h"
#include "CameraManager.h"
#include "ReadyBillboard.h"
#include "SoundManager.h"
#include "Components/BillboardComponent.h"
#include "Blueprint/UserWidget.h"
#include "PlayerInfoSlotWidget.h"
#include "ShotCinematicComponent.h"
#include "GolfPlayerController.h"
#include "ParkDay/BallNamePlateComponent.h"
#include "ParkDay/Widgets/BallNamePlateWidget.h"
#include "ParkDay/Widgets/InGameScoreBoardStatWidget.h"
#include "ParkDay/Widgets/InGameScoreBoardWidget.h"
#include "ParkDay/Widgets/InGameScoreBoardLineWidget.h"
#include "ParkDay/Widgets/InGameScoreBoardStatLineWidget.h"

UGolfPlayerManager::UGolfPlayerManager()
{
    SensorManager = nullptr;
    bSensorReady = false;
    CurrentActivePlayerIndex = -1;
}

void UGolfPlayerManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
  
    ShutdownPlayerManager();
    Super::EndPlay(EndPlayReason);
    UE_LOG(LogTemp, Log, TEXT("✅ UGolfPlayerManager::EndPlay --  "));

}

void UGolfPlayerManager::BeginDestroy()
{
    ShutdownPlayerManager();
    StopSensorReadyCheck();

    // 기타 타이머들도 정리
    if (GetWorld())
    {
        if (GetWorld()->GetTimerManager().IsTimerActive(DelayedReadyTimer))
        {
            GetWorld()->GetTimerManager().ClearTimer(DelayedReadyTimer);
        }
        // 다른 타이머들도 필요하면 정리...
    }

    Super::BeginDestroy();
}
void UGolfPlayerManager::ShutdownPlayerManager()
{
    if (bShuttingDown)
    {
        return;
    }

    bShuttingDown = true;
    bSensorReady = false;
    CurrentActivePlayerIndex = -1;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(SensorCheckTimer);
        World->GetTimerManager().ClearTimer(SensorCheckTimerHandle);
        World->GetTimerManager().ClearTimer(DelayedReadyTimer);
        World->GetTimerManager().ClearTimer(TH);
        World->GetTimerManager().ClearTimer(TH2);
    }

    if (SensorManager && IsValid(SensorManager))
    {
        SensorManager->OnShotDetected.RemoveAll(this);
        SensorManager->OnShotDetectedEx.RemoveAll(this);
        SensorManager->OnBallReady.RemoveAll(this);
        SensorManager->OnSensorStatusChanged.RemoveAll(this);
        SensorManager->StopSensorOperation();
        SensorManager->ShutdownSensor();
        SensorManager->Destroy();
        SensorManager = nullptr;
    }

    for (AGolfBall*& Ball : PlayerBalls)
    {
        if (IsValid(Ball))
        {
            Ball->Destroy();
        }
        Ball = nullptr;
    }

    for (AGolfPlayer*& Player : Players)
    {
        if (IsValid(Player))
        {
            Player->Destroy();
        }
        Player = nullptr;
    }

    PlayerBalls.Empty();
    Players.Empty();
    PlayerOrder.Empty();
}



void UGolfPlayerManager::InitializePlayers(const TArray<FPlayerInfo>& PlayerInfos, UWorld* World, const FMapInfo& MapInfo, int32 CurrentHole)
{

    for (int32 i = 0; i < Players.Num(); i++)
    {
        Players[i]->Destroy();
        PlayerBalls[i]->Destroy();
    }

    Players.Empty();
    PlayerBalls.Empty();
    PlayerOrder.Empty();

    UE_LOG(LogTemp, Log, TEXT("? InitializePlayers --  CurrentHole - %d"), CurrentHole);

    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("? World is null in InitializePlayers"));
        return;
    }

    PlayerController = Cast<AGolfPlayerController>(World->GetFirstPlayerController());
    if (!PlayerController)
    {
        UE_LOG(LogTemp, Error, TEXT("? Failed to get AGolfPlayerController"));
        return;
    }

    CameraManager = World->SpawnActor<ACameraManager>(ACameraManager::StaticClass());
    if (!CameraManager)
    {
        UE_LOG(LogTemp, Error, TEXT("? Failed to spawn CameraManager"));
        return;
    }

    // GameMode 참조
    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    if (!GameMode)
    {
        UE_LOG(LogTemp, Error, TEXT("? Failed to get InGameMode in InitializePlayers"));
        return;
    }

    for (int32 i = 0; i < PlayerInfos.Num(); i++)
    {
		if (PlayerInfos[i].bIsPendingDelete)
		{
			continue;
		}

        AGolfPlayer* Player = World->SpawnActor<AGolfPlayer>(AGolfPlayer::StaticClass());
        if (!IsValid(Player))
        {
            UE_LOG(LogTemp, Error, TEXT("? Failed to spawn Player %d"), i);
            continue;
        }

        Player->SetPlayerInfo(PlayerInfos[i]);
        Player->SetPlayerState(EPlayerState::Player_Des);
        Player->SlotIndex = PlayerInfos[i].SlotIndex;
        Player->PlayerIndex = i;
        float BeforePosX = PlayerInfos[i].BeforePosX;
        float BeforePosY = PlayerInfos[i].BeforePosY;
        float BeforePosZ = PlayerInfos[i].BeforePosZ;
        Player->BEFOREPos = FVector(BeforePosX, BeforePosY, BeforePosZ);
        Players.Add(Player);

        if (GameMode->GameInfo.GameOptions.SelectCourse == 1 && Player->PlayerInfo.HoleScores.Num() <= 0)
        {
            for (int32 j = 0; j < 9; j++)
            {
                Player->PlayerInfo.HoleScores.Add(100);     //스코어보드 상에서 없는 점수처리
            }
            for (int32 k = 9; k < GameMode->GameInfo.CurrentHole - 1; k++)
            {
                Player->PlayerInfo.HoleScores.Add(0);
            }
        }

        AGolfBall* Ball = World->SpawnActor<AGolfBall>(GameMode->BlueprintObjectsMap.Find(TEXT("Ball"))->Get());
        Ball->Tags.Add(FName("BirdEnemy"));
        if (!IsValid(Ball))
        {
            UE_LOG(LogTemp, Error, TEXT("? Failed to spawn GolfBall %d"), i);
            continue;
        }

        //홀 아웃 체크
        int32 CurrentHoleIndex = GameMode->CurrentHole - 1;

        if (GameMode->GameInfo.bIsRoundEnd == false)
        {
            if (Player->PlayerInfo.bIsHoleout)
            {
                Ball->SetHoleIn(true);
                Player->SetHoleIn(true);
                Player->bIsContinue = true;
                Player->SetPlayerState(EPlayerState::Player_HoleOut);
            }
        }

        float BallPosX = GameMode->GameInfo.Players[Player->PlayerIndex].BallPosX;
        float BallPosY = GameMode->GameInfo.Players[Player->PlayerIndex].BallPosY;
        float BallPosZ = GameMode->GameInfo.Players[Player->PlayerIndex].BallPosZ;

        Ball->SetBallState(EBallState::Ball_Init);
        Ball->SetBallForceHidden(true);

        if (FVector(BallPosX, BallPosY, BallPosZ).IsZero())
        {
            // ⭐ TeePositions 유효성 체크
            if (MapInfo.TeePositions.IsValidIndex(CurrentHole - 1))
                Ball->SetActorLocation(MapInfo.TeePositions[CurrentHole - 1] + FVector(0.f, 0.f, 5.f));
            else
                UE_LOG(LogTemp, Error, TEXT("❌ TeePositions empty! hole=%d size=%d"),
                    CurrentHole, MapInfo.TeePositions.Num());
        }
        else
        {
            if (!GameMode->GameInfo.Players[Player->PlayerIndex].bIsHoleout)
            {
                Ball->SetActorLocation(FVector(BallPosX, BallPosY, BallPosZ));
            }
            else
            {
                Ball->SetActorLocation(MapInfo.TeePositions.IsValidIndex(CurrentHole)
                    ? MapInfo.TeePositions[CurrentHole] + FVector(0.f, 0.f, 5.f)
                    : FVector::ZeroVector);
            }
        }

        if (i != 0)
        {
            Ball->SetBallCollisionEnabled(false);
        }

        Ball->SetBallVisibility(false, false);

        if (Ball->LoadPhysicsConfigFromFile())
        {
            UE_LOG(LogTemp, Log, TEXT("? Golf ball config loaded: %s"), *Ball->GetName());
        }

        Ball->OwningPlayerIndex = i;
        PlayerBalls.Add(Ball);
        PlayerOrder.Add(i);

        Ball->OnBallStateChangedInternal.AddDynamic(this, &UGolfPlayerManager::OnPlayerBallStateChanged);

        UBallSweepTraceSubsystem* Subsystem = GetWorld()->GetSubsystem<UBallSweepTraceSubsystem>();

        if (Subsystem->IsRegistered(Ball))
            Subsystem->UnregisterActor(Ball);
        else
        {
            FBallSweepTraceConfig Config;
            Subsystem->RegisterActor(Ball, Config);
        }

        UE_LOG(LogTemp, Log, TEXT("Player %d initialized: %s, Ball at %s (Hidden)"),
            i, *PlayerInfos[i].NickName, *Ball->GetActorLocation().ToString());

    }

    if (PlayerBalls.Num() > 0 && IsValid(CameraManager))
    {
        int32 TargetIndex = 0;
        if (GameMode->bIsContinueGame)
        {
            TargetIndex = FMath::Clamp(GameMode->GameInfo.CurrentPlayerIndex, 0, PlayerBalls.Num() - 1);
        }

        // Set CurrentPlayerIndex first because CameraManager::SetTargetBall() calls GameMode::CheckFirstShot().
        GameMode->CurrentPlayerIndex = TargetIndex;
        GameMode->GameInfo.CurrentPlayerIndex = TargetIndex;

        CameraManager->SetTargetBall(PlayerBalls[TargetIndex]);
        CameraManager->ChangeCameraMode(ECameraMode::Ready);
        PlayerController->CameraManager = CameraManager;

        UE_LOG(LogTemp, Log, TEXT("? Camera-Ball sync established for Player %d"), TargetIndex);
    }

    // CR2 센서 매니저 초기화
    InitializeSensorManager();
    SensorManager->StartSensorOperation();

    UE_LOG(LogTemp, Log, TEXT("? Initialized %d players with camera sync support"), Players.Num());
}


void UGolfPlayerManager::RemovePlayerBySlotIndex(int32 TargetSlotIndex)
{
    int32 PreserveSlotIndex = INDEX_NONE;
    if (AInGameMode* GM = GetInGameMode())
    {
        if (Players.IsValidIndex(GM->CurrentPlayerIndex) && IsValid(Players[GM->CurrentPlayerIndex]))
        {
            PreserveSlotIndex = Players[GM->CurrentPlayerIndex]->PlayerInfo.SlotIndex;
        }
    }

    // 배열에서 조건에 맞는 플레이어 제거
    int32 RemovedCount = Players.RemoveAll([&](AGolfPlayer* Player)
        {
            if (Player && Player->PlayerInfo.SlotIndex == TargetSlotIndex)
            {
                Player->Destroy();
                return true;
            }
            return false;
        }
    );

    int32 RemovedPlayerInfoCount = GetInGameMode()->GameInfo.Players.RemoveAll([&](FPlayerInfo PlayerInfo)
        {
            if (PlayerInfo.SlotIndex == TargetSlotIndex)
            {
                return true;
            }
            return false;
        }
    );

    int32 RemovedPlayerInfoSlotCount = GetInGameMode()->PlayerInfoSlotWidgets.RemoveAll([&](UPlayerInfoSlotWidget* PlayerInfoSlot)
        {
            if (PlayerInfoSlot->OwningPlayerSlotIndex == TargetSlotIndex)
            {
                if (IsValid(PlayerInfoSlot))
                {
                    PlayerInfoSlot->RemoveFromParent();
                }
                return true;
            }
            return false;
        }
    );

    if (RemovedCount > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("Removed %d playerInfo(s) with SlotIndex = %d"), RemovedCount, TargetSlotIndex);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("No playerInfo found with SlotIndex = %d"), TargetSlotIndex);
    }

    RebuildPlayerIndicesAndOrder(PreserveSlotIndex);
}

void UGolfPlayerManager::RemoveBallBySlotIndex(int32 TargetSlotIndex)
{
    int32 TargetPlayerIndex = INDEX_NONE;
    for (int32 i = 0; i < Players.Num(); ++i)
    {
        if (Players[i] && Players[i]->PlayerInfo.SlotIndex == TargetSlotIndex)
        {
            TargetPlayerIndex = i;
            break;
        }
    }

    if (TargetPlayerIndex == INDEX_NONE)
    {
        UE_LOG(LogTemp, Warning, TEXT("No player found with SlotIndex = %d (RemoveBallBySlotIndex)"), TargetSlotIndex);
        return;
    }

    int32 RemovedCount = PlayerBalls.RemoveAll([&](AGolfBall* Ball)
        {
            if (Ball && Ball->OwningPlayerIndex == TargetPlayerIndex)
            {
                Ball->Destroy();
                return true;
            }
            return false;
        }
    );

    if (RemovedCount > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("Removed %d playerball(s) with SlotIndex = %d"), RemovedCount, TargetSlotIndex);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("No playerball found with SlotIndex = %d"), TargetSlotIndex);
    }
}

void UGolfPlayerManager::ChangeNickName(AGolfPlayer* Player, FString NickName)
{
    if (AInGameMode* GM = Cast<AInGameMode>(GetInGameMode()))
    {
        Player->PlayerInfo.NickName = NickName;
        GM->FindPlayerInfoPtr(Player->SlotIndex)->NickName = NickName;
        GM->FindPlayerInfoSlot(Player->SlotIndex, Player->PlayerIndex)->UpdateNickName(NickName);
        GM->InGameScoreBoardWidgetInstance->UpdateScoreBoard();
        GM->InGameScoreBoardStatWidgetInstance->UpdateScoreBoardStats();
        //GM->InGameScoreBoardWidgetInstance->FindPlayerLine(Player->SlotIndex)->UpdateNickName();
        //GM->InGameScoreBoardStatWidgetInstance->FindStatLineBySlotIndex(Player->SlotIndex)->TextBlock_Name->SetText(FText::FromString(NickName));
    }
}

void UGolfPlayerManager::AddPlayerInfoSlot(AGolfPlayer* Player, FPlayerInfo PlayerInfo)
{
    AInGameMode* GM = GetInGameMode();
    UPanelWidget* PlayerSlotsContainer = Cast<UPanelWidget>(GM->StrokeWidgetInstance->GetWidgetFromName(TEXT("VerticalBox_PlayerList"))); // WBP_InGame의 VerticalBox 변수명

    if (GM->PlayerInfoSlotWidgetClass)
    {
        AGolfPlayerController* PC = Cast<AGolfPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
        UPlayerInfoSlotWidget* PlayerSlot = CreateWidget<UPlayerInfoSlotWidget>(PC, GM->PlayerInfoSlotWidgetClass);
        if (PlayerSlot)
        {
            float CurrentPlayerDistanceToHole = 0.0f;
            AGolfBall* PlayerBall = GetPlayerBalls().IsValidIndex(GM->PlayerManager->GetPlayers().Num() - 1) ? GetPlayerBalls()[GM->PlayerManager->GetPlayers().Num() - 1] : nullptr;
            if (IsValid(PlayerBall) && GM->MapInfo.HolecupPositions.IsValidIndex(GM->CurrentHole - 1))
            {
                CurrentPlayerDistanceToHole = FVector::Dist(PlayerBall->GetActorLocation(), GM->MapInfo.HolecupPositions[GM->CurrentHole - 1]);
            }
            PlayerSlot->OwningPlayerIndex = GM->PlayerManager->GetPlayers().Num() - 1;
            PlayerSlot->OwningPlayerSlotIndex = PlayerInfo.SlotIndex;
            PlayerSlot->DisplayIndex = Player->PlayerIndex;
            PlayerSlot->SetPlayerInfo(PlayerInfo, GM->CurrentHole, GM->PlayerManager->GetPlayers().Num() - 1, CurrentPlayerDistanceToHole);

            if (!GM->CheckTeeShotCountIsZero())
                PlayerSlot->UpdatePlayerStateDisplay(EPlayerState::Player_HoleOut);
            else
                PlayerSlot->UpdatePlayerStateDisplay(EPlayerState::Player_Des);

            GM->PlayerInfoSlotWidgets.Add(PlayerSlot);

            if (PlayerSlotsContainer)
            {
                TArray<UPlayerInfoSlotWidget*> OrderedSlots;
                for (AGolfPlayer* PlayerIter : GM->PlayerManager->GetPlayers())
                {
                    if (!IsValid(PlayerIter)) continue;
                    if (UPlayerInfoSlotWidget* const* FoundSlot = GM->PlayerInfoSlotWidgets.FindByPredicate(
                        [&](UPlayerInfoSlotWidget* Slot)
                        {
                            return Slot && Slot->OwningPlayerSlotIndex == PlayerIter->PlayerInfo.SlotIndex;
                        }))
                    {
                        OrderedSlots.Add(*FoundSlot);
                    }
                }
                for (UPlayerInfoSlotWidget* SlotWidget : GM->PlayerInfoSlotWidgets)
                {
                    if (SlotWidget && !OrderedSlots.Contains(SlotWidget))
                    {
                        OrderedSlots.Add(SlotWidget);
                    }
                }
                GM->PlayerInfoSlotWidgets = OrderedSlots;
                PlayerSlotsContainer->ClearChildren();
                for (UPlayerInfoSlotWidget* SlotWidget : GM->PlayerInfoSlotWidgets)
                {
                    PlayerSlotsContainer->AddChild(SlotWidget);
                }
            }
        }
    }

    AGolfBall* Ball = GM->FindBall(Player->PlayerIndex);
    GM->MiniMapWidget->AddPlayerToMiniMap(Player->PlayerIndex, GM->MapInfo.TeePositions[GM->CurrentHole - 1], Ball->GetBallColor());
}

void UGolfPlayerManager::RemovePlayerInfoSlot(FPlayerInfo PlayerInfo)
{
    AInGameMode* GM = GetInGameMode();
    UPanelWidget* PlayerSlotsContainer = Cast<UPanelWidget>(GM->StrokeWidgetInstance->GetWidgetFromName(TEXT("VerticalBox_PlayerList"))); // WBP_InGame의 VerticalBox 변수명

        //for (int32 i = 0; i < Players.Num(); i++)
        //{
        //    if (Players[i]->bIsPendingDelete)
        //        continue;

        //    Players[i]->PlayerIndex = i;
        //    PlayerBalls[i]->OwningPlayerIndex = i;
        //}

    for (int32 i = GM->PlayerInfoSlotWidgets.Num() - 1; i >= 0; --i)
    {
        UPlayerInfoSlotWidget* SlotWidget = GM->PlayerInfoSlotWidgets[i];
        if (!IsValid(SlotWidget))
        {
            GM->PlayerInfoSlotWidgets.RemoveAt(i);
            continue;
        }

        if (SlotWidget->OwningPlayerSlotIndex == PlayerInfo.SlotIndex)
        {
            if (PlayerSlotsContainer)
            {
                PlayerSlotsContainer->RemoveChild(SlotWidget);
            }
            else
            {
                SlotWidget->RemoveFromParent();
            }

            GM->PlayerInfoSlotWidgets.RemoveAt(i);
            return;
        }
    }
}

void UGolfPlayerManager::InGameAddPlayer(UObject* WorldContextObject, FPlayerInfo PlayerInfo)
{
    if (AInGameMode* GM = Cast<AInGameMode>(GetInGameMode()))
    {
        //기본 점수 세팅
        if (GM->GameInfo.GameOptions.SelectCourse == 1)
        {
            if (PlayerInfo.HoleScores.Num() <= 0)
            {
                for (int32 j = 0; j < 9; j++)
                {
                    PlayerInfo.HoleScores.Add(100);     //스코어보드 상에서 없는 점수처리
                }
            }
        }

        PlayerInfo.HoleCount = GM->CurrentHole;
        PlayerInfo.BallPosX = GM->MapInfo.TeePositions[GM->CurrentHole - 1].X;
        PlayerInfo.BallPosY = GM->MapInfo.TeePositions[GM->CurrentHole - 1].Y;
        PlayerInfo.BallPosZ = GM->MapInfo.TeePositions[GM->CurrentHole - 1].Z + 5.f;

        for (bool Mulligan : PlayerInfo.HoleMulligans)
        {
            Mulligan = false;
        }

        AGolfPlayer* FoundPlayer = nullptr;

        if (AGolfPlayer* const* Found = Players.FindByPredicate([&](AGolfPlayer* Player)
            {
                return Player && Player->PlayerInfo.SlotIndex == PlayerInfo.SlotIndex;
            }))
        {
            FoundPlayer = *Found; // 찾았을 때만 대입
        }
            
            //=================기존의 같은 슬롯의 플레이어인경우==============
			if (FoundPlayer)
			{
                //================이번 홀에서 삭제된 플레이어인 경우==================
				if (FoundPlayer->bIsPendingDelete)
				{
                    FoundPlayer->bIsPendingDelete = false;
                    FoundPlayer->PlayerInfo.bIsPendingDelete = false;

                    //==한 번이라도 친 경우 이번 홀까지 0으로 채움 아닌경우 이번 홀 전까지만
                    int32 FirstShot = GM->CheckTeeShotCountIsZero() ? 1 : 0;

                    int32 StartHoleNum = GM->GameInfo.GameOptions.SelectCourse % 2 * 9;
                    for (int32 i = StartHoleNum; i < GM->CurrentHole - FirstShot; i++)
                    {
                        PlayerInfo.ShotCountPerHole[i] = GM->GameInfo.SelectedMap.ParScores[i];
                        PlayerInfo.ShotCount += PlayerInfo.ShotCountPerHole[i];
                        PlayerInfo.HoleScores.Add(0);
                    }
                    FPlayerInfo* InfoPtr = GM->FindOrAddPlayerInfo(PlayerInfo);
                    if (InfoPtr)
                    {
                        InfoPtr->HoleScores = PlayerInfo.HoleScores;
                        InfoPtr->ShotCountPerHole = PlayerInfo.ShotCountPerHole;
                        InfoPtr->ShotCount = PlayerInfo.ShotCount;
                    }

                    //티샷을 한 번이라도 한 경우
                    if (!GM->CheckTeeShotCountIsZero())
                    {
                        //홀 아웃 처리
                        FoundPlayer->SetPlayerInfo(PlayerInfo);
                        FoundPlayer->SlotIndex = PlayerInfo.SlotIndex;
                        FoundPlayer->bIsRuntimeAdded = true;
                        FoundPlayer->SetHoleIn(true);
                        FoundPlayer->PlayerInfo.bIsHoleout = true;
                        GM->FindBall(FoundPlayer->PlayerIndex)->SetHoleIn(true);
                        FoundPlayer->SetPlayerState(EPlayerState::Player_HoleOut);
                    }
                    else // ========이번 홀에 한 번이라도 샷이 없던 경우==========
                    {
                        FoundPlayer->SetPlayerInfo(PlayerInfo);
                        FoundPlayer->SlotIndex = PlayerInfo.SlotIndex;
                        FoundPlayer->SetHoleIn(false);
                        FoundPlayer->PlayerInfo.bIsHoleout = false;
                        GM->FindBall(FoundPlayer->PlayerIndex)->SetHoleIn(false);
                        FoundPlayer->SetPlayerState(EPlayerState::Player_Des);
                    }

                    GM->SyncPlayerInfosToGameInfo();

                    AddPlayerInfoSlot(FoundPlayer, PlayerInfo);


                    if (FoundPlayer->PlayerInfo.NickName != PlayerInfo.NickName)
                    {
                        ChangeNickName(FoundPlayer, PlayerInfo.NickName);
                    }
                    GM->UpdateAllPlayerInfoSlots();
                    GM->MiniMapWidget->AddPlayerToMiniMap(FoundPlayer->PlayerIndex, GM->MapInfo.TeePositions[GM->CurrentHole-1], GM->FindBall(FoundPlayer->PlayerIndex)->GetBallColor());
                }
                else //===============추가/삭제가 아니고 그냥 바로 닉네임 변경 인경우==============
                {
                    ChangeNickName(FoundPlayer, PlayerInfo.NickName);
                }
            }
            else   //============아예 새로 추가인 경우 (다른 슬롯의 플레이어인경우)=================
            {
                AGolfPlayer* Player = WorldContextObject->GetWorld()->SpawnActor<AGolfPlayer>(AGolfPlayer::StaticClass());
               
                //==한 번이라도 친 경우 이번 홀까지 0으로 채움 아닌경우 이번 홀 전까지만
                int32 FirstShot = GM->CheckTeeShotCountIsZero() ? 1 : 0;

                int32 StartHoleNum = GM->GameInfo.GameOptions.SelectCourse % 2 * 9;
                for (int32 i = StartHoleNum; i < GM->CurrentHole - FirstShot; i++)
                {
                    PlayerInfo.ShotCountPerHole[i] = GM->GameInfo.SelectedMap.ParScores[i];
                    PlayerInfo.ShotCount += PlayerInfo.ShotCountPerHole[i];
                    PlayerInfo.HoleScores.Add(0);
                }
                FPlayerInfo* InfoPtr = GM->FindOrAddPlayerInfo(PlayerInfo);
                if (InfoPtr)
                {
                    InfoPtr->HoleScores = PlayerInfo.HoleScores;
                    InfoPtr->ShotCountPerHole = PlayerInfo.ShotCountPerHole;
                    InfoPtr->ShotCount = PlayerInfo.ShotCount;
                }

                Player->SetPlayerInfo(PlayerInfo);
                Player->SlotIndex = PlayerInfo.SlotIndex;
                Player->bIsRuntimeAdded = true;

                GM->PlayerManager->AddPlayer(Player);

                Player->PlayerIndex = GM->PlayerManager->GetPlayers().Num() - 1;

                AGolfBall* Ball = WorldContextObject->GetWorld()->SpawnActor<AGolfBall>(GM->BlueprintObjectsMap.Find(TEXT("Ball"))->Get());
                Ball->SetBallState(EBallState::Ball_Des);

                Ball->Tags.Add(FName("BirdEnemy"));
                if (!IsValid(Ball))
                {
                    return;
                }

                if (!GM->CheckTeeShotCountIsZero())
                {
                    Player->SetHoleIn(true);
                    Player->PlayerInfo.bIsHoleout = true;
                    Player->SetPlayerState(EPlayerState::Player_HoleOut);
                    Ball->SetHoleIn(true);
                }
                else
                {
                    Player->SetHoleIn(false);
                    Player->bIsRuntimeAdded = false;
                    Player->PlayerInfo.bIsHoleout = false;
                    Player->SetPlayerState(EPlayerState::Player_Des);
                    Ball->SetHoleIn(false);
                }

                GM->SyncPlayerInfosToGameInfo();

                //=====아예 처음 생성하는 플레이어 공 위치 설정=====
                Ball->SetActorLocation(GM->MapInfo.TeePositions[GM->CurrentHole - 1] + FVector(0.f, 0.f, 5.f));
                Ball->SetBallForceHidden(true);
                Ball->SetBallCollisionEnabled(false);
                Ball->SetBallVisibility(false, false);

                if (Ball->LoadPhysicsConfigFromFile())
                {
                    UE_LOG(LogTemp, Log, TEXT("? Golf ball config loaded: %s"), *Ball->GetName());
                }

                Ball->OwningPlayerIndex = GM->PlayerManager->GetPlayers().Num() - 1;
                GM->PlayerManager->AddBall(Ball);
                GM->PlayerManager->PlayerOrder.Add(GM->PlayerManager->GetPlayers().Num() - 1);

                Ball->OnBallStateChangedInternal.AddDynamic(this, &UGolfPlayerManager::OnPlayerBallStateChanged);
                //Ball State 변하는 델리게이트 인식
                GM->ReBindBallEvents();

                UBallSweepTraceSubsystem* Subsystem = GetWorld()->GetSubsystem<UBallSweepTraceSubsystem>();
                if (Subsystem->IsRegistered(Ball))
                    Subsystem->UnregisterActor(Ball);
                else
                {
                    FBallSweepTraceConfig Config;
                    Subsystem->RegisterActor(Ball, Config);
                }

                AddPlayerInfoSlot(Player, PlayerInfo);

                GM->MiniMapWidget->AddPlayerToMiniMap(Player->PlayerIndex, GM->MapInfo.TeePositions[GM->CurrentHole - 1], Ball->GetBallColor());
                GM->UpdateBallNamePlateAndMarker();
                GM->UpdateAllPlayerInfoSlots();
            }
    }
}

void UGolfPlayerManager::InGameRemovePlayer(UObject* WorldContextObject, FPlayerInfo PlayerInfo)
{
    if (AInGameMode* GM = Cast<AInGameMode>(GetInGameMode()))
    {
        AGolfPlayer* FoundPlayer = nullptr;

        if (AGolfPlayer* const* Found = Players.FindByPredicate([&](AGolfPlayer* Player)
            {
                return Player && Player->PlayerInfo.SlotIndex == PlayerInfo.SlotIndex;
            }))
        {
            FoundPlayer = *Found; // 찾았을 때만 대입
        }

		if (FoundPlayer)
		{
			FoundPlayer->bIsPendingDelete = true;
            FoundPlayer->PlayerInfo.bIsPendingDelete = true;
            FoundPlayer->SetHoleIn(true);
            GM->PlayerManager->GetPlayers()[FoundPlayer->PlayerIndex]->PlayerInfo.bIsHoleout = true;
            FoundPlayer->PlayerInfo.bIsHoleout = true;

            GM->SyncPlayerInfosToGameInfo();

            AGolfBall* Ball = GM->FindBall(FoundPlayer->PlayerIndex);

            if (Ball)
            {
                Ball->SetHoleIn(true);
                Ball->SetBallVisibility(false);
                Ball->SetBallState(EBallState::Ball_Des);
            }
            GM->UpdateBallNamePlateAndMarker();
		}

        RemovePlayerInfoSlot(PlayerInfo);
    }
}

void UGolfPlayerManager::UpdateGameInfoBallPos()
{
    AInGameMode* GM = GetInGameMode();
    for (int32 i = 0; i < GM->GameInfo.Players.Num(); i++)
    {
        GM->GameInfo.Players[i].BallPosX = GetPlayerBalls()[i]->GetActorLocation().X;
        GM->GameInfo.Players[i].BallPosY = GetPlayerBalls()[i]->GetActorLocation().Y;
        GM->GameInfo.Players[i].BallPosZ = GetPlayerBalls()[i]->GetActorLocation().Z;
    }
}

void UGolfPlayerManager::InitializeSensorManager()
{
    // 센서 매니저 찾기 또는 생성
    SensorManager = GetWorld()->SpawnActor<ACR2SensorManager>();
    if (SensorManager)
    {
        if (SensorManager->InitializeSensor())
        {
            // 이벤트 바인딩
            SensorManager->OnShotDetected.AddDynamic(this, &UGolfPlayerManager::OnSensorShotDetected);
            SensorManager->OnShotDetectedEx.AddDynamic(this, &UGolfPlayerManager::OnSensorShotDetectedEx);
            SensorManager->OnBallReady.AddDynamic(this, &UGolfPlayerManager::OnSensorBallReady);
            SensorManager->OnSensorStatusChanged.AddDynamic(this, &UGolfPlayerManager::OnSensorStatusChanged);

            // 센서 기본 설정 (드라이버 클럽으로 초기 설정)
            SensorManager->SetClubType(CR2CLUB_DRIVER);
            SensorManager->ConfigureSensor(2.67f, 2);

            UE_LOG(LogTemp, Log, TEXT("?? CR2 Sensor Manager initialized and configured successfully"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("?? Failed to initialize CR2 Sensor"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("?? Failed to create CR2 Sensor Manager"));
    }
}

void UGolfPlayerManager::SetSensorClub(int32 nClub, bool bIsRoughTerrain)
{
    if (!SensorManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("?? SensorManager is null"));
        return;
    }

    SensorManager->SetClubType(nClub, bIsRoughTerrain);

    int32 SensorStatus = SensorManager->GetSensorStatus();

    if (SensorStatus == CR2STATUS_READY)
    {
        UE_LOG(LogTemp, Warning, TEXT("?? SensorManager SetClub - %d"), nClub);
       // SensorManager->SetClubType(nClub);
        SensorManager->ResetSensorStatus();
    }

}


/**
 * @brief 센서 레디 상태를 체크하고 필요한 검증 수행 (BallCheck 통합 버전)
 * @param PlayerIndex 검사할 플레이어 인덱스
 *
 * @details
 * 상세 검증 프로세스:
 * 1??  사전 검증: SensorManager, PlayerIndex, Ball 유효성
 * 2??  센서 상태: CR2STATUS_READY인지 확인
 * 3??  센서 동작: StartSensorOperation() 실행
 * 4??  볼 영역 검사: BallCheck()로 3개 영역 동시 확인
 * 5??  클럽 매칭: 감지된 영역과 현재 설정된 클럽이 일치하는지 확인
 * 6??  상태 전환: 모든 검증 통과 시 레디 상태 진입
 *
 * BallCheck 통합 이점:
 * - 모든 영역의 볼 감지를 동시에 확인
 * - 비트마스크로 효율적인 처리
 * - Step별 진행 상황 명확히 추적
 * - 좌표 정보 명확하게 로깅
 */
void UGolfPlayerManager::CheckSensorReadyState(int32 PlayerIndex)
{
    if (bShuttingDown)
    {
        return;
    }
    // ===== [사전 검증] =====

    // 1??  SensorManager 유효성 검사
    if (!SensorManager)
    {
        UE_LOG(LogTemp, Error, TEXT("? [CheckSensorReadyState] CRITICAL: SensorManager is null"));
        return;
    }

    // 2??  PlayerIndex 유효성 검사
    if (PlayerIndex < 0 || !PlayerBalls.IsValidIndex(PlayerIndex))
    {
        UE_LOG(LogTemp, Error,
            TEXT("? [CheckSensorReadyState] INVALID PlayerIndex: %d (Valid range: 0-%d)"),
            PlayerIndex, PlayerBalls.Num() - 1);
        return;
    }

    // 3??  Ball 유효성 검사
    AGolfBall* Ball = PlayerBalls[PlayerIndex];
    if (!IsValid(Ball))
    {
        UE_LOG(LogTemp, Error,
            TEXT("? [CheckSensorReadyState] Ball object is null for player %d"),
            PlayerIndex);
        return;
    }

    // 4??  Ball 상태 검사
    if (Ball->GetBallState() != EBallState::Ball_Ready)
    {
        EBallState CurrentState = Ball->GetBallState();
        UE_LOG(LogTemp, Warning,
            TEXT("??  [CheckSensorReadyState] Ball state is not ready for player %d (Current: %d)"),
            PlayerIndex, (int32)CurrentState);
        return;
    }

    // UE_LOG(LogTemp, Log, TEXT("? [CheckSensorReadyState] Pre-validation passed for player %d"), PlayerIndex);

     // ===== [Step 1: 센서 상태 확인] =====

    int32 SensorStatus = SensorManager->GetSensorStatus();
    FString SensorStatusName = GetSensorStatusName(SensorStatus);

    if (SensorStatus != CR2STATUS_READY)
    {

        SensorManager->OnSensorStatusChanged.Broadcast(CR2STATUS_NOBALL);

        // 센서가 준비될 때까지 주기적으로 체크
        GetWorld()->GetTimerManager().SetTimer(
            SensorCheckTimer,
            [this, PlayerIndex]()
            {
                CheckSensorReadyState(PlayerIndex);
            },
            0.5f,  // 0.5초마다 체크
                false);

        return;
    }

    UE_LOG(LogTemp, Log, TEXT("? [Step 1] Sensor is READY"));

    // ===== [Step 2: 센서 동작 시작] =====

    CurrentActivePlayerIndex = PlayerIndex;

    if (!SensorManager->StartSensorOperation())
    {
        UE_LOG(LogTemp, Error,
            TEXT("? [Step 2] Failed to start sensor operation for player %d"),
            PlayerIndex);
        CurrentActivePlayerIndex = -1;
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("? [Step 2] Sensor operation started"));


    // BallCheck()는 내부적으로 상세한 로그를 출력합니다
    // (Step 2 ~ Step 6 수행)
    int32 BallCheckResult = SensorManager->BallCheck();


    // 어떤 영역에서도 볼이 감지되지 않았나?
    if (BallCheckResult == 0)
    {
        UE_LOG(LogTemp, Error,
            TEXT("? [Step 3] NO BALL DETECTED in any area!"));
        UE_LOG(LogTemp, Warning,
            TEXT("   Action: Please place the ball in appropriate area and try again"));

        SensorManager->OnSensorStatusChanged.Broadcast(CR2STATUS_NOBALL);
        CurrentActivePlayerIndex = -1;

        // 다시 체크하도록 타이머 설정
        GetWorld()->GetTimerManager().SetTimer(
            SensorCheckTimer,
            [this, PlayerIndex]()
            {
                CheckSensorReadyState(PlayerIndex);
            },
            0.5f,  // 1초 후 재확인
                false);

        return;
    }

    // ===== [Step 4: 현재 설정된 클럽 확인 및 영역 매칭] =====

    int32 SelectedClubCode = SensorManager->SelectClub;
    FString SelectedClubName = GetClubName(SelectedClubCode);


    // BallCheck 결과와 현재 클럽이 매칭되는지 확인
    // IsBallAreaMatchesClub은 EBallArea enum이 필요하지만,
    // BallCheck는 비트마스크를 반환하므로 변환이 필요합니다.

    bool bAreaMatches = false;
    EBallArea DetectedArea = EBallArea::BALL_AREA_NONE;

    // BallCheck 비트마스크에서 현재 설정된 클럽과 일치하는 영역 찾기
    if (SelectedClubCode == CR2CLUB_DRIVER)
    {
        // Driver: Tee 또는 Iron 영역에서 칠 수 있음
        if ((BallCheckResult & ACR2SensorManager::BALL_AREA_TEE_BIT))
        {
            bAreaMatches = true;
            DetectedArea = EBallArea::BALL_AREA_TEE;


        }
    }
    else if (SelectedClubCode == CR2CLUB_PUTTER)
    {
        // Putter: Putting 영역에서만 칠 수 있음
        if (BallCheckResult & ACR2SensorManager::BALL_AREA_PUTTING_BIT)
        {
            bAreaMatches = true;
            DetectedArea = EBallArea::BALL_AREA_PUTTING;
        }
    }
    else
    {
        // Other clubs (Iron, Wood, etc.): Iron 영역에서만 칠 수 있음
        if (BallCheckResult & ACR2SensorManager::BALL_AREA_IRON_BIT)
        {
            bAreaMatches = true;
            DetectedArea = EBallArea::BALL_AREA_IRON;
        }
    }

    if (!bAreaMatches)
    {

        UE_LOG(LogTemp, Error,
            TEXT("? [Step 4] AREA MISMATCH - REJECTING"));

        // 현재 설정된 클럽이 어느 영역에서 칠 수 있는지 안내
        FString RequiredArea = TEXT("");
        if (SelectedClubCode == CR2CLUB_DRIVER)
        {
            RequiredArea = TEXT("TEE or IRON area");
        }
        else if (SelectedClubCode == CR2CLUB_PUTTER)
        {
            RequiredArea = TEXT("PUTTING area (Green)");
        }
        else
        {
            RequiredArea = TEXT("IRON area (Fairway/Long game)");
        }


        SensorManager->OnSensorStatusChanged.Broadcast(CR2STATUS_NOBALL);
        CurrentActivePlayerIndex = -1;

        // 다시 체크하도록 타이머 설정
        GetWorld()->GetTimerManager().SetTimer(
            SensorCheckTimer,
            [this, PlayerIndex]()
            {
                CheckSensorReadyState(PlayerIndex);
            },
            0.5f,  // 1초 후 재확인
                false);

        return;  // ← 레디 상태 진입 거부!
    }

    // ===== [Step 5: ? 모든 검증 완료 - 레디 상태 진입] =====

    bSensorReady = true;

}


FString UGolfPlayerManager::GetSensorStatusName(int32 Status) const
{
    switch (Status)
    {
    case CR2STATUS_READY:
        return TEXT("?? READY");
    case CR2STATUS_GOODSHOT:
        return TEXT("?? GOODSHOT");
    case CR2STATUS_TRIALSHOT:
        return TEXT("?? TRIALSHOT");
    case CR2STATUS_DISCONNECT:
        return TEXT("?? DISCONNECT");
    case CR2STATUS_BIGSHADOW:
        return TEXT("?? BIGSHADOW");
    case CR2STATUS_NOBALL:
        return TEXT("?? NOBALL");
    default:
        return FString::Printf(TEXT("? UNKNOWN (0x%08X)"), Status);
    }
}

/**
 * @brief 볼 영역 코드를 문자열로 변환
 */
FString UGolfPlayerManager::GetBallAreaName(EBallArea Area) const
{
    switch (Area)
    {
    case EBallArea::BALL_AREA_TEE:
        return TEXT("TEE (티 영역)");
    case EBallArea::BALL_AREA_IRON:
        return TEXT("IRON (페어웨이/롱게임)");
    case EBallArea::BALL_AREA_PUTTING:
        return TEXT("PUTTING (퍼팅 그린)");
    case EBallArea::BALL_AREA_NONE:
        return TEXT("NONE (볼 미감지)");
    default:
        return TEXT("INVALID (알 수 없는 영역)");
    }
}

/**
 * @brief 클럽 코드를 문자열로 변환
 */
FString UGolfPlayerManager::GetClubName(int32 ClubCode) const
{
    switch (ClubCode)
    {
    case CR2CLUB_DRIVER:
        return TEXT("DRIVER");
    case CR2CLUB_IRON7:
        return TEXT("IRON7");
    case CR2CLUB_PUTTER:
        return TEXT("PUTTER");
    default:
        return FString::Printf(TEXT("OTHER(%d)"), ClubCode);
    }
}

void UGolfPlayerManager::OnSensorShotDetected(const FCR2ShotData& ShotData)
{
    if (bShuttingDown)
    {
        return;
    }
    if (!bSensorReady || CurrentActivePlayerIndex == -1)
    {
        UE_LOG(LogTemp, Warning, TEXT("?? Sensor shot detected but no active SensorReady - %d player - %d"), bSensorReady, CurrentActivePlayerIndex);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("?? Shot detected for Player %d - BallSpeed: %.1f m/s"),
        CurrentActivePlayerIndex, ShotData.BallSpeedX10 / 10.0f);

    // PlayerController 유효성 확인
    if (!PlayerController || !IsValid(PlayerController))
    {
        UE_LOG(LogTemp, Error, TEXT("? PlayerController is null or invalid"));
        return;
    }

    // 센서 데이터에서 각도와 파워 계산
    float SensorYaw = ShotData.AzimuthX10 / 10.0f;    // 센서의 좌우 각도
    float SensorPitch = ShotData.InclineX10 / 10.0f;  // 센서의 위아래 각도
    float ShotPower = CalculateShotPower(ShotData);

    // ?? 좌표계 변환 (필요에 따라 조정)
    float UnrealPitch = SensorPitch;  // 그대로 사용
    float UnrealYaw = SensorYaw * 0.5f;     // 

    UE_LOG(LogTemp, Log, TEXT("?? Sensor Data: Pitch=%.2f, Yaw=%.2f, Power=%.2f"),
        UnrealPitch, UnrealYaw, ShotPower);

    // PlayerController의 샷 파라미터 설정
    PlayerController->ShotPitchAngle = UnrealPitch;
    PlayerController->ShotYawAngle = UnrealYaw;
    PlayerController->ShotPower = ShotPower;

    // ?? 수정: PlayerController의 ExecuteShot() 호출
    PlayerController->ExecuteShot();

    UE_LOG(LogTemp, Log, TEXT("? Sensor shot executed via PlayerController for Player %d"),
        CurrentActivePlayerIndex);

    // 센서를 대기 상태로 전환
    SetSensorToStandby();
}

void UGolfPlayerManager::OnSensorShotDetectedEx(const FCR2ShotDataEx& ShotDataEx)
{
    if (bShuttingDown)
    {
        return;
    }
    if (!bSensorReady || CurrentActivePlayerIndex == -1)
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("?? Extended shot data - VMag: %.2f m/s, Spin: %.1f rpm"),
        ShotDataEx.VMag, ShotDataEx.SpinMag);

    // 확장 데이터 활용 (필요시 추가 처리)
}

void UGolfPlayerManager::OnSensorBallReady(const FCR2BallPosition& BallPosition)
{
    if (bShuttingDown)
    {
        return;
    }
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("?? Ball ready detected at position: %s"),
        *BallPosition.Position.ToString());

    AInGameMode* GM = Cast<AInGameMode>(World->GetAuthGameMode());
    if (!IsValid(GM) || !IsValid(GM->ReadyBillboard) || !IsValid(GM->ReadyBillboard->Billboard))
    {
        return;
    }

    AGolfPlayer* CurrentPlayer = GM->GetCurrentTurnGolfPlayer();
    if (IsValid(CurrentPlayer) && CurrentPlayer->GetPlayerState() == EPlayerState::Player_Ready)
    {
        if (auto* SM = World->GetGameInstance()->GetSubsystem<USoundManager>())
        {
            SM->Play2D_ById("Effect.Ready");
        }

        GM->ReadyBillboard->Billboard->SetSprite(GM->ReadyBillboard->ReadyImage);

        IsBillboardVisible = true;

        World->GetTimerManager().ClearTimer(TH);
        World->GetTimerManager().ClearTimer(TH2);

        TWeakObjectPtr<UGolfPlayerManager> SelfWeak(this);
        TWeakObjectPtr<AInGameMode> GMWeak(GM);

        World->GetTimerManager().SetTimer(TH,
            FTimerDelegate::CreateLambda([SelfWeak, GMWeak]()
            {
                UGolfPlayerManager* Self = SelfWeak.Get();
                AInGameMode* GMPtr = GMWeak.Get();
                if (!Self || !GMPtr || !IsValid(GMPtr->ReadyBillboard) || !IsValid(GMPtr->ReadyBillboard->Billboard))
                {
                    return;
                }
                GMPtr->ReadyBillboard->Billboard->SetVisibility(Self->IsBillboardVisible);
                Self->IsBillboardVisible = !Self->IsBillboardVisible;
            }), 0.2f, true);

        World->GetTimerManager().SetTimer(TH2,
            FTimerDelegate::CreateLambda([SelfWeak, GMWeak]()
                {
                    UGolfPlayerManager* Self = SelfWeak.Get();
                    if (!Self)
                    {
                        return;
                    }
                    if (UWorld* InnerWorld = Self->GetWorld())
                    {
                        InnerWorld->GetTimerManager().ClearTimer(Self->TH);
                    }
                    AInGameMode* GMPtr = GMWeak.Get();
                    if (GMPtr && IsValid(GMPtr->ReadyBillboard) && IsValid(GMPtr->ReadyBillboard->Billboard))
                    {
                        GMPtr->ReadyBillboard->Billboard->SetVisibility(false);
                    }
                    Self->IsBillboardVisible = false;
                }), 1.f, false);
    }
}

void UGolfPlayerManager::OnSensorStatusChanged(int32 Status)
{
    if (bShuttingDown)
    {
        return;
    }
    AInGameMode* GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());

    switch (Status)
    {
    case CR2STATUS_READY:
    //    UE_LOG(LogTemp, Log, TEXT("?? Sensor is ready"));
        break;

    case CR2STATUS_DISCONNECT:
     //   UE_LOG(LogTemp, Warning, TEXT("?? Sensor disconnected"));
        bSensorReady = false;
        break;

    case CR2STATUS_NOBALL:
     //  UE_LOG(LogTemp, Log, TEXT("?? No ball detected"));
        if (GM)
        {
            if (GM->GetCurrentTurnGolfPlayer()->GetPlayerState() == EPlayerState::Player_Ready)
            {
                GM->ReadyBillboard->Billboard->SetSprite(GM->ReadyBillboard->NoReadyImage);
                GM->ReadyBillboard->Billboard->SetVisibility(true);
            }
        }
        break;

    case CR2STATUS_BIGSHADOW:
      //  UE_LOG(LogTemp, Warning, TEXT("?? Light issue detected"));
        break;
    }
}

FVector UGolfPlayerManager::CalculateShotDirection(const FCR2ShotData& ShotData)
{
    // Azimuth와 Incline을 사용하여 3D 방향 벡터 계산
    float AzimuthDegrees = ShotData.AzimuthX10 / 10.0f;
    float InclineDegrees = ShotData.InclineX10 / 10.0f;

    // 각도를 라디안으로 변환
    float AzimuthRad = FMath::DegreesToRadians(AzimuthDegrees);
    float InclineRad = FMath::DegreesToRadians(InclineDegrees);

    // 3D 방향 벡터 계산
    FVector Direction;
    Direction.X = FMath::Cos(InclineRad) * FMath::Cos(AzimuthRad);  // 전진 방향
    Direction.Y = FMath::Cos(InclineRad) * FMath::Sin(AzimuthRad);  // 좌우 방향
    Direction.Z = FMath::Sin(InclineRad);                           // 상하 방향

    return Direction.GetSafeNormal();
}


FVector UGolfPlayerManager::CalculateAimDirection(const FCR2ShotData& ShotData)
{
    // 카메라가 유효한지 확인
    if (!CameraManager || !CameraManager->Camera)
    {
        UE_LOG(LogTemp, Warning, TEXT("? CameraManager or Camera is null in CalculateAimDirection"));
        return FVector::ForwardVector;  // 기본 방향 벡터 반환 (Fallback)
    }

    // 센서 데이터에서 각도 추출
    float SensorYaw = ShotData.AzimuthX10 / 10.0f;    // 센서의 좌우 각도
    float SensorPitch = ShotData.InclineX10 / 10.0f;  // 센서의 위아래 각도

    // 카메라의 현재 Forward Vector 가져오기 (카메라 방향 기준)
    FVector CameraForward = CameraManager->Camera->GetForwardVector().GetSafeNormal();


    // Yaw 각도를 적용한 회전 생성 (Pitch와 Roll은 0으로 유지, 샷 방향만 조정)
    FRotator Rotation(SensorYaw, SensorPitch, 0.0f);
    FQuat QuatRotation = Rotation.Quaternion();

    // 카메라 Forward를 기준으로 회전 적용
    FVector AimDir = QuatRotation.RotateVector(CameraForward);

    // 결과 벡터 정규화 (단위 벡터로 만듦)
    AimDir.Normalize();

  //  UE_LOG(LogTemp, Log, TEXT("? Sensor Direction Calculated: %s (SensorYaw=%.2f→UnrealYaw=%.2f, Pitch=%.2f)"),
 //       *FinalDirection.ToString(), SensorYaw, UnrealYaw, UnrealPitch);


    return AimDir;
}


float UGolfPlayerManager::CalculateShotPower(const FCR2ShotData& ShotData)
{
    // 볼 스피드를 파워로 변환 (적절한 스케일링 필요)
    float BallSpeedMS = ShotData.BallSpeedX10 / 10.0f; // m/s로 변환

    // 게임에 맞는 파워 스케일링 (이 값은 게임에 따라 조정 필요)
    float Power = FMath::Clamp(BallSpeedMS * 1.0f, 0.0f, 100.0f);

    return Power;
}

void UGolfPlayerManager::SetSensorToStandby()
{
    if (!SensorManager)
    {
        return;
    }

    // 센서 동작 중지
    if (SensorManager->StopSensorOperation())
    {
        UE_LOG(LogTemp, Log, TEXT("?? Sensor set to standby mode"));
    }

    bSensorReady = false;
    CurrentActivePlayerIndex = -1;

    // 센서 체크 타이머도 정리
    GetWorld()->GetTimerManager().ClearTimer(SensorCheckTimer);
}

void UGolfPlayerManager::ResetPlayers()
{
    bool bAllHoleIn = true;
    for (int32 i = 0; i < Players.Num(); i++)
    {
        if (Players.IsValidIndex(i) && IsValid(Players[i]) &&
            PlayerBalls.IsValidIndex(i) && IsValid(PlayerBalls[i]))
        {
            AGolfPlayer* Player = Players[i];
            AGolfBall* Ball = PlayerBalls[i];

            bool bPlayerHoleIn = Player->IsHoleIn();
            bool bBallHoleIn = Ball->IsHoleIn();
            bool bBallConceded = Ball->IsConceded();
            bool bDoublePar = Player->CheckDoublePar();

            bool bIsPlayerComplete = bPlayerHoleIn || bBallHoleIn || bBallConceded || bDoublePar;

            if (!bIsPlayerComplete)
            {
                bAllHoleIn = false;
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Invalid Player or Ball at index %d"), i);
        }
    }
}

void UGolfPlayerManager::UpdatePlayerState(int32 PlayerIndex, EPlayerState NewState)
{
    UE_LOG(LogTemp, Log, TEXT("GolfPlayerManager: Player %d State Changed to: %s"),
        PlayerIndex, *UEnum::GetValueAsString(NewState));
    if (Players.IsValidIndex(PlayerIndex))
    {
        Players[PlayerIndex]->SetPlayerState(NewState);
    }
}

void UGolfPlayerManager::UpdateBallState(int32 PlayerIndex, EBallState NewState)
{
    if (PlayerBalls.IsValidIndex(PlayerIndex))
    {
        PlayerBalls[PlayerIndex]->SetBallState(NewState);
    }
}

void UGolfPlayerManager::ProcessPlayerShot(int32 PlayerIndex, const FVector& Direction, float Power)
{
    if (Players.IsValidIndex(PlayerIndex) && PlayerBalls.IsValidIndex(PlayerIndex))
    {
        AGolfPlayer* Player = Players[PlayerIndex];
        AGolfBall* Ball = PlayerBalls[PlayerIndex];

        if (Player && Ball)
        {
            Player->PrepareShot(Direction, Power);
            Player->ExecuteShot();
            if (CameraManager)
            {
                CameraManager->ChangeCameraMode(ECameraMode::Flying);
            }

            UE_LOG(LogTemp, Log, TEXT("Player %d shot processed: Direction=%s, Power=%.1f"),
                PlayerIndex, *Direction.ToString(), Power);
        }
    }
}

bool UGolfPlayerManager::IsHoleComplete(int32 CurrentHole) const
{
    if (!GetWorld())
    {
        UE_LOG(LogTemp, Error, TEXT("? World is null in IsHoleComplete"));
        return false;
    }

    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    if (!GameMode)
    {
        UE_LOG(LogTemp, Error, TEXT("? Invalid GameMode in IsHoleComplete"));
        return false;
    }

    bool bAllHoleIn = true;
    for (int32 i = 0; i < Players.Num(); i++)
    {
        if (Players.IsValidIndex(i) && IsValid(Players[i]) &&
            PlayerBalls.IsValidIndex(i) && IsValid(PlayerBalls[i]))
        {
            AGolfPlayer* Player = Players[i];
            AGolfBall* Ball = PlayerBalls[i];

            bool bPlayerHoleIn = Player->IsHoleIn();
            bool bBallHoleIn = Ball->IsHoleIn();
            bool bBallConceded = Ball->IsConceded();
            //bool bDoublePar = Player->CheckDoublePar();

            ///갑자기 홀 아웃 때려버리는 원흉 문제 있음
            bool bIsPlayerComplete = bPlayerHoleIn || bBallHoleIn || bBallConceded;
            //bool bIsPlayerComplete = bPlayerHoleIn || bBallHoleIn || bBallConceded || bDoublePar;

            if (!bIsPlayerComplete)
            {
                bAllHoleIn = false;
            }
        }

    }

    if (bAllHoleIn)
    {
      //  UE_LOG(LogTemp, Log, TEXT("?? All players completed hole %d"), CurrentHole);
    }

    return bAllHoleIn;
}

bool UGolfPlayerManager::IsCurrentPlayerShotComplete() const
{
    if (AInGameMode* GameMode = Cast<AInGameMode>(GetOwner()))
    {
        int32 CurrentPlayerIndex = GameMode->CurrentPlayerIndex;

        if (PlayerOrder.IsValidIndex(CurrentPlayerIndex) &&
            PlayerBalls.IsValidIndex(PlayerOrder[CurrentPlayerIndex]) &&
            Players.IsValidIndex(PlayerOrder[CurrentPlayerIndex]))
        {
            AGolfBall* CurrentBall = PlayerBalls[PlayerOrder[CurrentPlayerIndex]];
            AGolfPlayer* CurrentPlayer = Players[PlayerOrder[CurrentPlayerIndex]];

            bool bBallStopped = CurrentBall && CurrentBall->GetBallState() == EBallState::Ball_Stop;
            bool bPlayerResults = CurrentPlayer && CurrentPlayer->GetPlayerState() == EPlayerState::Player_Results;

            bool bComplete = bBallStopped && bPlayerResults;

            if (bComplete)
            {
                UE_LOG(LogTemp, Log, TEXT("? Player %d shot complete: Ball stopped + Results state"),
                    PlayerOrder[CurrentPlayerIndex]);
            }

            return bComplete;
        }
    }

    return false;
}

void UGolfPlayerManager::AdvanceTurn()
{
    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    if (!GameMode)
    {
        UE_LOG(LogTemp, Error, TEXT("? Invalid GameMode in AdvanceTurn"));
        return;
    }


    // 현재 플레이어 정보
    int32 CurrentPlayerIndex = GameMode->CurrentPlayerIndex;

    UE_LOG(LogTemp, Log, TEXT("===================================="));
    UE_LOG(LogTemp, Log, TEXT("?? AdvanceTurn 시작 - 현재 플레이어: %d"), CurrentPlayerIndex);
    UE_LOG(LogTemp, Log, TEXT("===================================="));

    // ? 1단계: 현재 플레이어 상태를 Results로 변경
    if (Players.IsValidIndex(CurrentPlayerIndex))
    {
        AGolfPlayer* CurrentPlayer = Players[CurrentPlayerIndex];
        if (IsValid(CurrentPlayer))
        {
            // 1. 현재 플레이어가 Hole-Out 상태인지 확인
            if (CurrentPlayer->GetPlayerState() == EPlayerState::Player_HoleOut)
            {
                if (PlayerBalls.IsValidIndex(CurrentPlayerIndex))
                {
                    AGolfBall* CurrentBall = PlayerBalls[CurrentPlayerIndex];
                    if (IsValid(CurrentBall))
                    {
                        // 홀아웃이면 볼을 숨깁니다. (사용자 요청 사항)
                        CurrentBall->SetBallVisibility(false, false);
                        UE_LOG(LogTemp, Log, TEXT("? 홀아웃 플레이어 %d의 볼 숨김: SetBallVisibility(false)"), CurrentPlayerIndex);
                    }
                }
            }
            // 2. Hole-Out 상태가 아니면 턴 종료 후 Results 상태로 변경합니다. (기존 로직)
            else
            {
                if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
                {
                    AGolfPlayerController* GPC = Cast<AGolfPlayerController>(PC);

                    GPC->ShotCinematicComponent->StopCinematic(0.f);
                }

                CurrentPlayer->SetPlayerState(EPlayerState::Player_Results);
                UE_LOG(LogTemp, Log, TEXT("? 플레이어 %d 상태 → Player_Results"), CurrentPlayerIndex);
            }
        }
    }



    // ? Training/Range 모드 특수 처리
    if (IsTrainingMode())
    {
        UE_LOG(LogTemp, Log, TEXT("?? Training Mode: 플레이어 0으로 고정"));
        SetupNextPlayer(0);
        return;
    }

    if (IsRangeMode())
    {
        UE_LOG(LogTemp, Log, TEXT("?? Range Mode: 플레이어 0으로 고정"));
        SetupNextPlayer(0);
        return;
    }

    // ? 2단계: 모든 플레이어가 홀 아웃했는지 확인
    bool bAllPlayersAreHoleOut = true;
    for (AGolfPlayer* Player : Players)
    {
        if (IsValid(Player) && !Player->bIsPendingDelete && Player->GetPlayerState() != EPlayerState::Player_HoleOut)
        {
            bAllPlayersAreHoleOut = false;
            break;
        }
    }

    if (bAllPlayersAreHoleOut)
    {
        UE_LOG(LogTemp, Log, TEXT("?? 모든 플레이어가 홀 아웃! 홀 종료"));
        GameMode->ChangeGameState(EGameState::Game_HoleOut);
        return;
    }

    // ? 3단계: 첫 타 완료 여부 확인
    bool bAllPlayersHaveMadeFirstShot = CheckAllPlayersHaveFirstShot();

    // ? 4단계: 현재 볼의 홀컵까지 거리 계산
    float HoleDistance = CalculateCurrentBallDistanceToHole(GameMode);

    // ? 5단계: 다음 플레이어 결정
    int32 NextPlayerIdx = DetermineNextPlayer(
        CurrentPlayerIndex,
        bAllPlayersHaveMadeFirstShot,
        HoleDistance,
        GameMode
    );

    GameMode->UpdateBallNamePlateAndMarker();

    // ? 6단계: 다음 플레이어로 전환
    if (NextPlayerIdx != -1)
    {
        GameMode->CurrentPlayerIndex = NextPlayerIdx;
        UE_LOG(LogTemp, Log, TEXT("? 다음 플레이어: %d번"), NextPlayerIdx);
        UE_LOG(LogTemp, Log, TEXT("===================================="));
        SetupNextPlayer(NextPlayerIdx);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("? 다음 플레이어를 찾을 수 없음 → 강제 홀 아웃"));
        GameMode->ChangeGameState(EGameState::Game_HoleOut);
    }

    GameMode->CurrentPlayerIndex = NextPlayerIdx;
    GameMode->UpdateMiniMapForCurrentPlayer();
}


bool UGolfPlayerManager::CheckAllPlayersHaveFirstShot() const
{
    bool bAllHaveFirstShot = true;

    for (AGolfPlayer* Player : Players)
    {
        if (IsValid(Player) &&
            !Player->bIsPendingDelete &&
            Player->GetPlayerState() != EPlayerState::Player_HoleOut &&
            Player->GetCurrentHoleShotCount() == 0)
        {
            bAllHaveFirstShot = false;
            UE_LOG(LogTemp, Log, TEXT("  ?? 플레이어 %d는 아직 첫 샷을 치지 않음"), Player->PlayerIndex);
            break;
        }
    }

    UE_LOG(LogTemp, Log, TEXT("?? 모든 플레이어 첫 샷 완료 여부: %s"),
        bAllHaveFirstShot ? TEXT("예") : TEXT("아니오"));

    return bAllHaveFirstShot;
}

float UGolfPlayerManager::CalculateCurrentBallDistanceToHole(AInGameMode* GameMode) const
{
    float Distance = 0.f;

    if (GameMode && GameMode->GetCurrentTurnGolfBall())
    {
        FVector BallLocation = GameMode->GetCurrentTurnGolfBall()->GetActorLocation();
        FVector HolecupLocation = GameMode->MapInfo.HolecupPositions[GameMode->CurrentHole - 1];
        Distance = FVector::Dist(BallLocation, HolecupLocation);

        UE_LOG(LogTemp, Log, TEXT("?? 현재 볼 → 홀컵 거리: %.1f cm (%.2f m)"),
            Distance, Distance / 100.f);
    }

    return Distance;
}

int32 UGolfPlayerManager::DetermineNextPlayer(
    int32 CurrentPlayerIndex,
    bool bAllHaveFirstShot,
    float HoleDistance,
    AInGameMode* GameMode)
{
    int32 NextPlayerIdx = -1;

    // 퍼팅 연속 타석 조건 체크
    bool bContinuousPutting = ShouldContinuePutting(CurrentPlayerIndex, HoleDistance, GameMode);

    if (bContinuousPutting)
    {
        UE_LOG(LogTemp, Log, TEXT("? 퍼팅 연속 타석: 플레이어 %d 계속"), CurrentPlayerIndex);
        return CurrentPlayerIndex;
    }

    // 모든 플레이어가 첫 타를 친 경우
    if (bAllHaveFirstShot)
    {
        NextPlayerIdx = FindNextPlayerAfterAllFirstShots();
    }
    else // 아직 첫 타를 치지 않은 플레이어가 있는 경우
    {
        NextPlayerIdx = FindNextPlayerInTeeShotOrder(CurrentPlayerIndex);
    }

    return NextPlayerIdx;
}

bool UGolfPlayerManager::ShouldContinuePutting(
    int32 PlayerIndex,
    float HoleDistance,
    AInGameMode* GameMode) const
{
    // 연속 퍼팅 조건
    // 1. 거리가 5m 이내
    // 2. 아직 홀인하지 않음
    // 3. 게임 옵션에서 연속 퍼팅 활성화

    const float PUTTING_DISTANCE_THRESHOLD = 500.0f; // 5m

    if (HoleDistance > PUTTING_DISTANCE_THRESHOLD)
    {
        return false;
    }

    if (!Players.IsValidIndex(PlayerIndex) || !IsValid(Players[PlayerIndex]))
    {
        return false;
    }

    if (Players[PlayerIndex]->IsHoleIn())
    {
        return false;
    }

    if (PlayerBalls[PlayerIndex]->IsConceded())
    {
        return false;
    }

    if (!GameMode || GameMode->GameInfo.GameOptions.ContinuePutting != 1)
    {
        return false;
    }

    return true;
}

int32 UGolfPlayerManager::FindNextPlayerAfterAllFirstShots()
{
    UE_LOG(LogTemp, Log, TEXT("?? 거리 순으로 다음 플레이어 찾기"));

    // 거리 순으로 재정렬
    SortPlayersByDistance();

    // 홀 아웃하지 않은 첫 번째 플레이어 찾기
    for (int32 i = 0; i < PlayerOrder.Num(); ++i)
    {
        int32 PlayerIdx = PlayerOrder[i];

        if (Players.IsValidIndex(PlayerIdx) &&
            IsValid(Players[PlayerIdx]) &&
            !Players[PlayerIdx]->bIsPendingDelete &&
            Players[PlayerIdx]->GetPlayerState() != EPlayerState::Player_HoleOut)
        {
            UE_LOG(LogTemp, Log, TEXT("  ? 가장 먼 플레이어: %d번"), PlayerIdx);
            return PlayerIdx;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("  ?? 유효한 플레이어를 찾지 못함"));
    return -1;
}

int32 UGolfPlayerManager::FindNextPlayerInTeeShotOrder(int32 CurrentPlayerIndex)
{
    UE_LOG(LogTemp, Log, TEXT("?? 티샷 순서대로 다음 플레이어 찾기"));

    int32 CurrentOrderIndex = PlayerOrder.Find(CurrentPlayerIndex);
    if (CurrentOrderIndex == INDEX_NONE)
    {
        CurrentOrderIndex = 0;
        UE_LOG(LogTemp, Warning, TEXT("  ?? 현재 플레이어가 순서에 없음, 0으로 초기화"));
    }

    // 다음 플레이어를 순환하며 찾기
    for (int32 i = 1; i <= PlayerOrder.Num(); ++i)
    {
        int32 NextIdx = PlayerOrder[(CurrentOrderIndex + i) % PlayerOrder.Num()];

        if (Players.IsValidIndex(NextIdx) &&
            IsValid(Players[NextIdx]) &&
            Players[NextIdx]->GetPlayerState() != EPlayerState::Player_HoleOut &&
            !Players[NextIdx]->bIsPendingDelete)
        {
            UE_LOG(LogTemp, Log, TEXT("  ? 다음 순서 플레이어: %d번"), NextIdx);
            return NextIdx;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("  ?? 유효한 다음 플레이어를 찾지 못함"));
    return -1;
}


/**
 * 현재 플레이어를 건너뛰고 다음 플레이어로 턴을 넘깁니다.
 * - 현재 플레이어를 Results 상태로 변경
 * - 현재 플레이어를 제외한 다음 유효한 플레이어 찾기
 * - 다음 플레이어로 턴 전환
 *
 * 사용 예시:
 * - 플레이어가 페널티로 턴을 건너뛸 때
 * - 플레이어가 기권할 때
 * - 특정 이유로 현재 플레이어의 턴을 종료하고 다음으로 넘어갈 때
 */
void UGolfPlayerManager::SkipCurrentPlayerTurn()
{
    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    if (!GameMode)
    {
        UE_LOG(LogTemp, Error, TEXT("? SkipCurrentPlayerTurn: Invalid GameMode"));
        return;
    }

    int32 CurrentPlayerIndex = GameMode->CurrentPlayerIndex;

    UE_LOG(LogTemp, Log, TEXT("===================================="));
    UE_LOG(LogTemp, Log, TEXT("?? 현재 플레이어 건너뛰기: %d번"), CurrentPlayerIndex);
    UE_LOG(LogTemp, Log, TEXT("===================================="));

    // 1. 현재 플레이어 유효성 검사
    if (!Players.IsValidIndex(CurrentPlayerIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("? 잘못된 CurrentPlayerIndex: %d"), CurrentPlayerIndex);
        return;
    }

    AGolfPlayer* CurrentPlayer = Players[CurrentPlayerIndex];
    if (!IsValid(CurrentPlayer))
    {
        UE_LOG(LogTemp, Error, TEXT("? 현재 플레이어가 유효하지 않음"));
        return;
    }

    // 2. 현재 플레이어를 Results 상태로 변경
    if (CurrentPlayer->GetPlayerState() != EPlayerState::Player_HoleOut)
    {
        CurrentPlayer->SetPlayerState(EPlayerState::Player_Results);
        UE_LOG(LogTemp, Log, TEXT("? 플레이어 %d 상태 → Player_Results (건너뜀)"), CurrentPlayerIndex);
    }

    // 3. 모든 플레이어가 홀 아웃했는지 확인
    bool bAllPlayersAreHoleOut = true;
    for (AGolfPlayer* Player : Players)
    {
        if (IsValid(Player) && !Player->bIsPendingDelete && Player->GetPlayerState() != EPlayerState::Player_HoleOut)
        {
            bAllPlayersAreHoleOut = false;
            break;
        }
    }

    if (bAllPlayersAreHoleOut)
    {
        UE_LOG(LogTemp, Log, TEXT("?? 모든 플레이어가 홀 아웃! 홀 종료"));
        GameMode->ChangeGameState(EGameState::Game_HoleOut);
        return;
    }

    // 4. 현재 플레이어를 제외한 다음 플레이어 찾기
    int32 NextPlayerIdx = FindNextPlayerExcludingCurrent(CurrentPlayerIndex);

    // 5. 다음 플레이어로 전환
    if (NextPlayerIdx != -1)
    {
        GameMode->CurrentPlayerIndex = NextPlayerIdx;
        UE_LOG(LogTemp, Log, TEXT("? 다음 플레이어: %d번"), NextPlayerIdx);
        UE_LOG(LogTemp, Log, TEXT("===================================="));
        SetupNextPlayer(NextPlayerIdx);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("? 다음 플레이어를 찾을 수 없음 → 강제 홀 아웃"));
     //   GameMode->ChangeGameState(EGameState::Game_HoleOut);
    }
}


/**
 * 현재 플레이어를 제외하고 다음 유효한 플레이어를 찾습니다.
 *
 * @param CurrentPlayerIndex - 제외할 현재 플레이어 인덱스
 * @return 다음 유효한 플레이어 인덱스, 없으면 -1
 *
 * 찾기 규칙:
 * 1. PlayerOrder 배열에서 현재 플레이어의 다음 순서부터 검색
 * 2. 현재 플레이어 자신은 제외
 * 3. 홀 아웃된 플레이어도 제외
 * 4. 순환하여 모든 플레이어 검사
 */
int32 UGolfPlayerManager::FindNextPlayerExcludingCurrent(int32 CurrentPlayerIndex)
{
    UE_LOG(LogTemp, Log, TEXT("?? 현재 플레이어(%d)를 제외한 다음 플레이어 찾기"), CurrentPlayerIndex);

    if (PlayerOrder.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("? PlayerOrder가 비어있음"));
        return -1;
    }

    // PlayerOrder에서 현재 플레이어의 위치 찾기
    int32 CurrentOrderIndex = PlayerOrder.Find(CurrentPlayerIndex);
    if (CurrentOrderIndex == INDEX_NONE)
    {
        UE_LOG(LogTemp, Warning, TEXT("?? 현재 플레이어가 PlayerOrder에 없음, 0부터 시작"));
        CurrentOrderIndex = -1; // -1로 설정하면 다음 루프에서 0부터 시작
    }

    // PlayerOrder를 순환하며 다음 유효한 플레이어 찾기
    for (int32 i = 1; i <= PlayerOrder.Num(); ++i)
    {
        int32 NextOrderIndex = (CurrentOrderIndex + i) % PlayerOrder.Num();
        int32 CandidatePlayerIndex = PlayerOrder[NextOrderIndex];

        // 현재 플레이어 자신은 건너뛰기
        if (CandidatePlayerIndex == CurrentPlayerIndex)
        {
            UE_LOG(LogTemp, Log, TEXT("  ?? 플레이어 %d 건너뜀 (현재 플레이어)"), CandidatePlayerIndex);
            continue;
        }

        // 유효성 검사
        if (!Players.IsValidIndex(CandidatePlayerIndex))
        {
            UE_LOG(LogTemp, Warning, TEXT("  ?? 플레이어 %d 인덱스가 유효하지 않음"), CandidatePlayerIndex);
            continue;
        }

        AGolfPlayer* CandidatePlayer = Players[CandidatePlayerIndex];
        if (!IsValid(CandidatePlayer))
        {
            UE_LOG(LogTemp, Warning, TEXT("  ?? 플레이어 %d가 유효하지 않음"), CandidatePlayerIndex);
            continue;
        }

        // 홀 아웃된 플레이어는 제외
        if (CandidatePlayer->bIsPendingDelete)
        {
            UE_LOG(LogTemp, Log, TEXT("  ?? 플레이어 %d 건너뜀 (삭제 대기)"), CandidatePlayerIndex);
            continue;
        }

        if (CandidatePlayer->GetPlayerState() == EPlayerState::Player_HoleOut)
        {
            UE_LOG(LogTemp, Log, TEXT("  ?? 플레이어 %d 건너뜀 (홀 아웃)"), CandidatePlayerIndex);
            continue;
        }

        // 유효한 플레이어 발견!
        UE_LOG(LogTemp, Log, TEXT("  ? 다음 플레이어 발견: %d번"), CandidatePlayerIndex);
        return CandidatePlayerIndex;
    }

    // 유효한 플레이어를 찾지 못함
    UE_LOG(LogTemp, Warning, TEXT("  ?? 유효한 다음 플레이어를 찾지 못함"));
    return -1;
}



void UGolfPlayerManager::AdvanceToNextPlayer(int32 CurrentPlayerIndex)
{
    UE_LOG(LogTemp, Log, TEXT("?? Advancing from Player %d"), CurrentPlayerIndex);

    if (Players.IsValidIndex(CurrentPlayerIndex))
    {
        AGolfPlayer* CurrentPlayer = Players[CurrentPlayerIndex];
        if (IsValid(CurrentPlayer) && CurrentPlayer->GetPlayerState() != EPlayerState::Player_Results)
        {
            CurrentPlayer->SetPlayerState(EPlayerState::Player_Results);
            UE_LOG(LogTemp, Log, TEXT("Player %d set to Results state"), CurrentPlayerIndex);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("? Invalid CurrentPlayerIndex: %d"), CurrentPlayerIndex);
        return;
    }

    bool bAllTeeShotsDone = true;
    for (int32 i = 0; i < Players.Num(); i++)
    {
        if (Players[i]->GetPlayerState() == EPlayerState::Player_Ready || Players[i]->GetPlayerState() == EPlayerState::Player_Init)
        {
            bAllTeeShotsDone = false;
            break;
        }
    }

    if (bAllTeeShotsDone)
    {
        SortPlayersByDistance();
    }

    int32 NextPlayerIndex = FindNextPlayer(CurrentPlayerIndex);

    if (NextPlayerIndex != -1)
    {
        SetupNextPlayer(NextPlayerIndex);
        UE_LOG(LogTemp, Log, TEXT("? Advanced to Player %d"), NextPlayerIndex);
    }
    else
    {
        HandleAllPlayersComplete();
        UE_LOG(LogTemp, Log, TEXT("?? All players completed shots"));
    }


}


void UGolfPlayerManager::AddPlayer(AGolfPlayer* Player)
{
    Players.Add(Player);
}

void UGolfPlayerManager::AddBall(AGolfBall* Ball)
{
    PlayerBalls.Add(Ball);
}

void UGolfPlayerManager::RebuildPlayerIndicesAndOrder(int32 PreserveSlotIndex)
{
    AInGameMode* GameMode = GetInGameMode();

    // Re-index players.
    for (int32 i = 0; i < Players.Num(); ++i)
    {
        if (IsValid(Players[i]))
        {
            Players[i]->PlayerIndex = i;
        }
    }

    // Re-index balls to match player indices.
    for (int32 i = 0; i < PlayerBalls.Num(); ++i)
    {
        if (IsValid(PlayerBalls[i]))
        {
            PlayerBalls[i]->OwningPlayerIndex = i;
        }
    }

    // Rebuild turn order by PlayerIndex (0..N-1).
    PlayerOrder.Empty();
    for (int32 i = 0; i < Players.Num(); ++i)
    {
        PlayerOrder.Add(i);
    }

    // Preserve current player by SlotIndex if possible.
    if (GameMode)
    {
        if (PreserveSlotIndex != INDEX_NONE)
        {
            int32 NewCurrentIndex = INDEX_NONE;
            for (int32 i = 0; i < Players.Num(); ++i)
            {
                if (Players[i] && Players[i]->PlayerInfo.SlotIndex == PreserveSlotIndex)
                {
                    NewCurrentIndex = i;
                    break;
                }
            }
            if (NewCurrentIndex != INDEX_NONE)
            {
                GameMode->CurrentPlayerIndex = NewCurrentIndex;
            }
            else if (Players.Num() > 0)
            {
                GameMode->CurrentPlayerIndex = 0;
            }
        }
        else if (!Players.IsValidIndex(GameMode->CurrentPlayerIndex) && Players.Num() > 0)
        {
            GameMode->CurrentPlayerIndex = 0;
        }
    }
}

int32 UGolfPlayerManager::FindNextPlayer(int32 CurrentPlayerIndex)
{
    int32 CurrentOrderIndex = PlayerOrder.Find(CurrentPlayerIndex);
    if (CurrentOrderIndex == INDEX_NONE)
    {
        UE_LOG(LogTemp, Error, TEXT("? CurrentPlayerIndex %d not found in PlayerOrder"), CurrentPlayerIndex);
        return -1;
    }

    for (int32 i = 1; i < PlayerOrder.Num(); i++)
    {
        int32 NextOrderIndex = (CurrentOrderIndex + i) % PlayerOrder.Num();
        int32 PlayerIndex = PlayerOrder[NextOrderIndex];

        if (Players.IsValidIndex(PlayerIndex) && IsValid(Players[PlayerIndex]) && !Players[PlayerIndex]->bIsPendingDelete)
        {
            if (Players[PlayerIndex]->GetPlayerState() != EPlayerState::Player_Results &&
                Players[PlayerIndex]->GetPlayerState() != EPlayerState::Player_HoleOut)
            {
                UE_LOG(LogTemp, Log, TEXT("? Next player: %d (State: %s)"),
                    PlayerIndex, *UEnum::GetValueAsString(Players[PlayerIndex]->GetPlayerState()));
                return PlayerIndex;
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("?? No valid next player found"));
    return -1;
}

void UGolfPlayerManager::SetupNextPlayer(int32 NextPlayerIndex)
{
    if (!Players.IsValidIndex(NextPlayerIndex) || !PlayerBalls.IsValidIndex(NextPlayerIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("? Invalid player or ball index %d"), NextPlayerIndex);
        return;
    }

    AGolfPlayer* NextPlayer = Players[NextPlayerIndex];
    AGolfBall* NextBall = PlayerBalls[NextPlayerIndex];

    if (!IsValid(NextPlayer) || !IsValid(NextBall))
    {
        UE_LOG(LogTemp, Error, TEXT("? Invalid player or ball for index %d"), NextPlayerIndex);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("?? Setting up next player: %d (Tee box entry)"), NextPlayerIndex);

    int32 PreviousPlayerIndex = 0;
    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    if (GameMode)
    {
        PreviousPlayerIndex = GameMode->CurrentPlayerIndex;
        GameMode->CurrentPlayerIndex = NextPlayerIndex;
    }

    // ? FIXED: Safe logging that works with any number of players
    FString PlayerNames = TEXT("");
    for (int32 i = 0; i < Players.Num(); ++i)
    {
        if (Players.IsValidIndex(i) && IsValid(Players[i]))
        {
            if (i > 0) PlayerNames += TEXT(", ");
            PlayerNames += FString::Printf(TEXT("Player[%d] %s"), i, *Players[i]->GetName());
        }
    }
    UE_LOG(LogTemp, Log, TEXT("-------------------> Player %d ready for tee shot, ball visible ---- %s"),
        NextPlayerIndex, *PlayerNames);

    if (IsRangeMode())
    {
        GetWorld()->GetTimerManager().SetTimer(
            DelayedReadyTimer,
            [this, NextPlayerIndex]() {
                if (!Players.IsValidIndex(NextPlayerIndex) || !PlayerBalls.IsValidIndex(NextPlayerIndex))
                {
                    UE_LOG(LogTemp, Error, TEXT("? Invalid indices in delayed ready timer: %d"), NextPlayerIndex);
                    return;
                }

                // ? FIXED: Safe logging inside lambda too
                FString PlayerNames = TEXT("");
                for (int32 i = 0; i < Players.Num(); ++i)
                {
                    if (Players.IsValidIndex(i) && IsValid(Players[i]))
                    {
                        if (i > 0) PlayerNames += TEXT(", ");
                        PlayerNames += FString::Printf(TEXT("Player[%d] %s"), i, *Players[i]->GetName());
                    }
                }
                UE_LOG(LogTemp, Log, TEXT("-------------------> Player %d ready for tee shot, ball visible ---- %s"),
                    NextPlayerIndex, *PlayerNames);

                AGolfPlayer* Player = Players[NextPlayerIndex];
                AGolfBall* Ball = PlayerBalls[NextPlayerIndex];

                Player->SetPlayerState(EPlayerState::Player_Init);
                Ball->SetBallState(EBallState::Ball_Init);

                UE_LOG(LogTemp, Log, TEXT("?  ball_init player_init  %d ready for tee shot, ball visible"), NextPlayerIndex);
                if (!IsValid(Player) || !IsValid(Ball))
                {
                    UE_LOG(LogTemp, Error, TEXT("? Invalid player or ball in delayed ready timer: %d"), NextPlayerIndex);
                    return;
                }


                Ball->SetActorLocation(Player->BEFOREPos);

                UE_LOG(LogTemp, Log, TEXT("?  Ball->SetActorLocation  %d ready for tee shot, ball visible"), NextPlayerIndex);

                Ball->PrepareForTeeShot();
                Player->SetPlayerState(EPlayerState::Player_Ready);
                Ball->SetBallState(EBallState::Ball_Ready);
                Ball->SetBallForceHidden(false);
                Ball->SetBallVisibility(true, true);

                // ? 볼이 Ready 상태가 되면 센서 준비 상태 확인
                //CheckSensorReadyState(NextPlayerIndex);

                if (CameraManager)
                {
                    CameraManager->SetTargetBall(Ball);
                }
                UE_LOG(LogTemp, Log, TEXT("?  CameraManager->SetTargetBall  %d ready for tee shot, ball visible"), NextPlayerIndex);

                UE_LOG(LogTemp, Log, TEXT("? Player %d ready for tee shot, ball visible"), NextPlayerIndex);

                if (GEngine)
                {
                    GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
                        FString::Printf(TEXT("?? Player %d Ready"), NextPlayerIndex));
                }
            },
            0.5f,
                false
                );
    }
    else
    {

        if (IsStrokeMode())
        {
            if (GameMode->MiniMapWidget)
            {
                GameMode->MiniMapWidget->OnPlayerTurnChanged(NextPlayerIndex, PreviousPlayerIndex);
            }

            // PlayerController에도 알림
            if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
            {
                if (AGolfPlayerController* GolfPC = Cast<AGolfPlayerController>(PC))
                {
                    GolfPC->NotifyMiniMapPlayerChanged(NextPlayerIndex, PreviousPlayerIndex);
                }
            }
        }

        GetWorld()->GetTimerManager().SetTimer(
            DelayedReadyTimer,
            [this, NextPlayerIndex]() {
                if (!Players.IsValidIndex(NextPlayerIndex) || !PlayerBalls.IsValidIndex(NextPlayerIndex))
                {
                    UE_LOG(LogTemp, Error, TEXT("? Invalid indices in delayed ready timer: %d"), NextPlayerIndex);
                    return;
                }

                // ? FIXED: Safe logging in else branch too
                FString PlayerNames = TEXT("");
                for (int32 i = 0; i < Players.Num(); ++i)
                {
                    if (Players.IsValidIndex(i) && IsValid(Players[i]))
                    {
                        if (i > 0) PlayerNames += TEXT(", ");
                        PlayerNames += FString::Printf(TEXT("Player[%d] %s"), i, *Players[i]->GetName());
                    }
                }
                UE_LOG(LogTemp, Log, TEXT("-------------------> Player %d ready for tee shot, ball visible ---- %s"),
                    NextPlayerIndex, *PlayerNames);

                AGolfPlayer* Player = Players[NextPlayerIndex];
                AGolfBall* Ball = PlayerBalls[NextPlayerIndex];

                Player->SetPlayerState(EPlayerState::Player_Init);
                Ball->SetBallState(EBallState::Ball_Init);

                if (!IsValid(Player) || !IsValid(Ball))
                {
                    UE_LOG(LogTemp, Error, TEXT("? Invalid player or ball in delayed ready timer: %d"), NextPlayerIndex);
                    return;
                }

                if (CameraManager)
                {
                    CameraManager->SetTargetBall(Ball);
                }

                Ball->PrepareForTeeShot();
                Player->SetPlayerState(EPlayerState::Player_Ready);
                Ball->SetBallState(EBallState::Ball_Ready);
                Ball->SetBallForceHidden(false);
                Ball->SetBallVisibility(true, true);
                // ? 볼이 Ready 상태가 되면 센서 준비 상태 확인
                //CheckSensorReadyState(NextPlayerIndex);

  
                UE_LOG(LogTemp, Log, TEXT("? Player %d ready for tee shot, ball visible"), NextPlayerIndex);

                if (GEngine)
                {
                    GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
                        FString::Printf(TEXT("?? Player %d Ready"), NextPlayerIndex));
                }
            },
            0.5f,
                false
                );
    }

    UE_LOG(LogTemp, Log, TEXT("? Scheduled Des -> Init -> Ready transition for Player %d"), NextPlayerIndex);
}

void UGolfPlayerManager::HandleAllPlayersComplete()
{
    UE_LOG(LogTemp, Log, TEXT("?? All players completed their shots"));

    bool bAllResults = true;
    for (AGolfPlayer* Player : Players)
    {
        if (Player && Player->GetPlayerState() != EPlayerState::Player_Results)
        {
            bAllResults = false;
            UE_LOG(LogTemp, Warning, TEXT("?? Player not in Results state: %s"),
                *UEnum::GetValueAsString(Player->GetPlayerState()));
            break;
        }
    }

    if (bAllResults)
    {
        UE_LOG(LogTemp, Log, TEXT("??? All players finished, preparing for next round"));

        ResetAllPlayersToDes();

        GetWorld()->GetTimerManager().SetTimer(
            DelayedReadyTimer,
            [this]() {
                if (AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
                {
                    SortTeeShotPlayerOrderWithInit(GameMode->CurrentHole);
                    UE_LOG(LogTemp, Log, TEXT("? Players reordered for next round"));
                }
            },
            1.0f,
                false
                );
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("? Not all players in Results state, cannot proceed"));
        for (int32 i = 0; i < Players.Num(); i++)
        {
            if (Players[i])
            {
                UE_LOG(LogTemp, Warning, TEXT("Player %d State: %s"),
                    i, *UEnum::GetValueAsString(Players[i]->GetPlayerState()));
            }
        }
    }
}

void UGolfPlayerManager::ResetAllPlayersToDes()
{
    UE_LOG(LogTemp, Log, TEXT("?? Resetting all players to Des state"));

    for (int32 i = 0; i < Players.Num(); i++)
    {
        if (Players[i] && PlayerBalls[i])
        {
            Players[i]->SetPlayerState(EPlayerState::Player_Des);
            PlayerBalls[i]->SetBallState(EBallState::Ball_Init);
            PlayerBalls[i]->ResetForNewHole();
            PlayerBalls[i]->SetBallForceHidden(true);
            PlayerBalls[i]->SetBallVisibility(false, true);
            UE_LOG(LogTemp, Log, TEXT("?? Player %d reset to Des state, ball hidden"), i);
        }
    }
}

void UGolfPlayerManager::SortTeeShotPlayerOrder(int32 CurrentHole, bool bExcludeHoleOut)
{
    UE_LOG(LogTemp, Log, TEXT("----------------?? SortTeeShotPlayerOrder (ExcludeHoleOut=%d)"), bExcludeHoleOut ? 1 : 0);

    PlayerOrder.Empty();

    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    if (!GameMode)
    {
        UE_LOG(LogTemp, Error, TEXT("? Failed to get InGameMode in SortTeeShotPlayerOrder"));
        return;
    }

    if (CurrentHole == 1)
    {
        for (int32 i = 0; i < Players.Num(); i++)
        {
            if (bExcludeHoleOut)
            {
                if (!Players.IsValidIndex(i) || !IsValid(Players[i]))
                {
                    continue;
                }
                const bool bIsHoleOutPlayer =
                    Players[i]->GetPlayerState() == EPlayerState::Player_HoleOut ||
                    Players[i]->IsHoleIn() ||
                    Players[i]->GetPlayerInfo().bIsHoleout;
                if (bIsHoleOutPlayer)
                {
                    continue;
                }
            }
            PlayerOrder.Add(i);
            UE_LOG(LogTemp, Log, TEXT("PlayerOrder: Added Player %d for first hole"), i);
        }
        UE_LOG(LogTemp, Log, TEXT("SortTeeShotPlayerOrder: First hole, ordered by player index: %s"),
            *FString::JoinBy(PlayerOrder, TEXT(", "), [](const int32& Index) { return FString::FromInt(Index); }));
    }
    else
    {
        TArray<int32> PlayerIndices;
        for (int32 i = 0; i < Players.Num(); i++)
        {
            if (bExcludeHoleOut)
            {
                if (!Players.IsValidIndex(i) || !IsValid(Players[i]))
                {
                    continue;
                }
                const bool bIsHoleOutPlayer =
                    Players[i]->GetPlayerState() == EPlayerState::Player_HoleOut ||
                    Players[i]->IsHoleIn() ||
                    Players[i]->GetPlayerInfo().bIsHoleout;
                if (bIsHoleOutPlayer)
                {
                    continue;
                }
            }
            PlayerIndices.Add(i);
        }

        // 커스텀 정렬: 이전 홀 스코어를 재귀적으로 비교
        PlayerIndices.Sort([this, CurrentHole](const int32& A, const int32& B) {
            // 이전 홀부터 거슬러 올라가며 비교
            for (int32 HoleOffset = 1; HoleOffset < CurrentHole; HoleOffset++)
            {
                int32 HoleIndex = CurrentHole - 1 - HoleOffset; // 배열 인덱스 (0-based)

                int32 ScoreA = 999; // 기본값 (스코어 없을 경우)
                int32 ScoreB = 999;

                // Player A의 스코어
                if (Players.IsValidIndex(A) && Players[A]->GetPlayerInfo().HoleScores.IsValidIndex(HoleIndex))
                {
                    ScoreA = Players[A]->GetPlayerInfo().HoleScores[HoleIndex];
                }

                // Player B의 스코어
                if (Players.IsValidIndex(B) && Players[B]->GetPlayerInfo().HoleScores.IsValidIndex(HoleIndex))
                {
                    ScoreB = Players[B]->GetPlayerInfo().HoleScores[HoleIndex];
                }

                // 스코어가 다르면 낮은 쪽이 먼저
                if (ScoreA != ScoreB)
                {
                    UE_LOG(LogTemp, Log, TEXT("?? Comparing Hole %d: Player %d (Score=%d) vs Player %d (Score=%d)"),
                        HoleIndex + 1, A, ScoreA, B, ScoreB);
                    return ScoreA < ScoreB;
                }
                // 동점이면 다음 이전 홀로 계속 진행
            }

            // 모든 홀이 동점이면 플레이어 인덱스로 정렬
            UE_LOG(LogTemp, Log, TEXT("?? All holes tied: Player %d vs Player %d, sorting by index"), A, B);
            return A < B;
            });

        // 정렬된 순서를 PlayerOrder에 저장
        for (int32 PlayerIndex : PlayerIndices)
        {
            PlayerOrder.Add(PlayerIndex);

            // 디버그: 각 플레이어의 이전 홀 스코어 출력
            if (Players.IsValidIndex(PlayerIndex))
            {
                FString ScoreHistory = TEXT("");
                for (int32 h = 0; h < CurrentHole - 1; h++)
                {
                    if (Players[PlayerIndex]->GetPlayerInfo().HoleScores.IsValidIndex(h))
                    {
                        ScoreHistory += FString::Printf(TEXT("Hole%d:%d "), h + 1,
                            Players[PlayerIndex]->GetPlayerInfo().HoleScores[h]);
                    }
                }
                UE_LOG(LogTemp, Log, TEXT("PlayerOrder: Player %d - %s"), PlayerIndex, *ScoreHistory);
            }
        }
    }

    if (GameMode->bIsContinueGame && PlayerOrder.Num() > 0)
    {
        const auto IsHoleOutPlayer = [this](int32 PlayerIndex)
        {
            if (!Players.IsValidIndex(PlayerIndex) || !IsValid(Players[PlayerIndex]))
            {
                return true;
            }
            const FPlayerInfo& Info = Players[PlayerIndex]->GetPlayerInfo();
            return Players[PlayerIndex]->GetPlayerState() == EPlayerState::Player_HoleOut ||
                Players[PlayerIndex]->IsHoleIn() ||
                Info.bIsHoleout;
        };

        const int32 ResumeIndex = GameMode->GameInfo.CurrentPlayerIndex;
        int32 ResumeOrderPos = PlayerOrder.Find(ResumeIndex);

        if (ResumeOrderPos == INDEX_NONE || IsHoleOutPlayer(PlayerOrder[ResumeOrderPos]))
        {
            ResumeOrderPos = INDEX_NONE;
            for (int32 i = 0; i < PlayerOrder.Num(); ++i)
            {
                if (!IsHoleOutPlayer(PlayerOrder[i]))
                {
                    ResumeOrderPos = i;
                    break;
                }
            }
            if (ResumeOrderPos == INDEX_NONE)
            {
                UE_LOG(LogTemp, Warning, TEXT("Resume order: no valid (non-holeout) player found"));
            }
        }

        if (ResumeOrderPos > 0)
        {
            TArray<int32> RotatedOrder;
            RotatedOrder.Reserve(PlayerOrder.Num());
            for (int32 i = 0; i < PlayerOrder.Num(); ++i)
            {
                RotatedOrder.Add(PlayerOrder[(ResumeOrderPos + i) % PlayerOrder.Num()]);
            }
            PlayerOrder = MoveTemp(RotatedOrder);
            UE_LOG(LogTemp, Log, TEXT("Resume order: starting from PlayerOrder[0]=%d"), PlayerOrder[0]);
        }
        else if (ResumeOrderPos == INDEX_NONE)
        {
            UE_LOG(LogTemp, Warning, TEXT("Resume order: keeping default order"));
        }
    }

    // 첫 번째 플레이어 설정
    if (PlayerOrder.IsValidIndex(0))
    {
        int32 FirstPlayerIndex = PlayerOrder[0];
        if (PlayerBalls.IsValidIndex(FirstPlayerIndex) && Players.IsValidIndex(FirstPlayerIndex))
        {
            Players[FirstPlayerIndex]->SetPlayerState(EPlayerState::Player_Init);
            PlayerBalls[FirstPlayerIndex]->SetBallState(EBallState::Ball_Init);
            if (CameraManager)
            {
                CameraManager->SetTargetBall(PlayerBalls[FirstPlayerIndex]);
                CameraManager->ChangeCameraMode(ECameraMode::Ready);
            }
            GameMode->CurrentPlayerIndex = FirstPlayerIndex;
            if (GameMode->bIsContinueGame)
            {
                GameMode->GameInfo.CurrentPlayerIndex = FirstPlayerIndex;
            }

            GetWorld()->GetTimerManager().SetTimer(
                DelayedReadyTimer,
                [this, FirstPlayerIndex]() {
                    if (Players.IsValidIndex(FirstPlayerIndex) && PlayerBalls.IsValidIndex(FirstPlayerIndex))
                    {
						PlayerBalls[FirstPlayerIndex]->PrepareForTeeShot();
						Players[FirstPlayerIndex]->SetPlayerState(EPlayerState::Player_Ready);
						PlayerBalls[FirstPlayerIndex]->SetBallState(EBallState::Ball_Ready);
						PlayerBalls[FirstPlayerIndex]->SetBallForceHidden(false);
						PlayerBalls[FirstPlayerIndex]->SetBallVisibility(true, true);
                       // CheckSensorReadyState(FirstPlayerIndex);
                        // 센서 기본 설정 (드라이버 클럽으로 초기 설정)
                        AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
                        if(GameMode->CheckFirstShot())
                            SetSensorClub(CR2CLUB_DRIVER);
                        else
                            SetSensorClub(CR2CLUB_IRON7);
                        SensorManager->ConfigureSensor(2.67f, 2);



                        UE_LOG(LogTemp, Log, TEXT("? First player %d ready (Init -> Ready), ball visible"), FirstPlayerIndex);
                    }
                },
                0.5f,
                    false
                    );

            UE_LOG(LogTemp, Log, TEXT("?? First player %d set to Init, will transition to Ready"), FirstPlayerIndex);
        }
    }
}

void UGolfPlayerManager::SortTeeShotPlayerOrderWithInit(int32 CurrentHole)
{
    UE_LOG(LogTemp, Log, TEXT("-------------------------?? SortTeeShotPlayerOrderWithInit tee shot order with Init for hole %d"), CurrentHole);

    PlayerOrder.Empty();

    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    if (!GameMode)
    {
        UE_LOG(LogTemp, Error, TEXT("? Failed to get InGameMode in SortTeeShotPlayerOrderWithInit"));
        return;
    }

    if (CurrentHole == 1)
    {
        for (int32 i = 0; i < Players.Num(); i++)
        {
            PlayerOrder.Add(i);
            UE_LOG(LogTemp, Log, TEXT("PlayerOrder: Added Player %d"), i);
        }
        FString OrderString;
        for (int32 i = 0; i < PlayerOrder.Num(); i++)
        {
            OrderString += FString::FromInt(PlayerOrder[i]);
            if (i < PlayerOrder.Num() - 1)
            {
                OrderString += TEXT(", ");
            }
        }
        UE_LOG(LogTemp, Log, TEXT("SortTeeShotPlayerOrderWithInit: First hole, ordered by player index: %s"), *OrderString);
    }
    else
    {
        TArray<int32> PlayerIndices;
        for (int32 i = 0; i < Players.Num(); i++)
        {
            PlayerIndices.Add(i);
        }

        // 커스텀 정렬: 이전 홀 스코어를 재귀적으로 비교
        PlayerIndices.Sort([this, CurrentHole](const int32& A, const int32& B) {
            // 이전 홀부터 거슬러 올라가며 비교
            for (int32 HoleOffset = 1; HoleOffset < CurrentHole; HoleOffset++)
            {
                int32 HoleIndex = CurrentHole - 1 - HoleOffset; // 배열 인덱스 (0-based)

                int32 ScoreA = 999; // 기본값 (스코어 없을 경우)
                int32 ScoreB = 999;

                // Player A의 스코어
                if (Players.IsValidIndex(A) && Players[A]->GetPlayerInfo().HoleScores.IsValidIndex(HoleIndex))
                {
                    ScoreA = Players[A]->GetPlayerInfo().HoleScores[HoleIndex];
                }

                // Player B의 스코어
                if (Players.IsValidIndex(B) && Players[B]->GetPlayerInfo().HoleScores.IsValidIndex(HoleIndex))
                {
                    ScoreB = Players[B]->GetPlayerInfo().HoleScores[HoleIndex];
                }

                // 스코어가 다르면 낮은 쪽이 먼저
                if (ScoreA != ScoreB)
                {
                    UE_LOG(LogTemp, Log, TEXT("?? Comparing Hole %d: Player %d (Score=%d) vs Player %d (Score=%d)"),
                        HoleIndex + 1, A, ScoreA, B, ScoreB);
                    return ScoreA < ScoreB;
                }
                // 동점이면 다음 이전 홀로 계속 진행
            }

            // 모든 홀이 동점이면 플레이어 인덱스로 정렬
            UE_LOG(LogTemp, Log, TEXT("?? All holes tied: Player %d vs Player %d, sorting by index"), A, B);
            return A < B;
            });

        // 정렬된 순서를 PlayerOrder에 저장
        for (int32 PlayerIndex : PlayerIndices)
        {
            PlayerOrder.Add(PlayerIndex);

            // 디버그: 각 플레이어의 이전 홀 스코어 출력
            if (Players.IsValidIndex(PlayerIndex))
            {
                FString ScoreHistory = TEXT("");
                for (int32 h = 0; h < CurrentHole - 1; h++)
                {
                    if (Players[PlayerIndex]->GetPlayerInfo().HoleScores.IsValidIndex(h))
                    {
                        ScoreHistory += FString::Printf(TEXT("Hole%d:%d "), h + 1,
                            Players[PlayerIndex]->GetPlayerInfo().HoleScores[h]);
                    }
                }
                UE_LOG(LogTemp, Log, TEXT("PlayerOrder: Player %d - %s"), PlayerIndex, *ScoreHistory);
            }
        }
    }

    if (GameMode->bIsContinueGame && PlayerOrder.Num() > 0)
    {
        const auto IsHoleOutPlayer = [this](int32 PlayerIndex)
        {
            if (!Players.IsValidIndex(PlayerIndex) || !IsValid(Players[PlayerIndex]))
            {
                return true;
            }
            const FPlayerInfo& Info = Players[PlayerIndex]->GetPlayerInfo();
            return Players[PlayerIndex]->GetPlayerState() == EPlayerState::Player_HoleOut ||
                Players[PlayerIndex]->IsHoleIn() ||
                Info.bIsHoleout;
        };

        const int32 ResumeIndex = GameMode->GameInfo.CurrentPlayerIndex;
        int32 ResumeOrderPos = PlayerOrder.Find(ResumeIndex);

        if (ResumeOrderPos == INDEX_NONE || IsHoleOutPlayer(PlayerOrder[ResumeOrderPos]))
        {
            ResumeOrderPos = INDEX_NONE;
            for (int32 i = 0; i < PlayerOrder.Num(); ++i)
            {
                if (!IsHoleOutPlayer(PlayerOrder[i]))
                {
                    ResumeOrderPos = i;
                    break;
                }
            }
            if (ResumeOrderPos == INDEX_NONE)
            {
                UE_LOG(LogTemp, Warning, TEXT("Resume order (init): no valid (non-holeout) player found"));
            }
        }

        if (ResumeOrderPos > 0)
        {
            TArray<int32> RotatedOrder;
            RotatedOrder.Reserve(PlayerOrder.Num());
            for (int32 i = 0; i < PlayerOrder.Num(); ++i)
            {
                RotatedOrder.Add(PlayerOrder[(ResumeOrderPos + i) % PlayerOrder.Num()]);
            }
            PlayerOrder = MoveTemp(RotatedOrder);
            UE_LOG(LogTemp, Log, TEXT("Resume order (init): starting from PlayerOrder[0]=%d"), PlayerOrder[0]);
        }
        else if (ResumeOrderPos == INDEX_NONE)
        {
            UE_LOG(LogTemp, Warning, TEXT("Resume order (init): keeping default order"));
        }
    }

    // 첫 번째 플레이어 설정
    if (PlayerOrder.IsValidIndex(0))
    {
        int32 FirstPlayerIndex = PlayerOrder[0];
        if (PlayerBalls.IsValidIndex(FirstPlayerIndex) && Players.IsValidIndex(FirstPlayerIndex))
        {
            if (CameraManager)
            {
                CameraManager->SetTargetBall(PlayerBalls[FirstPlayerIndex]);
                CameraManager->ChangeCameraMode(ECameraMode::Ready);
            }
            GameMode->CurrentPlayerIndex = FirstPlayerIndex;
            if (GameMode->bIsContinueGame)
            {
                GameMode->GameInfo.CurrentPlayerIndex = FirstPlayerIndex;
            }

            Players[FirstPlayerIndex]->SetPlayerState(EPlayerState::Player_Init);
            GetWorld()->GetTimerManager().SetTimer(
                DelayedReadyTimer,
                [this, FirstPlayerIndex]() {
                    if (Players.IsValidIndex(FirstPlayerIndex) && PlayerBalls.IsValidIndex(FirstPlayerIndex))
                    {
						PlayerBalls[FirstPlayerIndex]->PrepareForTeeShot();
						Players[FirstPlayerIndex]->SetPlayerState(EPlayerState::Player_Ready);
						PlayerBalls[FirstPlayerIndex]->SetBallState(EBallState::Ball_Ready);
						PlayerBalls[FirstPlayerIndex]->SetBallForceHidden(false);
						PlayerBalls[FirstPlayerIndex]->SetBallVisibility(true, true);

                       // CheckSensorReadyState(FirstPlayerIndex);

                        UE_LOG(LogTemp, Log, TEXT("? First player %d ready (Init -> Ready), ball visible"), FirstPlayerIndex);
                    }
                },
                0.5f,
                    false
                    );

            UE_LOG(LogTemp, Log, TEXT("?? First player %d set to Init, will transition to Ready"), FirstPlayerIndex);
        }
    }
}

void UGolfPlayerManager::SortPlayersByDistance()
{
    UE_LOG(LogTemp, Log, TEXT("?? Sorting players by distance to holecup"));

    if (!GetWorld())
    {
        UE_LOG(LogTemp, Error, TEXT("? World is null in SortPlayersByDistance"));
        return;
    }

    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    if (!GameMode || !GameMode->MapInfo.HolecupPositions.IsValidIndex(GameMode->CurrentHole - 1))
    {
        UE_LOG(LogTemp, Error, TEXT("? Invalid GameMode or Holecup position"));
        return;
    }

    FVector HolecupPos = GameMode->MapInfo.HolecupPositions[GameMode->CurrentHole - 1];

    TArray<TPair<int32, float>> Distances;
    for (int32 i = 0; i < PlayerBalls.Num(); i++)
    {
        if (PlayerBalls.IsValidIndex(i) && IsValid(PlayerBalls[i]) &&
            Players.IsValidIndex(i) && IsValid(Players[i]) &&
            !Players[i]->bIsPendingDelete &&
            Players[i]->GetPlayerState() != EPlayerState::Player_HoleOut)
        {
            FVector BallPos = PlayerBalls[i]->GetActorLocation();
            float Distance = FVector::Dist(BallPos, HolecupPos);
            Distances.Add(TPair<int32, float>(i, Distance));
            UE_LOG(LogTemp, Log, TEXT("Player %d: Distance to holecup=%.1f cm"), i, Distance);
        }
    }

    Distances.Sort([](const TPair<int32, float>& A, const TPair<int32, float>& B) {
        return A.Value > B.Value || (A.Value == B.Value && A.Key < B.Key);
        });

    PlayerOrder.Empty();
    for (const auto& Pair : Distances)
    {
        PlayerOrder.Add(Pair.Key);
        UE_LOG(LogTemp, Log, TEXT("PlayerOrder: Added Player %d (Distance=%.1f cm)"), Pair.Key, Pair.Value);
    }

    FString OrderString;
    for (int32 i = 0; i < PlayerOrder.Num(); i++)
    {
        OrderString += FString::FromInt(PlayerOrder[i]);
        if (i < PlayerOrder.Num() - 1) OrderString += TEXT(", ");
    }
    UE_LOG(LogTemp, Log, TEXT("SortPlayersByDistance: Ordered by distance: %s"), *OrderString);
}

void UGolfPlayerManager::OnPlayerBallStateChanged(EBallState NewState, int32 OwningPlayerIndex)
{
    UE_LOG(LogTemp, Log, TEXT("UGolfPlayerManager: Ball %d state changed to %s"), OwningPlayerIndex, *UEnum::GetValueAsString(NewState));
    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());

    if (NewState == EBallState::Ball_Stop)
    {
        if (Players.IsValidIndex(OwningPlayerIndex) && PlayerBalls.IsValidIndex(OwningPlayerIndex))
        {
            AGolfPlayer* CurrentPlayer = Players[OwningPlayerIndex];
            AGolfBall* CurrentBall = PlayerBalls[OwningPlayerIndex];

            if (IsValid(CurrentPlayer) && IsValid(CurrentBall))
            {
                CurrentPlayer->UpdateBallPosition(CurrentBall->GetActorLocation());

                bool bHoleIn = CurrentBall->IsHoleIn();
                bool bOutOfBounds = CurrentBall->IsOutOfBounds();
                bool bConceded = CurrentBall->IsConceded();
                LatestShotPlayer = CurrentPlayer;

                CurrentPlayer->ProcessShotResult(bHoleIn, bOutOfBounds, bConceded);
            }
        }
    }
    else if (NewState == EBallState::Ball_Ready)
    {
      //  if (GameMode->GetCurrentTurnGolfPlayer()->GetCurrentHoleShotCount() > 0)
        if (this)
			CheckSensorReadyState(OwningPlayerIndex);
    }
}

// 게임 모드 확인 함수들 구현
bool UGolfPlayerManager::IsStrokeMode() const
{
    if (AInGameMode* GameMode = GetInGameMode())
    {
        return GameMode->IsStrokeMode();
    }
    return true; // 기본값은 Stroke Mode
}

bool UGolfPlayerManager::IsTrainingMode() const
{
    if (AInGameMode* GameMode = GetInGameMode())
    {
        return GameMode->IsTrainingMode();
    }
    return false;
}

bool UGolfPlayerManager::IsRangeMode() const
{
    if (AInGameMode* GameMode = GetInGameMode())
    {
        return GameMode->IsRangeMode();
    }
    return false;
}

AInGameMode* UGolfPlayerManager::GetInGameMode() const
{
    if (UWorld* World = GetWorld())
    {
        return Cast<AInGameMode>(World->GetAuthGameMode());
    }
    return nullptr;
}

/**
 * 특정 플레이어를 강제로 홀 아웃시키고 다음 플레이어로 넘어갑니다.
 *
 * @param PlayerIndex - 홀 아웃시킬 플레이어 인덱스
 */
void UGolfPlayerManager::ForcePlayerHoleOutAndAdvance(int32 PlayerIndex)
{
    UE_LOG(LogTemp, Log, TEXT("?? 플레이어 %d 강제 홀 아웃 및 턴 넘기기"), PlayerIndex);

    if (!Players.IsValidIndex(PlayerIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("? 잘못된 PlayerIndex: %d"), PlayerIndex);
        return;
    }

    AGolfPlayer* Player = Players[PlayerIndex];
    if (!IsValid(Player))
    {
        UE_LOG(LogTemp, Error, TEXT("? 플레이어가 유효하지 않음"));
        return;
    }

    // 플레이어를 홀 아웃 상태로
    Player->SetPlayerState(EPlayerState::Player_HoleOut);
    UE_LOG(LogTemp, Log, TEXT("? 플레이어 %d → Player_HoleOut"), PlayerIndex);

    // 현재 플레이어가 홀 아웃된 플레이어인 경우 다음으로 넘기기
    AInGameMode* GameMode = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    if (GameMode && GameMode->CurrentPlayerIndex == PlayerIndex)
    {
        SkipCurrentPlayerTurn();
    }
}


/**
 * 현재 플레이어가 마지막 유효한 플레이어인지 확인합니다.
 *
 * @param CurrentPlayerIndex - 확인할 플레이어 인덱스
 * @return true면 마지막 유효한 플레이어, false면 다른 플레이어가 남아있음
 */
bool UGolfPlayerManager::IsLastValidPlayer(int32 CurrentPlayerIndex) const
{
    int32 ValidPlayerCount = 0;

    for (AGolfPlayer* Player : Players)
    {
        if (IsValid(Player) && Player->GetPlayerState() != EPlayerState::Player_HoleOut)
        {
            ValidPlayerCount++;

            if (ValidPlayerCount > 1)
            {
                return false; // 2명 이상이면 마지막이 아님
            }
        }
    }

    // 유효한 플레이어가 1명이고, 그게 현재 플레이어인지 확인
    if (ValidPlayerCount == 1 &&
        Players.IsValidIndex(CurrentPlayerIndex) &&
        IsValid(Players[CurrentPlayerIndex]) &&
        Players[CurrentPlayerIndex]->GetPlayerState() != EPlayerState::Player_HoleOut)
    {
        return true;
    }

    return false;
}


/**
 * 남은 유효한 플레이어 수를 반환합니다.
 *
 * @return 홀 아웃하지 않은 플레이어 수
 */
int32 UGolfPlayerManager::GetRemainingPlayerCount() const
{
    int32 Count = 0;

    for (AGolfPlayer* Player : Players)
    {
        if (IsValid(Player) && Player->GetPlayerState() != EPlayerState::Player_HoleOut)
        {
            Count++;
        }
    }

    UE_LOG(LogTemp, Log, TEXT("?? 남은 플레이어 수: %d / %d"), Count, Players.Num());
    return Count;
}


void UGolfPlayerManager::StartSensorReadyCheck(int32 PlayerIndex)
{
    // 이전 타이머 정리
    if (GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(SensorCheckTimerHandle))
    {
        GetWorld()->GetTimerManager().ClearTimer(SensorCheckTimerHandle);
    }

    // 체크 상태 초기화
    SensorCheckingPlayerIndex = PlayerIndex;
    SensorCheckAttempts = 0;

    UE_LOG(LogTemp, Log, TEXT("?? Starting sensor ready check for Player %d"), PlayerIndex);

    // 새 타이머 설정
    GetWorld()->GetTimerManager().SetTimer(
        SensorCheckTimerHandle,
        [this, PlayerIndex]()
        {
            // 레벨 이동 중에도 안전하게 체크
            if (IsValid(this) && GetWorld())
            {
                CheckSensorReadyState(PlayerIndex);
            }
        },
        0.5f,  // 0.5초마다 체크
            true   // 반복 실행
            );
}

void UGolfPlayerManager::StopSensorReadyCheck()
{
    if (GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(SensorCheckTimerHandle))
    {
        GetWorld()->GetTimerManager().ClearTimer(SensorCheckTimerHandle);
        UE_LOG(LogTemp, Log, TEXT("?? Sensor ready check timer cleared"));
    }

    SensorCheckingPlayerIndex = -1;
    SensorCheckAttempts = 0;
}


void UGolfPlayerManager::OnLevelUnload()
{
    UE_LOG(LogTemp, Log, TEXT("?? Level unloading - cleaning up timers and sensor"));

    // 모든 타이머 정리
    StopSensorReadyCheck();
    bShuttingDown = true;

    if (GetWorld())
    {
        if (GetWorld()->GetTimerManager().IsTimerActive(DelayedReadyTimer))
        {
            GetWorld()->GetTimerManager().ClearTimer(DelayedReadyTimer);
        }
        if (GetWorld()->GetTimerManager().IsTimerActive(SensorCheckTimer))
        {
            GetWorld()->GetTimerManager().ClearTimer(SensorCheckTimer);
        }
        if (GetWorld()->GetTimerManager().IsTimerActive(TH))
        {
            GetWorld()->GetTimerManager().ClearTimer(TH);
        }
        if (GetWorld()->GetTimerManager().IsTimerActive(TH2))
        {
            GetWorld()->GetTimerManager().ClearTimer(TH2);
        }
    }

    // 센서 정지
    if (SensorManager && IsValid(SensorManager))
    {
        SensorManager->OnShotDetected.RemoveAll(this);
        SensorManager->OnShotDetectedEx.RemoveAll(this);
        SensorManager->OnBallReady.RemoveAll(this);
        SensorManager->OnSensorStatusChanged.RemoveAll(this);
        SensorManager->StopSensorOperation();
        SensorManager->ShutdownSensor();
        SensorManager->Destroy();
        UE_LOG(LogTemp, Log, TEXT("?? Sensor operation stopped"));
    }
    SensorManager = nullptr;

    bSensorReady = false;
    CurrentActivePlayerIndex = -1;
}

