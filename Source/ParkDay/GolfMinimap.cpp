#include "GolfMiniMap.h"
#include "CameraManager.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/SceneComponent.h"
#include "InGameMode.h"
#include "GolfBall.h"
#include "GolfPlayerController.h"
#include "GolfPlayerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"

UGolfMiniMap::UGolfMiniMap(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer) // ⭐ 이 부분이 올바르게 작성되어 있는지 다시 확인합니다.
{
    // UE4에서는 생성자에서 기본값 초기화
    MapWidth = 250.f;
    MapHeight = 500.f;
    VerticalViewPercentage = 0.1f;
    MapScale = 2.0f;
    CurrentHoleNumber = 1;
    AimLineLength = 100.0f;
    WorldToMapScale = 2.0f;
    CurrentDistance = 0.0f;
    CurrentElevation = 0.0f;
    bIsInitialized = false;
    bCaptureInitialized = false;
    // 미니맵 좌우/상하 뒤집기 기본값
    bFlipMapHorizontally = true;
    bFlipMapVertically = false;

    // 맵 캡처 관련 기본값
    CaptureHeight = 2000.0f;
    CaptureOrthoWidth = 2000.0f;
    RenderTargetResolution = 475.f;
    bAutoRefreshCapture = false;
    RefreshInterval = 5.0f;

    // 색상 기본값
    TeeColor = FLinearColor::Blue;
    HolecupColor = FLinearColor::Red;


    AimLineColor = FLinearColor::Yellow;
    FlagColor = FLinearColor::Red;
    BackgroundColor = FLinearColor(0.1f, 0.3f, 0.1f, 0.8f);


    // UE4에서는 nullptr로 초기화
    GameMode = nullptr;
    MiniMapCanvas = nullptr;
    BackgroundImage = nullptr;
    TeeImage = nullptr;
    HolecupImage = nullptr;



    FlagImage = nullptr;
    HoleNumberText = nullptr;
    DistanceText = nullptr;
    ElevationText = nullptr;
    WindInfoText = nullptr;
    ParInfoText = nullptr;
    OBLinesCanvas = nullptr; // OBLinesCanvas 초기화 누락되어 추가
    OBMaskOverlayImage = nullptr;
    OBMaskTexture = nullptr;

    // 캡처 관련 컴포넌트들
    CaptureActor = nullptr;
    SceneCaptureComponent = nullptr;
    MapRenderTarget = nullptr;
    CaptureMaterial = nullptr;

    // 추가된 TMap 멤버들 초기화
    PlayerBallImages.Empty();
    PlayerBallColors.Empty();
    PlayerBallWorldPositions.Empty();
    PlayerAimDirections.Empty();
    PlayerManager = nullptr;

    // PlayerBallTextures 배열 초기화
    PlayerBallTextures.SetNum(MaxPlayerCount);

    // 모든 플레이어 볼 텍스처 로드
    for (int32 i = 0; i < MaxPlayerCount; i++)
    {
        FString TexturePath = FString::Printf(TEXT("/Game/GolfGame/Image/mini_%02d"), i + 1);

        // ConstructorHelpers를 사용한 정적 로드
        UTexture2D* LoadedTexture = nullptr;

        switch (i)
        {
        case 0:
        {
            static ConstructorHelpers::FObjectFinder<UTexture2D> Texture1(TEXT("/Game/GolfGame/Image/mini_01"));
            if (Texture1.Succeeded()) LoadedTexture = Texture1.Object;
        }
        break;
        case 1:
        {
            static ConstructorHelpers::FObjectFinder<UTexture2D> Texture2(TEXT("/Game/GolfGame/Image/mini_02"));
            if (Texture2.Succeeded()) LoadedTexture = Texture2.Object;
        }
        break;
        case 2:
        {
            static ConstructorHelpers::FObjectFinder<UTexture2D> Texture3(TEXT("/Game/GolfGame/Image/mini_03"));
            if (Texture3.Succeeded()) LoadedTexture = Texture3.Object;
        }
        break;
        case 3:
        {
            static ConstructorHelpers::FObjectFinder<UTexture2D> Texture4(TEXT("/Game/GolfGame/Image/mini_04"));
            if (Texture4.Succeeded()) LoadedTexture = Texture4.Object;
        }
        break;
        case 4:
        {
            static ConstructorHelpers::FObjectFinder<UTexture2D> Texture5(TEXT("/Game/GolfGame/Image/mini_05"));
            if (Texture5.Succeeded()) LoadedTexture = Texture5.Object;
        }
        break;
        case 5:
        {
            static ConstructorHelpers::FObjectFinder<UTexture2D> Texture6(TEXT("/Game/GolfGame/Image/mini_06"));
            if (Texture6.Succeeded()) LoadedTexture = Texture6.Object;
        }
        break;
        }

        PlayerBallTextures[i] = LoadedTexture;

        if (LoadedTexture)
        {
            UE_LOG(LogTemp, Log, TEXT("Loaded texture for player %d: %s"), i, *TexturePath);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Failed to load texture for player %d: %s"), i, *TexturePath);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Loaded %d/%d player ball textures"), GetLoadedTextureCount(), MaxPlayerCount);


    // AimActor 텍스처 로드 (별도 텍스처가 있다면)
    static ConstructorHelpers::FObjectFinder<UTexture2D> AimActorTextureAsset(TEXT("/Game/GolfGame/Image/aim_icon"));
    if (AimActorTextureAsset.Succeeded())
    {
        AimActorTexture = AimActorTextureAsset.Object;
    }

    // TMap 초기화
    PlayerAimActorImages.Empty();
    PlayerAimActorPositions.Empty();
    PlayerBallToAimLineImages.Empty();
    PlayerAimToHoleLineImages.Empty();
}

void UGolfMiniMap::NativeConstruct()
{
    UE_LOG(LogTemp, Warning, TEXT("🚀 MiniMap NativeConstruct started"));

    // 기본 설정
    SetVisibility(ESlateVisibility::Visible);

    // 게임 모드 참조 획득
    GameMode = Cast<AInGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
    if (!GameMode)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ GameMode not found"));
        return;
    }

    // 미니맵 배경 설정
    if (BackgroundImage)
    {
        BackgroundImage->SetColorAndOpacity(BackgroundColor);
    }

    // 미니맵 캔버스 크기 설정
    if (MiniMapCanvas)
    {
        MiniMapCanvas->SetClipping(EWidgetClipping::ClipToBounds);
        UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(MiniMapCanvas->Slot);
        if (CanvasSlot)
        {
            CanvasSlot->SetSize(FVector2D(MapWidth, MapHeight));
        }
    }

    // 🔧 캡처 컴포넌트 생성 (즉시 실행)
    //CreateCaptureComponents();
    CreateCaptureComponentsInline();

    // 홀 정보 초기화
    if (GameMode->MapInfo.TeePositions.IsValidIndex(GameMode->CurrentHole - 1) &&
        GameMode->MapInfo.HolecupPositions.IsValidIndex(GameMode->CurrentHole - 1))
    {
        FVector TeePos = GameMode->MapInfo.TeePositions[GameMode->CurrentHole - 1];
        FVector HolecupPos = GameMode->MapInfo.HolecupPositions[GameMode->CurrentHole - 1];
        InitializeMiniMap(TeePos, HolecupPos);
        SetCurrentHole(GameMode->CurrentHole);
    }

    // 플레이어 매니저 설정
    if (GameMode)
    {
        PlayerManager = GameMode->PlayerManager;
        if (GameMode->GetCurrentGameMode() != EGolfGameMode::StrokeMode)
            CanvasPanel_Tip_2->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (PlayerManager)
    {
        for (AGolfBall* Ball : PlayerManager->GetPlayerBalls())
        {
            if (IsValid(Ball))
            {
                AddPlayerToMiniMap(Ball->OwningPlayerIndex, Ball->GetActorLocation(), Ball->GetBallColor());

                // 볼-홀컵 라인도 즉시 생성
                CreateBallToHoleLineForPlayer(Ball->OwningPlayerIndex);
                UpdateBallToHoleLinePosition(Ball->OwningPlayerIndex);

                UE_LOG(LogTemp, Log, TEXT("Initialized player %d with all line systems"), Ball->OwningPlayerIndex);
            }
        }
    }


    if (GameMode && GameMode->PlayerManager && GameMode->PlayerManager->GetPlayerBalls().Num() > 0)
    {
        // 타이머를 사용하여 위젯이 완전히 초기화된 후 호출
        GetWorld()->GetTimerManager().SetTimer(
            Tip2InitTimer,
            [this]()
            {
                if (IsValid(this) &&
                    TextBlock_Tip2_Distance_Pick &&
                    TextBlock_Tip2_Distance_PickToHole &&
                    TextBlock_Tip2_Height)
                {
                    InitTip2();
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("❌ Cannot initialize Tip2: Widget components not ready"));
                }
            },
            0.1f,  // 0.1초 지연
            false
        );
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Skipping InitTip2: GameMode or balls not ready"));
    }

    // 🔧 캡처 실행을 2초 후로 지연 (씬이 완전히 로드된 후)
    GetWorld()->GetTimerManager().SetTimer(
        DelayedCaptureTimer,
        [this]() {
            if (bCaptureInitialized)
            {
                UpdateImprovedCaptureCamera();

                // 추가 지연 후 캡처 실행
                GetWorld()->GetTimerManager().SetTimer(
                    DelayedCaptureTimer,
                    [this]() {
                        CaptureMapBackground();
                    },
                    0.5f,
                    false
                );
            }
        },
        2.0f,
        false
    );


    // 생성자에서 로드되지 않은 텍스처들 런타임 로드 시도
    LoadAllPlayerBallTextures();

    UE_LOG(LogTemp, Warning, TEXT("MiniMap NativeConstruct completed, Loaded textures: %d/%d"),
        GetLoadedTextureCount(), MaxPlayerCount);


}

// 새로운 인라인 함수 추가
void UGolfMiniMap::CreateCaptureComponentsInline()
{
    if (!GetWorld())
        return;

    // Render Target 생성 (⭐ MapWidth/MapHeight 비율을 유지하도록 통일)
    MapRenderTarget = NewObject<UTextureRenderTarget2D>(this);
    if (MapRenderTarget)
    {
        const FIntPoint RTSize = CalculateRenderTargetSize(RenderTargetResolution);
        MapRenderTarget->InitAutoFormat(RTSize.X, RTSize.Y);
        MapRenderTarget->UpdateResourceImmediate(true);
        MapRenderTarget->RenderTargetFormat = RTF_RGBA8;
        MapRenderTarget->bAutoGenerateMips = false;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to create MapRenderTarget"));
        return;
    }

    // 캡처 액터 스폰
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = nullptr;
    SpawnParams.Instigator = nullptr;
    SpawnParams.Name = TEXT("MiniMapCaptureActor");

    CaptureActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), SpawnParams);
    if (CaptureActor)
    {
        USceneComponent* RootComp = NewObject<USceneComponent>(CaptureActor, TEXT("RootComponent"));
        CaptureActor->SetRootComponent(RootComp);
        RootComp->RegisterComponent();

        // Scene Capture Component 생성
        SceneCaptureComponent = NewObject<USceneCaptureComponent2D>(CaptureActor, TEXT("SceneCaptureComponent"));
        if (SceneCaptureComponent)
        {
            SceneCaptureComponent->SetupAttachment(CaptureActor->GetRootComponent());
            SceneCaptureComponent->RegisterComponent();

            // 캡처 설정
            SceneCaptureComponent->TextureTarget = MapRenderTarget;
            SceneCaptureComponent->ProjectionType = ECameraProjectionMode::Perspective;
            SceneCaptureComponent->FOVAngle = 60.0f;
            SceneCaptureComponent->bCaptureEveryFrame = false;
            SceneCaptureComponent->bCaptureOnMovement = false;
            SceneCaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
            SceneCaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_LegacySceneCapture;

            // ⭐ 그림자 제거 설정 추가
            DisableShadowsForCapture();

            bCaptureInitialized = true;
            UE_LOG(LogTemp, Warning, TEXT("✅ SceneCapture component initialized (shadows disabled)"));
        }
    }
}


void UGolfMiniMap::DisableShadowsForCapture()
{
    if (!SceneCaptureComponent)
        return;

    UE_LOG(LogTemp, Log, TEXT("🌞 Disabling shadows for minimap capture..."));

    // 모든 그림자 타입 비활성화
    SceneCaptureComponent->ShowFlags.SetDynamicShadows(false);        // 동적 그림자
    SceneCaptureComponent->ShowFlags.SetContactShadows(false);        // 접촉 그림자
    SceneCaptureComponent->ShowFlags.SetCapsuleShadows(false);        // 캡슐 그림자
    SceneCaptureComponent->ShowFlags.SetShadowFrustums(false);        // 그림자 프러스텀

    // 추가 그림자 관련 설정
    SceneCaptureComponent->ShowFlags.SetStaticMeshes(true);           // 스태틱 메시는 표시
    SceneCaptureComponent->ShowFlags.SetLandscape(true);              // 랜드스케이프 표시
    SceneCaptureComponent->ShowFlags.SetFog(false);                   // 안개 제거 (선택사항)
    SceneCaptureComponent->ShowFlags.SetAtmosphere(false);            // 대기 효과 제거 (선택사항)

    // 더 밝고 깔끔한 이미지를 위한 추가 설정
    SceneCaptureComponent->ShowFlags.SetAmbientOcclusion(false);      // AO 제거
    SceneCaptureComponent->ShowFlags.SetTemporalAA(false);            // TAA 제거
    SceneCaptureComponent->ShowFlags.SetMotionBlur(false);            // 모션 블러 제거

    // PostProcess 비활성화 (더 깔끔한 이미지)
    SceneCaptureComponent->PostProcessBlendWeight = 0.0f;


    // 추가: 밝기 조정
    FPostProcessSettings& PPSettings = SceneCaptureComponent->PostProcessSettings;
    PPSettings.bOverride_AutoExposureBias = true;
    PPSettings.AutoExposureBias = 1.0f;  // 밝기 증가 (0.5 ~ 2.0 범위에서 조정)

    // SceneCaptureComponent->PostProcessBlendWeight = 1.0f;  // 이 경우 다시 활성화

    UE_LOG(LogTemp, Log, TEXT("✅ Shadows disabled for clean minimap"));
}


void UGolfMiniMap::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!bIsInitialized || !GameMode)
        return;

    // 현재 플레이어 인덱스 체크
    int32 CurrentPlayerIndex = GameMode->CurrentPlayerIndex;

    // 플레이어 변경 감지
    if (LastDisplayedPlayerIndex != CurrentPlayerIndex && LastDisplayedPlayerIndex >= 0)
    {
        OnPlayerTurnChanged(CurrentPlayerIndex, LastDisplayedPlayerIndex);
    }


    if (PlayerBallToAimLineImages.Contains(CurrentPlayerIndex))
    {
        UpdateBallToAimLinePosition(CurrentPlayerIndex);
    }

    if (PlayerAimToHoleLineImages.Contains(CurrentPlayerIndex))
    {
        UpdateAimToHoleLinePosition(CurrentPlayerIndex);
    }

    // 현재 플레이어 볼 위치 업데이트
    if (PlayerManager)
    {
        TArray<AGolfBall*> PlayerBalls = PlayerManager->GetPlayerBalls();

        // 현재 플레이어 볼만 업데이트
        if (PlayerBalls.IsValidIndex(CurrentPlayerIndex))
        {
            AGolfBall* CurrentBall = PlayerBalls[CurrentPlayerIndex];
            if (IsValid(CurrentBall))
            {
                UpdateBallPosition(CurrentPlayerIndex, CurrentBall->GetActorLocation());

                // 볼-홀컵 라인 업데이트 (현재 플레이어만)
                if (bShowBallToHoleLine)
                {
                    UpdateBallToHoleLinePosition(CurrentPlayerIndex);
                }

                // 거리/고도차 정보 업데이트 (현재 플레이어만)
                float Distance = FVector::Dist(CurrentBall->GetActorLocation(), HolecupWorldPosition);
                float Elevation = HolecupWorldPosition.Z - CurrentBall->GetActorLocation().Z;
                UpdateDistanceAndElevation(Distance, Elevation);
            }
        }
    }

    // AimActor 위치 업데이트 (현재 플레이어만)
    if (GameMode->bClickedMinimap && !GameMode->AimLocation.IsZero())
    {
        UpdateAimActorPosition(CurrentPlayerIndex, GameMode->AimLocation);
    }
}

void UGolfMiniMap::MoveToMouseTip2(FVector2D ViewportPos)
{
    if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(CanvasPanel_Tip_2->Slot))
    {
        CanvasSlot->SetPosition(ViewportPos);
    }
}

void UGolfMiniMap::UpdateTip2()
{
    // ⭐ 추가: NULL 체크
    if (!TextBlock_Tip2_Distance_Pick || !TextBlock_Tip2_Distance_PickToHole || !TextBlock_Tip2_Height)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ UpdateTip2: Text blocks are NULL"));
        return;
    }

    AInGameMode* GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    if (GM)
    {
        if (GM->GetCurrentGameMode() != EGolfGameMode::StrokeMode)
            return;

        if (GM->GetCurrentTurnGolfBall() && GM->MapInfo.HolecupPositions.IsValidIndex(GM->CurrentHole - 1))
        {
            FVector BallLocation = GM->GetCurrentTurnGolfBall()->GetActorLocation();
            FVector HoleLocation = GM->MapInfo.HolecupPositions[GM->CurrentHole - 1];
            FVector TargetWorld = GM->AimLocation;

            float PickDistance = FVector::Dist(BallLocation, TargetWorld) / 100.f;
            float PickToHoleDistance = FVector::Dist(TargetWorld, HoleLocation) / 100.f;
            float Height = (TargetWorld.Z - BallLocation.Z) * 0.01f;

            if (PickToHoleDistance < 0.6f)
            {
                PickToHoleDistance = 0.0f;

            }
            //PickDistance -= 0.5f;

            FText PickText = FText::FromString(FString::Printf(TEXT("%.1f m"), PickDistance));
            FText PickToHoleText = FText::FromString(FString::Printf(TEXT("%.1f m"), PickToHoleDistance));

            FString PlusMinus = Height >= 0 ? "+" : "";

            FText HeightText = FText::FromString(FString::Printf(TEXT("%s%.1f m"), *PlusMinus, Height));

            TextBlock_Tip2_Distance_Pick->SetText(PickText);
            TextBlock_Tip2_Distance_PickToHole->SetText(PickToHoleText);
            TextBlock_Tip2_Height->SetText(HeightText);
        }
    }
}

void UGolfMiniMap::InitTip2()
{
    // 가장 기본적인 this 포인터 체크
    if (!this)
    {
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("🔍 InitTip2 started - step 1: this pointer valid"));

    // UObject 유효성 체크
    if (!IsValid(this))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ InitTip2: this object is not valid"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("🔍 InitTip2 step 2: UObject is valid"));

    // World 체크를 더 안전하게
    UWorld* World = nullptr;
    if (HasAnyFlags(RF_ClassDefaultObject))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ InitTip2: Called on CDO"));
        return;
    }

    try
    {
        World = GetWorld();
    }
    catch (...)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ InitTip2: Exception in GetWorld()"));
        return;
    }

    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ InitTip2: World is NULL"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("🔍 InitTip2 step 3: World is valid"));

    // 텍스트 블록 체크
    if (!TextBlock_Tip2_Distance_Pick)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ InitTip2: TextBlock_Tip2_Distance_Pick is NULL"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("🔍 InitTip2 step 4: TextBlock_Tip2_Distance_Pick is valid"));

    if (!IsValid(TextBlock_Tip2_Distance_Pick))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ InitTip2: TextBlock_Tip2_Distance_Pick is not valid"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("🔍 InitTip2 step 5: TextBlock_Tip2_Distance_Pick passed IsValid"));

    if (!TextBlock_Tip2_Distance_PickToHole)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ InitTip2: TextBlock_Tip2_Distance_PickToHole is NULL"));
        return;
    }

    if (!IsValid(TextBlock_Tip2_Distance_PickToHole))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ InitTip2: TextBlock_Tip2_Distance_PickToHole is not valid"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("🔍 InitTip2 step 6: TextBlock_Tip2_Distance_PickToHole is valid"));

    if (!TextBlock_Tip2_Height)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ InitTip2: TextBlock_Tip2_Height is NULL"));
        return;
    }

    if (!IsValid(TextBlock_Tip2_Height))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ InitTip2: TextBlock_Tip2_Height is not valid"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("🔍 InitTip2 step 7: All text blocks are valid"));

    // GameMode 체크
    AGameModeBase* GameModeBase = nullptr;
    try
    {
        GameModeBase = World->GetAuthGameMode();
    }
    catch (...)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ InitTip2: Exception in GetAuthGameMode"));
        return;
    }

    AInGameMode* GM = Cast<AInGameMode>(GameModeBase);
    if (!GM)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ InitTip2: GameMode cast failed, using defaults"));
        SetTip2SafeDefaults();
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("🔍 InitTip2 step 8: GameMode is valid"));

    // 나머지 로직은 안전하게 진행
    float PickDistance = 0.f;
    float PickToHoleDistance = 0.f;
    float Height = 0.f;

    // 안전한 텍스트 설정
    SetTip2SafeDefaults();

    UE_LOG(LogTemp, Log, TEXT("✅ InitTip2 completed successfully"));
}


void UGolfMiniMap::SetTip2SafeDefaults()
{
    if (!this || !IsValid(this))
    {
        return;
    }

    try
    {
        FText DefaultText = FText::FromString(TEXT("0.0 m"));
        FText HeightText = FText::FromString(TEXT("+0.0 m"));

        if (TextBlock_Tip2_Distance_Pick && IsValid(TextBlock_Tip2_Distance_Pick))
        {
            TextBlock_Tip2_Distance_Pick->SetText(DefaultText);
            UE_LOG(LogTemp, VeryVerbose, TEXT("✅ Set Pick distance text"));
        }

        if (TextBlock_Tip2_Distance_PickToHole && IsValid(TextBlock_Tip2_Distance_PickToHole))
        {
            TextBlock_Tip2_Distance_PickToHole->SetText(DefaultText);
            UE_LOG(LogTemp, VeryVerbose, TEXT("✅ Set PickToHole distance text"));
        }

        if (TextBlock_Tip2_Height && IsValid(TextBlock_Tip2_Height))
        {
            TextBlock_Tip2_Height->SetText(HeightText);
            UE_LOG(LogTemp, VeryVerbose, TEXT("✅ Set Height text"));
        }
    }
    catch (...)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Exception in SetTip2SafeDefaults"));
    }
}

void UGolfMiniMap::SetTip2DefaultValues()
{
    if (!TextBlock_Tip2_Distance_Pick || !TextBlock_Tip2_Distance_PickToHole || !TextBlock_Tip2_Height)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Cannot set default values: Text blocks are NULL"));
        return;
    }

    try
    {
        FText DefaultText = FText::FromString(TEXT("0.0 m"));

        TextBlock_Tip2_Distance_Pick->SetText(DefaultText);
        TextBlock_Tip2_Distance_PickToHole->SetText(DefaultText);
        TextBlock_Tip2_Height->SetText(FText::FromString(TEXT("+0.0 m")));

        UE_LOG(LogTemp, Log, TEXT("✅ Default values set for Tip2"));
    }
    catch (...)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Exception occurred while setting default values"));
    }
}

