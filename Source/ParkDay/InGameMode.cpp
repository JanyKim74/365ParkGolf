#include "InGameMode.h"
#include "GolfPlayerManager.h"
#include "GolfPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "JsonHandler.h"
#include "Engine/Blueprint.h"
#include "Components/SplineComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/HUD.h"
#include "GolfPlayer.h" // AGolfPlayer 및 FPlayerInfo 사용을 위해 포함
#include "PlayerInfoSlotWidget.h" // PlayerInfoSlotWidget 클래스 사용을 위해 포함
#include "Components/VerticalBox.h" // WBP_InGame의 PlayerSlotsContainer가 VerticalBox라고 가정
#include "GolfBall.h" // AGolfBall 클래스를 사용하기 위해 포함
#include "GameFramework/PlayerController.h" // PlayerController를 얻기 위해 필요
#include "Blueprint/UserWidget.h" // ? UUserWidget을 위해 포함
#include "Widgets\InGameScoreBoardWidget.h" // Required for UInGameScoreBoardWidget
#include "StrokeMenuWidget.h" // ? StrokeMenuWidget.h 포함
#include "SerialPort/AutoTeeController.h"
#include "Widgets/InGameMenuPopup.h"
#include "Widgets/LoadingWidget.h"
#include "Structs/DataTableStruct.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "BallParticleManager.h"
#include "BallSweepTraceSubsystem.h"
#include "BoomLine.h"
#include "CameraManager.h"
#include "ParticleManager.h"
#include "ReadyBillboard.h"
#include "SoundManager.h"
#include "TerraParkGameInstance.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/WrapBox.h"
#include "Utils/LoadTexture2DFromFileAsync.h"
#include "Utils/JsonLoader.h"
#include "Utils/UtilLibrary.h"
#include "Widgets/ResultWidget.h"
#include "Widgets/ShotResultWidget.h"
#include "Widgets/GameEndWidget.h"
#include "Widgets/InGamePlayerSelectWidget.h"
#include "Widgets/ResultVideoWidget.h"
#include "Widgets/HoleTransitionWidget.h"
#include <Runtime/MoviePlayer/Public/MoviePlayer.h>
// Windows PlaySound 매크로 충돌 방지
#ifdef PlaySound
#undef PlaySound
#endif
#ifdef PlaySoundW
#undef PlaySoundW
#endif
#include "MediaSoundComponent.h"
#include "Widgets/DistanceWidget.h"
#include "BallDropMarkerActor.h"
#include "Utils/BallDropMarkerLibrary.h"
#include "ParkDay/Widgets/Menu/PlayerSelectWidget.h"
#include "ParkDay/Widgets/Menu/PlayerSelectProfileWidget.h"
#include "ParkDay/TourActor.h"
#include "ParkDay/Utils/JsonLoader.h"
#include "ParkDay/GolfBall.h"
#include "ParkDay/Widgets/RangeHUDStatWidget.h"
#include "Kismet/KismetMathLibrary.h"
#include "ParkDay/Widgets/RangeHUDStatLineWidget.h"
#include "ParkDay/Widgets/RangeHUDStatWidget.h"
#include "ParkDay/BallNamePlateComponent.h"
#include "GolfPlayerController.h"
#include "ShotCinematicComponent.h"
#include "ParkDay/Widgets/InGameScoreBoardStatWidget.h"
#include "ParkDay/Widgets/BallDistanceWidget.h"
#include "ParkDay/Widgets/CameraModePopupWidget.h"
#include "PuttingGuide.h"
#include "ParkDay/Widgets/TimerEndPopupWidget.h"
#include "ParkDay/Widgets/OffScreenIndicatorWidget.h"
#include "ParkDay/Widgets/RangeHUDWidget.h"
#include "ParkDay/StrokeMenuButtonWidget.h"
#include "ParkDayProfiling.h"

#include "Engine/StaticMeshActor.h"


// 로그 카테고리 정의 (한 번만)
//DEFINE_LOG_CATEGORY_STATIC(LogGameMode, Log, All);


