#include "TerraParkgameInstance.h"
#include "ExternalPakManager.h"
#include "InGameMode.h"
#include "SoundManager.h"
#include "Misc/PackageName.h"            // Register/UnRegister + LongPackageNameToFilename
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"     // IterateDirectory
#include "Algo/Unique.h"
#include <Runtime/MoviePlayer/Public/MoviePlayer.h>
#include <Runtime/UMG/Public/Blueprint/WidgetBlueprintLibrary.h>
#include "ParkDay/Widgets/FadeWidget.h"
#include "Widgets/LoadingWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGLoading, Log, All);

UTerraParkgameInstance::UTerraParkgameInstance()
{
    // ★ FadeWidget도 생성자에서 미리 레퍼런스 확보
    static ConstructorHelpers::FClassFinder<UFadeWidget> FadeWidgetBPClass(
        TEXT("/Game/UMG/UI/WBP_Fade.WBP_Fade_C")
    );
    if (FadeWidgetBPClass.Succeeded())
    {
        FadeWidgetClass_Ref = FadeWidgetBPClass.Class;
        UE_LOG(LogTemp, Log, TEXT("✅ FadeWidgetClass 생성자 로드 성공"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ FadeWidgetClass 생성자 로드 실패 - 경로 확인 필요"));
    }

    static ConstructorHelpers::FClassFinder<UUserWidget> LoadingScreenWidgetBPClass(
        TEXT("/Game/UMG/UI/Loding/WBP_Loading.WBP_Loading_C")
    );

    if (LoadingScreenWidgetBPClass.Succeeded())
    {
        LoadingScreenWidgetClass = LoadingScreenWidgetBPClass.Class;
        UE_LOG(LogTemp, Log, TEXT("✅ LoadingScreenWidgetClass 로드 성공"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ LoadingScreenWidgetClass 로드 실패"));
    }
}

void UTerraParkgameInstance::Init()
{
    Super::Init();
    UE_LOG(LogTemp, Log, TEXT("[GI] Init"));


    if (FSlateApplication::IsInitialized())
    {
        TouchFilter = MakeShared<FTouchDoubleTriggerFilter>();
        // 필요 시 조절: 0.06~0.12 권장
        TouchFilter->SetBlockWindowSeconds(0.15f);

        // 우선순위 0(높을수록 먼저 처리)로 등록
        FSlateApplication::Get().RegisterInputPreProcessor(TouchFilter, 100);
    }

    // 맵 전환이 완료되면 혹시 남은 로딩 화면 정리
    FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UTerraParkgameInstance::OnPostLoadMap);

    // ★핵심: OpenLevel 직전에 엔진이 호출하는 델리게이트에 바인딩 → 자동 로딩 화면
    if (IGameMoviePlayer* MP = GetMoviePlayer())
    {
        MP->OnPrepareLoadingScreen().AddUObject(this, &UTerraParkgameInstance::HandlePrepareLoadingScreen);
    }

    SetupRetryCount = 0;
    SetupAudioPolicy();

    // ★ 기존 TryLoadClass 대신 생성자에서 확보한 레퍼런스 사용
    if (FadeWidgetClass_Ref)
    {
        FadeWidget = CreateWidget<UFadeWidget>(this, FadeWidgetClass_Ref);
        if (FadeWidget)
        {
            FadeWidget->AddToViewport(99999);
            FadeWidget->SetVisibility(ESlateVisibility::Collapsed);
            UE_LOG(LogTemp, Log, TEXT("✅ FadeWidget 생성 완료"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("❌ FadeWidget CreateWidget 실패"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ FadeWidgetClass_Ref null - 생성자 로드 실패"));
    }
}
//OnStart는 늦게 됨
void UTerraParkgameInstance::OnStart()
{
    Super::OnStart();
    UE_LOG(LogTemp, Log, TEXT("[GI] OnStart"));


}

void UTerraParkgameInstance::Shutdown()
{
    UE_LOG(LogTemp, Log, TEXT("[GI] Shutdown"));
    Super::Shutdown();

    if (TouchFilter.IsValid() && FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().UnregisterInputPreProcessor(TouchFilter);
        TouchFilter.Reset();
    }

    if (IGameMoviePlayer* MP = GetMoviePlayer())
    {
        MP->OnPrepareLoadingScreen().RemoveAll(this);
    }
    FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
    Super::Shutdown();
}

// ---- BP용 래퍼 ----
void UTerraParkgameInstance::SetupAudioPolicy()
{
    SetupRetryCount = 0;
    SetupAudioPolicy_Internal();
}

USoundManager* UTerraParkgameInstance::GetSoundManagerBP() const
{
    return GetSubsystem<USoundManager>(); // null일 수 있으니 BP에서 IsValid 체크
}


// ---- 내부 구현 ----
void UTerraParkgameInstance::SetupAudioPolicy_Internal()
{
    USoundManager* SM = GetSubsystem<USoundManager>();
    if (!SM)
    {
        // 아직 Subsystem이 생성 전이면 다음 틱에 재시도
        if (UWorld* W = GetWorld())
        {
            if (W && SetupRetryCount < kMaxSetupRetry)
            {
                ++SetupRetryCount;
                W->GetTimerManager().SetTimerForNextTick(this, &UTerraParkgameInstance::SetupAudioPolicy_Internal);
                UE_LOG(LogTemp, Warning, TEXT("[GI] SoundManager not ready. Retry #%d"), SetupRetryCount);
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[GI] SoundManager not available (giving up)."));
        }
        return;
    }

    // Soft -> Hard 로드 (동기, 4.26)
    USoundClass* BgmClass = BGMClass.IsNull() ? nullptr : BGMClass.LoadSynchronous();
    USoundClass* VcClass = VoiceClass.IsNull() ? nullptr : VoiceClass.LoadSynchronous();
    USoundMix* Mix = DuckMix.IsNull() ? nullptr : DuckMix.LoadSynchronous();
    USoundConcurrency* Vc = VoiceConcurrency.IsNull() ? nullptr : VoiceConcurrency.LoadSynchronous();
    UDataTable* Table = SoundTable.IsNull() ? nullptr : SoundTable.LoadSynchronous();

    // SoundManager 세팅
    SM->SetupSoundPolicy(BgmClass, VcClass, Mix, Vc, Table);

    LogSoundSetup(TEXT("GI.SetupAudioPolicy"), true);
}

void UTerraParkgameInstance::LogSoundSetup(const TCHAR* Where, bool bOk) const
{
    UE_LOG(LogTemp, Log, TEXT("[%s] Audio policy set: BGM=%s, Voice=%s, Mix=%s, Concurrency=%s, Table=%s"),
        Where,
        BGMClass.IsNull() ? TEXT("None") : *BGMClass.ToString(),
        VoiceClass.IsNull() ? TEXT("None") : *VoiceClass.ToString(),
        DuckMix.IsNull() ? TEXT("None") : *DuckMix.ToString(),
        VoiceConcurrency.IsNull() ? TEXT("None") : *VoiceConcurrency.ToString(),
        SoundTable.IsNull() ? TEXT("None") : *SoundTable.ToString()
    );
}


bool UTerraParkgameInstance::CanUseMoviePlayer() const
{
#if WITH_EDITOR
    return IsRunningGame(); // Standalone/패키지에서만 MoviePlayer가 제대로 그림
#else
    return true;
#endif
}

void UTerraParkgameInstance::HandlePrepareLoadingScreen()
{
    if (!LoadingScreenWidgetClass)
    {
        UE_LOG(LogPGLoading, Warning, TEXT("LoadingScreenWidgetClass not set."));
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogPGLoading, Warning, TEXT("World is null in HandlePrepareLoadingScreen."));
        return;
    }

    if (!ActiveLoadingWidget.IsValid())
    {
        UE_LOG(LogPGLoading, Error, TEXT("Failed to create LoadingScreen widget."));
        return;
    }

    if (!World->GetLevel(0)->GetName().Equals("Level_UI"))
    {
        UE_LOG(LogTemp, Warning, TEXT("loadingSkip"));
        return;
    }

    if (CanUseMoviePlayer() && FSlateApplication::IsInitialized())
    {
        SetupMoviePlayerWithWidget(ActiveLoadingWidget.Get());
        ActiveLoadingWidget.Get()->SetVisibility(ESlateVisibility::Visible);

        ActiveLoadingWidget.Get()->PlayBGM();
        GetMoviePlayer()->PlayMovie(); // 자동 재생

        UE_LOG(LogPGLoading, Log, TEXT("Loading screen started (OnPrepareLoadingScreen)."));
    }
    else
    {
        // PIE 대체: 뷰포트에 그냥 올려서 분위기만
        ActiveLoadingWidget.Get()->AddToViewport(10000);
        ActiveLoadingWidget.Get()->SetVisibility(ESlateVisibility::Visible);
        if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
        {
            UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(PC, ActiveLoadingWidget.Get());
            PC->bShowMouseCursor = true;
        }
        UE_LOG(LogPGLoading, Log, TEXT("Fallback loading widget added to viewport (PIE)."));
    }
}

void UTerraParkgameInstance::SetupMoviePlayerWithWidget(UUserWidget* Widget)
{
    FLoadingScreenAttributes Attr;
    Attr.WidgetLoadingScreen = Widget->TakeWidget();
    Attr.bAutoCompleteWhenLoadingCompletes = bAutoCompleteWhenLoadingCompletes;
    Attr.bWaitForManualStop = bPlayUntilStopped;
    Attr.MinimumLoadingScreenDisplayTime = MinimumDisplayTime;
    Attr.bAllowEngineTick = true;
    Attr.bMoviesAreSkippable = true;

#if WITH_EDITOR
    Attr.bAutoCompleteWhenLoadingCompletes = true;
    Attr.bWaitForManualStop = false;
#endif

    GetMoviePlayer()->SetupLoadingScreen(Attr);
}

void UTerraParkgameInstance::OnPostLoadMap(UWorld* /*LoadedWorld*/)
{
    //StopLoadingScreen(); // 자동/수동 모두 안전하게 정리
}

void UTerraParkgameInstance::StartLoadingScreen()
{
    if (ActiveLoadingWidget.Get())
    {
        ActiveLoadingWidget.Get()->PlayBGM();
        ActiveLoadingWidget.Get()->AddToViewport(10000);
        ActiveLoadingWidget.Get()->SetVisibility(ESlateVisibility::Visible);
        UE_LOG(LogTemp, Log, TEXT("✅ 로딩 화면 위젯 초기화 완료"));
    }
    else
    {
        if (LoadingScreenWidgetClass)
        {
            APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
            if (PC)
            {
                ActiveLoadingWidget = CreateWidget<ULoadingWidget>(PC, LoadingScreenWidgetClass);
                if (ActiveLoadingWidget.Get())
                {
                    ActiveLoadingWidget.Get()->PlayBGM();
                    ActiveLoadingWidget.Get()->AddToViewport(10000);
                    ActiveLoadingWidget.Get()->SetVisibility(ESlateVisibility::Visible);
                    UE_LOG(LogTemp, Log, TEXT("✅ 로딩 화면 위젯 초기화 완료"));
                }
            }
        }
    }
}

void UTerraParkgameInstance::StopLoadingScreen()
{
    GetMoviePlayer()->StopMovie();

    if (ActiveLoadingWidget.Get())
    {
        ActiveLoadingWidget.Get()->SetVisibility(ESlateVisibility::Collapsed);
        ActiveLoadingWidget.Get()->StopBGM();
    }
}