void UGolfMiniMap::InitializeMiniMap(const FVector& TeePosition, const FVector& HolecupPosition)
{

    UE_LOG(LogTemp, Log, TEXT("🔴 UGolfMiniMap::InitializeMiniMap for hole %d"), GameMode->CurrentHole);

    TeeWorldPosition = TeePosition;
    HolecupWorldPosition = HolecupPosition;

    CaptureHeight = 2000.0f;

    CalculateHoleRotationAngle();
    MapCenterWorldPosition = CalculateImprovedMapCenter();
    WorldToMapScale = CalculateImprovedWorldToMapScale();

    //// ⭐ bCaptureInitialized 여부와 관계없이 CaptureWorldSize를 선계산
    //// (UpdateImprovedCaptureCamera와 동일하게 WorldToMapScale 기반으로 계산, OB는 아직 로드 전이라 티/홀컵만 사용)
    //// ⭐ Orthographic 캡처이므로 FOV/거리 역산이 필요 없음 — CaptureHeight는 단순 clearance 용도
    {
        float AspectRatio = MapWidth / MapHeight;

        // ⭐ 축척 → 캡처 반경 역산 (UpdateImprovedCaptureCamera와 동일한 공식)
        float DesiredCaptureRadius = (MapHeight * 0.5f) / FMath::Max(WorldToMapScale, KINDA_SMALL_NUMBER);

        // 티/홀컵 기준 BBox로 최소 필요 반경 산출 (OB 없이 기본값)
        float MaxNeedX = 0.f, MaxNeedY = 0.f;
        TArray<FVector> BasePoints = { TeeWorldPosition, HolecupWorldPosition };
        for (const FVector& Point : BasePoints)
        {
            FVector2D Rel(Point.X - MapCenterWorldPosition.X, Point.Y - MapCenterWorldPosition.Y);
            FVector2D Rotated = RotatePoint(Rel, -HoleRotationAngle);
            MaxNeedX = FMath::Max(MaxNeedX, FMath::Abs(Rotated.X) / AspectRatio);
            MaxNeedY = FMath::Max(MaxNeedY, FMath::Abs(Rotated.Y));
        }
        float MinRequiredRadius = FMath::Max(MaxNeedX, MaxNeedY) * 1.1f;
        float RequiredRadius = FMath::Max(DesiredCaptureRadius, MinRequiredRadius) * FMath::Max(CaptureViewMargin, 1.0f);

        CaptureHeight = FMath::Max(CaptureHeight, 3000.0f);
        CaptureWorldSize = RequiredRadius * 2.0f;  // ← 여기서 미리 설정
        CaptureWorldCenter = MapCenterWorldPosition;
    }


    bIsInitialized = true;

    // UI 요소들 위치 설정
   // if (TeeImage)
    //    UpdateImagePosition(TeeImage, TeeWorldPosition, TeeColor);


    // 모든 플레이어 볼을 미니맵에 추가
    if (PlayerManager)
    {
        for (AGolfBall* Ball : PlayerManager->GetPlayerBalls())
        {
            if (IsValid(Ball))
            {
                AddPlayerToMiniMap(Ball->OwningPlayerIndex, Ball->GetActorLocation(), Ball->GetBallColor());

                // ⭐ 볼-홀컵 라인도 즉시 생성
                CreateBallToHoleLineForPlayer(Ball->OwningPlayerIndex);
                UpdateBallToHoleLinePosition(Ball->OwningPlayerIndex);
            }
        }
    }

    if (GameMode)
    {
        UE_LOG(LogTemp, Log, TEXT("🔴 Minimap  GameMode  hole %d"), GameMode->CurrentHole);

        int32 CurrentHoleIndex = GameMode->CurrentHole - 1;
        if (GameMode->MapInfo.OBLines.IsValidIndex(CurrentHoleIndex))
        {
            const TArray<FVector>& OBPoints = GameMode->MapInfo.OBLines[CurrentHoleIndex].Points;
            if (OBPoints.Num() >= 3)
            {
                UE_LOG(LogTemp, Log, TEXT("🔴 Minimap  OB Lines  > 3  hole %d"), GameMode->CurrentHole);
                UpdateOBLines(OBPoints);

                // ⭐ OB 포인트 로드 후 카메라 높이 재계산

                UE_LOG(LogTemp, Log, TEXT("🔴 Minimap  OB Lines  > 3  bCaptureInitialized =  %d"), bCaptureInitialized);
                if (bCaptureInitialized)
                {
                    UpdateImprovedCaptureCamera();
                    GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
                        {
                            CaptureMapBackground();
                        });
                }

                UE_LOG(LogTemp, Log, TEXT("🔴 OB Lines updated + Camera recalculated for hole %d"), GameMode->CurrentHole);
            }
        }
    }

    if (HolecupImage)
        UpdateImagePosition(HolecupImage, HolecupWorldPosition, HolecupColor);



    if (bAutoRefreshCapture && RefreshInterval > 0.0f)
    {
        GetWorld()->GetTimerManager().SetTimer(RefreshTimerHandle, this,
            &UGolfMiniMap::RefreshMapBackground, RefreshInterval, true);
    }

    UE_LOG(LogTemp, Log, TEXT("✅ MiniMap initialization completed with Vertical Hole Alignment and Ball-to-Hole lines"));
}

// 5. 스케일 조정 함수 추가
void UGolfMiniMap::AdjustMiniMapScale(float ScaleFactor)
{
    UE_LOG(LogTemp, Log, TEXT("🔧 Adjusting MiniMap scale by factor: %.2f"), ScaleFactor);

    // 현재 스케일에 팩터 적용
    MapScale *= ScaleFactor;
    MapScale = FMath::Clamp(MapScale, 0.1f, 5.0f); // 10%~500% 범위로 제한

    // 스케일 재계산
    WorldToMapScale = CalculateImprovedWorldToMapScale();

    // UI 요소들 위치 업데이트
    RefreshUIElementPositions();

    // 캡처 카메라도 재조정
    if (bCaptureInitialized)
    {
        UpdateImprovedCaptureCamera();
        GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
            {
                CaptureMapBackground();
            });
    }

    UE_LOG(LogTemp, Log, TEXT("✅ New MapScale: %.2f, WorldToMapScale: %.6f"), MapScale, WorldToMapScale);
}

void UGolfMiniMap::UpdateCaptureCamera()
{
    if (!SceneCaptureComponent )
        return;

    SceneCaptureComponent->ProjectionType = ECameraProjectionMode::Orthographic;
    SceneCaptureComponent->FOVAngle = 120.0f;

    // 카메라를 맵 중심 위 높은 곳에 배치 (위에서 내려다보기)
    FVector CameraLocation = MapCenterWorldPosition + FVector(0, 0, CaptureHeight);
    FRotator CameraRotation = FRotator(-90.0f, 90.0f, 0.0f); // 아래를 향하도록

    // SceneCaptureComponent의 위치와 회전 직접 설정
    SceneCaptureComponent->SetWorldLocation(CameraLocation);
    SceneCaptureComponent->SetWorldRotation(CameraRotation);
    ;

    // 홀의 거리에 따라 캡처 범위 조정
    if (WorldToMapScale > 0.0f)
    {
        // UI가 세로(MapHeight)를 기준으로 축척을 정하므로, 
        // 카메라가 담아야 할 실제 월드 기준의 세로(Y) 범위는 (MapHeight / WorldToMapScale) 입니다.
        // Orthographic 모드의 OrthoWidth는 '가로(X)' 기준이므로, 여기에 미니맵 가로/세로 종횡비(Aspect Ratio)를 곱해줍니다.
        float AspectRatio = MapWidth / MapHeight;
        float DesiredOrthoWidth = (MapHeight / WorldToMapScale) * AspectRatio;

        // 만약 여유 마진(CaptureViewMargin)이 있다면 곱해줍니다. (기본 1.0f)
        SceneCaptureComponent->OrthoWidth = DesiredOrthoWidth * CaptureViewMargin;
    }
    else
    {
        float HoleDistance = FVector::Dist(TeeWorldPosition, HolecupWorldPosition);
        SceneCaptureComponent->OrthoWidth = FMath::Max(HoleDistance * 1.5f, CaptureOrthoWidth);
    }

}

void UGolfMiniMap::CaptureMapBackground()
{
    if (!IsValid(SceneCaptureComponent) || !IsValid(MapRenderTarget) || !bCaptureInitialized)
        return;

    // ✅ 이미 캡처 예약됨이면 중복 실행 방지
    if (bCaptureScheduled) return;
    bCaptureScheduled = true;

    SceneCaptureComponent->CaptureScene();

    GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
        {
            ApplyCapturedTexture();
            bCaptureScheduled = false;  // ✅ 완료 후 플래그 해제
            UE_LOG(LogTemp, Warning, TEXT("✅ Map background capture completed"));
        });
}


void UGolfMiniMap::ApplyCapturedTexture()
{
    if (!BackgroundImage || !MapRenderTarget) return;

    if (CaptureMaterial)
    {
        // ✅ 최초 1회만 생성, 이후 재사용
        if (!MapDynamicMaterial)
            MapDynamicMaterial = UMaterialInstanceDynamic::Create(CaptureMaterial, this);

        if (MapDynamicMaterial)
        {
            MapDynamicMaterial->SetTextureParameterValue(TEXT("MapTexture"), MapRenderTarget);
            BackgroundImage->SetBrushFromMaterial(MapDynamicMaterial);
            BackgroundImage->SetColorAndOpacity(FLinearColor::White);
        }
    }
    else
    {
        // ✅ Fallback: RenderTarget 직접 연결 대신 UTexture2D로 변환하거나
        //    최소한 RenderTarget 유효성 체크 후 연결
        if (!IsValid(MapRenderTarget)) return;

        FSlateBrush NewBrush;
        NewBrush.SetResourceObject(MapRenderTarget);
        NewBrush.DrawAs = ESlateBrushDrawType::Image;
        NewBrush.Tiling = ESlateBrushTileType::NoTile;
        NewBrush.ImageSize = FVector2D(MapWidth, MapHeight);
        NewBrush.Margin = FMargin(0.0f);
        NewBrush.TintColor = FSlateColor(FLinearColor::White);
        BackgroundImage->SetBrush(NewBrush);
        BackgroundImage->SetColorAndOpacity(FLinearColor::White);
    }

    ApplyBackgroundFlipTransform();
}

void UGolfMiniMap::SetCaptureHeight(float Height)
{
    UE_LOG(LogTemp, Log, TEXT("📸 SetCaptureHeight -=------Height %f"), Height);
    CaptureHeight = Height;

    if (bCaptureInitialized)
    {
        UpdateImprovedCaptureCamera();
    }
}

void UGolfMiniMap::RefreshMapBackground()
{
    if (bCaptureInitialized)
    {
        UpdateImprovedCaptureCamera();
        CaptureMapBackground();
    }
}

void UGolfMiniMap::ApplyBackgroundFlipTransform() const
{
    if (!BackgroundImage)
        return;

    // 가운데를 피벗으로 하여 좌우/상하 뒤집기 적용
    const FVector2D Pivot(0.5f, 0.5f);
    const float BackgroundScaleX = 1.0f;
    const float BackgroundScaleY = 1.0f;
    const FVector2D Scale(bFlipMapHorizontally ? -BackgroundScaleX : BackgroundScaleX,
        bFlipMapVertically ? BackgroundScaleY : -BackgroundScaleY);

    BackgroundImage->SetRenderTransformPivot(Pivot);
    BackgroundImage->SetRenderScale(Scale);
}

void UGolfMiniMap::SetMiniMapFlip(bool bFlipX, bool bFlipY)
{
    bFlipMapHorizontally = bFlipX;
    bFlipMapVertically = bFlipY;

    // 배경 이미지 즉시 반영
    ApplyBackgroundFlipTransform();

    // 좌표 변환도 즉시 반영되도록 UI 요소 재배치
    RefreshUIElementPositions();
}

void UGolfMiniMap::UpdateBallPosition(int32 PlayerIndex, const FVector& BallPosition)
{
    PlayerBallWorldPositions.Add(PlayerIndex, BallPosition); // 위치 저장

    if (PlayerBallImages.Contains(PlayerIndex) && bIsInitialized)
    {
        UImage* BallImageToUpdate = PlayerBallImages[PlayerIndex];
        FLinearColor BallColorToUse = PlayerBallColors.Contains(PlayerIndex) ? PlayerBallColors[PlayerIndex] : FLinearColor::White;
        UpdateImagePosition(BallImageToUpdate, BallPosition, BallColorToUse, PlayerIndex); // PlayerIndex 전달
    }
}

// ⭐ 수정 : UpdateAimDirection 이제 플레이어 인덱스를 받음
void UGolfMiniMap::UpdateAimDirection(int32 PlayerIndex, const FVector& AimDirection)
{
    PlayerAimDirections.Add(PlayerIndex, AimDirection); // 방향 저장

}

void UGolfMiniMap::SetCurrentHole(int32 HoleNumber)
{



    if (HoleNumberText)
    {
        HoleNumberText->SetText(FText::FromString(FString::Printf(TEXT("Hole %d"), HoleNumber)));
    }

    // ⭐ 홀 번호가 바뀐 경우 항상 재초기화
    // TeeWorldPosition/HolecupWorldPosition 비교는 신뢰할 수 없음
    // (GameMode가 미니맵보다 먼저 위치를 바꿔놓을 경우 Equals가 같다고 판단해 스킵됨)
    if (GameMode &&
        GameMode->MapInfo.TeePositions.IsValidIndex(HoleNumber - 1) &&
        GameMode->MapInfo.HolecupPositions.IsValidIndex(HoleNumber - 1))
    {
        FVector NewTee = GameMode->MapInfo.TeePositions[HoleNumber - 1];
        FVector NewHolecup = GameMode->MapInfo.HolecupPositions[HoleNumber - 1];

        bool bHoleNumberChanged = (HoleNumber != CurrentHoleNumber);
        bool bPositionChanged = !NewTee.Equals(TeeWorldPosition, 1.0f) ||
            !NewHolecup.Equals(HolecupWorldPosition, 1.0f);

        UE_LOG(LogTemp, Warning, TEXT("🗺️ SetCurrentHole(%d) - HoleChanged:%s PositionChanged:%s"),
            HoleNumber,
            bHoleNumberChanged ? TEXT("YES") : TEXT("NO"),
            bPositionChanged ? TEXT("YES") : TEXT("NO"));

        if (bHoleNumberChanged || bPositionChanged)
        {
            CurrentHoleNumber = HoleNumber;
            InitializeMiniMap(NewTee, NewHolecup);

            // ⭐ 추가: 강제 재캡처
            if (bCaptureInitialized)
            {
                GetWorld()->GetTimerManager().SetTimer(
                    DelayedCaptureTimer,
                    [this]() { CaptureMapBackground(); },
                    0.3f, false
                );
            }
            return;
        }
    }

    CurrentHoleNumber = HoleNumber;
    // Par 정보 업데이트
    if (ParInfoText && GameMode)
    {
        if (GameMode->MapInfo.ParScores.IsValidIndex(HoleNumber - 1))
        {
            int32 Par = GameMode->MapInfo.ParScores[HoleNumber - 1];
            ParInfoText->SetText(FText::FromString(FString::Printf(TEXT("Par %d"), Par)));
        }
    }

    // ⭐ 새 홀의 OB 라인 업데이트
    if (GameMode)
    {
        int32 HoleIndex = HoleNumber - 1;
        if (GameMode->MapInfo.OBLines.IsValidIndex(HoleIndex))
        {
            const TArray<FVector>& OBPoints = GameMode->MapInfo.OBLines[HoleIndex].Points;
            if (OBPoints.Num() >= 3)
            {
                UpdateOBLines(OBPoints);
                UE_LOG(LogTemp, Log, TEXT("🔄 OB Lines updated for hole %d"), HoleNumber);
            }
            else
            {
                ClearOBLines();
                UE_LOG(LogTemp, Log, TEXT("🚫 No OB Lines for hole %d"), HoleNumber);
            }
        }
        else
        {
            ClearOBLines();
            UE_LOG(LogTemp, Warning, TEXT("⚠️ No OB data for hole %d"), HoleNumber);
        }
    }
}

void UGolfMiniMap::UpdateDistanceAndElevation(float DistanceToHole, float ElevationDifference)
{
    CurrentDistance = DistanceToHole;
    CurrentElevation = ElevationDifference;

    UpdateInfoTexts();
}

FVector UGolfMiniMap::CalculateMapCenter() const
{
    // 홀컵과 티의 중점을 맵 중심으로 설정 (홀컵 기준으로 조정)
    FVector Center = (TeeWorldPosition + HolecupWorldPosition) * 0.5f;

    // 홀컵 방향으로 조금 더 치우치게 조정
    FVector HolecupDirection = (HolecupWorldPosition - TeeWorldPosition).GetSafeNormal();
    Center += HolecupDirection * (FVector::Dist(TeeWorldPosition, HolecupWorldPosition) * 0.1f);

    return Center;
}

float UGolfMiniMap::CalculateWorldToMapScale() const
{
    // 티와 홀컵 사이의 거리 계산
    float HoleDistance = FVector::Dist(TeeWorldPosition, HolecupWorldPosition);

    // 맵 높이의 60%를 홀 거리로 사용 (위아래 20%씩 여유 공간)
    float UsableMapHeight = MapHeight * (1.0f - 2.0f * VerticalViewPercentage);

    // 스케일 계산 (여유를 위해 0.9 계수)
    float Scale = (UsableMapHeight * 0.9f) / HoleDistance;

    return Scale * MapScale;
}

void UGolfMiniMap::UpdateMiniMapBounds()
{
    // 홀컵과 티 사이의 거리 계산
    float HoleDistance = FVector::Dist(TeeWorldPosition, HolecupWorldPosition);

    // 위아래 여유 공간 계산
    float VerticalPadding = HoleDistance * VerticalViewPercentage;

    // 맵 방향 계산 (홀컵에서 티로의 방향 - 반대로 변경)
    FVector HoleDirection = (TeeWorldPosition - HolecupWorldPosition).GetSafeNormal();

    // 경계 계산 (홀컵 기준)
    FVector ExtendedStart = HolecupWorldPosition - (HoleDirection * VerticalPadding);
    FVector ExtendedEnd = TeeWorldPosition + (HoleDirection * VerticalPadding);

    float HalfWidth = (MapWidth / MapHeight) * (HoleDistance + 2.0f * VerticalPadding) * 0.5f;

    MinWorldX = FMath::Min(ExtendedStart.X, ExtendedEnd.X) - HalfWidth;
    MaxWorldX = FMath::Max(ExtendedStart.X, ExtendedEnd.X) + HalfWidth;
    MinWorldY = FMath::Min(ExtendedStart.Y, ExtendedEnd.Y) - HalfWidth;
    MaxWorldY = FMath::Max(ExtendedStart.Y, ExtendedEnd.Y) + HalfWidth;
}


void UGolfMiniMap::UpdateImagePosition(UImage* Image, const FVector& WorldPosition, const FLinearColor& Color, int32 PlayerIndex)
{
    if (!IsValid(Image) || !IsValid(MiniMapCanvas))
    {
        UE_LOG(LogTemp, Warning, TEXT("UpdateImagePosition: Invalid Image or Canvas"));
        return;
    }

    FVector2D MapPosition = WorldToMapPosition(WorldPosition);

    // 볼 이미지인 경우 플레이어별 텍스처로 색상 업데이트
    if (PlayerIndex >= 0 && PlayerBallImages.Contains(PlayerIndex) && PlayerBallImages[PlayerIndex] == Image)
    {
        UTexture2D* PlayerTexture = GetPlayerBallTexture(PlayerIndex);
        if (PlayerTexture && PlayerBallBrushes.Contains(PlayerIndex))
        {
            // 저장된 브러시를 가져와서 색상만 업데이트
            FSlateBrush UpdatedBrush = PlayerBallBrushes[PlayerIndex];
            UpdatedBrush.TintColor = FSlateColor(Color);

            // 브러시 다시 저장
            PlayerBallBrushes[PlayerIndex] = UpdatedBrush;

            // 이미지에 적용
            Image->SetBrush(UpdatedBrush);
        }
        else
        {
            Image->SetColorAndOpacity(Color);
        }
    }
    else
    {
        Image->SetColorAndOpacity(Color);
    }

    // 나머지 위치 설정 코드는 기존과 동일
    if (UCanvasPanelSlot* ImageSlot = Cast<UCanvasPanelSlot>(Image->Slot))
    {
        FVector2D IconSize(12.0f, 12.0f);

        if (Image == FlagImage)
        {
            IconSize = FVector2D(12.0f, 16.0f);
        }
        //else if(Image == TeeImage)
        //{
        //    IconSize = FVector2D(2.0f, 2.0f);
        //}
        else if (PlayerIndex != -1 && GameMode && PlayerIndex == GameMode->CurrentPlayerIndex)
        {
            IconSize = FVector2D(16.0f, 16.0f);
        }

        ImageSlot->SetSize(IconSize);
        ImageSlot->SetAutoSize(true);
        FVector2D AdjustedPosition = MapPosition - (IconSize * 0.5f);
        if (Image == HolecupImage) AdjustedPosition.Y -= 10;
        ImageSlot->SetPosition(AdjustedPosition);
        ImageSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
        Image->SetVisibility(ESlateVisibility::Visible);
    }
}

void UGolfMiniMap::UpdateAimLinePosition(int32 PlayerIndex)
{
    // 에임 라인이 없으면 생성
    EnsureAimLineExists(PlayerIndex);


    // StrokeMode가 아니면 라인 숨김
    if (!GameMode || GameMode->GetCurrentGameMode() != EGolfGameMode::StrokeMode)
    {

        return;
    }

    // 현재 플레이어가 아니면 숨김
    if (!GameMode || PlayerIndex != GameMode->CurrentPlayerIndex)
    {

        return;
    }

    // 나머지 기존 로직은 동일...
    if (!GameMode || GameMode->AimLocation.IsZero())
    {

        return;
    }

    FVector PlayerBallCurrentPosition = PlayerBallWorldPositions.Contains(PlayerIndex) ?
        PlayerBallWorldPositions[PlayerIndex] : FVector::ZeroVector;

    if (PlayerBallCurrentPosition.IsZero())
    {

        return;
    }

    // 나머지 에임라인 업데이트 로직...
    FVector AimTargetPosition = GameMode->AimLocation;
    FVector AimDirection = (AimTargetPosition - PlayerBallCurrentPosition).GetSafeNormal();
    float LineDistance = FVector::Dist(PlayerBallCurrentPosition, AimTargetPosition);
    float DisplayLength = FMath::Min(LineDistance, AimLineLength);
    FVector AimEndPosition = PlayerBallCurrentPosition + (AimDirection * DisplayLength);

    FVector2D BallMapPos = WorldToMapPosition(PlayerBallCurrentPosition);
    FVector2D AimEndMapPos = WorldToMapPosition(AimEndPosition);
    FVector2D LineCenter = (BallMapPos + AimEndMapPos) * 0.5f;
    float LineLength = FVector2D::Distance(BallMapPos, AimEndMapPos);

    if (LineLength < 5.0f)
    {

        return;
    }

}

void UGolfMiniMap::EnsureAimLineExists(int32 PlayerIndex)
{

    // AimActor 이미지도 없으면 생성
    if (!PlayerAimActorImages.Contains(PlayerIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("Creating missing AimActor for player %d"), PlayerIndex);
        CreateAimActorForPlayer(PlayerIndex);
    }

    // Ball-to-Aim 라인도 없으면 생성
    if (!PlayerBallToAimLineImages.Contains(PlayerIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("Creating missing Ball-to-Aim line for player %d"), PlayerIndex);
        CreateBallToAimLineForPlayer(PlayerIndex);
    }
}

void UGolfMiniMap::UpdateFlagPosition()
{
    if (!FlagImage)
        return;

    // 홀컵 위치에 플래그 표시 (홀컵보다 약간 위에)
    FVector FlagPosition = HolecupWorldPosition + FVector(0, 0, 20.0f); // 20cm 위
    UpdateImagePosition(FlagImage, FlagPosition, FlagColor);

    // 플래그 크기를 조금 더 크게
    UCanvasPanelSlot* FlagSlot = Cast<UCanvasPanelSlot>(FlagImage->Slot);
    if (FlagSlot)
    {
        FlagSlot->SetSize(FVector2D(12.0f, 16.0f)); // 깃발 모양
    }
}

void UGolfMiniMap::UpdateInfoTexts()
{
    // 거리 정보 업데이트 (야드 단위)
    if (DistanceText)
    {
        float DistanceInYards = CurrentDistance / 91.44f; // cm를 야드로 변환
        FString DistanceStr = FString::Printf(TEXT("%.0f Y"), DistanceInYards);
        DistanceText->SetText(FText::FromString(DistanceStr));

        // 거리에 따른 색상 변경
        FSlateColor TextColor;
        if (DistanceInYards < 100.0f)
        {
            TextColor = FSlateColor(FLinearColor::Green);
        }
        else if (DistanceInYards < 200.0f)
        {
            TextColor = FSlateColor(FLinearColor::Yellow);
        }
        else
        {
            TextColor = FSlateColor(FLinearColor::Red);
        }
        DistanceText->SetColorAndOpacity(TextColor);
    }

    // 고도차 정보 업데이트
    if (ElevationText)
    {
        float ElevationInMeters = CurrentElevation / 100.0f; // cm를 미터로 변환
        FString ElevationStr;
        FSlateColor ElevationColor;

        if (FMath::Abs(ElevationInMeters) < 0.5f)
        {
            ElevationStr = TEXT("Level");
            ElevationColor = FSlateColor(FLinearColor::White);
        }
        else if (ElevationInMeters > 0)
        {
            ElevationStr = FString::Printf(TEXT("UP %.1fm"), ElevationInMeters);
            ElevationColor = FSlateColor(FLinearColor::Red);
        }
        else
        {
            ElevationStr = FString::Printf(TEXT("DOWN %.1fm"), FMath::Abs(ElevationInMeters));
            ElevationColor = FSlateColor(FLinearColor::Blue);
        }

        ElevationText->SetText(FText::FromString(ElevationStr));
        ElevationText->SetColorAndOpacity(ElevationColor);
    }

    // 바람 정보 (임시 - 추후 실제 바람 시스템 연동)
    if (WindInfoText)
    {
        WindInfoText->SetText(FText::FromString(TEXT("Wind: 5mph E")));
        WindInfoText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 1.0f, 1.0f)));
    }
}

