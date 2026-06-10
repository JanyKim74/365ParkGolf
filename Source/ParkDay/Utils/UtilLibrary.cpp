#include "UtilLibrary.h"
#include "../GolfPlayer.h"
#include "../GolfPlayerManager.h"
#include "../InGameMode.h"
#include "ParkDay/ExternalPakManager.h"
#include "ParkDay/TerraParkGameInstance.h"
#include "ParkDay/Widgets/FadeWidget.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "../JsonHandler.h"

static FVector MakeDirectionAToB(const FVector& A, const FVector& B, bool bKeepAZ)
{
    FVector Dir = (B - A);

    if (bKeepAZ)
    {
        Dir.Z = 0.0f; // 평면 이동을 원하면 Z 제거
    }

    // 너무 가까워서 방향 벡터가 0에 수렴하면 안전 처리
    return Dir.GetSafeNormal();
}

AGameModeBase* UUtilLibrary::GetGameModeBP(UObject* WorldContextObject)
{
    // 방법 A: UGameplayStatics 사용
    return UGameplayStatics::GetGameMode(WorldContextObject);

    // 방법 B: 월드 꺼내서 직접
    // if (UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr)
    // {
    //     return World->GetAuthGameMode(); // 서버에서만 유효
    // }
    // return nullptr;
}


bool UUtilLibrary::MoveActorTowardActorByDistance_KeepRotation(
    AActor* Mover,
    const AActor* Target,
    float DistanceUU,
    bool bKeepMoverZ,
    bool bSweep,
    bool bTeleport,
    FHitResult& OutHit
)
{
    OutHit = FHitResult();

    if (!Mover || !Target)
    {
        return false;
    }

    // 이동 전 회전 저장
    const FRotator SavedRot = Mover->GetActorRotation();

    const FVector MoverLoc = Mover->GetActorLocation();
    const FVector TargetLoc = Target->GetActorLocation();

    const FVector Dir = MakeDirectionAToB(MoverLoc, TargetLoc, bKeepMoverZ);
    if (Dir.IsNearlyZero())
    {
        return false;
    }

    FVector NewLoc = MoverLoc + (Dir * DistanceUU);

    if (bKeepMoverZ)
    {
        NewLoc.Z = MoverLoc.Z;
    }

    const ETeleportType TeleportType = bTeleport ? ETeleportType::TeleportPhysics : ETeleportType::None;

    // 위치만 이동
    const bool bMoved = Mover->SetActorLocation(NewLoc, bSweep, &OutHit, TeleportType);

    // 어떤 이유로든 회전이 바뀌었으면 원복 (충돌 스윕/물리/부착 관계 등 대비)
    // SetActorRotation은 위치를 건드리지 않게 하기 위해 TeleportType 동일 적용
    if (Mover->GetActorRotation() != SavedRot)
    {
        Mover->SetActorRotation(SavedRot, TeleportType);
    }

    return bMoved;
}

bool UUtilLibrary::MoveActorTowardActorByDistanceSimple_KeepRotation(
    AActor* Mover,
    const AActor* Target,
    float DistanceUU,
    bool bKeepMoverZ
)
{
    FHitResult DummyHit;
    return MoveActorTowardActorByDistance_KeepRotation(
        Mover, Target, DistanceUU,
        bKeepMoverZ,
        /*bSweep*/false,
        /*bTeleport*/false,
        DummyHit
    );
}

bool UUtilLibrary::MoveActorTowardActorByDistance(
    AActor* Mover,
    const AActor* Target,
    float DistanceUU,
    bool bKeepMoverZ,
    bool bSweep,
    bool bTeleport,
    FHitResult& OutHit
)
{
    OutHit = FHitResult();

    if (!Mover || !Target)
    {
        return false;
    }

    const FVector MoverLoc = Mover->GetActorLocation();
    const FVector TargetLoc = Target->GetActorLocation();

    const FVector Dir = MakeDirectionAToB(MoverLoc, TargetLoc, bKeepMoverZ);
    if (Dir.IsNearlyZero())
    {
        // 같은 위치이거나(또는 Z 제거 후) 방향을 만들 수 없는 경우
        return false;
    }

    FVector NewLoc = MoverLoc + (Dir * DistanceUU);

    // “Mover의 Z 유지” 옵션이면 최종 Z 고정
    if (bKeepMoverZ)
    {
        NewLoc.Z = MoverLoc.Z;
    }

    const ETeleportType TeleportType = bTeleport ? ETeleportType::TeleportPhysics : ETeleportType::None;

    // SetActorLocation은 내부적으로 RootComponent 기준으로 이동
    const bool bMoved = Mover->SetActorLocation(NewLoc, bSweep, &OutHit, TeleportType);

    return bMoved;
}

