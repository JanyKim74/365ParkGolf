#include "WebcamCapture.h"
#include "SwingVideoWidget.h"
#include "WebcamConfig.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "FileMediaSource.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Canvas.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "UObject/ConstructorHelpers.h"


// ========== ⭐ 프레임 저장을 위한 헤더 (UE4.26 호환) ==========
#include "ImageUtils.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
//#include <opencv2/opencv.hpp>

// ✅ Windows API for VID/PID search
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include <SetupAPI.h>
#include <Cfgmgr32.h>
#include <devguid.h>
#include <initguid.h>
#include <Dbt.h>
#include "Windows/HideWindowsPlatformTypes.h"
#include "ParkDayProfiling.h"

#pragma comment(lib, "SetupAPI.lib")
#pragma comment(lib, "Cfgmgr32.lib")

// KSCATEGORY_VIDEO_CAMERA GUID
DEFINE_GUID(KSCATEGORY_VIDEO_CAMERA,
    0xe5323777, 0xf976, 0x4f5b, 0x9b, 0x55, 0xb9, 0x46, 0x99, 0xc4, 0x6e, 0x44);
#endif




// ✅ Helper Functions
namespace WebcamSearchHelpers
{
    // VID/PID 추출
    bool ExtractVIDPIDFromString(const FString& HardwareID, FString& OutVID, FString& OutPID)
    {
        int32 VidIndex = HardwareID.Find(TEXT("VID_"), ESearchCase::IgnoreCase);
        int32 PidIndex = HardwareID.Find(TEXT("PID_"), ESearchCase::IgnoreCase);

        if (VidIndex == INDEX_NONE || PidIndex == INDEX_NONE)
            return false;

        OutVID = HardwareID.Mid(VidIndex + 4, 4).ToUpper();
        OutPID = HardwareID.Mid(PidIndex + 4, 4).ToUpper();

        return true;
    }

    // vidcap:// URL 생성
    FString CreateVidcapURL(const FString& DevicePath)
    {
        FString URL = TEXT("vidcap://");
        URL += DevicePath;
        return URL;
    }
}

AWebcamCapture::AWebcamCapture()
{
    PrimaryActorTick.bCanEverTick = true;

    UE_LOG(LogTemp, Log, TEXT("📹 WebcamCapture Constructor Start"));

    // VideoBufferComponent 생성
    VideoBufferComponent = CreateDefaultSubobject<UVideoBufferComponent>(TEXT("VideoBufferComponent"));
    if (VideoBufferComponent)
    {
        UE_LOG(LogTemp, Log, TEXT("  ✅ VideoBufferComponent created"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("  ❌ VideoBufferComponent creation FAILED"));
    }

    // ✅ MediaPlayer 에셋 로드
    static ConstructorHelpers::FObjectFinder<UMediaPlayer> MediaPlayerAsset(
        TEXT("/Game/GolfGameBluePrint/SwingAnalyzer/NewMediaPlayer")
    );
    if (MediaPlayerAsset.Succeeded())
    {
        MediaPlayer = MediaPlayerAsset.Object;
        UE_LOG(LogTemp, Log, TEXT("  ✅ MediaPlayer loaded from asset: NewMediaPlayer"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("  ❌ MediaPlayer asset not found at /Game/GolfGameBluePrint/SwingAnalyzer/NewMediaPlayer"));
    }

    // ✅ MediaTexture 에셋 로드
    static ConstructorHelpers::FObjectFinder<UMediaTexture> MediaTextureAsset(
        TEXT("/Game/GolfGameBluePrint/SwingAnalyzer/NewMediaPlayer_Video")
    );
    if (MediaTextureAsset.Succeeded())
    {
        MediaTexture = MediaTextureAsset.Object;
        UE_LOG(LogTemp, Log, TEXT("  ✅ MediaTexture loaded from asset: NewMediaPlayer_Video"));

        // MediaPlayer와 연결
        if (MediaPlayer && MediaTexture)
        {
            MediaTexture->SetMediaPlayer(MediaPlayer);
            MediaTexture->UpdateResource();
            UE_LOG(LogTemp, Log, TEXT("  ✅ MediaTexture linked to MediaPlayer"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("  ❌ MediaTexture asset not found"));
    }

    // ✅ MediaSource 에셋 로드
    //static ConstructorHelpers::FObjectFinder<UMediaSource> MediaSourceAsset(
    //    TEXT("/Game/GolfGameBluePrint/SwingAnalyzer/TerraStreamMediaSource")
    //);
    //if (MediaSourceAsset.Succeeded())
    //{
    //    WebcamSource = MediaSourceAsset.Object;
    //    UE_LOG(LogTemp, Log, TEXT("  ✅ MediaSource loaded from asset: TerraStreamMediaSource"));
    //}
    //else
    //{
    //    UE_LOG(LogTemp, Warning, TEXT("  ⚠️ MediaSource asset not found"));
    //}

    WebcamSource = nullptr;
    UE_LOG(LogTemp, Log, TEXT("  ✅ WebcamSource initialized for dynamic creation"));

    static ConstructorHelpers::FObjectFinder<UMaterial> MediaTextureMaterialAsset(
        TEXT("/Game/GolfGameBluePrint/SwingAnalyzer/M_MediaTexture.M_MediaTexture")
    );
    if (MediaTextureMaterialAsset.Succeeded())
    {
        MediaTextureMaterial = MediaTextureMaterialAsset.Object;
        UE_LOG(LogTemp, Log, TEXT("  ✅ MediaTextureMaterial loaded: M_MediaTexture"));
    }

    // RenderTarget 생성
    CaptureRenderTarget = CreateDefaultSubobject<UTextureRenderTarget2D>(TEXT("CaptureRenderTarget"));
    if (CaptureRenderTarget)
    {
        CaptureRenderTarget->InitAutoFormat(640, 480);
        CaptureRenderTarget->ClearColor = FLinearColor::Black;
        CaptureRenderTarget->UpdateResourceImmediate(true);
        UE_LOG(LogTemp, Log, TEXT("  ✅ CaptureRenderTarget created (640x480)"));
    }

    //static ConstructorHelpers::FClassFinder<USwingVideoWidget> WidgetBPClass(
    //    TEXT("/Game/GolfGameBluePrint/SwingAnalyzer/WBP_SwingAnalyzer")
    //);
    //if (WidgetBPClass.Succeeded())
    //{
    //    VideoWidgetClass = WidgetBPClass.Class;
    //}

    VideoWidgetClass = nullptr;

    UE_LOG(LogTemp, Log, TEXT("📹 WebcamCapture Constructor Complete"));
}

AWebcamCapture::~AWebcamCapture()
{
    UE_LOG(LogTemp, Warning, TEXT("🔴 WebcamCapture Destructor"));
}

void AWebcamCapture::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════════════════════════"));
    UE_LOG(LogTemp, Warning, TEXT("🔴 AWebcamCapture::EndPlay - Starting cleanup"));
    UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════════════════════════"));

    // ═══════════════════════════════════════════════════════════════════════════
    // Step 0: 상태 플래그 설정 (즉시 모든 작업 중지)
    // ═══════════════════════════════════════════════════════════════════════════
    bIsCapturing = false;
    bIsPausingCapture = false;
    bDummySwingInProgress = false;
    bShotPending = false;

    UE_LOG(LogTemp, Log, TEXT("✅ Step 0: Capture flags stopped"));

    // 1. 모든 렌더링 명령이 완료될 때까지 엔진을 강제로 대기시킴 (Fence)
    FlushRenderingCommands(); // 렌더링 스레드 작업 완료 보장

    // ═══════════════════════════════════════════════════════════════════════════
    // Step 1: 타이머 정리 (한 번에 모두 - 중복 제거)
    // ═══════════════════════════════════════════════════════════════════════════
    UE_LOG(LogTemp, Log, TEXT("  → Clearing all timers..."));

    if (GetWorld())
    {
        FTimerManager& TimerManager = GetWorld()->GetTimerManager();

        // 명시적 타이머 정리
        TimerManager.ClearTimer(InitTimerHandle);
        TimerManager.ClearTimer(CaptureTimerHandle);
        TimerManager.ClearTimer(PlayCheckTimerHandle);
        TimerManager.ClearTimer(RetryTimerHandle);
        TimerManager.ClearTimer(TrackFormatTimerHandle);
        TimerManager.ClearTimer(PlayStartTimerHandle);
        TimerManager.ClearTimer(PauseCaptureTimerHandle);        // ✅ 이전 누락
        TimerManager.ClearTimer(ResumeCaptureTimerHandle);       // ✅ 이전 누락
        TimerManager.ClearTimer(PendingShotTimerHandle);         // ✅ 이전 누락
        TimerManager.ClearTimer(PlayClipTimerHandle);            // ✅ 이전 누락

        // 안전장치: 남은 모든 타이머 정리
        TimerManager.ClearAllTimersForObject(this);

        UE_LOG(LogTemp, Log, TEXT("    ✅ All timers cleared"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("    ⚠️ World is null, skipping timer cleanup"));
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Step 2: 비동기 작업 대기 (저장 중단 방지)
    // ═══════════════════════════════════════════════════════════════════════════
    UE_LOG(LogTemp, Log, TEXT("  → Waiting for async tasks..."));

    if (AsyncSaveTask.IsValid() && !AsyncSaveTask.IsReady())
    {
        UE_LOG(LogTemp, Warning, TEXT("    ⏳ Waiting for async save to complete..."));
        double StartTime = FPlatformTime::Seconds();
        AsyncSaveTask.Wait();
        double WaitDuration = FPlatformTime::Seconds() - StartTime;
        UE_LOG(LogTemp, Warning, TEXT("    ✅ Async save completed (waited %.2f seconds)"), WaitDuration);
        bIsSavingAsync = false;
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("    ✅ No async tasks pending"));
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Step 3: MediaPlayer 이벤트 바인딩 해제
    // ═══════════════════════════════════════════════════════════════════════════
    UE_LOG(LogTemp, Log, TEXT("  → Unbinding MediaPlayer events..."));

    if (MediaPlayer && IsValid(MediaPlayer))
    {
        MediaPlayer->OnMediaOpened.RemoveAll(this);
        MediaPlayer->OnMediaOpenFailed.RemoveAll(this);
        MediaPlayer->OnPlaybackSuspended.RemoveAll(this);

        if (MediaPlayer->IsPlaying())
        {
            MediaPlayer->Close();
            UE_LOG(LogTemp, Log, TEXT("    ✅ MediaPlayer closed"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("    ⚠️ MediaPlayer invalid, skipping"));
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Step 4: Application 이벤트 바인딩 해제
    // ═══════════════════════════════════════════════════════════════════════════
    UE_LOG(LogTemp, Log, TEXT("  → Unbinding application events..."));

    FCoreDelegates::ApplicationWillDeactivateDelegate.RemoveAll(this);
    FCoreDelegates::ApplicationHasReactivatedDelegate.RemoveAll(this);
    UE_LOG(LogTemp, Log, TEXT("    ✅ Application events unbound"));

    // ═══════════════════════════════════════════════════════════════════════════
    // Step 5: 텍스처 풀 정리
    // ═══════════════════════════════════════════════════════════════════════════
    UE_LOG(LogTemp, Log, TEXT("  → Clearing texture pool..."));

    int32 TextureCount = TexturePool.Num();
    for (int32 i = 0; i < TexturePool.Num(); i++)
    {
        if (UTexture2D* Tex = TexturePool[i])
        {
            Tex->ConditionalBeginDestroy();
        }
    }
    TexturePool.Empty();
    CurrentPoolIndex = 0;

    // ✅ 캐시된 프레임 텍스처 정리
    if (CachedFrameTexture && IsValid(CachedFrameTexture))
    {
        CachedFrameTexture->ConditionalBeginDestroy();
        CachedFrameTexture = nullptr;
    }

    UE_LOG(LogTemp, Log, TEXT("    ✅ Texture pool cleared (%d textures)"), TextureCount);

    // ═══════════════════════════════════════════════════════════════════════════
    // Step 6: 머티리얼 풀 정리 (NEW!)
    // ═══════════════════════════════════════════════════════════════════════════
    UE_LOG(LogTemp, Log, TEXT("  → Clearing material pool..."));

    int32 MaterialCount = MaterialPool.Num();
    for (int32 i = 0; i < MaterialPool.Num(); i++)
    {
        UMaterialInstanceDynamic* Mat = MaterialPool[i];
        if (Mat && IsValid(Mat))
        {
            // DynamicMaterial은 명시적 MarkPendingKill 필요 없음
            // 참조만 해제
            MaterialPool[i] = nullptr;
        }
    }
    MaterialPool.Empty();
    MaterialPoolIndex = 0;

    UE_LOG(LogTemp, Log, TEXT("    ✅ Material pool cleared (%d materials)"), MaterialCount);

    // ═══════════════════════════════════════════════════════════════════════════
    // Step 7: 캐시된 동적 머티리얼 정리
    // ═══════════════════════════════════════════════════════════════════════════
    UE_LOG(LogTemp, Log, TEXT("  → Clearing cached dynamic material..."));

    if (CachedDynamicMaterial && IsValid(CachedDynamicMaterial))
    {
        CachedDynamicMaterial = nullptr;
    }

    UE_LOG(LogTemp, Log, TEXT("    ✅ Cached dynamic material cleared"));

    // ═══════════════════════════════════════════════════════════════════════════
    // Step 8: VideoBufferComponent 정리
    // ═══════════════════════════════════════════════════════════════════════════
    UE_LOG(LogTemp, Log, TEXT("  → Clearing video buffer..."));

    if (VideoBufferComponent && IsValid(VideoBufferComponent))
    {
        VideoBufferComponent->ClearBuffer();
        UE_LOG(LogTemp, Log, TEXT("    ✅ VideoBufferComponent cleared"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("    ⚠️ VideoBufferComponent invalid, skipping"));
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Step 9: 녹음된 샷 프레임 정리
    // ═══════════════════════════════════════════════════════════════════════════
    UE_LOG(LogTemp, Log, TEXT("  → Clearing last recorded shot..."));

    int32 RecordedFrameCount = LastRecordedShot.Num();
    LastRecordedShot.Empty();

    UE_LOG(LogTemp, Log, TEXT("    ✅ LastRecordedShot cleared (%d frames)"), RecordedFrameCount);

    // ═══════════════════════════════════════════════════════════════════════════
    // Step 10: MediaTexture 업데이트
    // ═══════════════════════════════════════════════════════════════════════════
    UE_LOG(LogTemp, Log, TEXT("  → Updating MediaTexture..."));

    if (MediaTexture && IsValid(MediaTexture))
    {
        MediaTexture->UpdateResource();
        UE_LOG(LogTemp, Log, TEXT("    ✅ MediaTexture resource updated"));
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Step 11: VideoWidget 정리 (중요! - 크래시 방지)
    // ═══════════════════════════════════════════════════════════════════════════
    UE_LOG(LogTemp, Log, TEXT("  → Cleaning up VideoWidget..."));

    if (VideoWidget && IsValid(VideoWidget))
    {
        // VideoDisplay 먼저 정리
        if (VideoWidget->VideoDisplay && IsValid(VideoWidget->VideoDisplay))
        {
            FSlateBrush EmptyBrush;
            VideoWidget->VideoDisplay->SetBrush(EmptyBrush);
            UE_LOG(LogTemp, Log, TEXT("    ✅ VideoDisplay brush cleared"));
        }

        // WebcamCapture 참조 해제
        VideoWidget->WebcamCaptureRef.Reset();
        UE_LOG(LogTemp, Log, TEXT("    ✅ WebcamCaptureRef cleared from VideoWidget"));

        // 재생 상태 초기화
        VideoWidget->bIsPlaying = false;
        VideoWidget->bIsPaused = false;
        int32 FrameCount = VideoWidget->SwingFrames.Num();
        VideoWidget->SwingFrames.Empty();
        UE_LOG(LogTemp, Log, TEXT("    ✅ VideoWidget playback state reset (%d frames cleared)"), FrameCount);

        // Visibility 설정 및 참조 해제
        VideoWidget->SetVisibility(ESlateVisibility::Collapsed);
        VideoWidget = nullptr;

        UE_LOG(LogTemp, Warning, TEXT("    ✅ VideoWidget cleanup completed"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("    ⚠️ VideoWidget invalid or null, skipping"));
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Step 12: SwingClips 폴더 정리
    // ═══════════════════════════════════════════════════════════════════════════
    UE_LOG(LogTemp, Log, TEXT("  → Cleaning up SwingClips folder..."));

    CleanupSwingClipsFolder();
    UE_LOG(LogTemp, Log, TEXT("    ✅ SwingClips folder cleanup completed"));

    // ═══════════════════════════════════════════════════════════════════════════
    // Step 13: 상태 변수 최종 초기화
    // ═══════════════════════════════════════════════════════════════════════════
    UE_LOG(LogTemp, Log, TEXT("  → Finalizing state variables..."));

    RetryCount = 0;
    FrameCounter = 0;
    LastCleanupFrame = 0;
    bIsInBackground = false;
    FrameSkipCounter = 0;
    bInitWebcamInProgress = false;
    bWebcamOpened = false;
    bSwingClipsDeleted = false;
    bDummySwingInProgress = false;
    DummySwingStartTime = 0.0f;
    CaptureStartTime = -1.0f;
    bCaptureStartTimeInitialized = false;

    UE_LOG(LogTemp, Log, TEXT("    ✅ State variables finalized"));

    // ═══════════════════════════════════════════════════════════════════════════
    // Step 14: 렌더링 명령 플러시 및 가비지 컬렉션
    // ═══════════════════════════════════════════════════════════════════════════
    UE_LOG(LogTemp, Log, TEXT("  → Flushing render commands and garbage collection..."));

    FlushRenderingCommands();
    UE_LOG(LogTemp, Log, TEXT("    ✅ Render commands flushed"));

    //GEngine->ForceGarbageCollection(true);
    //UE_LOG(LogTemp, Log, TEXT("    ✅ Force garbage collection completed"));

    // ═══════════════════════════════════════════════════════════════════════════
    // Step 15: 부모 클래스 정리
    // ═══════════════════════════════════════════════════════════════════════════
    Super::EndPlay(EndPlayReason);

    UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════════════════════════"));
    UE_LOG(LogTemp, Warning, TEXT("✅ AWebcamCapture::EndPlay - Cleanup complete"));
    UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════════════════════════"));
}

void AWebcamCapture::BeginPlay()
{
    Super::BeginPlay();


    // ✅ VideoWidgetClass 지연 로드 (CDO 경고 방지)
    if (!VideoWidgetClass)
    {
        VideoWidgetClass = LoadClass<USwingVideoWidget>(
            nullptr,
            TEXT("/Game/GolfGameBluePrint/SwingAnalyzer/WBP_SwingAnalyzer.WBP_SwingAnalyzer_C")
            );
        if (VideoWidgetClass)
        {
            UE_LOG(LogTemp, Log, TEXT("✅ VideoWidgetClass 지연 로드 성공: %s"),
                *VideoWidgetClass->GetName());
        }
        else
        {
            UE_LOG(LogTemp, Warning,
                TEXT("⚠️ VideoWidgetClass 지연 로드 실패 — 경로 확인 필요: WBP_SwingAnalyzer"));
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // ✅ 타임싱크 초기화 (게임 재시작/레벨 리로드 시에도 정상 동작)
    // ═══════════════════════════════════════════════════════════════════════════
    CaptureStartTime = -1.0f;
    CurrentCaptureTime = 0.0f;
    bCaptureStartTimeInitialized = false;
    UE_LOG(LogTemp, Warning, TEXT("✅ Timesync variables initialized"));

    UE_LOG(LogTemp, Warning, TEXT(""));
    UE_LOG(LogTemp, Warning, TEXT("🎬 ===== WebcamCapture BeginPlay ====="));

    // ✅ 텍스처 풀 초기화 (한 번만!)
    if (TexturePool.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("🔧 Initializing texture pool (%d textures)..."), TexturePoolSize);

        for (int32 i = 0; i < TexturePoolSize; i++)
        {
            UTexture2D* Tex = UTexture2D::CreateTransient(640, 480, PF_B8G8R8A8);
            if (Tex)
            {
                TexturePool.Add(Tex);
              //  UE_LOG(LogTemp, Log, TEXT("   ✅ Texture %d created: %p"), i, Tex);
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("   ❌ Failed to create texture %d"), i);
            }
        }

        UE_LOG(LogTemp, Warning, TEXT("✅ Texture pool initialized (total: %d)"), TexturePool.Num());
    }


    if (!MediaPlayer || !MediaTexture || !VideoBufferComponent || !CaptureRenderTarget)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ [CRITICAL] Component validation failed!"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("📹 BeginPlay - Checking components..."));

    // MediaPlayer
    if (!MediaPlayer)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Creating MediaPlayer..."));
        MediaPlayer = NewObject<UMediaPlayer>(this, UMediaPlayer::StaticClass());
    }

    // MediaTexture
    if (!MediaTexture)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Creating MediaTexture..."));
        MediaTexture = NewObject<UMediaTexture>(this, UMediaTexture::StaticClass());
        if (MediaPlayer)
        {
            MediaTexture->SetMediaPlayer(MediaPlayer);
            MediaTexture->UpdateResource();
        }
    }

    // MediaSource 체크 및 재로드
    if (!WebcamSource)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ WebcamSource is NULL, dynamically creating UFileMediaSource..."));

        // UFileMediaSource를 NewObject로 생성하여 vidcap:// URL을 설정할 수 있도록 합니다.
        WebcamSource = NewObject<class UFileMediaSource>(this, TEXT("DynamicWebcamSource"));

        if (WebcamSource)
        {
            UE_LOG(LogTemp, Warning, TEXT("✅ Dynamic UFileMediaSource object created."));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("❌ Dynamic UFileMediaSource creation FAILED"));
        }
    }

    // CaptureRenderTarget
    if (!CaptureRenderTarget)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Creating CaptureRenderTarget..."));
        CaptureRenderTarget = NewObject<UTextureRenderTarget2D>(this, UTextureRenderTarget2D::StaticClass());
        CaptureRenderTarget->InitAutoFormat(640, 480);
        CaptureRenderTarget->ClearColor = FLinearColor::Black;
        CaptureRenderTarget->UpdateResourceImmediate(true);
    }

    // ✅ MediaPlayer 이벤트 바인딩 (중복 방지)
    if (MediaPlayer)
    {
        // 기존 바인딩 제거 (중복 방지)
        MediaPlayer->OnMediaOpened.RemoveAll(this);
        MediaPlayer->OnMediaOpenFailed.RemoveAll(this);
        MediaPlayer->OnPlaybackSuspended.RemoveAll(this);

        // 새로 바인딩
        MediaPlayer->OnMediaOpened.AddDynamic(this, &AWebcamCapture::OnMediaOpened);
        MediaPlayer->OnMediaOpenFailed.AddDynamic(this, &AWebcamCapture::OnMediaOpenFailed);
        MediaPlayer->OnPlaybackSuspended.AddDynamic(this, &AWebcamCapture::OnPlaybackSuspended);

        UE_LOG(LogTemp, Log, TEXT("📹 MediaPlayer events bound"));

        // ✅ Application Focus 이벤트 바인딩 (포커스 손실 시 크래시 방지)
        FCoreDelegates::ApplicationWillDeactivateDelegate.AddUObject(this, &AWebcamCapture::OnApplicationWillDeactivate);
        FCoreDelegates::ApplicationHasReactivatedDelegate.AddUObject(this, &AWebcamCapture::OnApplicationHasReactivated);
        UE_LOG(LogTemp, Log, TEXT("📹 Application focus events bound"));
    }


    if (!MediaTextureMaterial)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ MediaTextureMaterial is not set!"));
        UE_LOG(LogTemp, Error, TEXT("   Please assign a Material in BP_WebcamCapture blueprint"));
    }
    else if (!IsValid(MediaTextureMaterial))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ MediaTextureMaterial is not valid!"));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("✅ MediaTextureMaterial is set: %s"),
            *MediaTextureMaterial->GetName());
    }


    // 나머지 초기화
    GetWorldTimerManager().SetTimer(
        InitTimerHandle,
        this,
        &AWebcamCapture::DelayedInitialization,
        0.5f,
        false
    );

    if (bAutoLoadConfig)
    {
        if (ConfigAsset)
        {
            ApplySettings(ConfigAsset->Settings);
        }
        else
        {
            LoadConfig(ConfigFilePath);
        }
    }


}

void AWebcamCapture::DelayedInitialization()
{
    InitWebcam();

    if (CurrentSettings.bAutoStart)
    {
        if (AutoConnectFirstWebcam())
        {
            // StartCapture();

            UE_LOG(LogTemp, Warning, TEXT("[4/5] MediaPlayer status AFTER StartCapture:"));
            UE_LOG(LogTemp, Warning, TEXT("      IsPlaying: %s"), MediaPlayer->IsPlaying() ? TEXT("YES") : TEXT("NO"));
            UE_LOG(LogTemp, Warning, TEXT("      IsPreparing: %s"), MediaPlayer->IsPreparing() ? TEXT("YES") : TEXT("NO"));
            UE_LOG(LogTemp, Warning, TEXT("      HasError: %s"), MediaPlayer->HasError() ? TEXT("YES") : TEXT("NO"));

            // 📍 Step 5: 타이머 설정 확인
            UE_LOG(LogTemp, Warning, TEXT("[5/5] Timer configuration:"));
            UE_LOG(LogTemp, Warning, TEXT("      bIsCapturing: %s"), bIsCapturing ? TEXT("✅ TRUE") : TEXT("❌ FALSE"));
            UE_LOG(LogTemp, Warning, TEXT("      Capture FPS: 30"));
            UE_LOG(LogTemp, Warning, TEXT("      Frame Interval: 33.3ms"));

            UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════════════"));
            UE_LOG(LogTemp, Warning, TEXT("✅ WebcamCapture::BeginPlay COMPLETE"));
            UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════════════"));

        }
    }

    // UI 위젯 생성
    if (VideoWidgetClass)
    {
        CreateVideoWidget();
    }
}

void AWebcamCapture::Tick(float DeltaTime)
{
    SCOPE_CYCLE_COUNTER(STAT_WebcamTick);

    Super::Tick(DeltaTime);

    // ✅ 캡처가 일시 중지된 경우 스킵
    if (!CurrentSettings.bEnableVideoSaving || !bIsCapturing)
    {
        //UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════════════"));
        //UE_LOG(LogTemp, Warning, TEXT("✅ WebcamCapture::Tick --- Capture Fail"));
        //UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════════════"));
        return;
    }

    // ✅ 정확한 타임싱크를 위해 GetWorld()->GetTimeSeconds() 사용
    if (bIsCapturing)
    {
        // 방법 1: DeltaTime 누적 (기존 - 부정확함)
        // CurrentCaptureTime += DeltaTime;

        // 방법 2: 절대 시간 사용 (정확함) ✅
        // ✅ FIXED: Static을 멤버 변수로 변경하여 게임 재시작 시에도 정상 동작

        if (CaptureStartTime < 0.0f)
        {
            // 첫 캡처 시작 시간 기록
            CaptureStartTime = GetWorld()->GetTimeSeconds();
            CurrentCaptureTime = 0.0f;
            bCaptureStartTimeInitialized = true;
            UE_LOG(LogTemp, Warning, TEXT("✅ Capture timing initialized at world time: %.3f"), CaptureStartTime);
        }
        else
        {
            // 절대 시간에서 시작 시간을 빼서 캡처 시간 계산
            float WorldTime = GetWorld()->GetTimeSeconds();
            CurrentCaptureTime = WorldTime - CaptureStartTime;

            // ═══════════════════════════════════════════════════════════════════════════
            // ✅ 추가: 타임스탐프 검증 로깅 (10프레임마다)
            // ═══════════════════════════════════════════════════════════════════════════
            //if (FrameCounter % 10 == 0)
            //{
            //    UE_LOG(LogTemp, Log, TEXT("📹 Tick - Frame %d:"), FrameCounter);
            //    UE_LOG(LogTemp, Log, TEXT("   World Time: %.3f"), WorldTime);
            //    UE_LOG(LogTemp, Log, TEXT("   CaptureStartTime: %.3f"), CaptureStartTime);
            //    UE_LOG(LogTemp, Log, TEXT("   CurrentCaptureTime: %.3f"), CurrentCaptureTime);

            //    // ⚠️ 타임스탐프가 음수이면 경고
            //    if (CurrentCaptureTime < 0.0f)
            //    {
            //        UE_LOG(LogTemp, Error, TEXT("❌ CRITICAL: Negative CurrentCaptureTime (%.3f)!"), CurrentCaptureTime);
            //        UE_LOG(LogTemp, Error, TEXT("   WorldTime (%.3f) < CaptureStartTime (%.3f)"), WorldTime, CaptureStartTime);
            //    }
            //}
        }
        SCOPE_CYCLE_COUNTER(STAT_WebcamCaptureFrame);
        CaptureFrame();  // ⭐ Tick에서 호출
    }
    SCOPE_CYCLE_COUNTER(STAT_WebcamDummySwing);
    ProcessDummySwing(DeltaTime);
}


void AWebcamCapture::InitWebcam()
{
    UE_LOG(LogTemp, Error, TEXT("🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴"));
    UE_LOG(LogTemp, Error, TEXT("🔴 IMPROVED InitWebcam() - PC Independent"));
    UE_LOG(LogTemp, Error, TEXT("🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴"));

    if (bInitWebcamInProgress)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ InitWebcam already in progress - skip"));
        return;
    }
    bInitWebcamInProgress = true;



    if (!MediaPlayer)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ MediaPlayer is null"));
        return;
    }

    // 이전 연결 정리
    if (MediaPlayer->IsPlaying() || MediaPlayer->IsPreparing())
    {
        MediaPlayer->Close();
        UE_LOG(LogTemp, Log, TEXT("🔄 Previous connection closed"));
    }

    // AutoConnectFirstWebcam이 여러 방법을 시도함
    if (AutoConnectFirstWebcam())
    {
        UE_LOG(LogTemp, Warning, TEXT("✅ 웹캠 연결 성공!"));

        // 트랙/포맷 설정은 OnMediaOpened에서 처리
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ 웹캠 연결 실패"));
    }

    // 현재 (웹캠 URL 설정 코드)
    if (WebcamSource)
    {
        FString WebcamURL = CurrentSettings.WebcamURL;
        // WebcamURL이 vidcap://...이어야 함!

        if (WebcamURL.IsEmpty())
        {
            UE_LOG(LogTemp, Error, TEXT("❌ WebcamURL is empty!"));
            // 기본값 설정?
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("📹 WebcamURL: %s"), *WebcamURL);
        }
    }

    bInitWebcamInProgress = false;
}


bool AWebcamCapture::AutoConnectFirstWebcam()
{
    UE_LOG(LogTemp, Warning, TEXT("🔌 Attempting to connect to webcam"));

    if (!MediaPlayer) return false;

    if (MediaPlayer->IsPlaying() || MediaPlayer->IsPreparing())
    {
        MediaPlayer->Close();
    }

    // [1/4] MediaSource 에셋 (에디터에서 고정 설정한 경우)
    if (WebcamSource)
    {
        UE_LOG(LogTemp, Warning, TEXT("🎯 [1/4] Trying MediaSource Asset..."));
        if (MediaPlayer->OpenSource(WebcamSource))
        {
            MediaPlayer->Play();
            UE_LOG(LogTemp, Warning, TEXT("✅ Connected via MediaSource!"));
            return true;
        }
    }

    // ✅ [2/4] VID/PID로 검색 (가장 신뢰할 수 있음, PC 독립적)
    if (!CurrentSettings.WebcamVID.IsEmpty() && !CurrentSettings.WebcamPID.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("🔍 [2/4] Searching by VID/PID: %s/%s"),
            *CurrentSettings.WebcamVID, *CurrentSettings.WebcamPID);

        if (FindAndConnectByVIDPID(CurrentSettings.WebcamVID, CurrentSettings.WebcamPID, CurrentSettings.WebcamIndex))
        {
            UE_LOG(LogTemp, Warning, TEXT("✅ Connected via VID/PID!"));
            // 성공한 URL을 JSON에 저장 (다음 참고용)
            SaveConfig();
            return true;
        }
        UE_LOG(LogTemp, Warning, TEXT("   ⚠️ VID/PID failed"));
    }

    // ✅ [3/4] 이름으로 검색
    if (!CurrentSettings.WebcamName.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("📝 [3/4] Searching by name: %s"),
            *CurrentSettings.WebcamName);

        if (FindWebcamByName(CurrentSettings.WebcamName))
        {
            UE_LOG(LogTemp, Warning, TEXT("✅ Connected via Name!"));
            SaveConfig();
            return true;
        }
        UE_LOG(LogTemp, Warning, TEXT("   ⚠️ Name search failed"));
    }

    // [4/4] 최후의 수단: 저장된 URL (구 PC에서 동일 장치일 때만 운 좋으면 됨)
    if (!CurrentSettings.WebcamURL.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("🔗 [4/4] Trying saved URL (last resort)..."));
        if (MediaPlayer->OpenUrl(CurrentSettings.WebcamURL))
        {
            MediaPlayer->Play();
            UE_LOG(LogTemp, Warning, TEXT("✅ Connected via saved URL!"));
            return true;
        }
        UE_LOG(LogTemp, Warning, TEXT("   ⚠️ Saved URL failed"));
    }

    UE_LOG(LogTemp, Error, TEXT("❌ ALL CONNECTION METHODS FAILED"));
    return false;

}


