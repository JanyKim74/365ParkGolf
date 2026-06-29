#include "TerraParkgameInstance.h"
#include "ExternalPakManager.h"
#include "InGameMode.h"
#include "SoundManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "Algo/Unique.h"
#include <Runtime/MoviePlayer/Public/MoviePlayer.h>
#include <Runtime/UMG/Public/Blueprint/WidgetBlueprintLibrary.h>
#include "ParkDay/Widgets/FadeWidget.h"
#include "Widgets/LoadingWidget.h"
#include "Kismet/KismetSystemLibrary.h"  // ← QuitGame

DEFINE_LOG_CATEGORY_STATIC(LogPGLoading, Log, All);

UTerraParkgameInstance::UTerraParkgameInstance()
{
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

    // LicenseErrorWidget 클래스 로드
    static ConstructorHelpers::FClassFinder<ULicenseErrorWidget> LicenseErrorBPClass(
        TEXT("/Game/UMG/UI/LicenseErrorWidget.LicenseErrorWidget_C")
    );
    if (LicenseErrorBPClass.Succeeded())
    {
        LicenseErrorWidgetClass = LicenseErrorBPClass.Class;
        UE_LOG(LogTemp, Log, TEXT("[License] LicenseErrorWidgetClass 로드 성공"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[License] WBP_LicenseError 없음 — 네이티브 메시지박스 사용"));
    }
}

void UTerraParkgameInstance::Init()
{
    Super::Init();
    UE_LOG(LogTemp, Log, TEXT("[GI] Init"));

    if (FSlateApplication::IsInitialized())
    {
        TouchFilter = MakeShared<FTouchDoubleTriggerFilter>();
        TouchFilter->SetBlockWindowSeconds(0.15f);
        FSlateApplication::Get().RegisterInputPreProcessor(TouchFilter, 100);
    }

    FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
        this, &UTerraParkgameInstance::OnPostLoadMap);

    if (IGameMoviePlayer* MP = GetMoviePlayer())
    {
        MP->OnPrepareLoadingScreen().AddUObject(
            this, &UTerraParkgameInstance::HandlePrepareLoadingScreen);
    }

    SetupRetryCount = 0;

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
        UE_LOG(LogTemp, Error, TEXT("❌ FadeWidgetClass_Ref null"));
    }

    // ── 라이선스 검증 ─────────────────────────────────────────
    LicenseManager = NewObject<ULicenseManager>(this);
    LicenseManager->Initialize();

    UE_LOG(LogTemp, Log, TEXT("[License] 라이선스 검증 시작..."));

    FOnLicenseResult Callback;
    Callback.BindUFunction(this, FName("OnLicenseResult"));
    LicenseManager->ValidateAsync(Callback);
}

void UTerraParkgameInstance::OnStart()
{
    Super::OnStart();
    UE_LOG(LogTemp, Log, TEXT("[GI] OnStart"));

    if (bSetupAudioOnStart)
    {
        SetupAudioPolicy();
    }
}