bool UUtilLibrary::MoveActorTowardActorByDistanceSimple(
    AActor* Mover,
    const AActor* Target,
    float DistanceUU,
    bool bKeepMoverZ
)
{
    FHitResult DummyHit;
    return MoveActorTowardActorByDistance(Mover, Target, DistanceUU, bKeepMoverZ, /*bSweep*/false, /*bTeleport*/false, DummyHit);
}

APlayerController* UUtilLibrary::GetPlayerControllerBP(UObject* WorldContextObject)
{
    return UGameplayStatics::GetPlayerController(WorldContextObject, 0);
}

void UUtilLibrary::SoftResetGameInfo(UObject* WorldContextObject, UPARAM(ref) 
    FGameInfo& GameInfo)
{
    GameInfo.SoftReset();
}

void UUtilLibrary::StopLoadingScreen(UObject* WorldContextObject)
{
    Cast<UTerraParkgameInstance>(WorldContextObject->GetWorld()->GetGameInstance())->StopLoadingScreen();
}

void UUtilLibrary::SortPlayersBySlot(UObject* WorldContextObject, UPARAM(ref) TArray<FPlayerInfo>& Players)
{
    Algo::SortBy(Players, &FPlayerInfo::SlotIndex, TLess<int32>()); // 오름차순

    //for (int32 i = 0 ; i < Players.Num() ; i++)
    //{
	   // FPlayerInfo& Player = Players[i];
    //    Player.SlotIndex = i + 1;
    //}
}

void UUtilLibrary::SortString(UObject* WorldContextObject, UPARAM(ref)TArray<FString>& Strings)
{
    Strings.Sort([](const FString& A, const FString& B) {
        return A.Compare(B, ESearchCase::CaseSensitive) < 0;
        });
}

static FString NormalizeLongLevelPath(const FString& InPath)
{
    FString Out = InPath;
    Out.TrimStartAndEndInline();
    Out.ReplaceInline(TEXT("\\"), TEXT("/"));
    if (Out.EndsWith(TEXT(".umap"), ESearchCase::IgnoreCase)) { Out = Out.LeftChop(5); }
    //if (!Out.IsEmpty() && !Out.StartsWith(TEXT("/"))) { Out = TEXT("/") + Out; }
    return Out;
}

int32 UUtilLibrary::GetCurrentPlayerShotCount(UObject* WorldContextObject)
{
    int32 Result = 0;
    if (AInGameMode* GM = Cast<AInGameMode>(GetGameModeBP(WorldContextObject)))
    	if (AGolfPlayer* Player = GM->PlayerManager->GetPlayers()[GM->CurrentPlayerIndex])
            Result = Player->PlayerInfo.ShotCountPerHole[GM->CurrentHole - 1];
   
    return Result;
}

int32 UUtilLibrary::GetCurrentPlayerIndex(UObject* WorldContextObject)
{
    if (AInGameMode* GM = Cast<AInGameMode>(GetGameModeBP(WorldContextObject)))
        if (AGolfPlayer* Player = GM->PlayerManager->GetPlayers()[GM->CurrentPlayerIndex])
            return Player->PlayerIndex;

    return 0;
}


int32 UUtilLibrary::GetCurrentScore(UObject* WorldContextObject)
{
    int32 Result = 0;
    if (AInGameMode* GM = Cast<AInGameMode>(GetGameModeBP(WorldContextObject)))
        if (AGolfPlayer* Player = GM->PlayerManager->GetPlayers()[GM->CurrentPlayerIndex])
            Result = Player->PlayerInfo.ShotCountPerHole[GM->CurrentHole - 1] - GM->GameInfo.SelectedMap.ParScores[GM->CurrentHole - 1];

    return Result;
}