AInGameMode::AInGameMode()
{
    PlayerControllerClass = AGolfPlayerController::StaticClass();
    DefaultPawnClass = nullptr;
    CurrentTurnCountdownTime = 0.0f;
    MaxTurnCountdownTime = 0.0f;

    // ? DataTable 로드 (유지 - 안전함)
    static ConstructorHelpers::FObjectFinder<UDataTable> BlueprintDTClass(
        TEXT("DataTable'/Game/DATA/InGame/DT_BlueprintObjects.DT_BlueprintObjects'")
    );

    if (BlueprintDTClass.Succeeded())
        BlueprintDT = BlueprintDTClass.Object;

    static ConstructorHelpers::FObjectFinder<UDataTable> ResultDT(
        TEXT("DataTable'/Game/DATA/InGame/DT_ResultUI.DT_ResultUI'")
    );

    if (ResultDT.Succeeded())
        ResultWidgetDT = ResultDT.Object;

    static ConstructorHelpers::FObjectFinder<UDataTable> DT_SI(
        TEXT("DataTable'/Game/DATA/InGame/DT_ScoreIcon.DT_ScoreIcon'")
    );

    if (DT_SI.Succeeded())
        DT_ScoreIcon = DT_SI.Object;


    static ConstructorHelpers::FObjectFinder<UDataTable> DT(
        TEXT("DataTable'/Game/DATA/InGame/DT_ResultParticle.DT_ResultParticle'")
    );
    if (DT.Succeeded())
    {
        ResultParticleDatatable = DT.Object;
        UE_LOG(LogTemp, Log, TEXT("/Game/DATA/InGame/DT_ResultParticle.DT_ResultParticle load done"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("DT is null"));
    }

    // 게임모드 설정
    if (LoadGameInfoFromJSON())
    {
        if (GameInfo.GameOptions.GameType == 0) // StrokeMode
            CurrentGameMode = EGolfGameMode::StrokeMode;
        else if (GameInfo.GameOptions.GameType == 1) // TrainningMode
            CurrentGameMode = EGolfGameMode::TrainingMode;
        else if (GameInfo.GameOptions.GameType == 2) // RangeMode
            CurrentGameMode = EGolfGameMode::RangeMode;
        else
            CurrentGameMode = EGolfGameMode::RangeMode;

        // ✅ 연습 모드 적용 (0: Driving, 1: Approach, 2: Putting)
        switch (GameInfo.GameOptions.PracticeMode)
        {
        case 1:  CurrentPracticeMode = EPracticeMode::Approach; break;
        case 2:  CurrentPracticeMode = EPracticeMode::Putting;  break;
        default: CurrentPracticeMode = EPracticeMode::Driving;  break;
        }

    }

    // 공용으로 사용할것들
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    PlayerManager = CreateDefaultSubobject<UGolfPlayerManager>(TEXT("PlayerManager"));
    MSC = CreateDefaultSubobject<UMediaSoundComponent>(TEXT("MediaSoundComponent"));
    MSC->SetupAttachment(RootComponent);
    MSC->bIsUISound = true;
    MSC->bAllowSpatialization = false;
    MSC->SetTickableWhenPaused(true);
    // 초기 상태 설정
    StateMachine.CurrentState = EGameState::Game_None;
    StateMachine.PreviousState = EGameState::Game_None;


    //Project Setting
    UPhysicsSettings::Get()->MaxPhysicsDeltaTime = 0.0333f; // 30 fps 기준 substep 반대로 적용
    UPhysicsSettings::Get()->bSubstepping = true;            // true면 maxPhysicDeltaTime 미적용 
    UPhysicsSettings::Get()->MaxSubstepDeltaTime = 0.0333f; //60 fps 기준설정(0.016667f) 
    UPhysicsSettings::Get()->MaxSubsteps = 3;
    UPhysicsSettings::Get()->MinContactOffset = 0.01f;      // contact 최소
    UPhysicsSettings::Get()->MaxContactOffset = 0.05f;      // contact 최대
    UPhysicsSettings::Get()->BounceThresholdVelocity = 100.0f;
    UPhysicsSettings::Get()->FrictionCombineMode = EFrictionCombineMode::Multiply;
    UPhysicsSettings::Get()->RestitutionCombineMode = EFrictionCombineMode::Multiply;

    // ? 이것들은 유지 (순환 참조 없음)
    static ConstructorHelpers::FClassFinder<UGolfShotControlWidget> ShotControlBPClass(
        TEXT("/Game/GolfGameBluePrint/WBP_ShotControl")
    );

    if (ShotControlBPClass.Succeeded())
    {
        DefaultShotControlWidget = ShotControlBPClass.Class;
        UE_LOG(LogTemp, Log, TEXT("? DefaultShotControlWidget loaded via ConstructorHelpers"));
    }
    else
    {
        DefaultShotControlWidget = GetWidgetClassByCacheBypass();
    }

    static ConstructorHelpers::FClassFinder<UTimerWidget> TimerWidgetBPClass(
        TEXT("/Game/UMG/UI/InGame/WBP_Timer.WBP_Timer_C")
    );

    if (TimerWidgetBPClass.Succeeded())
    {
        TimerWidgetClass = TimerWidgetBPClass.Class;
        UE_LOG(LogTemp, Log, TEXT("? TimerWidgetBPClass loaded via ConstructorHelpers"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("?? Failed to load TimerWidgetBPClass. Check path."));
    }


    static ConstructorHelpers::FClassFinder<UChanceWidget> ChanceWidgetBPClass(
        TEXT("/Game/365_widget/chance/chance.chance_C")
    );

    if (ChanceWidgetBPClass.Succeeded())
    {
        ChanceWidgetClass = ChanceWidgetBPClass.Class;
        UE_LOG(LogTemp, Log, TEXT("✅ ChanceWidgetClass loaded via ConstructorHelpers"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Failed to load ChanceWidgetClass. Check path."));
    }

    static ConstructorHelpers::FClassFinder<AActor> TeeAnim1(TEXT("Blueprint'/Game/Tee_ani/motion01/T_ani1.T_ani1_C'"));
    static ConstructorHelpers::FClassFinder<AActor> TeeAnim2(TEXT("Blueprint'/Game/Tee_ani/motion02/T_ani2.T_ani2_C'"));
    static ConstructorHelpers::FClassFinder<AActor> TeeAnim3(TEXT("Blueprint'/Game/Tee_ani/motion03/T_ani3.T_ani3_C'"));
    static ConstructorHelpers::FClassFinder<AActor> TeeAnim4(TEXT("Blueprint'/Game/Tee_ani/motion04/T_ani4.T_ani4_C'"));

    if (TeeAnim1.Succeeded()) TeeAnimArray.Add(TeeAnim1.Class);
    if (TeeAnim2.Succeeded()) TeeAnimArray.Add(TeeAnim2.Class);
    if (TeeAnim3.Succeeded()) TeeAnimArray.Add(TeeAnim3.Class);
    if (TeeAnim4.Succeeded()) TeeAnimArray.Add(TeeAnim4.Class);
    AutoTeeController = CreateDefaultSubobject<UAutoTeeController>(TEXT("AutoTeeController"));

    if (CurrentGameMode == EGolfGameMode::StrokeMode || CurrentGameMode == EGolfGameMode::TrainingMode)
    {
        static ConstructorHelpers::FClassFinder<ASubChangeCourse> SubChangeCourseBPClass(
            TEXT("/Game/GolfGameBluePrint/BP_SubChangeCourse")
        );
        if (SubChangeCourseBPClass.Succeeded())
        {
            SubChangeCourseClass = SubChangeCourseBPClass.Class;
            UE_LOG(LogTemp, Log, TEXT("? SubChangeCourseClass loaded via ConstructorHelpers"));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("?? Failed to load SubChangeCourseClass. Check path."));
        }

        static ConstructorHelpers::FClassFinder<AHUD> HUDClassFinder(TEXT("/Game/UMG/HUD_InGame"));
        if (HUDClassFinder.Succeeded())
        {
            HUDClass = HUDClassFinder.Class;
        }

        StrokeWidgetClass = FSoftClassPath(TEXT("/Game/UMG/UI/InGame/WBP_InGame.WBP_InGame_C"));


        // ============================================================================
        // ? 모든 Widget ConstructorHelpers 제거됨!
        // 
        // 삭제된 코드들:
        // - ScoreBoardWidgetBPClass (라인 233-244)
        // - MiniMapBPClass (라인 248-261)
        // - PlayerInfoSlotBPClass (라인 266-275)
        // - StrokeMenuWidgetBPClass (라인 279-290)
        // - InGamePopupWidgetBPClass (라인 293-304)
        // - ResultWidgetBPClass (라인 306-317)
        // - ShotResultWidgetBPClass (라인 319-330)
        // - GameEndWidgetBPClass (라인 332-343)
        // - InGamePlayerSelectBPClass (라인 345-355)
        // - HoleMarkBPClass (라인 358-369)
        // 
        // ? 대신 모든 Widget 클래스는 BeginPlay()의 LoadWidgetClasses()에서 로드됨!
        // ============================================================================

        UE_LOG(LogGameMode, Log, TEXT("? InGameMode Constructor: Widget classes will be loaded in BeginPlay()"));
    }

    UE_LOG(LogGameMode, Log, TEXT("? InGameMode Constructor completed"));
}


void AInGameMode::LoadResultVideoWidgetClassSafe()
{
    UE_LOG(LogTemp, Log, TEXT("?? LoadResultVideoWidgetClassSafe START"));

    if (!IsInGameThread())
    {
        UE_LOG(LogTemp, Warning, TEXT("?? Not in game thread"));
        return;
    }

    if (ResultVideoWidgetClass)
    {
        UE_LOG(LogTemp, Log, TEXT("? ResultVideoWidgetClass already loaded"));
        return;
    }

    // ? 경로 1: 주 경로
    ResultVideoWidgetClass = LoadClass<UResultVideoWidget>(
        nullptr,
        TEXT("/Game/UMG/UI/InGame/Result/WBP_ResultVideo.WBP_ResultVideo_C")
    );

    if (ResultVideoWidgetClass)
    {
        UE_LOG(LogTemp, Log, TEXT("? ResultVideoWidgetClass loaded: %s"),
            *ResultVideoWidgetClass->GetName());
        return;
    }

    // ? 경로 2: 대안 경로 1
    ResultVideoWidgetClass = LoadClass<UResultVideoWidget>(
        nullptr,
        TEXT("/Game/Widgets/Result/WBP_ResultVideo.WBP_ResultVideo_C")
    );

    if (ResultVideoWidgetClass)
    {
        UE_LOG(LogTemp, Log, TEXT("? ResultVideoWidgetClass loaded (alt): %s"),
            *ResultVideoWidgetClass->GetName());
        return;
    }

    // ? 경로 3: 대안 경로 2
    ResultVideoWidgetClass = LoadClass<UResultVideoWidget>(
        nullptr,
        TEXT("/Game/UMG/Result/WBP_ResultVideo.WBP_ResultVideo_C")
    );

    if (ResultVideoWidgetClass)
    {
        UE_LOG(LogTemp, Log, TEXT("? ResultVideoWidgetClass loaded (alt 2): %s"),
            *ResultVideoWidgetClass->GetName());
        return;
    }

    UE_LOG(LogTemp, Error, TEXT("? ResultVideoWidgetClass: All paths failed!"));
}


void AInGameMode::LoadMulliganTextures()
{
    UE_LOG(LogTemp, Log, TEXT("?? LoadMulliganTextures START"));

    if (!IsInGameThread())
    {
        UE_LOG(LogTemp, Warning, TEXT("?? LoadMulliganTextures: Not in game thread"));
        return;
    }

    MulliganTextureMap.Add(0, LoadObject<UTexture2D>(nullptr, TEXT("/Game/UMG/Resources/Images/InGame/TIME_0.TIME_0")));
    MulliganTextureMap.Add(1, LoadObject<UTexture2D>(nullptr, TEXT("/Game/UMG/Resources/Images/InGame/TIME_1.TIME_1")));
    MulliganTextureMap.Add(2, LoadObject<UTexture2D>(nullptr, TEXT("/Game/UMG/Resources/Images/InGame/TIME_2.TIME_2")));
    MulliganTextureMap.Add(3, LoadObject<UTexture2D>(nullptr, TEXT("/Game/UMG/Resources/Images/InGame/TIME_3.TIME_3")));
    MulliganTextureMap.Add(4, LoadObject<UTexture2D>(nullptr, TEXT("/Game/UMG/Resources/Images/InGame/TIME_4.TIME_4")));
    MulliganTextureMap.Add(5, LoadObject<UTexture2D>(nullptr, TEXT("/Game/UMG/Resources/Images/InGame/TIME_5.TIME_5")));
    MulliganTextureMap.Add(6, LoadObject<UTexture2D>(nullptr, TEXT("/Game/UMG/Resources/Images/InGame/TIME_6.TIME_6")));
    MulliganTextureMap.Add(-1, LoadObject<UTexture2D>(nullptr, TEXT("/Game/UMG/Resources/Images/InGame/TIME_inf.TIME_inf")));

    UE_LOG(LogTemp, Log, TEXT("? LoadMulliganTextures COMPLETE - %d textures loaded"), MulliganTextureMap.Num());
}


void AInGameMode::LoadStrokeWidgetClassSafe()
{
    // ? 게임 스레드 확인
    if (!IsInGameThread())
    {
        UE_LOG(LogTemp, Warning, TEXT("?? LoadStrokeWidgetClassSafe called from non-game thread"));
        return;
    }

    // ? 이미 로드된 경우 스킵
    if (StrokeWidgetClass)
    {
        UE_LOG(LogTemp, Verbose, TEXT("? StrokeWidgetClass already loaded"));
        return;
    }

    // ? 동기식 로드 (BeginPlay에서 게임 스레드 보장)
    StrokeWidgetClass = LoadClass<UStrokeWidget>(
        nullptr,
        TEXT("/Game/UMG/UI/InGame/WBP_InGame.WBP_InGame_C")
    );

    if (StrokeWidgetClass)
    {
        UE_LOG(LogTemp, Log, TEXT("? StrokeWidgetClass loaded successfully: %s"),
            *StrokeWidgetClass->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("? Failed to load StrokeWidgetClass from /Game/UMG/UI/InGame/WBP_InGame"));
        UE_LOG(LogTemp, Warning, TEXT("?? Check if WBP_InGame exists at: /Game/UMG/UI/InGame/"));
    }
}



AActor* AInGameMode::InitTeeAnim()
{
    if (TeeAnimInstance)
    {
        TeeAnimInstance->Destroy();
        TeeAnimInstance = nullptr;
    }

    int32 RandomNum = FMath::RandRange(0, TeeAnimArray.Num() - 1);
    int32 CurrentHoleNum = CurrentHole - 1;

    UE_LOG(LogTemp, Log, TEXT("InitTeeAnim() : CurrentHoleNum=%d"), CurrentHoleNum);

    if (GameInfo.SelectedMap.TeePositions.IsValidIndex(CurrentHoleNum))
    {
        if (!TeeRotationArray.IsValidIndex(CurrentHoleNum))
            return nullptr;

        FVector Location = GameInfo.SelectedMap.TeePositions[CurrentHoleNum];

        AInGameMode* GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
        int32 RangemodAddZ = 0.f;
		float fRotYaw = 90.0f;

        if (GM)
        {
            FRotator TeeRotation = TeeRotationArray[CurrentHole - 1];

            if (GM->IsRangeMode())
            {
                RangemodAddZ = 2.f;
				fRotYaw = 180.0f;
            }
                

            AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(TeeAnimArray[RandomNum], Location + FVector(0.f, 0.f, RangemodAddZ), FRotator(
                TeeRotation.Pitch,
                TeeRotation.Yaw + fRotYaw,
                TeeRotation.Roll));
            TeeAnimInstance = SpawnedActor;

            return TeeAnimInstance;
        }
    }
    return nullptr;
}


// 3) 시작 시 인덱스 구축(한 번만)
void AInGameMode::ResultParticleBuildIndex()
{
    HoleInParticleMap.Reset();
    if (!ResultParticleDatatable) return;

    static const FString Ctx = TEXT("BuildIndex");
    TArray<FResultParticle*> Rows;
    ResultParticleDatatable->GetAllRows(Ctx, Rows);

    for (const FResultParticle* Row : Rows)
    {
        if (!Row) continue;
        HoleInParticleMap.Add(Row->Score, Row->BP_Result);
    }
}

void AInGameMode::LoadMiniMapWidgetClassFallback()
{
    UE_LOG(LogTemp, Warning, TEXT("?? Trying fallback methods for MiniMapWidgetClass..."));

    if (!MiniMapWidgetClass)
        MiniMapWidgetClass = LoadMiniMapWidgetViaLoadClass();

    if (!MiniMapWidgetClass)
        MiniMapWidgetClass = LoadMiniMapWidgetViaBlueprint();

    if (!MiniMapWidgetClass)
        MiniMapWidgetClass = FindMiniMapWidgetViaAssetRegistry();

    if (!MiniMapWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("?? Using default C++ class as fallback"));
        MiniMapWidgetClass = UGolfMiniMap::StaticClass();
    }
}

UClass* AInGameMode::LoadMiniMapWidgetViaLoadClass()
{
    UE_LOG(LogTemp, Log, TEXT("?? Method 2: LoadClass"));

    TArray<FString> PossiblePaths = {
        TEXT("/Game/GolfGameBluePrint/WBP_GolfMiniMap.WBP_GolfMiniMap_C"),
        TEXT("/Game/GolfGameBluePrint/WBP_MiniMap.WBP_MiniMap_C"),
        TEXT("/Game/GolfGameBluePrint/WBP_GolfMiniMap.WBP_GolfMiniMap_C")
    };

    for (const FString& Path : PossiblePaths)
    {
        UClass* LoadedClass = LoadClass<UGolfMiniMap>(nullptr, *Path);
        if (LoadedClass)
        {
            UE_LOG(LogTemp, Log, TEXT("? MiniMap loaded via LoadClass: %s"), *Path);
            return LoadedClass;
        }
        else
        {
            UE_LOG(LogTemp, VeryVerbose, TEXT("? Failed to load: %s"), *Path);
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("?? LoadClass method failed"));
    return nullptr;
}

UClass* AInGameMode::LoadMiniMapWidgetViaBlueprint()
{
    UE_LOG(LogTemp, Log, TEXT("?? Method 3: Blueprint Asset"));

    TArray<FString> BlueprintNames = {
        TEXT("WBP_GolfMiniMap"),
        TEXT("WBP_MiniMap"),
        TEXT("BP_MiniMap"),
        TEXT("MiniMapWidget")
    };

    for (const FString& BPName : BlueprintNames)
    {
        UBlueprint* FoundBP = FindObject<UBlueprint>(nullptr, *BPName);
        if (FoundBP && FoundBP->GeneratedClass)
        {
            if (FoundBP->GeneratedClass->IsChildOf(UGolfMiniMap::StaticClass()))
            {
                UE_LOG(LogTemp, Log, TEXT("? MiniMap found via Blueprint: %s"), *BPName);
                return FoundBP->GeneratedClass;
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("?? Wrong parent class for %s: %s"),
                    *BPName,
                    FoundBP->ParentClass ? *FoundBP->ParentClass->GetName() : TEXT("NULL"));
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("?? Blueprint method failed"));
    return nullptr;
}

UClass* AInGameMode::FindMiniMapWidgetViaAssetRegistry()
{
    UE_LOG(LogTemp, Log, TEXT("?? Method 4: Asset Registry"));

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    TArray<FAssetData> BlueprintAssets;
    AssetRegistry.GetAssetsByClass(UStaticMesh::StaticClass()->GetClassPathName(), BlueprintAssets);

    for (const FAssetData& Asset : BlueprintAssets)
    {
        FString AssetName = Asset.AssetName.ToString();
        if (AssetName.Contains(TEXT("MiniMap")) ||
            AssetName.Contains(TEXT("Mini")) ||
            AssetName.Contains(TEXT("Map")))
        {
            UE_LOG(LogTemp, VeryVerbose, TEXT("?? Checking: %s"), *AssetName);
            if (UBlueprint* BP = Cast<UBlueprint>(Asset.GetAsset()))
            {
                if (BP->GeneratedClass && BP->GeneratedClass->IsChildOf(UGolfMiniMap::StaticClass()))
                {
                    UE_LOG(LogTemp, Log, TEXT("? MiniMap found via AssetRegistry: %s"), *AssetName);
                    return BP->GeneratedClass;
                }
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("?? Asset Registry method failed"));
    return nullptr;
}

UClass* AInGameMode::GetWidgetClassByCacheBypass()
{
    UE_LOG(LogTemp, Warning, TEXT("=== Cache Bypass Widget Search ==="));

    UBlueprint* WidgetBP = FindObject<UBlueprint>(nullptr, TEXT("WBP_ShotControl"));
    if (WidgetBP)
    {
        UE_LOG(LogTemp, Warning, TEXT("? Blueprint Asset found: %s"), *WidgetBP->GetName());
        if (WidgetBP->GeneratedClass)
        {
            UE_LOG(LogTemp, Warning, TEXT("? Generated Class found: %s"), *WidgetBP->GeneratedClass->GetName());
            if (WidgetBP->GeneratedClass->IsChildOf(UGolfShotControlWidget::StaticClass()))
            {
                UE_LOG(LogTemp, Warning, TEXT("?? Perfect! Using Blueprint->GeneratedClass"));
                return WidgetBP->GeneratedClass;
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("?? Wrong parent class: %s"),
                    WidgetBP->ParentClass ? *WidgetBP->ParentClass->GetName() : TEXT("NULL"));
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("?? Blueprint has no Generated Class - compilation failed"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("?? Blueprint Asset 'WBP_ShotControl' not found"));
    }

    TArray<FString> PossibleBPNames = {
        TEXT("WBP_ShotControl"),
        TEXT("ShotControlWidget"),
        TEXT("BP_ShotControl"),
        TEXT("WBP_Golf")
    };

    for (const FString& BPName : PossibleBPNames)
    {
        if (UBlueprint* BP = FindObject<UBlueprint>(nullptr, *BPName))
        {
            if (BP->GeneratedClass && BP->GeneratedClass->IsChildOf(UGolfShotControlWidget::StaticClass()))
            {
                UE_LOG(LogTemp, Warning, TEXT("? Found alternative: %s → %s"),
                    *BPName, *BP->GeneratedClass->GetName());
                return BP->GeneratedClass;
            }
        }
    }

    return SearchAllBlueprints();
}

UClass* AInGameMode::SearchAllBlueprints()
{
    UE_LOG(LogTemp, Warning, TEXT("Searching all blueprints..."));

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    TArray<FAssetData> BlueprintAssets;
    AssetRegistry.GetAssetsByClass(UStaticMesh::StaticClass()->GetClassPathName(), BlueprintAssets);

    for (const FAssetData& Asset : BlueprintAssets)
    {
        FString AssetName = Asset.AssetName.ToString();
        if (AssetName.Contains(TEXT("Shot")) ||
            AssetName.Contains(TEXT("Control")) ||
            AssetName.Contains(TEXT("Golf")))
        {
            UE_LOG(LogTemp, Log, TEXT("Checking blueprint: %s"), *AssetName);
            UBlueprint* BP = Cast<UBlueprint>(Asset.GetAsset());
            if (BP && BP->GeneratedClass)
            {
                if (BP->GeneratedClass->IsChildOf(UGolfShotControlWidget::StaticClass()))
                {
                    UE_LOG(LogTemp, Warning, TEXT("?? Found via asset registry: %s → %s"),
                        *AssetName, *BP->GeneratedClass->GetName());
                    return BP->GeneratedClass;
                }
            }
        }
    }
    UE_LOG(LogTemp, Error, TEXT("?? No suitable widget found"));
    return nullptr;
}

void AInGameMode::InitBlueprintObjects()
{
    UUtilLibrary::DataTableToMap<FBlueprintObject, FString, TSubclassOf<UObject>>(BlueprintDT, BlueprintObjectsMap,
        [](const FBlueprintObject& Row) { return Row.BPName; },   // Key 생성
        [](const FBlueprintObject& Row) { return Row.Blueprint; }     // Value 생성
    );

    if (BlueprintObjectsMap.Num())
        UE_LOG(LogTemp, Log, TEXT(" AInGameMode::InitBlueprintObjects() : Init BP Objects"));
}

void AInGameMode::InitConcedeLines()
{

    int32 DistanceOption = GameInfo.GameOptions.Concede_Distance;
    float DistanceOfConcede = DistanceOption <= 0 ? 0 : 100 + (DistanceOption - 1.0f) * 50.f;

    for (AGolfBall* Ball : PlayerManager->GetPlayerBalls())
    {
        Ball->ConcedeDistance = DistanceOfConcede;
    }

    FString ConcedeLineBPText = FString::Printf(TEXT("Concede%d"), DistanceOption);

    for (FVector HoleCupPosition : MapInfo.HolecupPositions)
    {
        if (DistanceOption > 0)
            GetWorld()->SpawnActor<AActor>(BlueprintObjectsMap.Find(ConcedeLineBPText)->Get(), HoleCupPosition, FRotator::ZeroRotator);
    }
}

bool AInGameMode::CheckTeeShotCountIsZero()
{
    for (AGolfPlayer* Player : PlayerManager->GetPlayers())
    {
        if (Player->GetCurrentHoleShotCount() > 0)
        {
            return false;
        }
    }

    return true;
}

bool AInGameMode::CheckFirstShot()
{
    if (!PlayerManager)
    {
        return false;
    }

    //AGolfBall* LatestShotBall = PlayerManager->GetPlayerBalls()[LatestShotSlotIndex];
    const TArray<AGolfBall*>& Balls = PlayerManager->GetPlayerBalls();
    if (!Balls.IsValidIndex(CurrentPlayerIndex) || !IsValid(Balls[CurrentPlayerIndex]))
    {
        UE_LOG(LogGameMode, Warning, TEXT("CheckFirstShot: invalid CurrentPlayerIndex=%d (Balls=%d)"),
            CurrentPlayerIndex, Balls.Num());
        return false;
    }

    AGolfBall* LatestShotBall = Balls[CurrentPlayerIndex];

    if (IsStrokeMode())
    {
        AGolfPlayer* CheckPlayer = FindPlayer(CurrentPlayerIndex);
        if (!IsValid(CheckPlayer))
        {
            UE_LOG(LogGameMode, Warning, TEXT("CheckFirstShot: invalid player for CurrentPlayerIndex=%d"), CurrentPlayerIndex);
            return false;
        }

        const FPlayerInfo PlayerInfo = CheckPlayer->GetPlayerInfo();
        const int32 HoleIdx = CurrentHole - 1;
        const int32 ShotCount = PlayerInfo.ShotCountPerHole.IsValidIndex(HoleIdx) ? PlayerInfo.ShotCountPerHole[HoleIdx] : 0;
        UE_LOG(LogTemp, Log, TEXT(" --CheckFirstShot - ShotCountPerHole shotcount = %d"), ShotCount);
        if (ShotCount == 0)
        {
            return true;
        }
    }

    if (LatestShotBall->CheckTeeShot())
    {
        return true;
    }

    return false;
}

FVector AInGameMode::GetCurrentTeeLocation()
{
    if (MapInfo.TeePositions.IsValidIndex(CurrentHole - 1))
        return MapInfo.TeePositions[CurrentHole - 1];

    return FVector::ZeroVector;
}

void AInGameMode::InitCourseMapImage()
{
    UE_LOG(LogTemp, Log, TEXT("??? InitCourseMapImage START"));

    if (CurrentGameMode == EGolfGameMode::RangeMode)
    {
        UE_LOG(LogTemp, Log, TEXT("?? Skipping InitCourseMapImage for RangeMode"));
        return;
    }

    // ? 1단계: StrokeWidgetInstance 검사
    if (!IsValid(StrokeWidgetInstance))
    {
        UE_LOG(LogTemp, Error, TEXT("? StrokeWidgetInstance is NOT VALID!"));
        return;
    }
    UE_LOG(LogTemp, Log, TEXT("? StrokeWidgetInstance is valid"));

    // ? 2단계: Image_CourseMap 검사
    if (!IsValid(StrokeWidgetInstance->Image_CourseMap))
    {
        UE_LOG(LogTemp, Error, TEXT("? Image_CourseMap is NOT VALID!"));
        return;
    }
    UE_LOG(LogTemp, Log, TEXT("? Image_CourseMap is valid"));

    // ? 3단계: CCName 검사
    if (GameInfo.SelectedMap.CCName.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("? CCName is empty!"));
        return;
    }
    UE_LOG(LogTemp, Log, TEXT("?? CCName: %s"), *GameInfo.SelectedMap.CCName);

    // ? 4단계: 경로 생성
    FString ImagePath = FPaths::ProjectContentDir() / TEXT("DATA/CourseMap/") +
        GameInfo.SelectedMap.CCName + TEXT("/image1.png");

    UE_LOG(LogTemp, Log, TEXT("?? Trying to load image from: %s"), *ImagePath);

    // ? 5단계: 파일 존재 확인
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.FileExists(*ImagePath))
    {
        UE_LOG(LogTemp, Warning, TEXT("?? Image file does NOT exist: %s"), *ImagePath);
        return;
    }
    UE_LOG(LogTemp, Log, TEXT("? Image file exists"));

    // ? 6단계: 텍스처 로드
    FString Err;
    UTexture2D* Tex = ULoadTexture2DFromFileAsync::LoadTexture2DFromFileSync(ImagePath, &Err);

    if (!Tex)
    {
        UE_LOG(LogTemp, Error, TEXT("? Failed to load texture: %s"), *Err);
        return;
    }
    UE_LOG(LogTemp, Log, TEXT("? Texture loaded successfully"));

    // ? 7단계: 최종 검사 후 브러시 설정
    if (!IsValid(StrokeWidgetInstance->Image_CourseMap))
    {
        UE_LOG(LogTemp, Error, TEXT("? Image_CourseMap became invalid during load!"));
        return;
    }

    StrokeWidgetInstance->Image_CourseMap->SetBrushFromTexture(Tex);
    UE_LOG(LogTemp, Log, TEXT("? Loaded external CourseMap Image: %s"), *ImagePath);
}
void AInGameMode::OnAnyBallSweepHit(AActor* TrackedActor, const FHitResult& Hit)
{
    //UE_LOG(LogTemp, Log, TEXT("Sweep %s, Gravity : %f"), *Hit.PhysMaterial->GetName(), GetWorld()->GetWorldSettings()->GlobalGravityZ);
    if (Hit.PhysMaterial->GetName() == "Leaves")
    {
        if (auto* SM = GetGameInstance()->GetSubsystem<USoundManager>())
        {
            SM->PlayAtLocation_ById("Effect.Ball.Material.Tree", Hit.ImpactPoint, 3.f);
            BallParticleManager->SpawnParticle(GetWorld(), "Bounce_Leaves", Hit.ImpactPoint);
            //AGolfBall* Ball = Cast<AGolfBall>(TrackedActor);
            //if (Ball)
            //{
            //    Ball->BallMesh->SetPhysicsLinearVelocity(Ball->BallMesh->GetPhysicsLinearVelocity() * 0.4f);
            //    Ball->BallMesh->SetPhysicsAngularVelocityInDegrees(Ball->BallMesh->GetPhysicsAngularVelocityInDegrees() * 0.4f);
            //}
        }
    }
    else if (Hit.PhysMaterial->GetName() == "Water")
    {
        if (auto* SM = GetGameInstance()->GetSubsystem<USoundManager>())
        {
            SM->PlayAtLocation_ById("Effect.Ball.Material.Water", Hit.ImpactPoint, 2.f);
            BallParticleManager->SpawnParticle(GetWorld(), "Bounce_Water", Hit.ImpactPoint);
            AGolfBall* Ball = Cast<AGolfBall>(TrackedActor);
            if (Ball)
            {
                Ball->BallMesh->SetPhysicsLinearVelocity(Ball->BallMesh->GetPhysicsLinearVelocity() * 0.1f);
                Ball->BallMesh->SetPhysicsAngularVelocityInDegrees(Ball->BallMesh->GetPhysicsAngularVelocityInDegrees() * 0.1f);
            }
        }
    }

    //else
    //{
    //    GetWorld()->GetWorldSettings()->GlobalGravityZ = DefaultGravity;
    //}
}

void AInGameMode::StopLoading()
{
    if (UTerraParkgameInstance* GI = Cast<UTerraParkgameInstance>(GetWorld()->GetGameInstance()))
    {
        GI->StopLoadingScreen();
    }
}

void AInGameMode::StartLoading()
{
    UTerraParkgameInstance* GI = Cast<UTerraParkgameInstance>(GetGameInstance());
    if (GI)
    {
        if (!GI->ActiveLoadingWidget.IsValid())
        {
            GI->ActiveLoadingWidget = CreateWidget<ULoadingWidget>(GetWorld(), GI->LoadingScreenWidgetClass);
        }
        if (!GI->ActiveLoadingWidget.Get()->IsInViewport())
            GI->ActiveLoadingWidget.Get()->AddToViewport(10000);
        GI->ActiveLoadingWidget.Get()->SetVisibility(ESlateVisibility::Visible);
        GI->ActiveLoadingWidget.Get()->PlayBGM();
    }
}

void AInGameMode::BeginPlay()
{
    Super::BeginPlay();

    StopLoading();
    StartLoading();

    RangeHUDWidgetPath = TEXT("/Game/UMG/UI/InGame/WBP_RangeHUD.WBP_RangeHUD_C");
    LoadWidgetClasses();
    LoadMulliganTextures();
    LoadStrokeWidgetClassSafe();
    LoadResultVideoWidgetClassSafe();  // ? 추가
    InitBlueprintObjects();
    DetermineGameMode();
    ResultParticleBuildIndex();

    GetWorld()->GetWorldSettings()->bGlobalGravitySet = true;
    GetWorld()->GetWorldSettings()->GlobalGravityZ = DefaultGravity;

    if (UBallSweepTraceSubsystem* Sys = GetWorld()->GetSubsystem<UBallSweepTraceSubsystem>())
    {
        Sys->OnSweepHitGlobal.AddDynamic(this, &ThisClass::OnAnyBallSweepHit);
    }

    // ? 화면 디버그 메시지 끄기
    //if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    //{
    //    PC->ConsoleCommand(TEXT("ShowDebug None"));
    //}

    //if (LoadingScreenWidgetClass)
    //{
    //    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    //    if (PC)
    //    {
    //        LoadingScreenWidgetInstance = CreateWidget<ULoadingWidget>(PC, LoadingScreenWidgetClass);
    //        if (LoadingScreenWidgetInstance)
    //        {
    //            if (auto* SM = GetGameInstance()->GetSubsystem<USoundManager>())
    //            {
    //                SM->PlayBGM_ById("BGM.Loading", 0.5);
    //                SM->PlayTTS_Interrupt_ById("Voice.Loading");
    //            }
    //            LoadingScreenWidgetInstance->SetRandomImageIndex();
    //            LoadingScreenWidgetInstance->AddToViewport(10000);
    //            LoadingScreenWidgetInstance->SetVisibility(ESlateVisibility::Visible);
    //            UE_LOG(LogTemp, Log, TEXT("? 로딩 화면 위젯 초기화 완료"));
    //        }
    //    }
    //}

    if (!StrokeMenuWidgetClass)
    {
        StrokeMenuWidgetClass = LoadClass<UStrokeMenuWidget>(
            nullptr,
            TEXT("/Game/UMG/UI/InGame/Popup/Menu/WBP_InGame_StrokeMenu.WBP_InGame_StrokeMenu_C")
        );

        if (StrokeMenuWidgetClass)
        {
            UE_LOG(LogTemp, Log, TEXT("? StrokeMenuWidgetClass loaded dynamically in BeginPlay"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("? Failed to load StrokeMenuWidgetClass dynamically"));
        }
    }

    if (InGamePopupWidgetClass)
    {
        APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        if (PC)
        {
            InGamePopupWidgetInstance = CreateWidget<UInGameMenuPopup>(PC, InGamePopupWidgetClass);
            if (InGamePopupWidgetInstance)
            {

                InGamePopupWidgetInstance->AddToViewport(9000);
                InGamePopupWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
                UE_LOG(LogTemp, Log, TEXT("? 인게임 팝업 위젯 로딩 완료"));
            }


        }
    }

    if (InGamePlayerSelectWidgetClass)
    {
        APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        if (PC)
        {
            InGamePlayerSelectWidget = CreateWidget<UInGamePlayerSelectWidget>(PC, InGamePlayerSelectWidgetClass);
            if (InGamePlayerSelectWidget)
            {

                InGamePlayerSelectWidget->AddToViewport(6000);
                InGamePlayerSelectWidget->SetVisibility(ESlateVisibility::Collapsed);
                UE_LOG(LogTemp, Log, TEXT("? 인게임 플레이어 선택 팝업 위젯 로딩 완료"));
            }
        }
    }


    if (BallDistanceWidgetClass)
    {
        APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        if (PC)
        {
            BallDistanceWidget = CreateWidget<UBallDistanceWidget>(PC, BallDistanceWidgetClass);
            if (BallDistanceWidget)
            {

                BallDistanceWidget->AddToViewport(900);
                BallDistanceWidget->SetRenderOpacity(0.f);
                UE_LOG(LogTemp, Log, TEXT("? 볼 거리 위젯 로딩 완료"));
            }
        }
    }


    if (ResultWidgetClass)
    {
        APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        if (PC)
        {
            ResultWidgetInstance = CreateWidget<UResultWidget>(PC, ResultWidgetClass);
            if (ResultWidgetInstance)
            {
                // ★ ZOrder를 StrokeWidget보다 높되 합리적인 값으로 조정
                // StrokeWidget ZOrder가 얼마인지 확인 후 그보다 낮게 설정하거나
                // SelfHitTestInvisible로 입력 차단 방지
                ResultWidgetInstance->AddToViewport(200);  // ★ 5000 → 200으로 낮춤

                // ★ 루트를 Collapsed로 시작
                ResultWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);

                UE_LOG(LogTemp, Log, TEXT("결과 위젯 초기화 완료"));
            }
        }
    }

    if (ShotResultWidgetClass)
    {
        APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        if (PC)
        {
            ShotResultWidgetInstance = CreateWidget<UShotResultWidget>(PC, ShotResultWidgetClass);
            if (ShotResultWidgetInstance)
            {
                ShotResultWidgetInstance->AddToViewport(5000);
                ShotResultWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
                UE_LOG(LogTemp, Log, TEXT("? 샷 결과 위젯 초기화 완료"));
            }
        }
    }

    if (GameEndWidgetClass)
    {
        APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        if (PC)
        {
            GameEndWidgetInstance = CreateWidget<UGameEndWidget>(PC, GameEndWidgetClass);
            if (GameEndWidgetInstance)
            {
                GameEndWidgetInstance->AddToViewport(2900);
                GameEndWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
                UE_LOG(LogTemp, Log, TEXT("? 최종 결과 위젯 초기화 완료"));
            }
        }
    }


    // 새롭게 추가: ASubChangeCourse 인스턴스 생성 및 초기화
    if (SubChangeCourseClass)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this; // GameMode를 Owner로 설정 (선택 사항)
        SpawnParams.Instigator = GetInstigator(); // 필요에 따라 Instigator 설정 (선택 사항)

        // 액터를 스폰할 위치와 회전 (기본값인 FVector::ZeroVector와 FRotator::ZeroRotator 사용)
        SubChangeCourseInstance = GetWorld()->SpawnActor<ASubChangeCourse>(SubChangeCourseClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

        if (IsValid(SubChangeCourseInstance))
        {
            UE_LOG(LogTemp, Log, TEXT("? ASubChangeCourse instance created successfully."));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("? Failed to create ASubChangeCourse instance."));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("?? SubChangeCourseClass is not set, cannot create instance."));
    }
    // 빠른 랜드타입체크
    SetupLandscapeChecker();

    InitializeStateTransitions();

    ChangeGameState(EGameState::Game_Init, 2.5f);

    // ? HoleMark 빌보드 스폰 (기존 코드 BoomLine, ParticleManager 근처에 추가)
    if (HoleMarkBillboardClass)
    {
        HoleMarkBillboard = GetWorld()->SpawnActor<AActor>(
            HoleMarkBillboardClass,
            FVector::ZeroVector,
            FRotator::ZeroRotator
        );

        if (HoleMarkBillboard)
        {
            // 초기에는 숨김
            HoleMarkBillboard->SetActorHiddenInGame(true);
            UE_LOG(LogTemp, Log, TEXT("? HoleMark billboard spawned"));
        }
    }

    BoomLine = GetWorld()->SpawnActor<ABoomLine>(BlueprintObjectsMap.Find(TEXT("BoomLine"))->Get());
    ParticleManager = GetWorld()->SpawnActor<AParticleManager>(FVector::ZeroVector, FRotator::ZeroRotator);
    ReadyBillboard = GetWorld()->SpawnActor<AReadyBillboard>(FVector::ZeroVector, FRotator::ZeroRotator);

    UE_LOG(LogTemp, Warning, TEXT("=== InGameMode BeginPlay Completed ==="));


    if (IsRangeMode())
    {
        FSoftClassPath TargetClassPath(TEXT("/Game/model_data/target/BP_Target.BP_Target_C"));

        if (TargetClassPath.IsValid())
        {
            UClass* TargetClass = TargetClassPath.TryLoadClass<AActor>();
            if (TargetClass)
            {
                BP_Target = GetWorld()->SpawnActor<AActor>(TargetClass, FVector::ZeroVector, FRotator::ZeroRotator);
            }
        }

        // ? 게임 스레드에서 안전하게 로딩
        if (!RangeHUDWidgetPath.IsEmpty())
        {
            TSoftClassPtr<UUserWidget> SoftClass(RangeHUDWidgetPath);
            if (UClass* LoadedClass = SoftClass.LoadSynchronous())
            {
                RangeHUDWidgetBPclass = LoadedClass;
                RangeHUDWidgetclass = RangeHUDWidgetBPclass;
                UE_LOG(LogTemp, Log, TEXT("? RangeHUDWidgetBPclass loaded in BeginPlay"));
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("? Failed to load RangeHUDWidgetBPclass"));
            }
        }

        FSoftClassPath TimerEndPopupWidgetClassPath(TEXT("/Game/UMG/UI/InGame/Popup/Practice/WBP_TimerEndPopup.WBP_TimerEndPopup_C"));
        if (UClass* LoadedClass = TimerEndPopupWidgetClassPath.TryLoadClass<UTimerEndPopupWidget>())
        {
            if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
            {
                TimerEndWidget = CreateWidget<UTimerEndPopupWidget>(PC, LoadedClass);
                if (TimerEndWidget)
                {
                    TimerEndWidget->AddToViewport(99999);
                    TimerEndWidget->SetVisibility(ESlateVisibility::Collapsed);
                }
            }

            UE_LOG(LogGameMode, Log, TEXT("? TimerEndWidget loaded"));
        }

    }
    else if (IsStrokeMode() || IsTrainingMode())
    {
        if (UClass* LoadedClass = HolecupIndicatorClassPath.TryLoadClass<UOffscreenIndicatorWidget>())
        {
            if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
            {
                HolecupIndicatorWidget = CreateWidget<UOffscreenIndicatorWidget>(PC, LoadedClass);
                if (HolecupIndicatorWidget)
                {
                    HolecupIndicatorWidget->AddToViewport(99999);
                    HolecupIndicatorWidget->SetVisibility(ESlateVisibility::Collapsed);
                }
            }

            UE_LOG(LogGameMode, Log, TEXT("HolecupIndicatorWidget loaded"));
        }

    }
    // ? 새로 추가: WebcamConfig 로드
    LoadWebcamConfig();

    if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
    {
        if (AGolfPlayerController* GolfPC = Cast<AGolfPlayerController>(PC))
        {
            //GolfPC->SetVideoSavingEnabled(WebcamSettings.bEnableVideoSaving);  // ? 이미 있음, 헤더 포함으로 해결
            if (GameInfo.GameOptions.SwingMotion)
                GolfPC->SetVideoSavingEnabled(true);
            else
                GolfPC->SetVideoSavingEnabled(false);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("? Failed to cast to AGolfPlayerController"));
        }
    }

    if (InGamePlayerSelectWidget)
    {
        if (InGamePlayerSelectWidget->PlayerSelect)
        {
            for (UWidget* Widget : InGamePlayerSelectWidget->PlayerSelect->WrapBox_PlayerProfiles->GetAllChildren())
            {
                UPlayerSelectProfileWidget* Profile = Cast<UPlayerSelectProfileWidget>(Widget);

                Profile->OnModifyPlayersDele.AddDynamic(this, &AInGameMode::HandleOnModifyPlayers);
                Profile->OnDeletePlayersDele.AddDynamic(this, &AInGameMode::HandleOnDeletePlayers);
            }
        }
    }

    // ? 모든 초기화가 끝난 후 AutoTee 연결
    GetWorld()->GetTimerManager().SetTimerForNextTick([this]() {
        ConnectAutoTeeDevice();
        });

    SpawnPuttingGuide();


    // ✅ 기존 다른 위젯들 초기화 코드 바로 아래에 추가
    if (HoleTransitionWidgetClass)
    {
        APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        if (PC)
        {
            HoleTransitionWidgetInstance = CreateWidget<UHoleTransitionWidget>(PC, HoleTransitionWidgetClass);
            if (HoleTransitionWidgetInstance)
            {
                HoleTransitionWidgetInstance->AddToViewport(6000);
                // 델리게이트 바인딩 — 애니메이션 끝나면 OnHoleTransitionFinished() 호출
                HoleTransitionWidgetInstance->OnTransitionFinished.AddDynamic(
                    this, &AInGameMode::OnHoleTransitionFinished);
                // NativeConstruct() 에서 이미 Collapsed 처리하므로 중복 호출 불필요
                // (넣어도 무방)
            }
        }
    }


    // UE 5.7에서 화면 디버그 메시지 완전 비활성화
    if (GEngine)
    {
        GEngine->bEnableOnScreenDebugMessages = false;
        GEngine->bEnableOnScreenDebugMessagesDisplay = false;  // 필요 시 추가
        UE_LOG(LogTemp, Warning, TEXT("✅ UE 5.7 화면 디버그 메시지 비활성화 완료"));
    }

   // InitHoleTransitionWidget();
}

void  AInGameMode::SetTestResult(int value)
{
    if (ResultWidgetClass)
    {

        if (ResultWidgetInstance)
        {
            ResultWidgetInstance->PlayResult(value);
           // ResultWidgetInstance->SetResultIndex(value);
            UE_LOG(LogTemp, Log, TEXT("========= ResultWidget -------------------Call"));
        }
       
    }

}

void AInGameMode::MoveBallOnPracticeMode()
{

    UE_LOG(LogGameMode, Log, TEXT("------------------------------------------AInGameMode::MoveBallOnPracticeMode()"));

    AGolfBall* Ball = GetCurrentTurnGolfBall();
    if (!IsValid(Ball) || !IsValid(PracticeModeStartPoint) || !IsValid(PracticeModeEndPoint))
    {
        UE_LOG(LogGameMode, Error, TEXT("MoveBallOnPracticeMode: Ball(%s) StartPoint(%s) EndPoint(%s) - abort"),
            IsValid(Ball) ? TEXT("OK") : TEXT("NULL"),
            IsValid(PracticeModeStartPoint) ? TEXT("OK") : TEXT("NULL"),
            IsValid(PracticeModeEndPoint) ? TEXT("OK") : TEXT("NULL"));
        return;
    }

    if (!IsValid(BP_Target))
    {
        UE_LOG(LogGameMode, Error, TEXT("MoveBallOnPracticeMode: BP_Target is NULL - target features disabled"));
    }

    FRotator LookAtRot = UKismetMathLibrary::FindLookAtRotation(Ball->GetActorLocation(), PracticeModeEndPoint->GetActorLocation());

    switch (CurrentPracticeMode)
    {
    case EPracticeMode::Driving:
        Ball->SetActorLocation(PracticeModeStartPoint->GetActorLocation() + FVector(0.f, 0.f, 7.f));
        if (IsValid(TeeAnimInstance))
        {
            TeeAnimInstance->SetActorLocation(Ball->GetActorLocation() - FVector(0.f, 0.f, 5.f));
        }
        if (IsValid(BP_Target))
        {
            BP_Target->SetActorLocation(PracticeModeStartPoint->GetActorLocation());
            UUtilLibrary::MoveActorTowardActorByDistanceSimple_KeepRotation(BP_Target, PracticeModeEndPoint, 8000.f, true);
            if (AGolfPlayerController* GolfPC = Cast<AGolfPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
            {
                GolfPC->GetAimActor()->SetActorLocation(BP_Target->GetActorLocation());
            }
            if (BallDistanceWidget)
            {
                BallDistanceWidget->SetCustomTargetLocation(BP_Target->GetActorLocation());
            }
        }
        LookAtRot = UKismetMathLibrary::FindLookAtRotation(Ball->GetActorLocation(), PracticeModeEndPoint->GetActorLocation());
        Ball->SetActorRotation(LookAtRot);
        break;

    case EPracticeMode::Approach:
        Ball->SetActorLocation(PracticeModeStartPoint->GetActorLocation() + FVector(0.f, 0.f, 5.f));
        TeeAnimInstance->SetActorLocation(FVector::ZeroVector);
        BP_Target->SetActorLocation(PracticeModeStartPoint->GetActorLocation());
        UUtilLibrary::MoveActorTowardActorByDistanceSimple_KeepRotation(BP_Target, PracticeModeEndPoint, RangeHUDWidgetInstance->ApproachModeDistance, true);
        LookAtRot = UKismetMathLibrary::FindLookAtRotation(Ball->GetActorLocation(), PracticeModeEndPoint->GetActorLocation());
        Ball->SetActorRotation(LookAtRot);
        if (AGolfPlayerController* GolfPC = Cast<AGolfPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
        {
            GolfPC->GetAimActor()->SetActorLocation(BP_Target->GetActorLocation());
        }
        if (BallDistanceWidget)
        {
            BallDistanceWidget->SetCustomTargetLocation(BP_Target->GetActorLocation());
        }
        break;
    case EPracticeMode::Putting:
        UE_LOG(LogGameMode, Log, TEXT("------------------------------------------AInGameMode::MoveBallOnPracticeMode() --- Putting"));
        BP_Target->SetActorLocation(FVector::ZeroVector); // 임시 주석
        Ball->SetActorLocation(PracticePuttingModeEndPoint->GetActorLocation() + FVector(0.f, 0.f, 10.f));
        //볼이 엔드포인트로 가고, 스타트포인트를 보고 있는 방향으로 5000cm 만큼 이동
        UUtilLibrary::MoveActorTowardActorByDistanceSimple_KeepRotation(Ball, PracticePuttingModeStartPoint, RangeHUDWidgetInstance->PuttingModeDistance, true);
        //Ball->SetActorLocation(Ball->GetActorLocation() - FVector(0.f, 0.f, 7.f));
        Ball->AdjustBallToGroundLevel();
        LookAtRot = UKismetMathLibrary::FindLookAtRotation(Ball->GetActorLocation(), PracticePuttingModeEndPoint->GetActorLocation());
        Ball->SetActorRotation(LookAtRot);
        if (AGolfPlayerController* GolfPC = Cast<AGolfPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
        {
            GolfPC->GetAimActor()->SetActorLocation(PracticePuttingModeEndPoint->GetActorLocation());
        }
        if (BallDistanceWidget)
        {
            BallDistanceWidget->SetCustomTargetLocation(
                PracticePuttingModeEndPoint->GetActorLocation());
        }
        break;
    default:
        break;
    }

}

void AInGameMode::HandleOnChangedDrivingCheckBoxState()
{
    MoveBallOnPracticeMode();
}

void AInGameMode::HandleOnChangedApproachCheckBoxState()
{
    MoveBallOnPracticeMode();
}

void AInGameMode::HandleOnChangedPuttingCheckBoxState()
{
    MoveBallOnPracticeMode();
}

void AInGameMode::SyncPlayerInfosToGameInfo()
{
    for (AGolfPlayer* Player : PlayerManager->GetPlayers())
    {
        for (FPlayerInfo& PlayerInfo : GameInfo.Players)
        {
            if (PlayerInfo.SlotIndex == Player->PlayerInfo.SlotIndex)
            {
                PlayerInfo = Player->PlayerInfo;
            }
        }
    }


}

void AInGameMode::HandleOnModifyPlayers(FPlayerInfo PlayerInfo)
{
    PlayerManager->InGameAddPlayer(GetWorld(), PlayerInfo);

    SaveGameInfoToJSON();
    InGameScoreBoardWidgetInstance->Init();
    InGameScoreBoardWidgetInstance->UpdateScoreBoard();
    InGameScoreBoardStatWidgetInstance->UpdateScoreBoardStats();
    UpdateBallNamePlateAndMarker();
    InGamePlayerSelectWidget->PlayerSelect->UpdateButtonStatus();
}

// 현제 플레이어일때 턴 넘겨주고 삭제해야함
void AInGameMode::HandleOnDeletePlayers(FPlayerInfo PlayerInfo)
{
    InGameScoreBoardWidgetInstance->DeleteMulliganForPlayer(PlayerInfo.SlotIndex);

    PlayerManager->InGameRemovePlayer(GetWorld(), PlayerInfo);


    // ✅ 삭제 후 CurrentPlayerIndex 유효성 보정
    int32 PlayerCount = PlayerManager->GetPlayers().Num();
    if (PlayerCount > 0 && !PlayerManager->GetPlayers().IsValidIndex(CurrentPlayerIndex))
    {
        CurrentPlayerIndex = 0;
    }


    UpdateAllPlayerInfoSlots();

    // 스코어보드/통계 UI 방어
    if (IsValid(InGameScoreBoardWidgetInstance))
    {
        InGameScoreBoardWidgetInstance->Init();
        InGameScoreBoardWidgetInstance->UpdateScoreBoard();
        InGameScoreBoardWidgetInstance->UpdateMulliganUse();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("HandleOnDeletePlayers: InGameScoreBoardWidgetInstance is null"));
    }

    if (IsValid(InGameScoreBoardStatWidgetInstance))
    {
        InGameScoreBoardStatWidgetInstance->UpdateScoreBoardStats();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("HandleOnDeletePlayers: InGameScoreBoardStatWidgetInstance is null"));
    }

    // 네임플레이트는 내부에서 방어한다고 가정 못하니 여기서도 방어
    MiniMapWidget->RemovePlayerFromMiniMap(FindPlayerSlotIndex(PlayerInfo.SlotIndex)->PlayerIndex);

    // PlayerSelect 접근 방어
    if (IsValid(InGamePlayerSelectWidget) && IsValid(InGamePlayerSelectWidget->PlayerSelect))
    {
        InGamePlayerSelectWidget->PlayerSelect->UpdateButtonStatus();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("HandleOnDeletePlayers: InGamePlayerSelectWidget or PlayerSelect is null"));
    }
}


void AInGameMode::InitInGameMenu()
{

    // ? 수정 1: Weak Pointer를 사용하여 비동기 호출 안전성 확보
    if (!IsInGameThread())
    {
        // 'this' 대신 WeakPtr을 캡처하여 객체 생존 여부 확인 후 실행
        TWeakObjectPtr<AInGameMode> WeakThis = this;
        AsyncTask(ENamedThreads::GameThread, [WeakThis]()
            {
                if (WeakThis.IsValid())
                {
                    WeakThis->InitInGameMenu();
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("?? InitInGameMenu skipped: GameMode is invalid"));
                }
            });
        return;
    }

    if (TimerWidgetClass)
    {
        APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        if (PC)
        {
            TimerWidgetInstance = CreateWidget<UTimerWidget>(PC, TimerWidgetClass);
            //if (TimerWidgetInstance)
            //{
            //    TimerWidgetInstance->SetVisibility(ESlateVisibility::Hidden); // 또는 Hidden
            UE_LOG(LogTemp, Log, TEXT("? TimerWidgetClass created and added to viewport"));
            //}
        }
    }

    if (IsRangeMode())
    {
        //if (RangeHUDWidgetclass)
        //{
        //    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);

        //    // ?  로컬 컨트롤러 유효성 체크 강화
        //    if (PC && PC->IsLocalController())
        //    {
        //        // 위젯 인스턴스가 유효하지 않을 때만 생성
        //        if (!IsValid(RangeHUDWidgetInstance))
        //        {
        //            RangeHUDWidgetInstance = CreateWidget<URangeHUDWidget>(PC, RangeHUDWidgetclass);
        //        }

        //        // 생성 후 유효성 재확인
        //        if (IsValid(RangeHUDWidgetInstance))
        //        {
        //            // ? 수정 3: 뷰포트에 없을 때만 추가 (중복 추가 방지)
        //            if (!RangeHUDWidgetInstance->IsInViewport())
        //            {
        //                RangeHUDWidgetInstance->AddToViewport(1001);
        //                UE_LOG(LogTemp, Log, TEXT("? RangeHUDWidgetInstance Added to Viewport"));
        //            }

        //            RangeHUDWidgetInstance->SetVisibility(ESlateVisibility::Visible);

        //            // WrapBox 안전성 체크
        //            if (IsValid(RangeHUDWidgetInstance->WrapBox_Menu) && IsValid(TimerWidgetInstance))
        //            {
        //                // 이미 자식으로 있는지 확인하는 로직이 있으면 좋지만, Slate는 중복 추가를 보통 처리함
        //                if (!TimerWidgetInstance->GetParent())
        //                {
        //                    RangeHUDWidgetInstance->WrapBox_Menu->AddChildToWrapBox(TimerWidgetInstance);
        //                }

        //                TimerWidgetInstance->SetUseRealTime(false);
        //                TimerWidgetInstance->Reset();
        //                TimerWidgetInstance->Start();
        //            }
        //            else
        //            {
        //                UE_LOG(LogTemp, Error, TEXT("? RangeHUD: WrapBox_Menu or TimerWidgetInstance is NULL"));
        //            }
        //        }
        //        else
        //        {
        //            UE_LOG(LogTemp, Error, TEXT("? Failed to create RangeHUDWidgetInstance"));
        //        }
        //    }
        //    else
        //    {
        //        UE_LOG(LogTemp, Warning, TEXT("?? Invalid PlayerController for RangeHUD creation"));
        //    }


        if (RangeHUDWidgetclass)
        {
            APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
            if (PC)
            {
                RangeHUDWidgetInstance = CreateWidget<URangeHUDWidget>(PC, RangeHUDWidgetclass);
                if (RangeHUDWidgetInstance)
                {
                    RangeHUDWidgetInstance->AddToViewport(1001); // ZOrder는 필요 시 조절
                    RangeHUDWidgetInstance->SetVisibility(ESlateVisibility::Visible); // 또는 Hidden
                    UE_LOG(LogTemp, Log, TEXT("? RangeHUDWidgetInstance created and added to viewport  -  AddChildToWrapBox -start"));

                    // ★ CreateWidget/AddChildToWrapBox 전부 삭제
                    if (IsValid(RangeHUDWidgetInstance) && IsValid(RangeHUDWidgetInstance->WBP_Timer))
                    {
                        UTimerWidget* Timer = RangeHUDWidgetInstance->WBP_Timer;

                        FAdminConfig AdminConfig;
                        if (UJsonLoader::LoadAdminConfigFromJson(TEXT("adminConfig.json"), AdminConfig))
                        {
                            Timer->SetInitialTotalSeconds(AdminConfig.PracticeTimeMinutes * 60 + 2);
                            Timer->SetUseRealTime(true);
                            Timer->Reset();
                            Timer->Start();
                            UE_LOG(LogTemp, Log, TEXT("?? RangeHUD 내장 WBP_Timer 시작: %d초"),
                                (int32)(AdminConfig.PracticeTimeMinutes * 60));
                        }
                        else
                        {
                            UE_LOG(LogTemp, Warning, TEXT("⚠️ adminConfig.json 로드 실패, 타이머 시작 안 됨"));
                        }
                    }
                    else
                    {
                        UE_LOG(LogTemp, Error, TEXT("❌ RangeHUD.WBP_Timer가 null - BindWidget 이름 확인 필요"));
                    }

                }
            }
        }

        if (RangeHUDStatWidgetClass)
        {
            APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
            if (PC)
            {
                UE_LOG(LogTemp, Log, TEXT("? RangeHUDStatWidgetInstance created and added to viewport - CreateWidgt"));
                RangeHUDStatWidgetInstance = CreateWidget<URangeHUDStatWidget>(PC, RangeHUDStatWidgetClass);
                if (RangeHUDStatWidgetInstance)
                {
                    RangeHUDStatWidgetInstance->AddToViewport(1002); // ZOrder는 필요 시 조절
                    RangeHUDStatWidgetInstance->SetVisibility(ESlateVisibility::Collapsed); // 또는 Hidden
                    UE_LOG(LogTemp, Log, TEXT("? RangeHUDStatWidgetInstance created and added to viewport"));
                }
            }
        }

        RangeHUDWidgetInstance->AverageLine = RangeHUDStatWidgetInstance->AverageLine;
        UE_LOG(LogTemp, Log, TEXT("? RangeHUDWidgetInstance->OnChangedApproachCheckBoxStateDele - start"));
        RangeHUDWidgetInstance->OnChangedApproachCheckBoxStateDele.AddDynamic(this, &AInGameMode::HandleOnChangedApproachCheckBoxState);
        RangeHUDWidgetInstance->OnChangedDrivingCheckBoxStateDele.AddDynamic(this, &AInGameMode::HandleOnChangedDrivingCheckBoxState);
        RangeHUDWidgetInstance->OnChangedPuttingCheckBoxStateDele.AddDynamic(this, &AInGameMode::HandleOnChangedPuttingCheckBoxState);
        RangeHUDWidgetInstance->UpdateApproachTargetMarker();
        UE_LOG(LogTemp, Log, TEXT("? RangeHUDWidgetInstance->OnChangedApproachCheckBoxStateDele -end"));
    }
    else if (IsStrokeMode())
    {
        //LoadStrokeWidget();
                // StrokeWidget 생성
       // ? 여기서 안전하게 로딩합니다.
        UClass* LoadedWidgetClass = StrokeWidgetClass.LoadSynchronous();

        if (LoadedWidgetClass && !IsValid(StrokeWidgetInstance))
        {
            APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
            if (PC)
            {
                StrokeWidgetInstance = CreateWidget<UStrokeWidget>(PC, LoadedWidgetClass);
                if (StrokeWidgetInstance)
                {
                    StrokeWidgetInstance->AddToViewport(1000);
                    // 필요한 경우 Visibility 설정
                    StrokeWidgetInstance->SetVisibility(ESlateVisibility::Visible);

                    UE_LOG(LogTemp, Log, TEXT("? StrokeWidget loaded and created safely via Soft Reference."));

                    InitCourseMapImage();
                }
            }
        }


        if (ResultVideoWidgetClass)
        {
            APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
            if (PC)
            {
                ResultVideoWidgetInstance = CreateWidget<UResultVideoWidget>(PC, ResultVideoWidgetClass);
                if (ResultVideoWidgetInstance)
                {
                    ResultVideoWidgetInstance->AddToViewport(20000); // ZOrder는 필요 시 조절
                    ResultVideoWidgetInstance->SetVisibility(ESlateVisibility::Collapsed); // 또는 Hidden
                    UE_LOG(LogTemp, Log, TEXT("? ResultVideoWidgetClass created and added to viewport"));
                }
            }


        }

        FSoftClassPath CameraModePopupWidgetClass(TEXT("/Game/UMG/UI/InGame/Popup/Menu/WBP_CameraModePopup.WBP_CameraModePopup_C"));

        if (UClass* LoadedClass = CameraModePopupWidgetClass.TryLoadClass<UCameraModePopupWidget>())
        {
            APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
            if (PC)
            {
                CameraModePopupWidget = CreateWidget<UCameraModePopupWidget>(PC, LoadedClass);
                if (CameraModePopupWidget)
                {
                    CameraModePopupWidget->AddToViewport(50000); // ZOrder는 필요 시 조절
                    CameraModePopupWidget->SetVisibility(ESlateVisibility::Collapsed); // 또는 Hidden
                    UE_LOG(LogTemp, Log, TEXT("? CameraModePopupWidget created and added to viewport"));
                }
            }
        }

        InitCourseMapImage();

        // TTS 초기화
        SetupTTS();
    }
    else if (IsTrainingMode())
    {
        // ? 여기서 안전하게 로딩합니다.
        UClass* LoadedWidgetClass = StrokeWidgetClass.LoadSynchronous();

        if (LoadedWidgetClass && !IsValid(StrokeWidgetInstance))
        {
            APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
            if (PC)
            {
                StrokeWidgetInstance = CreateWidget<UStrokeWidget>(PC, LoadedWidgetClass);
                if (StrokeWidgetInstance)
                {
                    StrokeWidgetInstance->AddToViewport(1000);
                    // 필요한 경우 Visibility 설정
                    StrokeWidgetInstance->SetVisibility(ESlateVisibility::Visible);

                    UE_LOG(LogTemp, Log, TEXT("? StrokeWidget loaded and created safely via Soft Reference."));

                    InitCourseMapImage();
                }
            }
        }

        FSoftClassPath CameraModePopupWidgetClass(TEXT("/Game/UMG/UI/InGame/Popup/Menu/WBP_CameraModePopup.WBP_CameraModePopup_C"));

        if (UClass* LoadedClass = CameraModePopupWidgetClass.TryLoadClass<UCameraModePopupWidget>())
        {
            APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
            if (PC)
            {
                CameraModePopupWidget = CreateWidget<UCameraModePopupWidget>(PC, LoadedClass);
                if (CameraModePopupWidget)
                {
                    CameraModePopupWidget->AddToViewport(50000); // ZOrder는 필요 시 조절
                    CameraModePopupWidget->SetVisibility(ESlateVisibility::Collapsed); // 또는 Hidden
                    UE_LOG(LogTemp, Log, TEXT("? CameraModePopupWidget created and added to viewport"));
                }
            }
        }

        InitCourseMapImage();
    }

    if (StrokeMenuWidgetClass && !StrokeMenuWidgetInstance)
    {
        APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        StrokeMenuWidgetInstance = CreateWidget<UStrokeMenuWidget>(PC, StrokeMenuWidgetClass);
        if (StrokeMenuWidgetInstance)
        {
            // 이 위젯이 다른 UMG 위젯 내부에 추가될 때 AddToViewport 대신 AddToHierarchy를 사용합니다.
            // StrokeWidget의 특정 패널에 추가하는 것이 일반적입니다.
            // 예: UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(GetRootWidget());
            //     if (RootCanvas) RootCanvas->AddChildToCanvas(StrokeMenuWidgetInstance);
            // 여기서는 간단하게 StrokeWidget의 자식으로 추가합니다.
            //StrokeMenuWidgetInstance->AddToRoot(); // 또는 특정 패널에 추가

            // 초기에는 메뉴를 숨김
            StrokeMenuWidgetInstance->AddToViewport(1500); // ZOrder는 필요에 따라 조절
            StrokeMenuWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);


            // StrokeMenuWidget의 버튼 클릭 이벤트에 바인딩
            //StrokeMenuWidgetInstance->OnMenuButtonClicked.AddDynamic(this, &UStrokeWidget::OnStrokeMenuButtonClicked);

            UE_LOG(LogTemp, Log, TEXT("StrokeMenuWidget created and added to StrokeWidget."));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("StrokeMenuWidgetClass is null. Cannot create StrokeMenuWidget."));
    }



    // 새롭게 추가: InGameScoreBoardWidget 생성 및 뷰포트 추가
    if (InGameScoreBoardWidgetClass)
    {
        APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        if (PC)
        {
            InGameScoreBoardWidgetInstance = CreateWidget<UInGameScoreBoardWidget>(PC, InGameScoreBoardWidgetClass);
            if (InGameScoreBoardWidgetInstance)
            {
                InGameScoreBoardWidgetInstance->AddToViewport(3000); // ZOrder는 필요에 따라 조절
                InGameScoreBoardWidgetInstance->SetVisibility(ESlateVisibility::Collapsed); // 초기에는 숨겨둡니다.
                UE_LOG(LogTemp, Log, TEXT("? InGameScoreBoardWidgetInstance created and added to viewport"));
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("? Failed to create InGameScoreBoardWidgetInstance."));
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("?? InGameScoreBoardWidgetClass is not set, cannot create scoreboard."));
    }

    if (InGameScoreBoardStatWidgetClass)
    {
        APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        if (PC)
        {
            InGameScoreBoardStatWidgetInstance = CreateWidget<UInGameScoreBoardStatWidget>(PC, InGameScoreBoardStatWidgetClass);
            if (InGameScoreBoardStatWidgetInstance)
            {
                InGameScoreBoardStatWidgetInstance->AddToViewport(3001); // ZOrder는 필요에 따라 조절
                InGameScoreBoardStatWidgetInstance->SetVisibility(ESlateVisibility::Collapsed); // 초기에는 숨겨둡니다.
                UE_LOG(LogTemp, Log, TEXT("? InGameScoreBoardWidgetInstance created and added to viewport"));
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("? Failed to create InGameScoreBoardWidgetInstance."));
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("?? InGameScoreBoardWidgetClass is not set, cannot create scoreboard."));
    }

    UE_LOG(LogTemp, Log, TEXT("Game mode started"));

}


void AInGameMode::InitPlayersInfo()
{
    LoadGameInfoFromJSON();
    GetWorld()->GetTimerManager().SetTimerForNextTick([this]() {

        SetupTerrainHeightGrid();

        DebugWidgetClass();

        if (!MiniMapWidgetClass)
        {
            UE_LOG(LogTemp, Warning, TEXT("?? MiniMapWidgetClass not loaded in constructor, trying runtime load..."));
            LoadMiniMapWidgetAtRuntimeSafe();
        }

        // 플레이어 컨트롤러 및 위젯 초기화
        if (UWorld* World = GetWorld())
        {
            if (APlayerController* PC = World->GetFirstPlayerController())
            {
                if (AGolfPlayerController* GolfPC = Cast<AGolfPlayerController>(PC))
                {
                    GolfPC->ShotControlWidgetClass = DefaultShotControlWidget;
                    GolfPC->CreateShotControlWidget();
                    if (GolfPC->ShotControlWidget)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("?? ShotControlWidget created successfully!"));
                    }
                    // ? 메인 HUD 위젯 생성 및 플레이어 슬롯 설정 (여기서 UBP_InGame을 사용한다고 가정)
                    // WBP_InGame 클래스를 로드합니다.
                    if (StrokeWidgetInstance)
                    {
                        // WBP_InGame 내의 StrokeWidget 부분에 대한 참조를 가져옵니다.
                        // WBP_InGame에서 StrokeWidget C++ 클래스를 지정한 UMG 위젯 변수를 가지고 있어야 합니다.
                        // 예: UPROPERTY(meta=(BindWidget)) UStrokeWidget* StrokeWidgetInstance;
                        // 그리고 WBP_InGame의 계층 구조에서 이 변수 이름으로 StrokeWidget이 바인딩되어야 합니다.
                        // 해당 오류 메시지를 보았으므로, WBP_InGame 블루프린트 내부에 StrokeWidget이 제대로 바인딩되어야 합니다.

                        // StrokeWidgetInstance에서 PlayerSlotsContainer (VerticalBox)를 찾습니다.
                        UPanelWidget* PlayerSlotsContainer = Cast<UPanelWidget>(StrokeWidgetInstance->GetWidgetFromName(TEXT("VerticalBox_PlayerList"))); // WBP_InGame의 VerticalBox 변수명
                        if (PlayerSlotsContainer)
                        {
                            // 플레이어 수만큼 슬롯 위젯 생성
                            PlayerInfoSlotWidgets.Empty();
                            PlayerSlotsContainer->ClearChildren();

                            for (int32 i = 0; i < GameInfo.Players.Num(); ++i) // GameInfo.Players는 초기화 후 유효하다고 가정
                            {
                                if (PlayerInfoSlotWidgetClass)
                                {
                                    UPlayerInfoSlotWidget* PlayerSlot = CreateWidget<UPlayerInfoSlotWidget>(PC, PlayerInfoSlotWidgetClass);
                                    if (PlayerSlot)
                                    {// ? 거리 계산 로직 추가
                                        float CurrentPlayerDistanceToHole = 0.0f;
                                        AGolfBall* PlayerBall = PlayerManager->GetPlayerBalls().IsValidIndex(i) ? PlayerManager->GetPlayerBalls()[i] : nullptr;
                                        if (IsValid(PlayerBall) && MapInfo.HolecupPositions.IsValidIndex(CurrentHole - 1))
                                        {
                                            CurrentPlayerDistanceToHole = FVector::Dist(PlayerBall->GetActorLocation(), MapInfo.HolecupPositions[CurrentHole - 1]);
                                        }
                                        // ? SetPlayerInfo 호출 시 DistanceToHole 전달
                                        PlayerSlot->OwningPlayerIndex = i;
                                        PlayerSlot->OwningPlayerSlotIndex = GameInfo.Players[i].SlotIndex;
                                        PlayerSlot->DisplayIndex = i;
                                        PlayerSlot->SetPlayerInfo(GameInfo.Players[i], CurrentHole, i, CurrentPlayerDistanceToHole);
                                        PlayerSlotsContainer->AddChild(PlayerSlot);
                                        PlayerInfoSlotWidgets.Add(PlayerSlot);
                                        UE_LOG(LogTemp, Log, TEXT("? Player slot for %s created."), *GameInfo.Players[i].NickName);
                                    }
                                }
                            }
                            // 초기 턴 플레이어 하이라이트
                            HighlightCurrentPlayerSlot(CurrentPlayerIndex);
                        }
                        else
                        {
                            UE_LOG(LogTemp, Error, TEXT("? PlayerSlotsContainer not found in WBP_InGame. Make sure 'Is Variable' is checked and name is correct."));
                        }
                    }
                }
            }
        }
        UE_LOG(LogTemp, Warning, TEXT("=== InGameMode BeginPlay Completed ==="));
        });
}