void UTerraParkgameInstance::Shutdown()
{
    UE_LOG(LogTemp, Log, TEXT("[GI] Shutdown"));

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

// ================================================================
//  Audio
// ================================================================

void UTerraParkgameInstance::SetupAudioPolicy()
{
    SetupRetryCount = 0;
    SetupAudioPolicy_Internal();
}

USoundManager* UTerraParkgameInstance::GetSoundManagerBP() const
{
    return GetSubsystem<USoundManager>();
}

void UTerraParkgameInstance::SetupAudioPolicy_Internal()
{
    USoundManager* SM = GetSubsystem<USoundManager>();
    if (!SM)
    {
        if (UWorld* W = GetWorld())
        {
            if (SetupRetryCount < kMaxSetupRetry)
            {
                ++SetupRetryCount;
                W->GetTimerManager().SetTimerForNextTick(
                    this, &UTerraParkgameInstance::SetupAudioPolicy_Internal);
                UE_LOG(LogTemp, Warning, TEXT("[GI] SoundManager not ready. Retry #%d"),
                    SetupRetryCount);
            }
        }
        return;
    }

    USoundClass* BgmClass = BGMClass.ToSoftObjectPath().IsValid()
        ? BGMClass.LoadSynchronous() : nullptr;
    USoundClass* VcClass = VoiceClass.ToSoftObjectPath().IsValid()
        ? VoiceClass.LoadSynchronous() : nullptr;
    USoundMix* Mix = DuckMix.ToSoftObjectPath().IsValid()
        ? DuckMix.LoadSynchronous() : nullptr;
    USoundConcurrency* Vc = VoiceConcurrency.ToSoftObjectPath().IsValid()
        ? VoiceConcurrency.LoadSynchronous() : nullptr;
    UDataTable* Table = SoundTable.ToSoftObjectPath().IsValid()
        ? SoundTable.LoadSynchronous() : nullptr;

    SM->SetupSoundPolicy(BgmClass, VcClass, Mix, Vc, Table);
    LogSoundSetup(TEXT("GI.SetupAudioPolicy"), true);
}

void UTerraParkgameInstance::LogSoundSetup(const TCHAR* Where, bool bOk) const
{
    UE_LOG(LogTemp, Log,
        TEXT("[%s] Audio policy set: BGM=%s, Voice=%s, Mix=%s, Concurrency=%s, Table=%s"),
        Where,
        BGMClass.IsNull() ? TEXT("None") : *BGMClass.ToString(),
        VoiceClass.IsNull() ? TEXT("None") : *VoiceClass.ToString(),
        DuckMix.IsNull() ? TEXT("None") : *DuckMix.ToString(),
        VoiceConcurrency.IsNull() ? TEXT("None") : *VoiceConcurrency.ToString(),
        SoundTable.IsNull() ? TEXT("None") : *SoundTable.ToString());
}

// ================================================================
//  LoadingScreen
// ================================================================

bool UTerraParkgameInstance::CanUseMoviePlayer() const
{
#if WITH_EDITOR
    return IsRunningGame();
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
    if (!World) return;

    if (!ActiveLoadingWidget.IsValid()) return;

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
        GetMoviePlayer()->PlayMovie();
        UE_LOG(LogPGLoading, Log, TEXT("Loading screen started."));
    }
    else
    {
        ActiveLoadingWidget.Get()->AddToViewport(10000);
        ActiveLoadingWidget.Get()->SetVisibility(ESlateVisibility::Visible);
        if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
        {
            UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(PC, ActiveLoadingWidget.Get());
            PC->bShowMouseCursor = true;
        }
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

void UTerraParkgameInstance::OnPostLoadMap(UWorld* /*LoadedWorld*/) {}

void UTerraParkgameInstance::StartLoadingScreen()
{
    if (ActiveLoadingWidget.Get())
    {
        ActiveLoadingWidget.Get()->PlayBGM();
        ActiveLoadingWidget.Get()->AddToViewport(10000);
        ActiveLoadingWidget.Get()->SetVisibility(ESlateVisibility::Visible);
    }
    else if (LoadingScreenWidgetClass)
    {
        if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
        {
            ActiveLoadingWidget = CreateWidget<ULoadingWidget>(PC, LoadingScreenWidgetClass);
            if (ActiveLoadingWidget.Get())
            {
                ActiveLoadingWidget.Get()->PlayBGM();
                ActiveLoadingWidget.Get()->AddToViewport(10000);
                ActiveLoadingWidget.Get()->SetVisibility(ESlateVisibility::Visible);
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

// ================================================================
//  라이선스
// ================================================================

void UTerraParkgameInstance::OnLicenseResult(bool bIsValid, ELicenseStatus Status)
{
    if (bIsValid)
    {
        UE_LOG(LogTemp, Log, TEXT("[License] 인증 성공 — %s"),
            *LicenseManager->GetCustomerName());
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[License] 인증 실패 — 에러 창 표시"));
    ShowLicenseError(Status);
}

void UTerraParkgameInstance::ShowLicenseError(ELicenseStatus Status)
{
    // ── 1순위: UMG 위젯 (에디터에서 WBP_LicenseError 지정 시) ──
    if (LicenseErrorWidgetClass)
    {
        APlayerController* PC = GetFirstLocalPlayerController();
        if (PC)
        {
            LicenseErrorWidget = CreateWidget<ULicenseErrorWidget>(PC, LicenseErrorWidgetClass);
            if (LicenseErrorWidget)
            {
                LicenseErrorWidget->AddToViewport(100000);  // 최상위 레이어
                PC->SetShowMouseCursor(true);
                PC->SetInputMode(FInputModeUIOnly());
                LicenseErrorWidget->ShowError(Status);      // 메시지 + 카운트다운
                UE_LOG(LogTemp, Log, TEXT("[License] 에러 위젯 표시 완료"));
                return;
            }
        }
    }

    // ── 2순위: 플랫폼 네이티브 메시지박스 (위젯 클래스 미설정 시) ──
    FString Title;
    FString Message;

    switch (Status)
    {
    case ELicenseStatus::Revoked:
        Title = TEXT("라이선스 취소");
        Message = TEXT("이 PC의 라이선스가 취소되었습니다.\n관리자에게 문의하세요.");
        break;
    case ELicenseStatus::Expired:
        Title = TEXT("라이선스 만료");
        Message = TEXT("라이선스 유효기간이 만료되었습니다.\n관리자에게 갱신을 요청하세요.");
        break;
    case ELicenseStatus::NotFound:
        Title = TEXT("라이선스 없음");
        Message = TEXT("이 PC에 등록된 라이선스가 없습니다.\n관리자에게 등록을 요청하세요.");
        break;
    case ELicenseStatus::SignatureFail:
        Title = TEXT("라이선스 손상");
        Message = TEXT("라이선스 파일이 손상되었습니다.\n재설치 후 다시 시도하세요.");
        break;
    case ELicenseStatus::OfflineExpired:
        Title = TEXT("오프라인 기간 초과");
        Message = TEXT("오프라인 상태가 너무 오래되었습니다.\n인터넷 연결 후 재시작하세요.");
        break;
    default:
        Title = TEXT("인증 실패");
        Message = TEXT("라이선스 서버에 연결할 수 없습니다.\n인터넷 연결을 확인하세요.");
        break;
    }

    UE_LOG(LogTemp, Error, TEXT("[License] %s — %s"), *Title, *Message);

    // Windows 네이티브 메시지박스 (블로킹) → 확인 클릭 후 즉시 종료
    FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *Message, *Title);
    UKismetSystemLibrary::QuitGame(GetWorld(), nullptr, EQuitPreference::Quit, false);
}