// 기존 함수들 (궤적, 애니메이션 등)
void UGolfMiniMap::ShowMiniMapWithAnimation(bool bShow, float AnimationDuration)
{
    // 애니메이션 구현 (필요시)
}

void UGolfMiniMap::ShowBallTrajectory(const TArray<FVector>& tTrajectoryPoints)
{
    // 기존 궤적 포인트 제거
    ClearTrajectoryPoints();

    CurrentTrajectory = tTrajectoryPoints;

    // 새로운 궤적 포인트 생성
    for (const FVector& Point : tTrajectoryPoints)
    {
        CreateTrajectoryPoint(Point);
    }
}

void UGolfMiniMap::ClearBallTrajectory()
{
    ClearTrajectoryPoints();
    CurrentTrajectory.Empty();
}

void UGolfMiniMap::CreateTrajectoryPoint(const FVector& WorldPosition)
{
    if (!TrajectoryCanvas)
        return;

    // 궤적 포인트 이미지 생성
    UImage* TrajectoryPoint = NewObject<UImage>(this);
    if (!TrajectoryPoint)
        return;

    // 캔버스에 추가
    UCanvasPanelSlot* TrajectorySlot = TrajectoryCanvas->AddChildToCanvas(TrajectoryPoint);
    if (!TrajectorySlot)
        return;

    // 궤적 포인트 설정
    TrajectoryPoint->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 0.0f, 0.7f)); // 반투명 노란색

    // 위치 설정
    FVector2D MapPosition = WorldToMapPosition(WorldPosition);
    FVector2D PointSize(3.0f, 3.0f);

    TrajectorySlot->SetSize(PointSize);
    TrajectorySlot->SetPosition(MapPosition - (PointSize * 0.5f));
    TrajectorySlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));

    // 배열에 저장
    TrajectoryPoints.Add(TrajectoryPoint);
}

void UGolfMiniMap::ClearTrajectoryPoints()
{
    for (UImage* Point : TrajectoryPoints)
    {
        if (Point)
        {
            Point->RemoveFromParent();
        }
    }
    TrajectoryPoints.Empty();
}

void UGolfMiniMap::UpdateDistanceInfo()
{
    // 홀컵과의 거리 계산
    float DistanceToHole = CalculateDistanceToHole();
    float LastShotDistance = CalculateLastShotDistance();

    // 미터를 야드로 변환 (1미터 = 1.09361야드)
    float DistanceInYards = DistanceToHole * 0.0109361f;
    float ShotDistanceInYards = LastShotDistance * 0.0109361f;

    // 텍스트 업데이트
    if (DistanceToHoleText)
    {
        FString strDistanceText = FString::Printf(TEXT("%.0f Y"), DistanceInYards);
        DistanceToHoleText->SetText(FText::FromString(strDistanceText));
    }

    if (ShotDistanceText && LastShotDistance > 0.0f)
    {
        FString ShotText = FString::Printf(TEXT("Last: %.0f Y"), ShotDistanceInYards);
        ShotDistanceText->SetText(FText::FromString(ShotText));
    }
}

void UGolfMiniMap::SetZoomLevel(float ZoomLevel)
{
    //CurrentZoomLevel = FMath::Clamp(ZoomLevel, MinZoomLevel, MaxZoomLevel);

    //// 스케일 재계산
    //WorldToMapScale = CalculateWorldToMapScale() * CurrentZoomLevel;

    //// 모든 요소 위치 업데이트
    //if (bIsInitialized)
    //{
    //    UpdateImagePosition(TeeImage, TeeWorldPosition, TeeColor);
    //    UpdateImagePosition(HolecupImage, HolecupWorldPosition, HolecupColor);
    //    UpdateImagePosition(BallImage, BallWorldPosition, BallColor);

    //    // 궤적 포인트들도 업데이트
    //    ClearTrajectoryPoints();
    //    for (const FVector& Point : CurrentTrajectory)
    //    {
    //        CreateTrajectoryPoint(Point);
    //    }

    //    // 캡처 범위도 조정
    //    if (bCaptureInitialized)
    //    {
    //        UpdateCaptureCamera();
    //    }
    //}
}

float UGolfMiniMap::CalculateDistanceToHole() const
{
    // ⭐ 수정: 현재 플레이어 볼의 위치 기준으로 거리 계산
    if (GameMode && PlayerBallWorldPositions.Contains(GameMode->CurrentPlayerIndex))
    {
        return FVector::Dist(PlayerBallWorldPositions[GameMode->CurrentPlayerIndex], HolecupWorldPosition);
    }
    return 0.0f;
}

float UGolfMiniMap::CalculateLastShotDistance() const
{
    // ⭐ 수정: 마지막 샷 거리는 PlayerManager 또는 GolfPlayer에서 관리해야 함.
    // 현재 TeeWorldPosition과 BallWorldPosition만으로는 마지막 샷 거리를 알 수 없음.
    // 임시로 0 반환 또는 적절한 로직 추가 필요.
    // (이 부분은 외부에서 ShotStartPos와 ShotEndPos를 받아야 정확함)
    return 0.0f; // 적절한 로직으로 교체 필요
}

// 1. 메모리 정리 로직 추가
void UGolfMiniMap::BeginDestroy()
{
    UE_LOG(LogTemp, Log, TEXT("UGolfMiniMap::BeginDestroy() - 리소스 정리 시작"));

    ClearOBLineSegments();

    // 플레이어 볼/에임 라인 위젯 제거
    for (auto& Elem : PlayerBallImages)
    {
        if (IsValid(Elem.Value))
        {
            Elem.Value->RemoveFromParent();
            Elem.Value->ConditionalBeginDestroy();
        }
    }
    PlayerBallImages.Empty();
    PlayerBallBrushes.Empty(); // 브러시 맵도 정리


    // ✅ RenderTarget 해제 전 Brush 참조 먼저 끊기
    if (BackgroundImage)
    {
        BackgroundImage->SetBrush(FSlateBrush());  // 빈 브러시로 교체
        BackgroundImage->SetBrushFromMaterial(nullptr);
    }
    MapDynamicMaterial = nullptr;  // UPROPERTY라 자동 GC됨

    // 나머지 정리 코드는 기존과 동일...

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
    }

    if (IsValid(CaptureActor))
    {
        UE_LOG(LogTemp, Log, TEXT("Destroying CaptureActor"));
        CaptureActor->Destroy();
        CaptureActor = nullptr;
    }

    SceneCaptureComponent = nullptr;

    if (IsValid(MapRenderTarget))
    {
        MapRenderTarget->ConditionalBeginDestroy();
        MapRenderTarget = nullptr;
    }

    ClearTrajectoryPoints();
    ClearAllBallToHoleLines();

    // OB 마스크 오버레이 텍스처 정리
    if (IsValid(OBMaskTexture))
    {
        OBMaskTexture->MarkAsGarbage();
        OBMaskTexture = nullptr;
    }
    OBMaskOverlayImage = nullptr;

    UE_LOG(LogTemp, Log, TEXT("UGolfMiniMap 리소스 정리 완료"));
    Super::BeginDestroy();
}

// 2. OB 라인 업데이트 함수
void UGolfMiniMap::UpdateOBLines(const TArray<FVector>& OBPoints)
{
    UE_LOG(LogTemp, Log, TEXT("🔴 Updating OB lines with %d points"), OBPoints.Num());

    // 현재 OB 포인트 저장
    CurrentOBPoints = OBPoints;

    // 기존 OB 라인 제거
    ClearOBLineSegments();

    // 새 OB 라인 생성
    if (bShowOBLines && OBPoints.Num() >= 3)
    {
        CreateOBLineSegments(OBPoints);
    }

    // OB 바깥 영역 마스크 오버레이 갱신
    UpdateOBMaskOverlay();
}

// 3. OB 라인 세그먼트 생성
void UGolfMiniMap::CreateOBLineSegments(const TArray<FVector>& OBPoints)
{
    if (!OBLinesCanvas || OBPoints.Num() < 3)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Cannot create OB lines: Canvas=%s, Points=%d"),
            OBLinesCanvas ? TEXT("Valid") : TEXT("NULL"), OBPoints.Num());
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("🔴 Creating OB line segments..."));

    // 각 세그먼트에 대해 라인 이미지 생성
    for (int32 i = 0; i < OBPoints.Num(); i++)
    {
        int32 NextIndex = (i + 1) % OBPoints.Num(); // 마지막 점은 첫 번째 점과 연결

        FVector2D StartMapPos = WorldToMapPosition(OBPoints[i]);
        FVector2D EndMapPos = WorldToMapPosition(OBPoints[NextIndex]);

        // 라인 세그먼트 생성
        UImage* LineSegment = CreateOBLineSegment(StartMapPos, EndMapPos);
        if (LineSegment)
        {
            OBLineSegments.Add(LineSegment);
            UE_LOG(LogTemp, VeryVerbose, TEXT("  OB Line %d: (%.1f,%.1f) → (%.1f,%.1f)"),
                i, StartMapPos.X, StartMapPos.Y, EndMapPos.X, EndMapPos.Y);
        }
    }


    UE_LOG(LogTemp, Log, TEXT("✅ Created %d OB line segments"), OBLineSegments.Num());
}

// 4. 개별 OB 라인 세그먼트 생성
UImage* UGolfMiniMap::CreateOBLineSegment(const FVector2D& StartPos, const FVector2D& EndPos)
{
    if (!OBLinesCanvas)
        return nullptr;

    // 새 이미지 위젯 생성
    UImage* LineImage = NewObject<UImage>(this);
    if (!LineImage)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to create line image"));
        return nullptr;
    }

    // 캔버스에 추가
    UCanvasPanelSlot* LineSlot = OBLinesCanvas->AddChildToCanvas(LineImage);
    if (!LineSlot)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to add line to canvas"));
        return nullptr;
    }

    // 라인 이미지 설정
    LineImage->SetColorAndOpacity(OBLineColor);

    // 단순한 흰색 픽셀 텍스처 생성 (라인용)
    // UE에서는 기본 흰색 텍스처를 사용
    FSlateBrush LineBrush;
    LineBrush.TintColor = FSlateColor(OBLineColor);
    LineBrush.DrawAs = ESlateBrushDrawType::Box;
    LineImage->SetBrush(LineBrush);

    // 라인 변환 적용
    UpdateOBLineSegmentTransform(LineImage, StartPos, EndPos);

    // 가시성 설정
    LineImage->SetVisibility(ESlateVisibility::Visible);

    return LineImage;
}

// 5. 라인 세그먼트 변환 업데이트
void UGolfMiniMap::UpdateOBLineSegmentTransform(UImage* LineImage, const FVector2D& StartPos, const FVector2D& EndPos)
{
    if (!LineImage)
        return;

    UCanvasPanelSlot* LineSlot = Cast<UCanvasPanelSlot>(LineImage->Slot);
    if (!LineSlot)
        return;

    // 라인의 중심점과 길이 계산
    FVector2D LineVector = EndPos - StartPos;
    float LineLength = LineVector.Size();
    FVector2D LineCenter = (StartPos + EndPos) * 0.5f;

    // 라인의 각도 계산 (라디안)
    float LineAngle = FMath::Atan2(LineVector.Y, LineVector.X);
    float LineAngleDegrees = FMath::RadiansToDegrees(LineAngle);

    // 슬롯 설정
    LineSlot->SetSize(FVector2D(LineLength, OBLineThickness));
    LineSlot->SetPosition(LineCenter - FVector2D(LineLength * 0.5f, OBLineThickness * 0.5f));
    LineSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));

    // 회전 변환 적용
    FWidgetTransform Transform;
    Transform.Translation = FVector2D::ZeroVector;
    Transform.Scale = FVector2D(1.0f, 1.0f);
    Transform.Shear = FVector2D::ZeroVector;
    Transform.Angle = LineAngleDegrees;

    LineImage->SetRenderTransform(Transform);
    LineImage->SetRenderTransformPivot(FVector2D(0.5f, 0.5f)); // 중심점 기준 회전
}

// 6. OB 라인 제거
void UGolfMiniMap::ClearOBLineSegments()
{
    UE_LOG(LogTemp, Log, TEXT("🧹 Clearing %d OB line segments"), OBLineSegments.Num());

    for (UImage* LineSegment : OBLineSegments)
    {
        if (IsValid(LineSegment))
        {
            LineSegment->RemoveFromParent();
        }
    }
    OBLineSegments.Empty();
}

void UGolfMiniMap::ClearOBLines()
{
    ClearOBLineSegments();
    CurrentOBPoints.Empty();
    ClearOBMaskOverlay();
    UE_LOG(LogTemp, Log, TEXT("🗑️ OB Lines cleared"));
}

// ──────────────────────────────────────────────────────────────────────────────
// OB 마스크 오버레이: OB 폴리곤 바깥 픽셀을 반투명 어둡게 처리
// UpdateOBLines() 에서만 호출 (Tick 금지 — 1회 생성 후 재사용)
// ──────────────────────────────────────────────────────────────────────────────
void UGolfMiniMap::UpdateOBMaskOverlay()
{
    // 마스크 기능이 꺼져 있거나 MiniMapCanvas 없으면 클리어만
    if (!bShowOBMaskOverlay || !MiniMapCanvas)
    {
        ClearOBMaskOverlay();
        return;
    }

    // OB 포인트가 부족하면 마스크 숨김
    if (CurrentOBPoints.Num() < 3)
    {
        if (IsValid(OBMaskOverlayImage))
            OBMaskOverlayImage->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    // 텍스처 해상도 — 미니맵 실제 픽셀 크기에 맞춤
    const int32 TexW = FMath::RoundToInt(MapWidth);
    const int32 TexH = FMath::RoundToInt(MapHeight);
    if (TexW <= 0 || TexH <= 0)
        return;

    // ── 텍스처 생성 (없거나 크기 불일치 시 재생성) ──────────────────────
    bool bNeedCreate = !IsValid(OBMaskTexture)
        || OBMaskTexture->GetSizeX() != TexW
        || OBMaskTexture->GetSizeY() != TexH;

    if (bNeedCreate)
    {
        if (IsValid(OBMaskTexture))
            OBMaskTexture->MarkAsGarbage();

        OBMaskTexture = UTexture2D::CreateTransient(TexW, TexH, PF_B8G8R8A8);
        if (!OBMaskTexture)
        {
            UE_LOG(LogTemp, Error, TEXT("❌ OBMaskOverlay: 텍스처 생성 실패"));
            return;
        }
        OBMaskTexture->Filter = TF_Bilinear;
        OBMaskTexture->AddressX = TA_Clamp;
        OBMaskTexture->AddressY = TA_Clamp;
        OBMaskTexture->SRGB = false;
        OBMaskTexture->UpdateResource();
    }

    // ── 픽셀 버퍼 작성 ────────────────────────────────────────────────────
    // OB 안쪽(인바운드) = 완전 투명, OB 바깥쪽 = OBOutsideColor * OBOutsideDarkness
    const int32 PixelCount = TexW * TexH;
    TArray<uint8> PixelData;
    PixelData.SetNumUninitialized(PixelCount * 4); // BGRA

    // 바깥 픽셀 색 (사전 계산)
    const uint8 OutB = FMath::Clamp(FMath::RoundToInt(OBOutsideColor.B * 255.f), 0, 255);
    const uint8 OutG = FMath::Clamp(FMath::RoundToInt(OBOutsideColor.G * 255.f), 0, 255);
    const uint8 OutR = FMath::Clamp(FMath::RoundToInt(OBOutsideColor.R * 255.f), 0, 255);
    const uint8 OutA = FMath::Clamp(FMath::RoundToInt(OBOutsideDarkness * 255.f), 0, 255);

    // OB 폴리곤 맵 픽셀 좌표 미리 변환 (월드→맵 1회만)
    TArray<FVector2D> PolyPts;
    PolyPts.Reserve(CurrentOBPoints.Num());
    for (const FVector& WP : CurrentOBPoints)
        PolyPts.Add(WorldToMapPosition(WP));

    // 와인딩 넘버 람다 (IsPointInOBArea 와 동일 알고리즘, 맵 픽셀 좌표로 동작)
    auto IsOutside = [&](float Px, float Py) -> bool
        {
            int32 Winding = 0;
            const int32 N = PolyPts.Num();
            for (int32 i = 0; i < N; ++i)
            {
                const FVector2D& P1 = PolyPts[i];
                const FVector2D& P2 = PolyPts[(i + 1) % N];
                if (P1.Y <= Py)
                {
                    if (P2.Y > Py)
                    {
                        float Cross = (P2.X - P1.X) * (Py - P1.Y) - (Px - P1.X) * (P2.Y - P1.Y);
                        if (Cross > 0.f) ++Winding;
                    }
                }
                else
                {
                    if (P2.Y <= Py)
                    {
                        float Cross = (P2.X - P1.X) * (Py - P1.Y) - (Px - P1.X) * (P2.Y - P1.Y);
                        if (Cross < 0.f) --Winding;
                    }
                }
            }
            // Winding == 0 → 바깥(OB 영역)
            return Winding == 0;
        };

    for (int32 Y = 0; Y < TexH; ++Y)
    {
        for (int32 X = 0; X < TexW; ++X)
        {
            const int32 Idx = (Y * TexW + X) * 4;
            if (IsOutside(static_cast<float>(X), static_cast<float>(Y)))
            {
                PixelData[Idx + 0] = OutB;
                PixelData[Idx + 1] = OutG;
                PixelData[Idx + 2] = OutR;
                PixelData[Idx + 3] = OutA;
            }
            else
            {
                // 안쪽 — 완전 투명
                PixelData[Idx + 0] = 0;
                PixelData[Idx + 1] = 0;
                PixelData[Idx + 2] = 0;
                PixelData[Idx + 3] = 0;
            }
        }
    }

    // ── GPU 업로드 ────────────────────────────────────────────────────────
    FTexture2DMipMap& Mip = OBMaskTexture->GetPlatformData()->Mips[0];
    void* MipData = Mip.BulkData.Lock(LOCK_READ_WRITE);
    FMemory::Memcpy(MipData, PixelData.GetData(), PixelData.Num());
    Mip.BulkData.Unlock();
    OBMaskTexture->UpdateResource();

    // ── UImage 오버레이 생성 (최초 1회) ──────────────────────────────────
    if (!IsValid(OBMaskOverlayImage))
    {
        OBMaskOverlayImage = NewObject<UImage>(this);
        UCanvasPanelSlot* CanvasSlot = MiniMapCanvas->AddChildToCanvas(OBMaskOverlayImage);
        if (CanvasSlot)
        {
            // 미니맵 전체를 덮도록 (0,0) ~ (MapWidth, MapHeight)
            CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
            CanvasSlot->SetPosition(FVector2D(0.f, 0.f));
            CanvasSlot->SetSize(FVector2D(MapWidth, MapHeight));
            // OB 라인(OBLineSegments)보다 아래, 볼/플래그보다 아래에 위치하도록 Z-Order 설정
            CanvasSlot->SetZOrder(1);
        }
        UE_LOG(LogTemp, Log, TEXT("✅ OBMaskOverlayImage 생성 완료"));
    }

    // 텍스처 적용
    FSlateBrush Brush;
    Brush.SetResourceObject(OBMaskTexture);
    Brush.DrawAs = ESlateBrushDrawType::Image;
    Brush.ImageSize = FVector2D(MapWidth, MapHeight);
    OBMaskOverlayImage->SetBrush(Brush);
    OBMaskOverlayImage->SetColorAndOpacity(FLinearColor::White); // 색상 틴트 없이 텍스처 그대로

    OBMaskOverlayImage->SetVisibility(ESlateVisibility::HitTestInvisible);

    UE_LOG(LogTemp, Log, TEXT("🟫 OBMaskOverlay 갱신 완료 (%dx%d, Darkness=%.2f)"),
        TexW, TexH, OBOutsideDarkness);
}

void UGolfMiniMap::ClearOBMaskOverlay()
{
    if (IsValid(OBMaskOverlayImage))
    {
        OBMaskOverlayImage->SetVisibility(ESlateVisibility::Collapsed);
    }
    // 텍스처는 재사용 가능하므로 즉시 파괴하지 않음
}

// 7. OB 라인 가시성 토글
void UGolfMiniMap::SetOBLinesVisible(bool bVisible)
{
    bShowOBLines = bVisible;

    for (UImage* LineSegment : OBLineSegments)
    {
        if (IsValid(LineSegment))
        {
            LineSegment->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("👁️ OB Lines visibility: %s"), bVisible ? TEXT("ON") : TEXT("OFF"));
}

// 1. 좌표 매칭 상태 진단
void UGolfMiniMap::DebugCoordinateMatching()
{
    UE_LOG(LogTemp, Warning, TEXT("🗺️ === Perspective MiniMap Debug ==="));

    // 기본 설정
    UE_LOG(LogTemp, Warning, TEXT("1️⃣ Basic Settings:"));
    UE_LOG(LogTemp, Warning, TEXT("   MapSize: %.1f x %.1f"), MapWidth, MapHeight);
    UE_LOG(LogTemp, Warning, TEXT("   MapScale: %.2f"), MapScale);
    UE_LOG(LogTemp, Warning, TEXT("   WorldToMapScale: %.6f"), WorldToMapScale);

    // Perspective 캡처 정보
    if (SceneCaptureComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("2️⃣ Perspective Capture:"));
        UE_LOG(LogTemp, Warning, TEXT("   Camera Type: %s"),
            SceneCaptureComponent->ProjectionType == ECameraProjectionMode::Perspective ? TEXT("Perspective") : TEXT("Orthographic"));
        UE_LOG(LogTemp, Warning, TEXT("   FOV Angle: %.1f°"), SceneCaptureComponent->FOVAngle);
        UE_LOG(LogTemp, Warning, TEXT("   Camera Height: %.1f cm"), CaptureHeight);
        UE_LOG(LogTemp, Warning, TEXT("   Capture Radius: %.1f cm"), CaptureWorldSize * 0.5f);
    }

    // 홀 정보
    float HoleDistance = FVector::Dist(TeeWorldPosition, HolecupWorldPosition);
    UE_LOG(LogTemp, Warning, TEXT("3️⃣ Hole Information:"));
    UE_LOG(LogTemp, Warning, TEXT("   Hole Distance: %.1f cm (%.1f m)"), HoleDistance, HoleDistance / 100.0f);
    UE_LOG(LogTemp, Warning, TEXT("   Map Center: %s"), *MapCenterWorldPosition.ToString());

    // 스케일 검증
    float ExpectedPixelDistance = HoleDistance * WorldToMapScale;
    float MapDiagonal = FMath::Sqrt(MapWidth * MapWidth + MapHeight * MapHeight);
    float UsagePercentage = (ExpectedPixelDistance / MapDiagonal) * 100.0f;

    UE_LOG(LogTemp, Warning, TEXT("4️⃣ Scale Verification:"));
    UE_LOG(LogTemp, Warning, TEXT("   Expected Pixel Distance: %.1f px"), ExpectedPixelDistance);
    UE_LOG(LogTemp, Warning, TEXT("   Map Usage: %.1f%% of diagonal"), UsagePercentage);
    UE_LOG(LogTemp, Warning, TEXT("   Scale Status: %s"),
        (UsagePercentage > 30.0f && UsagePercentage < 80.0f) ? TEXT("✅ Good") : TEXT("❌ Needs Adjustment"));

    UE_LOG(LogTemp, Warning, TEXT("=== End Debug ==="));
}

void UGolfMiniMap::TestCoordinateMappingDetailed()
{
    UE_LOG(LogTemp, Warning, TEXT("🧪 Detailed coordinate mapping test with vertical alignment..."));

    if (!bIsInitialized)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ MiniMap not initialized"));
        return;
    }

    // 테스트 포인트: 홀의 수직 정렬 확인
    struct FTestPoint
    {
        FString Name;
        FVector WorldPos;
        FVector2D ExpectedMapPos;
    };

    TArray<FTestPoint> TestPoints;

    // 중심점 (맵 중앙에 위치해야 함)
    TestPoints.Add({ TEXT("Center"), MapCenterWorldPosition, FVector2D(MapWidth * 0.5f, MapHeight * 0.5f) });

    // 티 (하단 중앙에 위치해야 함)
    TestPoints.Add({ TEXT("Tee"), TeeWorldPosition, FVector2D(MapWidth * 0.5f, MapHeight * 0.8f) });

    // 홀컵 (상단 중앙에 위치해야 함)
    TestPoints.Add({ TEXT("Holecup"), HolecupWorldPosition, FVector2D(MapWidth * 0.5f, MapHeight * 0.2f) });

    UE_LOG(LogTemp, Warning, TEXT("📍 Vertical Alignment Test Results:"));
    UE_LOG(LogTemp, Warning, TEXT("   Hole Rotation Angle: %.1f degrees"), FMath::RadiansToDegrees(HoleRotationAngle));

    for (const FTestPoint& Test : TestPoints)
    {
        FVector2D ActualMapPos = WorldToMapPosition(Test.WorldPos);
        FVector2D Difference = ActualMapPos - Test.ExpectedMapPos;
        float Error = Difference.Size();

        bool bVerticallyAligned = FMath::Abs(ActualMapPos.X - MapWidth * 0.5f) < 10.0f; // X축 중앙 정렬 확인
        bool bAccurate = Error < 15.0f; // 15픽셀 오차 허용

        UE_LOG(LogTemp, Warning, TEXT("   %s:"), *Test.Name);
        UE_LOG(LogTemp, Warning, TEXT("     World: %s"), *Test.WorldPos.ToString());
        UE_LOG(LogTemp, Warning, TEXT("     Expected: (%.1f, %.1f)"), Test.ExpectedMapPos.X, Test.ExpectedMapPos.Y);
        UE_LOG(LogTemp, Warning, TEXT("     Actual: (%.1f, %.1f)"), ActualMapPos.X, ActualMapPos.Y);
        UE_LOG(LogTemp, Warning, TEXT("     Error: %.1f pixels %s"), Error, bAccurate ? TEXT("✅") : TEXT("❌"));
        UE_LOG(LogTemp, Warning, TEXT("     Vertical Alignment: %s"), bVerticallyAligned ? TEXT("✅") : TEXT("❌"));
    }

    // 역변환 테스트
    UE_LOG(LogTemp, Warning, TEXT("🔄 Reverse Conversion Test:"));
    FVector2D TestMapPos(MapWidth * 0.5f, MapHeight * 0.1f); // 상단 중앙
    FVector ConvertedWorld = MapPositionToWorldPosition(TestMapPos);
    FVector2D BackToMap = WorldToMapPosition(ConvertedWorld);

    UE_LOG(LogTemp, Warning, TEXT("   Map: (%.1f, %.1f) → World: %s → Map: (%.1f, %.1f)"),
        TestMapPos.X, TestMapPos.Y, *ConvertedWorld.ToString(), BackToMap.X, BackToMap.Y);
}
// 2. 좌표 매핑 테스트
void UGolfMiniMap::TestCoordinateMapping()
{
    UE_LOG(LogTemp, Warning, TEXT("🧪 Testing coordinate mapping..."));

    if (!bIsInitialized)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ MiniMap not initialized"));
        return;
    }

    // 테스트 포인트들 생성 (미니맵 중심 기준)
    TArray<FVector> TestPoints;
    FVector Center = MapCenterWorldPosition;

    // 중심점
    TestPoints.Add(Center);

    // 4방향 테스트
    float TestDistance = 1000.0f; // 10미터
    TestPoints.Add(Center + FVector(TestDistance, 0, 0));      // 동쪽
    TestPoints.Add(Center + FVector(-TestDistance, 0, 0));     // 서쪽  
    TestPoints.Add(Center + FVector(0, TestDistance, 0));      // 북쪽
    TestPoints.Add(Center + FVector(0, -TestDistance, 0));     // 남쪽

    // 대각선 테스트
    TestPoints.Add(Center + FVector(TestDistance, TestDistance, 0));    // 북동
    TestPoints.Add(Center + FVector(-TestDistance, TestDistance, 0));   // 북서
    TestPoints.Add(Center + FVector(TestDistance, -TestDistance, 0));   // 남동
    TestPoints.Add(Center + FVector(-TestDistance, -TestDistance, 0));  // 남서

    UE_LOG(LogTemp, Warning, TEXT("📍 Test Points Mapping:"));
    for (int32 i = 0; i < TestPoints.Num(); i++)
    {
        FVector2D MapPos = WorldToMapPosition(TestPoints[i]);

        // 맵 범위 내에 있는지 확인
        bool bInRange = (MapPos.X >= 0 && MapPos.X <= MapWidth &&
            MapPos.Y >= 0 && MapPos.Y <= MapHeight);

        FString DirectionName;
        switch (i)
        {
        case 0: DirectionName = TEXT("Center"); break;
        case 1: DirectionName = TEXT("East"); break;
        case 2: DirectionName = TEXT("West"); break;
        case 3: DirectionName = TEXT("North"); break;
        case 4: DirectionName = TEXT("South"); break;
        case 5: DirectionName = TEXT("NorthEast"); break;
        case 6: DirectionName = TEXT("NorthWest"); break;
        case 7: DirectionName = TEXT("SouthEast"); break;
        case 8: DirectionName = TEXT("SouthWest"); break;
        }

        UE_LOG(LogTemp, Warning, TEXT("   %s: World%s → Map(%.1f, %.1f) %s"),
            *DirectionName, *TestPoints[i].ToString(), MapPos.X, MapPos.Y,
            bInRange ? TEXT("✅") : TEXT("❌ OUT OF RANGE"));
    }
}