FGameStateMachine& AInGameMode::GetStateMachine()
{
    return StateMachine;
}

void AInGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);

    const FString Diff = UGameplayStatics::ParseOption(Options, TEXT("GameMode"));
    int32 mode = FCString::Atoi(*Diff);

    CurrentGameMode = static_cast<EGolfGameMode>(mode);
}


void AInGameMode::Tick(float DeltaTime)
{
    SCOPE_CYCLE_COUNTER(STAT_InGameModeTick);
    Super::Tick(DeltaTime);
    ProcessStateMachine(DeltaTime);

    if (IsStrokeMode() || IsTrainingMode())
    {
        if (bGameInitialized && StateMachine.CurrentState == EGameState::Game_Play)
        {
            if (HolecupIndicatorWidget && HoleMarkBillboard)
            {
                HolecupIndicatorWidget->UpdateForTarget(UGameplayStatics::GetPlayerController(GetWorld(), 0), HoleMarkBillboard->GetActorLocation() - FVector(0.f, 0.f, 400.f));
            }
        }
    }
}

void AInGameMode::LoadMiniMapWidgetAtRuntimeSafe()
{
    UE_LOG(LogTemp, Log, TEXT("?? Safe runtime loading without UAssetManager..."));

    TArray<FString> PossiblePaths = {
        TEXT("/Game/GolfGameBluePrint/WBP_GolfMiniMap.WBP_GolfMiniMap_C"),
        TEXT("/Game/GolfGameBluePrint/WBP_MiniMap.WBP_MiniMap_C"),
        TEXT("/Game/GolfGameBluePrint/WBP_GolfMiniMap.WBP_GolfMiniMap_C")
    };

    for (const FString& Path : PossiblePaths)
    {
        UClass* LoadedClass = LoadClass<UGolfMiniMap>(nullptr, *Path);
        if (LoadedClass)
        {
            MiniMapWidgetClass = LoadedClass;
            UE_LOG(LogTemp, Log, TEXT("? MiniMap loaded safely: %s"), *Path);
            return;
        }
    }

    FSoftClassPath MiniMapPath(TEXT("/Game/GolfGameBluePrint/WBP_MiniMap.WBP_MiniMap_C"));
    UClass* LoadedClass = MiniMapPath.TryLoadClass<UGolfMiniMap>();
    if (LoadedClass)
    {
        MiniMapWidgetClass = LoadedClass;
        UE_LOG(LogTemp, Log, TEXT("? MiniMap loaded via SoftClassPath"));
        return;
    }

    UBlueprint* MiniMapBP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/GolfGameBluePrint/WBP_MiniMap"));
    if (MiniMapBP && MiniMapBP->GeneratedClass)
    {
        if (MiniMapBP->GeneratedClass->IsChildOf(UGolfMiniMap::StaticClass()))
        {
            MiniMapWidgetClass = MiniMapBP->GeneratedClass;
            UE_LOG(LogTemp, Log, TEXT("? MiniMap loaded via Blueprint asset"));
            return;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("?? Using default C++ class as fallback"));
    MiniMapWidgetClass = UGolfMiniMap::StaticClass();
}


void AInGameMode::ProcessStateMachine(float DeltaTime)
{
    SCOPE_CYCLE_COUNTER(STAT_InGameModeStateMachine);
    StateMachine.Update(DeltaTime);

    if (StateMachine.JustEnteredState())
    {
        UE_LOG(LogGameMode, Log, TEXT("State transition: %s -> %s [%s Mode]"),
            *UEnum::GetValueAsString(StateMachine.PreviousState),
            *UEnum::GetValueAsString(StateMachine.CurrentState),
            *UEnum::GetValueAsString(CurrentGameMode));

        HandleStateExit(StateMachine.PreviousState);
        HandleStateEnter(StateMachine.CurrentState);
        StateMachine.bStateChanged = false;
    }

    if (RequiresContinuousUpdate(StateMachine.CurrentState))
    {
        UpdateCurrentState(DeltaTime);
    }
}

void AInGameMode::HandleStateEnter(EGameState NewState)
{
    UE_LOG(LogGameMode, Verbose, TEXT("Entering state: %s (Time: %.2f) [%s Mode]"),
        *UEnum::GetValueAsString(NewState),
        StateMachine.GetStateTime(),
        *UEnum::GetValueAsString(CurrentGameMode));

    switch (NewState)
    {
    case EGameState::Game_Init:
        OnEnterGameInit();
        break;
    case EGameState::Game_HoleInit:
        OnEnterHoleInit();
        break;
    case EGameState::Game_HoleReady:
        OnEnterHoleReady();
        break;
    case EGameState::Game_HoleStart:
        OnEnterHoleStart();
        break;
    case EGameState::Game_Play:
        OnEnterGamePlay();
        break;
    case EGameState::Game_HoleOut:
        OnEnterHoleOut();
        break;
    case EGameState::Game_HoleResults:
        OnEnterHoleResults();
        break;
    case EGameState::Game_Results:
        OnEnterGameResults();
        break;
    case EGameState::Game_End:
        OnEnterGameEnd();
        break;
    default:
        UE_LOG(LogGameMode, Warning, TEXT("Unhandled state enter: %s"),
            *UEnum::GetValueAsString(NewState));
        break;
    }
}
void AInGameMode::OnExitGameResults()
{
    UE_LOG(LogGameMode, Log, TEXT("-----Exiting game results"));
    // 결과 위젯 숨기기

}

void AInGameMode::HandleStateExit(EGameState OldState)
{
    UE_LOG(LogGameMode, VeryVerbose, TEXT("Exiting state: %s"),
        *UEnum::GetValueAsString(OldState));

    switch (OldState)
    {
    case EGameState::Game_Play:
        OnExitGamePlay();
        break;
    case EGameState::Game_HoleOut:
        OnExitHoleOut();
        break;
    default:
        break;
    }
}

void AInGameMode::UpdateCurrentState(float DeltaTime)
{
    switch (StateMachine.CurrentState)
    {
    case EGameState::Game_HoleStart:
        UpdateHoleStart(DeltaTime);
        break;
    case EGameState::Game_Play:
        UpdateGamePlay(DeltaTime);
        break;
    default:
        break;
    }
}

void AInGameMode::UpdateHoleStart(float DeltaTime)
{
    if (StateMachine.GetStateTime() > 1.0f) // 1초 대기
    {
        ChangeGameState(EGameState::Game_Play);
    }
}

bool AInGameMode::RequiresContinuousUpdate(EGameState State) const
{
    switch (State)
    {
    case EGameState::Game_HoleStart:
    case EGameState::Game_Play:
        return true;
    default:
        return false;
    }
}

void AInGameMode::InitializeStateTransitions()
{
}

void AInGameMode::ChangeGameState(EGameState NewState, float Delay)
{
    //UE_LOG(LogGameMode, Log, TEXT("-----ChangeGameState  Old-[%s]   New-[%s]... Delay-[%f]"), *UEnum::GetValueAsString(StateMachine.CurrentState), *UEnum::GetValueAsString(NewState), Delay);
    if (StateMachine.IsTransitioning() && StateMachine.PendingState == NewState)
    {
        return;
    }
    if (!CanTransitionTo(NewState))
    {
        UE_LOG(LogGameMode, Warning, TEXT("Invalid state transition: %s -> %s"),
            *UEnum::GetValueAsString(StateMachine.CurrentState),
            *UEnum::GetValueAsString(NewState));
        return;
    }

    StateMachine.ChangeState(NewState, Delay);

    if (Delay > 0.0f)
    {
        UE_LOG(LogGameMode, Log, TEXT("-----Scheduled state transition: %s -> %s (Delay: %.2f)"),
            *UEnum::GetValueAsString(StateMachine.CurrentState),
            *UEnum::GetValueAsString(NewState), Delay);
    }

}

bool AInGameMode::CanTransitionTo(EGameState NewState) const
{
    EGameState CurrentState = StateMachine.CurrentState;

    if (CurrentState == NewState)
        return false;

    switch (CurrentState)
    {
    case EGameState::Game_None:
        return (NewState == EGameState::Game_Init);
    case EGameState::Game_Init:
        return (NewState == EGameState::Game_HoleInit);
    case EGameState::Game_HoleInit:
        return (NewState == EGameState::Game_HoleReady);
    case EGameState::Game_HoleReady:
        return (NewState == EGameState::Game_HoleStart);
    case EGameState::Game_HoleStart:
        return (NewState == EGameState::Game_Play);
    case EGameState::Game_Play:
        return (NewState == EGameState::Game_HoleOut || NewState == EGameState::Game_End);
    case EGameState::Game_HoleOut:
        // Allow transitioning to Game_HoleInit (for next hole), Game_Results, or Game_HoleResults
        return (NewState == EGameState::Game_HoleInit || NewState == EGameState::Game_Results || NewState == EGameState::Game_End || NewState == EGameState::Game_HoleResults);
    case EGameState::Game_HoleResults:
        return (NewState == EGameState::Game_HoleInit || NewState == EGameState::Game_Results || NewState == EGameState::Game_End || NewState == EGameState::Game_Exit);
    case EGameState::Game_Results:
        // Allow transitioning to Game_End, Game_Exit, or Game_HoleResults (if needed to re-enter)
        return (NewState == EGameState::Game_End || NewState == EGameState::Game_Exit || NewState == EGameState::Game_HoleResults);
    case EGameState::Game_End:
        return (NewState == EGameState::Game_Exit);
    default:
        return false;
    }
}


void AInGameMode::OnEnterGameInit()
{
    UE_LOG(LogGameMode, Log, TEXT("---------------Game_Init State OnEnter [%s Mode]- Initializing game..."),
        *UEnum::GetValueAsString(CurrentGameMode));

    // ? 라운드 재개 체크
    if (CanResumeRound() && IsStrokeMode())
    {
        bIsContinueGame = true;
        LoadGameInfoFromJSON();
        bool bAllHoleout = true;

        LatestShotSlotIndex = GameInfo.LatestShotPlayerSlotIndex == -1 ? GameInfo.Players[0].SlotIndex : GameInfo.LatestShotPlayerSlotIndex;

        for (int32 i = 0; i < GameInfo.Players.Num(); i++)
        {
            FPlayerInfo PlayerInfo = GameInfo.Players[i];
            if (!PlayerInfo.bIsHoleout)
                bAllHoleout = false;
        }

        if (bAllHoleout)
        {
            CurrentHole++;
            for (int32 i = 0; i < GameInfo.Players.Num(); i++)
            {
                GameInfo.CurrentHole = CurrentHole;
                GameInfo.Players[i].bIsHoleout = false;
                GameInfo.Players[i].HoleCount += 1;
                for (int32 j = 0; j < CurrentHole - 1; j++)
                {
                    if (!GameInfo.Players[i].HoleScores.IsValidIndex(j))
                    {
                        GameInfo.Players[i].HoleScores.Add(0);
                    }
                }
                GameInfo.Players[i].HoleScores.Add(GameInfo.SelectedMap.ParScores[CurrentHole - 1] - GameInfo.Players[i].ShotCount);
                GameInfo.Players[i].BallPosX = MapInfo.TeePositions[CurrentHole - 1].X;
                GameInfo.Players[i].BallPosY = MapInfo.TeePositions[CurrentHole - 1].Y;
                GameInfo.Players[i].BallPosZ = MapInfo.TeePositions[CurrentHole - 1].Z;
            }
        }

        SaveGameInfoToJSON();
        UE_LOG(LogGameMode, Log, TEXT("? 이전 라운드 재개: Hole %d부터 시작"), GameInfo.CurrentHole);

        if (IsStrokeMode())
        {
            OnEnterGameInit_StrokeMode();
            InitializeGame();
            if (bGameInitialized)
            {
                InitInGameMenu();
                InitPlayersInfo();
                InitConcedeLines();
               // PlayHoleTransition();
                ChangeGameState(EGameState::Game_HoleInit, 0.5f);
            }
        }
        return;
    }


    // Range Mode인 경우 별도 레벨로 전환
    if (IsRangeMode())
    {
        // OnEnterGameInit_RangeMode();
        // return;
    }

    // Stroke Mode와 Training Mode는 동일한 초기화 과정
    if (IsStrokeMode())
    {
        OnEnterGameInit_StrokeMode();
    }
    else if (IsTrainingMode())
    {
        OnEnterGameInit_TrainingMode();
    }

    InitializeGame();

    UE_LOG(LogGameMode, Warning, TEXT("?? bGameInitialized -------------------------------------- bGameInitialized [%d]"), bGameInitialized);

    if (bGameInitialized)
    {
        InitInGameMenu();
        InitPlayersInfo();
        InitConcedeLines();

        if (IsTrainingMode())
        {
            StrokeMenuWidgetInstance->GetButtonFromUserWidget(StrokeMenuWidgetInstance->WBP_InGame_Menu_ScoreCard, TEXT("Button_Menu"))->SetIsEnabled(false);
            StrokeMenuWidgetInstance->GetButtonFromUserWidget(StrokeMenuWidgetInstance->WBP_InGame_Menu_PlayerAdd, TEXT("Button_Menu"))->SetIsEnabled(false);
            //  StrokeMenuWidgetInstance->GetButtonFromUserWidget(StrokeMenuWidgetInstance->WBP_InGame_Menu_Button_3, TEXT("Button_Menu"))->SetIsEnabled(false);
            StrokeMenuWidgetInstance->GetButtonFromUserWidget(StrokeMenuWidgetInstance->WBP_InGame_Menu_PenaltyDrop, TEXT("Button_Menu"))->SetIsEnabled(false);
            StrokeMenuWidgetInstance->GetButtonFromUserWidget(StrokeMenuWidgetInstance->WBP_InGame_Menu_SkipTurn, TEXT("Button_Menu"))->SetIsEnabled(false);
        }

        ChangeGameState(EGameState::Game_HoleInit, 0.5f);
    }


}

void AInGameMode::InitTourActor()
{
    FSoftClassPath TourActorClass(TEXT("/Game/GolfGameBluePrint/BP_TourActor.BP_TourActor_C"));
    UClass* LoadedTourActorClass = TourActorClass.TryLoadClass<ATourActor>();
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    if (LoadedTourActorClass)
    {
        TourActor = GetWorld()->SpawnActor<ATourActor>(
            LoadedTourActorClass,
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            Params
        );
    }
}

void AInGameMode::OnEnterHoleInit()
{
    UE_LOG(LogGameMode, Log, TEXT("----------------Game_Hole_init State OnEnter [%s Mode]------Initializing hole %d..."),
        *UEnum::GetValueAsString(CurrentGameMode), CurrentHole);

    InitializeHole();

    if (IsHoleInitializationComplete())
    {
        ChangeGameState(EGameState::Game_HoleReady, 0.5f);
    }

    // Range Mode는 이 함수에 도달하지 않아야 함 (이미 다른 레벨로 전환됨)
    if (IsRangeMode())
    {
        UUtilLibrary::FadeIn(GetWorld(), 1.f, FFadeCallback::CreateLambda([this]()
            {
                StopLoading();
                UUtilLibrary::FadeOut(GetWorld(), 0.6f);
            }));

        UE_LOG(LogGameMode, Warning, TEXT("?? Range Mode should not reach HoleInit state"));
        return;
    }


    // Stroke Mode와 Training Mode 처리
    if (IsStrokeMode())
    {
        OnEnterHoleInit_StrokeMode();
        // ? 홀 초기화 시 마커 위치 업데이트
        UpdateHoleMarkPosition();

        UUtilLibrary::FadeIn(GetWorld(), 0.1f, FFadeCallback::CreateLambda([this]()
            {
                StopLoading();
                SetShowScoreBoard(0);

                if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
                {
                    AGolfPlayerController* GPC = Cast<AGolfPlayerController>(PC);

                    GPC->ShotCinematicComponent->StopCinematic(0.f);
                }

                UUtilLibrary::FadeOut(GetWorld(), 0.6f);
            }));
    }
    else if (IsTrainingMode())
    {

        OnEnterHoleInit_TrainingMode();
        // ? 홀 초기화 시 마커 위치 업데이트
        UpdateHoleMarkPosition();
        UUtilLibrary::FadeIn(GetWorld(), 1.f, FFadeCallback::CreateLambda([this]()
            {
                StopLoading();
                UUtilLibrary::FadeOut(GetWorld(), 0.6f);
            }));
    }
}

// ? Stroke Mode 홀 초기화
void AInGameMode::OnEnterHoleInit_StrokeMode()
{
    UE_LOG(LogGameMode, Log, TEXT("-- Initializing Hole for Stroke Mode --- %d H"), CurrentHole);
    // Stroke Mode 특화 홀 초기화 로직


    if (TourActor)
    {
        TourActor->SetSplineForHole(CurrentHole);
    }

    AGolfPlayerController* PC = Cast<AGolfPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
    if (PC->bTerrainGridVisible)
        PC->ToggleTerrainGrid();

    PlayHoleTransition();

    InitializeOBLines();

}

// ? Training Mode 홀 초기화
void AInGameMode::OnEnterHoleInit_TrainingMode()
{
    UE_LOG(LogGameMode, Log, TEXT("?? Initializing Hole for Training Mode"));

    // Training Mode에서는 홀 완료 체크를 비활성화
    if (PlayerManager)
    {
        TArray<AGolfPlayer*> Players = PlayerManager->GetPlayers();
        if (Players.IsValidIndex(0))
        {
            AGolfPlayer* TrainingPlayer = Players[0];


            // 플레이어 상태를 항상 Ready로 유지
            TrainingPlayer->SetPlayerState(EPlayerState::Player_Ready);

            UE_LOG(LogGameMode, Log, TEXT("? Training player configured"));
        }
    }

    // 미니맵 설정
    if (IsValid(MiniMapWidget))
    {
        //MiniMapWidget->bAllowBallMovement = true;
        UE_LOG(LogGameMode, Log, TEXT("? MiniMap configured for Training Mode"));
    }


    InitializeOBLines();
}

void AInGameMode::OnEnterHoleReady()
{


    UE_LOG(LogGameMode, Log, TEXT("-----Hole %d ready"), CurrentHole);

    if (CurrentGameMode == EGolfGameMode::StrokeMode || CurrentGameMode == EGolfGameMode::TrainingMode)
    {

       
        StrokeWidgetInstance->UpdateMapInfo(CurrentHole, MapInfo.ParScores[CurrentHole - 1], FVector::Dist(MapInfo.HolecupPositions[CurrentHole - 1], MapInfo.TeePositions[CurrentHole - 1]));

    }



    ChangeGameState(EGameState::Game_HoleStart, 3.0f);
}

void AInGameMode::OnEnterHoleStart()
{
    UE_LOG(LogGameMode, Log, TEXT("-----Starting hole %d"), CurrentHole);

    InitTeeAnim();
    AGolfPlayerController* PC = Cast<AGolfPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
    PC->UpdateAimActorPosition();

    UpdateBallNamePlateAndMarker();

    StartHole();

}

void AInGameMode::OnEnterGamePlay()
{
    bSetNextHole = false;

    UE_LOG(LogGameMode, Log, TEXT("-----Game play started for hole %d [%s Mode]"),
        CurrentHole, *UEnum::GetValueAsString(CurrentGameMode));

    // Range Mode는 이 함수에 도달하지 않아야 함
    if (IsRangeMode())
    {
        UE_LOG(LogGameMode, Warning, TEXT("?? Range Mode should not reach GamePlay state"));

        return;
    }

    UpdateMiniMapForCurrentHole();

    // 게임 모드별 처리
    if (IsStrokeMode())
    {

        OnEnterGamePlay_StrokeMode();
        StrokeWidgetInstance->WBP_Distance->InitShotDistanceText();
        InGameScoreBoardStatWidgetInstance->UpdateScoreBoardStats();
        if (GameInfo.bIsRoundEnd)
        {
            GameInfo.bIsRoundEnd = false;
        }

    }
    else if (IsTrainingMode())
    {
        OnEnterGamePlay_TrainingMode();
        StrokeWidgetInstance->WBP_Distance->InitShotDistanceText();
    }

    // 스코어보드 표시
    if (IsValid(InGameScoreBoardWidgetInstance))
    {
        SetShowScoreBoard(0);
    }

    SaveGameInfoToJSON();
}

// ? Stroke Mode 게임 플레이
void AInGameMode::OnEnterGamePlay_StrokeMode()
{
    UE_LOG(LogGameMode, Log, TEXT("??? Starting Stroke Mode gameplay"));

    MSC->SetMediaPlayer(ResultVideoWidgetInstance->MediaPlayer);

    const bool bSkipHoleOutTransition = bIsContinueGame;
    if (bSkipHoleOutTransition)
    {
        UE_LOG(LogGameMode, Log, TEXT("Resume detected: skipping Player_HoleOut transitions in OnEnterGamePlay_StrokeMode"));
    }
    else
    {
        int32 HoleInPlayerCount = 0;
        bool bCurrentPlayerHoleOut = false;
        AGolfPlayer* CurrentTurnPlayer = nullptr;
        if (PlayerManager && PlayerManager->GetPlayers().IsValidIndex(CurrentPlayerIndex))
        {
            CurrentTurnPlayer = PlayerManager->GetPlayers()[CurrentPlayerIndex];
        }
        if (PlayerManager)
        {
            for (AGolfPlayer* Player : PlayerManager->GetPlayers())
            {
                if (Player->IsHoleIn())
                {
                    Player->SetPlayerState(EPlayerState::Player_HoleOut);
                    HoleInPlayerCount++;
                    if (Player == CurrentTurnPlayer)
                    {
                        bCurrentPlayerHoleOut = true;
                    }
                }
            }
        }

        if (HoleInPlayerCount && bCurrentPlayerHoleOut)
            PlayerManager->AdvanceTurn();
    }

    if (bIsContinueGame)
    {
        // 이어하기 상태 초기화는 모든 볼/플레이어 세팅 이후에 수행
        if (PlayerManager)
        {
            for (AGolfPlayer* Player : PlayerManager->GetPlayers())
            {
                if (Player)
                {
                    Player->bIsContinue = false;
                }
            }
        }
        bIsContinueGame = false;
    }
    // Stroke Mode 특화 게임플레이 로직
    // 정규 스트로크 플레이 규칙 적용
}
// ? Training Mode 게임 플레이
void AInGameMode::OnEnterGamePlay_TrainingMode()
{
    UE_LOG(LogGameMode, Log, TEXT("?? Starting Training Mode gameplay"));

    // Training Mode에서는 턴 진행 없음
    CurrentPlayerIndex = 0; // 항상 첫 번째 플레이어

    // UI 업데이트
    if (StrokeWidgetInstance)
    {
        // StrokeWidgetInstance->SetTrainingModeUI(true);
       //  StrokeWidgetInstance->ShowTrainingModeHelp();
    }

    // 화면에 Training Mode 안내 표시
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan,
            TEXT("?? Training Mode: Click on minimap to move ball"));
    }
}
// ? 게임 모드별 초기화 함수
void AInGameMode::InitializeGameByMode()
{
    switch (CurrentGameMode)
    {
    case EGolfGameMode::StrokeMode:
        UE_LOG(LogGameMode, Log, TEXT("??? Initializing for Stroke Mode"));
        // Stroke Mode 특화 초기화
        break;

    case EGolfGameMode::TrainingMode:
        UE_LOG(LogGameMode, Log, TEXT("?? Initializing for Training Mode"));
        GameInfo.bIsRoundEnd = true;
        SaveGameInfoToJSON();
        // Training Mode 특화 초기화
        // 예: 훈련용 UI 요소, 가이드 시스템 등
        break;

    case EGolfGameMode::RangeMode:
        UE_LOG(LogGameMode, Log, TEXT("????♂? Range Mode - should transition to Range Level"));
        // Range Mode는 별도 레벨로 전환되므로 여기서는 최소한의 처리만
        GameInfo.bIsRoundEnd = true;
        SaveGameInfoToJSON();
        TransitionToRangeLevel();
        return;

    default:
        UE_LOG(LogGameMode, Warning, TEXT("?? Unknown game mode, defaulting to Stroke Mode"));
        CurrentGameMode = EGolfGameMode::StrokeMode;
        break;
    }
}


void AInGameMode::OnEnterHoleOut()
{
    UE_LOG(LogGameMode, Log, TEXT("-----Hole %d completed"), CurrentHole);

    TArray<AGolfPlayer*> Players = PlayerManager->GetPlayers();
    TArray<FPlayerInfo> PlayerInfos;

    for (AGolfPlayer* Player : Players)
    {
        PlayerInfos.Add(Player->GetPlayerInfo());
        Player->bIsRuntimeAdded = false;

        //다음 홀 이동시 상태 초기화
        Player->SetHoleIn(false);
        if (AGolfBall* Ball = FindBall(Player->PlayerIndex))
        {
            Ball->SetConceded(false);
            Ball->SetHoleIn(false);
        }
    }

    for (int32 i = 0; i < GameInfo.Players.Num(); i++)
    {
        if (PlayerInfoSlotWidgets.IsValidIndex(i))
            PlayerInfoSlotWidgets[i]->bIsRuntimeAdded = false;
    }

    //UpdateGameInfoPlayers(PlayerInfos);
    InGameScoreBoardWidgetInstance->UpdateMulliganUse();

    EndHole();

    ChangeGameState(EGameState::Game_HoleResults, 1.f);
}


void AInGameMode::OnEnterHoleResults()
{
    UE_LOG(LogGameMode, Log, TEXT("-----Showing hole %d results [%s Mode]"),
        CurrentHole, *UEnum::GetValueAsString(CurrentGameMode));

    if (IsStrokeMode())
    {
        for (AGolfPlayer* Player : PlayerManager->GetPlayers())
        {
            if (!IsValid(Player) || Player->bIsPendingDelete)
            {
                continue;
            }
            Player->SetHoleIn(false);
            Player->PlayerInfo.bIsHoleout = false;
            if (FPlayerInfo* Info = FindPlayerInfoPtr(Player->SlotIndex))
            {
                Info->bIsHoleout = false;
            }
            else
            {
                UE_LOG(LogGameMode, Warning, TEXT("OnEnterHoleResults: missing GameInfo for SlotIndex %d. Re-adding from runtime."), Player->SlotIndex);
                if (FPlayerInfo* NewInfo = FindOrAddPlayerInfo(Player->PlayerInfo))
                {
                    NewInfo->bIsHoleout = false;
                }
            }
        }
    }

    // Range Mode는 이 함수에 도달하지 않아야 함
    if (IsRangeMode())
    {
        UE_LOG(LogGameMode, Warning, TEXT("?? Range Mode should not reach HoleResults state"));
        return;
    }

    HoleResults(); // 결과 표시 로직 수행

    if (IsTrainingMode())
    {
        UE_LOG(LogGameMode, Log, TEXT("?? Training Mode - additional training options available"));
    }


    if (CurrentHole > MapInfo.HoleCount)
    {
        StateMachine.ChangeState(EGameState::Game_Results, 2.f);
    }
    else
    {
        //for (FPlayerInfo PlayerInfo : GameInfo.Players)
        //{
        //	while (PlayerInfo.HoleScores.Num() < CurrentHole - 1)
        //	{
        //		PlayerInfo.HoleScores.Add(0);
        //	}
        //}

        //for (AGolfPlayer* Player : PlayerManager->GetPlayers())
        //{
        //	while (Player->PlayerInfo.HoleScores.Num() < CurrentHole - 1)
        //	{
        //		Player->PlayerInfo.HoleScores.Add(0);
        //	}
        //}

  //      SyncPlayerInfosToGameInfo();

        // 스코어보드 표시
        if (IsValid(InGameScoreBoardWidgetInstance) && IsStrokeMode())
        {
            SetShowScoreBoard(1);
        }
        //// ✅ 스코어보드 보여준 뒤 transition 위젯 재생 (4초 후)
        //if (UWorld* World = GetWorld())
        //{
        //    FTimerHandle TransitionTimer;
        //    World->GetTimerManager().SetTimer(TransitionTimer, [this]()
        //        {
        //            PlayHoleTransition();
        //        }, 4.f, false);
        //}
        //ChangeGameState(EGameState::Game_HoleInit, 0.f);

        if (UWorld* World = GetWorld())
        {
            FTimerHandle TransitionTimer;
            World->GetTimerManager().SetTimer(TransitionTimer, [this]()
                {
                    OnHoleTransitionFinished();
                }, 4.f, false);
        }
    }
}

void AInGameMode::OnEnterGameResults()
{
    UE_LOG(LogGameMode, Log, TEXT("-----Showing game results"));

    //USoundManager* SM = Cast<USoundManager>(GetGameInstance()->GetSubsystem<USoundManager>());
    //if (SM)
    //{
    //    SM->PlayTTS_Interrupt_ById("Voice.EndGame");
    //}

    if (bClickedEndGameButton)
    {
        ChangeGameState(EGameState::Game_End, 1.f);
        StrokeWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
        ResultWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
        ShotResultWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
    }
    else
    {
        if (IsStrokeMode())
        {
            SetShowScoreBoard(1);
            ChangeGameState(EGameState::Game_End, 4.f);
        }

    }
}

void AInGameMode::OnEnterGameEnd()
{
    UE_LOG(LogGameMode, Log, TEXT("-----Game ended"));

    // ? 라운드 종료 플래그 설정
    if (!bIsGameMenuEnd)
        GameInfo.bIsRoundEnd = true;

    SaveGameInfoToJSON();

    if (bClickedEndGameButton)
    {
        bClickedEndGameButton = false;
        GetCurrentTurnGolfBall()->SetBallState(EBallState::Ball_Des);
        StrokeWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
        ResultWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
        FTimerHandle TH;
        GetWorld()->GetTimerManager().SetTimer(TH, [this]()
            {
                FString FromInGameStr = "true";
                const FString Options = FString::Printf(TEXT("?game=/Game/UMG/GM_UMG.GM_UMG_C?bFromInGame=%s"), *FromInGameStr);
                UGameplayStatics::OpenLevel(GetWorld(), "Level_UI", false, Options);
            }, 3.0f, false);
    }
    else
    {
        if (IsValid(GameEndWidgetInstance))
        {
            SetShowScoreBoard(0);
            GameEndWidgetInstance->SetVisibility(ESlateVisibility::Visible);
            GameEndWidgetInstance->Init();
        }
    }
}

void AInGameMode::OnExitGamePlay()
{
    if (IsValid(InGameScoreBoardWidgetInstance))
    {
        SetShowScoreBoard(0);
    }

    // Cancel any pending "next turn" countdown/advance when leaving Game_Play.
    // Otherwise, delayed AdvanceTurn() from the previous hole can fire during the next hole
    // and cause duplicate Ready/turn progression.
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DelayedReadyTimer);
        World->GetTimerManager().ClearTimer(TurnCountdownTimer);
    }
    CurrentTurnCountdownTime = 0.0f;
    MaxTurnCountdownTime = 0.0f;
}

void AInGameMode::OnExitHoleOut()
{

}

// ? Stroke Mode 초기화
void AInGameMode::OnEnterGameInit_StrokeMode()
{
    UE_LOG(LogGameMode, Log, TEXT("??? Initializing Stroke Mode"));

    MaxHoleCount = FMath::Clamp((GameInfo.GameOptions.SelectCourse + 1) * 9, 9, 18);
    MapInfo.HoleCount = MaxHoleCount;

    InitTourActor();

    if (!CanResumeRound())
    {
        if (UTerraParkgameInstance* GI = Cast<UTerraParkgameInstance>(GetGameInstance()))
        {
            GI->StartHoleNum = GameInfo.GameOptions.SelectCourse == 1 ? 10 : 1;

            GameInfo.CurrentHole = GI->StartHoleNum;
            CurrentHole = GameInfo.CurrentHole;
            SaveGameInfoToJSON();
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("GI is null Need Set TerraParkGameInstance"));
        }
    }

    FSoftClassPath DropMarkerClass(TEXT("/Game/GolfGameBluePrint/BP_BallDropMarker.BP_BallDropMarker_C"));
    UClass* LoadedDropMakerClass = DropMarkerClass.TryLoadClass<ABallDropMarkerActor>();
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    DropMarker = GetWorld()->SpawnActor<ABallDropMarkerActor>(
        LoadedDropMakerClass,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        Params
    );
}

// ? Training Mode 초기화
void AInGameMode::OnEnterGameInit_TrainingMode()
{
    UE_LOG(LogGameMode, Log, TEXT("?? Initializing Training Mode"));

    InitTourActor();

    // Training Mode는 단일 플레이어만 지원
    if (GameInfo.Players.Num() > 1)
    {
        UE_LOG(LogGameMode, Warning, TEXT("?? Training Mode supports only single player. Using first player only."));

        // 첫 번째 플레이어만 유지
        FPlayerInfo FirstPlayer = GameInfo.Players[0];
        GameInfo.Players.Empty();
        GameInfo.Players.Add(FirstPlayer);
    }

    // ? FGameOptionInfo에 실제 존재하는 멤버들만 설정
    // GameInfo.GameOptions.bAllowBallMovement = true; // 이 줄 제거 - 존재하지 않는 멤버

    // Training Mode 특별 설정 (실제 존재하는 멤버들만 사용)
    if (GameInfo.GameOptions.GameType != 1) // Training Mode가 1이라고 가정
    {
        GameInfo.GameOptions.GameType = 1; // Training Mode로 설정
    }



    UE_LOG(LogGameMode, Log, TEXT("? Training Mode initialized for single player"));
}


// ? Range Mode 초기화 (레벨 전환)
void AInGameMode::OnEnterGameInit_RangeMode()
{
    UE_LOG(LogGameMode, Log, TEXT("????♂? Range Mode detected - transitioning to Range Level"));
    // TransitionToRangeLevel();
}


bool AInGameMode::ShouldTransitionToHoleOut() const
{
    if (!PlayerManager)
        return false;

    // Range Mode는 다른 전환 조건을 가질 수 있음 (하지만 현재 레벨에서는 실행되지 않음)
    if (IsRangeMode())
    {
        return false; // Range Mode는 현재 레벨에서 HoleOut으로 전환하지 않음
    }

    // Stroke Mode와 Training Mode는 동일한 조건
    return PlayerManager->IsHoleComplete(CurrentHole);
}

bool AInGameMode::ShouldTransitionToGameEnd() const
{
    // Range Mode는 다른 종료 조건을 가질 수 있음
    if (IsRangeMode())
    {
        return false; // Range Mode는 현재 레벨에서 게임 종료하지 않음
    }

    // Stroke Mode와 Training Mode는 동일한 조건
    return (CurrentHole > MapInfo.HoleCount);
}


bool AInGameMode::IsHoleInitializationComplete() const
{
    return bHoleInitialized && PlayerManager != nullptr;
}

void AInGameMode::InitializeGame()
{
    if (bGameInitialized)
        return;

    UE_LOG(LogGameMode, Log, TEXT("Initializing game for %s Mode..."),
        *UEnum::GetValueAsString(CurrentGameMode));

    // 게임 모드별 초기화
    if (LoadGameInfoFromJSON())
    {
        // 저장된.. 진행중인 홀을 선택한홀로 세팅
        if (IsTrainingMode())
        {

            if (GameInfo.CurrentHole != CurrentHole)
            {
                UE_LOG(LogGameMode, Log, TEXT("?? Synchronizing CurrentHole: GameInfo(%d) -> InGameMode(%d)"),
                    GameInfo.CurrentHole, CurrentHole);

                // 유효한 범위 내에서 동기화
                if (GameInfo.CurrentHole > 0 && GameInfo.CurrentHole <= GameInfo.SelectedMap.HoleCount)
                {
                    CurrentHole = GameInfo.CurrentHole;
                }
                else
                {
                    // GameInfo의 값이 유효하지 않으면 InGameMode 값을 사용
                    GameInfo.CurrentHole = CurrentHole;
                }

                UE_LOG(LogGameMode, Log, TEXT("? Final CurrentHole: %d"), CurrentHole);
            }
        }

        LoadMapInfoFromLevel();

        MapInfo = GameInfo.SelectedMap;

        // ? 새로 추가: 카메라 모드 옵션 로그 출력
        int32 CameraModeOption = GameInfo.GameOptions.Camera_Mode;
        UE_LOG(LogGameMode, Log, TEXT("?? Camera Mode Option loaded from JSON: %d (%s)"),
            CameraModeOption,
            (CameraModeOption == 1) ? TEXT("Fixed Camera") : TEXT("Auto Camera"));

        if (PlayerManager)
        {
            for (int32 i = 0; i < GameInfo.Players.Num(); i++)
            {
                if (GameInfo.Players[i].bIsPendingDelete)
                {
                    GameInfo.Players.RemoveAt(i);
                }
            }

            SaveGameInfoToJSON();

            PlayerManager->InitializePlayers(GameInfo.Players, GetWorld(), MapInfo, CurrentHole);
            BindBallEvents();
            bGameInitialized = true;
            UE_LOG(LogGameMode, Log, TEXT("Game initialization completed for %s Mode"),
                *UEnum::GetValueAsString(CurrentGameMode));
        }
        else
        {
            UE_LOG(LogGameMode, Error, TEXT("PlayerManager is null during initialization"));
        }
    }
    else
    {
        UE_LOG(LogGameMode, Warning, TEXT("Failed to load GameData.json, using default settings"));
        GameInfo = FGameInfo();

        // JSON 로드 실패 시 기본값으로 설정
        CurrentHole = 1;
    }

    InitializeGameByMode();


    BallParticleManager = NewObject<UBallParticleManager>(this);
    BallParticleManager->Init();

}