bool AWebcamCapture::OpenWebcamByDisplayName(const FString& TargetName)
{
    /* ⚠️ UE 4.26에서는 MediaCaptureSupport::EnumerateVideoCaptureDevices()가
       제대로 작동하지 않을 수 있습니다.

       대신 다음 방법을 사용하세요:
       1. 에디터에서 MediaSource 에셋을 만들고 vidcap:// URL을 직접 입력
       2. Config 파일에 WebcamURL을 직접 저장

    if (!MediaPlayer)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ MediaPlayer is null"));
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("🔍 Searching for webcam: %s"), *TargetName);

    // MediaCaptureSupport 사용하여 사용 가능한 웹캠 목록 가져오기
    TArray<FMediaCaptureDevice> Devices;
    MediaCaptureSupport::EnumerateVideoCaptureDevices(Devices);

    UE_LOG(LogTemp, Log, TEXT("📹 Found %d video capture devices:"), Devices.Num());

    for (const FMediaCaptureDevice& Device : Devices)
    {
        UE_LOG(LogTemp, Log, TEXT("  - %s (URL: %s)"), *Device.DisplayName.ToString(), *Device.Url);

        // Display Name으로 매칭
        if (Device.DisplayName.ToString().Contains(TargetName))
        {
            UE_LOG(LogTemp, Warning, TEXT("✅ Found target webcam: %s"), *Device.DisplayName.ToString());
            UE_LOG(LogTemp, Log, TEXT("   Using URL: %s"), *Device.Url);

            // 동적으로 생성된 URL로 연결
            if (MediaPlayer->OpenUrl(Device.Url))
            {
                MediaPlayer->Play();
                UE_LOG(LogTemp, Warning, TEXT("✅ Successfully connected via Display Name"));

                // 성공한 URL을 설정에 저장
                CurrentSettings.WebcamURL = Device.Url;
                SaveConfig();

                return true;
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("❌ Failed to open URL: %s"), *Device.Url);
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("⚠️ Target webcam '%s' not found"), *TargetName);

    // 부분 매칭 시도 (예: "1080P" 검색)
    UE_LOG(LogTemp, Log, TEXT("🔍 Trying partial match..."));
    for (const FMediaCaptureDevice& Device : Devices)
    {
        FString DeviceName = Device.DisplayName.ToString();
        if (DeviceName.Contains(TEXT("1080P")) || DeviceName.Contains(TEXT("1080p")))
        {
            UE_LOG(LogTemp, Warning, TEXT("✅ Found 1080P webcam: %s"), *DeviceName);

            if (MediaPlayer->OpenUrl(Device.Url))
            {
                MediaPlayer->Play();
                UE_LOG(LogTemp, Warning, TEXT("✅ Connected via partial match"));

                // 성공한 URL을 설정에 저장
                CurrentSettings.WebcamURL = Device.Url;
                SaveConfig();

                return true;
            }
        }
    }
    */

    UE_LOG(LogTemp, Error, TEXT("⚠️ OpenWebcamByDisplayName is disabled in UE 4.26"));
    UE_LOG(LogTemp, Error, TEXT("   Use MediaSource asset with vidcap:// URL instead"));
    return false;
}


bool AWebcamCapture::SelectBest1080p60Format()
{
    if (!MediaPlayer || !MediaPlayer->IsReady())
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ MediaPlayer not ready"));
        return false;
    }

    int32 NumTracks = MediaPlayer->GetNumTracks(EMediaPlayerTrack::Video);
    UE_LOG(LogTemp, Log, TEXT("📊 비디오 트랙: %d개"), NumTracks);

    if (NumTracks <= TrackIndex)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Track %d not available (Total: %d)"), TrackIndex, NumTracks);
        return false;
    }

    // 트랙 선택
    if (!MediaPlayer->SelectTrack(EMediaPlayerTrack::Video, TrackIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to select track %d"), TrackIndex);
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("  트랙 %d 선택"), TrackIndex);

    int32 NumFormats = MediaPlayer->GetNumTrackFormats(EMediaPlayerTrack::Video, TrackIndex);
    UE_LOG(LogTemp, Log, TEXT("  사용 가능한 포맷 수: %d"), NumFormats);

    // 포맷 목록 출력
    //for (int32 i = 0; i < NumFormats; i++)
    //{
    //    FString FormatStr = MediaPlayer->GetTrackFormat(EMediaPlayerTrack::Video, TrackIndex).ToString();
    //    UE_LOG(LogTemp, Log, TEXT("     Format[%d]: %s"), i, *FormatStr);

    //    // 1920x1080, 60fps 포맷 찾기
    //    if (FormatStr.Contains(TEXT("1920x1080")) && FormatStr.Contains(TEXT("60")))
    //    {
    //        if (MediaPlayer->SetTrackFormat(EMediaPlayerTrack::Video, TrackIndex, i))
    //        {
    //            UE_LOG(LogTemp, Warning, TEXT("✅ Found and set 1080p60: Format %d"), i);
    //            FormatIndex = i;  // 설정 저장

    //            // 포맷 설정 후 재생 재시작
    //            GetWorld()->GetTimerManager().SetTimer(
    //                PlayStartTimerHandle,
    //                [this]()
    //                {
    //                    if (MediaPlayer && !MediaPlayer->IsPlaying())
    //                    {
    //                        MediaPlayer->Play();
    //                        UE_LOG(LogTemp, Warning, TEXT("🔄 Playback restarted after format change"));
    //                    }
    //                },
    //                0.3f,
    //                    false
    //                    );

    //            return true;
    //        }
    //    }
    //}

    // 1080p60을 찾지 못하면 FormatIndex 사용
    if (NumFormats > FormatIndex)
    {
        if (MediaPlayer->SetTrackFormat(EMediaPlayerTrack::Video, TrackIndex, FormatIndex))
        {
            UE_LOG(LogTemp, Warning, TEXT("✅ Using configured format %d"), FormatIndex);

            // 포맷 설정 후 재생 재시작
            GetWorld()->GetTimerManager().SetTimer(
                PlayStartTimerHandle,
                [this]()
                {
                    if (MediaPlayer && !MediaPlayer->IsPlaying())
                    {
                        MediaPlayer->Play();
                        UE_LOG(LogTemp, Warning, TEXT("🔄 Playback restarted"));
                    }
                },
                0.3f,
                    false
                    );

            return true;
        }
    }

    UE_LOG(LogTemp, Error, TEXT("❌ Failed to set any format"));
    return false;
}