// 3. 좌표 정렬 수정
void UGolfMiniMap::FixCoordinateAlignment()
{
    UE_LOG(LogTemp, Warning, TEXT("🔧 Auto-fixing coordinate alignment..."));

    // 1단계: 캡처 시스템 재조정
    UpdateImprovedCaptureCamera();

    // 2단계: 좌표 변환 시스템 재초기화  
    WorldToMapScale = CalculateImprovedWorldToMapScale();

    // 3단계: UI 요소들 위치 재조정
    RefreshUIElementPositions();

    // 4단계: 캡처 새로고침
    if (bCaptureInitialized)
    {
        GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
            {
                CaptureMapBackground();
                UE_LOG(LogTemp, Warning, TEXT("✅ Background recaptured with fixed alignment"));

                // 정렬 검증 (멤버 변수 타이머 핸들 사용)
                GetWorld()->GetTimerManager().SetTimer(
                    CoordinateVerificationTimer,
                    [this]() { DebugCoordinateMatching(); },
                    0.5f,
                    false
                );
            });
    }

    UE_LOG(LogTemp, Warning, TEXT("✅ Coordinate alignment auto-fix completed"));
}
// 4. 개선된 맵 중심점 계산
FVector UGolfMiniMap::CalculateImprovedMapCenter() const
{
    // 티와 홀컵의 중점을 기본 중심으로 설정
    FVector BasicCenter = (TeeWorldPosition + HolecupWorldPosition) * 0.5f;

    // 홀 거리에 따른 여유 공간 계산 (10%)
    float HoleDistance = FVector::Dist(TeeWorldPosition, HolecupWorldPosition);

    // 수직 정렬에서는 중심점을 조금 더 정확하게 계산
    // 홀컵 방향으로 살짝 치우치게 하지 않고 정확한 중점 사용
    return BasicCenter;
}

// 5. 개선된 스케일 계산
float UGolfMiniMap::CalculateImprovedWorldToMapScale() const
{
    // 홀 거리 계산
    float HoleDistance = FVector::Dist(TeeWorldPosition, HolecupWorldPosition);

    UE_LOG(LogTemp, Log, TEXT("🔢 Scale Calculation:"));
    UE_LOG(LogTemp, Log, TEXT("   Hole Distance: %.1f cm"), HoleDistance);

    // ⭐ 미니맵에서 홀이 차지할 비율 (50-70%가 적당)
    // ⚠️ TODO: 주석은 "60% 사용"이라고 되어있지만 실제 값은 2.0(=200%)입니다.
    //          이 값이 배경 캡처 확대/축소의 기준이 되므로, 실제 게임에서 확인 후
    //          의도한 값(0.6f 등)으로 조정이 필요할 수 있습니다. 현재는 기존 동작을
    //          유지하기 위해 값을 그대로 두었습니다.
    float HoleUsageRatio = 2.0f; // 60% 사용

    // 미니맵의 작은 축을 기준으로 계산 (정사각형이 아닐 수 있음)
    float MinMapSize = FMath::Min(MapWidth, MapHeight);
    float UsableMapSize = MinMapSize * HoleUsageRatio;

    // 기본 스케일 계산
    float BaseScale = UsableMapSize / HoleDistance;

    // ⭐ 스케일 제한 (너무 크거나 작지 않도록)
    float MinScale = 0.1f;  // 최소 스케일 (1cm = 0.05px)
    float MaxScale = 4.0f;   // 최대 스케일 (1cm = 2px)

    float LimitedScale = FMath::Clamp(BaseScale, MinScale, MaxScale);

    // 사용자 설정 MapScale 적용
    float FinalScale = LimitedScale * MapScale;

    UE_LOG(LogTemp, Log, TEXT("   Min Map Size: %.1f px"), MinMapSize);
    UE_LOG(LogTemp, Log, TEXT("   Usable Size: %.1f px"), UsableMapSize);
    UE_LOG(LogTemp, Log, TEXT("   Base Scale: %.6f"), BaseScale);
    UE_LOG(LogTemp, Log, TEXT("   Limited Scale: %.6f"), LimitedScale);
    UE_LOG(LogTemp, Log, TEXT("   User MapScale: %.2f"), MapScale);
    UE_LOG(LogTemp, Log, TEXT("   Final Scale: %.6f"), FinalScale);

    return FinalScale;
}

// 6. 최소 필요 캡처 반경 계산 (티/홀컵/OB 라인이 화면 밖으로 잘리지 않기 위한 하한선)
float UGolfMiniMap::CalculateMinRequiredCaptureRadius() const
{
    float AspectRatio = MapWidth / MapHeight;

    TArray<FVector> AllPoints;
    AllPoints.Add(TeeWorldPosition);
    AllPoints.Add(HolecupWorldPosition);
    for (const FVector& OBPoint : CurrentOBPoints)
        AllPoints.Add(OBPoint);

    float MaxNeedX = 0.f;
    float MaxNeedY = 0.f;

    for (const FVector& Point : AllPoints)
    {
        // 맵 중심 기준 상대 좌표
        FVector2D Rel(
            Point.X - MapCenterWorldPosition.X,
            Point.Y - MapCenterWorldPosition.Y
        );

        // 카메라 회전 기준 로컬 좌표로 변환 (WorldToMapPosition과 동일 방향)
        FVector2D Rotated = RotatePoint(Rel, -HoleRotationAngle);

        // X축은 AspectRatio 반영해서 Y 기준으로 환산
        float NeedX = FMath::Abs(Rotated.X) / AspectRatio;
        float NeedY = FMath::Abs(Rotated.Y);

        MaxNeedX = FMath::Max(MaxNeedX, NeedX);
        MaxNeedY = FMath::Max(MaxNeedY, NeedY);
    }

    return FMath::Max(MaxNeedX, MaxNeedY);
}

// 7. MapWidth/MapHeight 비율을 유지하는 렌더타겟 해상도 계산 (BaseResolution을 "높이" 기준으로 사용)
FIntPoint UGolfMiniMap::CalculateRenderTargetSize(int32 BaseResolution) const
{
    const float AspectRatio = (MapHeight > 0.0f) ? (MapWidth / MapHeight) : 1.0f;
    const int32 RTHeight = FMath::Max(BaseResolution, 8);
    const int32 RTWidth = FMath::Max(FMath::RoundToInt(RTHeight * AspectRatio), 8);
    return FIntPoint(RTWidth, RTHeight);
}

// 8. 개선된 캡처 카메라 설정 (⭐ WorldToMapScale 축척과 배경 캡처 반경을 동기화)
void UGolfMiniMap::UpdateImprovedCaptureCamera()
{
    if (!SceneCaptureComponent || !bCaptureInitialized)
        return;

    UE_LOG(LogTemp, Warning, TEXT("📸 Setting up scale-synced Orthographic capture camera..."));

    // ⭐ Perspective → Orthographic 전환
    //    Perspective는 카메라로부터의 "거리"에 비례해 화면 위치가 달라지므로,
    //    지형 고저차(Z)가 있는 홀에서는 실제 캡처된 지형 픽셀 위치와
    //    WorldToMapPosition()이 계산하는 마커(공/홀컵/OB/에임) 좌표가 어긋납니다.
    //    (WorldToMapPosition은 Z를 무시하고 XY 평면 선형 매핑만 사용하기 때문)
    //    Orthographic은 투영이 거리와 무관하게 항상 선형이라 이 어긋남이 없어집니다.
    SceneCaptureComponent->ProjectionType = ECameraProjectionMode::Orthographic;

    // ⭐ "계산된 축척"(WorldToMapScale, px/cm)을 최신 상태로 갱신
    WorldToMapScale = CalculateImprovedWorldToMapScale();

    float AspectRatio = MapWidth / MapHeight;

    // ⭐ 축척 → 캡처 반경 역산: WorldToMapPosition의 정규화 공식(MapY = NormalizedY * MapHeight/2)을 뒤집으면
    //    px/cm 축척 = (MapHeight/2) / CaptureRadius  →  CaptureRadius = (MapHeight/2) / 축척
    float DesiredCaptureRadius = (MapHeight * 0.5f) / FMath::Max(WorldToMapScale, KINDA_SMALL_NUMBER);

    // 티/홀컵/OB가 화면 밖으로 잘리지 않기 위한 최소 필요 반경 (안전장치)
    float MinRequiredRadius = CalculateMinRequiredCaptureRadius();

    // ⭐ 축척 기반 반경을 우선 사용하되, 잘림이 생기면 최소 필요 반경으로 보정
    float RequiredRadius = FMath::Max(DesiredCaptureRadius, MinRequiredRadius);

    if (DesiredCaptureRadius < MinRequiredRadius)
    {
        UE_LOG(LogTemp, Warning, TEXT("   ⚠️ 계산된 축척이 과도하게 확대되어 있어 최소 필요 반경으로 보정됨 (Desired=%.1f, MinRequired=%.1f)"),
            DesiredCaptureRadius, MinRequiredRadius);
    }

    // ⭐ Perspective→Orthographic 전환으로 인해 화면이 이전보다 확대되어 보이는 것을 보정하는 여유 배율
    //    (Perspective는 카메라 아래쪽(낮은 지형)일수록 실제보다 더 넓게 찍혀서 상대적으로 축소되어 보였던 것과 달리,
    //     Orthographic은 항상 계산값 그대로 꽉 차게 캡처하므로 CaptureViewMargin으로 여유를 줘야 체감 화각이 비슷해짐)
    RequiredRadius *= FMath::Max(CaptureViewMargin, 1.0f);

    CaptureWorldSize = RequiredRadius * 2.0f;
    CaptureWorldCenter = MapCenterWorldPosition;

    // ⭐ Orthographic은 카메라 "거리"가 확대/축소에 영향을 주지 않으므로,
    //    지형/오브젝트를 안 잘리게 덮을 정도의 여유 높이만 확보하면 됩니다.
    //    (기존 CaptureHeight는 Perspective FOV 역산용이었지만, 여기서는 단순 clearance 용도로만 사용)
    CaptureHeight = FMath::Max(CaptureHeight, 3000.0f);

    FVector CameraLocation = MapCenterWorldPosition + FVector(0, 0, CaptureHeight);
    float CameraYaw = FMath::RadiansToDegrees(HoleRotationAngle) + 90.0f;
    FRotator CameraRotation = FRotator(-90.0f, CameraYaw, 0.0f);

    SceneCaptureComponent->SetWorldLocation(CameraLocation);
    SceneCaptureComponent->SetWorldRotation(CameraRotation);

    // ⭐ 핵심: 렌더타겟 종횡비(=MapWidth:MapHeight, CalculateRenderTargetSize()로 통일해둔 값)를 전제로
    //    OrthoWidth(가로 폭, cm)를 설정하면 세로는 UE가 자동으로 OrthoWidth/AspectRatio로 계산합니다.
    //    이때 세로 값이 CaptureWorldSize(= MapHeight에 대응하는 실제 월드 폭)와 일치해야
    //    배경 이미지와 WorldToMapPosition() 마커 좌표의 축척이 정확히 맞습니다.
    SceneCaptureComponent->OrthoWidth = CaptureWorldSize * AspectRatio;

    UE_LOG(LogTemp, Warning, TEXT("   WorldToMapScale: %.6f px/cm"), WorldToMapScale);
    UE_LOG(LogTemp, Warning, TEXT("   DesiredCaptureRadius: %.1f cm, MinRequired: %.1f cm, Margin: %.2fx, Final: %.1f cm"),
        DesiredCaptureRadius, MinRequiredRadius, CaptureViewMargin, RequiredRadius);
    UE_LOG(LogTemp, Warning, TEXT("   Camera Location: %s"), *CameraLocation.ToString());
    UE_LOG(LogTemp, Warning, TEXT("   Camera Rotation: %s"), *CameraRotation.ToString());
    UE_LOG(LogTemp, Warning, TEXT("   Camera Clearance Height: %.1f cm"), CaptureHeight);
    UE_LOG(LogTemp, Warning, TEXT("   OrthoWidth: %.1f cm"), SceneCaptureComponent->OrthoWidth);
    UE_LOG(LogTemp, Warning, TEXT("   CaptureWorldSize(Height 기준): %.1f cm"), CaptureWorldSize);
    UE_LOG(LogTemp, Warning, TEXT("   Hole Rotation Angle: %.1f degrees"), FMath::RadiansToDegrees(HoleRotationAngle));
}
// 8. UI 요소 위치 새로고침
void UGolfMiniMap::RefreshUIElementPositions()
{
    UE_LOG(LogTemp, Warning, TEXT("🔄 Refreshing UI element positions..."));

    //if (TeeImage)
    //    UpdateImagePosition(TeeImage, TeeWorldPosition, TeeColor);

    if (HolecupImage)
        UpdateImagePosition(HolecupImage, HolecupWorldPosition, HolecupColor);

    // ⭐ 수정: 모든 플레이어 볼 이미지 위치 업데이트
    for (auto& Elem : PlayerBallImages)
    {
        int32 PlayerIndex = Elem.Key;
        UImage* BallImageToUpdate = Elem.Value;
        FVector BallPos = PlayerBallWorldPositions.Contains(PlayerIndex) ? PlayerBallWorldPositions[PlayerIndex] : FVector::ZeroVector;
        FLinearColor BallCol = PlayerBallColors.Contains(PlayerIndex) ? PlayerBallColors[PlayerIndex] : FLinearColor::White;
        UpdateImagePosition(BallImageToUpdate, BallPos, BallCol, PlayerIndex);
    }


    UpdateFlagPosition();
    // UpdateAimLinePosition(); // 개별 플레이어 에임 라인 업데이트 로직으로 대체됨
    if (CurrentOBPoints.Num() >= 3)
    {
        UpdateOBLines(CurrentOBPoints);
    }

    UE_LOG(LogTemp, Warning, TEXT("✅ UI elements refreshed"));
}




// 1. 미니맵 스케일 프리셋 함수들
void UGolfMiniMap::SetMiniMapZoomLevel(int32 ZoomLevel)
{
    float ScaleMultiplier = 1.0f;

    switch (ZoomLevel)
    {
    case 1: // 매우 넓게 (전체 홀 + 주변)
        ScaleMultiplier = 0.3f;
        break;
    case 2: // 넓게 (홀 전체 + 약간의 여유)
        ScaleMultiplier = 0.5f;
        break;
    case 3: // 보통 (홀이 60% 차지)
        ScaleMultiplier = 1.0f;
        break;
    case 4: // 가깝게 (홀이 80% 차지)
        ScaleMultiplier = 1.5f;
        break;
    case 5: // 매우 가깝게 (홀 주변만)
        ScaleMultiplier = 2.0f;
        break;
    default:
        ScaleMultiplier = 1.0f;
        break;
    }

    MapScale = ScaleMultiplier;
    AdjustMiniMapScale(1.0f); // 스케일 재계산 및 업데이트

    UE_LOG(LogTemp, Log, TEXT("🔍 MiniMap zoom level set to %d (Scale: %.2f)"), ZoomLevel, MapScale);
}

// 2. 캡처 품질 설정
void UGolfMiniMap::SetCaptureQuality(int32 QualityLevel)
{
    int32 NewResolution = 512;
    float NewFOV = 75.0f;

    switch (QualityLevel)
    {
    case 1: // 낮은 품질 (성능 우선)
        NewResolution = 256;
        NewFOV = 80.0f;
        break;
    case 2: // 보통 품질
        NewResolution = 512;
        NewFOV = 75.0f;
        break;
    case 3: // 높은 품질
        NewResolution = 1024;
        NewFOV = 70.0f;
        break;
    case 4: // 최고 품질 (품질 우선)
        NewResolution = 2048;
        NewFOV = 65.0f;
        break;
    default:
        NewResolution = 512;
        NewFOV = 75.0f;
        break;
    }

    // 렌더 타겟 해상도 변경 (⭐ MapWidth/MapHeight 비율 유지)
    RenderTargetResolution = NewResolution;
    if (MapRenderTarget)
    {
        const FIntPoint RTSize = CalculateRenderTargetSize(NewResolution);
        MapRenderTarget->InitAutoFormat(RTSize.X, RTSize.Y);
        MapRenderTarget->UpdateResourceImmediate(true);
    }

    // FOV 변경
    if (SceneCaptureComponent)
    {
        SceneCaptureComponent->FOVAngle = NewFOV;
        UpdateImprovedCaptureCamera(); // 카메라 재설정

        // 캡처 새로고침
        GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
            {
                CaptureMapBackground();
            });
    }

    UE_LOG(LogTemp, Log, TEXT("📸 Capture quality set to level %d (Resolution: %d, FOV: %.1f°)"),
        QualityLevel, NewResolution, NewFOV);
}

// 3. 미니맵 자동 조정 (홀 크기에 맞춤)
void UGolfMiniMap::AutoFitToHole()
{
    if (!bIsInitialized)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ MiniMap not initialized for auto-fit"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("🎯 Auto-fitting MiniMap to current hole..."));

    // 홀 거리 계산
    float HoleDistance = FVector::Dist(TeeWorldPosition, HolecupWorldPosition);

    // 적절한 스케일 계산 (홀이 미니맵의 60% 차지하도록)
    float TargetUsage = 0.6f;
    float MinMapSize = FMath::Min(MapWidth, MapHeight);
    float TargetPixelDistance = MinMapSize * TargetUsage;
    float OptimalScale = TargetPixelDistance / HoleDistance;

    // 스케일 범위 제한
    OptimalScale = FMath::Clamp(OptimalScale, 0.1f, 3.0f);

    // 적용
    MapScale = OptimalScale;
    AdjustMiniMapScale(1.0f);

    UE_LOG(LogTemp, Log, TEXT("✅ Auto-fit completed - Hole: %.1fm, Scale: %.3f"),
        HoleDistance / 100.0f, OptimalScale);
}

// 4. 디버그 정보 화면 표시
void UGolfMiniMap::ShowDebugInfo(bool bShow)
{
#if WITH_EDITOR

    if (!GEngine) return;
#endif
    if (bShow)
    {
        // 현재 미니맵 상태 정보 화면에 표시
        float HoleDistance = FVector::Dist(TeeWorldPosition, HolecupWorldPosition);
        float CaptureRadius = CaptureWorldSize * 0.5f;

        FString DebugText = FString::Printf(
            TEXT("=== MiniMap Debug Info ===\n")
            TEXT("Map Size: %.0f x %.0f px\n")
            TEXT("Map Scale: %.3f\n")
            TEXT("World→Map Scale: %.6f\n")
            TEXT("Hole Distance: %.1f m\n")
            TEXT("Capture Type: %s\n")
            TEXT("FOV: %.1f°\n")
            TEXT("Camera Height: %.1f m\n")
            TEXT("Capture Radius: %.1f m"),
            MapWidth, MapHeight,
            MapScale,
            WorldToMapScale,
            HoleDistance / 100.0f,
            SceneCaptureComponent && SceneCaptureComponent->ProjectionType == ECameraProjectionMode::Perspective ? TEXT("Perspective") : TEXT("Orthographic"),
            SceneCaptureComponent ? SceneCaptureComponent->FOVAngle : 0.0f,
            CaptureHeight / 100.0f,
            CaptureRadius / 100.0f
        );
#if WITH_EDITOR

        GEngine->AddOnScreenDebugMessage(999, 10.0f, FColor::Cyan, DebugText);
#endif
    }
    else
    {
#if WITH_EDITOR

        // 디버그 메시지 제거
        GEngine->RemoveOnScreenDebugMessage(999);
#endif
    }
}