void AInGameMode::InitializeHole()
{
    bHoleInitialized = false;

    UE_LOG(LogGameMode, Log, TEXT("Initializing hole %d"), CurrentHole);

    bHoleInitialized = true;
}

void AInGameMode::StartHole()
{
    UE_LOG(LogGameMode, Log, TEXT("-----InGameMode  Starting hole %d"), CurrentHole);

    //if (LoadingScreenWidgetInstance)
    //{
    //   
    //    LoadingScreenWidgetInstance->SetVisibility(ESlateVisibility::Hidden);
    //    UE_LOG(LogTemp, Log, TEXT("? 로딩 화면 위젯 초기화 완료"));
    //    if (auto* SM = GetGameInstance()->GetSubsystem<USoundManager>())
    //    {
    //        SM->StopBGM(1);
    //    }
    //}
    if (CurrentGameMode == EGolfGameMode::StrokeMode)
    {
        //   ShowHoleMark(false);

        if (PlayerManager)
        {
            const bool bResumeContinue = bIsContinueGame;
            TArray<AGolfPlayer*> Players = PlayerManager->GetPlayers();
            for (AGolfPlayer* Player : Players)
            {
                if (Player)
                {
                    FPlayerInfo PlayerInfo = Player->GetPlayerInfo();
                    if (!bResumeContinue)
                    {
                        PlayerInfo.HoleCount = CurrentHole;
                        PlayerInfo.OnceHoleMulligan = false;
                        PlayerInfo.bIsHoleout = false;
                        Player->ResetShotCountForHole(CurrentHole);
                        Player->SetPlayerState(EPlayerState::Player_Des); // 초기 상태를 Player_Des로 설정
                        Player->SetPlayerInfo(PlayerInfo);
                        Player->UpdateBallPosition(MapInfo.TeePositions[CurrentHole - 1] + FVector(0.f, 0.f, 5.f));
                    }
                    else
                    {
                        PlayerInfo.HoleCount = CurrentHole;
                        Player->SetPlayerInfo(PlayerInfo);

                        const int32 HoleIdx = CurrentHole - 1;
                        const bool bHasShot = PlayerInfo.ShotCountPerHole.IsValidIndex(HoleIdx) && PlayerInfo.ShotCountPerHole[HoleIdx] > 0;

                        //if (bHasShot && !PlayerInfo.bIsHoleout)
                        //{
                        //    FindBall(Player->PlayerIndex)->SetBallForceHidden(false);
                        //    FindBall(Player->PlayerIndex)->SetBallVisibility(true, false);
                        //}
                    }
                    SyncPlayerInfosToGameInfo();
                }
            }

            PlayerManager->SortTeeShotPlayerOrder(CurrentHole, bIsContinueGame);

            if (!bIsContinueGame)
            {
                AGolfPlayerController* PC = Cast<AGolfPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
                if (PC->bTerrainGridVisible)
                    PC->ToggleTerrainGrid();

                if (PlayerManager->PlayerOrder.IsValidIndex(0))
                {
                    CurrentPlayerIndex = PlayerManager->PlayerOrder[0];
                    UE_LOG(LogGameMode, Log, TEXT("? Set CurrentPlayerIndex to %d"), CurrentPlayerIndex);
                }
                else
                {
                    CurrentPlayerIndex = 0;
                    UE_LOG(LogGameMode, Warning, TEXT("?? PlayerOrder is empty, defaulting to CurrentPlayerIndex=0"));
                }
            }
            else
            {
                CurrentPlayerIndex = GameInfo.CurrentPlayerIndex;
            }

            TArray<AGolfBall*> PlayerBalls = PlayerManager->GetPlayerBalls();

            for (AGolfBall* Ball : PlayerManager->GetPlayerBalls())
            {
                if (IsValid(Ball))
                {
                    int32 ShotCount = 0;
                    bool checkShot = true;
                    for (FPlayerInfo PlayerInfo : GameInfo.Players)
                    {
                        ShotCount += PlayerInfo.ShotCountPerHole[CurrentHole - 1];
                    }

                    if (ShotCount > 0)
                        checkShot = false;

                    if (checkShot)
                        Ball->SetActorLocation(MapInfo.TeePositions[CurrentHole - 1] + FVector(0, 0, 5.f));
                }
            }
            if (PlayerBalls.IsValidIndex(CurrentPlayerIndex))
            {
                PlayerBalls[CurrentPlayerIndex]->PrepareForTeeShot();
            }

            //InitializeOBLines();

            // ? 여기에서 미니맵 위젯을 생성하거나 (아직 없다면), 데이터를 갱신합니다.
            // CreateMiniMapWidget()을 호출하여 위젯 인스턴스를 확보
            if (!IsValid(MiniMapWidget)) // 아직 위젯 인스턴스가 없다면 새로 생성 시도
            {
                CreateMiniMapWidget();
            }

            // 미니맵 위젯이 유효하다면 데이터를 갱신합니다.
            if (IsValid(MiniMapWidget))
            {
                UpdateMiniMapForCurrentHole(); // 이 함수가 미니맵에 현재 홀 데이터를 전달합니다.
                ShowMiniMap(true);
                UE_LOG(LogGameMode, Log, TEXT("? MiniMap data updated and visibility set for Hole %d."), CurrentHole);
            }
            else
            {
                UE_LOG(LogGameMode, Error, TEXT("? MiniMapWidget is NULL after creation/check in StartHole."));
            }

            //  PlayerSlot update
            //UpdateAllPlayerInfoSlots();

            ApplyCameraModeOptionToCameraManager();

            UpdateHoleMarkPosition();

            UE_LOG(LogGameMode, Log, TEXT("Hole %d initialized. HolecupPos=%s"),
                CurrentHole,
                MapInfo.HolecupPositions.IsValidIndex(CurrentHole - 1) ?
                *MapInfo.HolecupPositions[CurrentHole - 1].ToString() : TEXT("Invalid"));

            // ? HoleMark 위치 업데이트 추가 (기존 코드 마지막 부분에)
        }
    }
    else  if (CurrentGameMode == EGolfGameMode::TrainingMode)
    {
        if (PlayerManager)
        {
            TArray<AGolfPlayer*> Players = PlayerManager->GetPlayers();
            for (AGolfPlayer* Player : Players)
            {
                if (Player)
                {
                    FPlayerInfo PlayerInfo = Player->GetPlayerInfo();
                    PlayerInfo.HoleCount = CurrentHole;
                    PlayerInfo.ShotCount = 0;
                    Player->ResetShotCountForHole(CurrentHole);
                    Player->SetPlayerState(EPlayerState::Player_Des); // 초기 상태를 Player_Des로 설정
                    Player->SetPlayerInfo(PlayerInfo);
                }
            }

            PlayerManager->SortTeeShotPlayerOrder(CurrentHole, bIsContinueGame);
            if (PlayerManager->PlayerOrder.IsValidIndex(0))
            {
                CurrentPlayerIndex = PlayerManager->PlayerOrder[0];
                UE_LOG(LogGameMode, Log, TEXT("? Set CurrentPlayerIndex to %d"), CurrentPlayerIndex);
            }
            else
            {
                CurrentPlayerIndex = 0;
                UE_LOG(LogGameMode, Warning, TEXT("?? PlayerOrder is empty, defaulting to CurrentPlayerIndex=0"));
            }

            TArray<AGolfBall*> PlayerBalls = PlayerManager->GetPlayerBalls();
            for (AGolfBall* Ball : PlayerManager->GetPlayerBalls())
            {
                if (IsValid(Ball))
                {
                    //if (CheckFirstShot())
                    Ball->SetActorLocation(MapInfo.TeePositions[CurrentHole - 1] + FVector(0, 0, 5.f));
                }
            }
            if (PlayerBalls.IsValidIndex(CurrentPlayerIndex))
            {
                PlayerBalls[CurrentPlayerIndex]->PrepareForTeeShot();
            }

            //InitializeOBLines();

            // ? 여기에서 미니맵 위젯을 생성하거나 (아직 없다면), 데이터를 갱신합니다.
            // CreateMiniMapWidget()을 호출하여 위젯 인스턴스를 확보
            if (!IsValid(MiniMapWidget)) // 아직 위젯 인스턴스가 없다면 새로 생성 시도
            {
                CreateMiniMapWidget();
            }

            // 미니맵 위젯이 유효하다면 데이터를 갱신합니다.
            if (IsValid(MiniMapWidget))
            {
                UpdateMiniMapForCurrentHole(); // 이 함수가 미니맵에 현재 홀 데이터를 전달합니다.
                ShowMiniMap(true);
                UE_LOG(LogGameMode, Log, TEXT("? MiniMap data updated and visibility set for Hole %d."), CurrentHole);
            }
            else
            {
                UE_LOG(LogGameMode, Error, TEXT("? MiniMapWidget is NULL after creation/check in StartHole."));
            }

            //  PlayerSlot update
            UpdateAllPlayerInfoSlots();

            ApplyCameraModeOptionToCameraManager();

            UpdateHoleMarkPosition();

            UE_LOG(LogGameMode, Log, TEXT("Hole %d initialized. HolecupPos=%s"),
                CurrentHole,
                MapInfo.HolecupPositions.IsValidIndex(CurrentHole - 1) ?
                *MapInfo.HolecupPositions[CurrentHole - 1].ToString() : TEXT("Invalid"));

            // ? HoleMark 위치 업데이트 추가 (기존 코드 마지막 부분에)
        }
    }

    else if (CurrentGameMode == EGolfGameMode::RangeMode)
    {
        if (PlayerManager)
        {
            TArray<AGolfPlayer*> Players = PlayerManager->GetPlayers();
            for (AGolfPlayer* Player : Players)
            {
                if (Player)
                {
                    FPlayerInfo PlayerInfo = Player->GetPlayerInfo();
                    PlayerInfo.HoleCount = CurrentHole;
                    Player->ResetShotCountForHole(CurrentHole);
                    Player->SetPlayerState(EPlayerState::Player_Des); // 초기 상태를 Player_Des로 설정
                    Player->SetPlayerInfo(PlayerInfo);
                }
            }

            PlayerManager->SortTeeShotPlayerOrder(CurrentHole, bIsContinueGame);
            if (PlayerManager->PlayerOrder.IsValidIndex(0))
            {
                CurrentPlayerIndex = PlayerManager->PlayerOrder[0];
                UE_LOG(LogGameMode, Log, TEXT("? Set CurrentPlayerIndex to %d"), CurrentPlayerIndex);
            }
            else
            {
                CurrentPlayerIndex = 0;
                UE_LOG(LogGameMode, Warning, TEXT("?? PlayerOrder is empty, defaulting to CurrentPlayerIndex=0"));
            }

            TArray<AGolfBall*> PlayerBalls = PlayerManager->GetPlayerBalls();
            for (AGolfBall* Ball : PlayerManager->GetPlayerBalls())
            {
                if (IsValid(Ball))
                {
                    Ball->SetActorLocation(MapInfo.TeePositions[CurrentHole - 1] + FVector(0.f, 0.f, 7.f));
                }
            }
            if (PlayerBalls.IsValidIndex(CurrentPlayerIndex))
            {
                PlayerBalls[CurrentPlayerIndex]->PrepareForTeeShot();
            }
        }

        // ? Range Mode에서도 카메라 옵션 적용
        ApplyCameraModeOptionToCameraManager();


        HandleOnChangedDrivingCheckBoxState();

        UE_LOG(LogGameMode, Log, TEXT("Hole %d initialized. HolecupPos=%s"),
            CurrentHole,
            MapInfo.HolecupPositions.IsValidIndex(CurrentHole - 1) ?
            *MapInfo.HolecupPositions[CurrentHole - 1].ToString() : TEXT("Invalid"));
    }
}

AGolfPlayer* AInGameMode::FindPlayer(int32 PlayerIndex)
{
    if (PlayerManager->GetPlayers().IsValidIndex(PlayerIndex))
    {
        return PlayerManager->GetPlayers()[PlayerIndex];
    }

    return nullptr;
}

AGolfPlayer* AInGameMode::FindPlayerSlotIndex(int32 SlotIndex)
{
    for (AGolfPlayer* Player : PlayerManager->GetPlayers())
    {
        if (Player->SlotIndex == SlotIndex)
            return Player;
    }

    return nullptr;
}

FPlayerInfo AInGameMode::FindPlayerInfo(int32 SlotIndex)
{
    FPlayerInfo FoundedPlayerInfo;
    for (int32 i = 0; i < GameInfo.Players.Num(); i++)
    {
        if (GameInfo.Players[i].SlotIndex == SlotIndex)
            FoundedPlayerInfo = GameInfo.Players[i];
    }

    return FoundedPlayerInfo;
}

FPlayerInfo* AInGameMode::FindPlayerInfoPtr(int32 SlotIndex)
{
    for (int32 i = 0; i < GameInfo.Players.Num(); i++)
    {
        if (GameInfo.Players[i].SlotIndex == SlotIndex)
            return &(GameInfo.Players[i]);
    }

    return nullptr;
}

FPlayerInfo* AInGameMode::FindOrAddPlayerInfo(const FPlayerInfo& PlayerInfo)
{
    if (FPlayerInfo* Existing = FindPlayerInfoPtr(PlayerInfo.SlotIndex))
    {
        return Existing;
    }

    GameInfo.Players.Add(PlayerInfo);
    return &GameInfo.Players.Last();
}

void AInGameMode::DeduplicatePlayerInfos()
{
    if (GameInfo.Players.Num() <= 1)
    {
        return;
    }

    TMap<int32, int32> SlotToIndex;
    TArray<FPlayerInfo> UniquePlayers;
    UniquePlayers.Reserve(GameInfo.Players.Num());

    bool bHadDupes = false;
    for (const FPlayerInfo& Info : GameInfo.Players)
    {
        if (int32* ExistingIndex = SlotToIndex.Find(Info.SlotIndex))
        {
            UniquePlayers[*ExistingIndex] = Info;
            bHadDupes = true;
        }
        else
        {
            SlotToIndex.Add(Info.SlotIndex, UniquePlayers.Num());
            UniquePlayers.Add(Info);
        }
    }

    if (bHadDupes)
    {
        UE_LOG(LogGameMode, Warning, TEXT("DeduplicatePlayerInfos: removed duplicate SlotIndex entries. Before=%d After=%d"),
            GameInfo.Players.Num(), UniquePlayers.Num());
        GameInfo.Players = MoveTemp(UniquePlayers);
    }
}

AGolfBall* AInGameMode::FindBall(int32 PlayerIndex)
{
    if (PlayerManager->GetPlayerBalls().IsValidIndex(PlayerIndex))
    {
        return PlayerManager->GetPlayerBalls()[PlayerIndex];
    }

    return nullptr;
}

AGolfBall* AInGameMode::FindBallSlotIndex(int32 SlotIndex)
{
    if (FindPlayerSlotIndex(SlotIndex))
    {
        return FindBall(FindPlayerSlotIndex(SlotIndex)->PlayerIndex);
    }

    return nullptr;
}

// ? 새로 추가: 카메라 모드 옵션을 설정하는 함수 (블루프린트에서 호출 가능)
void AInGameMode::SetCameraModeOption(int32 NewCameraModeOption)
{
    GameInfo.GameOptions.Camera_Mode = NewCameraModeOption;
    ApplyCameraModeOptionToCameraManager();
    SaveGameInfoToJSON(); // 설정을 JSON에 저장

    UE_LOG(LogGameMode, Log, TEXT("?? Camera Mode Option changed to: %d"), NewCameraModeOption);
}

// ? 새로 추가: 현재 카메라 모드 옵션 반환
int32 AInGameMode::GetCameraModeOption() const
{
    return GameInfo.GameOptions.Camera_Mode;
}


// ? 새로 추가: 카메라 매니저에 Camera_Mode 옵션을 적용하는 함수
void AInGameMode::ApplyCameraModeOptionToCameraManager()
{
    // 카메라 매니저 찾기
    TArray<AActor*> CameraManagers;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACameraManager::StaticClass(), CameraManagers);

    if (CameraManagers.Num() > 0)
    {
        ACameraManager* CameraManager = Cast<ACameraManager>(CameraManagers[0]);
        if (IsValid(CameraManager))
        {
            int32 CameraModeOption = GameInfo.GameOptions.Camera_Mode;
            CameraManager->SetCameraModeOption(CameraModeOption);

            UE_LOG(LogGameMode, Log, TEXT("?? Applied Camera_Mode option (%d) to CameraManager"), CameraModeOption);

            // 화면에 디버그 메시지 표시
            if (GEngine)
            {
                FString ModeText = (CameraModeOption == 1) ? TEXT("PARTIAL FIXED CAMERA") : TEXT("AUTO CAMERA");
                GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan,
                    FString::Printf(TEXT("?? %s MODE ACTIVATED"), *ModeText));
            }
        }
        else
        {
            UE_LOG(LogGameMode, Warning, TEXT("? CameraManager cast failed"));
        }
    }
    else
    {
        UE_LOG(LogGameMode, Warning, TEXT("? No CameraManager found in world"));
    }
}



void AInGameMode::ChangeSensorState(ESensorState NewState)
{
    UE_LOG(LogGameMode, Log, TEXT("Sensor state changed to %s"), *UEnum::GetValueAsString(NewState));
}

void AInGameMode::EndHole()
{
    UE_LOG(LogGameMode, Log, TEXT("Ending hole %d"), CurrentHole);

    //Algo::SortBy(GameInfo.Players, &FPlayerInfo::SlotIndex, TLess<int32>()); // 오름차순
    Algo::SortBy(PlayerManager->GetPlayerBalls(), &AGolfBall::OwningPlayerIndex, TLess<int32>());
    Algo::SortBy(PlayerManager->GetPlayers(), &AGolfPlayer::PlayerIndex, TLess<int32>());

    for (AGolfPlayer* Player : PlayerManager->GetPlayers())
    {
        if (Player->bIsPendingDelete)
        {
            Player->bIsPendingDelete = false;
            CurrentPlayerIndex = 0;
            PlayerManager->PlayerOrder.Remove(Player->PlayerIndex);
            PlayerManager->RemoveBallBySlotIndex(Player->PlayerInfo.SlotIndex);
            //GameInfo에서 제거
            PlayerManager->RemovePlayerBySlotIndex(Player->PlayerInfo.SlotIndex);
            PlayerManager->GetPlayers().Remove(Player);
        }
    }

    //재 인덱싱
    for (int32 i = 0; i < PlayerManager->GetPlayers().Num(); i++)
    {
        PlayerManager->GetPlayers()[i]->PlayerIndex = i;
    }
    for (int32 i = 0; i < PlayerManager->GetPlayerBalls().Num(); i++)
    {
        PlayerManager->GetPlayerBalls()[i]->OwningPlayerIndex = i;
    }
    for (int32 i = 0; i < PlayerInfoSlotWidgets.Num(); i++)
    {
        PlayerInfoSlotWidgets[i]->OwningPlayerIndex = i;
    }

    //SyncPlayerInfosToGameInfo();
    InGamePlayerSelectWidget->Init();
    SaveGameInfoToJSON();
}

void  AInGameMode::HoleResults()
{
    UE_LOG(LogGameMode, Log, TEXT("HoleResults current Hole %d"), CurrentHole);
    CurrentHole++;
    SaveGameInfoToJSON();
}

void AInGameMode::EndGame()
{
    UE_LOG(LogGameMode, Log, TEXT("Game ended"));

    // 모든 위젯이 뷰포트에서 제거되고 필요한 경우 루트 해제되도록 합니다.
    if (MiniMapWidget && MiniMapWidget->IsValidLowLevel())
    {
        MiniMapWidget->RemoveFromParent(); // AddToViewport로 추가되었다면 루트 해제됨
        MiniMapWidget = nullptr;
        UE_LOG(LogTemp, Log, TEXT("MiniMapWidget removed and nulled."));
    }
    if (StrokeWidgetInstance && StrokeWidgetInstance->IsValidLowLevel())
    {
        StrokeWidgetInstance->RemoveFromParent();
        StrokeWidgetInstance = nullptr;
        UE_LOG(LogTemp, Log, TEXT("StrokeWidgetInstance removed and nulled."));
    }
    if (InGameScoreBoardWidgetInstance && InGameScoreBoardWidgetInstance->IsValidLowLevel())
    {
        InGameScoreBoardWidgetInstance->RemoveFromParent();
        InGameScoreBoardWidgetInstance = nullptr;
        UE_LOG(LogTemp, Log, TEXT("InGameScoreBoardWidgetInstance removed and nulled."));
    }

    // CRITICAL: 명시적으로 AddToRoot()된 StrokeMenuWidgetInstance의 경우, 반드시 RemoveFromRoot()를 호출해야 합니다.
    if (StrokeMenuWidgetInstance && StrokeMenuWidgetInstance->IsValidLowLevel())
    {
        StrokeMenuWidgetInstance->RemoveFromRoot(); // 명시적으로 루트 해제
        StrokeMenuWidgetInstance = nullptr;
        UE_LOG(LogTemp, Log, TEXT("StrokeMenuWidgetInstance unrooted and nulled."));
    }

    // 플레이어 정보 슬롯도 정리합니다.
    for (UPlayerInfoSlotWidget* SlotWidget : PlayerInfoSlotWidgets)
    {
        if (SlotWidget && SlotWidget->IsValidLowLevel())
        {
            SlotWidget->RemoveFromParent();
        }
    }
    PlayerInfoSlotWidgets.Empty();

    if (IsRangeMode())
    {
        if (BallDistanceWidget)
        {
            BallDistanceWidget->ClearCustomTargetLocation();
        }
    }


    UE_LOG(LogTemp, Log, TEXT("PlayerInfoSlotWidgets cleared."));

    // Super 호출
   // Super::EndGame(); // 레벨 전환을 직접 처리하는 경우, 이 코드가 올바른 위치에서 호출되는지 확인하세요.
    // UGameplayStatics::OpenLevel(GetWorld(), FName("MainMenu")); // 새로운 레벨 로드/정리를 트리거합니다.
}



void AInGameMode::InitRangeMode()
{

}

// AInGameMode에서 BeginDestroy를 오버라이드합니다.
void AInGameMode::BeginDestroy()
{
    // EndGame에서 이미 처리되지 않았다면 정리 로직을 호출합니다 (예: 게임 충돌 또는 예상치 못한 종료 시).
    // 이것은 안전망입니다. EndGame이 안정적으로 호출된다면 일부 객체에 대해서는 중복될 수 있습니다.
    // 그러나 수동으로 루팅된 객체의 경우 여기에 RemoveFromRoot를 두는 것이 더 안전합니다.
    // 델리게이트 언바인드 추가
    if (AutoTeeController)
    {
        AutoTeeController->OnKeyPressed.RemoveAll(this);
        AutoTeeController->OnKeyReleased.RemoveAll(this);
        AutoTeeController->OnTeeHeightChanged.RemoveAll(this);
        AutoTeeController->OnConnectionStatusChanged.RemoveAll(this);
    }

    DisconnectAutoTeeDevice();

    // CRITICAL: 명시적으로 AddToRoot()된 StrokeMenuWidgetInstance의 경우
    if (StrokeMenuWidgetInstance && StrokeMenuWidgetInstance->IsValidLowLevel())
    {
        StrokeMenuWidgetInstance->RemoveFromRoot(); // 명시적으로 루트 해제
        StrokeMenuWidgetInstance = nullptr;
        UE_LOG(LogTemp, Log, TEXT("BeginDestroy: StrokeMenuWidgetInstance unrooted."));
    }

    // ? HoleMark 정리
    if (IsValid(HoleMarkBillboard))
    {
        HoleMarkBillboard->Destroy();
        HoleMarkBillboard = nullptr;
        UE_LOG(LogTemp, Log, TEXT("BeginDestroy: HoleMarkBillboard destroyed."));
    }

    // Super::BeginDestroy를 가장 마지막에 호출합니다.
    Super::BeginDestroy();
    UE_LOG(LogTemp, Log, TEXT("AInGameMode::BeginDestroy completed."));
}
// 월드에서  티,홀컵 위치정보 가지고온다..
// 연습장도 동일하게 세팅
void AInGameMode::LoadMapInfoFromLevel()
{
    if (bMapInfoLoaded)
        return;

    UE_LOG(LogGameMode, Log, TEXT("Loading map info from level actors..."));


    UE_LOG(LogGameMode, Log, TEXT("Loading map info from level actors..."));

    GameInfo.SelectedMap.TeePositions.Empty();
    GameInfo.SelectedMap.HolecupPositions.Empty();
    this->TeeRotationArray.Empty();

    int32 TargetSublevel = GameInfo.SelectedMap.Sublevel;

    if (UTerraParkgameInstance* GI = Cast<UTerraParkgameInstance>(GetGameInstance()))
    {
        if (CurrentGameMode == EGolfGameMode::StrokeMode || CurrentGameMode == EGolfGameMode::TrainingMode)
        {
            //int32 IS_CD = GameInfo.SelectedMap.Sublevel == 2 ? 18 : 0;
            if (GameInfo.GameOptions.Holecup_Position == 3)
            {
                GameInfo.GameOptions.Holecup_Position = FMath::RandRange(0, 2);
            }

            for (int32 i = 1; i <= 18; i++)
            {
                int32 PhysicalHoleNum = GetPhysicalHoleNum(i, TargetSublevel);

                FString TeeActorName = FString::Printf(TEXT("Tee_hole%d"), PhysicalHoleNum);
                AActor* TeeActor = FindActorByName(TeeActorName);
                if (TeeActor)
                {
                    FVector TeePosition = TeeActor->GetActorLocation();
                    FRotator TeeRotation = TeeActor->GetRootComponent()->GetRelativeRotation();
                    GameInfo.SelectedMap.TeePositions.Add(TeePosition);
                    this->TeeRotationArray.Add(TeeRotation);
                    UE_LOG(LogGameMode, Log, TEXT("Found Tee position for hole %d: %s"), PhysicalHoleNum, *TeePosition.ToString());
                }
                else
                {
                    FVector DefaultTeePos(0.0f, 0.0f, 0.0f);
                    GameInfo.SelectedMap.TeePositions.Add(DefaultTeePos);
                    this->TeeRotationArray.Add(FRotator::ZeroRotator);
                }

                // =====================================================================
// ⭐ green_hole 홀컵 위치 수집 (홀당 5개 액터 구조)
//
// BP 내부 컴포넌트 구조:
//   green_hole{N} (Actor)
//     └─ green   (SceneComponent) ← 홀컵 위치
//          ├─ RVT     (SceneComponent) ← 미선택 시 HiddenInGame=true
//          ├─ Col     (SceneComponent) ← 미선택 시 Z=-1000
//          └─ trigger (SceneComponent)
//
// 액터 번호: 1홀 → green_hole1~5, 2홀 → green_hole6~10, ...
// =====================================================================
                {
                    const int32 GreenBaseIdx = (PhysicalHoleNum - 1) * 5 + 1;

                    TArray<FVector> GreenPositions;
                    TArray<AActor*> GreenHoleActors;
                    GreenPositions.Init(FVector::ZeroVector, 5);
                    GreenHoleActors.Init(nullptr, 5);

                    bool bAnyGreenFound = false;

                    // ── 1단계: 5개 green_hole 위치 수집 ──────────────────────────────
                    for (int32 PosIdx = 0; PosIdx < 5; PosIdx++)
                    {
                        int32   GreenActorNum = GreenBaseIdx + PosIdx;
                        FString GreenActorName = FString::Printf(TEXT("green_hole%d"), GreenActorNum);
                        AActor* GreenHoleActor = FindActorByName(GreenActorName);

                        if (!IsValid(GreenHoleActor))
                        {
                            UE_LOG(LogGameMode, Warning,
                                TEXT("[GreenHole] hole%d pos[%d] 액터 없음: %s"),
                                PhysicalHoleNum, PosIdx, *GreenActorName);
                            continue;
                        }

                        GreenHoleActors[PosIdx] = GreenHoleActor;

                        // ── "green" SceneComponent 탐색 ──────────────────────────────
                        // 구조: RootComp 자체가 "green"이거나, 자식 중에 "green"이 있음
                        USceneComponent* GreenComp = nullptr;
                        USceneComponent* RootComp = GreenHoleActor->GetRootComponent();

                        if (IsValid(RootComp))
                        {
                            if (RootComp->GetName().Equals(TEXT("green"), ESearchCase::IgnoreCase))
                            {
                                GreenComp = RootComp;
                            }
                            else
                            {
                                TArray<USceneComponent*> AllSC;
                                RootComp->GetChildrenComponents(true, AllSC);
                                for (USceneComponent* SC : AllSC)
                                {
                                    if (SC->GetName().Equals(TEXT("green"), ESearchCase::IgnoreCase))
                                    {
                                        GreenComp = SC;
                                        break;
                                    }
                                }
                            }
                        }

                        FVector GreenPos = FVector::ZeroVector;
                        if (IsValid(GreenComp))
                        {
                            GreenPos = GreenComp->GetComponentLocation();
                            UE_LOG(LogGameMode, Log,
                                TEXT("[GreenHole] hole%d pos[%d] green 컴포넌트 위치: %s"),
                                PhysicalHoleNum, PosIdx, *GreenPos.ToString());
                        }
                        else
                        {
                            // green 컴포넌트 못 찾으면 액터 위치 폴백
                            GreenPos = GreenHoleActor->GetActorLocation();
                            UE_LOG(LogGameMode, Warning,
                                TEXT("[GreenHole] hole%d pos[%d] green 컴포넌트 없음 → 액터위치 폴백: %s"),
                                PhysicalHoleNum, PosIdx, *GreenPos.ToString());
                        }

                        if (!GreenPos.IsZero())
                        {
                            GreenPositions[PosIdx] = GreenPos;
                            bAnyGreenFound = true;
                        }
                    }

                    // ── 2단계: 선택 인덱스 결정 ──────────────────────────────────────
                    if (bAnyGreenFound)
                    {
                        int32   SelectedIdx = FMath::Clamp(GameInfo.GameOptions.Holecup_Position, 0, 4);
                        FVector SelectedPos = GreenPositions[SelectedIdx];

                        if (SelectedPos.IsZero())
                        {
                            for (int32 k = 0; k < 5; k++)
                            {
                                if (!GreenPositions[k].IsZero())
                                {
                                    SelectedPos = GreenPositions[k];
                                    SelectedIdx = k;
                                    UE_LOG(LogGameMode, Warning,
                                        TEXT("[GreenHole] hole%d 선택위치 Zero → [%d] 대체"),
                                        PhysicalHoleNum, k);
                                    break;
                                }
                            }
                        }

                        if (!SelectedPos.IsZero())
                        {
                            // ── 3단계: 미선택 4개 → Col Z=-1000 / RVT HiddenInGame ──
                            for (int32 PosIdx = 0; PosIdx < 5; PosIdx++)
                            {
                                if (PosIdx == SelectedIdx) continue;

                                AActor* GreenHoleActor = GreenHoleActors[PosIdx];
                                if (!IsValid(GreenHoleActor)) continue;

                                // green 컴포넌트 재탐색
                                USceneComponent* GreenComp = nullptr;
                                USceneComponent* RootComp = GreenHoleActor->GetRootComponent();
                                if (IsValid(RootComp))
                                {
                                    if (RootComp->GetName().Equals(TEXT("green"), ESearchCase::IgnoreCase))
                                    {
                                        GreenComp = RootComp;
                                    }
                                    else
                                    {
                                        TArray<USceneComponent*> AllSC;
                                        RootComp->GetChildrenComponents(true, AllSC);
                                        for (USceneComponent* SC : AllSC)
                                        {
                                            if (SC->GetName().Equals(TEXT("green"), ESearchCase::IgnoreCase))
                                            {
                                                GreenComp = SC;
                                                break;
                                            }
                                        }
                                    }
                                }

                                if (!IsValid(GreenComp)) continue;

                                // green 직계 자식에서 Col / RVT 처리
                                TArray<USceneComponent*> GreenChildren;
                                GreenComp->GetChildrenComponents(false, GreenChildren);

                                for (USceneComponent* Child : GreenChildren)
                                {
                                    if (!IsValid(Child)) continue;
                                    FString ChildName = Child->GetName();

                                    // Col: Z = -1000 (월드 좌표 절대값으로 설정)
                                    if (ChildName.Equals(TEXT("Col"), ESearchCase::IgnoreCase))
                                    {
                                        FVector WorldLoc = Child->GetComponentLocation();
                                        WorldLoc.Z = WorldLoc.Z -100.0f;
                                        Child->SetWorldLocation(WorldLoc);
                                        UE_LOG(LogGameMode, Log,
                                            TEXT("[GreenHole] hole%d pos[%d] Col Z=-1000 적용: %s"),
                                            PhysicalHoleNum, PosIdx, *WorldLoc.ToString());
                                    }
                                    // RVT: 렌더링에서 숨김
                                    else if (ChildName.StartsWith(TEXT("RVT"), ESearchCase::IgnoreCase))
                                    {
                                        Child->SetVisibility(false, true);  // 자식까지 전파
                                        Child->SetHiddenInGame(true, true);
                                        UE_LOG(LogGameMode, Log,
                                            TEXT("[GreenHole] hole%d pos[%d] RVT HiddenInGame=true: '%s'"),
                                            PhysicalHoleNum, PosIdx, *ChildName);
                                    }
                                }
                            }

                            // ── Flag 이동 ─────────────────────────────────────────────
                            FString FlagActorName = FString::Printf(TEXT("flag_hole%d"), PhysicalHoleNum);
                            AActor* FlagActor = FindActorByName(FlagActorName);
                            if (IsValid(FlagActor))
                            {
                                FlagActor->SetActorLocation(SelectedPos);
                            }

                            GameInfo.SelectedMap.HolecupPositions.Add(SelectedPos);
                            UE_LOG(LogGameMode, Log,
                                TEXT("[GreenHole] hole%d HolecupPositions 추가 = %s (posIdx=%d)"),
                                PhysicalHoleNum, *SelectedPos.ToString(), SelectedIdx);

                            if (i > GameInfo.SelectedMap.ParScores.Num())
                            {
                                float DefaultPar = 3.0f + (i % 3);
                                GameInfo.SelectedMap.ParScores.Add(DefaultPar);
                            }

                            continue; // ⭐ 성공 → Cup_hole 폴백 스킵
                        }
                    }

                    UE_LOG(LogGameMode, Warning,
                        TEXT("[GreenHole] hole%d 전체 green_hole 수집 실패 → Cup_hole 폴백"),
                        PhysicalHoleNum);
                }
                // =====================================================================
                // green_hole 실패 시 기존 Cup_hole 폴백
                // =====================================================================


                if (i > GameInfo.SelectedMap.ParScores.Num())
                {
                    float DefaultPar = 3.0f + (i % 3);
                    GameInfo.SelectedMap.ParScores.Add(DefaultPar);
                }
            }

        }
        else if (CurrentGameMode == EGolfGameMode::RangeMode)
        {
            FString PutStartActorName = FString::Printf(TEXT("put_startpoint"));
            AActor* PutStartActor = FindActorByName(PutStartActorName);
            PracticePuttingModeStartPoint = PutStartActor;

            FString PutEndActorName = FString::Printf(TEXT("holecup"));
            AActor* PutEndActor = FindActorByName(PutEndActorName);
            PracticePuttingModeEndPoint = PutEndActor;

            //========//

            GameInfo.SelectedMap.Sublevel = 0;
            FString TeeActorName = FString::Printf(TEXT("startpoint"));
            AActor* TeeActor = FindActorByName(TeeActorName);
            PracticeModeStartPoint = TeeActor;

            FString CupActorName = FString::Printf(TEXT("endpoint"));
            AActor* CupActor = FindActorByName(CupActorName);
            PracticeModeEndPoint = CupActor;

            for (int32 i = 1; i <= GameInfo.SelectedMap.HoleCount; i++)
            {
                if (TeeActor)
                {
                    FVector TeePosition = TeeActor->GetActorLocation();
                    FRotator TeeRotation = TeeActor->GetRootComponent()->GetRelativeRotation();
                    GameInfo.SelectedMap.TeePositions.Add(TeePosition);
                    this->TeeRotationArray.Add(TeeRotation);
                    UE_LOG(LogGameMode, Verbose, TEXT("Found Tee position for hole %d: %s"), i, *TeePosition.ToString());
                }
                else
                {
                    FVector DefaultTeePos(0.0f, 0.0f, 0.0f);
                    GameInfo.SelectedMap.TeePositions.Add(DefaultTeePos);
                }

                if (CupActor)
                {
                    FVector CupPosition = CupActor->GetActorLocation();
                    GameInfo.SelectedMap.HolecupPositions.Add(CupPosition);
                    UE_LOG(LogGameMode, Verbose, TEXT("Found Cup position for hole %d: %s"), i, *CupPosition.ToString());
                }
                else
                {
                    FVector DefaultCupPos(100.0f * i, 0.0f, 0.0f);
                    GameInfo.SelectedMap.HolecupPositions.Add(DefaultCupPos);
                    UE_LOG(LogGameMode, Warning, TEXT("Cup actor not found for hole %d. Using default position."), i);
                }

                if (i > GameInfo.SelectedMap.ParScores.Num())
                {
                    float DefaultPar = 3.0f + (i % 3);
                    GameInfo.SelectedMap.ParScores.Add(DefaultPar);
                }
            }
            if (IsValid(BP_Target) && IsValid(PracticeModeStartPoint))
            {
                BP_Target->SetActorLocation(PracticeModeStartPoint->GetActorLocation());

                if (IsValid(PracticeModeEndPoint))
                {
                    UUtilLibrary::MoveActorTowardActorByDistanceSimple_KeepRotation(BP_Target, PracticeModeEndPoint, 8000.f, true);
                }
                else
                {
                    UE_LOG(LogGameMode, Warning, TEXT("LoadMapInfoFromLevel: PracticeModeEndPoint(endpoint) is null"));
                }
            }
            else
            {
                UE_LOG(LogGameMode, Error, TEXT("LoadMapInfoFromLevel: BP_Target(%s) or PracticeModeStartPoint(%s) is null"),
                    IsValid(BP_Target) ? TEXT("OK") : TEXT("NULL"),
                    IsValid(PracticeModeStartPoint) ? TEXT("OK") : TEXT("NULL"));
            }
        }
    }

    UE_LOG(LogGameMode, Log, TEXT("Loaded %d tee positions and %d cup positions"),
        GameInfo.SelectedMap.TeePositions.Num(),
        GameInfo.SelectedMap.HolecupPositions.Num());

    bMapInfoLoaded = true;


    //if (GameInfo.Complate)
    //{
    //    GameInfo.Complate = false;
    //}

    SaveGameInfoToJSON();
}