void AWebcamCapture::SelectTrackAndFormat()
{
    if (!MediaPlayer || !MediaPlayer->IsReady())
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ MediaPlayer not ready, retrying..."));
        GetWorld()->GetTimerManager().SetTimer(
            TrackFormatTimerHandle,
            this,
            &AWebcamCapture::SelectTrackAndFormat,
            0.1f,
            false
        );
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("========================================"));
    UE_LOG(LogTemp, Warning, TEXT("📹 Selecting Video Track & Format"));
    UE_LOG(LogTemp, Warning, TEXT("========================================"));

    int32 NumTracks = MediaPlayer->GetNumTracks(EMediaPlayerTrack::Video);
    UE_LOG(LogTemp, Log, TEXT("📊 Total video tracks: %d"), NumTracks);

    if (NumTracks == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ No video tracks available!"));
        return;
    }

    // ✅ 모든 트랙 정보 출력
    LogVideoTrackInfo();

    // ✅ STEP 1: 설정된 TrackIndex 확인
    if (TrackIndex >= NumTracks)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ TrackIndex %d is out of range (max: %d), using Track 0"),
            TrackIndex, NumTracks - 1);
        TrackIndex = 0;
    }

    // ✅ STEP 2: MJPEG 640x480@60fps 포맷 자동 검색 시도
    int32 FoundTrackIndex = -1;
    int32 FoundFormatIndex = -1;

    if (FindYUY2_640x480_30fps(FoundTrackIndex, FoundFormatIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("✅ Found MJPEG 640x480@60fps!"));
        UE_LOG(LogTemp, Warning, TEXT("   Track: %d, Format: %d"), FoundTrackIndex, FoundFormatIndex);

        TrackIndex = FoundTrackIndex;
        FormatIndex = FoundFormatIndex;

        // 트랙 선택
        if (MediaPlayer->SelectTrack(EMediaPlayerTrack::Video, TrackIndex))
        {
            UE_LOG(LogTemp, Log, TEXT("  ✅ Selected Track %d"), TrackIndex);

            // 포맷 설정
            if (MediaPlayer->SetTrackFormat(EMediaPlayerTrack::Video, TrackIndex, FormatIndex))
            {
                UE_LOG(LogTemp, Warning, TEXT("  ✅ Set Format %d (MJPEG 640x480@60fps)"), FormatIndex);

                // 🔥 포맷 설정 후 재생 재시작
                GetWorld()->GetTimerManager().SetTimer(
                    PlayStartTimerHandle,
                    [this]()
                    {
                        if (MediaPlayer && !MediaPlayer->IsPlaying())
                        {
                            MediaPlayer->Play();
                            UE_LOG(LogTemp, Warning, TEXT("🔄 Playback restarted with MJPEG format"));
                        }
                    },
                    0.2f,
                        false
                        );

                UE_LOG(LogTemp, Warning, TEXT("========================================"));
                return;
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("❌ Failed to set format %d"), FormatIndex);
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("❌ Failed to select track %d"), TrackIndex);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ MJPEG 640x480@60fps not found, using configured FormatIndex"));
    }

    // ✅ STEP 3: 자동 검색 실패 시 설정된 FormatIndex 사용
    if (NumTracks > TrackIndex)
    {
        if (MediaPlayer->SelectTrack(EMediaPlayerTrack::Video, TrackIndex))
        {
            UE_LOG(LogTemp, Log, TEXT("  ✅ Selected Track %d"), TrackIndex);

            int32 NumFormats = MediaPlayer->GetNumTrackFormats(EMediaPlayerTrack::Video, TrackIndex);
            UE_LOG(LogTemp, Log, TEXT("  📊 Available formats: %d"), NumFormats);

            if (NumFormats > FormatIndex)
            {
                if (MediaPlayer->SetTrackFormat(EMediaPlayerTrack::Video, TrackIndex, FormatIndex))
                {
                    UE_LOG(LogTemp, Warning, TEXT("  ✅ Set Track %d, Format %d"), TrackIndex, FormatIndex);

                    // 🔥 포맷 설정 후 재생 재시작
                    GetWorld()->GetTimerManager().SetTimer(
                        PlayStartTimerHandle,
                        [this]()
                        {
                            if (MediaPlayer && !MediaPlayer->IsPlaying())
                            {
                                MediaPlayer->Play();
                                UE_LOG(LogTemp, Warning, TEXT("🔄 Playback restarted"));
                            }
                        },
                        0.2f,
                            false
                            );
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("❌ Failed to set format %d"), FormatIndex);
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("⚠️ Format %d not available (max: %d)"), FormatIndex, NumFormats - 1);
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("❌ Failed to select track %d"), TrackIndex);
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("========================================"));
}

UFileMediaSource* AWebcamCapture::CreateMediaSourceFromURL(const FString& URL)
{
    UFileMediaSource* FileSource = NewObject<UFileMediaSource>(this);
    if (FileSource)
    {
        FileSource->FilePath = URL;
    }
    return FileSource;
}

// ========== ✅ 수정된 함수: StartCapture (자동 감지 제거) ==========

void AWebcamCapture::StartCapture()
{
    UE_LOG(LogTemp, Log, TEXT("🎥 ======================= START   capturing!"));


    if (bIsCapturing)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Already capturing!"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("🎥 StartCapture() called"));

    // ═══════════════════════════════════════════════════════════════════════════
    // ✅ 핵심 수정: CaptureStartTime 리셋 (타임스탐프 음수 문제 해결!)
    // ═══════════════════════════════════════════════════════════════════════════
    CaptureStartTime = GetWorld()->GetTimeSeconds();
    CurrentCaptureTime = 0.0f;
    bCaptureStartTimeInitialized = true;
    FrameCounter = 0;

    UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════"));
    UE_LOG(LogTemp, Warning, TEXT("🎬 Capture Timing Initialized"));
    UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════"));
    UE_LOG(LogTemp, Warning, TEXT("   World Time: %.3f"), GetWorld()->GetTimeSeconds());
    UE_LOG(LogTemp, Warning, TEXT("   CaptureStartTime: %.3f"), CaptureStartTime);
    UE_LOG(LogTemp, Warning, TEXT("   CurrentCaptureTime: %.3f"), CurrentCaptureTime);
    UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════"));

    // ✅ Step 1: 버퍼 완전 초기화
    if (VideoBufferComponent)
    {
        VideoBufferComponent->ClearBuffer();
        VideoBufferComponent->ToggleVideoSaving(true);
        UE_LOG(LogTemp, Log, TEXT("✅ VideoBuffer cleared and enabled"));
    }

    // ✅ Step 2: 모든 타이머 초기화
    LastCleanupFrame = 0;

    // ✅ Step 2-1: 캡처 시작 시간 초기화 (위에서 수정함!)
    UE_LOG(LogTemp, Warning, TEXT("✅ Capture timing synchronized"));

    // ✅ Step 3: 이전 녹화 데이터 초기화
   // LastRecordedShot.Empty();
    UE_LOG(LogTemp, Log, TEXT("✅ LastRecordedShot cleared"));

    // TexturePool 초기화
    TexturePool.Empty();
    for (int32 i = 0; i < PoolSize; ++i)
    {
        UTexture2D* Tex = UTexture2D::CreateTransient(640, 480, PF_B8G8R8A8);
        TexturePool.Add(Tex);
    }

    // ✅ Step 5: UI 업데이트
    if (VideoWidget)
    {
        VideoWidget->SwitchToLiveFeed();
        UE_LOG(LogTemp, Log, TEXT("✅ VideoWidget switched to LiveFeed"));
    }

    // ✅ Step 6: 캡처 시작
    bIsCapturing = true;

    // 이벤트 핸들러 등록
    if (MediaPlayer && !MediaPlayer->OnMediaOpened.IsBound())
    {
        MediaPlayer->OnMediaOpened.AddDynamic(this, &AWebcamCapture::OnMediaOpened);
    }

    if (MediaPlayer && !MediaPlayer->OnMediaOpenFailed.IsBound())
    {
        MediaPlayer->OnMediaOpenFailed.AddDynamic(this, &AWebcamCapture::OnMediaOpenFailed);
    }

    // ✅ 실시간 프레임 캡처 타이머 시작
    //if (GetWorld())
    //{
    //    float CaptureInterval = 1.0f / static_cast<float>(CurrentSettings.FPS);

    //    GetWorld()->GetTimerManager().SetTimer(
    //        CaptureTimerHandle,
    //        this,
    //        &AWebcamCapture::CaptureFrame,
    //        CaptureInterval,  // ✅ 60fps: 0.0167초
    //        true
    //    );

    //    UE_LOG(LogTemp, Log, TEXT("  Capture timer started (30fps)"));
    //}

    StartDummySwingProcess();

    UE_LOG(LogTemp, Warning, TEXT("✅ StartCapture() complete - ready to capture"));
}


// ========== ✅ 수정된 함수: StopCapture ==========

void AWebcamCapture::StopCapture()
{
    SafeStopCapture();
}

void AWebcamCapture::SafeStopCapture()
{
    UE_LOG(LogTemp, Log, TEXT("🎥 ======================= Stop   capturing!"));
    // Step 1: 캡처 정지
    bIsCapturing = false;
    bIsPausingCapture = false;

    // Step 1: VideoBuffer 정리
    if (VideoBufferComponent)
    {
        VideoBufferComponent->ClearBuffer();
        VideoBufferComponent->ToggleVideoSaving(false);
    }

    // Step 2: 타이머 정리
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(InitTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(CaptureTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(PlayCheckTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(RetryTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(TrackFormatTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(PlayStartTimerHandle);

        // ⭐ 추가: 캡처 제어 타이머 정리
        GetWorld()->GetTimerManager().ClearTimer(PauseCaptureTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(ResumeCaptureTimerHandle);

    }

    // Step 3: 녹화 데이터 정리
    LastRecordedShot.Empty();

    // Step 4: 시간 초기화
    CurrentCaptureTime = 0.0f;
    FrameCounter = 0;

    // ═══════════════════════════════════════════════════════════════════════════
    // ✅ 추가: 타임싱크 초기화 (다음 캡처 재시작을 위해)
    // ═══════════════════════════════════════════════════════════════════════════
    CaptureStartTime = -1.0f;  // 다음 캡처 시작 시 재초기화될 수 있도록
    bCaptureStartTimeInitialized = false;
    UE_LOG(LogTemp, Warning, TEXT("✅ Timesync reset for next capture session"));

    UE_LOG(LogTemp, Warning, TEXT("✅ SafeCleanup() complete"));
}
// ========== ✅ 새로운 함수: TriggerShotRecording (수동 트리거) ==========

void AWebcamCapture::TriggerShotRecording()
{
    if (!IsValidForOperation())
    {
        UE_LOG(LogTemp, Error, TEXT("📹 Cannot trigger shot - invalid state"));
        return;
    }

    if (!bIsCapturing)
    {
        UE_LOG(LogTemp, Warning, TEXT("📹 Cannot trigger shot - capture not started"));
        return;
    }

    // ✅ 추가: 버퍼 상태 확인
    int32 BufferedFrames = VideoBufferComponent->GetBufferedFrameCount();
    int32 RequiredFrames = FMath::CeilToInt(30.0f * (PreShotBufferTime + PostShotBufferTime));

    if (BufferedFrames < RequiredFrames)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("⚠️ Warning: Not enough buffered frames (Buffered: %d/%d, Expected: %.1fs)"),
            BufferedFrames, RequiredFrames,
            PreShotBufferTime + PostShotBufferTime);
    }

    UE_LOG(LogTemp, Warning,
        TEXT("🎬 ========== Shot Recording Triggered =========="));
    UE_LOG(LogTemp, Warning,
        TEXT("   Current Time: %.2f seconds"),
        CurrentCaptureTime);
    UE_LOG(LogTemp, Warning,
        TEXT("   Pre-shot buffer: %.1f seconds | Post-shot buffer: %.1f seconds"),
        PreShotBufferTime, PostShotBufferTime);
    UE_LOG(LogTemp, Warning,
        TEXT("   Buffered frames: %d (%.1f seconds)"),
        BufferedFrames, BufferedFrames / 30.0f);
    UE_LOG(LogTemp, Warning,
        TEXT("==============================================="));

    // 이미 예약된 샷이 있으면 중복 방지
    if (bShotPending)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Shot already pending, ignored"));
        return;
    }

    bShotPending = true;
    PendingShotTime = CurrentCaptureTime;

    // ✅ Timer 지연 계산
    float TimerDelay = PostShotBufferTime + 0.05f;  // 약간의 여유

    UE_LOG(LogTemp, Warning, TEXT("🎬 Shot pending at %.2f, will extract after %.2fs"),
        PendingShotTime, PostShotBufferTime);
    UE_LOG(LogTemp, Warning, TEXT("   Timer delay: %.2f (PostBuffer: %.2f + 0.05s margin)"),
        TimerDelay, PostShotBufferTime);
    UE_LOG(LogTemp, Warning, TEXT("   Will extract at time: %.2f"),
        PendingShotTime + TimerDelay);
    UE_LOG(LogTemp, Warning, TEXT("   Extract range: %.2f ~ %.2f"),
        PendingShotTime - PreShotBufferTime, PendingShotTime + PostShotBufferTime);

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            PendingShotTimerHandle,
            this,
            &AWebcamCapture::ProcessPendingShotRecording,
            TimerDelay,  // ✅ PostShotBufferTime이 이제 4.0f
            false
        );
    }
}

void AWebcamCapture::ProcessPendingShotRecording()
{
    bShotPending = false;

    UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════"));
    UE_LOG(LogTemp, Warning, TEXT("🎯 Processing pending shot recording"));
    UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════"));
    UE_LOG(LogTemp, Warning, TEXT("   Shot Time: %.2f"), PendingShotTime);
    UE_LOG(LogTemp, Warning, TEXT("   Pre-buffer: %.2f"), PreShotBufferTime);
    UE_LOG(LogTemp, Warning, TEXT("   Post-buffer: %.2f"), PostShotBufferTime);
    UE_LOG(LogTemp, Warning, TEXT("   Expected Range: %.2f ~ %.2f"),
        PendingShotTime - PreShotBufferTime,
        PendingShotTime + PostShotBufferTime);

    //PauseCapture();

    TArray<FVideoFrame> SwingFrames = ExtractSwingFrames(PendingShotTime);

    if (SwingFrames.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ No swing frames extracted"));
        UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════"));
        ResumeCapture();
        return;
    }

    LastRecordedShot = SwingFrames;

    UE_LOG(LogTemp, Warning, TEXT("✅ Extracted %d frames"), SwingFrames.Num());

    // ✅ 프레임 수 검증
    if (SwingFrames.Num() < 140)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ WARNING: Frames less than expected (%.0f expected)"), 150.0f);
    }
    else if (SwingFrames.Num() >= 150)
    {
        UE_LOG(LogTemp, Warning, TEXT("✅ EXCELLENT: Full frames captured!"));
    }

    FString ClipPath = SaveSwingClipToDisk(SwingFrames, PendingShotTime);

    if (ClipPath.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to save clip to disk"));
        UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════"));
        ResumeCapture();
        return;
    }

    if (OnSwingDetected.IsBound())
    {
        OnSwingDetected.Broadcast(SwingFrames);
    }

    // ========== ✅ 수정: 7초로 증가 ==========
    // ❌ 기존: PlaySwingClipFromPath(ClipPath, 3.0f);
    PlaySwingClipFromPath(ClipPath, 2.0f);  // ✅ 7초 (저장 완료 대기)

    UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════"));

    //GetWorldTimerManager().SetTimer(
    //    ResumeCaptureTimerHandle,
    //    this,
    //    &AWebcamCapture::ResumeCapture,
    //    ResumeCaptureDuration,
    //    false
    //);
}


void AWebcamCapture::TriggerShotRecordingAtTime(float ShotTime)
{
    if (!IsValidForOperation())
    {
        UE_LOG(LogTemp, Error, TEXT("📹 Cannot trigger shot - invalid state"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("🎬 Shot triggered at specified time: %.2f seconds"), ShotTime);
    ProcessShotRecording(ShotTime);
}

// ========== ✅ 내부 함수: ProcessShotRecording ==========

void AWebcamCapture::ProcessShotRecording(float ShotTime)
{
    // ✅ 중복 호출 방지 가드 추가
    if (bShotPending)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ ProcessShotRecording: Shot already pending, ignored"));
        return;
    }
    bShotPending = true;
    UE_LOG(LogTemp, Warning, TEXT("🎯 Processing shot recording at time %.2f"), ShotTime);

    // ========== Step 1: 캡처 일시 중지 ==========
    PauseCapture();

    // ========== Step 2: 스윙 프레임 추출 ==========
    TArray<FVideoFrame> SwingFrames = ExtractSwingFrames(ShotTime);

    if (SwingFrames.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ No swing frames extracted"));
        ResumeCapture();
        return;
    }

    // ✅ LastRecordedShot 저장 (메모리에도 보관, 옵션)
    LastRecordedShot = SwingFrames;

    UE_LOG(LogTemp, Warning, TEXT("✅ Extracted %d frames"), SwingFrames.Num());

    // ========== Step 3: 디스크에 JPG 시퀀스 저장 ==========
    FString ClipPath = SaveSwingClipToDisk(SwingFrames, ShotTime);

    if (ClipPath.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to save clip to disk"));
        ResumeCapture();
        return;
    }

    // ========== Step 4: 델리게이트 브로드캐스트 (옵션) ==========
    if (OnSwingDetected.IsBound())
    {
        OnSwingDetected.Broadcast(SwingFrames);
    }

    // ========== Step 5: 위젯에 재생 요청 (3초 후) ==========
    PlaySwingClipFromPath(ClipPath, 2.0f);

    // ========== Step 6: 캡처 재개 예약 ==========
    // (위젯 재생이 끝나면 자동 재개하도록 이벤트 바인딩 가능)
    //GetWorldTimerManager().SetTimer(
    //    ResumeCaptureTimerHandle,
    //    this,
    //    &AWebcamCapture::ResumeCapture,
    //    ResumeCaptureDuration,
    //    false
    //);

    bShotPending = false;  // 처리 완료 후 해제
}

TArray<FVideoFrame> AWebcamCapture::ExtractSwingFrames(float ShotTime)
{
    SCOPE_CYCLE_COUNTER(STAT_WebcamExtractFrames);
    TArray<FVideoFrame> SwingFrames;

    if (!VideoBufferComponent)
    {
        return SwingFrames;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // ✅ 개선: 동적 Pre-buffer 조정 로직
    // ═══════════════════════════════════════════════════════════════════════════

    // Step 1: 버퍼의 실제 시간 범위 파악
    float MinBufferTime = 0.0f;  // 첫 유효 프레임의 시간
    float MaxBufferTime = 0.0f;  // 마지막 유효 프레임의 시간

    // 버퍼에서 최소/최대 타임스탐프 찾기
    if (VideoBufferComponent && VideoBufferComponent->GetMaxBufferSize() > 0)
    {
        // 버퍼 내 유효한 프레임들의 시간 범위 계산
        TArray<FVideoFrame> AllFrames = VideoBufferComponent->GetAllFrames();

        if (AllFrames.Num() > 0)
        {
            MinBufferTime = AllFrames[0].Timestamp;
            MaxBufferTime = AllFrames[AllFrames.Num() - 1].Timestamp;
        }
    }

    // Step 2: 요청된 범위 계산
    float RequestedStartTime = ShotTime - PreShotBufferTime;
    float EndTime = ShotTime + PostShotBufferTime;

    // Step 3: 동적 Pre-buffer 조정 (핵심!)
    float AdjustedStartTime = RequestedStartTime;
    float DynamicPreBuffer = PreShotBufferTime;

    if (RequestedStartTime < MinBufferTime)
    {
        // Pre-buffer를 줄여야 함
        AdjustedStartTime = MinBufferTime;
        DynamicPreBuffer = ShotTime - MinBufferTime;

        UE_LOG(LogTemp, Warning, TEXT("⚠️ Pre-buffer adjusted (buffer too short):"));
        UE_LOG(LogTemp, Warning, TEXT("   Original Pre-buffer: %.2f"), PreShotBufferTime);
        UE_LOG(LogTemp, Warning, TEXT("   Adjusted Pre-buffer: %.2f"), DynamicPreBuffer);
        UE_LOG(LogTemp, Warning, TEXT("   Reason: Buffer starts at %.2f (requested %.2f)"),
            MinBufferTime, RequestedStartTime);
    }

    // ✅ 버퍼 상태 진단
    int32 TotalFrames = VideoBufferComponent->GetMaxBufferSize();
    float BufferDuration = MaxBufferTime - MinBufferTime;

    UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════"));
    UE_LOG(LogTemp, Warning, TEXT("📊 ExtractSwingFrames Diagnostics"));
    UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════"));
    UE_LOG(LogTemp, Warning, TEXT("   Total Buffer Frames: %d"), TotalFrames);
    UE_LOG(LogTemp, Warning, TEXT("   Buffer Time Range: %.2f ~ %.2f (%.2fs)"),
        MinBufferTime, MaxBufferTime, BufferDuration);
    UE_LOG(LogTemp, Warning, TEXT("   Shot Time: %.2f"), ShotTime);
    UE_LOG(LogTemp, Warning, TEXT("   Pre-buffer: %.2f → %.2f"),
        PreShotBufferTime, DynamicPreBuffer);
    UE_LOG(LogTemp, Warning, TEXT("   Post-buffer: %.2f"), PostShotBufferTime);
    UE_LOG(LogTemp, Warning, TEXT("   Requested Range: %.2f ~ %.2f"), RequestedStartTime, EndTime);
    UE_LOG(LogTemp, Warning, TEXT("   Adjusted Range:  %.2f ~ %.2f"), AdjustedStartTime, EndTime);
    UE_LOG(LogTemp, Warning, TEXT("   Expected Frames: %.0f"),
        (EndTime - AdjustedStartTime) * 30.0f);

    if (TotalFrames == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ VideoBuffer is EMPTY!"));
        UE_LOG(LogTemp, Error, TEXT("   Capture might not be running!"));
        UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════"));
        return SwingFrames;
    }

    // Step 4: 조정된 범위로 프레임 추출
    SwingFrames = VideoBufferComponent->GetFramesInRange(AdjustedStartTime, EndTime);

    UE_LOG(LogTemp, Warning, TEXT("   ✅ Extracted Frames: %d"), SwingFrames.Num());
    UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════"));

    // ✅ 추가: 프레임이 없으면 전체 버퍼 사용 (비상 모드)
    if (SwingFrames.Num() == 0 && TotalFrames > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ No frames in adjusted range, using ALL buffer frames!"));
        SwingFrames = VideoBufferComponent->GetAllFrames();

        UE_LOG(LogTemp, Warning, TEXT("   Emergency extraction: %d frames"), SwingFrames.Num());
    }

    return SwingFrames;
}

void AWebcamCapture::PlayLastRecordedShot()
{
    //if (LastRecordedShot.Num() == 0)
    //{
    //    UE_LOG(LogTemp, Warning, TEXT("📹 No recorded shot to play"));
    //    return;
    //}

    //if (VideoWidget)
    //{

    //   VideoWidget->SetSwingFrames(LastRecordedShot);
    //    VideoWidget->SwitchToSwingPlayback();
    //    VideoWidget->SetVisibility(ESlateVisibility::Visible);
    //    VideoWidget->PlaySwingVideo();
    //    UE_LOG(LogTemp, Log, TEXT("📹 Playing last recorded shot"));
    //}


        // ✅ 재생 시작할 때 캡처 Pause
    //PauseCapture();

    //// ✅ 재생 완료 시 Resume을 위해 델리게이트 바인딩
    //if (VideoWidget)
    //{
    //    // 기존 바인딩 해제 후 재연결
    //    VideoWidget->OnClipPlaybackFinished.RemoveAll(this);
    //    VideoWidget->OnClipPlaybackFinished.AddDynamic(this, &AWebcamCapture::ResumeCapture);
    //}



     // ========== VideoWidget 유효성 검사 ==========
    if (!VideoWidget || !IsValid(VideoWidget))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ VideoWidget is not valid"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════"));
    UE_LOG(LogTemp, Warning, TEXT("🎬 Playing Last Recorded Shot"));
    UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════"));

    // ========== ✅ 방법 1: JPG 파일 기반 재생 (권장) ==========
    // 최신 클립 디렉토리에서 직접 로드
    if (!LastSavedClipPath.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("📁 Playback Method: JPG File Based"));
        UE_LOG(LogTemp, Log, TEXT("   Path: %s"), *LastSavedClipPath);
        UE_LOG(LogTemp, Log, TEXT("   Benefits:"));
        UE_LOG(LogTemp, Log, TEXT("   ✅ Memory efficient"));
        UE_LOG(LogTemp, Log, TEXT("   ✅ Persistent storage"));
        UE_LOG(LogTemp, Log, TEXT("   ✅ Latest data guaranteed"));

        // JPG 파일 디렉토리에서 클립 재생
        VideoWidget->PlaySwingClipFromDirectory(LastSavedClipPath);
        VideoWidget->SetVisibility(ESlateVisibility::Visible);

        UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════"));
        return;
    }


    // ========== ✅ 방법 2: 메모리 버퍼 재생 (폴백) ==========
    // LastSavedClipPath가 없을 때 메모리 버퍼에서 재생
    if (LastRecordedShot.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ No recorded shot in memory"));
        UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("📌 Playback Method: Memory Buffer (Fallback)"));
    UE_LOG(LogTemp, Log, TEXT("   Frames: %d"), LastRecordedShot.Num());
    UE_LOG(LogTemp, Log, TEXT("   Reason: JPG file path not available"));

    // 메모리 버퍼에서 재생
    VideoWidget->SetSwingFrames(LastRecordedShot);
    VideoWidget->SwitchToSwingPlayback();
    VideoWidget->SetVisibility(ESlateVisibility::Visible);
    VideoWidget->PlaySwingVideo();

    UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════"));

}