// 5. 미니맵 리셋 함수
void UGolfMiniMap::ResetMiniMapSettings()
{
    UE_LOG(LogTemp, Log, TEXT("🔄 Resetting MiniMap to default settings..."));

    // 기본값으로 리셋
    MapScale = 1.0f;
    CaptureHeight = 2000.0f;
    RenderTargetResolution = 512;

    if (SceneCaptureComponent)
    {
        SceneCaptureComponent->FOVAngle = 75.0f;
        SceneCaptureComponent->ProjectionType = ECameraProjectionMode::Perspective;
    }

    // 렌더 타겟 재생성 (⭐ MapWidth/MapHeight 비율 유지)
    if (MapRenderTarget)
    {
        const FIntPoint RTSize = CalculateRenderTargetSize(RenderTargetResolution);
        MapRenderTarget->InitAutoFormat(RTSize.X, RTSize.Y);
        MapRenderTarget->UpdateResourceImmediate(true);
    }

    // 미니맵 재초기화
    if (bIsInitialized)
    {
        InitializeMiniMap(TeeWorldPosition, HolecupWorldPosition);
    }

    UE_LOG(LogTemp, Log, TEXT("✅ MiniMap reset completed"));
}

// 6. 성능 모드 설정
void UGolfMiniMap::SetPerformanceMode(bool bHighPerformance)
{
    if (bHighPerformance)
    {
        UE_LOG(LogTemp, Log, TEXT("⚡ Switching to high performance mode"));

        // 성능 우선 설정
        SetCaptureQuality(1); // 낮은 품질
        bAutoRefreshCapture = false; // 자동 새로고침 비활성화
        RefreshInterval = 10.0f; // 새로고침 간격 증가

        // 캡처 빈도 제한
        bEnableAdaptiveCapture = true;
        MinCaptureInterval = 2.0f; // 2초에 한 번만 캡처
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("🎨 Switching to high quality mode"));

        // 품질 우선 설정
        SetCaptureQuality(3); // 높은 품질
        bAutoRefreshCapture = true; // 자동 새로고침 활성화
        RefreshInterval = 5.0f; // 새로고침 간격 감소

        // 캡처 빈도 완화
        bEnableAdaptiveCapture = false;
        MinCaptureInterval = 0.5f; // 0.5초에 한 번 캡처 가능
    }

    // 설정 적용
    UpdateImprovedCaptureCamera();
    RefreshMapBackground();
}

// ⭐ 새로 추가: 플레이어를 미니맵에 추가 (볼 이미지 및 에임 라인 생성)
void UGolfMiniMap::AddPlayerToMiniMap(int32 PlayerIndex, const FVector& InitialBallPosition, const FLinearColor& BallColor)
{
    if (!MiniMapCanvas)
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot add player %d to minimap: MiniMapCanvas is null."), PlayerIndex);
        return;
    }

    if (PlayerBallImages.Contains(PlayerIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("Player %d already exists on minimap, updating position instead."), PlayerIndex);
        UpdateBallPosition(PlayerIndex, InitialBallPosition);
        PlayerBallColors.Add(PlayerIndex, BallColor);
        return;
    }

    // 볼 이미지 생성 (기존 코드)
    UImage* NewBallImage = NewObject<UImage>(this, FName(*FString::Printf(TEXT("BallImage_%d"), PlayerIndex)));
    if (!NewBallImage)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create ball image for player %d."), PlayerIndex);
        return;
    }

    // 플레이어별 텍스처 적용 (기존 코드)
    UTexture2D* PlayerTexture = GetPlayerBallTexture(PlayerIndex);
    if (PlayerTexture)
    {
        FSlateBrush BallBrush;
        BallBrush.SetResourceObject(PlayerTexture);
        BallBrush.DrawAs = ESlateBrushDrawType::Image;
        BallBrush.Tiling = ESlateBrushTileType::NoTile;
        BallBrush.ImageSize = FVector2D(16.0f, 16.0f);
        BallBrush.TintColor = FSlateColor(BallColor);

        PlayerBallBrushes.Add(PlayerIndex, BallBrush);
        NewBallImage->SetBrush(BallBrush);
    }
    else
    {
        NewBallImage->SetColorAndOpacity(BallColor);
    }

    PlayerBallImages.Add(PlayerIndex, NewBallImage);
    PlayerBallColors.Add(PlayerIndex, BallColor);

    UCanvasPanelSlot* BallSlot = MiniMapCanvas->AddChildToCanvas(NewBallImage);
    if (BallSlot)
    {
        BallSlot->SetAutoSize(true);
        BallSlot->SetZOrder(1);
    }

    UpdateImagePosition(NewBallImage, InitialBallPosition, BallColor, PlayerIndex);

    // ★ 중요: 기존 에임 라인 생성 (누락된 부분)
    CreateAimLineForPlayer(PlayerIndex);

    // AimActor 아이콘 생성
    CreateAimActorForPlayer(PlayerIndex);

    // 볼-홀컵 라인 생성
    CreateBallToHoleLineForPlayer(PlayerIndex);

    UE_LOG(LogTemp, Log, TEXT("Added player %d to minimap with all line systems"), PlayerIndex);
}
void UGolfMiniMap::CreateAimLineForPlayer(int32 PlayerIndex)
{
    if (!MiniMapCanvas)
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot create aim line: MiniMapCanvas is null"));
        return;
    }


    // 기존 에임 라인 이미지 생성
    UImage* NewAimLineImage = NewObject<UImage>(this,
        FName(*FString::Printf(TEXT("AimLineImage_%d"), PlayerIndex)));

    if (!NewAimLineImage)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create aim line image for player %d"), PlayerIndex);
        return;
    }


    // 캔버스에 추가
    UCanvasPanelSlot* AimLineSlot = MiniMapCanvas->AddChildToCanvas(NewAimLineImage);
    if (AimLineSlot)
    {
        AimLineSlot->SetAutoSize(true);
        AimLineSlot->SetZOrder(2); // 볼보다 위에 표시
    }

    // 초기 색상 설정
    NewAimLineImage->SetColorAndOpacity(AimLineColor);

    // 초기에는 숨김 (에임 방향이 설정되면 표시)
    NewAimLineImage->SetVisibility(ESlateVisibility::Hidden);

    UE_LOG(LogTemp, Log, TEXT("Created aim line for player %d"), PlayerIndex);
}

// ⭐ 새로 추가: 플레이어를 미니맵에서 제거
void UGolfMiniMap::RemovePlayerFromMiniMap(int32 PlayerIndex)
{
    // 기존 볼 이미지 제거...
    if (PlayerBallImages.Contains(PlayerIndex))
    {
        if (UImage* BallImageToRemove = PlayerBallImages[PlayerIndex])
        {
            BallImageToRemove->RemoveFromParent();
            BallImageToRemove->ConditionalBeginDestroy();
        }
        PlayerBallImages.Remove(PlayerIndex);
        PlayerBallWorldPositions.Remove(PlayerIndex);
        PlayerBallColors.Remove(PlayerIndex);
        PlayerBallBrushes.Remove(PlayerIndex);
    }

    // AimActor 제거
    RemoveAimActorForPlayer(PlayerIndex);

    // 볼-홀컵 라인 제거
    RemoveBallToHoleLineForPlayer(PlayerIndex);
}


FVector UGolfMiniMap::MapPositionToWorldPosition(const FVector2D& MapPosition) const
{
    if (!bIsInitialized)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ MiniMap not initialized for coordinate conversion"));
        return FVector::ZeroVector;
    }

    float MapX = MapPosition.X;
    float MapY = MapPosition.Y;

    // 좌우/상하 뒤집기 옵션 역적용
    if (bFlipMapHorizontally)
    {
        MapX = MapWidth - MapX;
    }
    if (bFlipMapVertically)
    {
        MapY = MapHeight - MapY;
    }

    // 미니맵 픽셀 좌표를 정규화된 좌표로 변환 (-1 ~ 1 범위)
    float NormalizedX = (MapX - (MapWidth * 0.5f)) / (MapWidth * 0.5f);
    float NormalizedY = -((MapY - (MapHeight * 0.5f)) / (MapHeight * 0.5f)); // Y축 뒤집기 역적용

    // 캡처 범위 기준으로 월드 상대 좌표 계산
    float CaptureRadius = CaptureWorldSize * 0.5f;
    FVector2D RelativePosition2D;
    float AspectRatio = MapWidth / MapHeight; // 313/475 ≈ 0.659
    RelativePosition2D.X = NormalizedX * (CaptureRadius * AspectRatio); // AspectRatio로 변경
    RelativePosition2D.Y = NormalizedY * CaptureRadius;

    // ⭐ 회전 변환의 역변환 적용 (양수로 정회전)
    FVector2D RotatedPosition = RotatePoint(RelativePosition2D, HoleRotationAngle);

    // 3D 월드 좌표로 변환
    FVector RelativePosition(RotatedPosition.X, RotatedPosition.Y, 0.0f);

    // 맵 중심 기준으로 실제 월드 좌표 계산
    FVector WorldPosition = MapCenterWorldPosition + RelativePosition;

    return WorldPosition;
}


FVector2D UGolfMiniMap::WorldToMapPosition(const FVector& WorldPosition) const
{
    if (!bIsInitialized)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ MiniMap not initialized"));
        return FVector2D::ZeroVector;
    }

    if (WorldPosition.ContainsNaN())
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Invalid WorldPosition (NaN): %s"), *WorldPosition.ToString());
        return FVector2D::ZeroVector;
    }

    // 맵 중심 기준 상대 좌표
    FVector RelativePosition = WorldPosition - MapCenterWorldPosition;

    // ⭐ 홀 방향에 따른 회전 변환 적용 (음수로 역회전)
    FVector2D RelativePosition2D(RelativePosition.X, RelativePosition.Y);
    FVector2D RotatedPosition = RotatePoint(RelativePosition2D, -HoleRotationAngle);

    // Perspective 캡처 범위 기준으로 정규화
    float CaptureRadius = CaptureWorldSize * 0.5f;
    if (CaptureRadius <= 0.0f)  // 0 나누기 방지
        return FVector2D(MapWidth * 0.5f, MapHeight * 0.5f);

    // ⭐ 0.68f 매직넘버 제거 → 맵 가로세로 비율 기반으로 계산
    float AspectRatio = MapWidth / MapHeight; // 예: 313/475 ≈ 0.659
    float NormalizedX = RotatedPosition.X / (CaptureRadius * AspectRatio);
    float NormalizedY = RotatedPosition.Y / CaptureRadius;

    NormalizedX = FMath::Clamp(NormalizedX, -1.0f, 1.0f);
    NormalizedY = FMath::Clamp(NormalizedY, -1.0f, 1.0f);

    // ⭐ 미니맵 픽셀 좌표로 변환 (Y축 뒤집기로 티가 아래쪽에 위치)
    float MapX = (NormalizedX * MapWidth * 0.5f) + (MapWidth * 0.5f);
    float MapY = (-NormalizedY * MapHeight * 0.5f) + (MapHeight * 0.5f); // Y축 뒤집기

    // 좌우/상하 뒤집기 옵션 적용
    if (bFlipMapHorizontally)
    {
        MapX = MapWidth - MapX;
    }
    if (bFlipMapVertically)
    {
        MapY = MapHeight - MapY;
    }

    // 최종 경계 체크
    MapX = FMath::Clamp(MapX, 0.0f, MapWidth);
    MapY = FMath::Clamp(MapY, 0.0f, MapHeight);

    return FVector2D(MapX, MapY);
}




bool UGolfMiniMap::IsPointInMiniMapBounds(const FVector2D& MapPosition) const
{
    return (MapPosition.X >= 0.0f && MapPosition.X <= MapWidth &&
        MapPosition.Y >= 0.0f && MapPosition.Y <= MapHeight);
}


FReply UGolfMiniMap::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{

    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        // 마우스 위치를 로컬 좌표계로 변환
        FVector2D LocalClickPosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

        UE_LOG(LogTemp, Log, TEXT("🖱️ Mouse down at local position: (%.1f, %.1f)"),
            LocalClickPosition.X, LocalClickPosition.Y);

        // 클릭이 미니맵 영역 내인지 확인
        if (IsClickOnMiniMapArea(LocalClickPosition))
        {
            UE_LOG(LogTemp, Warning, TEXT(" Click Position: %s"), *LocalClickPosition.ToString());
            // 미니맵 캔버스 기준 상대 좌표로 변환
            FVector2D MiniMapPosition = LocalClickPosition;

            // MiniMapCanvas가 위젯 내에서 특정 위치에 있다면 오프셋 조정이 필요할 수 있음
            if (MiniMapCanvas)
            {
                // 캔버스의 실제 위치 고려한 좌표 조정
                // 필요에 따라 추가 계산 수행
            }
            OnMiniMapClicked(MiniMapPosition);

            // 이벤트 처리됨을 표시
            return FReply::Handled();
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("---------------------->  MinimapOut Area "));
            return FReply::Unhandled();
        }
    }

    // 다른 영역이나 다른 버튼 클릭은 처리하지 않음
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);

}

FReply UGolfMiniMap::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    // 필요한 경우 마우스 업 이벤트도 처리
    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

bool UGolfMiniMap::IsClickOnMiniMapArea(const FVector2D& LocalClickPosition) const
{
    if (!MiniMapCanvas) return false;

    const FGeometry& CanvasGeometry = MiniMapCanvas->GetCachedGeometry();
    FVector2D CanvasSize = CanvasGeometry.GetLocalSize();

    // 만약 Canvas Position이 (0,0)이라면, LocalClickPosition을 직접 사용 (오프셋 고려)
    UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(MiniMapCanvas->Slot);
    FVector2D CanvasOffset = CanvasSlot ? CanvasSlot->GetPosition() : FVector2D::ZeroVector;

    FVector2D AdjustedPosition = LocalClickPosition - CanvasOffset;  // 오프셋 보정

    bool bInArea = (AdjustedPosition.X >= 0.0f && AdjustedPosition.X <= CanvasSize.X &&
        AdjustedPosition.Y >= 0.0f && AdjustedPosition.Y <= CanvasSize.Y);

    UE_LOG(LogTemp, Log, TEXT("🖱️ Canvas Click Check - Adjusted:(%.1f,%.1f), Canvas:(%.1fx%.1f), InArea:%s"),
        AdjustedPosition.X, AdjustedPosition.Y, CanvasSize.X, CanvasSize.Y, bInArea ? TEXT("YES") : TEXT("NO"));

    return bInArea;
}

// 추가 편의 함수들
UFUNCTION(BlueprintCallable, Category = "MiniMap Click")
FVector UGolfMiniMap::GetWorldPositionFromMiniMapClick(float MapX, float MapY) const
{
    return MapPositionToWorldPosition(FVector2D(MapX, MapY));
}

UFUNCTION(BlueprintCallable, Category = "MiniMap Click")
void UGolfMiniMap::SetMiniMapClickEnabled(bool bEnabled)
{
    if (bEnabled)
    {
        SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        // 마우스 이벤트 활성화
    }
    else
    {
        // 클릭 비활성화하되 표시는 유지
        SetVisibility(ESlateVisibility::Visible);
    }
}

UFUNCTION(BlueprintCallable, Category = "MiniMap Click")
void UGolfMiniMap::TestCoordinateConversion()
{
    if (!bIsInitialized)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ MiniMap not initialized for coordinate test"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("🧪 Testing coordinate conversion (Map ↔ World)..."));

    // 테스트 포인트들 (미니맵 픽셀 좌표)
    TArray<FVector2D> TestMapPoints;
    TestMapPoints.Add(FVector2D(MapWidth * 0.5f, MapHeight * 0.5f));  // 중심
    TestMapPoints.Add(FVector2D(0, 0));                                // 좌상단
    TestMapPoints.Add(FVector2D(MapWidth, 0));                         // 우상단
    TestMapPoints.Add(FVector2D(0, MapHeight));                        // 좌하단
    TestMapPoints.Add(FVector2D(MapWidth, MapHeight));                 // 우하단
    TestMapPoints.Add(FVector2D(MapWidth * 0.25f, MapHeight * 0.75f)); // 임의 점

    TArray<FString> PointNames = {
        TEXT("Center"), TEXT("TopLeft"), TEXT("TopRight"),
        TEXT("BottomLeft"), TEXT("BottomRight"), TEXT("Random")
    };

    for (int32 i = 0; i < TestMapPoints.Num(); i++)
    {
        FVector2D MapPos = TestMapPoints[i];
        FVector WorldPos = MapPositionToWorldPosition(MapPos);
        FVector2D BackToMapPos = WorldToMapPosition(WorldPos);

        FVector2D ConversionError = BackToMapPos - MapPos;
        float ErrorMagnitude = ConversionError.Size();

        bool bAccurate = ErrorMagnitude < 2.0f; // 2픽셀 오차 허용

        UE_LOG(LogTemp, Warning, TEXT("   %s:"), *PointNames[i]);
        UE_LOG(LogTemp, Warning, TEXT("     Map: (%.1f, %.1f)"), MapPos.X, MapPos.Y);
        UE_LOG(LogTemp, Warning, TEXT("     World: %s"), *WorldPos.ToString());
        UE_LOG(LogTemp, Warning, TEXT("     Back to Map: (%.1f, %.1f)"), BackToMapPos.X, BackToMapPos.Y);
        UE_LOG(LogTemp, Warning, TEXT("     Conversion Error: %.2f pixels %s"),
            ErrorMagnitude, bAccurate ? TEXT("✅") : TEXT("❌"));
    }

    UE_LOG(LogTemp, Warning, TEXT("✅ Coordinate conversion test completed"));
}

void UGolfMiniMap::OnMiniMapClicked(const FVector2D& ClickPosition)
{
    if (!bIsInitialized || !MiniMapCanvas)
    {
        return;
    }

    if (GameMode->GetCurrentTurnGolfPlayer()->GetPlayerState() != EPlayerState::Player_Ready)
        return;

    // 클릭 위치를 MiniMapCanvas 좌표계로 변환
    const FGeometry& CanvasGeometry = MiniMapCanvas->GetCachedGeometry();
    FVector2D CanvasClickPosition = CanvasGeometry.AbsoluteToLocal(
        GetCachedGeometry().LocalToAbsolute(ClickPosition)
    );

    // 미니맵 좌표 유효성 검사
    FVector2D CanvasSize = CanvasGeometry.GetLocalSize();
    if (CanvasClickPosition.X < 0.0f || CanvasClickPosition.X > CanvasSize.X ||
        CanvasClickPosition.Y < 0.0f || CanvasClickPosition.Y > CanvasSize.Y)
    {
        UE_LOG(LogTemp, Warning, TEXT("Minimap Click position outside canvas bounds"));
        return;
    }



    // 월드 좌표로 변환
    FVector ClickedWorldPosition = MapPositionToWorldPosition(CanvasClickPosition);

    UE_LOG(LogTemp, Log, TEXT("MiniMap clicked - Canvas:(%.1f,%.1f) → World:%s"),
        CanvasClickPosition.X, CanvasClickPosition.Y, *ClickedWorldPosition.ToString());

    // 홀컵 거리 제한 적용 (선택적)
   // FVector FinalAimPosition = ApplyHolecupDistanceLimit(ClickedWorldPosition);
    FVector FinalAimPosition = ClickedWorldPosition;
    // 기존 처리 로직
    if (GameMode)
    {
        EGolfGameMode CurrentMode = GameMode->GetCurrentGameMode();
        switch (CurrentMode)
        {
        case EGolfGameMode::StrokeMode:
            MoveToMouseTip2(CanvasClickPosition);
            HandleStrokeModeClick(FinalAimPosition);
            break;
        case EGolfGameMode::TrainingMode:
            HandleTrainingModeClick(FinalAimPosition);
            break;
        default:
            MoveToMouseTip2(CanvasClickPosition);
            HandleStrokeModeClick(FinalAimPosition);
            break;
        }
    }

    // 델리게이트 이벤트
    if (OnMiniMapClickedEvent.IsBound())
    {
        OnMiniMapClickedEvent.Broadcast(FinalAimPosition);
    }
}

// ⭐ Stroke Mode 클릭 처리 (에임 이동)
void UGolfMiniMap::HandleStrokeModeClick(const FVector& WorldPosition)
{

    UE_LOG(LogTemp, Log, TEXT("Stroke Mode: Setting aim to clicked position with holecup limit"));

    if (!GameMode || !GameMode->PlayerManager)
    {
        UE_LOG(LogTemp, Error, TEXT("GameMode or PlayerManager is null"));
        return;
    }

    // 현재 플레이어 볼 위치 가져오기
    TArray<AGolfBall*> PlayerBalls = GameMode->PlayerManager->GetPlayerBalls();
    if (!PlayerBalls.IsValidIndex(GameMode->CurrentPlayerIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid CurrentPlayerIndex: %d"), GameMode->CurrentPlayerIndex);
        return;
    }

    AGolfBall* CurrentBall = PlayerBalls[GameMode->CurrentPlayerIndex];
    if (!IsValid(CurrentBall))
    {
        UE_LOG(LogTemp, Error, TEXT("Current ball is invalid"));
        return;
    }

    // ⭐ 홀컵 거리 제한 적용 (기존 함수 재사용)
    FVector FinalAimPosition = ApplyHolecupDistanceLimit(WorldPosition);

    // GameMode의 AimLocation 업데이트 (제한된 위치로)
    GameMode->AimLocation = FinalAimPosition;
    GameMode->UpdateMiniMapAimLine();

    // PlayerController에게 제한된 위치로 에임 설정 요청
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
    {
        if (AGolfPlayerController* GolfPC = Cast<AGolfPlayerController>(PC))
        {
            GolfPC->SetAimToExactPosition(FinalAimPosition);
            // GolfPC->SetAimToPosition(FinalAimPosition);
        }
    }

    UpdateTip2();
}

FVector UGolfMiniMap::ApplyHolecupDistanceLimit(const FVector& ClickedPosition)
{
    if (!GameMode || !GameMode->PlayerManager)
        return ClickedPosition;

    // 현재 볼 위치
    TArray<AGolfBall*> PlayerBalls = GameMode->PlayerManager->GetPlayerBalls();
    if (!PlayerBalls.IsValidIndex(GameMode->CurrentPlayerIndex))
        return ClickedPosition;

    AGolfBall* CurrentBall = PlayerBalls[GameMode->CurrentPlayerIndex];
    if (!CurrentBall)
        return ClickedPosition;

    FVector BallLocation = CurrentBall->GetActorLocation();

    // 홀컵 위치
    if (!GameMode->MapInfo.HolecupPositions.IsValidIndex(GameMode->CurrentHole - 1))
        return ClickedPosition;

    FVector HolecupLocation = GameMode->MapInfo.HolecupPositions[GameMode->CurrentHole - 1];

    // 거리 계산
    float ClickedDistance = FVector::Dist(BallLocation, ClickedPosition);
    float HolecupDistance = FVector::Dist(BallLocation, HolecupLocation);

    // 클릭한 위치가 홀컵보다 멀면 홀컵 위치로 제한
    if (ClickedDistance > HolecupDistance)
    {
        // ⭐ 수정: 홀컵 위치 그 자체로 제한
        UE_LOG(LogTemp, Log, TEXT("🎯 Aim limited to holecup position: %.1fm → %.1fm"),
            ClickedDistance / 100.0f, HolecupDistance / 100.0f);

        return HolecupLocation;
    }

    return ClickedPosition;
}


// ⭐ Training Mode 클릭 처리 (공 이동)
void UGolfMiniMap::HandleTrainingModeClick(const FVector& WorldPosition)
{
    UE_LOG(LogTemp, Log, TEXT("Training Mode: Attempting to move ball to clicked position"));

    if (!GameMode || !GameMode->PlayerManager)
    {
        UE_LOG(LogTemp, Error, TEXT("GameMode or PlayerManager is null"));
        return;
    }

    // 클릭 위치 유효성 체크 (OB 포함)
    if (!IsClickPositionValid(WorldPosition))
    {
        // OB 지역인지 별도로 체크해서 적절한 경고 표시
        if (bEnableOBCheck && IsPointInOBArea(WorldPosition))
        {
            ShowOBWarning(WorldPosition);
        }
        else
        {
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
                    TEXT("Invalid position for ball placement"));
            }
        }
        return;
    }

#if WITH_EDITOR
    // Training Mode 전용 추가 검증
    //if (!IsValidBallPlacement(WorldPosition))
    //{
    //    if (GEngine)
    //    {
    //        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange,
    //            TEXT("Cannot place ball at this position"));
    //    }
    //    return;
    //}