// =============================================================================
// 공통 헬퍼: 패키지 경로에서 순수 레벨 이름 추출
//   PIE:      "/Game/Seobong/ob_level/UEDPIE_0_ob_hole1" → "ob_hole1"
//   Shipping: "/Game/Seobong/ob_level/ob_hole1"          → "ob_hole1"
//   숫자 접두사(UEDPIE_N_)는 N이 0 이상의 숫자여도 모두 제거
// =============================================================================
static FString GetCleanLevelShortName(const FString& PackageName)
{
    // 슬래시 기준 마지막 세그먼트 추출
    FString ShortName;
    if (!PackageName.Split(TEXT("/"), nullptr, &ShortName,
        ESearchCase::IgnoreCase, ESearchDir::FromEnd))
    {
        ShortName = PackageName;
    }

    // "UEDPIE_숫자_" 패턴 제거 (예: UEDPIE_0_, UEDPIE_1_, UEDPIE_12_)
    // 정규식 없이 수동 파싱
    if (ShortName.StartsWith(TEXT("UEDPIE_"), ESearchCase::IgnoreCase))
    {
        // "UEDPIE_" 이후 숫자 + "_" 찾아서 제거
        int32 UnderscorePos = ShortName.Find(TEXT("_"), ESearchCase::IgnoreCase,
            ESearchDir::FromStart, 7); // "UEDPIE_" 길이=7 이후부터 탐색
        if (UnderscorePos != INDEX_NONE)
        {
            ShortName = ShortName.Mid(UnderscorePos + 1);
        }
    }

    return ShortName;
}

// =============================================================================
// 공통 헬퍼: 순수 레벨 이름으로 ULevelStreaming* 찾기 (PIE/Shipping 공통)
// =============================================================================
static ULevelStreaming* FindOBStreamingLevel(UWorld* World, const FString& CleanLevelName)
{
    if (!World) return nullptr;
    for (ULevelStreaming* SL : World->GetStreamingLevels())
    {
        if (!SL) continue;
        FString Clean = GetCleanLevelShortName(SL->GetWorldAssetPackageName());
        if (Clean.Equals(CleanLevelName, ESearchCase::IgnoreCase))
            return SL;
    }
    return nullptr;
}

void AInGameMode::InitializeOBLines()
{
    UE_LOG(LogGameMode, Log, TEXT("InitializeOBLines() ..."));
    MapInfo.OBLines.SetNum(FMath::Max(MapInfo.OBLines.Num(), CurrentHole), EAllowShrinking::No);

    int32 TargetSublevel = MapInfo.Sublevel;
    int32 PhysicalHoleNum = GetPhysicalHoleNum(CurrentHole, TargetSublevel);
    UWorld* World = GetWorld();
    if (!World) return;

    // =========================================================================
    // ① 전체 ob_hole 서브레벨 숨김 (Visible=false, 로드는 유지)
    // =========================================================================
    for (ULevelStreaming* SL : World->GetStreamingLevels())
    {
        if (!SL) continue;
        FString CleanName = GetCleanLevelShortName(SL->GetWorldAssetPackageName());
        if (CleanName.StartsWith(TEXT("ob_hole"), ESearchCase::IgnoreCase))
        {
            SL->SetShouldBeVisible(false);
            UE_LOG(LogGameMode, Verbose,
                TEXT("[OBLines] 숨김: %s"), *CleanName);
        }
    }

    // =========================================================================
    // ② 현재 홀 ob_hole 서브레벨 활성화
    // =========================================================================
    FString TargetLevelName = FString::Printf(TEXT("ob_hole%d"), PhysicalHoleNum);
    ULevelStreaming* CurrentOBLevel = FindOBStreamingLevel(World, TargetLevelName);

    if (!CurrentOBLevel)
    {
        UE_LOG(LogGameMode, Warning,
            TEXT("[OBLines] 서브레벨 찾기 실패: '%s'"), *TargetLevelName);
        return;
    }

    CurrentOBLevel->SetShouldBeLoaded(true);
    CurrentOBLevel->SetShouldBeVisible(true);
    UE_LOG(LogGameMode, Log,
        TEXT("[OBLines] 활성화 요청: '%s' (Loaded=%d Visible=%d)"),
        *TargetLevelName,
        CurrentOBLevel->IsLevelLoaded() ? 1 : 0,
        CurrentOBLevel->IsLevelVisible() ? 1 : 0);

    // =========================================================================
    // ③ 로드 완료 확인 — 비동기이므로 타이머 폴링
    // =========================================================================
    if (!CurrentOBLevel->IsLevelLoaded() || !CurrentOBLevel->IsLevelVisible())
    {
        UE_LOG(LogGameMode, Warning,
            TEXT("[OBLines] '%s' 로드 대기 중 — 0.1초 폴링 시작"), *TargetLevelName);

        if (!bOBLevelWaiting)
        {
            bOBLevelWaiting = true;

            GetWorldTimerManager().SetTimer(
                OBLevelWaitTimer,
                [this]()
                {
                    int32   PhysHole = GetPhysicalHoleNum(CurrentHole, MapInfo.Sublevel);
                    FString LvName = FString::Printf(TEXT("ob_hole%d"), PhysHole);
                    ULevelStreaming* SL = FindOBStreamingLevel(GetWorld(), LvName);

                    if (SL && SL->IsLevelLoaded() && SL->IsLevelVisible())
                    {
                        GetWorldTimerManager().ClearTimer(OBLevelWaitTimer);
                        bOBLevelWaiting = false;
                        UE_LOG(LogGameMode, Log,
                            TEXT("[OBLines] '%s' 로드 완료 → OBLines 수집"), *LvName);
                        CollectOBLinesFromCurrentLevel();
                    }
                },
                0.1f, true
            );
        }
        return;
    }

    // 이미 로드 완료 → 즉시 수집
    CollectOBLinesFromCurrentLevel();
}

// =============================================================================
// OB 포인트 수집 전용 함수
// =============================================================================
void AInGameMode::CollectOBLinesFromCurrentLevel()
{
    int32 TargetSublevel = MapInfo.Sublevel;
    int32 PhysicalHoleNum = GetPhysicalHoleNum(CurrentHole, TargetSublevel);
    UWorld* World = GetWorld();
    if (!World) return;

    // =========================================================================
    // ④ ULevel* 획득 — UEDPIE 접두사 제거 후 비교
    // =========================================================================
    FString TargetLevelName = FString::Printf(TEXT("ob_hole%d"), PhysicalHoleNum);
    ULevel* TargetLevel = nullptr;

    for (ULevelStreaming* SL : World->GetStreamingLevels())
    {
        if (!SL) continue;
        FString CleanName = GetCleanLevelShortName(SL->GetWorldAssetPackageName());
        if (CleanName.Equals(TargetLevelName, ESearchCase::IgnoreCase))
        {
            TargetLevel = SL->GetLoadedLevel();
            UE_LOG(LogGameMode, Log,
                TEXT("[OBLines] ULevel* 획득: '%s' (Level=%s)"),
                *TargetLevelName,
                TargetLevel ? TEXT("OK") : TEXT("nullptr"));
            break;
        }
    }

    if (!TargetLevel)
    {
        UE_LOG(LogGameMode, Error,
            TEXT("[OBLines] ULevel* 획득 실패: '%s'"), *TargetLevelName);
        return;
    }

    // =========================================================================
    // ⑤ 같은 서브레벨 소속 mal_pack 액터 수집 + 정렬
    // =========================================================================
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);

    TArray<AActor*> MalPackActors;
    for (AActor* Actor : AllActors)
    {
        if (!Actor) continue;
        if (Actor->GetLevel() != TargetLevel) continue;

        FString stActorLabel = Actor->GetActorNameOrLabel();
        FString ActorName = Actor->GetName();

        if (stActorLabel.StartsWith(TEXT("mal_pack"), ESearchCase::IgnoreCase) ||
            ActorName.StartsWith(TEXT("mal_pack"), ESearchCase::IgnoreCase))
        {
            MalPackActors.Add(Actor);
        }
    }

    // 숫자 기준 오름차순 정렬
    // 사전식: mal_pack1, mal_pack10, mal_pack11, mal_pack2 (❌)
    // 숫자식: mal_pack1, mal_pack2, mal_pack10, mal_pack11 (✅)
// 숫자 기준 오름차순 자연어 정렬 (Natural Sort)
// 예: mal_pack11-1, mal_pack11-2, mal_pack12 순으로 정렬
    MalPackActors.Sort([](const AActor& A, const AActor& B)
        {
            FString StrA = A.GetActorNameOrLabel();
            FString StrB = B.GetActorNameOrLabel();

            int32 IdxA = 0;
            int32 IdxB = 0;

            while (IdxA < StrA.Len() && IdxB < StrB.Len())
            {
                // 양쪽 문자열에서 모두 숫자가 시작되는 구간인지 확인
                if (FChar::IsDigit(StrA[IdxA]) && FChar::IsDigit(StrB[IdxB]))
                {
                    // A에서 연속된 숫자 구간 추출 및 값 파싱
                    int32 StartA = IdxA;
                    while (IdxA < StrA.Len() && FChar::IsDigit(StrA[IdxA])) { IdxA++; }
                    int32 NumA = FCString::Atoi(*StrA.Mid(StartA, IdxA - StartA));

                    // B에서 연속된 숫자 구간 추출 및 값 파싱
                    int32 StartB = IdxB;
                    while (IdxB < StrB.Len() && FChar::IsDigit(StrB[IdxB])) { IdxB++; }
                    int32 NumB = FCString::Atoi(*StrB.Mid(StartB, IdxB - StartB));

                    // 두 숫자가 다르면 숫자 크기로 비교 결과 반환
                    if (NumA != NumB)
                    {
                        return NumA < NumB;
                    }

                    // 숫자가 같다면 뒤에 붙은 하이픈(-)이나 다음 숫자를 비교하기 위해 계속 루프 진행
                    continue;
                }

                // 숫자가 아닌 일반 문자 구간은 문자 자체를 비교
                if (StrA[IdxA] != StrB[IdxB])
                {
                    return StrA[IdxA] < StrB[IdxB];
                }

                IdxA++;
                IdxB++;
            }

            // 루프가 끝났는데도 같다면 더 짧은 문자열(예: 'mal_pack11'이 'mal_pack11-1'보다 먼저)을 앞으로 보냄
            return StrA.Len() < StrB.Len();
        });

    UE_LOG(LogGameMode, Log,
        TEXT("[OBLines] ob_hole%d: mal_pack %d개 수집"),
        PhysicalHoleNum, MalPackActors.Num());

    // =========================================================================
    // ⑥ mal_pack 월드 위치 → OBLines.Points 저장
    // =========================================================================
    TArray<FVector> OBPoints;
    for (AActor* MalPack : MalPackActors)
    {
        if (!MalPack) continue;
        FVector Pos = MalPack->GetActorLocation();
        OBPoints.Add(Pos);
        UE_LOG(LogGameMode, Verbose,
            TEXT("[OBLines]   '%s' → %s"),
            *MalPack->GetName() , *Pos.ToString());
    }

    // =========================================================================
    // ⑦ mal_pack 없으면 SplineComponent 폴백 (레거시 대응)
    // =========================================================================
    if (OBPoints.Num() == 0)
    {
        UE_LOG(LogGameMode, Warning,
            TEXT("[OBLines] mal_pack 없음 — SplineComponent 폴백"));

        for (AActor* Actor : AllActors)
        {
            if (!Actor || Actor->GetLevel() != TargetLevel) continue;
            if (USplineComponent* SC = Actor->FindComponentByClass<USplineComponent>())
            {
                for (int32 i = 0; i < SC->GetNumberOfSplinePoints(); i++)
                    OBPoints.Add(SC->GetWorldLocationAtSplinePoint(i));

                UE_LOG(LogGameMode, Log,
                    TEXT("[OBLines] Spline 폴백: %d points"), OBPoints.Num());
                break;
            }
        }
    }

    MapInfo.OBLines[CurrentHole - 1].Points = OBPoints;

    UE_LOG(LogGameMode, Log,
        TEXT("[OBLines] ✅ ob_hole%d: OBLines[%d] = %d points 저장 완료"),
        PhysicalHoleNum, CurrentHole - 1, OBPoints.Num());

    // ✅ 추가: 미니맵에 즉시 반영
    if (IsValid(MiniMapWidget) && OBPoints.Num() >= 3)
    {
        MiniMapWidget->UpdateOBLines(OBPoints);
        UE_LOG(LogGameMode, Log,
            TEXT("[OBLines] ✅ MiniMap OB 라인 업데이트: %d points"), OBPoints.Num());
    }
    else if (!IsValid(MiniMapWidget))
    {
        UE_LOG(LogGameMode, Warning,
            TEXT("[OBLines] ⚠️ MiniMapWidget 없음 — OB 라인 미니맵 반영 스킵"));
    }

    UE_LOG(LogGameMode, Log,
        TEXT("[OBLines] ✅ ob_hole%d: OBLines[%d] = %d points 저장 완료"),
        PhysicalHoleNum, CurrentHole - 1, OBPoints.Num());
}



AActor* AInGameMode::FindActorByName(const FString& InActorName)
{
    SCOPE_CYCLE_COUNTER(STAT_InGameModeFindActor);

    UWorld* World = GetWorld();
    if (!World) return nullptr;

    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), FoundActors);

    const FString SuffixPattern = InActorName + TEXT("_");

    // ── 1순위: Tags 정확 일치 (가장 신뢰도 높음) ─────────────────────────
    for (AActor* Actor : FoundActors)
    {
        if (!Actor) continue;
        for (const FName& Tag : Actor->Tags)
        {
            FString TagStr = Tag.ToString();
            if (TagStr.Equals(InActorName, ESearchCase::IgnoreCase) ||
                TagStr.StartsWith(SuffixPattern, ESearchCase::IgnoreCase))
            {
                UE_LOG(LogGameMode, Log,
                    TEXT("[FindActor] ✅ Tags 매칭: '%s' → '%s'"),
                    *InActorName, *Actor->GetName());
                return Actor;
            }
        }
    }

    // ── 2순위: GetName() 정확 일치 (일반 AActor) ─────────────────────────
    for (AActor* Actor : FoundActors)
    {
        if (!Actor) continue;
        const FString ActorName = Actor->GetActorNameOrLabel();
        if (ActorName.Equals(InActorName, ESearchCase::IgnoreCase) ||
            ActorName.StartsWith(SuffixPattern, ESearchCase::IgnoreCase))
        {
            UE_LOG(LogGameMode, Log,
                TEXT("[FindActor] ✅ NameOrLabel 매칭: '%s' → '%s'"),
                *InActorName, *ActorName);
            return Actor;
        }
    }

    // ── 3순위: GetActorLabel() + Blueprint 클래스 타입 검증 ──────────────
    // Label은 중복될 수 있으므로 반드시 Blueprint 계열 액터인지 확인
#if WITH_EDITOR
    for (AActor* Actor : FoundActors)
    {
        if (!Actor) continue;

        const FString stActorLabel = Actor->GetActorLabel();
        if (!stActorLabel.Equals(InActorName, ESearchCase::IgnoreCase) &&
            !stActorLabel.StartsWith(SuffixPattern, ESearchCase::IgnoreCase))
        {
            continue; // 라벨 불일치 → 스킵
        }

        // ⭐ 핵심: StaticMeshActor 등 비BP 액터 제외
        // Blueprint 액터는 클래스 이름이 "BP_" 또는 "R" 접두사이거나
        // GetClass()->GetName()이 "_C"로 끝남 (UE Blueprint 컴파일 규칙)
        const FString ClassName = Actor->GetClass()->GetName();
        const bool bIsBlueprint = ClassName.EndsWith(TEXT("_C"));
        const bool bIsStaticMesh = Actor->IsA<AStaticMeshActor>();

        if (bIsStaticMesh)
        {
            // StaticMeshActor는 라벨이 같아도 제외
            UE_LOG(LogGameMode, Warning,
                TEXT("[FindActor] ⛔ StaticMeshActor 제외 (라벨만 같음): Label='%s' Name='%s'"),
                *stActorLabel, *Actor->GetName());
            continue;
        }

        if (bIsBlueprint)
        {
            UE_LOG(LogGameMode, Warning,
                TEXT("[FindActor] ⚠️ Label+BP 매칭 (Tags 미설정): '%s' → '%s' (Class: %s)  ← Tags 추가 권장!"),
                *InActorName, *Actor->GetName(), *ClassName);
            return Actor;
        }
    }
#endif

    UE_LOG(LogGameMode, Error,
        TEXT("[FindActor] ❌ Failed to FindActorByName: '%s'"), *InActorName);
    return nullptr;
}


bool AInGameMode::LoadGameInfoFromJSON()
{
    bool bSuccess = UJsonHandler::LoadGameInfoFromJson(GameInfo, FPaths::ProjectSavedDir() + TEXT("GameData.json"));
    if (!bSuccess)
    {
        UE_LOG(LogGameMode, Warning, TEXT("Failed to load GameData.json, using default settings"));
        GameInfo = FGameInfo();

        // JSON 로드 실패 시 기본값으로 설정
        CurrentHole = 1;
    }
    else
    {
        UE_LOG(LogGameMode, Log, TEXT("Game data loaded successfully"));
        DeduplicatePlayerInfos();

        // ? JSON에서 로드한 CurrentHole 값을 InGameMode의 CurrentHole에 적용
        if (GameInfo.CurrentHole > 0)
        {
            CurrentHole = GameInfo.CurrentHole;
            UE_LOG(LogGameMode, Log, TEXT("? CurrentHole loaded from JSON: %d"), CurrentHole);
        }
        else
        {
            // GameInfo.CurrentHole이 유효하지 않으면 기본값 사용
            CurrentHole = 1;
            GameInfo.CurrentHole = CurrentHole; // GameInfo도 동기화
            UE_LOG(LogGameMode, Warning, TEXT("?? Invalid CurrentHole in JSON, using default: %d"), CurrentHole);
        }
        // Clamp loaded CurrentPlayerIndex to the loaded player count to avoid OOB crashes on resume.
        if (GameInfo.Players.Num() > 0)
        {
            CurrentPlayerIndex = FMath::Clamp(GameInfo.CurrentPlayerIndex, 0, GameInfo.Players.Num() - 1);
        }
        else
        {
            CurrentPlayerIndex = 0;
        }
    }
    return bSuccess;
}

void AInGameMode::SaveGameInfoToJSON()
{
    // ? 저장하기 전에 CurrentHole 동기화 확인
    if (GameInfo.CurrentHole != CurrentHole)
    {
        UE_LOG(LogGameMode, Log, TEXT("?? Syncing CurrentHole before save: %d -> %d"),
            GameInfo.CurrentHole, CurrentHole);
        GameInfo.CurrentHole = CurrentHole;
    }

    GameInfo.CurrentHole = CurrentHole;
    GameInfo.CurrentPlayerIndex = CurrentPlayerIndex;
    DeduplicatePlayerInfos();


    bool bSuccess = UJsonHandler::SaveGameInfoToJson(GameInfo, FPaths::ProjectSavedDir() + TEXT("GameData.json"));
    if (!bSuccess)
    {
        UE_LOG(LogGameMode, Error, TEXT("Failed to save GameData.json"));
    }
    else
    {
        UE_LOG(LogGameMode, Log, TEXT("Game data saved successfully"));
    }
}

void AInGameMode::UpdateMiniMapForCurrentHole()
{
    if (!IsValid(MiniMapWidget))
    {
        UE_LOG(LogGameMode, Warning, TEXT("?? MiniMapWidget not valid in UpdateMiniMapForCurrentHole"));
        return;
    }

    int32 HoleIndex = CurrentHole - 1;
    if (!MapInfo.TeePositions.IsValidIndex(HoleIndex) ||
        !MapInfo.HolecupPositions.IsValidIndex(HoleIndex))
    {
        UE_LOG(LogGameMode, Error, TEXT("? Invalid hole positions for hole %d"), CurrentHole);
        return;
    }

    FVector TeePosition = MapInfo.TeePositions[HoleIndex];
    FVector HolecupPosition = MapInfo.HolecupPositions[HoleIndex];

    if (TeePosition.ContainsNaN() || HolecupPosition.ContainsNaN())
    {
        UE_LOG(LogGameMode, Error, TEXT("? Invalid positions (NaN) for hole %d"), CurrentHole);
        return;
    }

    // MiniMapWidget->InitializeMiniMap(TeePosition, HolecupPosition);
    MiniMapWidget->SetCurrentHole(CurrentHole);

    // ? 현재 플레이어의 공 위치를 가져옵니다.
    AGolfBall* CurrentBall = nullptr;
    if (IsValid(PlayerManager))
    {
        TArray<AGolfBall*> PlayerBalls = PlayerManager->GetPlayerBalls();
        if (PlayerBalls.IsValidIndex(CurrentPlayerIndex))
        {
            CurrentBall = PlayerBalls[CurrentPlayerIndex];
        }
    }

    if (IsValid(CurrentBall))
    {
        // ? 수정: UpdateBallPosition 호출 시 CurrentBall->OwningPlayerIndex 전달
        // 1502번째 줄에서 오류가 발생한다면 이 부분을 수정합니다.
        MiniMapWidget->UpdateBallPosition(CurrentBall->OwningPlayerIndex, CurrentBall->GetActorLocation());
        float Distance = FVector::Dist(CurrentBall->GetActorLocation(), HolecupPosition);
        float Elevation = HolecupPosition.Z - CurrentBall->GetActorLocation().Z;
        MiniMapWidget->UpdateDistanceAndElevation(Distance, Elevation);
    }
    else
    {
        // 로깅 추가: 왜 CurrentBall이 유효하지 않은지 디버깅에 도움을 줄 수 있습니다.
        UE_LOG(LogGameMode, Warning, TEXT("?? CurrentBall is invalid in UpdateMiniMapForCurrentHole for Player %d"), CurrentPlayerIndex);
    }

    UE_LOG(LogGameMode, Log, TEXT("??? MiniMap updated for hole %d"), CurrentHole);
}

void AInGameMode::CreateMiniMapWidget()
{
    if (!MiniMapWidgetClass)
    {
        UE_LOG(LogGameMode, Error, TEXT("? MiniMapWidgetClass not set in GameMode Blueprint! Cannot create minimap."));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("?? MiniMapWidgetClass not configured!"));
        }
        return;
    }

    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!IsValid(PC))
    {
        UE_LOG(LogGameMode, Error, TEXT("? PlayerController not found. Cannot create minimap."));
        return;
    }

    // 기존 위젯이 있다면 제거 (이 부분은 재활용 로직을 위해 유지)
    if (IsValid(MiniMapWidget))
    {
        UE_LOG(LogGameMode, Log, TEXT("??? Removing existing MiniMapWidget for recreation."));
        MiniMapWidget->RemoveFromParent();
        MiniMapWidget = nullptr; // nullptr로 확실히 설정
    }

    // 새 위젯 생성 시도
    MiniMapWidget = CreateWidget<UGolfMiniMap>(PC, MiniMapWidgetClass);
    if (IsValid(MiniMapWidget))
    {
        //MiniMapWidget->AddToViewport(1000); // ZOrder는 필요 시 조절
        StrokeWidgetInstance->CanvasPanel_Minimap->AddChildToCanvas(MiniMapWidget);
        //MiniMapWidget->SetDesiredSizeInViewport(MiniMapWidget->GetDesiredSize());
        UCanvasPanelSlot* MinimapSlot = Cast<UCanvasPanelSlot>(MiniMapWidget->Slot);
        MinimapSlot->SetAutoSize(true);

        // 미니맵 초기화 및 데이터 업데이트는 호출하는 측에서 수행 (아래 UpdateMiniMapForCurrentHole 참조)
        // MiniMapWidget->InitializeMiniMap(TeePosition, HolecupPosition); // 여기서 직접 호출하지 않음
        // MiniMapWidget->SetCurrentHole(CurrentHole); // 여기서 직접 호출하지 않음
        // UpdateMiniMapForCurrentHole(); // 여기서 직접 호출하지 않음

        UE_LOG(LogGameMode, Log, TEXT("? MiniMapWidget created successfully and added to viewport."));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, TEXT("??? MiniMap created."));
        }
    }
    else
    {
        UE_LOG(LogGameMode, Error, TEXT("? Failed to create MiniMapWidget."));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("?? MiniMap creation failed!"));
        }
    }
}

void AInGameMode::DestroyMiniMapWidget()
{
    if (MiniMapWidget)
    {
        MiniMapWidget->RemoveFromParent();
        MiniMapWidget = nullptr;
    }
}

void AInGameMode::ShowMiniMap(bool bShow)
{
    if (MiniMapWidget)
    {
        ESlateVisibility Visibility = bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
        MiniMapWidget->SetVisibility(Visibility);
    }
}

void AInGameMode::RefreshMiniMapData()
{
    if (!MiniMapWidget)
        return;

    UpdateMiniMapForCurrentHole();
}

void AInGameMode::DebugWidgetClass()
{
    UE_LOG(LogGameMode, Warning, TEXT("=== Widget Class Debug Info ==="));
    UE_LOG(LogGameMode, Warning, TEXT("DefaultShotControlWidget: %s"),
        DefaultShotControlWidget ? *DefaultShotControlWidget->GetName() : TEXT("NULL"));

    if (DefaultShotControlWidget)
    {
        UE_LOG(LogGameMode, Warning, TEXT("Widget Class Path: %s"),
            *DefaultShotControlWidget->GetPathName());
        UE_LOG(LogGameMode, Warning, TEXT("Widget Class Valid: %s"),
            DefaultShotControlWidget->IsValidLowLevel() ? TEXT("True") : TEXT("False"));
    }
}

void AInGameMode::TryLoadWidgetClassDirectly(AGolfPlayerController* GolfPC)
{
    UE_LOG(LogGameMode, Warning, TEXT("?? Attempting to load widget class directly..."));

    FString WidgetPath = TEXT("/GolfGameBluePrint/WBP_GolfShotControlWidget.uasset");
    UClass* WidgetClass = LoadClass<UGolfShotControlWidget>(nullptr, *WidgetPath);

    if (WidgetClass)
    {
        UE_LOG(LogGameMode, Warning, TEXT("? Widget class loaded directly: %s"), *WidgetClass->GetName());
        GolfPC->ShotControlWidgetClass = WidgetClass;
        GolfPC->CreateShotControlWidget();
    }
    else
    {
        UE_LOG(LogGameMode, Error, TEXT("?? Failed to load widget class from: %s"), *WidgetPath);
        SearchForWidgetBlueprint(GolfPC);
    }
}

void AInGameMode::SearchForWidgetBlueprint(AGolfPlayerController* GolfPC)
{
    UE_LOG(LogGameMode, Warning, TEXT("?? Searching for widget blueprints..."));

    TArray<FString> PossiblePaths = {
        TEXT("/Game/GolfGameBluePrint/WBP_ShotControl.WBP_ShotControl_C"),
        TEXT("/Game/GolfGameBluePrint/WBP_ShotControl.WBP_ShotControl_C"),
        TEXT("/Game/GolfGameBluePrint/WBP_ShotControl.WBP_ShotControl_C"),
        TEXT("/Game/GolfGameBluePrint/WBP_ShotControl.WBP_ShotControl_C")
    };

    for (const FString& Path : PossiblePaths)
    {
        if (UClass* FoundClass = LoadClass<UGolfShotControlWidget>(nullptr, *Path))
        {
            UE_LOG(LogGameMode, Warning, TEXT("? Found widget class at: %s"), *Path);
            GolfPC->ShotControlWidgetClass = FoundClass;
            GolfPC->CreateShotControlWidget();
            return;
        }
    }

    UE_LOG(LogGameMode, Error, TEXT("?? No widget blueprint found in common locations!"));
}


void AInGameMode::ToggleMiniMapOBLines()
{
    if (IsValid(MiniMapWidget))
    {
        bool bCurrentlyVisible = MiniMapWidget->bShowOBLines;
        MiniMapWidget->SetOBLinesVisible(!bCurrentlyVisible);
        UE_LOG(LogGameMode, Log, TEXT("?? OB Lines toggled: %s"),
            !bCurrentlyVisible ? TEXT("ON") : TEXT("OFF"));
    }
}

void AInGameMode::SetMiniMapOBLineColor(FLinearColor NewColor)
{
    if (IsValid(MiniMapWidget))
    {
        MiniMapWidget->OBLineColor = NewColor;
        int32 HoleIndex = CurrentHole - 1;
        if (MapInfo.OBLines.IsValidIndex(HoleIndex))
        {
            MiniMapWidget->UpdateOBLines(MapInfo.OBLines[HoleIndex].Points);
        }
        UE_LOG(LogGameMode, Log, TEXT("?? OB Line color changed: %s"), *NewColor.ToString());
    }
}

void AInGameMode::RefreshMiniMapOBLines()
{
    if (IsValid(MiniMapWidget))
    {
        int32 HoleIndex = CurrentHole - 1;
        if (MapInfo.OBLines.IsValidIndex(HoleIndex))
        {
            const TArray<FVector>& OBPoints = MapInfo.OBLines[HoleIndex].Points;
            MiniMapWidget->UpdateOBLines(OBPoints);
            UE_LOG(LogGameMode, Log, TEXT("?? OB Lines refreshed for hole %d"), CurrentHole);
        }
    }
}

void AInGameMode::SetupLandscapeChecker()
{
    UE_LOG(LogTemp, Log, TEXT("?? Setting up LandscapeChecker..."));

    if (!GetWorld())
    {
        UE_LOG(LogTemp, Error, TEXT("? World is null in SetupLandscapeChecker"));
        return;
    }

    LandscapeChecker = ALandscapeChecker::GetLandscapeChecker(GetWorld());
    if (!LandscapeChecker)
    {
        FVector SpawnLocation = FVector::ZeroVector;
        FRotator SpawnRotation = FRotator::ZeroRotator;
        LandscapeChecker = GetWorld()->SpawnActor<ALandscapeChecker>(ALandscapeChecker::StaticClass(), SpawnLocation, SpawnRotation);
        if (!LandscapeChecker)
        {
            UE_LOG(LogTemp, Error, TEXT("? Failed to spawn LandscapeChecker"));
            return;
        }
        UE_LOG(LogTemp, Log, TEXT("?? LandscapeChecker created"));
    }

    SetupPhysicalMaterialMappings(LandscapeChecker);
    // 랜드체크를 마스크로 체킹
    SetupMaskTexture(LandscapeChecker);

#if WITH_EDITOR
    LandscapeChecker->bShowDebugInfo = false;
#endif
    UE_LOG(LogTemp, Log, TEXT("? LandscapeChecker setup completed"));
}