float AWebcamCapture::GetBufferedDuration() const
{
    return CurrentCaptureTime;
}

// ========== ❌ 자동 샷 감지 핸들러 제거 ==========
// void AWebcamCapture::OnShotDetectedHandler(float ShotTime)
// {
//     UE_LOG(LogTemp, Log, TEXT("📹 Auto shot detected at time: %.2f"), ShotTime);
//     ProcessShotRecording(ShotTime);
// }

// ========== ✅ 프레임 캡처 (분석 없음) ==========

void AWebcamCapture::CaptureFrame()
{
     // ✅ Step 1: 캡처 활성화 상태 확인
    if (!CurrentSettings.bEnableVideoSaving)
    {
        return;
    }

    if (!bIsCapturing)
    {
        return;
    }

    // ✅ Step 2: 타임싱크 관리
    if (CaptureStartTime < 0.0f)
    {
        CaptureStartTime = GetWorld()->GetTimeSeconds();
        CurrentCaptureTime = 0.0f;
        bCaptureStartTimeInitialized = true;
        UE_LOG(LogTemp, Warning, TEXT("✅ Capture started at world time: %.3f"), CaptureStartTime);
    }
    else
    {
        CurrentCaptureTime = GetWorld()->GetTimeSeconds() - CaptureStartTime;
    }

    FrameCounter++;

    // ✅ Step 3: MediaTexture 상태 확인 (상세 검증)
    if (!MediaTexture)
    {
        static int32 NoTextureCount = 0;
        if (++NoTextureCount % 30 == 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("⚠️ MediaTexture is null"));
        }
        return;
    }

    const int32 TexW = MediaTexture->GetWidth();
    const int32 TexH = MediaTexture->GetHeight();

    if (TexW <= 8 || TexH <= 8)
    {
        //static int32 SmallSizeCount = 0;
        //if (++SmallSizeCount % 30 == 0)
        //{
        //    UE_LOG(LogTemp, Warning, TEXT("⏳ MediaTexture too small: %dx%d"), TexW, TexH);
        //}
        return;
    }

    // ✅ Step 4: RenderTarget 리사이즈 (필요시에만)
    if (!CaptureRenderTarget)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ CaptureRenderTarget is null"));
        return;
    }

    if (CaptureRenderTarget->SizeX != TexW || CaptureRenderTarget->SizeY != TexH)
    {
        CaptureRenderTarget->ResizeTarget(TexW, TexH);
        UE_LOG(LogTemp, Log, TEXT("📐 RenderTarget resized to %dx%d"), TexW, TexH);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // ✅ Step 5: 프레임 캡처 (재시도 로직)
    // ✅ 중요: 변수명을 LocalRetryCount, LocalMaxRetries로 변경!
    // 이렇게 하면 클래스 멤버 RetryCount, MaxRetries와 충돌 없음
    // ═══════════════════════════════════════════════════════════════════════════

    UTexture2D* CurrentFrame = nullptr;
    int32 LocalRetryCount = 0;           // ✅ 변경: RetryCount → LocalRetryCount
    const int32 LocalMaxRetries = 3;     // ✅ 변경: MaxRetries → LocalMaxRetries

    while (!CurrentFrame && LocalRetryCount < LocalMaxRetries)
    {
        CurrentFrame = CaptureCurrentFrame();

        if (!CurrentFrame)
        {
            LocalRetryCount++;
            if (LocalRetryCount < LocalMaxRetries)
            {
                // 짧은 대기 후 재시도
                FPlatformProcess::Sleep(0.0005f);  // 0.5ms
            }
        }
    }

    if (!CurrentFrame)
    {
        //static int32 SkipCount = 0;
        //if (++SkipCount % 60 == 0)
        //{
        //    UE_LOG(LogTemp, Warning,
        //        TEXT("⚠️ Frame skip (total: %d, rate: %.1f%%)"),
        //        SkipCount, (SkipCount * 100.0f) / FrameCounter);
        //}
        return;
    }

    // ✅ Step 6: 프레임을 버퍼에 추가
    //if (VideoBufferComponent)
    //{
    //    VideoBufferComponent->AddFrame(CurrentFrame, CurrentCaptureTime);
    //}

    // ✅ Step 7: 주기적 정리 및 로깅
    //if (FrameCounter % 60 == 0)
    //{
    //    float WorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    //    float FPS = 1.0f / GetWorld()->DeltaTimeSeconds;

    //    UE_LOG(LogTemp, Log,
    //        TEXT("📊 Frame: %d | Time: %.3f | FPS: %.1f"),
    //        FrameCounter, CurrentCaptureTime, FPS);
    //}

    // ✅ 주기적 메모리 정리
  /*  const int32 CleanupIntervalFrames = FMath::Max(60, CurrentSettings.FPS * 10);
    if (FrameCounter - LastCleanupFrame >= CleanupIntervalFrames)
    {
        CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
        LastCleanupFrame = FrameCounter;

        UE_LOG(LogTemp, Log,
            TEXT("🧹 Memory cleanup at frame %d"),
            FrameCounter);
    }*/
}


// ========== 기존 프레임 캡처 함수들 (변경 없음) ==========

// ============================================================================
// 수정된 CaptureCurrentFrame() - Material 동기화 보장
// ============================================================================

UTexture2D* AWebcamCapture::CaptureCurrentFrame()
{ 
    SCOPE_CYCLE_COUNTER(STAT_WebcamCaptureCurrentFrame);
    // ========== Step 1: 포인터 검증 ==========
    if (!MediaPlayer || !MediaTexture || !CaptureRenderTarget)
    {
        return nullptr;
    }

    if (!MediaPlayer->IsPlaying())
    {
        return nullptr;
    }

    // ========== Step 2: MediaTexture 준비 상태 확인 ==========
    const int32 TexW = MediaTexture->GetWidth();
    const int32 TexH = MediaTexture->GetHeight();
    if (TexW <= 8 || TexH <= 8)
    {
        return nullptr;
    }

    // ========== Step 3: RenderTarget 리사이즈 ==========
    if (CaptureRenderTarget->SizeX != TexW || CaptureRenderTarget->SizeY != TexH)
    {
        CaptureRenderTarget->ResizeTarget(TexW, TexH);
    }

    // ========== ✅ Step 4: Material Pool 생성 (첫 한 번) ==========
    if (MaterialPool.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════"));
        UE_LOG(LogTemp, Warning, TEXT("🔧 Creating Material Pool"));

        // ❌ 기존: for (int32 i = 0; i < 3; i++)
        // ✅ 개선: 상수 사용
        for (int32 i = 0; i < POOL_SIZE; i++)
        {
            if (MediaTextureMaterial && IsValid(MediaTextureMaterial) && IsValid(MediaTexture))
            {
                UMaterialInstanceDynamic* MatInstance =
                    UMaterialInstanceDynamic::Create(MediaTextureMaterial, this);

                if (MatInstance)
                {
                    MatInstance->SetTextureParameterValue(FName("MediaTexture"), MediaTexture);
                    MaterialPool.Add(MatInstance);

                    UE_LOG(LogTemp, Log, TEXT("   Created pool material %d/%d"), i + 1, POOL_SIZE);
                }
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("❌ Cannot create material - invalid pointers"));
                return nullptr;
            }
        }

        UE_LOG(LogTemp, Warning, TEXT("✅ Material Pool created: %d materials"), MaterialPool.Num());
        UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════"));
    }

    // ========== ✅ Step 5: Material Pool에서 순환 선택 ==========
    UMaterialInstanceDynamic* ActiveMaterial = nullptr;

    if (MaterialPool.Num() > 0)
    {
        // ✅ 순환 방식: 0 → 1 → 2 → 0 → ...
        MaterialPoolIndex = (MaterialPoolIndex + 1) % MaterialPool.Num();
        ActiveMaterial = MaterialPool[MaterialPoolIndex];

        if (!IsValid(ActiveMaterial))
        {
            UE_LOG(LogTemp, Error, TEXT("❌ Material pool broken at index %d!"), MaterialPoolIndex);
            return nullptr;
        }

        // ✅ 로깅 (300번마다)
        static int32 MaterialReuseCount = 0;
        if (++MaterialReuseCount % 300 == 0)
        {
          //  UE_LOG(LogTemp, Warning, TEXT("📊 Material Pool reused: %d times (%.1f sec)"),
          //      MaterialReuseCount, MaterialReuseCount / 30.0f);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Material Pool is empty!"));
        return nullptr;
    }



    // 4. ✅ Material 생성 및 검증 (새로 추가!)
    //if (!CachedDynamicMaterial)
    //{
    //    // Material 생성 시도
    //    if (MediaTextureMaterial && IsValid(MediaTextureMaterial) && IsValid(MediaTexture))
    //    {
    //        CachedDynamicMaterial = UMaterialInstanceDynamic::Create(MediaTextureMaterial, this);
    //        if (!CachedDynamicMaterial)
    //        {
    //            UE_LOG(LogTemp, Error, TEXT("❌ Failed to create dynamic material"));
    //            return nullptr;  // ✅ Material 생성 실패 시 즉시 반환
    //        }

    //        // ✅ 새로 생성한 Material의 TextureParameter 설정
    //        CachedDynamicMaterial->SetTextureParameterValue(FName("MediaTexture"), MediaTexture);

    //        UE_LOG(LogTemp, Warning, TEXT("✅ Dynamic Material created and set"));
    //    }
    //    else
    //    {
    //        UE_LOG(LogTemp, Error, TEXT("❌ MediaTextureMaterial or MediaTexture invalid"));
    //        return nullptr;
    //    }
    //}

    //// 5. ✅ Material 유효성 재확인 (필수!)
    //if (!IsValid(CachedDynamicMaterial))
    //{
    //    UE_LOG(LogTemp, Warning, TEXT("⚠️ CachedDynamicMaterial is invalid, resetting"));
    //    CachedDynamicMaterial = nullptr;
    //    return nullptr;  // ✅ 유효하지 않으면 재생성하도록 next frame에서 시도
    //}

    // ========== ✅ Step 6: RenderTarget 렌더링 ==========
    // ⚠️ 중요: SetTextureParameterValue를 다시 호출하지 않음!
    // Material은 이미 생성 시점에 파라미터 설정됨
    if (!ActiveMaterial || !IsValid(ActiveMaterial))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ ActiveMaterial invalid"));
        MaterialPool.Empty(); // Pool 재생성 유도
        return nullptr;
    }
    SCOPE_CYCLE_COUNTER(STAT_WebcamDrawMaterial);
    UKismetRenderingLibrary::ClearRenderTarget2D(this, CaptureRenderTarget, FLinearColor::Black);

    // ✅ Material Pool에서 선택한 Material 사용
    UKismetRenderingLibrary::DrawMaterialToRenderTarget(this, CaptureRenderTarget, ActiveMaterial);

    SCOPE_CYCLE_COUNTER(STAT_WebcamCreateTexture);
    // ========== Step 7: Texture2D로 변환 ==========
    return CreateTexture2DFromPixels(CaptureRenderTarget);
}


// ✅ 추가: 텍스처 픽셀 유효성 검사
void AWebcamCapture::ValidateCapturedTexture(UTexture2D* Texture)
{
    if (!Texture || !IsValid(Texture))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ ValidateCapturedTexture: Texture is invalid"));
        return;
    }

    // PlatformData 접근
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION <= 26
    if (!Texture->PlatformData || Texture->PlatformData->Mips.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ PlatformData invalid"));
        return;
    }
    FTexture2DMipMap& Mip = Texture->PlatformData->Mips[0];
#else
    if (!Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ PlatformData invalid"));
        return;
    }
    FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
#endif

    // 픽셀 데이터 샘플링
    void* Data = Mip.BulkData.Lock(LOCK_READ_ONLY);
    if (!Data)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ BulkData.Lock failed"));
        return;
    }

    // 처음, 중간, 끝 픽셀 검사
    FColor* PixelData = (FColor*)Data;
    int32 TotalPixels = Texture->GetSizeX() * Texture->GetSizeY();

    if (TotalPixels > 0)
    {
        FColor FirstPixel = PixelData[0];
        FColor MiddlePixel = PixelData[TotalPixels / 2];
        FColor LastPixel = PixelData[TotalPixels - 1];

        // 모두 검은색(0,0,0)은 문제!
        if (FirstPixel.R == 0 && FirstPixel.G == 0 && FirstPixel.B == 0 &&
            MiddlePixel.R == 0 && MiddlePixel.G == 0 && MiddlePixel.B == 0 &&
            LastPixel.R == 0 && LastPixel.G == 0 && LastPixel.B == 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("⚠️ Texture is completely black - may not be rendered properly"));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("✅ Texture has valid pixel data"));
            UE_LOG(LogTemp, Log, TEXT("   First pixel: R=%d, G=%d, B=%d"),
                FirstPixel.R, FirstPixel.G, FirstPixel.B);
        }
    }

    Mip.BulkData.Unlock();
}


// ============================================================================
// 수정된 CreateTexture2DFromPixels() - 더 안전한 버전
// ============================================================================

UTexture2D* AWebcamCapture::CreateTexture2DFromPixels(UTextureRenderTarget2D* RenderTarget)
{
    // ========== Step 1: RenderTarget 검증 ==========
    if (!RenderTarget || !IsValid(RenderTarget))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ RenderTarget is NULL or invalid!"));
        return nullptr;
    }

    const int32 Width = RenderTarget->SizeX;
    const int32 Height = RenderTarget->SizeY;

    if (Width <= 0 || Height <= 0)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Invalid RenderTarget size: %dx%d"), Width, Height);
        return nullptr;
    }

    // ========== ✅ FIX: 매 프레임마다 새로운 Transient 텍스처 생성 ==========
    // 이전: TexturePool 순환으로 같은 메모리 주소 반복 → 파일 중첩
    // 개선: 각 프레임이 고유한 텍스처 주소를 가짐 → 올바른 저장

    UTexture2D* NewTexture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);

    if (!NewTexture)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ CreateTransient() failed!"));
        return nullptr;
    }

    NewTexture->SRGB = true;

    // ========== Step 2: RenderTarget → Texture2D 데이터 복사 ==========
    if (!UpdateTexture2DFromRenderTargetAsync(NewTexture, RenderTarget))
    {
       // UE_LOG(LogTemp, Error, TEXT("❌ Failed to update texture from render target"));
        return nullptr;
    }

    // ========== Step 3: 로깅 (매 100프레임마다) ==========
    static int32 CreatedTextureCount = 0;
    static int32 LastLoggedCount = 0;

    if (++CreatedTextureCount - LastLoggedCount >= 1000)
    {
        LastLoggedCount = CreatedTextureCount;
        //UE_LOG(LogTemp, Warning, TEXT("📊 TexturePool statistics:"));
        //UE_LOG(LogTemp, Warning, TEXT("   Created new textures: %d"), CreatedTextureCount);
        //UE_LOG(LogTemp, Warning, TEXT("   Size per frame: %.1f MB"),
        //    (Width * Height * 4) / (1024.0f * 1024.0f));
        //UE_LOG(LogTemp, Warning, TEXT("   Estimated total memory: %.1f MB"),
        //    (Width * Height * 4 * CreatedTextureCount) / (1024.0f * 1024.0f));
    }

    return NewTexture;  // ← 고유한 메모리 주소!
}


// ============================================================================
// ✅ 새로운 헬퍼 함수: RenderTarget → Texture2D 데이터 복사
// ============================================================================

bool AWebcamCapture::UpdateTexture2DFromRenderTarget(UTexture2D* TargetTexture, UTextureRenderTarget2D* RenderTarget)
{
    if (!TargetTexture || !RenderTarget)
    {
        return false;
    }

    // 1. RenderTarget의 리소스 가져오기
    FTextureRenderTargetResource* RTResource = RenderTarget->GameThread_GetRenderTargetResource();
    if (!RTResource)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ RTResource is NULL!"));
        return false;
    }

    // ❌ 기존: 매번 동적 할당
    // TArray<FColor> SurfaceData;
    // RTResource->ReadPixels(SurfaceData);

    // ✅ 개선: ReusableSurfaceData 재사용
    const int32 RequiredSize = RenderTarget->SizeX * RenderTarget->SizeY;

    // 크기가 다르면 재할당, 같으면 재사용
    if (ReusableSurfaceData.Num() != RequiredSize)
    {
        ReusableSurfaceData.SetNum(RequiredSize);
        UE_LOG(LogTemp, Log, TEXT("📊 SurfaceData resized: %d pixels (%.1f MB)"),
            RequiredSize, (RequiredSize * sizeof(FColor)) / (1024.0f * 1024.0f));
    }

    // 픽셀 데이터 읽기 (재사용 버퍼 사용!)
    RTResource->ReadPixels(ReusableSurfaceData);

    if (ReusableSurfaceData.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ SurfaceData is empty!"));
        return false;
    }

    // 3. 텍스처의 PlatformData 접근
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION <= 26
    if (!TargetTexture->PlatformData || TargetTexture->PlatformData->Mips.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ PlatformData is invalid!"));
        return false;
    }
    FTexture2DMipMap& Mip = TargetTexture->PlatformData->Mips[0];
#else
    if (!TargetTexture->GetPlatformData() || TargetTexture->GetPlatformData()->Mips.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ PlatformData is invalid!"));
        return false;
    }
    FTexture2DMipMap& Mip = TargetTexture->GetPlatformData()->Mips[0];
#endif

    // 4. BulkData 잠금 및 복사
    void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
    if (!Data)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ BulkData.Lock() failed!"));
        return false;
    }

    // 5. 메모리 복사 (ReusableSurfaceData 사용!)
    FMemory::Memcpy(Data, ReusableSurfaceData.GetData(),
        ReusableSurfaceData.Num() * sizeof(FColor));

    Mip.BulkData.Unlock();

    // 6. ✅ 리소스 업데이트 (GPU 동기화)
    TargetTexture->UpdateResource();

    return true;
}



UTexture2D* AWebcamCapture::GetCurrentFrameAsTexture2D()
{
    return CaptureCurrentFrame();
}

// ========== UI 함수들 (변경 없음) ==========