#endif

    // Training Mode는 단일 플레이어이므로 첫 번째 플레이어의 볼을 이동
    TArray<AGolfBall*> PlayerBalls = GameMode->PlayerManager->GetPlayerBalls();
    if (PlayerBalls.Num() == 0 || !IsValid(PlayerBalls[0]))
    {
        UE_LOG(LogTemp, Error, TEXT("No valid ball found in Training Mode"));
        return;
    }

    AGolfBall* TrainingBall = PlayerBalls[0];

    // 개선된 지형 높이 찾기
    FVector FinalPosition = FindGroundPosition(WorldPosition, TrainingBall);

    // 지면을 찾지 못한 경우 원래 높이 사용
    if (FinalPosition.IsZero())
    {
        FinalPosition = WorldPosition;
        UE_LOG(LogTemp, Warning, TEXT("Could not find ground position, using original height"));
    }

    // 볼을 새 위치로 이동
   // if(GameMode->CheckFirstShot()) 

    TrainingBall->SetActorLocation(FinalPosition);
    TrainingBall->SetBallState(EBallState::Ball_Ready);
    TrainingBall->UpdateCurrentLandType();

    // 물리 시뮬레이션 리셋
    if (TrainingBall->BallMesh)
    {
        TrainingBall->BallMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
        TrainingBall->BallMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    }

    // 미니맵 볼 위치 업데이트
    UpdateBallPosition(0, FinalPosition); // Training Mode는 플레이어 인덱스 0

    // 티샷이 아니면 홀컵바라보게
    if (!GameMode->CheckFirstShot())
    {
        // PlayerController에게 제한된 위치로 에임 설정 요청
        if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
        {
            if (AGolfPlayerController* GolfPC = Cast<AGolfPlayerController>(PC))
            {
                // GolfPC->SetAimToExactPosition(HolecupWorldPosition);
                GolfPC->SetAimToPosition(HolecupWorldPosition);
                //   GolfPC->AimActor->SetActorLocation(HolecupWorldPosition);


            }
        }
    }

    // 거리 정보 업데이트
    float Distance = FVector::Dist(FinalPosition, HolecupWorldPosition);
    float Elevation = HolecupWorldPosition.Z - FinalPosition.Z;
    UpdateDistanceAndElevation(Distance, Elevation);
    GameMode->StrokeWidgetInstance->UpdateAimInfo(Distance * 0.01f, Elevation * 0.01f);

    GameMode->PlayerManager->SetSensorClub(CR2CLUB_IRON7);

    UE_LOG(LogTemp, Log, TEXT("Training ball moved to ground position: %s (Ground height: %.1fcm)"),
        *FinalPosition.ToString(), FinalPosition.Z);

#if WITH_EDITOR
    // 화면에 성공 피드백 표시
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
            FString::Printf(TEXT("Ball placed on ground at height: %.1fm"), FinalPosition.Z / 100.0f));
    }
#endif
}


FVector UGolfMiniMap::FindGroundPosition(const FVector& TargetPosition, AActor* IgnoreActor)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return FVector::ZeroVector;
    }

    // Landscape 검색을 위한 넓은 범위 설정
    float SearchHeight = 5000.0f;  // 5미터 위아래로 검색
    FVector StartLocation = TargetPosition + FVector(0, 0, SearchHeight);
    FVector EndLocation = TargetPosition - FVector(0, 0, SearchHeight);

    FHitResult HitResult;
    FCollisionQueryParams CollisionParams;
    CollisionParams.bTraceComplex = true;  // Landscape의 복잡한 지형 고려
    CollisionParams.bReturnPhysicalMaterial = true;

    if (IgnoreActor)
    {
        CollisionParams.AddIgnoredActor(IgnoreActor);
    }

    // 정적/동적 메시를 무시하고 Landscape만 검색하기 위한 설정
    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic); // Landscape는 일반적으로 WorldStatic 채널 사용

    // Landscape 전용 검색을 위해 특별한 필터링 수행
    TArray<FHitResult> HitResults;
    bool bHit = World->LineTraceMultiByObjectType(
        HitResults,
        StartLocation,
        EndLocation,
        ObjectParams,
        CollisionParams
    );

    if (bHit)
    {
        // 여러 히트 결과 중 Landscape만 필터링
        for (const FHitResult& Hit : HitResults)
        {
            if (IsLandscapeHit(Hit))
            {
                // Landscape 히트를 찾았으면 볼 위치 계산
                float BallRadius = 2.1f; // 골프볼 반지름 + 여유
                FVector GroundPosition = Hit.Location + (Hit.Normal * BallRadius);

                UE_LOG(LogTemp, Log, TEXT("Landscape ground position found: %s (Normal: %s)"),
                    *GroundPosition.ToString(), *Hit.Normal.ToString());

                return GroundPosition;
            }
        }
    }

    // Landscape를 찾지 못한 경우 다시 한 번 더 넓은 범위에서 검색
    TArray<FHitResult> AllHitResults;
    FCollisionQueryParams WideParams;
    WideParams.bTraceComplex = true;
    WideParams.bReturnPhysicalMaterial = true;

    if (IgnoreActor)
    {
        WideParams.AddIgnoredActor(IgnoreActor);
    }

    // 더 넓은 범위에서 모든 히트 결과 가져오기
    bool bWideHit = World->LineTraceMultiByChannel(
        AllHitResults,
        StartLocation,
        EndLocation,
        ECC_WorldStatic,
        WideParams
    );

    if (bWideHit)
    {
        // 모든 히트 결과를 순회하면서 Landscape만 찾기
        for (const FHitResult& Hit : AllHitResults)
        {
            if (IsLandscapeHit(Hit))
            {
                FVector LandscapeGroundPosition = Hit.Location + FVector(0, 0, 2.1f);
                UE_LOG(LogTemp, Log, TEXT("Found Landscape using wide search: %s"),
                    *LandscapeGroundPosition.ToString());
                return LandscapeGroundPosition;
            }
        }
    }

    // 마지막 시도: 매우 단순한 트레이스
    FHitResult FinalHitResult;
    FCollisionQueryParams FinalParams;
    FinalParams.bTraceComplex = false; // 단순한 트레이스

    if (IgnoreActor)
    {
        FinalParams.AddIgnoredActor(IgnoreActor);
    }

    bool bFinalHit = World->LineTraceSingleByChannel(
        FinalHitResult,
        StartLocation,
        EndLocation,
        ECC_WorldStatic,
        FinalParams
    );

    if (bFinalHit)
    {
        // 마지막으로 찾은 지면이 Landscape인지 확인
        if (IsLandscapeHit(FinalHitResult))
        {
            FVector FinalGroundPosition = FinalHitResult.Location + FVector(0, 0, 2.1f);
            UE_LOG(LogTemp, Warning, TEXT("Found Landscape using final simple trace: %s"),
                *FinalGroundPosition.ToString());
            return FinalGroundPosition;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Final hit was not Landscape: %s"),
                *FinalHitResult.GetActor()->GetName());
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("Could not find Landscape ground at position: %s"), *TargetPosition.ToString());
    return FVector::ZeroVector;
}

bool UGolfMiniMap::IsLandscapeHit(const FHitResult& HitResult) const
{
    if (!HitResult.GetActor())
    {
        return false;
    }

    AActor* HitActor = HitResult.GetActor();

    // 1. 클래스 이름으로 Landscape 확인
    FString ActorMeshClassName = HitActor->GetClass()->GetName();
    if (ActorMeshClassName.Contains(TEXT("landphysic")) || ActorMeshClassName.Contains(TEXT("Landphysic")))
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("Found landphysic by class name: %s"), *ActorMeshClassName);
        return true;
    }


    // 1. 클래스 이름으로 Landscape 확인
    FString ActorClassName = HitActor->GetClass()->GetName();
    if (ActorClassName.Contains(TEXT("Landscape")))
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("Found Landscape by class name: %s"), *ActorClassName);
        return true;
    }

    // 2. Actor 이름으로 Landscape 확인
    FString ActorName = HitActor->GetName();
    if (ActorName.Contains(TEXT("Landscape")))
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("Found Landscape by actor name: %s"), *ActorName);
        return true;
    }

    // 3. 컴포넌트로 Landscape 확인
    if (HitResult.GetComponent())
    {
        FString ComponentClassName = HitResult.GetComponent()->GetClass()->GetName();
        if (ComponentClassName.Contains(TEXT("Landscape")))
        {
            UE_LOG(LogTemp, VeryVerbose, TEXT("Found Landscape by component: %s"), *ComponentClassName);
            return true;
        }
    }

    // 4. Tag로 Landscape 확인 (사용자 정의 태그가 있을 경우)
    if (HitActor->Tags.Contains(FName(TEXT("Landscape"))) ||
        HitActor->Tags.Contains(FName(TEXT("Terrain"))))
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("Found Landscape by tag"));
        return true;
    }

    return false;
}


bool UGolfMiniMap::IsValidGroundPosition(const FVector& Position) const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    // 해당 위치가 실제로 플레이 가능한 지면인지 확인
    FHitResult HitResult;
    FVector StartPos = Position + FVector(0, 0, 10.0f);
    FVector EndPos = Position - FVector(0, 0, 10.0f);

    FCollisionQueryParams Params;
    Params.bTraceComplex = true;

    bool bHit = World->LineTraceSingleByChannel(
        HitResult,
        StartPos,
        EndPos,
        ECC_WorldStatic,
        Params
    );

    if (bHit)
    {
        // 경사도 검사 (너무 가파르면 볼이 굴러떨어질 수 있음)
        float SlopeAngle = FMath::Acos(FVector::DotProduct(HitResult.Normal, FVector::UpVector)) * 180.0f / PI;

        // 45도 이상 경사면은 피하기
        if (SlopeAngle > 45.0f)
        {
            UE_LOG(LogTemp, Warning, TEXT("Ground too steep (%.1f degrees) at position: %s"),
                SlopeAngle, *Position.ToString());
            return false;
        }

        // 물리 머터리얼 검사 (물이나 특수 지형 회피)
        if (HitResult.PhysMaterial.IsValid())
        {
            // 필요에 따라 특정 물리 머터리얼 타입 필터링 가능
            FString MaterialName = HitResult.PhysMaterial->GetName();
            if (MaterialName.Contains(TEXT("Water")) || MaterialName.Contains(TEXT("Lava")))
            {
                UE_LOG(LogTemp, Warning, TEXT("Invalid ground material (%s) at position: %s"),
                    *MaterialName, *Position.ToString());
                return false;
            }
        }

        return true;
    }

    return false;
}

bool UGolfMiniMap::IsValidBallPlacement(const FVector& WorldPosition) const
{
    if (!bIsInitialized || !GameMode)
    {
        return false;
    }

    // Training Mode가 아니면 볼 이동 불가
    if (!GameMode->IsTrainingMode())
    {
        return false;
    }

    // 볼 이동이 비활성화된 경우
    if (!bAllowBallMovement)
    {
        return false;
    }

    // 1. OB 지역 체크 (가장 먼저)
    if (bEnableOBCheck && IsPointInOBArea(WorldPosition))
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid placement: Position is in OB area"));
        return false;
    }


    // 홀컵과의 거리 체크
    float DistanceToHole = FVector::Dist(WorldPosition, HolecupWorldPosition);
    if (DistanceToHole < MinDistanceFromHole)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Too close to hole: %.1fcm (min: %.1fcm)"),
            DistanceToHole, MinDistanceFromHole);
        return false;
    }

    if (DistanceToHole > MaxDistanceFromHole)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Too far from hole: %.1fcm (max: %.1fcm)"),
            DistanceToHole, MaxDistanceFromHole);
        return false;
    }

    // 미니맵 경계 내에 있는지 체크
    FVector2D MapPosition = WorldToMapPosition(WorldPosition);
    if (!IsPointInMiniMapBounds(MapPosition))
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Position outside minimap bounds"));
        return false;
    }

    return true;
}

// ⭐ MoveBallToPosition 함수 구현
void UGolfMiniMap::MoveBallToPosition(const FVector& WorldPosition)
{
    // OB 지역 체크 먼저 수행
    if (bEnableOBCheck && IsPointInOBArea(WorldPosition))
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot move ball to OB area: %s"), *WorldPosition.ToString());
        ShowOBWarning(WorldPosition);
        return;
    }

    if (!IsValidBallPlacement(WorldPosition))
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Invalid ball placement position"));
        return;
    }

    if (!GameMode || !GameMode->PlayerManager)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ GameMode or PlayerManager is null"));
        return;
    }

    // Training Mode에서 첫 번째 플레이어의 볼 이동
    TArray<AGolfBall*> PlayerBalls = GameMode->PlayerManager->GetPlayerBalls();
    if (PlayerBalls.Num() == 0 || !IsValid(PlayerBalls[0]))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ No valid training ball found"));
        return;
    }

    AGolfBall* TrainingBall = PlayerBalls[0];

    // 지형 높이에 맞춰 위치 조정
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

    // 볼 이동
    TrainingBall->SetActorLocation(FinalPosition);
    TrainingBall->SetBallState(EBallState::Ball_Ready);

    // 물리 상태 리셋
    if (TrainingBall->BallMesh)
    {
        TrainingBall->BallMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
        TrainingBall->BallMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    }

    // 미니맵 업데이트
    UpdateBallPosition(0, FinalPosition);

    // 거리 정보 업데이트
    float Distance = FVector::Dist(FinalPosition, HolecupWorldPosition);
    float Elevation = HolecupWorldPosition.Z - FinalPosition.Z;
    UpdateDistanceAndElevation(Distance, Elevation);

    UE_LOG(LogTemp, Log, TEXT("✅ Training ball moved to: %s"), *FinalPosition.ToString());
}

// ⭐ ResetTrainingBall 함수 구현
void UGolfMiniMap::ResetTrainingBall()
{
    if (!GameMode || !GameMode->IsTrainingMode())
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Not in Training Mode"));
        return;
    }

    if (!GameMode->PlayerManager)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ PlayerManager is null"));
        return;
    }

    TArray<AGolfBall*> PlayerBalls = GameMode->PlayerManager->GetPlayerBalls();
    if (PlayerBalls.Num() == 0 || !IsValid(PlayerBalls[0]))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ No valid training ball found"));
        return;
    }

    AGolfBall* TrainingBall = PlayerBalls[0];

    // 티 위치로 리셋
    if (GameMode->MapInfo.TeePositions.IsValidIndex(GameMode->CurrentHole - 1))
    {
        FVector TeePosition = GameMode->MapInfo.TeePositions[GameMode->CurrentHole - 1] + FVector(0, 0, 5.0f);
        TrainingBall->SetActorLocation(TeePosition);
        TrainingBall->SetBallState(EBallState::Ball_Ready);

        // 물리 상태 리셋
        if (TrainingBall->BallMesh)
        {
            TrainingBall->BallMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
            TrainingBall->BallMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
        }

        // 미니맵 업데이트
        UpdateBallPosition(0, TeePosition);

        UE_LOG(LogTemp, Log, TEXT("✅ Training ball reset to tee"));
    }
}

// ⭐ SetAimDirection 함수 구현 (Stroke Mode용)
void UGolfMiniMap::SetAimDirection(const FVector& WorldPosition)
{
    if (!GameMode || !GameMode->PlayerManager)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ GameMode or PlayerManager is null"));
        return;
    }

    // 현재 플레이어 볼 위치 가져오기
    TArray<AGolfBall*> PlayerBalls = GameMode->PlayerManager->GetPlayerBalls();
    if (!PlayerBalls.IsValidIndex(GameMode->CurrentPlayerIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Invalid CurrentPlayerIndex: %d"), GameMode->CurrentPlayerIndex);
        return;
    }

    AGolfBall* CurrentBall = PlayerBalls[GameMode->CurrentPlayerIndex];
    if (!IsValid(CurrentBall))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Current ball is invalid"));
        return;
    }

    FVector BallPosition = CurrentBall->GetActorLocation();
    FVector TargetDirection = (WorldPosition - BallPosition).GetSafeNormal();

    // Z축 성분 제거 (수평 방향만)
    TargetDirection.Z = 0.0f;
    TargetDirection.Normalize();

    // PlayerController를 통해 에임 방향 설정
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
    {
        if (AGolfPlayerController* GolfPC = Cast<AGolfPlayerController>(PC))
        {
            GolfPC->AimDirection = TargetDirection;

            // 카메라 회전 업데이트
            UpdateCameraAim(TargetDirection);

            // 미니맵 에임 업데이트
            UpdateAimDirection(GameMode->CurrentPlayerIndex, TargetDirection);

            UE_LOG(LogTemp, Log, TEXT("✅ Aim direction set to: %s"), *TargetDirection.ToString());
        }
    }
}

// ⭐ UpdateCameraAim 함수 구현
void UGolfMiniMap::UpdateCameraAim(const FVector& TargetDirection)
{
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
    {
        if (AGolfPlayerController* GolfPC = Cast<AGolfPlayerController>(PC))
        {
            if (GolfPC->CameraManager)
            {
                FRotator NewRotation = TargetDirection.Rotation();
                GolfPC->CameraManager->SetActorRotation(NewRotation);

                // AimActor 위치도 업데이트
                GolfPC->UpdateAimActorPosition();

                UE_LOG(LogTemp, VeryVerbose, TEXT("🎥 Camera rotation updated: %s"), *NewRotation.ToString());
            }
        }
    }
}


bool UGolfMiniMap::IsPointInOBArea(const FVector& WorldPoint) const
{
    // OB 라인 포인트가 3개 미만이면 유효한 다각형이 아니므로 false 반환
    if (CurrentOBPoints.Num() < 3)
    {
        return false;
    }

    int32 WindingNumber = 0;
    FVector TestPoint(WorldPoint.X, WorldPoint.Y, 0.0f); // Z축 무시

    // 각 OB 라인 세그먼트를 순회하며 와인딩 넘버 계산
    for (int32 i = 0; i < CurrentOBPoints.Num(); i++)
    {
        FVector P1 = FVector(CurrentOBPoints[i].X, CurrentOBPoints[i].Y, 0.0f);
        FVector P2 = FVector(CurrentOBPoints[(i + 1) % CurrentOBPoints.Num()].X, CurrentOBPoints[(i + 1) % CurrentOBPoints.Num()].Y, 0.0f);

        // 점이 P1과 P2 사이의 Y 범위에 있는지 확인
        if (P1.Y <= TestPoint.Y)
        {
            if (P2.Y > TestPoint.Y)
            {
                // 교차점 계산
                float CrossProduct = (P2.X - P1.X) * (TestPoint.Y - P1.Y) - (TestPoint.X - P1.X) * (P2.Y - P1.Y);
                if (CrossProduct > 0)
                {
                    WindingNumber++;
                }
            }
        }
        else
        {
            if (P2.Y <= TestPoint.Y)
            {
                // 교차점 계산
                float CrossProduct = (P2.X - P1.X) * (TestPoint.Y - P1.Y) - (TestPoint.X - P1.X) * (P2.Y - P1.Y);
                if (CrossProduct < 0)
                {
                    WindingNumber--;
                }
            }
        }
    }

    // 수정: 와인딩 넘버가 0이면 점은 다각형 외부에 있음 (OB 영역)
    // 골프에서 OB 라인 내부 = 인바운드, OB 라인 외부 = 아웃오브바운드
    return WindingNumber == 0;
}

// 추가: 인바운드 영역 체크 함수 (명확성을 위해)
bool UGolfMiniMap::IsPointInBounds(const FVector& WorldPoint) const
{
    // OB 라인이 없으면 모든 곳이 인바운드
    if (CurrentOBPoints.Num() < 3)
    {
        return true;
    }

    // IsPointInOBArea의 반대 결과 반환
    return !IsPointInOBArea(WorldPoint);
}


// 클릭 위치가 유효한지 체크 (OB, 거리, 경계 모두 포함)
bool UGolfMiniMap::IsClickPositionValid(const FVector& WorldPosition) const
{
    if (!bIsInitialized)
    {
        UE_LOG(LogTemp, Warning, TEXT("MiniMap not initialized for position validation"));
        return false;
    }

    // 1. OB 지역 체크
    if (bEnableOBCheck && IsPointInOBArea(WorldPosition))
    {
        UE_LOG(LogTemp, Warning, TEXT("Click position is in OB area: %s"), *WorldPosition.ToString());
        return false;
    }

    // 2. 미니맵 경계 내에 있는지 체크
    FVector2D MapPosition = WorldToMapPosition(WorldPosition);
    if (!IsPointInMiniMapBounds(MapPosition))
    {
        UE_LOG(LogTemp, Warning, TEXT("Position outside minimap bounds"));
        return false;
    }

    return true;
}

// OB 경고 메시지 표시
void UGolfMiniMap::ShowOBWarning(const FVector& ClickedPosition)
{
    if (!bShowOBWarnings)
        return;
#if WITH_EDITOR

    if (GEngine)
    {
        FString WarningMessage = TEXT("Cannot move to OB area!");
        GEngine->AddOnScreenDebugMessage(-1, OBWarningDuration, FColor::Red, WarningMessage);
    }

    UE_LOG(LogTemp, Warning, TEXT("OB Area clicked at: %s"), *ClickedPosition.ToString());
#endif
}


// ⭐ 새로 추가: 홀 방향 회전 각도 계산
void UGolfMiniMap::CalculateHoleRotationAngle()
{
    // 티에서 홀컵으로의 방향 벡터 (2D만 사용)
    FVector2D HoleDirection2D = FVector2D(
        HolecupWorldPosition.X - TeeWorldPosition.X,
        HolecupWorldPosition.Y - TeeWorldPosition.Y
    );

    // 벡터 정규화
    HoleDirection2D.Normalize();

    // ⭐ 수정: Y축 양의 방향(0, 1)을 기준으로 현재 홀 방향까지의 회전각 계산
    // atan2(y, x)로 일반적인 각도를 구한 후, Y축 기준으로 변환
    float StandardAngle = FMath::Atan2(HoleDirection2D.Y, HoleDirection2D.X); // X축 기준 각도
    HoleRotationAngle = StandardAngle - (PI * 0.5f); // Y축 기준으로 변환 (90도 - 표준각도)

    // ⭐ 각도 정규화 (-PI ~ PI 범위)
    while (HoleRotationAngle > PI) HoleRotationAngle -= 2.0f * PI;
    while (HoleRotationAngle < -PI) HoleRotationAngle += 2.0f * PI;

    // 도 단위로 변환 (디버깅용)
    float HoleRotationDegrees = FMath::RadiansToDegrees(HoleRotationAngle);

}

FVector2D UGolfMiniMap::RotatePoint(const FVector2D& Point, float AngleRadians) const
{
    float CosAngle = FMath::Cos(AngleRadians);
    float SinAngle = FMath::Sin(AngleRadians);

    float RotatedX = Point.X * CosAngle - Point.Y * SinAngle;
    float RotatedY = Point.X * SinAngle + Point.Y * CosAngle;

    return FVector2D(RotatedX, RotatedY);

}

// ==================== 새로 추가할 함수들 구현 ====================

void UGolfMiniMap::UpdateBallToHoleLinePosition(int32 PlayerIndex)
{
    if (!PlayerBallToHoleLineImages.Contains(PlayerIndex) || !bIsInitialized)
        return;

    UImage* BallToHoleLineImage = PlayerBallToHoleLineImages[PlayerIndex];
    if (!IsValid(BallToHoleLineImage))
        return;

    // StrokeMode가 아니면 라인 숨김
    if (!GameMode || GameMode->GetCurrentGameMode() != EGolfGameMode::StrokeMode)
    {
        BallToHoleLineImage->SetVisibility(ESlateVisibility::Hidden);
        return;
    }

    // 현재 플레이어 볼 위치 가져오기
    if (!PlayerBallWorldPositions.Contains(PlayerIndex))
    {
        BallToHoleLineImage->SetVisibility(ESlateVisibility::Hidden);
        return;
    }

    FVector BallPosition = PlayerBallWorldPositions[PlayerIndex];

    // 볼 위치와 홀컵 위치를 미니맵 좌표로 변환
    FVector2D BallMapPos = WorldToMapPosition(BallPosition);
    FVector2D HoleMapPos = WorldToMapPosition(HolecupWorldPosition);

    // 라인 중심점과 길이 계산
    FVector2D LineCenter = (BallMapPos + HoleMapPos) * 0.5f;
    float LineLength = FVector2D::Distance(BallMapPos, HoleMapPos);

    // 라인이 너무 짧으면 숨김 (볼이 홀컵에 매우 가까울 때)
    if (LineLength < 5.0f)
    {
        BallToHoleLineImage->SetVisibility(ESlateVisibility::Hidden);
        return;
    }

    // 라인 방향 계산
    FVector2D LineDirection2D = (HoleMapPos - BallMapPos).GetSafeNormal();
    float Angle = FMath::Atan2(LineDirection2D.Y, LineDirection2D.X) * 180.0f / PI;

    // UI 업데이트
    UCanvasPanelSlot* LineSlot = Cast<UCanvasPanelSlot>(BallToHoleLineImage->Slot);
    if (LineSlot)
    {
        LineSlot->SetSize(FVector2D(LineLength, BallToHoleLineThickness));
        LineSlot->SetPosition(LineCenter - FVector2D(LineLength * 0.5f, BallToHoleLineThickness * 0.5f));

        FWidgetTransform Transform;
        Transform.Angle = Angle;
        BallToHoleLineImage->SetRenderTransform(Transform);

        // 현재 플레이어인지에 따라 색상 조정
        FLinearColor LineColor = BallToHoleLineColor;
        if (GameMode && PlayerIndex == GameMode->CurrentPlayerIndex)
        {
            LineColor.A = 1.0f; // 현재 플레이어는 완전 불투명
        }
        else
        {
            LineColor.A = 0.5f; // 다른 플레이어는 더 투명하게
        }

        BallToHoleLineImage->SetColorAndOpacity(LineColor);
        LineSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
    }

    //BallToHoleLineImage->SetVisibility(ESlateVisibility::Visible);

    UE_LOG(LogTemp, VeryVerbose, TEXT("🟢 Ball-to-hole line updated for player %d: Distance %.1fcm"),
        PlayerIndex, FVector::Dist(BallPosition, HolecupWorldPosition));
}