void AInGameMode::SetupTerrainHeightGrid()
{
    UE_LOG(LogTemp, Log, TEXT("?? Setting up TerrainHeightGrid..."));

    if (!GetWorld())
    {
        UE_LOG(LogTemp, Error, TEXT("? World is null in SetupTerrainHeightGrid"));
        return;
    }

    TerrainHeightGrid = ATerrainHeightGrid::GetOrCreateTerrainGrid(GetWorld());
    if (TerrainHeightGrid)
    {
        //FHeightColorSettings ColorSettings;
        //ColorSettings.LowHeightColor = FLinearColor(0.0f, 0.4f, 1.0f, 1.0f);
        //ColorSettings.MidHeightColor = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);
        //ColorSettings.HighHeightColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
        //ColorSettings.LowToMidThreshold = 0.1f;
        //ColorSettings.MidToHighThreshold = 1.f;
        //TerrainHeightGrid->HeightColorSettings = ColorSettings;

        for (AGolfBall* Ball : PlayerManager->GetPlayerBalls())
        {
            TerrainHeightGrid->AddTraceIgnoreActor(Ball);
        }

        UE_LOG(LogTemp, Log, TEXT("? TerrainHeightGrid setup completed"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("? Failed to setup TerrainHeightGrid"));
    }
}

void AInGameMode::ResetGameData()
{
    GameInfo.Reset();
    FDefaultGameOption DefaultGameOption;
    FString DefaultGameOptionPath =  TEXT("defaultGameData.json");
    UJsonLoader::LoadGameOptionFromJson(DefaultGameOptionPath, DefaultGameOption);
    GameInfo.GameOptions = DefaultGameOption.GameOptions;
}

void AInGameMode::SetupPhysicalMaterialMappings(ALandscapeChecker* Checker)
{
    if (!Checker)
    {
        UE_LOG(LogTemp, Error, TEXT("? Checker is null in SetupPhysicalMaterialMappings"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("?? Setting up Physical Material mappings..."));

    FString AssetPath = FString::Printf(
        TEXT("/Game/Land_physics/Grass"));

    int32 MappingCount = 0;
    if (UPhysicalMaterial* GrassPhysMat = LoadObject<UPhysicalMaterial>(nullptr, *AssetPath))
    {
        if (IsValid(GrassPhysMat))
        {
            FLandProperties GrassProps(ELandType::Grass, 1.0f, 1.0f, 0.0f, FLinearColor::Green, TEXT("잔디"));
            Checker->AddPhysicalMaterialMapping(GrassPhysMat, GrassProps);
            UE_LOG(LogTemp, Log, TEXT("? Grass material mapping added"));
            MappingCount++;
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("? Grass material is invalid"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("? Failed to load %s material"), *AssetPath);
    }

    AssetPath = FString::Printf(
        TEXT("/Game/Land_physics/Bunker"));

    if (UPhysicalMaterial* SandPhysMat = LoadObject<UPhysicalMaterial>(nullptr, *AssetPath))
    {
        if (IsValid(SandPhysMat))
        {
            FLandProperties SandProps(ELandType::Sand, 2.0f, 0.3f, 0.4f, FLinearColor::Yellow, TEXT("모래"));
            Checker->AddPhysicalMaterialMapping(SandPhysMat, SandProps);
            UE_LOG(LogTemp, Log, TEXT("? Sand material mapping added"));
            MappingCount++;
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("? Failed to load %s material"), *AssetPath);
    }

    AssetPath = FString::Printf(
        TEXT("/Game/Land_physics/Water"));
    if (UPhysicalMaterial* WaterPhysMat = LoadObject<UPhysicalMaterial>(nullptr, *AssetPath))
    {
        if (IsValid(WaterPhysMat))
        {
            FLandProperties WaterProps(ELandType::Water, 0.1f, 0.1f, 0.8f, FLinearColor::Blue, TEXT("물"));
            Checker->AddPhysicalMaterialMapping(WaterPhysMat, WaterProps);
            UE_LOG(LogTemp, Log, TEXT("? Water material mapping added"));
            MappingCount++;
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("? Failed to load %s material"), *AssetPath);
    }

    AssetPath = FString::Printf(
        TEXT("/Game/Land_physics/Green"));
    if (UPhysicalMaterial* GreenPhysMat = LoadObject<UPhysicalMaterial>(nullptr, *AssetPath))
    {
        if (IsValid(GreenPhysMat))
        {
            FLandProperties GreenProps(ELandType::Fairway, 0.5f, 1.0f, 0.0f, FLinearColor(0.2f, 0.8f, 0.2f, 1.0f), TEXT("그린"));
            Checker->AddPhysicalMaterialMapping(GreenPhysMat, GreenProps);
            UE_LOG(LogTemp, Log, TEXT("? Fairway material mapping added"));
            MappingCount++;
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("? Failed to load %s material"), *AssetPath);
    }



    AssetPath = FString::Printf(
        TEXT("/Game/Land_physics/Mat"));
    if (UPhysicalMaterial* MatPhysMat = LoadObject<UPhysicalMaterial>(nullptr, *AssetPath))
    {
        if (IsValid(MatPhysMat))
        {
            FLandProperties MatProps(ELandType::TeeBox, 0.5f, 1.0f, 0.0f, FLinearColor(0.2f, 0.8f, 0.2f, 1.0f), TEXT("티")); // 구름 개선
            Checker->AddPhysicalMaterialMapping(MatPhysMat, MatProps);
            UE_LOG(LogTemp, Log, TEXT("? Mat material mapping added"));
            MappingCount++;
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("? Failed to load %s material"), *AssetPath);
    }

    AssetPath = FString::Printf(
        TEXT("/Game/Land_physics/Fair"));
    if (UPhysicalMaterial* FairwayPhysMat = LoadObject<UPhysicalMaterial>(nullptr, *AssetPath))
    {
        if (IsValid(FairwayPhysMat))
        {
            FLandProperties FairwayProps(ELandType::Fairway, 0.6f, 1.1f, 0.0f, FLinearColor(0.2f, 0.8f, 0.2f, 1.0f), TEXT("페어웨이")); // 구름 개선
            Checker->AddPhysicalMaterialMapping(FairwayPhysMat, FairwayProps);
            UE_LOG(LogTemp, Log, TEXT("? Fairway material mapping added"));
            MappingCount++;
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("? Failed to load %s material"), *AssetPath);
    }

    AssetPath = FString::Printf(
        TEXT("/Game/Land_physics/Rough"));

    if (UPhysicalMaterial* RoughPhysMat = LoadObject<UPhysicalMaterial>(nullptr, *AssetPath))
    {
        if (IsValid(RoughPhysMat))
        {
            FLandProperties RoughProps(ELandType::Rough, 1.5f, 0.8f, 0.2f, FLinearColor(0.1f, 0.4f, 0.1f, 1.0f), TEXT("러프"));
            Checker->AddPhysicalMaterialMapping(RoughPhysMat, RoughProps);
            UE_LOG(LogTemp, Log, TEXT("? Rough material mapping added"));
            MappingCount++;
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("? Failed to load %s material"), *AssetPath);
    }

    UE_LOG(LogTemp, Log, TEXT("?? Physical Material mappings setup completed: %d mappings added"), MappingCount);
}

void AInGameMode::SetupPlayerInfoSlots()
{
    // 이 함수는 BeginPlay에서 이미 처리되었으므로 필요에 따라 호출 시점을 변경하거나,
    // 특정 이벤트 (예: 플레이어 수 변경)에 반응하도록 확장할 수 있습니다.
    UE_LOG(LogTemp, Log, TEXT("Player info slots setup initiated."));
    // 이미 BeginPlay에서 HUD를 생성하고 위젯을 추가했으므로,
    // 이 함수는 필요 없을 수도 있습니다.
    // 만약 플레이어가 동적으로 추가/삭제되는 경우가 아니라면, BeginPlay의 초기화로 충분합니다.
}

void AInGameMode::HighlightCurrentPlayerSlot(int32 PlayerIndex)
{
    for (int32 i = 0; i < PlayerInfoSlotWidgets.Num(); ++i)
    {
        if (PlayerInfoSlotWidgets.IsValidIndex(i) && IsValid(PlayerInfoSlotWidgets[i]))
        {
            if (PlayerInfoSlotWidgets[i]->OwningPlayerIndex == PlayerIndex)
            {
                PlayerInfoSlotWidgets[i]->UpdatePlayerStateDisplay(EPlayerState::Player_Ready); // 예시: 현재 턴 플레이어를 Ready 상태로 가정
                UE_LOG(LogTemp, Log, TEXT("Highlighted player slot: %d"), PlayerIndex);
            }
            else
            {
                // 다른 플레이어 슬롯의 하이라이트를 제거
                PlayerInfoSlotWidgets[i]->UpdatePlayerStateDisplay(EPlayerState::Player_Des); // 예시: 비활성 플레이어를 Des 상태로 가정
            }
        }
    }
}

UPlayerInfoSlotWidget* AInGameMode::GetCurrentSlot()
{
    for (int32 i = 0; i < PlayerInfoSlotWidgets.Num(); ++i)
    {
        if (PlayerInfoSlotWidgets.IsValidIndex(i) && IsValid(PlayerInfoSlotWidgets[i]))
        {
            if (PlayerInfoSlotWidgets[i]->OwningPlayerIndex == CurrentPlayerIndex)
            {
                return PlayerInfoSlotWidgets[i];
            }
        }
    }

    return nullptr;
}

UPlayerInfoSlotWidget* AInGameMode::FindPlayerInfoSlot(int32 SlotIndex, int32 PlayerIndex)
{
    const bool bUseSlotIndex = SlotIndex >= 0;
    const bool bUsePlayerIndex = PlayerIndex >= 0;

    if (!bUseSlotIndex && !bUsePlayerIndex)
    {
        return nullptr;
    }

    for (UPlayerInfoSlotWidget* SlotWidget : PlayerInfoSlotWidgets)
    {
        if (!IsValid(SlotWidget))
        {
            continue;
        }

        if (bUseSlotIndex && SlotWidget->OwningPlayerSlotIndex != SlotIndex)
        {
            continue;
        }

        if (bUsePlayerIndex && SlotWidget->OwningPlayerIndex != PlayerIndex)
        {
            continue;
        }

        return SlotWidget;
    }

    return nullptr;
}

void AInGameMode::UpdateAllPlayerInfoSlots()
{
    if (!PlayerManager) return;

    TArray<AGolfPlayer*> Players = PlayerManager->GetPlayers();
    for (int32 i = 0; i < PlayerInfoSlotWidgets.Num(); ++i)
    {
        if (!PlayerInfoSlotWidgets.IsValidIndex(i) || !IsValid(PlayerInfoSlotWidgets[i]))
        {
            continue;
        }

        const int32 SlotIndex = PlayerInfoSlotWidgets[i]->OwningPlayerSlotIndex;
        if (SlotIndex < 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("UpdateAllPlayerInfoSlots: Slot %d has invalid OwningPlayerSlotIndex=%d"), i, SlotIndex);
            continue;
        }

        int32 PlayerIdx = INDEX_NONE;
        for (int32 p = 0; p < Players.Num(); ++p)
        {
            if (Players.IsValidIndex(p) && IsValid(Players[p]) && Players[p]->PlayerInfo.SlotIndex == SlotIndex)
            {
                PlayerIdx = p;
                break;
            }
        }

        if (PlayerIdx == INDEX_NONE)
        {
            UE_LOG(LogTemp, Warning, TEXT("UpdateAllPlayerInfoSlots: Slot %d has no matching player for SlotIndex=%d"), i, SlotIndex);
            continue;
        }

        // Keep OwningPlayerIndex in sync for other UI logic (highlight/current turn).
        PlayerInfoSlotWidgets[i]->OwningPlayerIndex = PlayerIdx;
        // Always refresh display order for background image numbering.
        PlayerInfoSlotWidgets[i]->DisplayIndex = i;

        // ? 거리 계산 로직 추가
        float CurrentPlayerDistanceToHole = 0.0f;
        AGolfBall* PlayerBall = PlayerManager->GetPlayerBalls().IsValidIndex(PlayerIdx) ? PlayerManager->GetPlayerBalls()[PlayerIdx] : nullptr;
        if (IsValid(PlayerBall) && MapInfo.HolecupPositions.IsValidIndex(CurrentHole - 1))
        {
            CurrentPlayerDistanceToHole = FVector::Dist(PlayerBall->GetActorLocation(), MapInfo.HolecupPositions[CurrentHole - 1]);
        }
        // ? SetPlayerInfo 호출 시 DistanceToHole 전달
        PlayerInfoSlotWidgets[i]->SetPlayerInfo(Players[PlayerIdx]->GetPlayerInfo(), CurrentHole, PlayerIdx, CurrentPlayerDistanceToHole);
    }
    UE_LOG(LogTemp, Log, TEXT("All player info slots updated."));
}


void AInGameMode::BindBallEvents()
{
    if (!PlayerManager) return;

    for (AGolfBall* Ball : PlayerManager->GetPlayerBalls())
    {
        if (IsValid(Ball))
        {
            Ball->OnBallGameFlowEvent.AddDynamic(this, &AInGameMode::HandleBallGameFlowEvent); // 볼의 이벤트를 GameMode에 바인딩
            UE_LOG(LogTemp, Log, TEXT("? Bound BallGameFlowEvent for Ball %s"), *Ball->GetName());
        }
    }
}

void AInGameMode::ReBindBallEvents()
{
    if (!PlayerManager) return;

    for (AGolfBall* Ball : PlayerManager->GetPlayerBalls())
    {
        if (IsValid(Ball))
        {
            if (Ball->OnBallGameFlowEvent.IsBound())
                Ball->OnBallGameFlowEvent.RemoveDynamic(this, &AInGameMode::HandleBallGameFlowEvent);

            Ball->OnBallGameFlowEvent.AddDynamic(this, &AInGameMode::HandleBallGameFlowEvent); // 볼의 이벤트를 GameMode에 바인딩
            UE_LOG(LogTemp, Log, TEXT("? Bound BallGameFlowEvent for Ball %s"), *Ball->GetName());
        }
    }
}

void AInGameMode::HandleBallGameFlowEvent(EBallEvent EventType)
{
    // [Fix] 메인 스레드 보장 (CR2 센서 스레드 등에서 호출될 경우 대비)
    if (!IsInGameThread())
    {
        AsyncTask(ENamedThreads::GameThread, [this, EventType]()
            {
                HandleBallGameFlowEvent(EventType);
            });
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("InGameMode: Received Ball Game Flow Event: %s"), *UEnum::GetValueAsString(EventType));
    float DelayTime = 6.f;

    // Training Mode 별도 처리
    if (IsTrainingMode())
    {
        HandleBallGameFlowEvent_TrainingMode(EventType);
        return;
    }

    float HoleDistance = GetCurrentTurnGolfBall()->GetHoleDistance();

    // 기존 Stroke Mode 처리
    switch (EventType)
    {
    case EBallEvent::BallStopped:
        //DelayTime = HoleDistance <= 500.f && GameInfo.GameOptions.ContinuePutting == 1 ? 1.f : 4.f;
        if (GameInfo.GameOptions.SwingMotion)
        {
            DelayTime = 5.0f;  // 4.5 → 5.5초
        }
        else
        {
            if (IsRangeMode())
                DelayTime = 1.5f;
            else
                DelayTime = 4.5f;
        }

        break;
    case EBallEvent::HoleIn:
        DelayTime = 9.f;
        SetPlayerStateForPlayer(CurrentPlayerIndex, EPlayerState::Player_HoleOut);
        break;
    case EBallEvent::OutOfBounds:
        DelayTime = 5.f;
        HandleOBDropLogic();
        break;
    case EBallEvent::Conceded:
        DelayTime = 9.f;
        SetPlayerStateForPlayer(CurrentPlayerIndex, EPlayerState::Player_HoleOut);
        break;
    default:
        break;
    }
    if (GetCurrentTurnGolfPlayer()->CheckDoublePar())
        DelayTime = 6.f;

    // ⭐ 영상 재생 케이스(이글/알바트로스/홀인원) 판별
    //    반드시 HoleIn(홀인) 이벤트일 때만 판정 — BallStopped 등 중간 샷에서는 절대 걸리면 안 됨
    bool bIsVideoResult = false;
    if (EventType == EBallEvent::HoleIn)
    {
        int32 HoleIdx = CurrentHole - 1;
        if (MapInfo.ParScores.IsValidIndex(HoleIdx) && GetCurrentTurnGolfPlayer())
        {
            int32 ParScore = MapInfo.ParScores[HoleIdx];
            int32 CurrentShots = GetCurrentTurnGolfPlayer()->GetCurrentHoleShotCount();
            int32 RelativeScore = CurrentShots - ParScore;

            // 홀인원: 1타 홀인 / 이글: -2 / 알바트로스: -3 이하
            if (CurrentShots == 1 || RelativeScore <= -2)
            {
                bIsVideoResult = true;
            }

            UE_LOG(LogTemp, Warning, TEXT("🎬 HoleIn 판정: Shots=%d Par=%d Relative=%+d → Video=%s"),
                CurrentShots, ParScore, RelativeScore, bIsVideoResult ? TEXT("YES") : TEXT("NO"));
        }
    }

    if (bIsVideoResult)
    {
        // ⭐ 영상 케이스: 자동 턴전환 타이머를 걸지 않고 버튼 클릭까지 대기
        UE_LOG(LogTemp, Warning, TEXT("🎬 영상 결과: 자동 진행 보류, 버튼 클릭 대기"));

        // 영상 위젯의 버튼 클릭 델리게이트에 AdvanceTurn 연결 (중복 방지 위해 먼저 제거)
        if (ResultVideoWidgetInstance)
        {
            ResultVideoWidgetInstance->OnResultVideoClosed.RemoveDynamic(this, &AInGameMode::OnResultVideoClosed);
            ResultVideoWidgetInstance->OnResultVideoClosed.AddDynamic(this, &AInGameMode::OnResultVideoClosed);
        }
        else
        {
            // 안전장치: 영상 위젯이 없으면 기존처럼 자동 진행
            UE_LOG(LogTemp, Warning, TEXT("⚠️ ResultVideoWidgetInstance 없음 → 자동 진행 폴백"));
            StartTurnTransitionCountdown(DelayTime);
        }
    }
    else
    {
        // 일반 케이스: 기존대로 자동 턴 전환
        StartTurnTransitionCountdown(DelayTime);
    }

 //   StartTurnTransitionCountdown(DelayTime);
}

// GameMode 내에서 OB 드롭 로직을 처리하는 새로운 함수 예시
void AInGameMode::HandleOBDropLogic()
{
    // PlayerManager에서 현재 플레이어의 볼 가져오기
    if (PlayerManager && PlayerManager->GetPlayerBalls().IsValidIndex(CurrentPlayerIndex))
    {
        AGolfBall* CurrentBall = PlayerManager->GetPlayerBalls()[CurrentPlayerIndex];
        if (IsValid(CurrentBall))
        {
            CurrentBall->HandleOBDrop(); // 볼이 자체적으로 위치를 재조정
        }
    }
}

void AInGameMode::HandlePanelltyDropLogic()
{
    // PlayerManager에서 현재 플레이어의 볼 가져오기
    AGolfPlayer* Player = GetCurrentTurnGolfPlayer();

    if (Player)
    {
        if (Player->bDropAlready)
            return;

        if (PlayerManager && PlayerManager->GetPlayerBalls().IsValidIndex(CurrentPlayerIndex))
        {
            AGolfBall* CurrentBall = PlayerManager->GetPlayerBalls()[CurrentPlayerIndex];
            if (IsValid(CurrentBall))
            {
                CurrentBall->HandlePenaltyDrop(); // 볼이 자체적으로 위치를 재조정
                Player->bDropAlready = true;
            }
        }
    }

}

// ? StartTurnTransitionCountdown 함수 구현
void AInGameMode::StartTurnTransitionCountdown(float DelayTime)
{
    if (DelayTime <= 0.0f)
    {
        DelayTime = AGolfBall::TURN_TRANSITION_DELAY; // GolfBall의 상수를 활용
    }

    CurrentTurnCountdownTime = DelayTime;
    MaxTurnCountdownTime = DelayTime;

    // 기존 타이머가 있다면 클리어
    GetWorld()->GetTimerManager().ClearTimer(TurnCountdownTimer);

    // 카운트다운 업데이트 타이머 설정
    GetWorld()->GetTimerManager().SetTimer(
        TurnCountdownTimer,
        this,
        &AInGameMode::UpdateTurnCountdown,
        1.0f, // 1초마다 업데이트
        true  // 반복
    );

    // 실제 턴 전환을 위한 타이머 (전체 딜레이 시간 후 실행)
    GetWorld()->GetTimerManager().SetTimer(
        DelayedReadyTimer, // 기존 타이머 재활용 또는 새 타이머 정의
        [this]() {
            if (PlayerManager)
            {
                PlayerManager->AdvanceTurn(); // 플레이어 매니저에게 턴 진행 명령
                UE_LOG(LogTemp, Log, TEXT("? Scheduled turn advance executed by GameMode"));
            }
            GetWorld()->GetTimerManager().ClearTimer(TurnCountdownTimer); // 턴 진행 후 카운트다운 타이머 종료
            CurrentTurnCountdownTime = 0.0f; // 카운트다운 초기화
        },
        DelayTime,
        false
    );

    UE_LOG(LogTemp, Log, TEXT("?? GameMode: Turn transition countdown started: %.0f seconds"), DelayTime);
}

// ? UpdateTurnCountdown 함수 구현 (화면 표시용)
void AInGameMode::UpdateTurnCountdown()
{
    CurrentTurnCountdownTime -= 1.0f;

    if (CurrentTurnCountdownTime > 0.0f)
    {
        int32 SecondsLeft = FMath::RoundToInt(CurrentTurnCountdownTime);

        FColor DisplayColor;
        if (SecondsLeft <= 1)
            DisplayColor = FColor::Red;
        else if (SecondsLeft <= 2)
            DisplayColor = FColor::Orange;
        else
            DisplayColor = FColor::Yellow;
#if WITH_EDITOR

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(999, 1.1f, DisplayColor,
                FString::Printf(TEXT("? Next Turn: %d seconds"), SecondsLeft));
        }
#endif

    }
    else
    {
        // 타이머는 DelayedReadyTimer에 의해 자동으로 클리어되므로 여기서는 추가 작업 불필요
    }
}


void AInGameMode::UpdateBallNamePlateAndMarker()
{
    if (bGameInitialized)
    {
        for (int32 i = 0; i < PlayerManager->GetPlayers().Num(); i++)
        {
            if (PlayerManager->GetPlayerBalls().IsValidIndex(i) && PlayerManager->GetPlayers().IsValidIndex(i))
            {
                AGolfBall* Ball = PlayerManager->GetPlayerBalls()[i];
                AGolfPlayer* Player = PlayerManager->GetPlayers()[i];

                if (Player && Ball)
                {
                    FString NickName = Player->PlayerInfo.NickName;
                    Ball->BallNamePlateComponent->SetPlayerNameString(NickName);

                    Ball->BallNamePlateComponent->SetNamePlateVisible(true);

                    if (GetCurrentTurnGolfPlayer()->SlotIndex == Player->SlotIndex)
                    {
                        Ball->BallNamePlateComponent->SetNamePlateVisible(false);
                        Ball->GroundMarkerMesh->SetVisibility(false);
                    }
                    else
                    {
                        Ball->GroundMarkerMesh->SetVisibility(true);
                        Ball->SetBallVisibility(false);
                    }

                    if (Ball->CheckHoleIn() || Player->IsHoleIn() || Ball->IsConceded() || Player->bIsRuntimeAdded || Player->bIsPendingDelete || Ball->CheckTeeShot())
                    {
                        Ball->BallNamePlateComponent->SetNamePlateVisible(false);
                        Ball->GroundMarkerMesh->SetVisibility(false);
                    }

                    if (Player->PlayerInfo.ShotCountPerHole[CurrentHole - 1] == 0)
                    {
                        Ball->BallNamePlateComponent->SetNamePlateVisible(false);
                        Ball->GroundMarkerMesh->SetVisibility(false);
                    }
                }
            }
        }
    }
}


// ? FPlayerInfo 정보 갱신 함수 구현
void AInGameMode::UpdateGameInfoPlayers(const TArray<FPlayerInfo>& NewPlayers)
{
    GameInfo.Players = NewPlayers;
    UE_LOG(LogGameMode, Log, TEXT("? GameInfo.Players updated. Total players: %d"), GameInfo.Players.Num());
}

// ? FMapInfo 정보 갱신 함수 구현
void AInGameMode::UpdateGameInfoMapInfo(const FMapInfo& NewMapInfo)
{
    GameInfo.SelectedMap = NewMapInfo;
    UE_LOG(LogGameMode, Log, TEXT("? GameInfo.SelectedMap updated. Map Name: %s"), *GameInfo.SelectedMap.MapName);
}

// ? FGameOptionInfo 정보 갱신 함수 구현
void AInGameMode::UpdateGameInfoGameOptions(const FGameOptionInfo& NewGameOptions)
{
    GameInfo.GameOptions = NewGameOptions;
    UE_LOG(LogGameMode, Log, TEXT("? GameInfo.GameOptions updated. Game Type: %d"), GameInfo.GameOptions.GameType);
}

void AInGameMode::PlaceActorInFrontOnPlane(AActor* SourceActor, AActor* DescActor, float Distance)
{
    if (!SourceActor) return;

    const FVector SourceLoc = SourceActor->GetActorLocation();

    // 바라보는 방향에서 수평 성분만 사용
    FVector Forward = SourceActor->GetActorForwardVector();
    Forward.Z = 0.0f;
    Forward = Forward.GetSafeNormal(); // Z 제거 후 정규화 필수

    FVector TargetLoc = SourceLoc + (Forward * Distance);
    TargetLoc.Z = SourceLoc.Z; // “Z 동일” 명시

    // 회전도 수평만 쓰고 싶으면 Yaw만 사용
    const FRotator SourceRot = SourceActor->GetActorRotation();
    const FRotator TargetRot(0.0f, SourceRot.Yaw, 0.0f);

    DescActor->SetActorLocation(TargetLoc);
}


void AInGameMode::SetShowScoreBoard(int32 iShow)
{
    if (iShow)
    {
        if (StrokeWidgetInstance)
        {
            StrokeWidgetInstance->SetVisibility(ESlateVisibility::Hidden);
        }

        InGameScoreBoardWidgetInstance->Init();
        InGameScoreBoardWidgetInstance->UpdateScoreBoard();
        InGameScoreBoardWidgetInstance->SetVisibility(ESlateVisibility::Visible);
        USoundManager* SM = GetGameInstance()->GetSubsystem<USoundManager>();
        SM->PlayBGM_ById("BGM.ScoreBoard", 0.5f);
    }
    else
    {
        if (StrokeWidgetInstance)
        {
            StrokeWidgetInstance->SetVisibility(ESlateVisibility::Visible);
        }
        InGameScoreBoardWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
        USoundManager* SM = GetGameInstance()->GetSubsystem<USoundManager>();
        SM->StopBGM(0.5f);
    }
}

void AInGameMode::SetShowScoreStatBoard(bool bIsVisible)
{
    if (bIsVisible)
    {
        InGameScoreBoardStatWidgetInstance->SetVisibility(ESlateVisibility::Visible);
    }
}


// ? StrokeMenuWidget을 보이게 하는 함수
void AInGameMode::ShowStrokeMenu()
{
    if (StrokeMenuWidgetInstance)
    {
        // StrokeMenuWidgetInstance->AddToViewport(2000);
        StrokeMenuWidgetInstance->SetVisibility(ESlateVisibility::Visible);
        UE_LOG(LogTemp, Log, TEXT("StrokeMenuWidget Visible."));
    }
    else
        UE_LOG(LogTemp, Log, TEXT("----StrokeMenuWidget Not Visible."));
}

// ? StrokeMenuWidget을 숨기는 함수
void AInGameMode::HideStrokeMenu()
{
    if (StrokeMenuWidgetInstance)
    {
        StrokeMenuWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
        UE_LOG(LogTemp, Log, TEXT("StrokeMenuWidget Collapsed."));
    }
}

void AInGameMode::ShowInGameMenuPopup()
{
    if (InGamePopupWidgetInstance)
        InGamePopupWidgetInstance->SetVisibility(ESlateVisibility::Visible);
}


//void AInGameMode::ShowLoadingScreen()
//{
//    if (LoadingScreenWidgetInstance)
//    {
//        LoadingScreenWidgetInstance->SetVisibility(ESlateVisibility::Visible);
//        UE_LOG(LogTemp, Log, TEXT("? 로딩 화면 표시"));
//    }
//    else if (LoadingScreenWidgetClass)
//    {
//        APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
//        if (PC)
//        {
//            LoadingScreenWidgetInstance = CreateWidget<ULoadingWidget>(PC, LoadingScreenWidgetClass);
//            if (LoadingScreenWidgetInstance)
//            {
//                LoadingScreenWidgetInstance->SetRandomImageIndex();
//                LoadingScreenWidgetInstance->AddToViewport(10000); // 높은 ZOrder로 최상단 표시
//                LoadingScreenWidgetInstance->SetVisibility(ESlateVisibility::Visible);
//                UE_LOG(LogTemp, Log, TEXT("? 로딩 화면 위젯 생성 및 표시"));
//            }
//        }
//    }
//    else
//    {
//        UE_LOG(LogTemp, Warning, TEXT("?? 로딩 화면 위젯 클래스 없음"));
//    }
//}

//void AInGameMode::HideLoadingScreen()
//{
//    if (LoadingScreenWidgetInstance)
//    {
//        LoadingScreenWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
//        UE_LOG(LogTemp, Log, TEXT("? 로딩 화면 숨김"));
//    }
//}

void AInGameMode::TransitionToLevel(FName LevelName)
{
    //ShowLoadingScreen();
    UE_LOG(LogTemp, Log, TEXT("-----------------TransitionToLevel"));
    UGameplayStatics::OpenLevel(GetWorld(), LevelName);
    // OpenLevel 호출 후 로딩 화면은 비동기 로딩 완료 시 숨김 처리 필요
}



// ? 새로 추가 : 특정 플레이어의 상태를 변경하는 함수 구현
void AInGameMode::SetPlayerStateForPlayer(int32 PlayerIndex, EPlayerState NewState)
{
    if (!PlayerManager)
    {
        UE_LOG(LogTemp, Error, TEXT("? SetPlayerStateForPlayer: PlayerManager is null."));
        return;
    }

    TArray<AGolfPlayer*> Players = PlayerManager->GetPlayers();
    if (Players.IsValidIndex(PlayerIndex))
    {
        AGolfPlayer* TargetPlayer = Players[PlayerIndex];
        if (IsValid(TargetPlayer))
        {
            TargetPlayer->SetPlayerState(NewState);
            UE_LOG(LogTemp, Log, TEXT("? Player %d's state changed to %s by GameMode."), PlayerIndex, *UEnum::GetValueAsString(NewState));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("? SetPlayerStateForPlayer: Target player %d is invalid."), PlayerIndex);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("? SetPlayerStateForPlayer: Player index %d is out of bounds."), PlayerIndex);
    }
}


// 이벤트 핸들러 함수들

void AInGameMode::OnTeeHeightChanged(int32 Height)
{
    UE_LOG(LogTemp, Log, TEXT("Tee height changed to: %d mm"), Height);
}


void AInGameMode::OnKeyPressed(EAutoTeeKey Key)
{
    switch (Key)
    {
    case EAutoTeeKey::Mulligan:
        // 멀리건 처리
        break;
    case EAutoTeeKey::Function:
        // 기능 버튼 처리
        break;
        // ... 기타 키 처리
    }
}

void AInGameMode::SoftResetGameInfo()
{
    GameInfo.SoftReset();

    for (AGolfPlayer* Player : PlayerManager->GetPlayers())
    {
        Player->PlayerInfo.SoftReset();
    }

    SaveGameInfoToJSON();
}

AGolfBall* AInGameMode::GetCurrentTurnGolfBall()
{
    if (PlayerManager)
    {
        if (PlayerManager->GetPlayerBalls().IsValidIndex(CurrentPlayerIndex))
        {

            return Cast<AGolfBall>(PlayerManager->GetPlayerBalls()[CurrentPlayerIndex]);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("GolfBall is null from:AInGameMode::GetCurrentTurnGolfBall()"));
            return nullptr;
        }
    }

    return nullptr;
}

AGolfPlayer* AInGameMode::GetCurrentTurnGolfPlayer()
{
    if (PlayerManager)
    {
        if (PlayerManager->GetPlayers().IsValidIndex(CurrentPlayerIndex))
        {
            return Cast<AGolfPlayer>(PlayerManager->GetPlayers()[CurrentPlayerIndex]);
        }
        else
        {
            //    UE_LOG(LogTemp, Error, TEXT("GolfPlayer is null from:AInGameMode::GetCurrentTurnGolfPlayer() ----- index %d"),CurrentPlayerIndex);
            return nullptr;
        }
    }

    return nullptr;
}

void AInGameMode::SpawnHoleInParticle()
{
    if (AGolfPlayer* Player = GetCurrentTurnGolfPlayer())
    {
        FVector         HoleCupLocation = GameInfo.SelectedMap.HolecupPositions[CurrentHole - 1];
        FVector         BeforeBallPos = Player->BEFOREPos;
        int32           Score = Player->PlayerInfo.ShotCountPerHole[CurrentHole - 1] - GameInfo.SelectedMap.ParScores[CurrentHole - 1];

        float Distance = FVector::Dist(HoleCupLocation, BeforeBallPos);

        if (Distance >= 15 * 100.f &&
            Score <= 0
            ) //par 이상
        {
            AActor* Particle = nullptr;
            Particle = BallParticleManager->SpawnParticle(GetWorld(), "HoleInParty", HoleCupLocation + FVector(0, 0, 5.f));
            UE_LOG(LogTemp, Log, TEXT("SpawnHoleInParticle() : SPawned Party Particle, dist =%f, score=%d"), Distance, Score);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("SpawnHoleInParticle() : Player is null"));
    }
}
// ? 게임 모드 결정 함수 구현
void AInGameMode::DetermineGameMode()
{
    // GameInfo에서 게임 타입을 읽어와서 게임 모드 결정
    if (LoadGameInfoFromJSON())
    {
        // GameInfo.GameOptions.GameType을 사용하여 게임 모드 결정
        // GameType이 enum으로 정의되어 있다고 가정 (0: Stroke, 1: Training, 2: Range)
        switch (GameInfo.GameOptions.GameType)
        {
        case 0:
            CurrentGameMode = EGolfGameMode::StrokeMode;
            break;
        case 1:
            CurrentGameMode = EGolfGameMode::TrainingMode;
            break;
        case 2:
            CurrentGameMode = EGolfGameMode::RangeMode;
            break;
        default:
            CurrentGameMode = EGolfGameMode::StrokeMode; // 기본값
            break;
        }
    }
    else
    {
        // JSON 로드 실패 시 기본값
        CurrentGameMode = EGolfGameMode::StrokeMode;
    }





    UE_LOG(LogTemp, Log, TEXT("?? Game Mode Determined: %s (GameType: %d)"),
        *UEnum::GetValueAsString(CurrentGameMode), GameInfo.GameOptions.GameType);
}

// ? 게임 모드 설정 함수
void AInGameMode::SetGameMode(EGolfGameMode NewGameMode)
{
    if (CurrentGameMode != NewGameMode)
    {
        EGolfGameMode PreviousMode = CurrentGameMode;
        CurrentGameMode = NewGameMode;

        UE_LOG(LogTemp, Log, TEXT("?? Game Mode Changed: %s -> %s"),
            *UEnum::GetValueAsString(PreviousMode),
            *UEnum::GetValueAsString(CurrentGameMode));

        // 게임 모드 변경에 따른 추가 처리가 필요하면 여기에 구현
        if (IsRangeMode())
        {

            // TransitionToRangeLevel();
        }
    }
}

// ? Range Mode 레벨 전환 함수
void AInGameMode::TransitionToRangeLevel()
{
    UE_LOG(LogTemp, Log, TEXT("??? Transitioning to Range Level: %s"), *RangeLevelName.ToString());

    //ShowLoadingScreen();

    // Range 모드 전용 레벨로 전환
   // UGameplayStatics::OpenLevel(GetWorld(), RangeLevelName);
}


// ? MoveBallToTrainingPosition 함수 구현
void AInGameMode::MoveBallToTrainingPosition(const FVector& WorldPosition)
{
    if (!IsTrainingMode())
    {
        UE_LOG(LogTemp, Warning, TEXT("?? MoveBallToTrainingPosition called outside Training Mode"));
        return;
    }

    if (!IsTrainingModeClickAllowed(WorldPosition))
    {
        UE_LOG(LogTemp, Warning, TEXT("?? Training Mode ball movement not allowed at this position"));
        return;
    }

    if (!PlayerManager)
    {
        UE_LOG(LogTemp, Error, TEXT("? PlayerManager is null"));
        return;
    }

    TArray<AGolfBall*> PlayerBalls = PlayerManager->GetPlayerBalls();
    if (PlayerBalls.Num() == 0 || !IsValid(PlayerBalls[0]))
    {
        UE_LOG(LogTemp, Error, TEXT("? No valid training ball found"));
        return;
    }

    AGolfBall* TrainingBall = PlayerBalls[0];

    // 지형 높이 확인
    FVector FinalPosition = WorldPosition;
    if (UWorld* World = GetWorld())
    {
        FHitResult HitResult;
        FVector StartLocation = WorldPosition + FVector(0, 0, 1000.0f);
        FVector EndLocation = WorldPosition - FVector(0, 0, 1000.0f);

        FCollisionQueryParams CollisionParams;
        CollisionParams.AddIgnoredActor(TrainingBall);

        if (World->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, ECC_WorldStatic, CollisionParams))
        {
            FinalPosition = HitResult.Location + FVector(0, 0, 5.0f);
        }
    }

    // 볼을 새 위치로 이동
    TrainingBall->SetActorLocation(FinalPosition);
    TrainingBall->SetBallState(EBallState::Ball_Ready);

    // 물리 상태 리셋
    if (TrainingBall->BallMesh)
    {
        TrainingBall->BallMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
        TrainingBall->BallMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    }

    // 마지막 위치 저장
    LastTrainingBallPosition = FinalPosition;

    // 미니맵 업데이트
    if (IsValid(MiniMapWidget))
    {
        MiniMapWidget->UpdateBallPosition(0, FinalPosition);

        float Distance = FVector::Dist(FinalPosition, MapInfo.HolecupPositions[CurrentHole - 1]);
        float Elevation = MapInfo.HolecupPositions[CurrentHole - 1].Z - FinalPosition.Z;
        MiniMapWidget->UpdateDistanceAndElevation(Distance, Elevation);
    }

    UE_LOG(LogTemp, Log, TEXT("? Training ball moved to: %s"), *FinalPosition.ToString());

    // 화면에 피드백
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
            TEXT("?? Ball moved to clicked position"));
    }
}


// ? ShouldTransitionToHoleOut_StrokeMode 함수 구현
bool AInGameMode::ShouldTransitionToHoleOut_StrokeMode() const
{
    if (!PlayerManager)
        return false;

    // Stroke Mode에서 홀 완료 체크
    return PlayerManager->IsHoleComplete(CurrentHole);
}

// ? UpdateGamePlay_StrokeMode 함수 구현  
void AInGameMode::UpdateGamePlay_StrokeMode(float DeltaTime)
{
    // Stroke Mode 전용 게임 플레이 업데이트
    if (ShouldTransitionToHoleOut_StrokeMode())
    {
        //// IMPORTANT:
        //// HandleBallGameFlowEvent()/ResultWidget can schedule a delayed AdvanceTurn() via DelayedReadyTimer.
        //// If we transition to Game_HoleOut here while that timer is still pending, we effectively "skip"
        //// the intended result delay and can also end up with a stray AdvanceTurn firing in the next hole.
        //if (UWorld* World = GetWorld())
        //{
        //    if (World->GetTimerManager().IsTimerActive(TurnCountdownTimer) ||
        //        World->GetTimerManager().IsTimerActive(DelayedReadyTimer) ||
        //        CurrentTurnCountdownTime > 0.0f)
        //    {
        //        UE_LOG(LogGameMode, Verbose, TEXT("Deferring Game_HoleOut transition: turn transition countdown is active."));
        //        return;
        //    }
        //}

        ChangeGameState(EGameState::Game_HoleOut, 5.f);
        return;
    }

    if (ShouldTransitionToGameEnd())
    {
        ChangeGameState(EGameState::Game_End);
        return;
    }
}

// ? 기존 UpdateGamePlay 함수 수정 (모드별 분기 추가)
void AInGameMode::UpdateGamePlay(float DeltaTime)
{
    // 게임 모드별 업데이트 분기
    switch (CurrentGameMode)
    {
    case EGolfGameMode::StrokeMode:
        UpdateGamePlay_StrokeMode(DeltaTime);
        break;

    case EGolfGameMode::TrainingMode:
        UpdateGamePlay_TrainingMode(DeltaTime);
        break;

    case EGolfGameMode::RangeMode:
        // Range Mode는 다른 레벨에서 실행되므로 여기서는 처리하지 않음
        break;

    default:
        UpdateGamePlay_StrokeMode(DeltaTime); // 기본값
        break;
    }
}

// ? Training Mode 전환 조건 수정
bool AInGameMode::ShouldTransitionToHoleOut_TrainingMode() const
{
    // Training Mode에서는 홀 완료로 전환하지 않음
    return false;
}

// ? Training Mode에서 볼 이벤트 처리
void AInGameMode::HandleBallGameFlowEvent_TrainingMode(EBallEvent EventType)
{
    UE_LOG(LogTemp, Log, TEXT("?? Training Mode: Ball Event: %s"), *UEnum::GetValueAsString(EventType));

    switch (EventType)
    {
    case EBallEvent::BallStopped:
        // Training Mode에서는 즉시 다음 샷 준비
        PrepareNextTrainingShot();
        break;

    case EBallEvent::HoleIn:
        // 홀인 시에도 계속 연습 모드 유지
        HandleTrainingHoleIn();
        break;

    case EBallEvent::OutOfBounds:
        // OB 처리 후 계속 연습
        HandleTrainingOB();
        break;

    case EBallEvent::Conceded:
        // Concede도 무시하고 계속 연습
        PrepareNextTrainingShot();
        break;

    default:
        PrepareNextTrainingShot();
        break;
    }
}

// ? Training Mode 다음 샷 준비
void AInGameMode::PrepareNextTrainingShot()
{
    if (!PlayerManager)
        return;

    TArray<AGolfBall*> PlayerBalls = PlayerManager->GetPlayerBalls();
    if (PlayerBalls.IsValidIndex(0))
    {
        AGolfBall* TrainingBall = PlayerBalls[0];
        if (IsValid(TrainingBall))
        {
            // 1초 후 이전 출발 위치로 리셋
            GetWorld()->GetTimerManager().SetTimer(
                DelayedReadyTimer,
                [this, TrainingBall]()
                {

                    TArray<AGolfPlayer*> Players = PlayerManager->GetPlayers();
                    if (Players.IsValidIndex(0))
                    {

                        AGolfPlayer* TrainingPlayer = Players[0];
                        TrainingPlayer->SetPlayerInfoToGameInfo();
                        FVector BeforePos = TrainingPlayer->BEFOREPos;
                        // 볼 위치 리셋
                        TrainingBall->SetActorLocation(BeforePos);
                        TrainingBall->SetBallState(EBallState::Ball_Ready);

                        // 물리 리셋
                        if (TrainingBall->BallMesh)
                        {
                            TrainingBall->BallMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
                            TrainingBall->BallMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
                        }

                        // 미니맵 업데이트
                        if (IsValid(MiniMapWidget))
                        {
                            MiniMapWidget->UpdateBallPosition(0, BeforePos);
                        }

                        // 플레이어 상태 Ready로 설정
                        TrainingPlayer->SetPlayerState(EPlayerState::Player_Ready);

                        // ? 볼이 Ready 상태가 되면 센서 준비 상태 확인
                        if (this)
                            PlayerManager->CheckSensorReadyState(0);

                        UE_LOG(LogTemp, Log, TEXT("? Training shot ready at before position: %s"), *BeforePos.ToString());
                    }
                    else
                    {
                        UE_LOG(LogTemp, Log, TEXT("? Training shot ready Not 0 index:"));
                    }
                },
                1.0f,
                false
            );
        }
    }
}
// ? Training Mode 홀인 처리
void AInGameMode::HandleTrainingHoleIn()
{
    UE_LOG(LogTemp, Log, TEXT("?? Training Mode: Hole In! Continuing practice..."));

    // 홀인 파티클 효과 (선택사항)
    //SpawnHoleInParticle();

    // 화면에 피드백
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
            TEXT("?? Hole In! Great shot! Continue practicing..."));
    }

    // 볼을 티 위치로 리셋 (또는 현재 위치 유지)
    ResetStartBallPosReturn();
}