void AWebcamCapture::CreateVideoWidget()
{
    if (!VideoWidgetClass)
    {
        UE_LOG(LogTemp, Error, TEXT("📹 VideoWidgetClass not set"));
        return;
    }

    if (VideoWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("📹 VideoWidget already exists"));
        return;
    }

    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC)
    {
        UE_LOG(LogTemp, Error, TEXT("📹 PlayerController not found"));
        return;
    }

    VideoWidget = CreateWidget<USwingVideoWidget>(PC, VideoWidgetClass);
    if (VideoWidget)
    {
        VideoWidget->WebcamCaptureRef = this;
        VideoWidget->AddToViewport(3000);  // 50 → 999로 변경
        VideoWidget->SetVisibility(ESlateVisibility::Hidden);  // 가시성
        UE_LOG(LogTemp, Log, TEXT("📹 VideoWidget created successfully"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("📹 Failed to create VideoWidget"));
    }
}

void AWebcamCapture::ShowVideoWidget(bool bValue)
{
    if (VideoWidget)
    {
        if (bValue)
        {
            VideoWidget->SetVisibility(ESlateVisibility::Visible);  // 가시성
        }
        else
        {
            VideoWidget->SetVisibility(ESlateVisibility::Hidden);  // 가시성
        }


        UE_LOG(LogTemp, Log, TEXT("📹 VideoWidget shown"));
    }
}



void AWebcamCapture::HideVideoWidget()
{
    if (VideoWidget)
    {
        VideoWidget->RemoveFromParent();
        UE_LOG(LogTemp, Log, TEXT("📹 VideoWidget hidden"));
    }
}

// ========== 설정 관련 함수들 (변경 없음) ==========

bool AWebcamCapture::LoadConfig(const FString& FilePath)
{
    FString ConfigPath = FilePath.IsEmpty() ? UWebcamConfigLoader::GetDefaultConfigPath() : FilePath;

    if (!UWebcamConfigLoader::ConfigFileExists(ConfigPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("📹 Config file not found: %s"), *ConfigPath);

        if (bCreateDefaultConfigIfNotFound)
        {
            return CreateDefaultConfigFile();
        }
        return false;
    }

    if (UWebcamConfigLoader::LoadConfigFromJSON(ConfigPath, CurrentSettings))
    {
        ApplySettings(CurrentSettings);
        return true;
    }

    return false;
}

bool AWebcamCapture::SaveConfig(const FString& FilePath)
{
    // SwingcamConfig.json은 저장하지 않음 (읽기 전용)
    UE_LOG(LogTemp, Warning, TEXT("📹 SaveConfig is disabled. SwingcamConfig.json is read-only."));
    return false;
}

void AWebcamCapture::ApplySettings(const FWebcamSettings& Settings)
{
    // 기존 설정 적용 로직
    CurrentSettings = Settings;

    // ✅ 비디오 저장 설정 적용 (캡처 중지/시작)
    if (!CurrentSettings.bEnableVideoSaving && bIsCapturing)
    {
        StopCapture();
        UE_LOG(LogTemp, Warning, TEXT("⏸️ Video saving disabled - Stopping capture"));
    }
    else if (CurrentSettings.bEnableVideoSaving && !bIsCapturing)
    {
        StartCapture();
        UE_LOG(LogTemp, Log, TEXT("▶️ Video saving enabled - Starting capture"));
    }
}

bool AWebcamCapture::CreateDefaultConfigFile()
{
    // SwingcamConfig.json 자동 생성 비활성화 (읽기 전용 정책)
    UE_LOG(LogTemp, Warning, TEXT("📹 Config file not found. Using default settings in memory only (no file creation)."));
    CurrentSettings = UWebcamConfigLoader::GetDefaultSettings();
    ApplySettings(CurrentSettings);
    return false;
}

bool AWebcamCapture::IsValidForOperation() const
{
    bool bValid = true;

    if (!MediaPlayer)
    {
        UE_LOG(LogTemp, Error, TEXT("  ❌ Validation failed: MediaPlayer is NULL"));
        bValid = false;
    }

    if (!MediaTexture)
    {
        UE_LOG(LogTemp, Error, TEXT("  ❌ Validation failed: MediaTexture is NULL"));
        bValid = false;
    }

    if (!VideoBufferComponent)
    {
        UE_LOG(LogTemp, Error, TEXT("  ❌ Validation failed: VideoBufferComponent is NULL"));
        bValid = false;
    }

    if (!CaptureRenderTarget)
    {
        UE_LOG(LogTemp, Error, TEXT("  ❌ Validation failed: CaptureRenderTarget is NULL"));
        bValid = false;
    }

    if (bValid)
    {
        UE_LOG(LogTemp, Log, TEXT("  ✅ All components valid"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("  ❌ Component validation FAILED"));
    }

    return bValid;
}

void AWebcamCapture::SafeCleanup()
{

    // ✅ TexturePool 정리
    // ✅ Material Pool 정리
    if (MaterialPool.Num() > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("   Cleaning up Material Pool (%d materials)..."), MaterialPool.Num());

        for (int32 i = 0; i < MaterialPool.Num(); i++)
        {
            UMaterialInstanceDynamic* Mat = MaterialPool[i];
            if (Mat && IsValid(Mat))
            {
                // ✅ Material 명시적 정리
                Mat = nullptr;
            }
        }

        MaterialPool.Empty();
        MaterialPoolIndex = 0;

        UE_LOG(LogTemp, Log, TEXT("   ✅ Material Pool cleaned"));
    }


    SafeStopCapture();
}


void AWebcamCapture::TestWebcamConnection()
{
    UE_LOG(LogTemp, Warning, TEXT("🧪 ========== Testing Webcam Connection =========="));

    if (!MediaPlayer)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ MediaPlayer is NULL"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("MediaPlayer Status:"));
    UE_LOG(LogTemp, Log, TEXT("  Is Playing: %s"), MediaPlayer->IsPlaying() ? TEXT("✅ YES") : TEXT("❌ NO"));
    UE_LOG(LogTemp, Log, TEXT("  Is Preparing: %s"), MediaPlayer->IsPreparing() ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogTemp, Log, TEXT("  Has Error: %s"), MediaPlayer->HasError() ? TEXT("❌ YES") : TEXT("✅ NO"));
    UE_LOG(LogTemp, Log, TEXT("  Current URL: %s"), *MediaPlayer->GetUrl());

    if (MediaPlayer->HasError())
    {
        UE_LOG(LogTemp, Error, TEXT("  ⚠️ MediaPlayer has errors - check Output Log"));
    }

    if (MediaTexture)
    {
        UE_LOG(LogTemp, Log, TEXT("MediaTexture:"));
        UE_LOG(LogTemp, Log, TEXT("  Size: %dx%d"), MediaTexture->GetWidth(), MediaTexture->GetHeight());
    }

    // 수동으로 다시 열어보기
    UE_LOG(LogTemp, Log, TEXT(""));
    UE_LOG(LogTemp, Log, TEXT("Attempting to open webcam..."));

    TArray<FString> UrlsToTest = {
        TEXT("wmf://0"),
        TEXT("wmf://1"),
        TEXT("wmf://2")
    };

    for (const FString& Url : UrlsToTest)
    {
        UE_LOG(LogTemp, Log, TEXT("  Testing: %s"), *Url);

        if (MediaPlayer->OpenUrl(Url))
        {
            UE_LOG(LogTemp, Warning, TEXT("  ✅ SUCCESS with %s"), *Url);

            // 재생 시도
            if (!MediaPlayer->IsPlaying())
            {
                MediaPlayer->Play();
            }

            UE_LOG(LogTemp, Warning, TEXT("  Update WebcamURL to '%s' for permanent fix"), *Url);
            break;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("  ❌ Failed with %s"), *Url);
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("🧪 ========================================"));
}

bool AWebcamCapture::CheckVideoWidgetStatus() const
{
    // ✅ 클래스가 설정되어 있는지 확인
    if (!VideoWidgetClass)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ VideoWidgetClass not set"));
        return false;
    }

    // ✅ 인스턴스가 생성되었는지 확인
    if (!VideoWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ VideoWidget instance is null"));
        return false;
    }

    // ✅ Viewport에 등록되어 있는지 확인
    if (!VideoWidget->IsInViewport())
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ VideoWidget exists but not in viewport"));
        return false;
    }

    // ✅ 가시성 상태 확인
    if (VideoWidget->GetVisibility() != ESlateVisibility::Visible)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ VideoWidget not visible (Current state: %d)"),
            static_cast<int32>(VideoWidget->GetVisibility()));
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("✅ VideoWidget is visible and active on screen"));
    return true;
}

// ✅ 새로운 이벤트 핸들러들
void AWebcamCapture::OnMediaOpened(FString OpenedUrl)
{
    UE_LOG(LogTemp, Warning, TEXT("✅ MediaPlayer Opened: %s"), *OpenedUrl);

    bWebcamOpened = true;
    bInitWebcamInProgress = false;

    if (!MediaPlayer->IsPlaying())
        MediaPlayer->Play();


    // ✅ 유효성 검사 추가
    if (!IsValidForOperation())
    {
        UE_LOG(LogTemp, Error, TEXT("❌ OnMediaOpened called but object is not valid"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("✅ MediaPlayer Opened: %s"), *OpenedUrl);

    // ✅ World 체크만 수행 (IsValid() 제거)
    if (GetWorld())
    {
        GetWorldTimerManager().SetTimer(
            TrackFormatTimerHandle,
            [this]()
            {
                // ✅ 람다 내부에서도 유효성 검사
                if (this && IsValidForOperation())
                {
                    SelectBest1080p60Format();
                }
            },
            0.1f,
                false
                );

        // 재생 확인 타이머
        GetWorldTimerManager().SetTimer(
            PlayCheckTimerHandle,
            this,
            &AWebcamCapture::CheckPlaybackStatus,
            1.0f,
            false
        );
    }
}

void AWebcamCapture::OnMediaOpenFailed(FString FailedUrl)
{
    // ✅ 유효성 검사 추가 (IsValid() 제거)
    if (!GetWorld())
    {
        UE_LOG(LogTemp, Error, TEXT("❌ OnMediaOpenFailed: World invalid"));
        return;
    }

    UE_LOG(LogTemp, Error, TEXT("❌ MediaPlayer Open Failed: %s"), *FailedUrl);

    // 재시도 로직
    RetryCount++;
    if (RetryCount < MaxRetries)
    {
        UE_LOG(LogTemp, Warning, TEXT("🔄 Retrying connection... (%d/%d)"), RetryCount, MaxRetries);
        GetWorldTimerManager().SetTimer(
            RetryTimerHandle,
            this,
            &AWebcamCapture::InitWebcam,
            2.0f,
            false
        );
    }
}

void AWebcamCapture::OnPlaybackSuspended()
{
    UE_LOG(LogTemp, Warning, TEXT("⏸️ MediaPlayer Playback Suspended"));

    // ✅ VideoWidget Material 크래시 방지
    if (VideoWidget && VideoWidget->IsInViewport())
    {
        UE_LOG(LogTemp, Log, TEXT("  📹 Temporarily hiding VideoWidget during suspension"));
        VideoWidget->SetVisibility(ESlateVisibility::Collapsed);
    }

    // ✅ 포커스 복귀 시 자동 재개 시도 (IsValid() 제거)
    if (GetWorld() && MediaPlayer)
    {
        FTimerHandle ResumeTimer;
        GetWorld()->GetTimerManager().SetTimer(
            ResumeTimer,
            [this]()
            {
                if (this && MediaPlayer && !MediaPlayer->IsPlaying())
                {
                    UE_LOG(LogTemp, Warning, TEXT("🔄 Attempting to resume playback after suspension"));
                    MediaPlayer->Play();

                    // VideoWidget 복원
                    if (VideoWidget && VideoWidget->IsInViewport())
                    {
                        VideoWidget->SetVisibility(ESlateVisibility::Visible);
                    }
                }
            },
            1.0f,
                false
                );
    }
}

void AWebcamCapture::CheckPlaybackStatus()
{
    // ✅ 유효성 검사 추가 (IsValid() 제거)
    if (!MediaPlayer || !GetWorld())
    {
        UE_LOG(LogTemp, Error, TEXT("❌ CheckPlaybackStatus: Invalid state"));
        return;
    }

    bool bIsPlaying = MediaPlayer->IsPlaying();
    bool bIsPreparing = MediaPlayer->IsPreparing();
    bool bIsReady = MediaPlayer->IsReady();

    UE_LOG(LogTemp, Warning, TEXT("📊 Playback Status:"));
    UE_LOG(LogTemp, Warning, TEXT("   IsPlaying: %s"), bIsPlaying ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogTemp, Warning, TEXT("   IsPreparing: %s"), bIsPreparing ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogTemp, Warning, TEXT("   IsReady: %s"), bIsReady ? TEXT("YES") : TEXT("NO"));

    if (!bIsPlaying && !bIsPreparing)
    {
        if (RetryCount < MaxRetries)
        {
            RetryCount++;
            UE_LOG(LogTemp, Warning, TEXT("🔄 Retry %d/%d"), RetryCount, MaxRetries);

            MediaPlayer->Close();

            GetWorld()->GetTimerManager().SetTimer(
                RetryTimerHandle,
                this,
                &AWebcamCapture::InitWebcam,
                1.0f,
                false
            );
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("❌ Failed after %d retries"), MaxRetries);
        }
    }
    else if (bIsPlaying)
    {
        UE_LOG(LogTemp, Display, TEXT("✅ Webcam playing successfully!"));
        RetryCount = 0;
    }
    LogVideoTrackInfo();
}
// ============================================================================
// ✅ Application Focus 이벤트 핸들러 (포커스 손실 시 크래시 방지)
// ============================================================================

void AWebcamCapture::OnApplicationWillDeactivate()
{
    UE_LOG(LogTemp, Warning, TEXT("Application regained focus"));

    bIsInBackground = false;

    // Material 재생성
    if (!CachedDynamicMaterial && MediaTextureMaterial && MediaTexture)
    {
        CachedDynamicMaterial = UMaterialInstanceDynamic::Create(MediaTextureMaterial, this);
        if (CachedDynamicMaterial)
        {
            CachedDynamicMaterial->SetTextureParameterValue(FName("MediaTexture"), MediaTexture);
            UE_LOG(LogTemp, Log, TEXT("Dynamic Material recreated"));
        }
    }

    // VideoWidget에 다시 연결
    if (VideoWidget && VideoWidget->VideoDisplay)
    {
        FSlateBrush Brush;
        Brush.SetResourceObject(CachedDynamicMaterial);
        Brush.ImageSize = FVector2D(640, 480);
        VideoWidget->VideoDisplay->SetBrush(Brush);
    }

    if (MediaPlayer) {
        MediaPlayer->Play();
    }

    if (VideoWidget) {
        VideoWidget->SetVisibility(ESlateVisibility::Visible);
    }
}

void AWebcamCapture::OnApplicationHasReactivated()
{
    UE_LOG(LogTemp, Warning, TEXT("Application regained focus"));

    bIsInBackground = false;

    // Material 재생성
    if (!CachedDynamicMaterial && MediaTextureMaterial && MediaTexture)
    {
        CachedDynamicMaterial = UMaterialInstanceDynamic::Create(MediaTextureMaterial, this);
        if (CachedDynamicMaterial)
        {
            CachedDynamicMaterial->SetTextureParameterValue(FName("MediaTexture"), MediaTexture);
            UE_LOG(LogTemp, Log, TEXT("Dynamic Material recreated"));
        }
    }

    // VideoWidget에 다시 연결
    if (VideoWidget && VideoWidget->VideoDisplay)
    {
        FSlateBrush Brush;
        Brush.SetResourceObject(CachedDynamicMaterial);
        Brush.ImageSize = FVector2D(640, 480);
        VideoWidget->VideoDisplay->SetBrush(Brush);
    }

    if (MediaPlayer) {
        MediaPlayer->Play();
    }

    if (VideoWidget) {
        VideoWidget->SetVisibility(ESlateVisibility::Visible);
    }
}

// ✅ 새 함수 구현: 외부에서 설정 변경 가능 (e.g., GolfPlayerController에서 호출)
void AWebcamCapture::SetVideoSavingEnabled(bool bEnable)
{
    CurrentSettings.bEnableVideoSaving = bEnable;
    UE_LOG(LogTemp, Log, TEXT("📹 SetVideoSavingEnabled: %s"), bEnable ? TEXT("true") : TEXT("false"));

    // 설정 즉시 적용
    ApplySettings(CurrentSettings);
}
// =============================================================================
// ✅ 범용 웹캠 검색 함수들 (VID/PID 기반)
// =============================================================================

// =============================================================================
// ✅ 실제 구현: VID/PID로 웹캠 검색
// =============================================================================
bool AWebcamCapture::FindAndConnectByVIDPID(const FString& VID, const FString& PID, int32 Index)
{
#if PLATFORM_WINDOWS
    UE_LOG(LogTemp, Log, TEXT("🔍 Searching for webcam with VID: %s, PID: %s, Index: %d"), *VID, *PID, Index);

    if (!MediaPlayer)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ MediaPlayer is null"));
        return false;
    }

    // 1. 비디오 캡처 디바이스 클래스 GUID
    GUID VideoGuid = KSCATEGORY_VIDEO_CAMERA;

    // 2. 현재 연결된 모든 비디오 디바이스 열거
    HDEVINFO DeviceInfoSet = SetupDiGetClassDevsW(
        &VideoGuid,
        NULL,
        NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );

    if (DeviceInfoSet == INVALID_HANDLE_VALUE)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to get device list (Error: %d)"), GetLastError());
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("📹 Enumerating video capture devices..."));

    int32 MatchingDeviceIndex = 0;
    bool bFound = false;
    FString FoundDevicePath;
    FString FoundDeviceName;

    // 3. 각 디바이스 열거
    SP_DEVICE_INTERFACE_DATA DeviceInterfaceData;
    DeviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(DeviceInfoSet, NULL, &VideoGuid, i, &DeviceInterfaceData); i++)
    {
        // 4. 디바이스 인터페이스 세부 정보 크기 가져오기
        DWORD RequiredSize = 0;
        SetupDiGetDeviceInterfaceDetailW(DeviceInfoSet, &DeviceInterfaceData, NULL, 0, &RequiredSize, NULL);

        if (RequiredSize == 0)
            continue;

        // 5. 메모리 할당 및 세부 정보 가져오기
        PSP_DEVICE_INTERFACE_DETAIL_DATA_W DeviceInterfaceDetailData =
            (PSP_DEVICE_INTERFACE_DETAIL_DATA_W)FMemory::Malloc(RequiredSize);

        if (!DeviceInterfaceDetailData)
            continue;

        DeviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        SP_DEVINFO_DATA DeviceInfoData;
        DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        if (SetupDiGetDeviceInterfaceDetailW(DeviceInfoSet, &DeviceInterfaceData,
            DeviceInterfaceDetailData, RequiredSize, NULL, &DeviceInfoData))
        {
            // 6. 디바이스 경로 가져오기
            FString DevicePath(DeviceInterfaceDetailData->DevicePath);

            // 7. 디바이스 이름 가져오기
            WCHAR DeviceDesc[256];
            if (SetupDiGetDeviceRegistryPropertyW(DeviceInfoSet, &DeviceInfoData,
                SPDRP_DEVICEDESC, NULL, (PBYTE)DeviceDesc, sizeof(DeviceDesc), NULL))
            {
                FString DeviceName(DeviceDesc);

                // 8. 하드웨어 ID 가져오기 (VID/PID 포함)
                WCHAR HardwareID[1024];
                if (SetupDiGetDeviceRegistryPropertyW(DeviceInfoSet, &DeviceInfoData,
                    SPDRP_HARDWAREID, NULL, (PBYTE)HardwareID, sizeof(HardwareID), NULL))
                {
                    FString HardwareIDStr(HardwareID);

                    // 9. VID/PID 추출
                    FString DeviceVID, DevicePID;
                    if (WebcamSearchHelpers::ExtractVIDPIDFromString(HardwareIDStr, DeviceVID, DevicePID))
                    {
                        UE_LOG(LogTemp, Log, TEXT("   [%d] %s"), i, *DeviceName);
                        UE_LOG(LogTemp, Log, TEXT("       VID: %s, PID: %s"), *DeviceVID, *DevicePID);

                        // 10. VID/PID 매칭 확인
                        if (DeviceVID.Equals(VID, ESearchCase::IgnoreCase) &&
                            DevicePID.Equals(PID, ESearchCase::IgnoreCase))
                        {
                            UE_LOG(LogTemp, Warning, TEXT("       ✅ Match found!"));

                            // 11. Index 확인
                            if (MatchingDeviceIndex == Index)
                            {
                                FoundDevicePath = DevicePath;
                                FoundDeviceName = DeviceName;
                                bFound = true;
                                UE_LOG(LogTemp, Warning, TEXT("       🎯 This is the target device (Index: %d)"), Index);
                                break;
                            }
                            else
                            {
                                UE_LOG(LogTemp, Log, TEXT("       ⏭️ Skipping (Looking for Index: %d)"), Index);
                                MatchingDeviceIndex++;
                            }
                        }
                    }
                }
            }
        }

        FMemory::Free(DeviceInterfaceDetailData);
    }

    // 12. 정리
    SetupDiDestroyDeviceInfoList(DeviceInfoSet);

    // 13. 연결 시도
    if (bFound)
    {
        UE_LOG(LogTemp, Warning, TEXT("✅ Found matching device: %s"), *FoundDeviceName);
        UE_LOG(LogTemp, Log, TEXT("   Path: %s"), *FoundDevicePath);

        FString URL = WebcamSearchHelpers::CreateVidcapURL(FoundDevicePath);
        UE_LOG(LogTemp, Log, TEXT("   URL: %s"), *URL);

        if (MediaPlayer->OpenUrl(URL))
        {
            MediaPlayer->Play();
            UE_LOG(LogTemp, Warning, TEXT("✅ Successfully connected via VID/PID!"));

            // Config에 저장
            CurrentSettings.WebcamURL = URL;
            SaveConfig();

            return true;
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("❌ Failed to open URL"));
            return false;
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ No matching device found (VID: %s, PID: %s, Index: %d)"),
            *VID, *PID, Index);
        return false;
    }