void UGolfMiniMap::CreateBallToHoleLineForPlayer(int32 PlayerIndex)
{
    if (!MiniMapCanvas)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Cannot create ball-to-hole line: MiniMapCanvas is null"));
        return;
    }

    if (PlayerBallToHoleLineImages.Contains(PlayerIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("Ball-to-hole line already exists for player %d"), PlayerIndex);
        return;
    }

    // 볼-홀컵 라인 이미지 생성
    UImage* NewBallToHoleLineImage = NewObject<UImage>(this,
        FName(*FString::Printf(TEXT("BallToHoleLineImage_%d"), PlayerIndex)));

    if (!NewBallToHoleLineImage)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to create ball-to-hole line image for player %d"), PlayerIndex);
        return;
    }

    PlayerBallToHoleLineImages.Add(PlayerIndex, NewBallToHoleLineImage);

    // 캔버스에 추가
    UCanvasPanelSlot* LineSlot = MiniMapCanvas->AddChildToCanvas(NewBallToHoleLineImage);
    if (LineSlot)
    {
        LineSlot->SetAutoSize(true);
        LineSlot->SetZOrder(0); // 배경과 다른 요소들 사이에 배치 (에임라인보다는 뒤에)
    }

    // 초기 라인 설정
    UpdateBallToHoleLinePosition(PlayerIndex);

    UE_LOG(LogTemp, Log, TEXT("✅ Created ball-to-hole line for player %d"), PlayerIndex);
}

void UGolfMiniMap::RemoveBallToHoleLineForPlayer(int32 PlayerIndex)
{
    if (PlayerBallToHoleLineImages.Contains(PlayerIndex))
    {
        if (UImage* LineImageToRemove = PlayerBallToHoleLineImages[PlayerIndex])
        {
            LineImageToRemove->RemoveFromParent();
            LineImageToRemove->ConditionalBeginDestroy();
        }
        PlayerBallToHoleLineImages.Remove(PlayerIndex);
        UE_LOG(LogTemp, Log, TEXT("🗑️ Removed ball-to-hole line for player %d"), PlayerIndex);
    }
}

void UGolfMiniMap::ClearAllBallToHoleLines()
{
    for (auto& Elem : PlayerBallToHoleLineImages)
    {
        if (IsValid(Elem.Value))
        {
            Elem.Value->RemoveFromParent();
            Elem.Value->ConditionalBeginDestroy();
        }
    }
    PlayerBallToHoleLineImages.Empty();
    UE_LOG(LogTemp, Log, TEXT("🧹 Cleared all ball-to-hole lines"));
}

// ==================== 블루프린트 호출 가능한 함수들 ====================

void UGolfMiniMap::SetBallToHoleLineVisible(bool bVisible)
{
    bShowBallToHoleLine = bVisible;

    if (!bVisible)
    {
        // 모든 라인 숨김
        for (auto& Elem : PlayerBallToHoleLineImages)
        {
            if (IsValid(Elem.Value))
            {
                Elem.Value->SetVisibility(ESlateVisibility::Hidden);
            }
        }
    }
    // bVisible이 true면 다음 NativeTick에서 자동으로 표시됨

    UE_LOG(LogTemp, Log, TEXT("🟢 Ball-to-hole lines visibility: %s"), bVisible ? TEXT("ON") : TEXT("OFF"));
}