// ? Training Mode OB 처리
void AInGameMode::HandleTrainingOB()
{
    UE_LOG(LogTemp, Log, TEXT("?? Training Mode: OB! Resetting for practice..."));

    // 화면에 피드백
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
            TEXT("?? Out of Bounds! Ball reset for practice"));
    }

    // 볼을 마지막 유효 위치 또는 티로 리셋
    ResetStartBallPosReturn();
}

// ? Training Mode 볼을 티로 리셋
void AInGameMode::ResetTrainingBallToTee()
{
    if (!PlayerManager || !MapInfo.TeePositions.IsValidIndex(CurrentHole - 1))
        return;

    TArray<AGolfBall*> PlayerBalls = PlayerManager->GetPlayerBalls();
    if (PlayerBalls.IsValidIndex(0))
    {
        AGolfBall* TrainingBall = PlayerBalls[0];
        if (IsValid(TrainingBall))
        {
            FVector TeePosition = MapInfo.TeePositions[CurrentHole - 1] + FVector(0, 0, 5.0f);
            TrainingBall->SetActorLocation(TeePosition);
            TrainingBall->SetBallState(EBallState::Ball_Ready);

            // 물리 리셋
            if (TrainingBall->BallMesh)
            {
                TrainingBall->BallMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
                TrainingBall->BallMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
            }

            // 미니맵 업데이트
            if (IsValid(MiniMapWidget))
            {
                MiniMapWidget->UpdateBallPosition(0, TeePosition);
            }

            UE_LOG(LogTemp, Log, TEXT("? Training ball reset to tee"));
        }
    }
}

void AInGameMode::ResetStartBallPosReturn()
{
    if (!PlayerManager || !MapInfo.TeePositions.IsValidIndex(CurrentHole - 1))
        return;

    TArray<AGolfBall*> PlayerBalls = PlayerManager->GetPlayerBalls();
    if (PlayerBalls.IsValidIndex(0))
    {
        AGolfBall* TrainingBall = PlayerBalls[0];
        if (IsValid(TrainingBall))
        {
            TArray<AGolfPlayer*> Players = PlayerManager->GetPlayers();
            AGolfPlayer* TrainingPlayer = Players[0];
            TrainingPlayer->SetPlayerInfoToGameInfo();
            FVector BeforePos = TrainingPlayer->BEFOREPos;

            TrainingBall->SetActorLocation(BeforePos);
            TrainingBall->SetBallState(EBallState::Ball_Ready);

            // 물리 리셋
            if (TrainingBall->BallMesh)
            {
                TrainingBall->BallMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
                TrainingBall->BallMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
            }

            // 미니맵 업데이트
            if (IsValid(MiniMapWidget))
            {
                MiniMapWidget->UpdateBallPosition(0, BeforePos);
            }

            UE_LOG(LogTemp, Log, TEXT("? Training ball reset to tee"));
        }
    }
}
// ? Training Mode 전용 업데이트(기존 UpdateGamePlay 수정)
void AInGameMode::UpdateGamePlay_TrainingMode(float DeltaTime)
{
    // Training Mode에서는 홀 완료나 게임 종료로 전환하지 않음
    // 단순히 연습을 계속할 수 있도록 유지

    if (!PlayerManager)
        return;

    // 플레이어가 항상 Ready 상태를 유지하도록 확인
    TArray<AGolfPlayer*> Players = PlayerManager->GetPlayers();
    if (Players.IsValidIndex(0))
    {
        AGolfPlayer* TrainingPlayer = Players[0];
        if (TrainingPlayer->GetPlayerState() == EPlayerState::Player_Des)
        {
            TrainingPlayer->SetPlayerState(EPlayerState::Player_Ready);
        }
    }
}


// ? Training Mode에서 미니맵 클릭 허용 여부 확인
bool AInGameMode::IsTrainingModeClickAllowed(const FVector& WorldPosition) const
{
    if (!IsTrainingMode())
        return false;

    // 홀컵에서 최소 거리 체크
    if (MapInfo.HolecupPositions.IsValidIndex(CurrentHole - 1))
    {
        FVector HolecupPos = MapInfo.HolecupPositions[CurrentHole - 1];
        float DistanceToHole = FVector::Dist(WorldPosition, HolecupPos);

        if (DistanceToHole < 100.0f) // 홀컵에서 1m 이내는 금지
        {
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red,
                    TEXT("? Too close to hole! Move further away"));
            }
            return false;
        }
    }

    // OB 영역 체크 (선택사항)
    // 여기에 OB 체크 로직 추가 가능

    return true;
}


bool AInGameMode::IsPointInOBArea(const FVector& WorldPoint) const
{
    // OB 라인 포인트가 3개 미만이면 유효한 다각형이 아니므로 false 반환
    if (MapInfo.OBLines.Num() < 3)
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("OB Check: Not enough points (%d)"), MapInfo.OBLines.Num());
        return false;
    }

    // NaN 체크
    if (WorldPoint.ContainsNaN())
    {
        UE_LOG(LogTemp, Warning, TEXT("OB Check: WorldPoint contains NaN: %s"), *WorldPoint.ToString());
        return true; // 안전을 위해 OB로 처리
    }

    // Z축을 무시하고 2D 평면에서 계산
    FVector2D TestPoint(WorldPoint.X, WorldPoint.Y);

    // 와인딩 넘버 알고리즘 사용
    int32 WindingNumber = CalculateWindingNumber(TestPoint, MapInfo.OBLines[CurrentHole - 1].Points);

    // 와인딩 넘버가 0이면 다각형 외부(OB), 0이 아니면 내부(인바운드)
    bool bIsOB = (WindingNumber == 0);

    UE_LOG(LogTemp, VeryVerbose, TEXT("OB Check: Point %s, WindingNumber=%d, IsOB=%s"),
        *WorldPoint.ToString(), WindingNumber, bIsOB ? TEXT("True") : TEXT("False"));

    return bIsOB;
}

void AInGameMode::UseMulligan()
{

    //일단 전 플레이어의 Ball, Player를 찾음
    //

}

int32 AInGameMode::CalculateWindingNumber(const FVector2D& TestPoint, const TArray<FVector>& Polygon) const
{
    int32 WindingNumber = 0;
    int32 PolygonSize = Polygon.Num();

    for (int32 i = 0; i < PolygonSize; i++)
    {
        int32 NextIndex = (i + 1) % PolygonSize;

        FVector2D P1(Polygon[i].X, Polygon[i].Y);
        FVector2D P2(Polygon[NextIndex].X, Polygon[NextIndex].Y);

        // 상향 교차 확인
        if (P1.Y <= TestPoint.Y)
        {
            if (P2.Y > TestPoint.Y) // 상향 교차
            {
                float CrossProduct = CalculateCrossProduct2D(P1, P2, TestPoint);
                if (CrossProduct > 0) // 점이 선분의 왼쪽에 있음
                {
                    WindingNumber++;
                }
            }
        }
        // 하향 교차 확인
        else
        {
            if (P2.Y <= TestPoint.Y) // 하향 교차
            {
                float CrossProduct = CalculateCrossProduct2D(P1, P2, TestPoint);
                if (CrossProduct < 0) // 점이 선분의 오른쪽에 있음
                {
                    WindingNumber--;
                }
            }
        }
    }

    return WindingNumber;
}

float AInGameMode::CalculateCrossProduct2D(const FVector2D& P1, const FVector2D& P2, const FVector2D& TestPoint) const
{
    // 외적 계산: (P2 - P1) × (TestPoint - P1)
    // 결과가 양수면 TestPoint가 P1->P2 벡터의 왼쪽, 음수면 오른쪽
    return (P2.X - P1.X) * (TestPoint.Y - P1.Y) - (TestPoint.X - P1.X) * (P2.Y - P1.Y);
}


void AInGameMode::UpdateMiniMapAimLine()
{
    if (!MiniMapWidget || !PlayerManager)
    {
        return;
    }

    // 현재 플레이어의 에임 라인만 업데이트
    TArray<AGolfBall*> PlayerBalls = PlayerManager->GetPlayerBalls();
    if (PlayerBalls.IsValidIndex(CurrentPlayerIndex))
    {
        AGolfBall* CurrentBall = PlayerBalls[CurrentPlayerIndex];
        if (IsValid(CurrentBall))
        {
            FVector BallLocation = CurrentBall->GetActorLocation();
            MiniMapWidget->UpdateBallPosition(CurrentPlayerIndex, BallLocation);

            // AimLocation이 설정되어 있으면 AimActor 위치도 업데이트
            if (!AimLocation.IsZero())
            {
                MiniMapWidget->UpdateAimActorPosition(CurrentPlayerIndex, AimLocation);
            }
        }
    }
}

void AInGameMode::OnPlayerIndexChanged(int32 NewPlayerIndex, int32 OldPlayerIndex)
{
    UE_LOG(LogTemp, Log, TEXT("Player index changed from %d to %d"), OldPlayerIndex, NewPlayerIndex);

    // 미니맵에서 이전 플레이어 AimActor 숨기고 현재 플레이어만 표시
    if (MiniMapWidget)
    {
        MiniMapWidget->HideAllAimActorExceptCurrent();
    }

    // PlayerController에서 AimActor 위치 업데이트 (새로운 현재 플레이어 기준)
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
    {
        if (AGolfPlayerController* GolfPC = Cast<AGolfPlayerController>(PC))
        {
            // 새로운 현재 플레이어를 위해 AimActor 위치 재설정
            GolfPC->UpdateAimActorPosition();
        }
    }
    // ? 새로 추가: 플레이어 변경 시 카메라 옵션 재적용
   // ApplyCameraModeOptionToCameraManager();
}


// 미니맵에서 현재 플레이어 정보만 업데이트하는 함수 추가
void AInGameMode::UpdateMiniMapForCurrentPlayer()
{
    if (!MiniMapWidget || !PlayerManager)
    {
        return;
    }

    int32 PlayerIndex = CurrentPlayerIndex;
    TArray<AGolfBall*> PlayerBalls = PlayerManager->GetPlayerBalls();

    if (PlayerBalls.IsValidIndex(PlayerIndex))
    {
        AGolfBall* CurrentBall = PlayerBalls[PlayerIndex];
        if (IsValid(CurrentBall))
        {
            // 볼 위치 업데이트
            FVector BallLocation = CurrentBall->GetActorLocation();
            MiniMapWidget->UpdateBallPosition(PlayerIndex, BallLocation);

            // 거리/고도차 정보 업데이트
            if (MapInfo.HolecupPositions.IsValidIndex(CurrentHole - 1))
            {
                FVector HolePos = MapInfo.HolecupPositions[CurrentHole - 1];
                float Distance = FVector::Dist(BallLocation, HolePos);
                float Elevation = HolePos.Z - BallLocation.Z;
                MiniMapWidget->UpdateDistanceAndElevation(Distance, Elevation);
            }

            // 에임 방향 업데이트 (PlayerController에서 설정된 경우)
            if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
            {
                if (AGolfPlayerController* GolfPC = Cast<AGolfPlayerController>(PC))
                {
                    FVector AimDir = GolfPC->AimDirection;
                    if (!AimDir.IsNearlyZero())
                    {
                        MiniMapWidget->UpdateAimDirection(PlayerIndex, AimDir);
                    }
                }
            }
        }
    }
}


void AInGameMode::SetupMaskTexture(ALandscapeChecker* Checker)
{
    if (!Checker)
    {
        UE_LOG(LogTemp, Error, TEXT("? Checker is null in SetupMaskTexture"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("?? Setting up Mask Texture..."));

    // 마스크 텍스처 로드
    FString MaskAssetPath = TEXT("/Game/Landscape_Material/mask");
    UTexture2D* MaskTexture = LoadObject<UTexture2D>(nullptr, *MaskAssetPath);

    if (!MaskTexture)
    {
        UE_LOG(LogTemp, Warning, TEXT("?? Failed to load mask texture from: %s"), *MaskAssetPath);
        UE_LOG(LogTemp, Warning, TEXT("?? Mask texture not found, using Physical Material only"));
        return;
    }

    if (!IsValid(MaskTexture))
    {
        UE_LOG(LogTemp, Error, TEXT("? Loaded mask texture is invalid"));
        return;
    }

    // 마스크 텍스처 설정
    Checker->SetMaskTexture(MaskTexture);

    //Checker->AnalyzeLandscapeBounds();

    // 마스크 사용 활성화
    Checker->bUseMaskTexture = true;

    // RGB 임계값 설정 (필요시 조정)
    Checker->BunkerRedThreshold = 8;  // R값 128 이상이면 벙커
    Checker->GreenGreenThreshold = 8; // G값 128 이상이면 그린

    UE_LOG(LogTemp, Log, TEXT("? Mask texture setup completed"));
    UE_LOG(LogTemp, Log, TEXT("?? Texture: %s (%dx%d)"),
        *MaskTexture->GetName(), MaskTexture->GetSizeX(), MaskTexture->GetSizeY());
    UE_LOG(LogTemp, Log, TEXT("?? RGB Thresholds - Red(Bunker): %d, Green(Green): %d"),
        Checker->BunkerRedThreshold, Checker->GreenGreenThreshold);
}


// ? AutoTee 장치 연결 함수
void AInGameMode::ConnectAutoTeeDevice()
{
    LoadSystemConfig();

    if (!SystemConfig.bAutoTeeEnabled)
    {
        return;
    }

    if (!AutoTeeController)
    {
        AutoTeeController = NewObject<UAutoTeeController>(this);
    }

    FString PortName = FString::Printf(TEXT("COM%d"), SystemConfig.ComPort);

    // 이벤트 바인딩
    if (!AutoTeeController->OnTeeHeightChanged.IsAlreadyBound(this, &AInGameMode::OnAutoTeeHeightChanged))
    {
        AutoTeeController->OnTeeHeightChanged.AddDynamic(this, &AInGameMode::OnAutoTeeHeightChanged);
    }

    if (!AutoTeeController->OnKeyPressed.IsAlreadyBound(this, &AInGameMode::OnAutoTeeKeyPressed))
    {
        AutoTeeController->OnKeyPressed.AddDynamic(this, &AInGameMode::OnAutoTeeKeyPressed);
    }

    // ? KeyReleased 델리게이트 바인딩 추가
    if (!AutoTeeController->OnKeyReleased.IsAlreadyBound(this, &AInGameMode::OnAutoTeeKeyReleased))
    {
        AutoTeeController->OnKeyReleased.AddDynamic(this, &AInGameMode::OnAutoTeeKeyReleased);
        UE_LOG(LogTemp, Log, TEXT("? OnKeyReleased delegate bound"));
    }

    // ? V3 키패드 설정
    AutoTeeController->SetKeypadVersion(EKeypadVersion::V3);

    // ? 키 반복 간격 설정 (0.3초로 증가 - 더 안정적)
    AutoTeeController->SetKeyRepeatInterval(0.3f);
    AutoTeeController->SetKeyRepeatDelay(0.3f);  // 첫 반복도 0.3초 후

    AutoTeeController->ConnectToDeviceAsync(PortName, SystemConfig.BaudRate);

    OnAutoTeeConnectionChanged();

    UE_LOG(LogTemp, Log, TEXT("? V3 Keypad with 0.3s repeat interval enabled"));
}


// ? 연결 상태 변경 핸들러 구현
void AInGameMode::OnAutoTeeConnectionChanged()
{
    if (AutoTeeController)
    {
        // ? AutoTeeController의 연결 상태를 InGameMode에 동기화
        bAutoTeeConnected = AutoTeeController->IsConnected();

        if (bAutoTeeConnected)
        {
            UE_LOG(LogTemp, Log, TEXT("? InGameMode: AutoTee connection established"));

            // 연결 성공 시 초기화 (홈 포지션으로)
            AutoTeeController->MoveToHomeAsync();
            CurrentAutoTeeHeight = 0;

            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
                    FString::Printf(TEXT("? AutoTee Connected (COM%d)"), SystemConfig.ComPort));
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("?? InGameMode: AutoTee disconnected"));
            CurrentAutoTeeHeight = 0;
        }
    }
}



// ? AutoTee 장치 연결 해제 함수
void AInGameMode::DisconnectAutoTeeDevice()
{
    if (AutoTeeController && bAutoTeeConnected)
    {
        AutoTeeController->DisconnectFromDevice();
        bAutoTeeConnected = false;
        UE_LOG(LogTemp, Log, TEXT("? AutoTee device disconnected"));
    }
}

// ? 티 높이 설정 함수
void AInGameMode::SetAutoTeeHeight()
{
    // ? AutoTeeController가 없거나 연결되지 않았으면 종료
    if (!AutoTeeController)
    {
        UE_LOG(LogTemp, Log, TEXT("?? AutoTeeController is null"));
        return;
    }

    if (!bAutoTeeConnected)
    {
        UE_LOG(LogTemp, Log, TEXT("?? AutoTee not connected"));
        return;
    }

    AGolfPlayer* CurrentPlayer = GetCurrentTurnGolfPlayer();
    if (!CurrentPlayer)
    {
        UE_LOG(LogTemp, Warning, TEXT("?? No current player found"));
        return;
    }

    int32 PreferredHeight = 25; // 기본 티 높이

    // 티샷인 경우에만 티 높이 설정
    //if (CurrentPlayer->GetCurrentHoleShotCount() == 0)
    //{
    //    PreferredHeight = FMath::Clamp(PreferredHeight, 0, 60);

    //    // ? 비동기 함수 사용
    //  //  AutoTeeController->SetTeeHeightAsync(PreferredHeight);

    //    UE_LOG(LogTemp, Log, TEXT("?? Setting tee height to %d mm for Player %d"),
    //        PreferredHeight, CurrentPlayer->PlayerIndex);

    //    if (GEngine)
    //    {
    //        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan,
    //            FString::Printf(TEXT("?? Tee Height: %dmm"), PreferredHeight));
    //    }
    //}
    //else
    {

        //  PreferredHeight = FMath::Clamp(PreferredHeight, 0, 60);

          // ? 비동기 함수 사용
        AutoTeeController->SetTeeHeightAsync(PreferredHeight);

        UE_LOG(LogTemp, Log, TEXT("?? Setting tee height to %d mm for Player %d"),
            PreferredHeight, CurrentPlayer->PlayerIndex);

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan,
                FString::Printf(TEXT("?? Tee Height: %dmm"), PreferredHeight));
        }

        //// 티샷이 아닌 경우 티를 내림
        //AutoTeeController->MoveToHomeAsync();
        //UE_LOG(LogTemp, Log, TEXT("?? Not a tee shot - moving tee to home position"));
    }
}

// ? 키패드 Release 이벤트 핸들러 (단발 액션 처리)
void AInGameMode::OnAutoTeeKeyReleased(EAutoTeeKey Key)
{
    // [Fix] 백그라운드 스레드에서 호출될 경우 메인 스레드로 넘김
    if (!IsInGameThread())
    {
        AsyncTask(ENamedThreads::GameThread, [this, Key]()
            {
                OnAutoTeeKeyReleased(Key);
            });
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("?? Key Released: %d"), (int32)Key);

    switch (Key)
    {
    case EAutoTeeKey::Grid:
        // ? 단발 액션: 그리드 토글 (Release 시 한 번만)
        if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
        {
            if (AGolfPlayerController* GolfPC = Cast<AGolfPlayerController>(PC))
            {
                GolfPC->ToggleTerrainGrid();
                UE_LOG(LogTemp, Log, TEXT("?? Grid Toggled"));
            }
        }
        break;

    case EAutoTeeKey::Mulligan:
        // ? 단발 액션: 멀리건 사용 (Release 시 한 번만)
        UE_LOG(LogTemp, Log, TEXT("?? Mulligan key pressed"));

        // 팝업이 떠있으면 멀리건 사용
        if (InGamePopupWidgetInstance->GetVisibility() == ESlateVisibility::Visible)
        {
            AGolfPlayer* CurrentPlayer = GetCurrentTurnGolfPlayer();
            if (!CheckFirstShot() && CurrentPlayer && GameInfo.GameOptions.Mulligan_Count > 0
                && GameInfo.LatestUseMulliganPlayerIndex != CurrentPlayerIndex && CurrentPlayer->GetPlayerState() == EPlayerState::Player_Ready)
            {
                // 멀리건 로직 실행
                CurrentPlayer->UseMulligan();

                if (GEngine)
                {
                    GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange,
                        TEXT("?? Mulligan Used!"));
                }
            }
            InGamePopupWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
        }
        else
        {
            StrokeWidgetInstance->OnMulliganButtonClicked();
        }
        break;

    case EAutoTeeKey::Function:
        // ? 단발 액션: 메뉴 토글 (Release 시 한 번만)
        ShowInGameMenuPopup();
        UE_LOG(LogTemp, Log, TEXT("?? Function Menu Toggled"));
        break;

        // ? 연속 액션들은 자동으로 StopKeyRepeat이 호출되므로 별도 처리 불필요
    case EAutoTeeKey::Left:
    case EAutoTeeKey::Right:
    case EAutoTeeKey::Up:
    case EAutoTeeKey::Down:
        UE_LOG(LogTemp, VeryVerbose, TEXT("?? Continuous action stopped"));
        break;

    default:
        break;
    }
}

// ? 티 높이 변경 이벤트 핸들러
void AInGameMode::OnAutoTeeHeightChanged(int32 Height)
{
    // [Fix] 메인 스레드 보장
    if (!IsInGameThread())
    {
        AsyncTask(ENamedThreads::GameThread, [this, Height]()
            {
                OnAutoTeeHeightChanged(Height);
            });
        return;
    }

    CurrentAutoTeeHeight = Height;
    UE_LOG(LogTemp, Log, TEXT("?? AutoTee height changed to: %d mm"), Height);
}
// ? 키패드 입력 이벤트 핸들러 (연속 액션만 처리)
void AInGameMode::OnAutoTeeKeyPressed(EAutoTeeKey Key)
{
    UE_LOG(LogTemp, Log, TEXT("?? Key Pressed: %d"), (int32)Key);

    switch (Key)
    {
    case EAutoTeeKey::Up:
        // ? 연속 액션: 티 높이를 계속 올림
        if (AutoTeeController && AutoTeeController->IsConnected())
        {
            int32 NewHeight = FMath::Clamp(CurrentAutoTeeHeight + 5, 0, 60);
            AutoTeeController->SetTeeHeightAsync(NewHeight);
            UE_LOG(LogTemp, Log, TEXT("?? Tee Up: %d mm"), NewHeight);
        }
        break;

    case EAutoTeeKey::Down:
        // ? 연속 액션: 티 높이를 계속 내림
        if (AutoTeeController && AutoTeeController->IsConnected())
        {
            int32 NewHeight = FMath::Clamp(CurrentAutoTeeHeight - 5, 0, 60);
            AutoTeeController->SetTeeHeightAsync(NewHeight);
            UE_LOG(LogTemp, Log, TEXT("?? Tee Down: %d mm"), NewHeight);
        }
        break;

    case EAutoTeeKey::Left:
        // ? 연속 액션: 카메라를 계속 회전
        if (CanRotateCamera())
        {
            if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
            {
                if (AGolfPlayerController* GolfPC = Cast<AGolfPlayerController>(PC))
                {
                    GolfPC->RotateLeft();
                    UE_LOG(LogTemp, Verbose, TEXT("?? Rotate Left"));
                }
            }
        }
        break;

    case EAutoTeeKey::Right:
        // ? 연속 액션: 카메라를 계속 회전
        if (CanRotateCamera())
        {
            if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
            {
                if (AGolfPlayerController* GolfPC = Cast<AGolfPlayerController>(PC))
                {
                    GolfPC->RotateRight();
                    UE_LOG(LogTemp, Verbose, TEXT("?? Rotate Right"));
                }
            }
        }
        break;

        // ? 단발 액션들은 Release에서 처리
    case EAutoTeeKey::Grid:
    case EAutoTeeKey::Mulligan:
    case EAutoTeeKey::Function:
        // Press에서는 무시 (Release에서 처리)
        UE_LOG(LogTemp, VeryVerbose, TEXT("?? Single-action key pressed, waiting for release..."));
        break;

    default:
        break;
    }
}

bool AInGameMode::LoadSystemConfig()
{
    FString ConfigPath = FPaths::ProjectSavedDir() + TEXT("SystemConfig.json");
    bool bSuccess = UJsonHandler::LoadSystemConfigFromJson(SystemConfig, ConfigPath);

    if (!bSuccess)
    {
        UE_LOG(LogTemp, Warning, TEXT("?? Using default SystemConfig values"));
        // 기본값으로 저장
        SaveSystemConfig();
    }

    return bSuccess;
}

void AInGameMode::SaveSystemConfig()
{
    FString ConfigPath = FPaths::ProjectSavedDir() + TEXT("SystemConfig.json");
    UJsonHandler::SaveSystemConfigToJson(SystemConfig, ConfigPath);
}


// ? HoleMark 위치 업데이트 함수 구현
void AInGameMode::UpdateHoleMarkPosition()
{
    if (!IsValid(HoleMarkBillboard))
    {
        UE_LOG(LogTemp, Warning, TEXT("?? HoleMarkBillboard is not valid"));
        return;
    }

    // 현재 홀의 홀컵 위치 가져오기
    int32 HoleIndex = CurrentHole - 1;
    if (!MapInfo.HolecupPositions.IsValidIndex(HoleIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("? Invalid hole index: %d"), HoleIndex);
        HoleMarkBillboard->SetActorHiddenInGame(true);
        return;
    }

    FVector HolecupPosition = MapInfo.HolecupPositions[HoleIndex];

    // ? 홀컵 위 일정 높이에 배치 (200cm 위)
    FVector MarkPosition = HolecupPosition + FVector(0, 0, 400.0f);

    // 위치 설정
    HoleMarkBillboard->SetActorLocation(MarkPosition);

    // 마커 표시
    HoleMarkBillboard->SetActorHiddenInGame(false);

    UE_LOG(LogTemp, Log, TEXT("? HoleMark updated for Hole %d at position: %s"),
        CurrentHole, *MarkPosition.ToString());

    // ? 화면에 디버그 메시지 표시
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
            FString::Printf(TEXT("?? Hole %d Mark Updated"), CurrentHole));
    }

    // ? 홀 마크 위치 업데이트 후 UI도 함께 업데이트
    if (IsValid(StrokeWidgetInstance))
    {
        StrokeWidgetInstance->PositionCanvasPanelAboveHole();
    }
}

// 2026.01.03 추가
int32 AInGameMode::GetPhysicalHoleNum(int32 RoundHoleIdx, int32 Sublevel)
{
    switch (Sublevel)
    {
    case 1: // AB 코스
        return RoundHoleIdx;

    case 2: // CD 코스
        return RoundHoleIdx + 18;

    case 3: // AC 코스
        if (RoundHoleIdx <= 9) return RoundHoleIdx;      // 1~9 -> 1~9
        else                   return RoundHoleIdx + 9;  // 10~18 -> 19~27

    case 4: // BD 코스
        if (RoundHoleIdx <= 9) return RoundHoleIdx + 9;  // 1~9 -> 10~18
        else                   return RoundHoleIdx + 18; // 10~18 -> 28~36

    default:
        return RoundHoleIdx;
    }
}

void AInGameMode::UpdateHoleFlagDisplay()
{
    int32 PhysicalNum = GetPhysicalHoleNum(CurrentHole, GameInfo.SelectedMap.Sublevel);

    // =========================================================================
    // ① flag_hole 액터 탐색
    // =========================================================================
    FString FlagActorName = FString::Printf(TEXT("flag_hole%d"), PhysicalNum);
    AActor* FlagActor = FindActorByName(FlagActorName);

    if (!IsValid(FlagActor))
    {
        UE_LOG(LogGameMode, Warning,
            TEXT("[FlagDisplay] flag_hole%d 액터를 찾지 못했습니다."), PhysicalNum);
        return;
    }

    // =========================================================================
    // ② 현재 홀컵 위치 가져오기
    //    GameInfo.SelectedMap.HolecupPositions[CurrentHole - 1] 에
    //    Holecup_Position(0~4) 에 해당하는 위치가 이미 저장돼 있음
    // =========================================================================
    if (!GameInfo.SelectedMap.HolecupPositions.IsValidIndex(CurrentHole - 1))
    {
        UE_LOG(LogGameMode, Warning,
            TEXT("[FlagDisplay] HolecupPositions[%d] 유효하지 않음 (size=%d)"),
            CurrentHole - 1,
            GameInfo.SelectedMap.HolecupPositions.Num());
        return;
    }

    FVector HolecupPos = GameInfo.SelectedMap.HolecupPositions[CurrentHole - 1];

    // =========================================================================
    // ③ 깃발을 홀컵 위치로 이동
    //    Z 오프셋: 깃발 바닥이 홀컵 중심에 오도록 조정 (필요 시 수정)
    // =========================================================================
    const float FlagZOffset = 0.0f;  // 필요 시 조정 (예: 깃대가 땅속으로 들어가면 +10)
    FVector FlagTargetPos = HolecupPos + FVector(0.f, 0.f, FlagZOffset);

    // Movable 설정 (Static이면 이동 불가)
    if (USceneComponent* RootComp = FlagActor->GetRootComponent())
    {
        if (RootComp->Mobility != EComponentMobility::Movable)
        {
            RootComp->SetMobility(EComponentMobility::Movable);
        }
    }

    FlagActor->SetActorLocation(FlagTargetPos);

    UE_LOG(LogGameMode, Log,
        TEXT("[FlagDisplay] hole%d(Physical:%d) 깃발 이동 → %s"),
        CurrentHole, PhysicalNum, *FlagTargetPos.ToString());

    // =========================================================================
    // ④ Blueprint 변수 'hole' 메쉬 인덱스 동기화 (기존 로직 유지)
    // =========================================================================
    FProperty* HoleProp = FlagActor->GetClass()->FindPropertyByName(TEXT("hole"));
    if (HoleProp)
    {
        int32 CourseIdx = (PhysicalNum - 1) / 9;
        int32 SegmentOffset = (CurrentHole > 9) ? 9 : 0;
        int32 HoleOffset = (CurrentHole - 1) % 9;
        int32 FinalMeshIndex = (CourseIdx * 18) + SegmentOffset + HoleOffset;

        if (FByteProperty* ByteProp = CastField<FByteProperty>(HoleProp))
        {
            ByteProp->SetPropertyValue_InContainer(FlagActor, (uint8)FinalMeshIndex);
        }
        else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(HoleProp))
        {
            void* PropertyAddress = EnumProp->ContainerPtrToValuePtr<void>(FlagActor);
            EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(PropertyAddress, (int64)FinalMeshIndex);
        }

#if WITH_EDITOR
        FlagActor->RerunConstructionScripts();
#endif

        UE_LOG(LogGameMode, Log,
            TEXT("[FlagDisplay] 메쉬 인덱스 동기화 | Physical:%d Logical:%d → Course:%d Index:%d"),
            PhysicalNum, CurrentHole, CourseIdx, FinalMeshIndex);
    }
    else
    {
        UE_LOG(LogGameMode, Warning,
            TEXT("[FlagDisplay] flag_hole%d 에서 'hole' 프로퍼티를 찾지 못했습니다."),
            PhysicalNum);
    }
}


// ? HoleMark 표시/숨김 함수
void AInGameMode::ShowHoleMark(bool bShow)
{
    if (IsValid(HoleMarkBillboard))
    {
        HoleMarkBillboard->SetActorHiddenInGame(!bShow);
        UE_LOG(LogTemp, Log, TEXT("?? HoleMark visibility set to: %s"),
            bShow ? TEXT("Visible") : TEXT("Hidden"));
    }
}

// ? 회전 가능 여부 체크 함수
bool AInGameMode::CanRotateCamera()
{
    float CurrentTime = GetWorld()->GetTimeSeconds();
    float TimeSinceLastRotation = CurrentTime - LastRotationTime;

    if (TimeSinceLastRotation >= RotationCooldown)
    {
        LastRotationTime = CurrentTime;
        return true;
    }

    UE_LOG(LogTemp, VeryVerbose, TEXT("?? Camera rotation on cooldown: %.2f/%.2f"),
        TimeSinceLastRotation, RotationCooldown);
    return false;
}

void AInGameMode::ShowSwingMotion(bool bShow)
{
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
    {
        if (AGolfPlayerController* GolfPC = Cast<AGolfPlayerController>(PC))
        {
            if (bShow)
                GolfPC->ShowSwingVideoWidget();
            else
                GolfPC->PlayLastSwingReplay();




        }
    }

}

void AInGameMode::LoadWebcamConfig()
{
    FString ConfigPath = UWebcamConfigLoader::GetDefaultConfigPath();

    if (!UWebcamConfigLoader::ConfigFileExists(ConfigPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("?? Config file not found. Creating default."));
        WebcamSettings = UWebcamConfigLoader::GetDefaultSettings();
        UWebcamConfigLoader::SaveConfigToJSON(ConfigPath, WebcamSettings);
    }
    else
    {
        if (!UWebcamConfigLoader::LoadConfigFromJSON(ConfigPath, WebcamSettings))
        {
            UE_LOG(LogTemp, Error, TEXT("? Failed to load config. Using defaults."));
            WebcamSettings = UWebcamConfigLoader::GetDefaultSettings();
        }
    }
}

void AInGameMode::ResetRoundStatus()
{
    GameInfo.bIsRoundEnd = false;
    GameInfo.CurrentHole = 1;
    GameInfo.CurrentPlayerIndex = 0;
    SaveGameInfoToJSON();
    UE_LOG(LogGameMode, Log, TEXT("? Round status reset for new game"));
}


bool AInGameMode::CanResumeRound() const
{
    return (!GameInfo.bIsRoundEnd && GameInfo.CurrentHole >= 1);
}

// 1. Cup 액터의 메쉬를 설정하는 함수
void AInGameMode::SetupCupActorMesh(AActor* CupActor, int32 HoleNumber)
{
    if (!CupActor) return;

    // Mobility 설정
    //if (CupActor->GetRootComponent())
    //{
    //    CupActor->GetRootComponent()->SetMobility(EComponentMobility::Movable);
    //}

    // ? FindComponentByClass 사용 (이것만 바뀜!)
    UStaticMeshComponent* CupMeshComponent = CupActor->FindComponentByClass<UStaticMeshComponent>();

    if (!CupMeshComponent)
    {
        UE_LOG(LogGameMode, Warning, TEXT("? Static Mesh Component not found"));
        return;
    }

    FString CupPath = TEXT("StaticMesh'/Game/Landscape_Material/cup_in.cup_in'");

    // cup_in 메쉬 로드
    UStaticMesh* CupInMesh = LoadObject<UStaticMesh>(nullptr, *CupPath);

    if (!CupInMesh)
    {
        UE_LOG(LogGameMode, Warning, TEXT("? Failed to load cup_in mesh"));
        return;
    }

    // 메쉬 할당
    CupMeshComponent->SetStaticMesh(CupInMesh);
    UE_LOG(LogGameMode, Log, TEXT("? Cup mesh set for hole %d"), HoleNumber);


    if (!CupInMesh)
    {
        UE_LOG(LogGameMode, Warning, TEXT("? Failed to load cup_in mesh for hole %d"), HoleNumber);
        UE_LOG(LogGameMode, Warning, TEXT("   Attempted path: StaticMesh'/Game/model_data/hole_cup/cup_in'"));
        return;
    }

    UE_LOG(LogGameMode, Log, TEXT("? cup_in mesh loaded successfully: %s"),
        *CupInMesh->GetName());

    // ? Step 4: holecup 컴포넌트에 cup_in 메쉬 할당
    CupMeshComponent->SetStaticMesh(CupInMesh);
    UE_LOG(LogGameMode, Log, TEXT("? Cup mesh assigned to holecup component"));
    UE_LOG(LogGameMode, Log, TEXT("   Component: %s"), *CupMeshComponent->GetName());
    UE_LOG(LogGameMode, Log, TEXT("   Mesh: %s"), *CupInMesh->GetName());

    // ? Step 5: flag_hole%d 액터 찾기 및 위치로 이동
    FString FlagActorName = FString::Printf(TEXT("flag_hole%d"), HoleNumber);
    AActor* FlagActor = FindActorByName(FlagActorName);

    if (FlagActor)
    {

        // Mobility 설정
        if (FlagActor->GetRootComponent())
        {
            FlagActor->GetRootComponent()->SetMobility(EComponentMobility::Movable);
        }


        FVector OldPosition = CupActor->GetActorLocation();
        // Cup 액터를 flag 위치로 이동
        FlagActor->SetActorLocation(OldPosition);

        UE_LOG(LogGameMode, Log, TEXT("? Cup actor repositioned"));
        UE_LOG(LogGameMode, Log, TEXT("  Move Flag position: (%.2f, %.2f, %.2f)"),
            OldPosition.X, OldPosition.Y, OldPosition.Z);

        UE_LOG(LogGameMode, Log, TEXT("? Cup setup complete for hole %d\n"), HoleNumber);
    }
    else
    {
        UE_LOG(LogGameMode, Warning, TEXT("?? Flag actor not found: %s"), *FlagActorName);
        UE_LOG(LogGameMode, Warning, TEXT("   Cup mesh is set but position not updated\n"));
    }
}