#else
    UE_LOG(LogTemp, Error, TEXT("❌ VID/PID search only supported on Windows"));
    return false;
#endif
}

// =============================================================================
// ✅ 실제 구현: 디바이스 이름으로 검색
// =============================================================================
bool AWebcamCapture::FindWebcamByName(const FString& DeviceName)
{
#if PLATFORM_WINDOWS
    UE_LOG(LogTemp, Log, TEXT("🔍 Searching for webcam by name: %s"), *DeviceName);

    if (!MediaPlayer)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ MediaPlayer is null"));
        return false;
    }

    GUID VideoGuid = KSCATEGORY_VIDEO_CAMERA;

    HDEVINFO DeviceInfoSet = SetupDiGetClassDevsW(
        &VideoGuid,
        NULL,
        NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );

    if (DeviceInfoSet == INVALID_HANDLE_VALUE)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to get device list"));
        return false;
    }

    bool bFound = false;
    FString FoundDevicePath;
    FString FoundDeviceName;

    SP_DEVICE_INTERFACE_DATA DeviceInterfaceData;
    DeviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(DeviceInfoSet, NULL, &VideoGuid, i, &DeviceInterfaceData); i++)
    {
        DWORD RequiredSize = 0;
        SetupDiGetDeviceInterfaceDetailW(DeviceInfoSet, &DeviceInterfaceData, NULL, 0, &RequiredSize, NULL);

        if (RequiredSize == 0)
            continue;

        PSP_DEVICE_INTERFACE_DETAIL_DATA_W DeviceInterfaceDetailData =
            (PSP_DEVICE_INTERFACE_DETAIL_DATA_W)FMemory::Malloc(RequiredSize);

        if (!DeviceInterfaceDetailData)
            continue;

        DeviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        SP_DEVINFO_DATA DeviceInfoData;
        DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        if (SetupDiGetDeviceInterfaceDetailW(DeviceInfoSet, &DeviceInterfaceData,
            DeviceInterfaceDetailData, RequiredSize, NULL, &DeviceInfoData))
        {
            FString DevicePath(DeviceInterfaceDetailData->DevicePath);

            WCHAR DeviceDesc[256];
            if (SetupDiGetDeviceRegistryPropertyW(DeviceInfoSet, &DeviceInfoData,
                SPDRP_DEVICEDESC, NULL, (PBYTE)DeviceDesc, sizeof(DeviceDesc), NULL))
            {
                FString CurrentDeviceName(DeviceDesc);
                UE_LOG(LogTemp, Log, TEXT("   [%d] %s"), i, *CurrentDeviceName);

                // 부분 일치 검사 (대소문자 구분 안 함)
                if (CurrentDeviceName.Contains(DeviceName, ESearchCase::IgnoreCase))
                {
                    UE_LOG(LogTemp, Warning, TEXT("       ✅ Name match found!"));
                    FoundDevicePath = DevicePath;
                    FoundDeviceName = CurrentDeviceName;
                    bFound = true;
                    break;
                }
            }
        }

        FMemory::Free(DeviceInterfaceDetailData);
    }

    SetupDiDestroyDeviceInfoList(DeviceInfoSet);

    if (bFound)
    {
        UE_LOG(LogTemp, Warning, TEXT("✅ Found device: %s"), *FoundDeviceName);

        FString URL = WebcamSearchHelpers::CreateVidcapURL(FoundDevicePath);

        if (MediaPlayer->OpenUrl(URL))
        {
            MediaPlayer->Play();
            UE_LOG(LogTemp, Warning, TEXT("✅ Successfully connected via name search!"));

            CurrentSettings.WebcamURL = URL;
            SaveConfig();

            return true;
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("❌ Failed to open URL"));
            return false;
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ No device found matching: %s"), *DeviceName);
        return false;
    }

#else
    UE_LOG(LogTemp, Error, TEXT("❌ Name search only supported on Windows"));
    return false;
#endif
}

// =============================================================================
// ✅ 실제 구현: 모든 웹캠 열거
// =============================================================================
TArray<FString> AWebcamCapture::EnumerateWebcams()
{
    TArray<FString> WebcamList;

#if PLATFORM_WINDOWS
    UE_LOG(LogTemp, Log, TEXT("📹 Enumerating all available webcams..."));

    GUID VideoGuid = KSCATEGORY_VIDEO_CAMERA;

    HDEVINFO DeviceInfoSet = SetupDiGetClassDevsW(
        &VideoGuid,
        NULL,
        NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );

    if (DeviceInfoSet == INVALID_HANDLE_VALUE)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to get device list"));
        return WebcamList;
    }

    SP_DEVICE_INTERFACE_DATA DeviceInterfaceData;
    DeviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(DeviceInfoSet, NULL, &VideoGuid, i, &DeviceInterfaceData); i++)
    {
        DWORD RequiredSize = 0;
        SetupDiGetDeviceInterfaceDetailW(DeviceInfoSet, &DeviceInterfaceData, NULL, 0, &RequiredSize, NULL);

        if (RequiredSize == 0)
            continue;

        PSP_DEVICE_INTERFACE_DETAIL_DATA_W DeviceInterfaceDetailData =
            (PSP_DEVICE_INTERFACE_DETAIL_DATA_W)FMemory::Malloc(RequiredSize);

        if (!DeviceInterfaceDetailData)
            continue;

        DeviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        SP_DEVINFO_DATA DeviceInfoData;
        DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        if (SetupDiGetDeviceInterfaceDetailW(DeviceInfoSet, &DeviceInterfaceData,
            DeviceInterfaceDetailData, RequiredSize, NULL, &DeviceInfoData))
        {
            WCHAR DeviceDesc[256];
            if (SetupDiGetDeviceRegistryPropertyW(DeviceInfoSet, &DeviceInfoData,
                SPDRP_DEVICEDESC, NULL, (PBYTE)DeviceDesc, sizeof(DeviceDesc), NULL))
            {
                FString DeviceName(DeviceDesc);
                FString DevicePath(DeviceInterfaceDetailData->DevicePath);

                // VID/PID 추출
                WCHAR HardwareID[1024];
                FString VID, PID;
                if (SetupDiGetDeviceRegistryPropertyW(DeviceInfoSet, &DeviceInfoData,
                    SPDRP_HARDWAREID, NULL, (PBYTE)HardwareID, sizeof(HardwareID), NULL))
                {
                    FString HardwareIDStr(HardwareID);
                    WebcamSearchHelpers::ExtractVIDPIDFromString(HardwareIDStr, VID, PID);
                }

                UE_LOG(LogTemp, Log, TEXT("   [%d] %s"), i, *DeviceName);
                if (!VID.IsEmpty() && !PID.IsEmpty())
                {
                    UE_LOG(LogTemp, Log, TEXT("       VID: %s, PID: %s"), *VID, *PID);
                }

                WebcamList.Add(DeviceName);
            }
        }

        FMemory::Free(DeviceInterfaceDetailData);
    }

    SetupDiDestroyDeviceInfoList(DeviceInfoSet);

    UE_LOG(LogTemp, Log, TEXT("📹 Found %d webcam(s)"), WebcamList.Num());

    // 첫 번째 웹캠으로 연결 시도
    if (WebcamList.Num() > 0 && MediaPlayer)
    {
        UE_LOG(LogTemp, Warning, TEXT("🎲 Attempting to connect to first webcam..."));
        if (FindWebcamByName(WebcamList[0]))
        {
            UE_LOG(LogTemp, Warning, TEXT("✅ Connected to first available webcam"));
        }
    }

#else
    UE_LOG(LogTemp, Error, TEXT("❌ Webcam enumeration only supported on Windows"));
#endif

    return WebcamList;
}


void AWebcamCapture::SaveFrameToPNG(const FString& FrameName)
{

    UTexture2D* CurrentFrame = CaptureCurrentFrame();


    // 저장 폴더 생성
    FString SaveDir = FPaths::ProjectSavedDir() / TEXT("DebugFrames/");

    // ✅ 폴더 생성 (UE 4.26 호환)
    IFileManager::Get().MakeDirectory(*SaveDir, true);

    // 파일명 생성
    FString FileName = FString::Printf(TEXT("%s_%lld.png"),
        *FrameName,
        FDateTime::Now().GetTicks());
    FString FilePath = SaveDir + FileName;

    // 저장
    if (SaveTextureToPNG(CurrentFrame, FilePath))
    {
        UE_LOG(LogTemp, Warning, TEXT("✅ Frame saved: %s"), *FilePath);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to save frame: %s"), *FilePath);
    }
}

void AWebcamCapture::SaveAllBufferedFramesToPNG(const FString& OutputFolder)
{
    if (!VideoBufferComponent)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ SaveAllBufferedFramesToPNG: VideoBufferComponent not found"));
        return;
    }

    // 저장 폴더 결정
    FString SaveDir = OutputFolder.IsEmpty() ?
        FPaths::ProjectSavedDir() / TEXT("DebugFrames/") :
        OutputFolder;

    // ✅ 폴더 생성 (UE 4.26 호환)
    IFileManager::Get().MakeDirectory(*SaveDir, true);

    UE_LOG(LogTemp, Warning, TEXT("📁 Saving all buffered frames to: %s"), *SaveDir);

    // 모든 프레임 추출
    TArray<FVideoFrame> AllFrames = VideoBufferComponent->GetFramesInRange(0.0f, FLT_MAX);

    if (AllFrames.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ No frames in buffer to save"));
        return;
    }

    int32 SavedCount = 0;
    int32 FailCount = 0;

    // 모든 프레임 저장
    for (int32 FrameIdx = 0; FrameIdx < AllFrames.Num(); ++FrameIdx)
    {
        const FVideoFrame& Frame = AllFrames[FrameIdx];

        if (!Frame.FrameTexture || !IsValid(Frame.FrameTexture))
        {
            FailCount++;
            continue;
        }

        FString FileName = FString::Printf(TEXT("frame_%04d_t%.2f.png"), FrameIdx, Frame.Timestamp);
        FString FilePath = SaveDir + FileName;

        if (SaveTextureToPNG(Frame.FrameTexture, FilePath))
        {
            SavedCount++;
        }
        else
        {
            FailCount++;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("📁 Saved %d frames, %d failed to: %s"),
        SavedCount, FailCount, *SaveDir);
}

void AWebcamCapture::SaveBufferedFrameAt(int32 Index, const FString& OutputFolder)
{
    if (!VideoBufferComponent)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ SaveBufferedFrameAt: VideoBufferComponent not found"));
        return;
    }

    // 모든 프레임 추출
    TArray<FVideoFrame> AllFrames = VideoBufferComponent->GetFramesInRange(0.0f, FLT_MAX);

    // 범위 체크
    if (!AllFrames.IsValidIndex(Index))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ SaveBufferedFrameAt: Index out of range: %d (available: %d)"),
            Index, AllFrames.Num());
        return;
    }

    const FVideoFrame& Frame = AllFrames[Index];

    if (!Frame.FrameTexture || !IsValid(Frame.FrameTexture))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ SaveBufferedFrameAt: Frame at index %d has invalid texture"), Index);
        return;
    }

    FString SaveDir = OutputFolder.IsEmpty() ?
        FPaths::ProjectSavedDir() / TEXT("DebugFrames/") :
        OutputFolder;

    // ✅ 폴더 생성 (UE 4.26 호환)
    IFileManager::Get().MakeDirectory(*SaveDir, true);

    FString FileName = FString::Printf(TEXT("frame_%04d_t%.2f.png"), Index, Frame.Timestamp);
    FString FilePath = SaveDir + FileName;

    if (SaveTextureToPNG(Frame.FrameTexture, FilePath))
    {
        UE_LOG(LogTemp, Warning, TEXT("✅ Frame %d saved: %s"), Index, *FilePath);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to save frame %d"), Index);
    }
}

// ========== ✅ 헬퍼 함수: 텍스처를 PNG로 저장 ==========

bool AWebcamCapture::SaveTextureToPNG(UTexture2D* Texture, const FString& FilePath)
{
    if (!Texture || !IsValid(Texture))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Invalid texture"));
        return false;
    }

    int32 Width = Texture->GetSizeX();
    int32 Height = Texture->GetSizeY();

    if (Width <= 0 || Height <= 0)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Invalid dimensions: %dx%d"), Width, Height);
        return false;
    }

    // ✅ Mipmap 데이터 접근
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION <= 26
    if (!Texture->PlatformData || Texture->PlatformData->Mips.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Invalid PlatformData"));
        return false;
    }
    FTexture2DMipMap& Mip = Texture->PlatformData->Mips[0];
#else
    if (!Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Invalid PlatformData"));
        return false;
    }
    FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
#endif

    // ✅ BulkData에서 픽셀 데이터 추출 (FColor 배열 사용)
    TArray<FColor> MipData;
    MipData.SetNum(Width * Height);

    void* SrcData = Mip.BulkData.Lock(LOCK_READ_ONLY);
    if (!SrcData)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to lock bulk data"));
        return false;
    }

    // ✅ 메모리 복사 (FColor 구조로 복사)
    FMemory::Memcpy(MipData.GetData(), SrcData, Width * Height * sizeof(FColor));
    Mip.BulkData.Unlock();

    // ✅ PNG 포맷으로 압축 (FColor 배열 전달)
    TArray<uint8> CompressedBitmap;
    FImageUtils::CompressImageArray(Width, Height, MipData, CompressedBitmap);

    // ✅ 파일로 저장
    if (FFileHelper::SaveArrayToFile(CompressedBitmap, *FilePath))
    {
        UE_LOG(LogTemp, Log, TEXT("✔ 저장 완료: %s (%dx%d)"), *FilePath, Width, Height);
        return true;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to save: %s"), *FilePath);
        return false;
    }
}

//
//void AWebcamCapture::LogVideoTrackInfo() const
//{
//    if (!MediaPlayer) return;
//
//    const int32 NumVideoTracks = MediaPlayer->GetNumTracks(EMediaPlayerTrack::Video);
//    UE_LOG(LogTemp, Warning, TEXT("🎞️ VideoTracks: %d"), NumVideoTracks);
//
//    const int32 SelTrack = MediaPlayer->GetSelectedTrack(EMediaPlayerTrack::Video);
//    UE_LOG(LogTemp, Warning, TEXT("🎯 SelectedVideoTrack: %d"), SelTrack);
//
//    if (NumVideoTracks <= 0 || SelTrack < 0)
//    {
//        UE_LOG(LogTemp, Warning, TEXT("⚠️ No selected video track"));
//        return;
//    }
//
//    // ✅ 선택된 트랙 기준
//    const int32 NumFormats = MediaPlayer->GetNumTrackFormats(EMediaPlayerTrack::Video, SelTrack);
//    UE_LOG(LogTemp, Warning, TEXT("🧩 SelectedTrack Formats: %d"), NumFormats);
//
//    // ✅ 선택된 포맷 인덱스
//    const int32 SelFmt = MediaPlayer->GetTrackFormat(EMediaPlayerTrack::Video, SelTrack);
//    UE_LOG(LogTemp, Warning, TEXT("🎛️ SelectedFormatIndex: %d"), SelFmt);
//
//    // ✅ 선택된 포맷 기준 정보
//    if (NumFormats > 0 && SelFmt >= 0)
//    {
//        const FIntPoint Dim = MediaPlayer->GetVideoTrackDimensions(SelTrack, SelFmt);
//        const float FPS = MediaPlayer->GetVideoTrackFrameRate(SelTrack, SelFmt);
//        UE_LOG(LogTemp, Warning, TEXT("📐 Selected Dim: %dx%d, FPS: %.2f"), Dim.X, Dim.Y, FPS);
//    }
//
//    if (MediaTexture)
//    {
//        UE_LOG(LogTemp, Warning, TEXT("🖼️ MediaTexture Size: %dx%d"),
//            MediaTexture->GetWidth(), MediaTexture->GetHeight());
//    }
//}

void AWebcamCapture::PauseCapture()
{
    if (bIsCapturing)
    {
        bIsCapturing = false;
        bIsPausingCapture = true;

        if (MediaPlayer && MediaPlayer->IsPlaying())
        {
            MediaPlayer->Pause();
        }

        UE_LOG(LogTemp, Warning, TEXT("⏸️ Capture PAUSED"));
        UE_LOG(LogTemp, Warning, TEXT("   Current time: %.2f seconds"), CurrentCaptureTime);
        UE_LOG(LogTemp, Warning, TEXT("   Captured frames: %d"), FrameCounter);
    }
}


// ========== Step 2-2: ResumeCapture() 함수 추가 ==========

void AWebcamCapture::ResumeCapture()
{
    if (!bIsCapturing && bIsPausingCapture)
    {
        bIsCapturing = true;
        bIsPausingCapture = false;

        // ✅ MediaPlayer 재개 추가
        if (MediaPlayer && !MediaPlayer->IsPlaying())
        {
            MediaPlayer->Play();
        }

        UE_LOG(LogTemp, Warning, TEXT("▶️ Capture RESUMED"));
        UE_LOG(LogTemp, Warning, TEXT("   Ready for next shot"));
    }
}


bool AWebcamCapture::FindYUY2_640x480_30fps(int32& OutTrackIndex, int32& OutFormatIndex)
{
    if (!MediaPlayer || !MediaPlayer->IsReady())
    {
        return false;
    }

    int32 NumTracks = MediaPlayer->GetNumTracks(EMediaPlayerTrack::Video);

    UE_LOG(LogTemp, Warning, TEXT("🔍 Searching for YUY2 640x480@30fps format..."));

    // 모든 트랙 순회
    for (int32 TrackIdx = 0; TrackIdx < NumTracks; TrackIdx++)
    {
        // 트랙 선택 (임시)
        if (!MediaPlayer->SelectTrack(EMediaPlayerTrack::Video, TrackIdx))
        {
            continue;
        }

        int32 NumFormats = MediaPlayer->GetNumTrackFormats(EMediaPlayerTrack::Video, TrackIdx);

        // 모든 포맷 순회
        for (int32 FormatIdx = 0; FormatIdx < NumFormats; FormatIdx++)
        {
            // ✅ GetTrackDisplayName 사용 (3개 파라미터 지원!)
            FText DisplayName = MediaPlayer->GetTrackDisplayName(
                EMediaPlayerTrack::Video,
                TrackIdx
            );

            FString FormatStr = DisplayName.ToString();

            // YUY2 640x480 30fps 체크
            bool bIsYUY2 = FormatStr.Contains(TEXT("YUY2"), ESearchCase::IgnoreCase);
            bool bIs640x480 = FormatStr.Contains(TEXT("640")) && FormatStr.Contains(TEXT("480"));
            bool bIs30fps = FormatStr.Contains(TEXT("30"));

            UE_LOG(LogTemp, Verbose, TEXT("  Track %d, Format %d: %s"),
                TrackIdx, FormatIdx, *FormatStr);

            if (bIsYUY2 && bIs640x480 && bIs30fps)
            {
                OutTrackIndex = TrackIdx;
                OutFormatIndex = FormatIdx;

                UE_LOG(LogTemp, Warning, TEXT("✅ Found YUY2 640x480@30fps!"));
                UE_LOG(LogTemp, Warning, TEXT("   Track Index: %d"), TrackIdx);
                UE_LOG(LogTemp, Warning, TEXT("   Format Index: %d"), FormatIdx);
                UE_LOG(LogTemp, Warning, TEXT("   Format String: %s"), *FormatStr);

                return true;
            }
        }
    }

    UE_LOG(LogTemp, Error, TEXT("❌ YUY2 640x480@30fps format not found!"));
    UE_LOG(LogTemp, Warning, TEXT("   Please check available formats using LogVideoTrackInfo()"));
    return false;
}