void UGolfMiniMap::SetBallToHoleLineColor(FLinearColor NewColor)
{
    BallToHoleLineColor = NewColor;

    // 기존 라인들 색상 업데이트
    for (auto& Elem : PlayerBallToHoleLineImages)
    {
        if (IsValid(Elem.Value))
        {
            Elem.Value->SetColorAndOpacity(NewColor);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("🎨 Ball-to-hole line color updated: %s"), *NewColor.ToString());
}

void UGolfMiniMap::SetShowOnlyCurrentPlayerLine(bool bOnlyCurrentPlayer)
{
    bShowOnlyCurrentPlayerLine = bOnlyCurrentPlayer;

    UE_LOG(LogTemp, Log, TEXT("👤 Show only current player's ball-to-hole line: %s"),
        bOnlyCurrentPlayer ? TEXT("ON") : TEXT("OFF"));
}


void UGolfMiniMap::EnableBallToHoleLine(bool bEnable)
{
    bShowBallToHoleLine = bEnable;

    if (bEnable)
    {
        // 모든 플레이어의 볼-홀컵 라인 다시 생성/업데이트
        if (PlayerManager)
        {
            for (AGolfBall* Ball : PlayerManager->GetPlayerBalls())
            {
                if (IsValid(Ball))
                {
                    if (!PlayerBallToHoleLineImages.Contains(Ball->OwningPlayerIndex))
                    {
                        CreateBallToHoleLineForPlayer(Ball->OwningPlayerIndex);
                    }
                    UpdateBallToHoleLinePosition(Ball->OwningPlayerIndex);
                }
            }
        }
    }
    else
    {
        // 모든 볼-홀컵 라인 숨김
        for (auto& Elem : PlayerBallToHoleLineImages)
        {
            if (IsValid(Elem.Value))
            {
                Elem.Value->SetVisibility(ESlateVisibility::Hidden);
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("🟢 Ball-to-hole lines %s"), bEnable ? TEXT("ENABLED") : TEXT("DISABLED"));
}

void UGolfMiniMap::SetBallToHoleLineThickness(float Thickness)
{
    BallToHoleLineThickness = FMath::Clamp(Thickness, 1.0f, 10.0f);

    // 기존 라인들의 두께 업데이트
    for (auto& Elem : PlayerBallToHoleLineImages)
    {
        if (IsValid(Elem.Value))
        {
            UpdateBallToHoleLinePosition(Elem.Key); // 두께가 포함된 업데이트
        }
    }

    UE_LOG(LogTemp, Log, TEXT("🟢 Ball-to-hole line thickness set to %.1f"), BallToHoleLineThickness);
}

void UGolfMiniMap::RefreshAllBallToHoleLines()
{
    if (!bShowBallToHoleLine || !PlayerManager)
        return;

    UE_LOG(LogTemp, Log, TEXT("🔄 Refreshing all ball-to-hole lines..."));

    // 모든 플레이어의 볼-홀컵 라인 업데이트
    for (AGolfBall* Ball : PlayerManager->GetPlayerBalls())
    {
        if (IsValid(Ball))
        {
            // 라인이 없으면 생성
            if (!PlayerBallToHoleLineImages.Contains(Ball->OwningPlayerIndex))
            {
                CreateBallToHoleLineForPlayer(Ball->OwningPlayerIndex);
            }

            // 위치 업데이트
            UpdateBallToHoleLinePosition(Ball->OwningPlayerIndex);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("✅ All ball-to-hole lines refreshed"));
}

// 모든 플레이어 볼 텍스처 로드
void UGolfMiniMap::LoadAllPlayerBallTextures()
{
    for (int32 i = 0; i < MaxPlayerCount; i++)
    {
        if (!PlayerBallTextures.IsValidIndex(i) || !PlayerBallTextures[i])
        {
            // 런타임에 텍스처 로드 시도
            FString TexturePath = FString::Printf(TEXT("/Game/GolfGame/Image/mini_%02d"), i + 1);
            UTexture2D* LoadedTexture = LoadObject<UTexture2D>(nullptr, *TexturePath);

            if (!LoadedTexture)
            {
                // 대체 경로들 시도
                TArray<FString> AlternatePaths = {
                    FString::Printf(TEXT("/Game/GolfGame/Image/mini_%02d.mini_%02d"), i + 1, i + 1),
                    FString::Printf(TEXT("/Game/GolfGame/Images/mini_%02d"), i + 1),
                    FString::Printf(TEXT("/Game/GolfGame/Textures/mini_%02d"), i + 1),
                };

                for (const FString& AltPath : AlternatePaths)
                {
                    LoadedTexture = LoadObject<UTexture2D>(nullptr, *AltPath);
                    if (LoadedTexture)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("Loaded player %d texture from alternate path: %s"), i, *AltPath);
                        break;
                    }
                }
            }

            if (PlayerBallTextures.IsValidIndex(i))
            {
                PlayerBallTextures[i] = LoadedTexture;
            }

            if (LoadedTexture)
            {
                UE_LOG(LogTemp, Log, TEXT("Runtime loaded texture for player %d"), i);
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("Failed to load texture for player %d"), i);
            }
        }
    }
}

// 특정 플레이어의 볼 텍스처 가져오기
UTexture2D* UGolfMiniMap::GetPlayerBallTexture(int32 PlayerIndex) const
{
    if (PlayerBallTextures.IsValidIndex(PlayerIndex) && PlayerBallTextures[PlayerIndex])
    {
        return PlayerBallTextures[PlayerIndex];
    }

    // 인덱스가 범위를 벗어나면 첫 번째 텍스처 사용
    if (PlayerBallTextures.IsValidIndex(0) && PlayerBallTextures[0])
    {
        UE_LOG(LogTemp, Warning, TEXT("Player %d texture not found, using default (player 0)"), PlayerIndex);
        return PlayerBallTextures[0];
    }

    return nullptr;
}

// 로드된 텍스처 개수 확인
int32 UGolfMiniMap::GetLoadedTextureCount() const
{
    int32 Count = 0;
    for (UTexture2D* Texture : PlayerBallTextures)
    {
        if (Texture)
        {
            Count++;
        }
    }
    return Count;
}

// 모든 텍스처가 로드되었는지 확인
bool UGolfMiniMap::AreAllTexturesLoaded() const
{
    return GetLoadedTextureCount() == MaxPlayerCount;
}

void UGolfMiniMap::SetBallTexture(UTexture2D* NewTexture)
{
    // 모든 플레이어의 첫 번째 텍스처를 변경하는 경우
    if (PlayerBallTextures.IsValidIndex(0))
    {
        PlayerBallTextures[0] = NewTexture;
    }

    // 기존 볼 이미지들에 새 텍스처 적용
    for (auto& Elem : PlayerBallImages)
    {
        int32 PlayerIndex = Elem.Key;
        UImage* BallImage = Elem.Value;

        if (IsValid(BallImage))
        {
            UTexture2D* PlayerTexture = GetPlayerBallTexture(PlayerIndex);
            if (PlayerTexture)
            {
                FLinearColor PlayerColor = PlayerBallColors.Contains(PlayerIndex) ?
                    PlayerBallColors[PlayerIndex] : FLinearColor::White;

                FSlateBrush BallBrush;
                BallBrush.SetResourceObject(PlayerTexture);
                BallBrush.DrawAs = ESlateBrushDrawType::Image;
                BallBrush.Tiling = ESlateBrushTileType::NoTile;
                BallBrush.ImageSize = FVector2D(16.0f, 16.0f);
                BallBrush.TintColor = FSlateColor(PlayerColor);

                // 브러시 저장 및 적용
                PlayerBallBrushes.Add(PlayerIndex, BallBrush);
                BallImage->SetBrush(BallBrush);
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Ball textures updated for all players"));
}


// AimActor 아이콘 생성
void UGolfMiniMap::CreateAimActorForPlayer(int32 PlayerIndex)
{
    if (!MiniMapCanvas)
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot create AimActor: MiniMapCanvas is null"));
        return;
    }

    if (PlayerAimActorImages.Contains(PlayerIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("AimActor already exists for player %d"), PlayerIndex);
        return;
    }

    // AimActor 아이콘 생성
    UImage* NewAimActorImage = NewObject<UImage>(this,
        FName(*FString::Printf(TEXT("AimActorImage_%d"), PlayerIndex)));

    if (!NewAimActorImage)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create AimActor image for player %d"), PlayerIndex);
        return;
    }

    // AimActor 텍스처 적용
    if (AimActorTexture)
    {
        FSlateBrush AimBrush;
        AimBrush.SetResourceObject(AimActorTexture);
        AimBrush.DrawAs = ESlateBrushDrawType::Image;
        AimBrush.Tiling = ESlateBrushTileType::NoTile;
        AimBrush.ImageSize = FVector2D(AimActorIconSize, AimActorIconSize);
        AimBrush.TintColor = FSlateColor(AimActorColor);

        NewAimActorImage->SetBrush(AimBrush);
    }
    else
    {
        // 텍스처가 없으면 기본 색상 사용
        NewAimActorImage->SetColorAndOpacity(AimActorColor);
    }

    PlayerAimActorImages.Add(PlayerIndex, NewAimActorImage);

    UCanvasPanelSlot* AimActorSlot = MiniMapCanvas->AddChildToCanvas(NewAimActorImage);
    if (AimActorSlot)
    {
        AimActorSlot->SetAutoSize(true);
        AimActorSlot->SetZOrder(3); // 볼보다 위에 표시
        AimActorSlot->SetSize(FVector2D(AimActorIconSize, AimActorIconSize));
    }

    // 볼 → AimActor 라인 생성
    CreateBallToAimLineForPlayer(PlayerIndex);

    // AimActor → 홀컵 라인 생성
    CreateAimToHoleLineForPlayer(PlayerIndex);

    // ⭐ 추가: 초기 위치 업데이트 호출
    if (GameMode && PlayerIndex == GameMode->CurrentPlayerIndex)
    {
        // 현재 플레이어라면 즉시 라인 위치 업데이트
        UpdateBallToAimLinePosition(PlayerIndex);
        UpdateAimToHoleLinePosition(PlayerIndex);
    }

    // 초기에는 숨김 (AimActor 위치가 설정되면 표시)
    NewAimActorImage->SetVisibility(ESlateVisibility::Hidden);

    UE_LOG(LogTemp, Log, TEXT("Created AimActor for player %d"), PlayerIndex);
}

// 볼 → AimActor 라인 생성
void UGolfMiniMap::CreateBallToAimLineForPlayer(int32 PlayerIndex)
{
    if (!MiniMapCanvas || PlayerBallToAimLineImages.Contains(PlayerIndex))
        return;

    UImage* NewBallToAimLine = NewObject<UImage>(this,
        FName(*FString::Printf(TEXT("BallToAimLine_%d"), PlayerIndex)));

    if (NewBallToAimLine)
    {
        // ⭐ 적절한 브러시 설정
        FSlateBrush LineBrush;
        LineBrush.DrawAs = ESlateBrushDrawType::Box;
        LineBrush.TintColor = FSlateColor(BallToAimLineColor);
        LineBrush.Margin = FMargin(0.0f);

        NewBallToAimLine->SetBrush(LineBrush);

        PlayerBallToAimLineImages.Add(PlayerIndex, NewBallToAimLine);

        UCanvasPanelSlot* LineSlot = MiniMapCanvas->AddChildToCanvas(NewBallToAimLine);
        if (LineSlot)
        {
            LineSlot->SetAutoSize(false); // ⭐ AutoSize 비활성화
            LineSlot->SetZOrder(2);
        }

        NewBallToAimLine->SetColorAndOpacity(BallToAimLineColor);
        NewBallToAimLine->SetVisibility(ESlateVisibility::Hidden);

        UE_LOG(LogTemp, Log, TEXT("Created Ball-to-Aim line for player %d"), PlayerIndex);
    }
}

// AimActor → 홀컵 라인 생성
void UGolfMiniMap::CreateAimToHoleLineForPlayer(int32 PlayerIndex)
{
    if (!MiniMapCanvas || PlayerAimToHoleLineImages.Contains(PlayerIndex))
        return;

    UImage* NewAimToHoleLine = NewObject<UImage>(this,
        FName(*FString::Printf(TEXT("AimToHoleLine_%d"), PlayerIndex)));

    if (NewAimToHoleLine)
    {
        // ⭐ 적절한 브러시 설정 추가
        FSlateBrush LineBrush;
        LineBrush.DrawAs = ESlateBrushDrawType::Box;
        LineBrush.TintColor = FSlateColor(AimToHoleLineColor);
        LineBrush.Margin = FMargin(0.0f);

        NewAimToHoleLine->SetBrush(LineBrush);

        PlayerAimToHoleLineImages.Add(PlayerIndex, NewAimToHoleLine);

        UCanvasPanelSlot* LineSlot = MiniMapCanvas->AddChildToCanvas(NewAimToHoleLine);
        if (LineSlot)
        {
            LineSlot->SetAutoSize(false); // ⭐ AutoSize 비활성화
            LineSlot->SetZOrder(2);
        }

        NewAimToHoleLine->SetColorAndOpacity(AimToHoleLineColor);
        NewAimToHoleLine->SetVisibility(ESlateVisibility::Hidden);

        UE_LOG(LogTemp, Log, TEXT("Created Aim-to-Hole line for player %d"), PlayerIndex);
    }
}


// 1. UpdateAimActorPosition 함수 수정
void UGolfMiniMap::UpdateAimActorPosition(int32 PlayerIndex, const FVector& AimActorPosition)
{
    // 현재 플레이어가 아니면 무시
    if (!GameMode || PlayerIndex != GameMode->CurrentPlayerIndex)
    {
        UE_LOG(LogTemp, Log, TEXT("Ignoring AimActor update for non-current player %d (current: %d)"),
            PlayerIndex, GameMode ? GameMode->CurrentPlayerIndex : -1);
        return;
    }



    // ⭐ 홀컵 거리 제한 적용
    FVector LimitedAimPosition = ApplyAimPositionLimit(AimActorPosition);

    // 제한된 위치로 저장
    PlayerAimActorPositions.Add(PlayerIndex, LimitedAimPosition);

    if (!PlayerAimActorImages.Contains(PlayerIndex) || !bIsInitialized)
        return;

    UImage* AimActorImage = PlayerAimActorImages[PlayerIndex];
    if (!IsValid(AimActorImage))
        return;

    // AimActor 아이콘 위치 업데이트 (제한된 위치 사용)
    FVector2D MapPosition = WorldToMapPosition(LimitedAimPosition);



    if (UCanvasPanelSlot* ImageSlot = Cast<UCanvasPanelSlot>(AimActorImage->Slot))
    {
        FVector2D IconSize(AimActorIconSize, AimActorIconSize);
        ImageSlot->SetSize(IconSize);
        ImageSlot->SetPosition(MapPosition - (IconSize * 0.5f));
        ImageSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
    }

    // 현재 플레이어만 표시
    bool bShouldShow = bShowAimActor && (PlayerIndex == GameMode->CurrentPlayerIndex);
    AimActorImage->SetVisibility(bShouldShow ? ESlateVisibility::Visible : ESlateVisibility::Hidden);

    // 라인들 업데이트 (현재 플레이어만)
    UpdateBallToAimLinePosition(PlayerIndex);
    UpdateAimToHoleLinePosition(PlayerIndex);

    TArray<AGolfBall*> PlayerBalls = GameMode->PlayerManager->GetPlayerBalls();
    AGolfBall* CurrentBall = PlayerBalls[GameMode->CurrentPlayerIndex];
    FVector BallLocation = CurrentBall->GetActorLocation();


    float Elevation = LimitedAimPosition.Z - BallLocation.Z;
    UpdateDistanceAndElevation(CurrentDistance, Elevation);

    MoveToMouseTip2(MapPosition);
    UpdateTip2();

    UE_LOG(LogTemp, Log, TEXT("Updated AimActor position for current player %d: %s (Limited from: %s)"),
        PlayerIndex, *LimitedAimPosition.ToString(), *AimActorPosition.ToString());
}


// 2. 새로운 Aim 위치 제한 함수 추가
FVector UGolfMiniMap::ApplyAimPositionLimit(const FVector& OriginalAimPosition)
{
    if (!GameMode || !GameMode->PlayerManager)
        return OriginalAimPosition;

    // 현재 볼 위치 가져오기
    TArray<AGolfBall*> PlayerBalls = GameMode->PlayerManager->GetPlayerBalls();
    if (!PlayerBalls.IsValidIndex(GameMode->CurrentPlayerIndex))
        return OriginalAimPosition;

    AGolfBall* CurrentBall = PlayerBalls[GameMode->CurrentPlayerIndex];
    if (!CurrentBall)
        return OriginalAimPosition;

    FVector BallLocation = CurrentBall->GetActorLocation();

    // 홀컵 위치 가져오기
    if (!GameMode->MapInfo.HolecupPositions.IsValidIndex(GameMode->CurrentHole - 1))
        return OriginalAimPosition;

    FVector HolecupLocation = GameMode->MapInfo.HolecupPositions[GameMode->CurrentHole - 1];

    // 1. 에임과 홀컵 사이의 거리 계산 (평면 거리 기준인 경우 Dist2D 사용 권장)
    float AimToHolecupDistance = FVector::Dist(OriginalAimPosition, HolecupLocation);

    FVector FinalAimPosition = OriginalAimPosition;

    // ✅ 추가된 로직: 에임이 홀컵 반경 30cm 이내인 경우 높이값 동기화
    if (AimToHolecupDistance <= 30.0f) // 30cm = 30.0 units
    {
        FinalAimPosition.Z = HolecupLocation.Z;

        // (선택사항) 로그 출력
        UE_LOG(LogTemp, Log, TEXT("⛳ Aim close to Holecup (%.1fcm): Snapping Z height"), AimToHolecupDistance);
    }

    // 2. 최대 거리 제한 계산
    float BallToAimDistance = FVector::Dist(BallLocation, FinalAimPosition);
    float BallToHolecupDistance = FVector::Dist(BallLocation, HolecupLocation);

    // Aim이 홀컵보다 멀리 있으면 제한
    if (BallToAimDistance > BallToHolecupDistance)
    {
        // 볼에서 현재 처리된 에임 방향으로의 단위 벡터
        FVector DirectionToAim = (FinalAimPosition - BallLocation).GetSafeNormal();

        // 홀컵 거리의 98%로 제한
        float MaxAllowedDistance = BallToHolecupDistance * 0.98f;
        FVector LimitedPosition = BallLocation + (DirectionToAim * MaxAllowedDistance);

        // 높이값 제한 후에도 홀컵 근처라면 홀컵 높이 유지 (보정)
        if (AimToHolecupDistance <= 30.0f)
        {
            LimitedPosition.Z = HolecupLocation.Z;
        }

        UE_LOG(LogTemp, Log, TEXT("🚫 Aim position limited: %.1fm → %.1fm (Holecup: %.1fm)"),
            BallToAimDistance / 100.0f, MaxAllowedDistance / 100.0f, BallToHolecupDistance / 100.0f);

        return LimitedPosition;
    }

    return FinalAimPosition;
}

// 볼 → AimActor 라인 위치 업데이트
void UGolfMiniMap::UpdateBallToAimLinePosition(int32 PlayerIndex)
{
    if (!PlayerBallToAimLineImages.Contains(PlayerIndex) || !bIsInitialized)
    {
        UE_LOG(LogTemp, Warning, TEXT("UpdateBallToAimLinePosition: No line image for player %d"), PlayerIndex);
        return;
    }

    UImage* LineImage = PlayerBallToAimLineImages[PlayerIndex];
    if (!IsValid(LineImage))
    {
        UE_LOG(LogTemp, Warning, TEXT("UpdateBallToAimLinePosition: Invalid line image for player %d"), PlayerIndex);
        return;
    }

    // StrokeMode가 아니면 라인 숨김
    if (!GameMode || GameMode->GetCurrentGameMode() != EGolfGameMode::StrokeMode)
    {
        LineImage->SetVisibility(ESlateVisibility::Hidden);
        return;
    }

    // 현재 플레이어가 아니면 숨김 (Single AimActor 시스템)
    if (!GameMode || PlayerIndex != GameMode->CurrentPlayerIndex)
    {
        LineImage->SetVisibility(ESlateVisibility::Hidden);
        UE_LOG(LogTemp, Warning, TEXT("UpdateBallToAimLinePosition: Invalid line image for player %d --- GM CurrentPLayerindex -[%d]"), PlayerIndex, GameMode->CurrentPlayerIndex);
        return;
    }

    // 볼 위치와 AimActor 위치 가져오기
    if (!PlayerBallWorldPositions.Contains(PlayerIndex))
    {
        LineImage->SetVisibility(ESlateVisibility::Hidden);
        UE_LOG(LogTemp, Warning, TEXT("No ball position for Ball-to-Aim line, player %d"), PlayerIndex);
        return;
    }

    FVector BallPosition = PlayerBallWorldPositions[PlayerIndex];
    FVector AimPosition;

    // AimActor 위치 확인
    if (PlayerAimActorPositions.Contains(PlayerIndex))
    {
        AimPosition = PlayerAimActorPositions[PlayerIndex];
    }
    else if (GameMode && !GameMode->AimLocation.IsZero())
    {
        // GameMode의 AimLocation 사용 (fallback)
        AimPosition = GameMode->AimLocation;
        PlayerAimActorPositions.Add(PlayerIndex, AimPosition);
        UE_LOG(LogTemp, Log, TEXT("Using GameMode AimLocation for Ball-to-Aim line: %s"), *AimPosition.ToString());
    }
    else
    {
        LineImage->SetVisibility(ESlateVisibility::Hidden);
        UE_LOG(LogTemp, Warning, TEXT("No AimActor position for Ball-to-Aim line, player %d"), PlayerIndex);
        return;
    }

    // 미니맵 좌표로 변환
    FVector2D BallMapPos = WorldToMapPosition(BallPosition);
    FVector2D AimMapPos = WorldToMapPosition(AimPosition);

    // 라인 업데이트
    UpdateLinePosition(LineImage, BallMapPos, AimMapPos, BallToAimLineColor, PlayerIndex);

    UE_LOG(LogTemp, VeryVerbose, TEXT("Ball-to-Aim line updated for player %d"), PlayerIndex);
}

// AimActor → 홀컵 라인 위치 업데이트
void UGolfMiniMap::UpdateAimToHoleLinePosition(int32 PlayerIndex)
{
    if (!PlayerAimToHoleLineImages.Contains(PlayerIndex) || !bIsInitialized)
        return;

    UImage* LineImage = PlayerAimToHoleLineImages[PlayerIndex];
    if (!IsValid(LineImage))
        return;


    // StrokeMode가 아니면 라인 숨김
    if (!GameMode || GameMode->GetCurrentGameMode() != EGolfGameMode::StrokeMode)
    {
        LineImage->SetVisibility(ESlateVisibility::Hidden);
        return;
    }

    // ⭐ 수정: 현재 플레이어가 아니면 숨김
    if (!GameMode || PlayerIndex != GameMode->CurrentPlayerIndex)
    {
        LineImage->SetVisibility(ESlateVisibility::Hidden);
        return;
    }

    // AimActor 위치 가져오기
    if (!PlayerAimActorPositions.Contains(PlayerIndex))
    {
        LineImage->SetVisibility(ESlateVisibility::Hidden);
        return;
    }

    FVector AimPosition = PlayerAimActorPositions[PlayerIndex];

    // 미니맵 좌표로 변환
    FVector2D AimMapPos = WorldToMapPosition(AimPosition);
    FVector2D HoleMapPos = WorldToMapPosition(HolecupWorldPosition);

    // 라인 업데이트
    UpdateLinePosition(LineImage, AimMapPos, HoleMapPos, AimToHoleLineColor, PlayerIndex);
}

// 라인 위치 업데이트 헬퍼 함수
void UGolfMiniMap::UpdateLinePosition(UImage* LineImage, const FVector2D& StartPos, const FVector2D& EndPos,
    const FLinearColor& LineColor, int32 PlayerIndex)
{
    if (!IsValid(LineImage))
        return;

    // StrokeMode가 아니면 라인 숨김
    if (!GameMode || GameMode->GetCurrentGameMode() != EGolfGameMode::StrokeMode)
    {
        LineImage->SetVisibility(ESlateVisibility::Hidden);
        return;
    }

    // 라인 길이 계산
    float LineLength = FVector2D::Distance(StartPos, EndPos);

    // 라인이 너무 짧으면 숨김
    if (LineLength < 3.0f)
    {
        LineImage->SetVisibility(ESlateVisibility::Hidden);
        return;
    }

    // 라인 중심점과 각도 계산
    FVector2D LineCenter = (StartPos + EndPos) * 0.5f;
    FVector2D LineDirection = (EndPos - StartPos).GetSafeNormal();
    float Angle = FMath::Atan2(LineDirection.Y, LineDirection.X) * 180.0f / PI;

    // UI 업데이트
    UCanvasPanelSlot* LineSlot = Cast<UCanvasPanelSlot>(LineImage->Slot);
    if (LineSlot)
    {
        LineSlot->SetSize(FVector2D(LineLength, AimLineThickness));
        LineSlot->SetPosition(LineCenter - FVector2D(LineLength * 0.5f, AimLineThickness * 0.5f));

        FWidgetTransform Transform;
        Transform.Angle = Angle;
        LineImage->SetRenderTransform(Transform);

        // 현재 플레이어에 따른 투명도 조정
        FLinearColor AdjustedColor = LineColor;
        if (GameMode && PlayerIndex == GameMode->CurrentPlayerIndex)
        {
            AdjustedColor.A = 1.0f; // 현재 플레이어는 완전 불투명
        }
        else
        {
            AdjustedColor.A = 0.5f; // 다른 플레이어는 더 투명
        }

        LineImage->SetColorAndOpacity(AdjustedColor);
        LineSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
    }

    // 표시 여부 결정 (StrokeMode에서만 표시)
    bool bShouldShow = !bShowOnlyCurrentPlayerAimLines ||
        (GameMode && PlayerIndex == GameMode->CurrentPlayerIndex);

    LineImage->SetVisibility(bShouldShow ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}

void UGolfMiniMap::RemoveAimActorForPlayer(int32 PlayerIndex)
{
    // AimActor 아이콘 제거
    if (PlayerAimActorImages.Contains(PlayerIndex))
    {
        if (UImage* AimActorImageToRemove = PlayerAimActorImages[PlayerIndex])
        {
            AimActorImageToRemove->RemoveFromParent();
            AimActorImageToRemove->ConditionalBeginDestroy();
        }
        PlayerAimActorImages.Remove(PlayerIndex);
        PlayerAimActorPositions.Remove(PlayerIndex);
    }

    // 볼 → AimActor 라인 제거
    if (PlayerBallToAimLineImages.Contains(PlayerIndex))
    {
        if (UImage* LineToRemove = PlayerBallToAimLineImages[PlayerIndex])
        {
            LineToRemove->RemoveFromParent();
            LineToRemove->ConditionalBeginDestroy();
        }
        PlayerBallToAimLineImages.Remove(PlayerIndex);
    }

    // AimActor → 홀컵 라인 제거
    if (PlayerAimToHoleLineImages.Contains(PlayerIndex))
    {
        if (UImage* LineToRemove = PlayerAimToHoleLineImages[PlayerIndex])
        {
            LineToRemove->RemoveFromParent();
            LineToRemove->ConditionalBeginDestroy();
        }
        PlayerAimToHoleLineImages.Remove(PlayerIndex);
    }

    UE_LOG(LogTemp, Log, TEXT("Removed AimActor system for player %d"), PlayerIndex);
}

void UGolfMiniMap::SetAimActorVisible(bool bVisible)
{
    bShowAimActor = bVisible;

    for (auto& Elem : PlayerAimActorImages)
    {
        if (IsValid(Elem.Value))
        {
            Elem.Value->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("AimActor visibility: %s"), bVisible ? TEXT("ON") : TEXT("OFF"));
}

void UGolfMiniMap::SetAimLineColors(FLinearColor BallToAimColor, FLinearColor AimToHoleColor)
{
    BallToAimLineColor = BallToAimColor;
    AimToHoleLineColor = AimToHoleColor;

    // 기존 라인들 색상 업데이트
    for (auto& Elem : PlayerBallToAimLineImages)
    {
        if (IsValid(Elem.Value))
        {
            Elem.Value->SetColorAndOpacity(BallToAimLineColor);
        }
    }

    for (auto& Elem : PlayerAimToHoleLineImages)
    {
        if (IsValid(Elem.Value))
        {
            Elem.Value->SetColorAndOpacity(AimToHoleLineColor);
        }
    }
}

void UGolfMiniMap::ShowAimActorMapPosition(int32 PlayerIndex)
{
    if (!PlayerAimActorImages.Contains(PlayerIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("No AimActor image for player %d"), PlayerIndex);
        return;
    }

    UImage* AimActorImage = PlayerAimActorImages[PlayerIndex];
    if (!IsValid(AimActorImage))
        return;

    UCanvasPanelSlot* ImageSlot = Cast<UCanvasPanelSlot>(AimActorImage->Slot);
    if (ImageSlot)
    {
        FVector2D CurrentPosition = ImageSlot->GetPosition();
        FVector2D CurrentSize = ImageSlot->GetSize();

        UE_LOG(LogTemp, Warning, TEXT("🗺️ Player %d AimActor on MiniMap:"), PlayerIndex);
        UE_LOG(LogTemp, Warning, TEXT("   Position: (%.1f, %.1f)"), CurrentPosition.X, CurrentPosition.Y);
        UE_LOG(LogTemp, Warning, TEXT("   Size: (%.1f, %.1f)"), CurrentSize.X, CurrentSize.Y);
        UE_LOG(LogTemp, Warning, TEXT("   Visible: %s"),
            AimActorImage->GetVisibility() == ESlateVisibility::Visible ? TEXT("YES") : TEXT("NO"));
    }
}


void UGolfMiniMap::SetAimToWorldPosition(const FVector& WorldPosition)
{
    if (!GameMode)
        return;

    // PlayerController 가져오기
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    AGolfPlayerController* GolfPC = Cast<AGolfPlayerController>(PC);

    if (!GolfPC)
    {
        UE_LOG(LogTemp, Error, TEXT("Could not get GolfPlayerController"));
        return;
    }

    // 현재 플레이어 볼 위치 가져오기
    FVector BallLocation = FVector::ZeroVector;
    if (GameMode->PlayerManager)
    {
        TArray<AGolfBall*> PlayerBalls = GameMode->PlayerManager->GetPlayerBalls();
        if (PlayerBalls.IsValidIndex(GameMode->CurrentPlayerIndex))
        {
            AGolfBall* CurrentBall = PlayerBalls[GameMode->CurrentPlayerIndex];
            if (CurrentBall)
            {
                BallLocation = CurrentBall->GetActorLocation();
            }
        }
    }

    if (BallLocation.IsZero())
    {
        UE_LOG(LogTemp, Warning, TEXT("Could not get current ball location"));
        return;
    }

    // 볼에서 클릭 위치로의 방향 계산
    FVector TargetDirection = (WorldPosition - BallLocation).GetSafeNormal();

    // Z축 성분 제거 (수평 방향만)
    TargetDirection.Z = 0.0f;
    TargetDirection.Normalize();

    // PlayerController의 AimDirection 업데이트
    GolfPC->AimDirection = TargetDirection;

    // AimActor 위치를 클릭한 위치로 설정
    if (GolfPC->GetAimActor())
    {
        // 지형 높이에 맞춘 위치로 조정
        FVector AdjustedPosition = GolfPC->AdjustToTerrainHeight(WorldPosition);
        GolfPC->GetAimActor()->SetActorLocation(AdjustedPosition);

        UE_LOG(LogTemp, Log, TEXT("AimActor moved to clicked position: %s"), *AdjustedPosition.ToString());
    }

    // 카메라를 해당 방향으로 회전
    if (GolfPC->CameraManager)
    {
        FRotator TargetRotation = TargetDirection.Rotation();
        GolfPC->CameraManager->SetActorRotation(TargetRotation);

        //  UE_LOG(LogTemp, Log, TEXT("Camera rotated to direction: %s"), *TargetDirection.ToString());
    }

    // GameMode의 AimLocation 업데이트
    GameMode->AimLocation = WorldPosition;


    // 미니맵 업데이트
    UpdateAimDirection(GameMode->CurrentPlayerIndex, TargetDirection);
    UpdateAimActorPosition(GameMode->CurrentPlayerIndex, WorldPosition);
    UpdateTip2();

    UE_LOG(LogTemp, Log, TEXT("Aim set to world position: %s (Direction: %s)"),
        *WorldPosition.ToString(), *TargetDirection.ToString());
}

// 플레이어 전환 시 AimActor 이미지들 숨기기
void UGolfMiniMap::HideAllAimActorExceptCurrent()
{
    if (!GameMode)
        return;

    for (auto& Elem : PlayerAimActorImages)
    {
        int32 PlayerIndex = Elem.Key;
        UImage* AimActorImage = Elem.Value;

        if (IsValid(AimActorImage))
        {
            // 현재 플레이어가 아니면 숨김
            bool bShouldShow = bShowAimActor && (PlayerIndex == GameMode->CurrentPlayerIndex);
            AimActorImage->SetVisibility(bShouldShow ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
        }
    }

    // 라인들도 현재 플레이어만 표시
    for (auto& Elem : PlayerBallToAimLineImages)
    {
        int32 PlayerIndex = Elem.Key;
        UImage* LineImage = Elem.Value;

        if (IsValid(LineImage))
        {
            bool bShouldShow = (PlayerIndex == GameMode->CurrentPlayerIndex) && !bShowOnlyCurrentPlayerAimLines;
            LineImage->SetVisibility(bShouldShow ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
        }
    }

    for (auto& Elem : PlayerAimToHoleLineImages)
    {
        int32 PlayerIndex = Elem.Key;
        UImage* LineImage = Elem.Value;

        if (IsValid(LineImage))
        {
            bool bShouldShow = (PlayerIndex == GameMode->CurrentPlayerIndex) && !bShowOnlyCurrentPlayerAimLines;
            LineImage->SetVisibility(bShouldShow ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Hidden all AimActor elements except current player %d"), GameMode->CurrentPlayerIndex);
}

// 5. 디버그 함수 추가 - 에임 라인 상태 체크
void UGolfMiniMap::DebugAimLineStatus()
{
    if (!GameMode)
    {
        UE_LOG(LogTemp, Error, TEXT("No GameMode for debug"));
        return;
    }

    int32 CurrentPlayer = GameMode->CurrentPlayerIndex;

    UE_LOG(LogTemp, Warning, TEXT("=== Aim Line Debug Status ==="));
    UE_LOG(LogTemp, Warning, TEXT("Current Player: %d"), CurrentPlayer);

    // 볼 위치 체크
    if (PlayerBallWorldPositions.Contains(CurrentPlayer))
    {
        FVector BallPos = PlayerBallWorldPositions[CurrentPlayer];
        FVector2D BallMapPos = WorldToMapPosition(BallPos);
        UE_LOG(LogTemp, Warning, TEXT("Ball Position: World=%s, Map=(%.1f,%.1f)"),
            *BallPos.ToString(), BallMapPos.X, BallMapPos.Y);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("No ball position for current player"));
    }

    // AimActor 위치 체크
    if (PlayerAimActorPositions.Contains(CurrentPlayer))
    {
        FVector AimPos = PlayerAimActorPositions[CurrentPlayer];
        FVector2D AimMapPos = WorldToMapPosition(AimPos);
        UE_LOG(LogTemp, Warning, TEXT("AimActor Position: World=%s, Map=(%.1f,%.1f)"),
            *AimPos.ToString(), AimMapPos.X, AimMapPos.Y);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("No AimActor position for current player"));

        if (GameMode && !GameMode->AimLocation.IsZero())
        {
            UE_LOG(LogTemp, Warning, TEXT("GameMode AimLocation available: %s"), *GameMode->AimLocation.ToString());
        }
    }


    UE_LOG(LogTemp, Warning, TEXT("================================"));
}

void UGolfMiniMap::DebugAimActorLines(int32 PlayerIndex)
{
    UE_LOG(LogTemp, Warning, TEXT("=== AimActor Lines Debug for Player %d ==="), PlayerIndex);

    // Ball-to-Aim 라인 체크
    if (PlayerBallToAimLineImages.Contains(PlayerIndex))
    {
        UImage* LineImage = PlayerBallToAimLineImages[PlayerIndex];
        if (IsValid(LineImage))
        {
            ESlateVisibility nVisibility = LineImage->GetVisibility();
            UE_LOG(LogTemp, Warning, TEXT("Ball-to-Aim Line: Valid, Visibility=%s"),
                nVisibility == ESlateVisibility::Visible ? TEXT("Visible") : TEXT("Hidden"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Ball-to-Aim Line: Invalid"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Ball-to-Aim Line: Not Found"));
    }

    // Aim-to-Hole 라인 체크
    if (PlayerAimToHoleLineImages.Contains(PlayerIndex))
    {
        UImage* LineImage = PlayerAimToHoleLineImages[PlayerIndex];
        if (IsValid(LineImage))
        {
            ESlateVisibility nVisibility = LineImage->GetVisibility();
            UE_LOG(LogTemp, Warning, TEXT("Aim-to-Hole Line: Valid, Visibility=%s"),
                nVisibility == ESlateVisibility::Visible ? TEXT("Visible") : TEXT("Hidden"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Aim-to-Hole Line: Invalid"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Aim-to-Hole Line: Not Found"));
    }
}

void UGolfMiniMap::ShowOnlyCurrentPlayer(int32 CurrentPlayerIndex)
{
    if (!bIsInitialized || !GameMode)
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("🎯 ShowOnlyCurrentPlayer: %d"), CurrentPlayerIndex);

    // 모든 플레이어 요소 숨기기
    HideAllPlayersExcept(CurrentPlayerIndex);

    // 현재 플레이어만 표시
    ShowPlayerElements(CurrentPlayerIndex);

    // 현재 표시된 플레이어 인덱스 저장
    LastDisplayedPlayerIndex = CurrentPlayerIndex;
}

void UGolfMiniMap::HideAllPlayersExcept(int32 ExceptPlayerIndex)
{

    // 모든 플레이어의 AimActor 이미지 숨기기
    for (auto& Elem : PlayerAimActorImages)
    {
        int32 PlayerIndex = Elem.Key;
        if (PlayerIndex != ExceptPlayerIndex && IsValid(Elem.Value))
        {
            Elem.Value->SetVisibility(ESlateVisibility::Hidden);
        }
    }

    // 모든 플레이어의 볼-에임 라인 숨기기
    for (auto& Elem : PlayerBallToAimLineImages)
    {
        int32 PlayerIndex = Elem.Key;
        if (PlayerIndex != ExceptPlayerIndex && IsValid(Elem.Value))
        {
            Elem.Value->SetVisibility(ESlateVisibility::Hidden);
        }
    }

    // 모든 플레이어의 에임-홀컵 라인 숨기기
    for (auto& Elem : PlayerAimToHoleLineImages)
    {
        int32 PlayerIndex = Elem.Key;
        if (PlayerIndex != ExceptPlayerIndex && IsValid(Elem.Value))
        {
            Elem.Value->SetVisibility(ESlateVisibility::Hidden);
        }
    }

    // 모든 플레이어의 볼-홀컵 라인 숨기기 (선택사항)
    for (auto& Elem : PlayerBallToHoleLineImages)
    {
        int32 PlayerIndex = Elem.Key;
        if (PlayerIndex != ExceptPlayerIndex && IsValid(Elem.Value))
        {
            Elem.Value->SetVisibility(ESlateVisibility::Hidden);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("🙈 Hidden all player elements except player %d"), ExceptPlayerIndex);
}

void UGolfMiniMap::HidePlayerElements(int32 PlayerIndex)
{


    // AimActor 이미지 숨기기
    if (PlayerAimActorImages.Contains(PlayerIndex))
    {
        UImage* AimActorImage = PlayerAimActorImages[PlayerIndex];
        if (IsValid(AimActorImage))
        {
            AimActorImage->SetVisibility(ESlateVisibility::Hidden);
        }
    }

    // 볼-에임 라인 숨기기
    if (PlayerBallToAimLineImages.Contains(PlayerIndex))
    {
        UImage* BallToAimLineImage = PlayerBallToAimLineImages[PlayerIndex];
        if (IsValid(BallToAimLineImage))
        {
            BallToAimLineImage->SetVisibility(ESlateVisibility::Hidden);
        }
    }

    // 에임-홀컵 라인 숨기기
    if (PlayerAimToHoleLineImages.Contains(PlayerIndex))
    {
        UImage* AimToHoleLineImage = PlayerAimToHoleLineImages[PlayerIndex];
        if (IsValid(AimToHoleLineImage))
        {
            AimToHoleLineImage->SetVisibility(ESlateVisibility::Hidden);
        }
    }

    // 볼-홀컵 라인 숨기기
    if (PlayerBallToHoleLineImages.Contains(PlayerIndex))
    {
        UImage* BallToHoleLineImage = PlayerBallToHoleLineImages[PlayerIndex];
        if (IsValid(BallToHoleLineImage))
        {
            BallToHoleLineImage->SetVisibility(ESlateVisibility::Hidden);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("🚫 Hidden elements for player %d"), PlayerIndex);
}

void UGolfMiniMap::ShowPlayerElements(int32 PlayerIndex)
{
    if (!GameMode)
        return;

    // StrokeMode가 아니면 에임 관련 요소들 표시하지 않음
    bool bShowAimElements = (GameMode->GetCurrentGameMode() == EGolfGameMode::StrokeMode);

    // 볼 이미지는 항상 표시
    if (PlayerBallImages.Contains(PlayerIndex))
    {
        UImage* BallImage = PlayerBallImages[PlayerIndex];
        if (IsValid(BallImage))
        {
            BallImage->SetVisibility(ESlateVisibility::Visible);
        }
    }

    // 에임 관련 요소들은 StrokeMode에서만 표시
    if (bShowAimElements)
    {

        // AimActor 이미지 표시
        if (PlayerAimActorImages.Contains(PlayerIndex) && bShowAimActor)
        {
            UImage* AimActorImage = PlayerAimActorImages[PlayerIndex];
            if (IsValid(AimActorImage))
            {
                AimActorImage->SetVisibility(ESlateVisibility::Visible);
                // AimActor 위치도 업데이트
                if (GameMode && !GameMode->AimLocation.IsZero())
                {
                    UpdateAimActorPosition(PlayerIndex, GameMode->AimLocation);
                }
            }
        }

        // 볼-에임 라인 표시
        if (PlayerBallToAimLineImages.Contains(PlayerIndex))
        {
            UpdateBallToAimLinePosition(PlayerIndex); // 위치 업데이트 후 표시
        }

        // 에임-홀컵 라인 표시
        if (PlayerAimToHoleLineImages.Contains(PlayerIndex))
        {
            UpdateAimToHoleLinePosition(PlayerIndex); // 위치 업데이트 후 표시
        }
    }

    // 볼-홀컵 라인은 설정에 따라 표시
    if (bShowBallToHoleLine && PlayerBallToHoleLineImages.Contains(PlayerIndex))
    {
        UpdateBallToHoleLinePosition(PlayerIndex); // 위치 업데이트 후 표시
    }

    UE_LOG(LogTemp, Log, TEXT("👁️ Showed elements for player %d (AimElements: %s)"),
        PlayerIndex, bShowAimElements ? TEXT("ON") : TEXT("OFF"));
}

void UGolfMiniMap::OnPlayerTurnChanged(int32 NewCurrentPlayerIndex, int32 PreviousPlayerIndex)
{
    if (!bIsInitialized)
    {
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("🔄 Player turn changed: %d → %d"), PreviousPlayerIndex, NewCurrentPlayerIndex);

    // 이전 플레이어 요소들 숨기기
    if (PreviousPlayerIndex >= 0 && PreviousPlayerIndex != NewCurrentPlayerIndex)
    {
        HidePlayerElements(PreviousPlayerIndex);
        UE_LOG(LogTemp, Log, TEXT("🙈 Hidden previous player %d elements"), PreviousPlayerIndex);
    }

    // 현재 플레이어만 표시
    ShowOnlyCurrentPlayer(NewCurrentPlayerIndex);

    // 미니맵의 거리/고도 정보도 현재 플레이어 기준으로 업데이트
    if (PlayerBallWorldPositions.Contains(NewCurrentPlayerIndex))
    {
        FVector BallPos = PlayerBallWorldPositions[NewCurrentPlayerIndex];
        float Distance = FVector::Dist(BallPos, HolecupWorldPosition);
        float Elevation = HolecupWorldPosition.Z - BallPos.Z;
        UpdateDistanceAndElevation(Distance, Elevation);
    }

    UE_LOG(LogTemp, Warning, TEXT("✅ Player turn change completed for player %d "), NewCurrentPlayerIndex);
}