bool UUtilLibrary::GetIsConcede(UObject* WorldContextObject)
{
    if (!WorldContextObject)
    {
        UE_LOG(LogTemp, Error, TEXT("OpenLevel_Long: WorldContextObject is null"));
        return false;
    }

    AInGameMode* GM = Cast<AInGameMode>(GetGameModeBP(WorldContextObject));
    if (GM)
    {
        if (GM->PlayerManager->GetPlayerBalls().IsValidIndex(GM->CurrentPlayerIndex))
        {
            return GM->PlayerManager->GetPlayerBalls()[GM->CurrentPlayerIndex]->GetIsConcede();
        }
    }

    return false;
}

void UUtilLibrary::OpenLevelCPP(UObject* WorldContextObject, const FString& LongPackageLevelPath,
	const FString& Options)
{
    if (!WorldContextObject)
    {
        UE_LOG(LogTemp, Error, TEXT("OpenLevel_Long: WorldContextObject is null"));
        return;
    }

#if !WITH_EDITOR

    UExternalPakManager* PM = WorldContextObject->GetWorld()->GetGameInstance()->GetSubsystem<UExternalPakManager>();
    if (PM)
    {
        if (!PM->MountPakByName(LongPackageLevelPath + TEXT(".pak"), 1000))
        {
            UE_LOG(LogTemp, Error, TEXT("%s.pak mount failed"), *LongPackageLevelPath);
        }
        // RegisterAll 호출 제거: 잘못된 마운트 포인트 등록이 /Game/ 경로를 오염시킴
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("ExternalPakManager is null"));
    }

#endif


    const FString LevelPathName = FString::Printf(TEXT("/Game/%s/%s"), *LongPackageLevelPath, *LongPackageLevelPath);

    const FString Norm = NormalizeLongLevelPath(LongPackageLevelPath);
    const FName LevelFName(*LevelPathName);
    UE_LOG(LogTemp, Log, TEXT("-----------------OpenLevelCPP =[%s]    = %s"), *LongPackageLevelPath, *Norm);
    // UGameplayStatics::OpenLevel: true/false 반환 (성공 시 true)
   UGameplayStatics::OpenLevel(WorldContextObject, LevelFName, /*bAbsolute=*/false, Options);

  // UGameplayStatics::OpenLevel(WorldContextObject, *LongPackageLevelPath, /*bAbsolute=*/false, Options);

   //UE_LOG(LogTemp, Log, TEXT("-----------------OpenLevelCPP=  /Game/SancheoneoPark/SancheoneoPark   = %s"), *Norm);
   //UGameplayStatics::OpenLevel(
   //    WorldContextObject,
   //    TEXT("/Game/SancheoneoPark/SancheoneoPark"),
   //    /*bAbsolute=*/false, Options
   //);


}

void UUtilLibrary::UnMountPak(UObject* WorldContextObject)
{
#if !WITH_EDITOR
    UExternalPakManager* PM = WorldContextObject->GetWorld()->GetGameInstance()->GetSubsystem<UExternalPakManager>();

    if (PM)
    {
        PM->UnmountAllMountedPaks(true, true);
    }
#endif
}

void UUtilLibrary::FadeInOut(UObject* WorldContextObject, float FadeInDuration, float HoldDuration, float FadeOutDuration)
{
    if (UTerraParkgameInstance* GI = Cast<UTerraParkgameInstance>(GetGameModeBP(WorldContextObject)->GetGameInstance()))
    {
        if (GI->FadeWidget)
        {
            if (!GI->FadeWidget->IsInViewport())
            {
                GI->FadeWidget->AddToViewport(99999);
                GI->FadeWidget->SetVisibility(ESlateVisibility::Hidden);
            }
            GI->FadeWidget->FadeInOut(FadeInDuration, HoldDuration, FadeOutDuration);
        }
    }
}

void UUtilLibrary::LockButtonForSeconds(UButton* Button, UObject* WorldContext, float LockSeconds)
{
    if (!Button || !WorldContext || LockSeconds <= 0.f)
    {
        return;
    }

    UWorld* World = WorldContext->GetWorld();
    if (!World)
    {
        return;
    }

    // 즉시 비활성화 -> 이후 입력 "인식 자체"가 안 들어옴
    Button->SetIsEnabled(false);

    FTimerHandle TimerHandle;
    TWeakObjectPtr<UButton> WeakButton(Button);

    World->GetTimerManager().SetTimer(
        TimerHandle,
        [WeakButton]()
        {
            if (WeakButton.IsValid())
            {
                WeakButton->SetIsEnabled(true);
            }
        },
        LockSeconds,
            false
    );
}