// 2. 디버그용 함수 - Cup 액터의 모든 컴포넌트 출력
void AInGameMode::DebugCupComponents(AActor* CupActor)
{
    if (!CupActor) return;

    UE_LOG(LogGameMode, Warning, TEXT("=== Debug Cup Actor Components: %s ==="), *CupActor->GetName());

    TArray<UActorComponent*> AllComponents;
    CupActor->GetComponents(UActorComponent::StaticClass(), AllComponents);

    for (UActorComponent* Comp : AllComponents)
    {
        if (Comp)
        {
            UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(Comp);
            if (MeshComp)
            {
                FString MeshName = MeshComp->GetStaticMesh() ?
                    MeshComp->GetStaticMesh()->GetName() : TEXT("None");

                UE_LOG(LogGameMode, Warning,
                    TEXT("  └─ [StaticMesh] %s -> Mesh: %s"),
                    *Comp->GetName(), *MeshName);
            }
            else
            {
                UE_LOG(LogGameMode, Warning,
                    TEXT("  └─ [%s] %s"),
                    *Comp->GetClass()->GetName(),
                    *Comp->GetName());
            }
        }
    }
    UE_LOG(LogGameMode, Warning, TEXT("=== End Debug ==="));
}

// ─────────────────────────────────────────────────────────────────────────────
// [TTS 음성 안내 함수]
// ─────────────────────────────────────────────────────────────────────────────

void AInGameMode::AnnounceGameStart()
{
    if (!IsTTSReady())
    {
        UE_LOG(LogTemp, Warning, TEXT("[InGameMode] TTS가 준비되지 않았습니다"));
        return;
    }

    TTSManager.SetVolume(100);
    TTSManager.SetRate(0);

    SafeSpeak(TEXT("18홀 라운드를 시작합니다"));
    UE_LOG(LogTemp, Warning, TEXT("[InGameMode] 게임 시작 안내"));
}

void AInGameMode::AnnounceHole(int32 HoleNumber, int32 Par)
{
    if (!IsTTSReady())
    {
        return;
    }


    FString HoleAnnouncement = FString::Printf(
        TEXT("%d번 홀입니다. 파는 %d입니다."),
        HoleNumber,
        Par
    );

    SafeSpeak(HoleAnnouncement);
    UE_LOG(LogTemp, Warning, TEXT("[InGameMode] %d번 홀 안내 (파 %d)"), HoleNumber, Par);
}

void AInGameMode::AnnounceStroke(int32 StrokeCount)
{
    if (!IsTTSReady())
    {
        return;
    }


    FString StrokeAnnouncement = FString::Printf(
        TEXT("%d번 타격했습니다."),
        StrokeCount
    );

    // 짧은 안내는 약간 빠르게
    TTSManager.SetRate(2);
    SafeSpeak(StrokeAnnouncement);
    TTSManager.SetRate(0);  // 복구

    UE_LOG(LogTemp, Warning, TEXT("[InGameMode] %d번 타격"), StrokeCount);
}

void AInGameMode::AnnounceScore(int32 Score)
{
    if (!IsTTSReady())
    {
        return;
    }

    FString ScoreAnnouncement = FString::Printf(
        TEXT("현재 스코어는 %d입니다."),
        Score
    );

    SafeSpeak(ScoreAnnouncement);
    UE_LOG(LogTemp, Warning, TEXT("[InGameMode] 현재 스코어: %d"), Score);
}

void AInGameMode::AnnounceHoleComplete(int32 HoleNumber, const FString& Result)
{
    if (!IsTTSReady())
    {
        return;
    }

    FString CompleteAnnouncement = FString::Printf(
        TEXT("%d번 홀이 완료되었습니다. %s입니다."),
        HoleNumber,
        *Result
    );

    SafeSpeak(CompleteAnnouncement);
    UE_LOG(LogTemp, Warning, TEXT("[InGameMode] %d번 홀 완료 (%s)"), HoleNumber, *Result);
}

void AInGameMode::AnnounceGameEnd(int32 FinalScore)
{
    if (!IsTTSReady())
    {
        return;
    }

    TTSManager.SetVolume(100);
    TTSManager.SetRate(0);

    SafeSpeak(TEXT("라운드가 종료되었습니다."));

    // 잠시 대기
    FPlatformProcess::Sleep(1.5f);

    FString EndAnnouncement = FString::Printf(
        TEXT("최종 스코어는 %d입니다."),
        FinalScore
    );

    SafeSpeak(EndAnnouncement);
    UE_LOG(LogTemp, Warning, TEXT("[InGameMode] 게임 종료 (최종 스코어: %d)"), FinalScore);
}

void AInGameMode::Speak(const FString& Text)
{
    if (!IsTTSReady())
    {
        UE_LOG(LogTemp, Warning, TEXT("[InGameMode] TTS가 준비되지 않았습니다"));
        return;
    }

    SafeSpeak(Text);
    UE_LOG(LogTemp, Warning, TEXT("[InGameMode] 커스텀 음성: %s"), *Text);
}

void AInGameMode::StopSpeaking()
{
    TTSManager.Stop();
    UE_LOG(LogTemp, Warning, TEXT("[InGameMode] 음성 재생 중지"));
}

bool AInGameMode::IsTTSReady() const
{
    return TTSManager.IsInitialized();
}

bool AInGameMode::IsSpeaking() const
{
    return TTSManager.IsPlaying();
}

void AInGameMode::SetTTSVolume(int32 Volume)
{
    if (IsTTSReady())
    {
        TTSManager.SetVolume(Volume);
        UE_LOG(LogTemp, Warning, TEXT("[InGameMode] TTS 볼륨 설정: %d"), Volume);
    }
}

void AInGameMode::SetTTSRate(int32 Rate)
{
    if (IsTTSReady())
    {
        TTSManager.SetRate(Rate);
        UE_LOG(LogTemp, Warning, TEXT("[InGameMode] TTS 속도 설정: %d"), Rate);
    }
}

FString AInGameMode::GetTTSError() const
{
    return TTSManager.GetLastError();
}

// ─────────────────────────────────────────────────────────────────────────────
// [Private 헬퍼 함수]
// ─────────────────────────────────────────────────────────────────────────────

void AInGameMode::SetupTTS()
{
    if (TTSManager.IsInitialized())
    {
        UE_LOG(LogTemp, Warning, TEXT("[InGameMode] TTS 시스템 준비 완료"));

        // 초기 설정
        TTSManager.SetVolume(100);
        TTSManager.SetRate(0);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[InGameMode] TTS 시스템 초기화 실패"));
        FString Error = TTSManager.GetLastError();
        UE_LOG(LogTemp, Error, TEXT("[InGameMode] 에러: %s"), *Error);
    }
}

bool AInGameMode::SafeSpeak(const FString& Text)
{
    if (Text.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[InGameMode] 빈 텍스트 재생 시도"));
        return false;
    }

    // 1순위: Supertonic 온디바이스 TTS (준비됐을 때만)
    if (USupertonicTTSSubsystem* ST = USupertonicTTSSubsystem::Get(this))
    {
        if (ST->IsReady())
        {
            ST->SpeakDynamic(Text);   // 기본 화자/한국어. 화자 바꾸려면 두번째 인자 지정
            return true;
        }
    }

    // 2순위: 기존 SAPI 폴백 (Supertonic 미준비/비활성 시)
    if (!TTSManager.IsInitialized())
    {
        UE_LOG(LogTemp, Error, TEXT("[InGameMode] TTS 미초기화 상태에서 음성 재생 시도"));
        return false;
    }

    bool bSuccess = TTSManager.Speak(Text);
    if (!bSuccess)
    {
        FString Error = TTSManager.GetLastError();
        UE_LOG(LogTemp, Error, TEXT("[InGameMode] 음성 재생 실패: %s"), *Error);
    }
    return bSuccess;
}

void AInGameMode::LoadWidgetClasses()
{
    UE_LOG(LogGameMode, Warning, TEXT("?? LoadWidgetClasses: Start loading all widget classes"));

    // ? 이 함수는 반드시 게임 스레드에서 실행됨!
    check(IsInGameThread());

    // ============================================================================
    // 1. InGameScoreBoardWidget
    // ============================================================================
    if (!InGameScoreBoardWidgetClass)
    {
        InGameScoreBoardWidgetClass = LoadClass<UInGameScoreBoardWidget>(
            nullptr,
            TEXT("/Game/UMG/UI/InGame/Popup/ScoreBoard/WBP_InGame_ScoreBoard.WBP_InGame_ScoreBoard_C")  // ? "_C" 추가됨
        );
        if (InGameScoreBoardWidgetClass)
        {
            UE_LOG(LogGameMode, Log, TEXT("? InGameScoreBoardWidgetClass loaded"));
        }
        else
        {
            UE_LOG(LogGameMode, Error, TEXT("? Failed to load InGameScoreBoardWidgetClass"));
            UE_LOG(LogGameMode, Error, TEXT("   Path: /Game/UMG/UI/InGame/Popup/ScoreBoard/WBP_InGame_ScoreBoard.WBP_InGame_ScoreBoard_C"));
        }
    }

    if (!InGameScoreBoardStatWidgetClass)
    {
        InGameScoreBoardStatWidgetClass = LoadClass<UInGameScoreBoardStatWidget>(
            nullptr,
            TEXT("/Game/UMG/UI/InGame/Popup/ScoreBoard/WBP_InGame_ScoreBoard_Stat.WBP_InGame_ScoreBoard_Stat_C")  // ? "_C" 추가됨
        );
        if (InGameScoreBoardStatWidgetClass)
        {
            UE_LOG(LogGameMode, Log, TEXT("? InGameScoreBoardStatWidgetClass loaded"));
        }
        else
        {
            UE_LOG(LogGameMode, Error, TEXT("? Failed to load InGameScoreBoardStatWidgetClass"));
            UE_LOG(LogGameMode, Error, TEXT("   Path: /Game/UMG/UI/InGame/Popup/ScoreBoard/WBP_InGame_ScoreBoard_Stat.WBP_InGame_ScoreBoard_Stat_C"));
        }
    }
    // ============================================================================
    // 2. MiniMapWidget
    // ============================================================================
    if (!MiniMapWidgetClass)
    {
        MiniMapWidgetClass = LoadClass<UGolfMiniMap>(
            nullptr,
            TEXT("/Game/UMG/UI/InGame/Minimap/MiniMap.MiniMap_C")
        );
        if (MiniMapWidgetClass)
        {
            UE_LOG(LogGameMode, Log, TEXT("? MiniMapWidgetClass loaded"));
        }
        else
        {
            UE_LOG(LogGameMode, Warning, TEXT("?? Failed to load MiniMapWidgetClass, trying fallback"));
            LoadMiniMapWidgetClassFallback();
        }
    }

    // ============================================================================
    // 3. PlayerInfoSlotWidget
    // ============================================================================
    if (!PlayerInfoSlotWidgetClass)
    {
        PlayerInfoSlotWidgetClass = LoadClass<UPlayerInfoSlotWidget>(
            nullptr,
            TEXT("/Game/UMG/UI/InGame/Player/WBP_PlayerInfoSlot.WBP_PlayerInfoSlot_C")
        );
        if (PlayerInfoSlotWidgetClass)
        {
            UE_LOG(LogGameMode, Log, TEXT("? PlayerInfoSlotWidgetClass loaded"));
        }
        else
        {
            UE_LOG(LogGameMode, Error, TEXT("? Failed to load PlayerInfoSlotWidgetClass"));
        }
    }

    // ============================================================================
    // 4. StrokeMenuWidget
    // ============================================================================
    if (!StrokeMenuWidgetClass)
    {
        StrokeMenuWidgetClass = LoadClass<UStrokeMenuWidget>(
            nullptr,
            TEXT("/Game/UMG/UI/InGame/Popup/Menu/WBP_InGame_StrokeMenu.WBP_InGame_StrokeMenu_C")
        );
        if (StrokeMenuWidgetClass)
        {
            UE_LOG(LogGameMode, Log, TEXT("? StrokeMenuWidgetClass loaded"));
        }
        else
        {
            UE_LOG(LogGameMode, Error, TEXT("? Failed to load StrokeMenuWidgetClass"));
        }
    }

    // ============================================================================
    // 5. InGamePopupWidget
    // ============================================================================
    if (!InGamePopupWidgetClass)
    {
        InGamePopupWidgetClass = LoadClass<UUserWidget>(
            nullptr,
            TEXT("/Game/UMG/UI/InGame/Popup/WBP_InGame_Popup.WBP_InGame_Popup_C")
        );

        if (InGamePopupWidgetClass)
        {
            UE_LOG(LogGameMode, Log, TEXT("? InGamePopupWidgetClass loaded"));
        }
        else
        {
            UE_LOG(LogGameMode, Warning, TEXT("?? Failed to load InGamePopupWidgetClass (WBP_InGame_Popup)"));

            // Fallback 1: WBP_InGameMenuPopup 시도
            InGamePopupWidgetClass = LoadClass<UUserWidget>(
                nullptr,
                TEXT("/Game/UMG/UI/InGame/Popup/WBP_InGameMenuPopup.WBP_InGameMenuPopup_C")
            );

            if (InGamePopupWidgetClass)
            {
                UE_LOG(LogGameMode, Log, TEXT("? InGamePopupWidgetClass loaded (fallback: WBP_InGameMenuPopup)"));
            }
            else
            {
                UE_LOG(LogGameMode, Error, TEXT("? Failed to load InGamePopupWidgetClass (all attempts)"));
                UE_LOG(LogGameMode, Error, TEXT("   Tried:"));
                UE_LOG(LogGameMode, Error, TEXT("   1. /Game/UMG/UI/InGame/Popup/WBP_InGame_Popup.WBP_InGame_Popup_C"));
                UE_LOG(LogGameMode, Error, TEXT("   2. /Game/UMG/UI/InGame/Popup/WBP_InGameMenuPopup.WBP_InGameMenuPopup_C"));
            }
        }
    }
    // ============================================================================
    // 6. ResultWidget
    // ============================================================================
    if (!ResultWidgetClass)
    {
        ResultWidgetClass = LoadClass<UUserWidget>(
            nullptr,
            TEXT("/Game/365_widget/Result_Widget/result_widget.result_widget_C")
        );
        if (ResultWidgetClass)
        {
            UE_LOG(LogGameMode, Log, TEXT("? ResultWidgetClass loaded"));
        }
    }

    // ============================================================================
    // 7. ShotResultWidget
    // ============================================================================
    if (!ShotResultWidgetClass)
    {
        ShotResultWidgetClass = LoadClass<UUserWidget>(
            nullptr,
            TEXT("/Game/UMG/UI/InGame/Result/WBP_ShotResult.WBP_ShotResult_C")
        );
        if (ShotResultWidgetClass)
        {
            UE_LOG(LogGameMode, Log, TEXT("? ShotResultWidgetClass loaded"));
        }
    }

    // ============================================================================
    // 8. GameEndWidget
    // ============================================================================
    if (!GameEndWidgetClass)
    {
        GameEndWidgetClass = LoadClass<UUserWidget>(
            nullptr,
            TEXT("/Game/UMG/UI/InGame/WBP_GameEnd.WBP_GameEnd_C")
        );
        if (GameEndWidgetClass)
        {
            UE_LOG(LogGameMode, Log, TEXT("? GameEndWidgetClass loaded"));
        }
    }

    // ============================================================================
    // 9. InGamePlayerSelectWidget
    // ============================================================================
    if (!InGamePlayerSelectWidgetClass)
    {
        InGamePlayerSelectWidgetClass = LoadClass<UInGamePlayerSelectWidget>(
            nullptr,
            TEXT("/Game/UMG/UI/InGame/Popup/WBP_InGame_PlayerModify.WBP_InGame_PlayerModify_C")
        );
        if (InGamePlayerSelectWidgetClass)
        {
            UE_LOG(LogGameMode, Log, TEXT("? InGamePlayerSelectWidgetClass loaded"));
        }
    }

    // ============================================================================
    // 10. RangeHUDWidget
    // ============================================================================
    if (!RangeHUDWidgetclass && !RangeHUDWidgetPath.IsEmpty())
    {
        TSoftClassPtr<UUserWidget> SoftClass(RangeHUDWidgetPath);
        UClass* LoadedClass = SoftClass.LoadSynchronous();

        if (LoadedClass && LoadedClass->IsChildOf(URangeHUDWidget::StaticClass()))
        {
            RangeHUDWidgetclass = LoadedClass;
            UE_LOG(LogGameMode, Log, TEXT("? RangeHUDWidgetclass loaded: %s"), *LoadedClass->GetName());
        }
        else
        {
            UE_LOG(LogGameMode, Error, TEXT("? LoadedClass is not URangeHUDWidget child. Path=%s Class=%s"),
                *RangeHUDWidgetPath,
                LoadedClass ? *LoadedClass->GetName() : TEXT("NULL"));
        }
    }

    FSoftClassPath RangeHUDStatWidgetClassPath(TEXT("/Game/UMG/UI/InGame/Popup/Practice/WBP_RangeHUD_Stat.WBP_RangeHUD_Stat_C"));
    if (!RangeHUDStatWidgetClass)
    {
        TSoftClassPtr<UUserWidget> SoftClass(RangeHUDStatWidgetClassPath);
        if (UClass* LoadedClass = SoftClass.LoadSynchronous())
        {
            RangeHUDStatWidgetClass = LoadedClass;
            UE_LOG(LogGameMode, Log, TEXT("? RangeHUDStatWidgetClass loaded"));
        }
    }


    if (!BallDistanceWidgetClass)
    {
        BallDistanceWidgetClass = LoadClass<UBallDistanceWidget>(
            nullptr,
            TEXT("/Game/UMG/UI/InGame/WBP_BallDistance.WBP_BallDistance_C")  // ? "_C" 추가됨
        );
        if (BallDistanceWidgetClass)
        {
            UE_LOG(LogGameMode, Log, TEXT("? WBP_BallDistance loaded"));
        }
        else
        {
            UE_LOG(LogGameMode, Error, TEXT("? Failed to load WBP_BallDistance"));
            UE_LOG(LogGameMode, Error, TEXT("   Path: /Game/UMG/UI/InGame/WBP_BallDistance.WBP_BallDistance"));
        }
    }

    // ============================================================================
    // 11. 기타 Actor 클래스들
    // ============================================================================
    if (!HoleMarkBillboardClass)
    {
        HoleMarkBillboardClass = LoadClass<AActor>(
            nullptr,
            TEXT("/Game/1_mark/bp_holeMark.bp_holeMark_C")
        );
        if (HoleMarkBillboardClass)
        {
            UE_LOG(LogGameMode, Log, TEXT("? HoleMarkBillboardClass loaded"));
        }
    }


    if (!HoleTransitionWidgetClass)
    {
        HoleTransitionWidgetClass = LoadClass<UHoleTransitionWidget>(
            nullptr,
            TEXT("/Game/365_widget/transition_widget/transition.transition_C")
        );

        if (HoleTransitionWidgetClass)
        {
            UE_LOG(LogGameMode, Log, TEXT("✅ HoleTransitionWidgetClass loaded"));
        }
        else
        {
            UE_LOG(LogGameMode, Error, TEXT("❌ Failed to load HoleTransitionWidgetClass"));
            UE_LOG(LogGameMode, Error, TEXT("   Path: /Game/365_widget/transition_widget/transition.transition_C"));
        }
    }



    UE_LOG(LogGameMode, Warning, TEXT("? LoadWidgetClasses: All widget classes loaded successfully"));
}

void AInGameMode::SpawnPuttingGuide()
{
    if (!GetWorld())
    {
        UE_LOG(LogTemp, Error, TEXT("[AInGameMode] World is not valid"));
        return;
    }

    // 이미 생성되었으면 반환
    if (PuttingGuideActor)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AInGameMode] PuttingGuide already exists"));
        return;
    }

    // APuttingGuide 스폰
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = FName(TEXT("PuttingGuide_0"));
    SpawnParams.Owner = this;

    PuttingGuideActor = GetWorld()->SpawnActor<APuttingGuide>(
        APuttingGuide::StaticClass(),
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        SpawnParams
    );

    if (PuttingGuideActor)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AInGameMode] ? PuttingGuide spawned: %s"),
            *PuttingGuideActor->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[AInGameMode] ? Failed to spawn PuttingGuide"));
    }
}

void UStrokeWidget::TestPuttingGuidancePosition(float TestRightDistance)
{
    /**
     * 특정 좌우 거리에서 패널이 어느 위치에 배치되는지 테스트합니다.
     *
     * @param TestRightDistance 테스트할 오른쪽 거리 (cm)
     *
     * 사용 예:
     * - TestRightDistance = 0 (중앙) → 패널이 화면 중앙에 배치
     * - TestRightDistance = 1000 (우측 1m) → 패널이 화면 왼쪽에 배치
     * - TestRightDistance = -1000 (좌측 1m) → 패널이 화면 오른쪽에 배치
     */

    if (!Canvas_PuttingGuid)
    {
        UE_LOG(LogTemp, Error, TEXT("[DEBUG] Canvas_PuttingGuid not valid"));
        return;
    }

    APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
    if (!PlayerController || !PlayerController->PlayerCameraManager)
    {
        UE_LOG(LogTemp, Error, TEXT("[DEBUG] PlayerController not found"));
        return;
    }

    // ===== 테스트 설정 =====
    FVector CameraLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
    FVector CameraForwardVector = PlayerController->PlayerCameraManager->GetActorForwardVector();
    FVector CameraRightVector = PlayerController->PlayerCameraManager->GetActorRightVector();
    FVector CameraUpVector = PlayerController->PlayerCameraManager->GetActorUpVector();

    // 테스트: 카메라 앞 TestHoleDistance cm 지점
    float ForwardDist = TestHoleDistance;
    float RightDist = TestRightDistance;
    float UpDist = 0.0f;  // 높이는 0으로 (중앙 높이)

    // Viewport 크기
    FVector2D ViewportSizeTemp = FVector2D(GEngine->GameViewport->Viewport->GetSizeXY());
    ViewportSize = ViewportSizeTemp;

    // FOV 계산
    float FOVAngle = PlayerController->PlayerCameraManager->GetFOVAngle();
    float AspectRatio = ViewportSize.X / ViewportSize.Y;
    float HalfFOVTan = FMath::Tan(FMath::DegreesToRadians(FOVAngle * 0.5f));

    // 스크린 스페이스 계산
    float ScreenX = (RightDist / ForwardDist) / HalfFOVTan;
    float ScreenY = (UpDist / ForwardDist) / HalfFOVTan / AspectRatio;

    // 화면 좌표 변환
    FVector2D ScreenPosition;
    ScreenPosition.X = (ViewportSize.X * 0.5f) - (ScreenX * ViewportSize.X * 0.5f);
    ScreenPosition.Y = (ViewportSize.Y * 0.5f) - (ScreenY * ViewportSize.Y * 0.5f);

    // 패널 중앙 배치
    FVector2D PanelHalfSize = PuttingGuidancePanelSize / 2.0f;
    FVector2D AdjustedScreenPosition = ScreenPosition - PanelHalfSize;

    // 클램핑
    AdjustedScreenPosition.X = FMath::Clamp(AdjustedScreenPosition.X, 0.0f, ViewportSize.X - PuttingGuidancePanelSize.X);
    AdjustedScreenPosition.Y = FMath::Clamp(AdjustedScreenPosition.Y, 0.0f, ViewportSize.Y - PuttingGuidancePanelSize.Y);

    // ===== 디버그 로그 출력 =====
    UE_LOG(LogTemp, Warning, TEXT(""));
    UE_LOG(LogTemp, Warning, TEXT("??????????????????????????????????????????????????????????????????"));
    UE_LOG(LogTemp, Warning, TEXT("?              좌우 편차 패널 위치 테스트 결과                    ?"));
    UE_LOG(LogTemp, Warning, TEXT("??????????????????????????????????????????????????????????????????"));

    UE_LOG(LogTemp, Warning, TEXT("[TEST] ─────────────────────────────────────────────────────────"));
    UE_LOG(LogTemp, Warning, TEXT("[TEST] ?? 입력 파라미터"));
    UE_LOG(LogTemp, Warning, TEXT("[TEST]   RightDistance (좌우): %.0f cm"), TestRightDistance);
    UE_LOG(LogTemp, Warning, TEXT("[TEST]   ForwardDistance (앞뒤): %.0f cm"), ForwardDist);
    UE_LOG(LogTemp, Warning, TEXT("[TEST]   UpDistance (상하): %.0f cm"), UpDist);

    UE_LOG(LogTemp, Warning, TEXT("[TEST] ─────────────────────────────────────────────────────────"));
    UE_LOG(LogTemp, Warning, TEXT("[TEST] ?? 카메라 정보"));
    UE_LOG(LogTemp, Warning, TEXT("[TEST]   FOV: %.2f°"), FOVAngle);
    UE_LOG(LogTemp, Warning, TEXT("[TEST]   Aspect Ratio: %.2f"), AspectRatio);
    UE_LOG(LogTemp, Warning, TEXT("[TEST]   Viewport: (%.0f x %.0f)"), ViewportSize.X, ViewportSize.Y);

    UE_LOG(LogTemp, Warning, TEXT("[TEST] ─────────────────────────────────────────────────────────"));
    UE_LOG(LogTemp, Warning, TEXT("[TEST] ?? 계산 중간값"));
    UE_LOG(LogTemp, Warning, TEXT("[TEST]   ScreenX (정규화): %.4f"), ScreenX);
    UE_LOG(LogTemp, Warning, TEXT("[TEST]   ScreenY (정규화): %.4f"), ScreenY);

    UE_LOG(LogTemp, Warning, TEXT("[TEST] ─────────────────────────────────────────────────────────"));
    UE_LOG(LogTemp, Warning, TEXT("[TEST] ?? 최종 패널 위치"));
    UE_LOG(LogTemp, Warning, TEXT("[TEST]   중앙 기준 좌표: (%.0f, %.0f)"), ScreenPosition.X, ScreenPosition.Y);
    UE_LOG(LogTemp, Warning, TEXT("[TEST]   조정 후 좌표: (%.0f, %.0f)"), AdjustedScreenPosition.X, AdjustedScreenPosition.Y);
    UE_LOG(LogTemp, Warning, TEXT("[TEST]   패널 크기: (%.0f x %.0f)"), PuttingGuidancePanelSize.X, PuttingGuidancePanelSize.Y);

    UE_LOG(LogTemp, Warning, TEXT("[TEST] ─────────────────────────────────────────────────────────"));
    UE_LOG(LogTemp, Warning, TEXT("[TEST] ?? 화면 위치 해석"));

    float PercentageFromCenter = (AdjustedScreenPosition.X - (ViewportSize.X * 0.5f - PuttingGuidancePanelSize.X * 0.5f)) /
        (ViewportSize.X - PuttingGuidancePanelSize.X) * 100.0f;

    if (AdjustedScreenPosition.X < ViewportSize.X * 0.25f)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TEST]   ?? 위치: 화면 LEFT (좌측)"));
    }
    else if (AdjustedScreenPosition.X > ViewportSize.X * 0.75f)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TEST]   ?? 위치: 화면 RIGHT (우측)"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[TEST]   ?? 위치: 화면 CENTER (중앙)"));
    }

    UE_LOG(LogTemp, Warning, TEXT("[TEST] ─────────────────────────────────────────────────────────"));
    UE_LOG(LogTemp, Warning, TEXT("[TEST] ? 테스트 완료"));
    UE_LOG(LogTemp, Warning, TEXT(""));

    // 패널 배치
    ShowPuttingGuidancePanel(false);
    PositionPuttingGuidancePanel(AdjustedScreenPosition);
}


void UStrokeWidget::TestMultipleLateralDistances()
{
    /**
     * 여러 좌우 거리에서 패널 위치를 테스트합니다.
     * 좌우 편차에 따른 패널 움직임을 한 눈에 확인할 수 있습니다.
     *
     * 테스트 거리:
     * - 0 cm (중앙)
     * - ±500 cm (좌우 5m)
     * - ±1000 cm (좌우 10m)
     * - ±1500 cm (좌우 15m)
     */

    UE_LOG(LogTemp, Warning, TEXT(""));
    UE_LOG(LogTemp, Warning, TEXT("??????????????????????????????????????????????????????????????????"));
    UE_LOG(LogTemp, Warning, TEXT("?            여러 거리에서 패널 위치 테스트                      ?"));
    UE_LOG(LogTemp, Warning, TEXT("??????????????????????????????????????????????????????????????????"));
    UE_LOG(LogTemp, Warning, TEXT(""));

    // 테스트할 거리 배열
    TArray<float> TestDistances = {
        0.0f,      // 중앙
        500.0f,    // 우측 5m
        -500.0f,   // 좌측 5m
        1000.0f,   // 우측 10m
        -1000.0f,  // 좌측 10m
        1500.0f,   // 우측 15m
        -1500.0f   // 좌측 15m
    };

    FString DirectionText;
    for (int32 i = 0; i < TestDistances.Num(); ++i)
    {
        float Distance = TestDistances[i];

        if (Distance > 0.0f)
        {
            DirectionText = FString::Printf(TEXT("우측(RIGHT) %.0f cm"), Distance);
        }
        else if (Distance < 0.0f)
        {
            DirectionText = FString::Printf(TEXT("좌측(LEFT) %.0f cm"), FMath::Abs(Distance));
        }
        else
        {
            DirectionText = TEXT("중앙(CENTER)");
        }

        UE_LOG(LogTemp, Warning, TEXT("[TEST %d] ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"), i + 1);
        UE_LOG(LogTemp, Warning, TEXT("[TEST %d] 입력: %s"), i + 1, *DirectionText);

        // 테스트 실행
        TestPuttingGuidancePosition(Distance);

        FPlatformProcess::Sleep(0.1f);  // 짧은 지연 (로그 보기 좋게)
    }

    UE_LOG(LogTemp, Warning, TEXT(""));
    UE_LOG(LogTemp, Warning, TEXT("??????????????????????????????????????????????????????????????????"));
    UE_LOG(LogTemp, Warning, TEXT("?                    모든 테스트 완료! ?                        ?"));
    UE_LOG(LogTemp, Warning, TEXT("??????????????????????????????????????????????????????????????????"));
    UE_LOG(LogTemp, Warning, TEXT(""));
}


void AInGameMode::StoppingSensor()
{
    if (PlayerManager)
    {
        PlayerManager->OnLevelUnload();
    }



}



void AInGameMode::InitHoleTransitionWidget()
{
    if (!HoleTransitionWidgetClass) return;

    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!PC) return;

    HoleTransitionWidgetInstance = CreateWidget<UHoleTransitionWidget>(PC, HoleTransitionWidgetClass);
    if (HoleTransitionWidgetInstance)
    {
        HoleTransitionWidgetInstance->AddToViewport(6000); // 최상단
        HoleTransitionWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
        UE_LOG(LogTemp, Log, TEXT("✅ HoleTransitionWidget 초기화 완료"));
    }
}


void AInGameMode::PlayHoleTransition()
{
    if (!IsValid(HoleTransitionWidgetInstance))
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ HoleTransitionWidget 없음 → 바로 HoleInit 진행"));
        OnHoleTransitionFinished();
        return;
    }

    // 다음 홀 번호·파 정보 세팅
    const int32 NextHole = CurrentHole ;
    if (MapInfo.ParScores.IsValidIndex(NextHole - 1))
    {
        HoleTransitionWidgetInstance->SetHoleInfo(NextHole, MapInfo.ParScores[NextHole - 1]);
    }
    else
    {
        HoleTransitionWidgetInstance->SetHoleInfo(NextHole, 0); // 파 정보 없을 때 0
    }

    // 애니메이션 재생 (내부에서 Visible 처리 + 끝나면 OnTransitionFinished Broadcast)
    HoleTransitionWidgetInstance->PlayTransitionAnim();

    UE_LOG(LogTemp, Log, TEXT("▶️ HoleTransition 재생 시작 (Hole %d)"), NextHole);
}

void AInGameMode::OnHoleTransitionFinished()
{
    UE_LOG(LogTemp, Log, TEXT("✅ HoleTransition 완료 → HoleInit 진행"));

    // 위젯 숨기기는 UHoleTransitionWidget::NotifyTransitionFinished() 에서 처리됨
    // 여기서는 게임 상태만 전환
    ChangeGameState(EGameState::Game_HoleInit, 0.f);
}


#if WITH_EDITOR
void AInGameMode::AutoSetActorTags()
{
    UWorld* World = GetWorld();
    if (!World) return;

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);

    int32 TaggedCount = 0;

    for (AActor* Actor : AllActors)
    {
        if (!Actor) continue;

        FString Label = Actor->GetActorNameOrLabel();
        FString ClassName = Actor->GetClass()->GetName();

        // Tags에 이미 라벨과 동일한 태그가 있으면 스킵
        bool bAlreadyTagged = false;
        for (const FName& Tag : Actor->Tags)
        {
            if (Tag.ToString().Equals(Label, ESearchCase::IgnoreCase))
            {
                bAlreadyTagged = true;
                break;
            }
        }
        if (bAlreadyTagged) continue;

        // 대상 액터 판별: FindActorByName으로 탐색하는 이름 패턴들
        bool bShouldTag = false;

        // green_hole1~18, flag_hole1~18, Tee_hole1~18
        for (int32 i = 1; i <= 18; i++)
        {
            if (Label.Equals(FString::Printf(TEXT("green_hole%d"), i), ESearchCase::IgnoreCase) ||
                Label.Equals(FString::Printf(TEXT("flag_hole%d"), i), ESearchCase::IgnoreCase) ||
                Label.Equals(FString::Printf(TEXT("Tee_hole%d"), i), ESearchCase::IgnoreCase))
            {
                bShouldTag = true;
                break;
            }
        }

        // 연습장/Range 모드 액터
        if (!bShouldTag &&
            (Label.Equals(TEXT("put_startpoint"), ESearchCase::IgnoreCase) ||
                Label.Equals(TEXT("holecup"), ESearchCase::IgnoreCase) ||
                Label.Equals(TEXT("startpoint"), ESearchCase::IgnoreCase) ||
                Label.Equals(TEXT("endpoint"), ESearchCase::IgnoreCase)))
        {
            bShouldTag = true;
        }

        if (bShouldTag)
        {
            Actor->Tags.Add(FName(*Label));
            TaggedCount++;
            UE_LOG(LogGameMode, Log,
                TEXT("[AutoTag] ✅ '%s' (Class: %s) → Tag 추가됨"),
                *Label, *ClassName);

            // 변경사항을 에디터에 반영
            Actor->MarkPackageDirty();
        }
    }

    UE_LOG(LogGameMode, Warning,
        TEXT("[AutoTag] 완료: 총 %d개 액터에 Tag 추가됨. 레벨을 저장하세요!"),
        TaggedCount);
}
#endif



void AInGameMode::ShowChanceWidget(int32 Score)
{
    if (!IsStrokeMode()) return;

    int32 AdjustedScore = Score + 1;
    int32 FinalScore = AdjustedScore - GameInfo.SelectedMap.ParScores[CurrentHole - 1];
    UE_LOG(LogTemp, Log, TEXT("ShowChanceWidget: ChanceWidgetClass ->Score[%d] AdjustedScore[%d] -> Final[%d]"), Score, AdjustedScore, FinalScore);
    FString ChanceText;
    switch (FinalScore)
    {
    case 0: ChanceText = TEXT("파 찬스"); break;
    case -1: ChanceText = TEXT("버디 찬스"); break;
    case -2: ChanceText = TEXT("이글 찬스"); break;
    case -3: ChanceText = TEXT("알바트로스 찬스"); break;
    default:
        HideChanceWidget();
        return;
    }

    if (!ChanceWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("ShowChanceWidget: ChanceWidgetClass가 설정되지 않았습니다."));
        return;
    }

    // ⭐ 인스턴스가 없으면 먼저 생성
    if (!IsValid(ChanceWidgetInstance))
    {
        ChanceWidgetInstance = CreateWidget<UChanceWidget>(GetWorld(), ChanceWidgetClass);
        UE_LOG(LogTemp, Log, TEXT("ShowChanceWidget: ChanceWidgetInstance 재생성!!."));
    }

    if (IsValid(ChanceWidgetInstance))
    {
        ChanceWidgetInstance->SetChanceText(ChanceText);

        // ⭐ 매번 새로 추가 (이전에 제거되었으므로 상태가 깨끗함)
        if (!ChanceWidgetInstance->IsInViewport())
        {
            ChanceWidgetInstance->AddToViewport(100);
        }
        ChanceWidgetInstance->SetVisibility(ESlateVisibility::Visible);

        ChanceWidgetInstance->OnChanceShown(FinalScore);

        UE_LOG(LogTemp, Log, TEXT("ShowChanceWidget: ChanceWidgetClass -> SetText[%s]"), *ChanceText);

       // GetWorldTimerManager().ClearTimer(ChanceWidgetHideTimerHandle);
       // GetWorldTimerManager().SetTimer(ChanceWidgetHideTimerHandle, this, &AInGameMode::HideChanceWidget, 5.5f, false);
    }
}

void AInGameMode::HideChanceWidget()
{
    GetWorldTimerManager().ClearTimer(ChanceWidgetHideTimerHandle);
    if (IsValid(ChanceWidgetInstance))
    {
       // ChanceWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
        ChanceWidgetInstance->RemoveFromParent();  // ⭐ Collapsed 대신 뷰포트에서 제거
    }
}

void AInGameMode::OnResultVideoClosed()
{
    UE_LOG(LogTemp, Warning, TEXT("🎬 영상 종료(버튼 클릭): 다음 스테이트 진행"));

    // 중복 호출 방지: 델리게이트 해제
    if (ResultVideoWidgetInstance)
    {
        ResultVideoWidgetInstance->OnResultVideoClosed.RemoveDynamic(this, &AInGameMode::OnResultVideoClosed);
    }

    // 대기 중이던 턴 전환 실행
    if (PlayerManager)
    {
        PlayerManager->AdvanceTurn();
    }
}