// ✅ 4. 새로운 헬퍼 함수 - 모든 비디오 트랙/포맷 정보 출력
void AWebcamCapture::LogVideoTrackInfo() const
{
    if (!MediaPlayer || !MediaPlayer->IsReady())
    {
        return;
    }

    int32 NumTracks = MediaPlayer->GetNumTracks(EMediaPlayerTrack::Video);

    UE_LOG(LogTemp, Warning, TEXT("📹 Video Track Information:"));
    UE_LOG(LogTemp, Warning, TEXT("========================================"));

    for (int32 TrackIdx = 0; TrackIdx < NumTracks; TrackIdx++)
    {
        // 임시로 트랙 선택
        if (!MediaPlayer->SelectTrack(EMediaPlayerTrack::Video, TrackIdx))
        {
            UE_LOG(LogTemp, Warning, TEXT("Track %d: [Failed to select]"), TrackIdx);
            continue;
        }

        int32 NumFormats = MediaPlayer->GetNumTrackFormats(EMediaPlayerTrack::Video, TrackIdx);
        UE_LOG(LogTemp, Log, TEXT("Track %d: %d formats available"), TrackIdx, NumFormats);

        // 모든 포맷 출력
        for (int32 FormatIdx = 0; FormatIdx < NumFormats; FormatIdx++)
        {
            // ✅ GetTrackDisplayName 사용 (3개 파라미터 지원!)
            FText DisplayName = MediaPlayer->GetTrackDisplayName(
                EMediaPlayerTrack::Video,
                TrackIdx
            );

            FString FormatStr = DisplayName.ToString();

            // ✅ YUY2 포맷 강조 표시
            if (FormatStr.Contains(TEXT("YUY2"), ESearchCase::IgnoreCase))
            {
                UE_LOG(LogTemp, Warning, TEXT("  Format %d: %s ⭐ YUY2"), FormatIdx, *FormatStr);
            }
            else if (FormatStr.Contains(TEXT("MJPG"), ESearchCase::IgnoreCase) ||
                FormatStr.Contains(TEXT("MJPEG"), ESearchCase::IgnoreCase))
            {
                UE_LOG(LogTemp, Log, TEXT("  Format %d: %s (MJPEG)"), FormatIdx, *FormatStr);
            }
            else
            {
                UE_LOG(LogTemp, Log, TEXT("  Format %d: %s"), FormatIdx, *FormatStr);
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("========================================"));
}


bool AWebcamCapture::SetVideoFormat(int32 InTrackIndex, int32 InFormatIndex)
{
    if (!MediaPlayer || !MediaPlayer->IsReady())
    {
        UE_LOG(LogTemp, Error, TEXT("❌ MediaPlayer not ready"));
        return false;
    }

    int32 NumTracks = MediaPlayer->GetNumTracks(EMediaPlayerTrack::Video);
    if (InTrackIndex >= NumTracks)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Invalid track index: %d (max: %d)"), InTrackIndex, NumTracks - 1);
        return false;
    }

    // 트랙 선택
    if (!MediaPlayer->SelectTrack(EMediaPlayerTrack::Video, InTrackIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to select track %d"), InTrackIndex);
        return false;
    }

    int32 NumFormats = MediaPlayer->GetNumTrackFormats(EMediaPlayerTrack::Video, InTrackIndex);
    if (InFormatIndex >= NumFormats)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Invalid format index: %d (max: %d)"), InFormatIndex, NumFormats - 1);
        return false;
    }

    // 포맷 설정
    if (!MediaPlayer->SetTrackFormat(EMediaPlayerTrack::Video, InTrackIndex, InFormatIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to set format %d"), InFormatIndex);
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("✅ Successfully set Track %d, Format %d"), InTrackIndex, InFormatIndex);

    // 설정 저장
    TrackIndex = InTrackIndex;
    FormatIndex = InFormatIndex;

    // 재생 재시작
    GetWorld()->GetTimerManager().SetTimer(
        PlayStartTimerHandle,
        [this]()
        {
            if (MediaPlayer && !MediaPlayer->IsPlaying())
            {
                MediaPlayer->Play();
                UE_LOG(LogTemp, Warning, TEXT("🔄 Playback restarted"));
            }
        },
        0.2f,
            false
            );

    return true;
}


void AWebcamCapture::StopCaptureInternal(bool bClearBufferNow)
{
    UE_LOG(LogTemp, Log, TEXT("🎥 ======================= AWebcamCapture::StopCaptureInternal"));
    bIsCapturing = false;
    bIsPausingCapture = true;

    // 타이머 완전 중지 (중요!)
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(CaptureTimerHandle);
    }

    // 버퍼 저장 비활성화
    if (VideoBufferComponent)
    {
        VideoBufferComponent->ToggleVideoSaving(false);

        // 즉시 비울지 여부 선택
        if (bClearBufferNow)
        {
            VideoBufferComponent->ClearBuffer();
        }
    }

    // 카운터 리셋(다음 샷 대비)
    CurrentCaptureTime = 0.f;
    FrameCounter = 0;
}

void AWebcamCapture::StartCaptureInternal()
{
    if (!GetWorld()) return;

    UE_LOG(LogTemp, Log, TEXT("🎥 ======================= AWebcamCapture::StartCaptureInternal"));
    // "새 샷"을 위한 버퍼 정리
    if (VideoBufferComponent)
    {
        VideoBufferComponent->ClearBuffer();
        VideoBufferComponent->ToggleVideoSaving(true);
    }

    bIsCapturing = true;
    bIsPausingCapture = false;
    FrameCounter = 0;
    CurrentCaptureTime = 0.f;

    // tic에서 시작
    //const float CaptureInterval = 1.0f / FMath::Max(1, CurrentSettings.FPS);

    //GetWorld()->GetTimerManager().SetTimer(
    //    CaptureTimerHandle,
    //    this,
    //    &AWebcamCapture::CaptureFrame,
    //    CaptureInterval,
    //    true
    //);
}


void AWebcamCapture::StartCaptureForSwing()
{
    UE_LOG(LogTemp, Log, TEXT("🎥 ======================= AWebcamCapture:: StartCaptureForSwing"));

    // 완전히 깨끗한 상태에서 시작
    StopCaptureInternal(true);   // 타이머 중지 + 버퍼 비움
    StartCaptureInternal();      // 새 타이머 시작
}

void AWebcamCapture::StopCaptureAfterShot()
{
    UE_LOG(LogTemp, Log, TEXT("🎥 =======================AWebcamCapture:: StopCaptureAfterShot"));

    // 타이머는 멈추되,
    // 버퍼는 바로 비우지 않음(→ SwingFrames 추출에 필요)
    StopCaptureInternal(false);
}

void AWebcamCapture::ResumeCaptureForNextSwing()
{
    UE_LOG(LogTemp, Log, TEXT("🎥 =======================AWebcamCapture:: ResumeCaptureForNextSwing"));

    // 완전 리셋 후 다시 시작
    StartCaptureInternal();
}

FString AWebcamCapture::SaveSwingClipToDisk(const TArray<FVideoFrame>& SwingFrames, float ShotTime)
{
    SCOPE_CYCLE_COUNTER(STAT_WebcamSaveClip);
    UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════"));
    UE_LOG(LogTemp, Warning, TEXT("💾 Saving Swing Clip (Flat File Mode + Async)"));
    UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════"));


    if (SwingFrames.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ No frames to save"));
        return FString();
    }

    // ========== Step 1: 고정 디렉토리 설정 ========== ✅
    FString ProjectDir = FPaths::ProjectDir();
    FString ClipDir = FPaths::Combine(ProjectDir, TEXT("Saved"), TEXT("SwingClips"));

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    // 디렉토리 생성 (최초 1회만)
    if (!PlatformFile.DirectoryExists(*ClipDir))
    {
        if (!PlatformFile.CreateDirectoryTree(*ClipDir))
        {
            UE_LOG(LogTemp, Error, TEXT("❌ Failed to create directory: %s"), *ClipDir);
            return FString();
        }
        UE_LOG(LogTemp, Log, TEXT("📁 Created clip directory: %s"), *ClipDir);
    }

    UE_LOG(LogTemp, Log, TEXT("📁 Clip directory: %s"), *ClipDir);
    UE_LOG(LogTemp, Log, TEXT("📊 Total frames: %d"), SwingFrames.Num());

    // ========== Step 2: 이전 비동기 작업 완료 대기 ========== ✅
    if (AsyncSaveTask.IsValid() && !AsyncSaveTask.IsReady())
    {
        UE_LOG(LogTemp, Warning, TEXT("⏳ Waiting for previous save to complete..."));

        double StartWaitTime = FPlatformTime::Seconds();
        AsyncSaveTask.Wait();
        double WaitDuration = FPlatformTime::Seconds() - StartWaitTime;

        UE_LOG(LogTemp, Warning, TEXT("✅ Previous save completed (waited %.2f seconds)"), WaitDuration);
    }

    // ========== Step 3: 메타데이터 계산 ========== ✅
    int32 Width = 640;
    int32 Height = 480;
    float FPS = 30.0f;

    if (SwingFrames.Num() > 0 && SwingFrames[0].FrameTexture)
    {
        Width = SwingFrames[0].FrameTexture->GetSizeX();
        Height = SwingFrames[0].FrameTexture->GetSizeY();
    }

    if (SwingFrames.Num() > 1)
    {
        float Duration = SwingFrames.Last().Timestamp - SwingFrames[0].Timestamp;
        if (Duration > 0.0f)
        {
            FPS = (SwingFrames.Num() - 1) / Duration;
        }
    }

    // ========== Step 4: 프레임 데이터 복사 ========== ✅
    TArray<FVideoFrame> FramesCopy = SwingFrames;

    UE_LOG(LogTemp, Warning, TEXT("🚀 Starting async save in background..."));

    // ========== Step 5: 비동기 작업 시작 ========== ✅
    AsyncSaveTask = Async(EAsyncExecution::Thread, [FramesCopy, ClipDir]()
        {
            UE_LOG(LogTemp, Log, TEXT("🔧 Background worker started"));
            UE_LOG(LogTemp, Log, TEXT("   Thread ID: %d"), FPlatformTLS::GetCurrentThreadId());

            IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();

            // ========== Phase 1: 기존 JPG 파일 모두 삭제 ========== ✅
            UE_LOG(LogTemp, Log, TEXT("🗑️ Deleting previous JPG files..."));

            TArray<FString> FoundFiles;
            IFileManager::Get().FindFiles(FoundFiles, *FPaths::Combine(ClipDir, TEXT("*.jpg")), true, false);

            int32 DeletedCount = 0;
            for (const FString& FileName : FoundFiles)
            {
                FString FilePath = FPaths::Combine(ClipDir, FileName);
                if (PF.DeleteFile(*FilePath))
                {
                    DeletedCount++;
                }
            }

            UE_LOG(LogTemp, Log, TEXT("   Deleted %d JPG files"), DeletedCount);

            // ========== Phase 2: 새 프레임 저장 ========== ✅
            int32 SavedCount = 0;
            int32 FailedCount = 0;

            for (int32 i = 0; i < FramesCopy.Num(); i++)
            {
                const FVideoFrame& Frame = FramesCopy[i];

                if (!Frame.FrameTexture || !IsValid(Frame.FrameTexture))
                {
                    FailedCount++;
                    continue;
                }

                FString FileName = FString::Printf(TEXT("%04d.jpg"), i + 1);
                FString FilePath = FPaths::Combine(ClipDir, FileName);

                if (AWebcamCapture::SaveFrameAsJPG_ThreadSafe(Frame.FrameTexture, FilePath, 85))
                {
                    SavedCount++;

                    // 진행률 로그 (20% 단위)
                    if ((i + 1) % FMath::Max(1, FramesCopy.Num() / 5) == 0)
                    {
                        float Progress = ((float)(i + 1) / FramesCopy.Num()) * 100.0f;
                        UE_LOG(LogTemp, Log, TEXT("  📊 Background save: %.0f%% (%d/%d frames)"),
                            Progress, i + 1, FramesCopy.Num());
                    }
                }
                else
                {
                    FailedCount++;
                }
            }

            UE_LOG(LogTemp, Warning, TEXT("✅ Background save completed!"));
            UE_LOG(LogTemp, Warning, TEXT("   Saved: %d frames"), SavedCount);
            UE_LOG(LogTemp, Warning, TEXT("   Failed: %d frames"), FailedCount);
            UE_LOG(LogTemp, Warning, TEXT("   Path: %s"), *ClipDir);
        });

    bIsSavingAsync = true;

    // ========== Step 6: 메타데이터 저장 (동기 - 빠름) ========== ✅
    SaveClipMetadata(ClipDir, SwingFrames.Num(), FPS, Width, Height, ShotTime);

    // ========== Step 7: 로그 출력 ==========
    UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════"));
    UE_LOG(LogTemp, Warning, TEXT("✅ Async Save Started (Flat File Mode)"));
    UE_LOG(LogTemp, Warning, TEXT("   Frames: %d"), SwingFrames.Num());
    UE_LOG(LogTemp, Warning, TEXT("   Quality: 85"));
    UE_LOG(LogTemp, Warning, TEXT("   Path: %s"), *ClipDir);
    UE_LOG(LogTemp, Warning, TEXT("   Mode: Single clip, no subfolders"));
    UE_LOG(LogTemp, Warning, TEXT("   Status: Saving in background... (no lag!)"));
    UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════"));

    return ClipDir;
}

// ========== 2. 워커 스레드 함수 (static) ==========
void AWebcamCapture::SaveFramesWorker(TArray<FVideoFrame> Frames, FString ClipDir, int32 Quality)
{
    UE_LOG(LogTemp, Log, TEXT("🔧 Background save worker started"));
    UE_LOG(LogTemp, Log, TEXT("   Thread ID: %d"), FPlatformTLS::GetCurrentThreadId());

    int32 SavedCount = 0;
    int32 FailedCount = 0;

    // ✅ 프레임별로 JPG 저장
    for (int32 i = 0; i < Frames.Num(); i++)
    {
        const FVideoFrame& Frame = Frames[i];

        // 프레임 유효성 검사
        if (!Frame.FrameTexture || !IsValid(Frame.FrameTexture))
        {
            FailedCount++;
            continue;
        }

        // 파일명: 0001.jpg, 0002.jpg, ...
        FString FileName = FString::Printf(TEXT("%04d.jpg"), i + 1);
        FString FilePath = FPaths::Combine(ClipDir, FileName);

        // JPG 저장
        if (SaveFrameAsJPG_ThreadSafe(Frame.FrameTexture, FilePath, Quality))
        {
            SavedCount++;

            // 진행률 로그 (20% 단위)
            if ((i + 1) % FMath::Max(1, Frames.Num() / 5) == 0)
            {
                float Progress = ((float)(i + 1) / Frames.Num()) * 100.0f;
                UE_LOG(LogTemp, Log, TEXT("  📊 Background save: %.0f%% (%d/%d frames)"),
                    Progress, i + 1, Frames.Num());
            }
        }
        else
        {
            FailedCount++;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("✅ Background save completed!"));
    UE_LOG(LogTemp, Warning, TEXT("   Saved: %d frames"), SavedCount);
    UE_LOG(LogTemp, Warning, TEXT("   Failed: %d frames"), FailedCount);
    UE_LOG(LogTemp, Warning, TEXT("   Path: %s"), *ClipDir);
}

// ═══════════════════════════════════════════════════════════════════════════
// ✅ 새로 추가: SwingClips 폴더 자동 삭제 함수
// ═══════════════════════════════════════════════════════════════════════════
void AWebcamCapture::CleanupSwingClipsFolder()
{
    UE_LOG(LogTemp, Warning, TEXT(""));
    UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════"));
    UE_LOG(LogTemp, Warning, TEXT("🧹 Cleaning up SwingClips folder..."));
    UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════"));

    // ========== Step 1: 경로 구성 ==========
    FString ProjectDir = FPaths::ProjectDir();
    FString SwingClipsDir = FPaths::Combine(ProjectDir, TEXT("Saved"), TEXT("SwingClips"));

    UE_LOG(LogTemp, Warning, TEXT("📁 Target directory: %s"), *SwingClipsDir);

    // ========== Step 2: PlatformFile 획득 ==========
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    // ========== Step 3: 디렉토리 존재 여부 확인 ==========
    if (!PlatformFile.DirectoryExists(*SwingClipsDir))
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ SwingClips directory does not exist (already deleted?)"));
        UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("✅ Directory exists, proceeding with deletion"));

    // ========== Step 4: 디렉토리 내 파일 목록 확인 ==========
    TArray<FString> FoundFiles;
    PlatformFile.FindFiles(FoundFiles, *SwingClipsDir, TEXT("*"));

    UE_LOG(LogTemp, Log, TEXT("📊 Found %d files/directories to delete"), FoundFiles.Num());

    if (FoundFiles.Num() > 0)
    {
        // 파일 목록 출력 (처음 10개만)
        int32 MaxDisplay = FMath::Min(10, FoundFiles.Num());
        for (int32 i = 0; i < MaxDisplay; i++)
        {
            UE_LOG(LogTemp, Log, TEXT("   [%d/%d] %s"), i + 1, FoundFiles.Num(), *FoundFiles[i]);
        }
        if (FoundFiles.Num() > 10)
        {
            UE_LOG(LogTemp, Log, TEXT("   ... and %d more files"), FoundFiles.Num() - 10);
        }
    }

    // ========== Step 5: 디렉토리 재귀적으로 삭제 ==========
    if (PlatformFile.DeleteDirectoryRecursively(*SwingClipsDir))
    {
        UE_LOG(LogTemp, Warning, TEXT("✅ SwingClips folder deleted successfully!"));
        UE_LOG(LogTemp, Warning, TEXT("   📁 Path: %s"), *SwingClipsDir);
        UE_LOG(LogTemp, Warning, TEXT("   📊 Files deleted: %d"), FoundFiles.Num());
        UE_LOG(LogTemp, Warning, TEXT("   💾 Disk space freed"));
        bSwingClipsDeleted = true;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to delete SwingClips folder!"));
        UE_LOG(LogTemp, Error, TEXT("   📁 Path: %s"), *SwingClipsDir);
        UE_LOG(LogTemp, Error, TEXT("   Possible causes:"));
        UE_LOG(LogTemp, Error, TEXT("   - Directory in use by another process"));
        UE_LOG(LogTemp, Error, TEXT("   - Permission denied (insufficient access rights)"));
        UE_LOG(LogTemp, Error, TEXT("   - File locking issue (files still open)"));
        UE_LOG(LogTemp, Error, TEXT("   Recommendation: Check file permissions or manually delete"));
        bSwingClipsDeleted = false;
    }

    UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════"));
    UE_LOG(LogTemp, Warning, TEXT(""));
}

// ========== 3. 스레드 안전 JPG 저장 함수 (static) ==========
bool AWebcamCapture::SaveFrameAsJPG_ThreadSafe(UTexture2D* Texture, const FString& FilePath, int32 Quality)
{
    if (!Texture || !IsValid(Texture))
    {
        return false;
    }

    // ========== Step 1: PlatformData 접근 ==========
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION <= 26
    if (!Texture->PlatformData || Texture->PlatformData->Mips.Num() == 0)
    {
        return false;
    }
    FTexture2DMipMap& Mip = Texture->PlatformData->Mips[0];
#else
    if (!Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.Num() == 0)
    {
        return false;
    }
    FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
#endif

    // ========== Step 2: 픽셀 데이터 읽기 ==========
    void* Data = Mip.BulkData.Lock(LOCK_READ_ONLY);
    if (!Data)
    {
        return false;
    }

    int32 Width = Texture->GetSizeX();
    int32 Height = Texture->GetSizeY();
    int32 TotalPixels = Width * Height;

    // FColor 배열로 변환
    TArray<FColor> Pixels;
    Pixels.SetNum(TotalPixels);
    FMemory::Memcpy(Pixels.GetData(), Data, TotalPixels * sizeof(FColor));

    Mip.BulkData.Unlock();

    // ========== Step 3: ImageWrapper로 JPG 압축 ==========
    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(
        FName("ImageWrapper"));

    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(
        EImageFormat::JPEG);

    if (!ImageWrapper.IsValid())
    {
        return false;
    }

    // ✅ BGRA → RGBA 변환 (최적화)
    TArray<uint8> RawData;
    RawData.SetNum(TotalPixels * 4);

    uint8* Dest = RawData.GetData();
    const FColor* Src = Pixels.GetData();

    for (int32 i = 0; i < TotalPixels; i++)
    {
        *Dest++ = Src->R;
        *Dest++ = Src->G;
        *Dest++ = Src->B;
        *Dest++ = Src->A;
        Src++;
    }

    if (!ImageWrapper->SetRaw(RawData.GetData(), RawData.Num(), Width, Height,
        ERGBFormat::RGBA, 8))
    {
        return false;
    }

    // JPG 압축
    const TArray64<uint8>& CompressedData = ImageWrapper->GetCompressed(Quality);

    // ========== Step 4: 파일로 저장 ==========
    if (!FFileHelper::SaveArrayToFile(CompressedData, *FilePath))
    {
        return false;
    }

    return true;
}

bool AWebcamCapture::SaveFrameAsJPG(UTexture2D* Texture, const FString& FilePath, int32 Quality)
{
    if (!Texture || !IsValid(Texture))
    {
        return false;
    }

    // ========== Step 1: PlatformData 접근 ==========
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION <= 26
    if (!Texture->PlatformData || Texture->PlatformData->Mips.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ No PlatformData"));
        return false;
    }
    FTexture2DMipMap& Mip = Texture->PlatformData->Mips[0];
#else
    if (!Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ No PlatformData"));
        return false;
    }
    FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
#endif

    // ========== Step 2: 픽셀 데이터 읽기 ==========
    void* Data = Mip.BulkData.Lock(LOCK_READ_ONLY);
    if (!Data)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ BulkData.Lock failed"));
        return false;
    }

    // FColor 배열로 변환
    int32 Width = Texture->GetSizeX();
    int32 Height = Texture->GetSizeY();
    int32 TotalPixels = Width * Height;

    TArray<FColor> Pixels;
    Pixels.SetNum(TotalPixels);
    FMemory::Memcpy(Pixels.GetData(), Data, TotalPixels * sizeof(FColor));

    Mip.BulkData.Unlock();

    // ========== Step 3: ImageWrapper로 JPG 압축 ==========
    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(
        FName("ImageWrapper"));

    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(
        EImageFormat::JPEG);

    if (!ImageWrapper.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to create ImageWrapper"));
        return false;
    }

    // 포맷: BGRA8 → RGBA8 변환 필요
    TArray<uint8> RawData;
    RawData.SetNum(TotalPixels * 4);

    for (int32 i = 0; i < TotalPixels; i++)
    {
        RawData[i * 4 + 0] = Pixels[i].R;
        RawData[i * 4 + 1] = Pixels[i].G;
        RawData[i * 4 + 2] = Pixels[i].B;
        RawData[i * 4 + 3] = Pixels[i].A;
    }

    if (!ImageWrapper->SetRaw(RawData.GetData(), RawData.Num(), Width, Height,
        ERGBFormat::RGBA, 8))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ ImageWrapper->SetRaw failed"));
        return false;
    }

    // JPG 압축
    const TArray64<uint8>& CompressedData = ImageWrapper->GetCompressed(Quality);

    // ========== Step 4: 파일로 저장 ==========
    if (!FFileHelper::SaveArrayToFile(CompressedData, *FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to save file: %s"), *FilePath);
        return false;
    }

    return true;
}