void UUtilLibrary::LockCheckBoxForSeconds(UCheckBox* CheckBox, UObject* WorldContext, float LockSeconds)
{
    if (!CheckBox || !WorldContext || LockSeconds <= 0.f)
    {
        return;
    }

    UWorld* World = WorldContext->GetWorld();
    if (!World)
    {
        return;
    }

    // 즉시 비활성화 -> 이후 입력 "인식 자체"가 안 들어옴
    CheckBox->SetIsEnabled(false);

    FTimerHandle TimerHandle;
    TWeakObjectPtr<UCheckBox> WeakCheckBox(CheckBox);

    World->GetTimerManager().SetTimer(
        TimerHandle,
        [WeakCheckBox]()
        {
            if (WeakCheckBox.IsValid())
            {
                WeakCheckBox->SetIsEnabled(true);
            }
        },
        LockSeconds,
            false
            );
}

void UUtilLibrary::FadeIn(UObject* WorldContextObject, float FadeInDuration)
{
    if (UTerraParkgameInstance* GI = Cast<UTerraParkgameInstance>(GetGameModeBP(WorldContextObject)->GetGameInstance()))
    {
        if (GI->FadeWidget)
        {
            if (!GI->FadeWidget->IsInViewport())
            {
                GI->FadeWidget->AddToViewport(99999);
                GI->FadeWidget->SetVisibility(ESlateVisibility::Hidden);
            }
            GI->FadeWidget->FadeIn(FadeInDuration);
        }
    }
}


void UUtilLibrary::FadeIn(UObject* WorldContextObject, float FadeInDuration, FFadeCallback CallBack)
{
    if (UTerraParkgameInstance* GI = Cast<UTerraParkgameInstance>(GetGameModeBP(WorldContextObject)->GetGameInstance()))
    {
        if (GI->FadeWidget)
        {
            if (!GI->FadeWidget->IsInViewport())
            {
                GI->FadeWidget->AddToViewport(99999);
                GI->FadeWidget->SetVisibility(ESlateVisibility::Hidden);
            }
            GI->FadeWidget->FadeInWithCallback(FadeInDuration, CallBack);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("FadeWidget is null"));
        }
    }
}

void UUtilLibrary::FadeOut(UObject* WorldContextObject, float FadeOutDuration)
{
    if (UTerraParkgameInstance* GI = Cast<UTerraParkgameInstance>(GetGameModeBP(WorldContextObject)->GetGameInstance()))
    {
        if (GI->FadeWidget)
        {
            if (!GI->FadeWidget->IsInViewport())
            {
                GI->FadeWidget->AddToViewport(99999);
                GI->FadeWidget->SetVisibility(ESlateVisibility::Hidden);
            }
            GI->FadeWidget->FadeOut(FadeOutDuration);
        }
    }
}

void UUtilLibrary::AddPlayerToInGame(UObject* WorldContextObject, FPlayerInfo PlayerInfo)
{
    AInGameMode* GM = Cast<AInGameMode>(GetGameModeBP(WorldContextObject));

    if (GM)
    {
        GM->PlayerManager->InGameAddPlayer(WorldContextObject, PlayerInfo);
    }
}

void UUtilLibrary::RemovePlayerToInGame(UObject* WorldContextObject, FPlayerInfo PlayerInfo)
{
    AInGameMode* GM = Cast<AInGameMode>(GetGameModeBP(WorldContextObject));

    if (GM)
    {
        GM->PlayerManager->InGameRemovePlayer(WorldContextObject, PlayerInfo);
    }
}

void UUtilLibrary::ResetGameData(UObject* WorldContextObject)
{
    if (AInGameMode* GM = Cast<AInGameMode>(GetGameModeBP(WorldContextObject)))
    {
        GM->GameInfo.Reset();

        UJsonHandler::SaveGameInfoToJson(GM->GameInfo, FPaths::ProjectSavedDir() + TEXT("GameData.json"));
    }
}

//void UUtilLibrary::RemovePlayerToInGame(UObject* WorldContextObject, FPlayerInfo PlayerInfo)
//{
//    AInGameMode* GM = Cast<AInGameMode>(GetGameModeBP(WorldContextObject));
//
//    if (GM)
//    {
//        GM->PlayerManager->InGameAddPlayer();
//    }
//}