bool AWebcamCapture::SaveClipMetadata(const FString& ClipDir, int32 TotalFrames,
    float FPS, int32 Width, int32 Height, float ShotTime)
{
    FString MetadataPath = FPaths::Combine(ClipDir, TEXT("metadata.json"));

    // JSON 문자열 생성
    FString JsonString = FString::Printf(TEXT("{\n"));
    JsonString += FString::Printf(TEXT("  \"clipName\": \"%s\",\n"), *FPaths::GetCleanFilename(ClipDir));
    JsonString += FString::Printf(TEXT("  \"totalFrames\": %d,\n"), TotalFrames);
    JsonString += FString::Printf(TEXT("  \"fps\": %.2f,\n"), FPS);
    JsonString += FString::Printf(TEXT("  \"resolution\": {\n"));
    JsonString += FString::Printf(TEXT("    \"width\": %d,\n"), Width);
    JsonString += FString::Printf(TEXT("    \"height\": %d\n"), Height);
    JsonString += FString::Printf(TEXT("  },\n"));
    JsonString += FString::Printf(TEXT("  \"shotTime\": %.2f,\n"), ShotTime);

    FDateTime Now = FDateTime::Now();
    JsonString += FString::Printf(TEXT("  \"timestamp\": \"%s\"\n"), *Now.ToIso8601());
    JsonString += FString::Printf(TEXT("}\n"));

    // 파일 저장
    if (FFileHelper::SaveStringToFile(JsonString, *MetadataPath))
    {
        UE_LOG(LogTemp, Log, TEXT("✅ Metadata saved: %s"), *MetadataPath);
        return true;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to save metadata"));
        return false;
    }
}

void AWebcamCapture::PlaySwingClipFromPath(const FString& ClipPath, float DelaySeconds)
{
    UE_LOG(LogTemp, Warning, TEXT("🎬 Scheduling clip playback: %s (delay: %.1fs)"),
        *ClipPath, DelaySeconds);


    if (!VideoWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ VideoWidget is not available"));
        return;
    }

    // ✅ 딜레이 타이머 설정
    FTimerDelegate TimerDelegate;
    TimerDelegate.BindUObject(this, &AWebcamCapture::OnPlayClipTimerComplete, ClipPath);

    GetWorldTimerManager().SetTimer(
        PlayClipTimerHandle,
        TimerDelegate,
        DelaySeconds,
        false
    );

    UE_LOG(LogTemp, Log, TEXT("⏰ Timer set: %.1f seconds"), DelaySeconds);
}

void AWebcamCapture::OnPlayClipTimerComplete(FString ClipPath)
{
  //  UE_LOG(LogTemp, Warning, TEXT("⏰ Timer complete, validating clip files..."));

    // ========== Step 1: 파일 개수 확인 ========== ✅
    IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
    int32 FileCount = 0;

    for (int32 i = 1; i <= 152; i++)
    {
        FString FileName = FString::Printf(TEXT("%04d.jpg"), i);
        FString FilePath = FPaths::Combine(ClipPath, FileName);

        if (PF.FileExists(*FilePath))
        {
            FileCount++;
        }
        else
        {
            break;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("   📁 Found %d JPG files in clip directory"), FileCount);

    // ========== Step 2: 최소 파일 수 확인 ========== ✅
    // 30fps × 5초 = 150 프레임
    // 최소 140개 있으면 거의 완료 (93%)
    const int32 MinRequiredFiles = 140;

    if (FileCount < MinRequiredFiles)
    {
        UE_LOG(LogTemp, Warning, TEXT("⏳ Not enough files yet (%d < %d)"), FileCount, MinRequiredFiles);
        UE_LOG(LogTemp, Warning, TEXT("   Progress: %.1f%% - Retrying in 2 seconds..."),
            (float)FileCount / 150.0f * 100.0f);

        // ✅ 2초 후 다시 시도
        FTimerDelegate RetryDelegate;
        RetryDelegate.BindUObject(this, &AWebcamCapture::OnPlayClipTimerComplete, ClipPath);

        GetWorldTimerManager().SetTimer(
            PlayClipTimerHandle,
            RetryDelegate,
            2.0f,
            false
        );
        return;
    }

    // ========== Step 3: 충분한 파일 → 재생 시작 ========== ✅
    UE_LOG(LogTemp, Warning, TEXT("✅ Sufficient files found (%d files)"), FileCount);
    UE_LOG(LogTemp, Warning, TEXT("   Starting playback..."));

   // SettingPlaySwingClip();

}


TArray<FString> AWebcamCapture::GetSavedClipList()
{
    TArray<FString> ClipList;

    FString SwingClipsDir = FPaths::Combine(
        FPaths::ProjectDir(), TEXT("Saved"), TEXT("SwingClips"));

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    // 디렉토리 순회
    class FDirectoryVisitor : public IPlatformFile::FDirectoryVisitor
    {
    public:
        TArray<FString>& Directories;

        FDirectoryVisitor(TArray<FString>& InDirs) : Directories(InDirs) {}

        virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
        {
            if (bIsDirectory)
            {
                Directories.Add(FilenameOrDirectory);
            }
            return true;
        }
    };

    FDirectoryVisitor Visitor(ClipList);
    PlatformFile.IterateDirectory(*SwingClipsDir, Visitor);

    // 날짜 기준 정렬 (최신순)
    ClipList.Sort([](const FString& A, const FString& B)
        {
            return A > B;  // 역순 정렬
        });

    UE_LOG(LogTemp, Log, TEXT("📋 Found %d clips"), ClipList.Num());

    return ClipList;
}

bool AWebcamCapture::DeleteClip(const FString& ClipDirectory)
{
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    if (!PlatformFile.DirectoryExists(*ClipDirectory))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Directory does not exist: %s"), *ClipDirectory);
        return false;
    }

    // 디렉토리 및 모든 파일 삭제
    if (PlatformFile.DeleteDirectoryRecursively(*ClipDirectory))
    {
        UE_LOG(LogTemp, Warning, TEXT("🗑️ Clip deleted: %s"), *ClipDirectory);
        return true;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to delete clip: %s"), *ClipDirectory);
        return false;
    }
}

void AWebcamCapture::CleanupOldClips()
{
    TArray<FString> ClipList = GetSavedClipList();

    // ========== 개수 제한 ==========
    while (ClipList.Num() > MaxStoredClips)
    {
        // 가장 오래된 클립 삭제
        DeleteClip(ClipList.Last());
        ClipList.RemoveAt(ClipList.Num() - 1);

        UE_LOG(LogTemp, Warning, TEXT("🗑️ Deleted old clip (count limit exceeded)"));
    }

    // ========== 용량 제한 ==========
    FString SwingClipsDir = FPaths::Combine(
        FPaths::ProjectDir(), TEXT("Saved"), TEXT("SwingClips"));

    int64 TotalSize = GetDirectorySize(SwingClipsDir);
    int64 MaxSizeBytes = (int64)(MaxStorageSizeGB * 1024 * 1024 * 1024);

    while (TotalSize > MaxSizeBytes && ClipList.Num() > 0)
    {
        // 가장 오래된 클립 삭제
        DeleteClip(ClipList.Last());
        ClipList.RemoveAt(ClipList.Num() - 1);

        TotalSize = GetDirectorySize(SwingClipsDir);

        UE_LOG(LogTemp, Warning, TEXT("🗑️ Deleted old clip (size limit exceeded)"));
    }

    UE_LOG(LogTemp, Log, TEXT("✅ Cleanup complete: %d clips, %.2f GB"),
        ClipList.Num(), TotalSize / (1024.0f * 1024.0f * 1024.0f));
}

int64 AWebcamCapture::GetDirectorySize(const FString& Directory)
{
    int64 TotalSize = 0;

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    class FSizeVisitor : public IPlatformFile::FDirectoryVisitor
    {
    public:
        int64& Size;
        IPlatformFile& Platform;

        FSizeVisitor(int64& InSize, IPlatformFile& InPlatform)
            : Size(InSize), Platform(InPlatform) {}

        virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
        {
            if (!bIsDirectory)
            {
                Size += Platform.FileSize(FilenameOrDirectory);
            }
            else
            {
                Platform.IterateDirectoryRecursively(FilenameOrDirectory, *this);
            }
            return true;
        }
    };

    FSizeVisitor Visitor(TotalSize, PlatformFile);
    PlatformFile.IterateDirectoryRecursively(*Directory, Visitor);

    return TotalSize;
}


// ═══════════════════════════════════════════════════════════════════════════════════════════════
// 📋 Step 1: 더미 스윙 프로세스 시작
// ═══════════════════════════════════════════════════════════════════════════════════════════════

void AWebcamCapture::StartDummySwingProcess()
{
    if (bDummySwingInProgress)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Dummy swing already in progress!"));
        return;
    }

    bDummySwingInProgress = true;
    DummySwingStartTime = GetWorld()->GetTimeSeconds();

    UE_LOG(LogTemp, Warning, TEXT(""));
    UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════════════════════════"));
    UE_LOG(LogTemp, Warning, TEXT("🎬 DUMMY SWING PROCESS STARTED"));
    UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════════════════════════"));
    UE_LOG(LogTemp, Warning, TEXT("📍 Purpose: Buffer warm-up & texture pool stabilization"));
    UE_LOG(LogTemp, Warning, TEXT("   Duration: %.1f seconds"), DUMMY_SWING_DURATION);
    UE_LOG(LogTemp, Warning, TEXT("   Start Time: %.3f"), DummySwingStartTime);
    UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════════════════════════"));
    UE_LOG(LogTemp, Warning, TEXT(""));

    // ✅ 더미 스윙 처리 결과를 수집하기 위한 초기화
    // (내부적으로는 모든 처리를 수행하되, UI 표시는 안 함)
}

// ═══════════════════════════════════════════════════════════════════════════════════════════════
// 📋 Step 2: 더미 스윙 프로세스 진행 중 (Tick에서 호출)
// ═══════════════════════════════════════════════════════════════════════════════════════════════

void AWebcamCapture::ProcessDummySwing(float DeltaTime)
{
    if (!bDummySwingInProgress)
    {
        return;
    }

    // ✅ 진행 상황 파악
    float ElapsedTime = GetWorld()->GetTimeSeconds() - DummySwingStartTime;
    float Progress = ElapsedTime / DUMMY_SWING_DURATION;

    // 📊 주기적 로깅 (0.25초마다)
    static float LastLogTime = 0.0f;
    if (ElapsedTime - LastLogTime >= 0.25f)
    {
        LogDummySwingStatus();
        LastLogTime = ElapsedTime;
    }

    // ✅ 완료 검사
    if (ElapsedTime >= DUMMY_SWING_DURATION)
    {
        CompleteDummySwingProcess();
    }
}

// ═══════════════════════════════════════════════════════════════════════════════════════════════
// 📋 Step 3: 더미 스윙 프로세스 로깅
// ═══════════════════════════════════════════════════════════════════════════════════════════════

void AWebcamCapture::LogDummySwingStatus() const
{
    if (!bDummySwingInProgress || !VideoBufferComponent)
    {
        return;
    }

    float ElapsedTime = GetWorld()->GetTimeSeconds() - DummySwingStartTime;
    float Progress = ElapsedTime / DUMMY_SWING_DURATION;
    int32 ProgressPercent = static_cast<int32>(Progress * 100.0f);

    // ✅ 프로그래스 바 시뮬레이션
    FString ProgressBar = TEXT("[");
    int32 FilledBars = ProgressPercent / 10;
    for (int32 i = 0; i < 10; i++)
    {
        ProgressBar += (i < FilledBars) ? TEXT("█") : TEXT("░");
    }
    ProgressBar += TEXT("]");

    // ✅ 현재 버퍼 상태
    int32 CurrentFrameCount = VideoBufferComponent->GetTotalFrameCount();
    float BufferDuration = VideoBufferComponent->GetBufferedDuration();
    float MemoryUsageMB = VideoBufferComponent->GetEstimatedMemoryUsageMB();

    UE_LOG(LogTemp, Log, TEXT("🎬 Dummy Swing Progress: %s %d%% (%.2f/%.1f sec)"),
        *ProgressBar, ProgressPercent, ElapsedTime, DUMMY_SWING_DURATION);
    UE_LOG(LogTemp, Log, TEXT("   📦 Buffer: %d frames, %.2f sec duration, %.1f MB memory"),
        CurrentFrameCount, BufferDuration, MemoryUsageMB);
    UE_LOG(LogTemp, Log, TEXT("   🎥 Capture Status: %s, FPS: %.1f"),
        bIsCapturing ? TEXT("ACTIVE") : TEXT("INACTIVE"),
        CurrentFrameCount > 0 ? CurrentFrameCount / (BufferDuration > 0.0f ? BufferDuration : 1.0f) : 0.0f);
}

// ═══════════════════════════════════════════════════════════════════════════════════════════════
// 📋 Step 4: 더미 스윙 프로세스 완료
// ═══════════════════════════════════════════════════════════════════════════════════════════════

void AWebcamCapture::CompleteDummySwingProcess()
{
    if (!bDummySwingInProgress)
    {
        return;
    }

    float ElapsedTime = GetWorld()->GetTimeSeconds() - DummySwingStartTime;

    UE_LOG(LogTemp, Warning, TEXT(""));
    UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════════════════════════"));
    UE_LOG(LogTemp, Warning, TEXT("✅ DUMMY SWING PROCESS COMPLETED"));
    UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════════════════════════"));
    UE_LOG(LogTemp, Warning, TEXT("   Duration: %.3f / %.1f seconds"), ElapsedTime, DUMMY_SWING_DURATION);

    // ✅ 최종 버퍼 상태 확인
    if (VideoBufferComponent)
    {
        int32 FinalFrameCount = VideoBufferComponent->GetTotalFrameCount();
        float FinalDuration = VideoBufferComponent->GetBufferedDuration();
        float FinalMemoryUsageMB = VideoBufferComponent->GetEstimatedMemoryUsageMB();
        int32 MaxBufferSize = VideoBufferComponent->GetMaxBufferSize();

        UE_LOG(LogTemp, Warning, TEXT(""));
        UE_LOG(LogTemp, Warning, TEXT("📊 Final Buffer State:"));
        UE_LOG(LogTemp, Warning, TEXT("   Frame Count: %d / %d (%.1f%%)"),
            FinalFrameCount, MaxBufferSize,
            MaxBufferSize > 0 ? (FinalFrameCount * 100.0f / MaxBufferSize) : 0.0f);
        UE_LOG(LogTemp, Warning, TEXT("   Duration: %.2f seconds"), FinalDuration);
        UE_LOG(LogTemp, Warning, TEXT("   Memory Usage: %.1f MB"), FinalMemoryUsageMB);

        // ✅ 텍스처 풀 상태 확인
        int32 TexturePoolCount = TexturePool.Num();
        int32 ValidTextures = 0;
        for (const auto& Tex : TexturePool)
        {
            if (IsValid(Tex))
            {
                ValidTextures++;
            }
        }

        UE_LOG(LogTemp, Warning, TEXT("   Texture Pool: %d / %d valid"), ValidTextures, TexturePoolCount);

        // ✅ 워밍업 성과 요약
        UE_LOG(LogTemp, Warning, TEXT(""));
        UE_LOG(LogTemp, Warning, TEXT("🔧 Warm-up Results:"));
        UE_LOG(LogTemp, Warning, TEXT("   ✅ Buffer fully initialized and stabilized"));
        UE_LOG(LogTemp, Warning, TEXT("   ✅ Texture pool index rotation completed"));
        UE_LOG(LogTemp, Warning, TEXT("   ✅ JPG encoding and I/O systems warmed up"));
        UE_LOG(LogTemp, Warning, TEXT("   ✅ Ready for production swing recording"));
    }

    UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════════════════════════"));
    UE_LOG(LogTemp, Warning, TEXT(""));

    // ✅ 플래그 초기화
    bDummySwingInProgress = false;
    DummySwingStartTime = 0.0f;
}


bool AWebcamCapture::UpdateTexture2DFromRenderTargetAsync(UTexture2D* TargetTexture, UTextureRenderTarget2D* RenderTarget)
{
    SCOPE_CYCLE_COUNTER(STAT_WebcamUpdateTextureAsync);
    // 1. 기초 유효성 검사 (반드시 Game Thread에서 수행)
    if (!IsInGameThread() || !TargetTexture || !RenderTarget || bIsAsyncReading)
    {
        return false;
    }

    FTextureRenderTargetResource* RTResource = RenderTarget->GameThread_GetRenderTargetResource();
    if (!RTResource) return false;

    // 렌더링 스레드에서 UObject에 직접 접근하는 것을 피하기 위해 필요한 정보를 복사
    FTextureRHIRef RenderTargetTextureRHI = RTResource->GetRenderTargetTexture();
    const int32 Width = RenderTarget->SizeX;
    const int32 Height = RenderTarget->SizeY;
    const float CapturedTime = CurrentCaptureTime; // 현재 타임스탬프 복사

    bIsAsyncReading = true;

    // ✅ [해결책] 여기서 변수를 정의합니다!
    TWeakObjectPtr<UTexture2D> WeakTargetTexture = TargetTexture;

    // 2. 렌더링 스레드 명령 예약
    ENQUEUE_RENDER_COMMAND(ReadSurfaceCommand)(
        [RenderTargetTextureRHI, WeakTargetTexture, this, Width, Height, CapturedTime](FRHICommandListImmediate& RHICmdList)
        {
            TArray<FColor> LocalBuffer;
            RHICmdList.ReadSurfaceData(
                RenderTargetTextureRHI,
                FIntRect(0, 0, Width, Height),
                LocalBuffer,
                FReadSurfaceDataFlags()
            );

            // 3. 게임 스레드로 복귀 (여기서도 WeakTargetTexture를 전달)
            AsyncTask(ENamedThreads::GameThread, [this, LocalBuffer = MoveTemp(LocalBuffer), WeakTargetTexture, CapturedTime]()
            {
                // ✅ 약참조가 유효한지 확인하여 크래시 방지
                if (WeakTargetTexture.IsValid() && IsValid(WeakTargetTexture.Get()) && LocalBuffer.Num() > 0)
                {
                    UTexture2D* Tex = WeakTargetTexture.Get();
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION <= 26
                    FTexture2DMipMap& Mip = Tex->PlatformData->Mips[0];
#else
                    FTexture2DMipMap& Mip = Tex->GetPlatformData()->Mips[0];
#endif
                    void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
                    if (Data)
                    {
                        FMemory::Memcpy(Data, LocalBuffer.GetData(), LocalBuffer.Num() * sizeof(FColor));
                        Mip.BulkData.Unlock();
                        Tex->UpdateResource();

                        if (VideoBufferComponent)
                        {
                            VideoBufferComponent->AddFrame(Tex, CapturedTime);
                        }
                    }
                }

                // 작업 완료 후 플래그 해제
                bIsAsyncReading = false;
            });
        }
    );

    return true;
}


void AWebcamCapture::SettingPlaySwingClip()
{
    if (VideoWidget && IsValid(VideoWidget))
    {
        FString ProjectDir = FPaths::ProjectDir();
        FString ClipDir = FPaths::Combine(ProjectDir, TEXT("Saved"), TEXT("SwingClips"));

        VideoWidget->PlaySwingClipFromDirectory(*ClipDir);
        UE_LOG(LogTemp, Log, TEXT("------- SettingPlaySwingClip() ------    Path: %s"), *ClipDir);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ VideoWidget is invalid"));
    }
}