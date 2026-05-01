#include "SwingVideoWidget.h"
#include "WebcamCapture.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Engine.h"
#include "Kismet/KismetRenderingLibrary.h"

#include "ImageUtils.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"  // ? IFileManager::Get().FindFiles() 사용을 위해 추가

// ? 주요 수정사항:
// 1. NativeTick에서 강화된 유효성 검사
// 2. BeginDestroy에서 완전한 참조 정리
// 3. UpdateLiveFeed에서 안전한 MediaPlayer 접근
// 4. 타이머 정리 추가

USwingVideoWidget::USwingVideoWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    CurrentVideoMode = EVideoMode::LiveFeed;
    PlaybackSpeed = 1.0f;
    bLoopPlayback = false;
    FrameRate = 30.0f;  // ? 30fps로 변경 (YUY2 포맷)
}

void USwingVideoWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 버튼 이벤트 바인딩
    if (PlayButton)
    {
        PlayButton->OnClicked.AddDynamic(this, &USwingVideoWidget::OnPlayButtonClicked);
    }

    if (SwingPauseButton)
    {
        SwingPauseButton->OnClicked.AddDynamic(this, &USwingVideoWidget::OnPauseButtonClicked);
    }

    if (SwingStopButton)
    {
        SwingStopButton->OnClicked.AddDynamic(this, &USwingVideoWidget::OnStopButtonClicked);
    }

    if (SwingLiveFeedButton)
    {
        SwingLiveFeedButton->OnClicked.AddDynamic(this, &USwingVideoWidget::OnLiveFeedButtonClicked);
    }

    if (CloseButton)
    {
        CloseButton->OnClicked.AddDynamic(this, &USwingVideoWidget::CloseSwingMotion);
    }

    if (ProgressSlider)
    {
        ProgressSlider->OnValueChanged.AddDynamic(this, &USwingVideoWidget::OnProgressSliderChanged);
    }

    UE_LOG(LogTemp, Warning, TEXT("========== SwingVideoWidget NativeConstruct =========="));

    // ========== SwingRT 생성 ==========
    UWorld* World = GetWorld();
    if (!World || !IsValid(World))
    {
        UE_LOG(LogTemp, Error, TEXT("? World is invalid in NativeConstruct"));
    }


    InitBlitSystem();


    // ? 삭제: VideoDisplay에 SwingRT 초기 설정하지 않음
    // (LiveFeed 모드가 기본이므로)
    /*
    if (VideoDisplay && SwingRT)
    {
        FSlateBrush Brush;
        Brush.SetResourceObject(SwingRT);
        Brush.ImageSize = FVector2D(640, 480);
        VideoDisplay->SetBrush(Brush);
    }
    */

    // ? 초기 모드 설정
    //SwitchToLiveFeed();
    SwitchToSwingPlayback();
}


void USwingVideoWidget::InitBlitSystem()
{
    if (BlitMaterial)
    {
        // 부모(Outer)를 이 위젯(this)으로 설정하여 생성
        SwingMID = UMaterialInstanceDynamic::Create(BlitMaterial, this);

        if (SwingMID)
        {
            UE_LOG(LogTemp, Log, TEXT("? SwingMID Created Successfully"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("? BlitMaterial is not assigned!"));
    }
}



void USwingVideoWidget::BlitFrameToRT(UTexture2D* FrameTex)
{
    if (!IsValid(FrameTex)) return;

    UWorld* World = GetWorld();

    // 1. SwingRT가 없으면 생성 (최초 1회 실행)
    if (!SwingRT || !IsValid(SwingRT))
    {
        // 해상도는 원본 영상에 맞게 조절하세요 (예: 1920x1080)
        SwingRT = UKismetRenderingLibrary::CreateRenderTarget2D(World, 640, 480, RTF_RGBA8);
        SwingRT->ClearColor = FLinearColor::Blue; // 테스트용: 성공하면 화면이 파랗게 변해야 함

        if (SwingRT)
        {
            // 검은색 대신 파란색으로 클리어해서 연결 확인용으로 사용
            SwingRT->ClearColor = FLinearColor::Blue;
            UE_LOG(LogTemp, Warning, TEXT("? SwingRT 생성 완료 (Blue 초기화)"));
        }
    }

    // 2. SwingMID가 없으면 생성 (이전 가이드 참고)
    if (!SwingMID && BlitMaterial)
    {
        SwingMID = UMaterialInstanceDynamic::Create(BlitMaterial, this);
        UE_LOG(LogTemp, Warning, TEXT("? SwingRT 생성실패  ( 초기화)"));
    }

    if (SwingMID && SwingRT)
    {
        // 3. 머티리얼에 텍스처 전달 및 그리기
        SwingMID->SetTextureParameterValue(TEXT("InputTex"), FrameTex);
        UKismetRenderingLibrary::DrawMaterialToRenderTarget(World, SwingRT, SwingMID);

        // 4. ?? 가장 중요한 단계: RenderTarget을 UI 이미지에 연결
        // SetBrushFromTexture 대신 SetBrushResourceObject를 사용하세요.
        if (VideoDisplay)
        {
            VideoDisplay->SetBrushResourceObject(SwingRT);
            // 브러시 크기가 0이 되지 않도록 크기 명시
            VideoDisplay->SetBrushSize(FVector2D(640, 480));
        }
    }

    // 디버그 로그
    static int32 BlitCount = 0;
    if (++BlitCount % 60 == 0) {
        UE_LOG(LogTemp, Log, TEXT("? Rendering SwingRT to UI Image (Count: %d)"), BlitCount);
    }
}



// ? 수정: 강화된 유효성 검사를 추가한 NativeTick
void USwingVideoWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    // ? 이 코드를 맨 처음에 추가!
    if (GetVisibility() == ESlateVisibility::Collapsed)
    {
        return;  // ← 이 한 줄이 크래시 방지!
    }



    // ? 기본 유효성 검사 (기존)
    if (!IsValid(this) || !VideoDisplay || !IsValid(VideoDisplay))
    {
        return;
    }

    if (CurrentVideoMode == EVideoMode::LiveFeed)
    {
        if (!WebcamCaptureRef.IsValid())
        {
            bIsPlaying = false;
            bIsPaused = false;
            return;
        }
    }



    // 모드별 처리
    switch (CurrentVideoMode)
    {
    case EVideoMode::LiveFeed:
        HandleLiveFeedMode(InDeltaTime);
        break;

    case EVideoMode::SwingPlayback:
        // ? 클립 재생 또는 메모리 재생 구분
       // if (!CurrentClipDirectory.IsEmpty() && ClipFramePaths.Num() > 0)
    {
        HandleClipPlaybackMode(InDeltaTime);  // ? 새로 추가!

        //  HandleSwingPlaybackMode(InDeltaTime);  // 기존 메모리 재생
    }
    break;

    case EVideoMode::FilePlayback:
        HandleFilePlaybackMode(InDeltaTime);
        break;
    }

    // ? 비디오 디스플레이 업데이트
    UpdateVideoDisplay();

    // ? UI 업데이트
    UpdateUI();


}


// ? 수정: 완전한 참조 정리
void USwingVideoWidget::BeginDestroy()
{
    UE_LOG(LogTemp, Warning, TEXT("???????????????????????????????????????????????????????????"));
    UE_LOG(LogTemp, Warning, TEXT("?? SwingVideoWidget::BeginDestroy - Starting cleanup"));
    UE_LOG(LogTemp, Warning, TEXT("???????????????????????????????????????????????????????????"));

    // ???????????????????????????????????????????????????????????????????????????
    // Step 0: 상태 플래그 즉시 초기화 (? 새로운 단계!)
    // ???????????????????????????????????????????????????????????????????????????
    bIsPlaying = false;
    bIsPaused = false;
    CurrentVideoMode = EVideoMode::LiveFeed;  // 안전한 모드로 전환

    bAsyncPreloadInProgress = false;
    bClipFinishBroadcasted = false;

    UE_LOG(LogTemp, Log, TEXT("? Step 0: State flags cleared immediately"));

    // ???????????????????????????????????????????????????????????????????????????
    // Step 0.5: VideoBuffer와 동기화 (? 새로운 단계! PATCH 3)
    // ???????????????????????????????????????????????????????????????????????????
    if (WebcamCaptureRef.IsValid())
    {
        AWebcamCapture* Webcam = Cast<AWebcamCapture>(WebcamCaptureRef.Get());

        // ? FIX: VideoBuffer → VideoBufferComponent
        if (Webcam && Webcam->VideoBufferComponent)
        {
            UE_LOG(LogTemp, Log, TEXT("  → Notifying VideoBuffer of widget destruction"));

            // VideoBuffer의 리플레이 상태 해제
            Webcam->VideoBufferComponent->SetIsReplaying(false);

            // VideoBuffer 정리
            Webcam->VideoBufferComponent->ClearBuffer();

            UE_LOG(LogTemp, Log, TEXT("  ? VideoBuffer synchronized"));
        }
        else if (Webcam)
        {
            UE_LOG(LogTemp, Warning, TEXT("  ?? VideoBufferComponent is null"));
        }
    }

    // ???????????????????????????????????????????????????????????????????????????
    // Step 1: 타이머 정리 (? 새로운 추가! PATCH 3)
    // ???????????????????????????????????????????????????????????????????????????
    if (GetWorld())
    {
        UE_LOG(LogTemp, Log, TEXT("  → Clearing all timers for this widget"));
        GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
        UE_LOG(LogTemp, Log, TEXT("  ? All timers cleared"));
    }

    // ???????????????????????????????????????????????????????????????????????????
    // Step 2: UI 디스플레이 정리 (원본과 동일)
    // ???????????????????????????????????????????????????????????????????????????
    if (VideoDisplay && IsValid(VideoDisplay))
    {
        UE_LOG(LogTemp, Log, TEXT("  → Clearing VideoDisplay brush..."));

        FSlateBrush EmptyBrush;
        VideoDisplay->SetBrush(EmptyBrush);

        // 삼중 정리
        VideoDisplay->SetBrushFromMaterial(nullptr);
        VideoDisplay->SetBrushFromTexture(nullptr);
        VideoDisplay->SetBrushResourceObject(nullptr);

        VideoDisplay = nullptr;
        UE_LOG(LogTemp, Log, TEXT("  ? VideoDisplay cleared"));
    }

    // ???????????????????????????????????????????????????????????????????????????
    // Step 3: WebcamCapture 참조 정리 (원본과 동일)
    // ???????????????????????????????????????????????????????????????????????????
    if (WebcamCaptureRef.IsValid())
    {
        UE_LOG(LogTemp, Log, TEXT("  → Clearing WebcamCaptureRef..."));
        WebcamCaptureRef.Reset();
        UE_LOG(LogTemp, Log, TEXT("  ? WebcamCaptureRef cleared"));
    }

    // ???????????????????????????????????????????????????????????????????????????
    // Step 4: 파일 재생 리소스 정리 (원본과 동일)
    // ???????????????????????????????????????????????????????????????????????????
    UE_LOG(LogTemp, Log, TEXT("  → Clearing file playback resources..."));

    if (FileMediaPlayer && IsValid(FileMediaPlayer))
    {
        if (FileMediaPlayer->IsPlaying())
        {
            FileMediaPlayer->Close();
        }
        FileMediaPlayer = nullptr;
        UE_LOG(LogTemp, Log, TEXT("    ? FileMediaPlayer closed"));
    }

    if (FileMediaSource)
    {
        FileMediaSource = nullptr;
        UE_LOG(LogTemp, Log, TEXT("    ? FileMediaSource cleared"));
    }

    if (FileMediaTexture)
    {
        FileMediaTexture = nullptr;
        UE_LOG(LogTemp, Log, TEXT("    ? FileMediaTexture cleared"));
    }

    // ???????????????????????????????????????????????????????????????????????????
    // Step 5: 렌더 타겟 및 머티리얼 정리 (원본과 동일)
    // ???????????????????????????????????????????????????????????????????????????
    UE_LOG(LogTemp, Log, TEXT("  → Clearing render target and material..."));

    if (SwingRT && IsValid(SwingRT))
    {
        SwingRT = nullptr;
        UE_LOG(LogTemp, Log, TEXT("    ? SwingRT cleared"));
    }

    if (SwingMID && IsValid(SwingMID))
    {
        SwingMID = nullptr;
        UE_LOG(LogTemp, Log, TEXT("    ? SwingMID cleared"));
    }

    // ???????????????????????????????????????????????????????????????????????????
    // Step 6: 프레임 캐시 정리 (원본과 동일)
    // ???????????????????????????????????????????????????????????????????????????
    UE_LOG(LogTemp, Log, TEXT("  → Clearing frame cache pool..."));

    int32 ClearedCacheCount = 0;
    for (FFrameCache& Cache : FrameCachePool)
    {
        if (Cache.Texture && IsValid(Cache.Texture))
        {
            Cache.Texture->MarkAsGarbage();
            ClearedCacheCount++;
        }
    }
    FrameCachePool.Empty();
    UE_LOG(LogTemp, Log, TEXT("    ? FrameCachePool cleared (%d items)"), ClearedCacheCount);

    // ???????????????????????????????????????????????????????????????????????????
    // Step 7: 로드된 프레임 텍스처 정리 (원본과 동일)
    // ???????????????????????????????????????????????????????????????????????????
    UE_LOG(LogTemp, Log, TEXT("  → Clearing loaded frame textures..."));

    int32 ClearedFrameCount = 0;
    for (UTexture2D* FrameTex : LoadedFrameTextures)
    {
        if (FrameTex && IsValid(FrameTex))
        {
            FrameTex->RemoveFromRoot();     // ? AddToRoot() 해제 필수
            FrameTex->MarkAsGarbage();
            ClearedFrameCount++;
        }
    }
    LoadedFrameTextures.Empty();
    bUseTextureCache = false;
    UE_LOG(LogTemp, Log, TEXT("    ? LoadedFrameTextures cleared (%d items)"), ClearedFrameCount);

    // ???????????????????????????????????????????????????????????????????????????
    // Step 8: Rooted 텍스처 정리 (원본과 동일)
    // ???????????????????????????????????????????????????????????????????????????
    UE_LOG(LogTemp, Log, TEXT("  → Unrooting swing textures..."));

    UnrootSwingTextures();
    int32 ClearedRootedCount = RootedSwingTextures.Num();
    UE_LOG(LogTemp, Log, TEXT("    ? RootedSwingTextures unrooted and cleared (%d items)"), ClearedRootedCount);

    // ???????????????????????????????????????????????????????????????????????????
    // Step 9: 스윙 프레임 데이터 정리 (원본과 동일)
    // ???????????????????????????????????????????????????????????????????????????
    UE_LOG(LogTemp, Log, TEXT("  → Clearing swing frames..."));

    int32 ClearedSwingFrames = SwingFrames.Num();
    SwingFrames.Empty();
    UE_LOG(LogTemp, Log, TEXT("    ? SwingFrames cleared (%d frames)"), ClearedSwingFrames);

    // ???????????????????????????????????????????????????????????????????????????
    // Step 10: 캐시 변수 정리 (원본과 동일)
    // ???????????????????????????????????????????????????????????????????????????
    UE_LOG(LogTemp, Log, TEXT("  → Clearing cache variables..."));

    LastSetTexture = nullptr;
    NextFrameTexture = nullptr;
    LastFrameTextureIndex = -1;
    CurrentClipDirectory = TEXT("");
    ClipFramePaths.Empty();

    UE_LOG(LogTemp, Log, TEXT("    ? Cache variables cleared"));

    // ???????????????????????????????????????????????????????????????????????????
    // Step 11: 버튼 이벤트 바인딩 해제 (원본과 동일)
    // ???????????????????????????????????????????????????????????????????????????
    UE_LOG(LogTemp, Log, TEXT("  → Unbinding button events..."));

    if (PlayButton && IsValid(PlayButton))
    {
        PlayButton->OnClicked.RemoveAll(this);
    }
    if (SwingPauseButton && IsValid(SwingPauseButton))
    {
        SwingPauseButton->OnClicked.RemoveAll(this);
    }
    if (SwingStopButton && IsValid(SwingStopButton))
    {
        SwingStopButton->OnClicked.RemoveAll(this);
    }
    if (SwingLiveFeedButton && IsValid(SwingLiveFeedButton))
    {
        SwingLiveFeedButton->OnClicked.RemoveAll(this);
    }
    if (CloseButton && IsValid(CloseButton))
    {
        CloseButton->OnClicked.RemoveAll(this);
    }
    if (ProgressSlider && IsValid(ProgressSlider))
    {
        ProgressSlider->OnValueChanged.RemoveAll(this);
    }

    UE_LOG(LogTemp, Log, TEXT("    ? Button events unbound"));

    // ???????????????????????????????????????????????????????????????????????????
    // Step 12: 상태 변수 최종 초기화 (원본과 동일)
    // ???????????????????????????????????????????????????????????????????????????
    UE_LOG(LogTemp, Log, TEXT("  → Finalizing state variables..."));

    CurrentFrameIndex = 0;
    CurrentPlaybackTime = 0.0f;
    TotalPlaybackDuration = 0.0f;
    PlaybackTimerAccumulator = 0.0f;
    LastCalculatedFrameIndex = 0;
    bHasEverSetSwingBrush = false;

    UE_LOG(LogTemp, Log, TEXT("    ? State variables finalized"));

    // ???????????????????????????????????????????????????????????????????????????
    // Step 13: 부모 클래스 정리 (원본과 동일)
    // ???????????????????????????????????????????????????????????????????????????
    Super::BeginDestroy();

    UE_LOG(LogTemp, Warning, TEXT("???????????????????????????????????????????????????????????"));
    UE_LOG(LogTemp, Warning, TEXT("? SwingVideoWidget::BeginDestroy - Cleanup complete"));
    UE_LOG(LogTemp, Warning, TEXT("???????????????????????????????????????????????????????????"));
}

void USwingVideoWidget::SetSwingFrames(const TArray<FVideoFrame>& Frames)
{
    SwingFrames = Frames;

    // ? 기본 검증
    if (SwingFrames.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("?? Empty swing frames array"));
        return;
    }

    // ? 타임스탬프 기반 분석
    float FirstTimestamp = SwingFrames[0].Timestamp;
    float LastTimestamp = SwingFrames.Last().Timestamp;
    float ActualDuration = LastTimestamp - FirstTimestamp;

    // ? 실제 FPS 추정
    float EstimatedFPS = ActualDuration > 0.0f ? (SwingFrames.Num() - 1) / ActualDuration : 0.0f;

    // ? 상세 로그
    UE_LOG(LogTemp, Warning, TEXT("========================================"));
    UE_LOG(LogTemp, Warning, TEXT("?? Swing Frames Loaded"));
    UE_LOG(LogTemp, Warning, TEXT("========================================"));
    UE_LOG(LogTemp, Warning, TEXT("   Total Frames: %d"), SwingFrames.Num());
    UE_LOG(LogTemp, Warning, TEXT("   Time Range: %.2f - %.2f seconds"), FirstTimestamp, LastTimestamp);
    UE_LOG(LogTemp, Warning, TEXT("   Actual Duration: %.2f seconds"), ActualDuration);
    UE_LOG(LogTemp, Warning, TEXT("   Estimated FPS: %.1f"), EstimatedFPS);
    UE_LOG(LogTemp, Warning, TEXT("   Widget FPS Setting: %.0f"), FrameRate);

    // ? FPS 자동 조정 (합리적 범위 내)
    if (EstimatedFPS >= 15.0f && EstimatedFPS <= 120.0f)
    {
        // ? 추정 FPS와 현재 설정 차이가 5fps 이상이면 자동 조정
        if (FMath::Abs(EstimatedFPS - FrameRate) > 5.0f)
        {
            float OldFPS = FrameRate;
            FrameRate = EstimatedFPS;

            UE_LOG(LogTemp, Warning, TEXT("   ?? Auto-adjusted FPS: %.0f → %.1f"), OldFPS, FrameRate);
            UE_LOG(LogTemp, Warning, TEXT("      This ensures accurate playback speed!"));
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("   ? FPS Match - Playback will be accurate"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("   ?? Unusual FPS: %.1f (keeping %.0f fps)"),
            EstimatedFPS, FrameRate);
    }

    UE_LOG(LogTemp, Warning, TEXT("========================================"));

    CurrentFrameIndex = 0;
    CurrentPlaybackTime = 0.0f;

    if (BlitMaterial)
    {
        // UKismetRenderingLibrary 대신 UMaterialInstanceDynamic::Create를 사용합니다.
        SwingMID = UMaterialInstanceDynamic::Create(BlitMaterial, this);
        UE_LOG(LogTemp, Log, TEXT("? SwingMID Created Successfully"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("? BlitMaterial is null!"));
    }

    CalculatePlaybackDuration();

    // ? 추가: 프레임 유효성 체크
    //ValidateSwingFramesData();

    // ? [추가 필수 로직] 하얀 화면 방지를 위해 즉시 0번 프레임 Blit
    if (SwingFrames.Num() > 0 && SwingFrames[0].FrameTexture)
    {
        CurrentFrameIndex = 0;
        CurrentPlaybackTime = 0.0f;
        // RenderTarget을 즉시 첫 프레임으로 채웁니다.
        BlitFrameToRT(SwingFrames[0].FrameTexture);
    }

}


void USwingVideoWidget::SwitchToLiveFeed()
{
    UE_LOG(LogTemp, Warning, TEXT("???????????????????????????????????????"));
    UE_LOG(LogTemp, Warning, TEXT("?? Switching to LiveFeed mode"));
    UE_LOG(LogTemp, Warning, TEXT("???????????????????????????????????????"));

    // ? 재생 중이면 정지
    if (bIsPlaying || bIsPaused)
    {
        UE_LOG(LogTemp, Log, TEXT("   Stopping current playback..."));
        StopSwingVideo();
    }

    CurrentVideoMode = EVideoMode::LiveFeed;
    UE_LOG(LogTemp, Log, TEXT("   Mode changed to: LiveFeed"));

    // ? 추가: VideoDisplay 브러시 초기화 (SwingRT 제거)
    if (VideoDisplay && IsValid(VideoDisplay))
    {
        FSlateBrush EmptyBrush;
        VideoDisplay->SetBrush(EmptyBrush);
        UE_LOG(LogTemp, Log, TEXT("   ??? Cleared VideoDisplay brush"));
    }

    // ? 컨트롤 박스 숨기기
    if (ControlsBox)
    {
        ControlsBox->SetVisibility(ESlateVisibility::Collapsed);
        UE_LOG(LogTemp, Log, TEXT("   Controls hidden"));
    }

    // ? 상태 텍스트 업데이트
    if (StatusText)
    {
        StatusText->SetText(FText::FromString(TEXT("?? Live Stream")));
    }

    // ? MediaPlayer 재시작 확인
    if (WebcamCaptureRef.IsValid() && WebcamCaptureRef->MediaPlayer)
    {
        UMediaPlayer* MediaPlayer = WebcamCaptureRef->MediaPlayer;

        if (IsValid(MediaPlayer))
        {
            if (!MediaPlayer->IsPlaying() && !MediaPlayer->IsPreparing())
            {
                UE_LOG(LogTemp, Warning, TEXT("   ?? Restarting MediaPlayer..."));

                if (WebcamCaptureRef->WebcamSource && IsValid(WebcamCaptureRef->WebcamSource))
                {
                    MediaPlayer->OpenSource(WebcamCaptureRef->WebcamSource);
                    UE_LOG(LogTemp, Warning, TEXT("   ? MediaPlayer restarted"));
                }
            }
            else
            {
                UE_LOG(LogTemp, Log, TEXT("   ?? MediaPlayer already playing"));
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("   ? WebcamCaptureRef or MediaPlayer is invalid!"));
    }

    UE_LOG(LogTemp, Warning, TEXT("? Switched to live feed mode"));
    UE_LOG(LogTemp, Warning, TEXT("???????????????????????????????????????"));
}

void USwingVideoWidget::SwitchToSwingPlayback()
{
    CurrentVideoMode = EVideoMode::SwingPlayback;

    // ? 캐시/상태 리셋
    LastFrameTextureIndex = INDEX_NONE;
    LastSetTexture = nullptr;
    bHasEverSetSwingBrush = false;

    // ? 이전 LiveFeed 잔상 제거를 위해 브러시 초기화
    if (VideoDisplay && IsValid(VideoDisplay))
    {
        FSlateBrush Empty;
        VideoDisplay->SetBrush(Empty);
    }

    if (SwingFrames.Num() <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("?? SwitchToSwingPlayback: SwingFrames is empty"));
        return;
    }

    // ──────────────────────────────────────────────────────────
    // ? 수정: 모드 전환 시 항상 0번 프레임으로 세팅
    // ──────────────────────────────────────────────────────────
    CurrentFrameIndex = 0;
    CurrentPlaybackTime = 0.0f;
    PlaybackTimerAccumulator = 0.0f;

    // 첫 프레임을 즉시 그려서 라이브 화면이 남아보이는 문제 방지
    SetVideoFrame(SwingFrames[CurrentFrameIndex]);

    UE_LOG(LogTemp, Log, TEXT("? Switched to Swing Playback - Reset to Frame 0"));
}

// ============================================================================
//  PlaySwingVideo 함수 개선
// ============================================================================

void USwingVideoWidget::PlaySwingVideo()
{
    UE_LOG(LogTemp, Warning, TEXT("▶? PlaySwingVideo() called"));

    // ? 1?? 클립 재생 모드인 경우
    if (!CurrentClipDirectory.IsEmpty() && ClipFramePaths.Num() > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("??? Playing Clip Mode"));
        UE_LOG(LogTemp, Log, TEXT("   Total Frames: %d"), ClipFramePaths.Num());
        UE_LOG(LogTemp, Log, TEXT("   FPS: %.1f"), ClipFPS);

        CurrentFrameIndex = 0;
        CurrentPlaybackTime = 0.0f;
        PlaybackTimerAccumulator = 0.0f;
        bIsPlaying = true;
        bIsPaused = false;
        bClipFinishBroadcasted = false;
        CurrentVideoMode = EVideoMode::SwingPlayback;  // ? 클립도 SwingPlayback 모드 사용

        UE_LOG(LogTemp, Warning, TEXT("? Clip Playback Ready"));
        return;
    }

    // ? 2?? 스윙 프레임 재생 모드인 경우
    if (CurrentVideoMode != EVideoMode::SwingPlayback)
    {
        // 만약 Live 모드에서 바로 Play를 눌렀다면 모드 전환을 먼저 수행
        SwitchToSwingPlayback();
    }

    if (SwingFrames.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("? PlaySwingVideo: No frames loaded"));
        return;
    }

    // ? 이동: 재생 직전에 검증
    ValidateSwingFramesData();  // ← 여기로 이동!

    // ──────────────────────────────────────────────────────────
    // ? 수정: 재생 시작 시 0번 프레임 및 타이머 초기화 보장
    // ──────────────────────────────────────────────────────────
    CurrentFrameIndex = 0;
    CurrentPlaybackTime = 0.0f;
    PlaybackTimerAccumulator = 0.0f;
    bIsPlaying = true;
    bIsPaused = false;
    bClipFinishBroadcasted = false;
    // 첫 프레임 강제 세팅
    SetVideoFrame(SwingFrames[0]);
    bHasEverSetSwingBrush = true;

    UE_LOG(LogTemp, Warning, TEXT("▶? Swing Playback Started from Frame 0"));
    UE_LOG(LogTemp, Log, TEXT("   Total Frames: %d"), SwingFrames.Num());
    UE_LOG(LogTemp, Log, TEXT("   Duration: %.2f seconds"), TotalPlaybackDuration);
}


// ============================================================================
// PauseSwingVideo 함수
// ============================================================================

void USwingVideoWidget::PauseSwingVideo()
{
    if (!bIsPlaying)
    {
        UE_LOG(LogTemp, Warning, TEXT("?? No playback in progress"));
        return;
    }

    bIsPaused = true;

    // ? 현재 모드에 따른 상세 로그
    if (!CurrentClipDirectory.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("?? Clip paused"));
        UE_LOG(LogTemp, Log, TEXT("   Current Frame: %d / %d"), CurrentClipFrameIndex, ClipFramePaths.Num());
        UE_LOG(LogTemp, Log, TEXT("   Time: %.2f seconds"), CurrentPlaybackTime);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("?? Swing video paused"));
        UE_LOG(LogTemp, Log, TEXT("   Current Frame: %d / %d"), CurrentFrameIndex, SwingFrames.Num());
        UE_LOG(LogTemp, Log, TEXT("   Time: %.2f / %.2f seconds"), CurrentPlaybackTime, TotalPlaybackDuration);
    }
}

void USwingVideoWidget::StopSwingVideo()
{
    if (!bIsPlaying && !bIsPaused)
    {
        UE_LOG(LogTemp, Log, TEXT("?? Already stopped"));
        return;
    }

    bIsPlaying = false;
    bIsPaused = false;
    bClipFinishBroadcasted = false;
    CurrentPlaybackTime = 0.0f;
    CurrentFrameIndex = 0;
    PlaybackTimerAccumulator = 0.0f;

    // ? 현재 모드에 따른 상세 로그
    if (!CurrentClipDirectory.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("?? Clip playback stopped"));
        UE_LOG(LogTemp, Log, TEXT("   Total Frames: %d"), ClipFramePaths.Num());

        // ? 클립 재생 정리
        CurrentClipDirectory.Empty();
        ClipFramePaths.Empty();
        CurrentClipFrameIndex = 0;

        // ? 로드된 텍스처 정리 (AddToRoot 해제 포함)
        for (UTexture2D* Tex : LoadedFrameTextures)
        {
            if (Tex && IsValid(Tex))
            {
                Tex->RemoveFromRoot();      // ? 필수: AddToRoot와 쌍으로 호출
                Tex->MarkAsGarbage();
            }
        }
        LoadedFrameTextures.Empty();
        bUseTextureCache = false;

        UE_LOG(LogTemp, Log, TEXT("??? Clip resources cleared"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("?? Swing video stopped"));
        UE_LOG(LogTemp, Log, TEXT("   Total Frames: %d"), SwingFrames.Num());
        UE_LOG(LogTemp, Log, TEXT("   Duration: %.2f seconds"), TotalPlaybackDuration);
    }

    UE_LOG(LogTemp, Log, TEXT("   Playback time: %.2f seconds"), CurrentPlaybackTime);
}

void USwingVideoWidget::SetPlaybackPosition(float Position)
{
    Position = FMath::Clamp(Position, 0.0f, 1.0f);

    // ? ClipPlayback 모드 (JPG 시퀀스)
    if (!CurrentClipDirectory.IsEmpty() && ClipFramePaths.Num() > 0)
    {
        int32 TargetFrame = FMath::FloorToInt(Position * (float)(ClipFramePaths.Num() - 1));
        TargetFrame = FMath::Clamp(TargetFrame, 0, ClipFramePaths.Num() - 1);

        CurrentClipFrameIndex = TargetFrame;
        CurrentPlaybackTime = Position * (ClipFramePaths.Num() / ClipFPS);
        PlaybackTimerAccumulator = 0.0f;

        // ? 즉시 해당 프레임 렌더
        RenderClipFrameAt(CurrentClipFrameIndex);

        UE_LOG(LogTemp, Log, TEXT("? Clip Seek → Frame %d / %d (%.1f%%)"),
            TargetFrame, ClipFramePaths.Num(), Position * 100.0f);
        return;
    }

    // ? SwingFrames 모드 (기존 로직)
    if (CurrentVideoMode != EVideoMode::SwingPlayback || SwingFrames.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot seek: invalid mode or no frames"));
        return;
    }

    float NewPlaybackTime = Position * TotalPlaybackDuration;
    CurrentPlaybackTime = NewPlaybackTime;
    UpdateCurrentFrameIndex();
    PlaybackTimerAccumulator = 0.0f;

    // 즉시 렌더
    if (SwingFrames.IsValidIndex(CurrentFrameIndex))
    {
        UTexture2D* Tex = SwingFrames[CurrentFrameIndex].FrameTexture;
        if (Tex && IsValid(Tex) && Tex->GetResource() && VideoDisplay)
        {
            VideoDisplay->SetBrushFromTexture(Tex);
            LastSetTexture = Tex;
        }
    }

    UE_LOG(LogTemp, Log, TEXT("? Swing Seek → Frame %d / %d (%.1f%%)"),
        CurrentFrameIndex, SwingFrames.Num(), Position * 100.0f);
}

void USwingVideoWidget::UpdateVideoDisplay()
{
    // ? 주기적 모드 로그 (30초마다)
    static float LastModeLogTime = 0.0f;
    float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    if (CurrentVideoMode != EVideoMode::FilePlayback)
    {
        if (!WebcamCaptureRef.IsValid())
            return;
    }


    if (CurrentTime - LastModeLogTime >= 30.0f)
    {
        FString ModeStr;
        switch (CurrentVideoMode)
        {
        case EVideoMode::LiveFeed:
            ModeStr = TEXT("LiveFeed");
            break;
        case EVideoMode::SwingPlayback:
            ModeStr = TEXT("SwingPlayback");
            break;
        case EVideoMode::FilePlayback:
            ModeStr = TEXT("FilePlayback");
            break;
        }
        UE_LOG(LogTemp, Log, TEXT("?? Current Video Mode: %s"), *ModeStr);
        LastModeLogTime = CurrentTime;
    }

    switch (CurrentVideoMode)
    {
    case EVideoMode::LiveFeed:
        UpdateLiveFeed();
        break;
    case EVideoMode::SwingPlayback:
        // UpdateSwingPlayback();
        if (!CurrentClipDirectory.IsEmpty() && ClipFramePaths.Num() > 0)
        {
            // ClipPlayback: HandleClipPlaybackMode에서 렌더하므로 여기선 스킵
            // (단, bIsPlaying=false인 탐색 상태면 현재 프레임 유지)
        }
        else
            UpdateFilePlayback();
        break;
    case EVideoMode::FilePlayback:
        UpdateFilePlayback();
        break;
    }
}
// ? 수정: 강화된 유효성 검사
void USwingVideoWidget::UpdateLiveFeed()
{
    // ? 1. WebcamCapture 유효성 검사
    if (!WebcamCaptureRef.IsValid())
    {
        static float LastWarningTime = 0.0f;
        float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

        if (CurrentTime - LastWarningTime >= 10.0f)
        {
            UE_LOG(LogTemp, Warning, TEXT("?? UpdateLiveFeed: WebcamCaptureRef is invalid"));
            LastWarningTime = CurrentTime;
        }
        return;
    }

    // ? 2. MediaTexture 유효성 검사
    UMediaTexture* MediaTex = WebcamCaptureRef->MediaTexture;
    if (!MediaTex || !IsValid(MediaTex))
    {
        static float LastWarningTime = 0.0f;
        float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

        if (CurrentTime - LastWarningTime >= 10.0f)
        {
            UE_LOG(LogTemp, Warning, TEXT("?? UpdateLiveFeed: MediaTexture is invalid"));
            LastWarningTime = CurrentTime;
        }
        return;
    }

    if (MediaTex->GetResource() == nullptr)
    {
        static float LastWarningTime = 0.0f;
        float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

        if (CurrentTime - LastWarningTime >= 10.0f)
        {
            UE_LOG(LogTemp, Warning, TEXT("?? UpdateLiveFeed: MediaTexture Resource is null"));
            LastWarningTime = CurrentTime;
        }
        return;
    }

    // ? 3. VideoDisplay 유효성 검사
    if (!VideoDisplay || !IsValid(VideoDisplay))
    {
        UE_LOG(LogTemp, Error, TEXT("? UpdateLiveFeed: VideoDisplay is invalid"));
        return;
    }

    // ========================================
    // ? 방법 A: CachedDynamicMaterial 사용 (기존 방식)
    // ========================================
    UMaterialInstanceDynamic* CachedMat = WebcamCaptureRef->GetCachedDynamicMaterial();

    if (CachedMat && IsValid(CachedMat) && CachedMat->GetRenderProxy())
    {
        FSlateBrush Brush;
        Brush.SetResourceObject(CachedMat);
        Brush.ImageSize = FVector2D(640, 480);
        Brush.DrawAs = ESlateBrushDrawType::Image;

        if (Brush.GetResourceObject() != nullptr)
        {
            VideoDisplay->SetBrush(Brush);

            // ? 주기적 로그 (60초마다)
            static float LastLogTime = 0.0f;
            float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

            if (CurrentTime - LastLogTime >= 60.0f)
            {
                UE_LOG(LogTemp, Log, TEXT("? LiveFeed displaying (Material method)"));
                LastLogTime = CurrentTime;
            }
        }
        return;
    }

    // ========================================
    // ? 방법 B: MediaTexture 직접 사용 (Material 없을 때 대체)
    // ========================================
    static bool bLoggedFallback = false;
    if (!bLoggedFallback)
    {
        UE_LOG(LogTemp, Warning, TEXT("?? CachedDynamicMaterial unavailable, using MediaTexture directly"));
        bLoggedFallback = true;
    }

    FSlateBrush Brush;
    Brush.SetResourceObject(MediaTex);
    Brush.ImageSize = FVector2D(640, 480);
    Brush.DrawAs = ESlateBrushDrawType::Image;

    if (Brush.GetResourceObject() != nullptr)
    {
        VideoDisplay->SetBrush(Brush);

        // ? 주기적 로그 (60초마다)
        static float LastLogTime = 0.0f;
        float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

        if (CurrentTime - LastLogTime >= 60.0f)
        {
            UE_LOG(LogTemp, Log, TEXT("? LiveFeed displaying (MediaTexture direct method)"));
            LastLogTime = CurrentTime;
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("? Failed to create brush from MediaTexture"));
    }
}


// ? 수정: 강화된 유효성 검사
void USwingVideoWidget::CheckAndRestartMediaPlayer()
{
    // ? 1. WebcamCapture 유효성 검사
    if (!WebcamCaptureRef.IsValid())
    {
        return;
    }

    // ? 2. MediaPlayer 유효성 검사
    if (!WebcamCaptureRef->MediaPlayer || !IsValid(WebcamCaptureRef->MediaPlayer))
    {
        return;
    }

    UMediaPlayer* MediaPlayer = WebcamCaptureRef->MediaPlayer;

    // ? 3. MediaPlayer가 재생 중이 아니고 에러도 없으면 재시작
    if (!MediaPlayer->IsPlaying() && !MediaPlayer->IsPreparing() && !MediaPlayer->HasError())
    {
        UE_LOG(LogTemp, Warning, TEXT("?? MediaPlayer not playing, attempting to restart..."));

        // ? 4. WebcamSource 유효성 검사
        if (WebcamCaptureRef->WebcamSource && IsValid(WebcamCaptureRef->WebcamSource))
        {
            MediaPlayer->OpenSource(WebcamCaptureRef->WebcamSource);
        }
    }
}
void USwingVideoWidget::UpdateSwingPlayback()
{

    if (!bIsPlaying) return;

    // ? GC 중에는 프레임 업데이트 스킵
    if (IsGarbageCollecting())
    {
        static float LastWarning = 0.0f;
        float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

        if (Now - LastWarning > 5.0f)
        {
            UE_LOG(LogTemp, Warning, TEXT("?? Playback paused: GC in progress"));
            LastWarning = Now;
        }
        return;
    }

    if (!bIsPlaying)
        return;

    // ? 기본 유효성 검사
    if (SwingFrames.Num() == 0)
    {
        static bool bFirstWarning = true;
        if (bFirstWarning)
        {
            UE_LOG(LogTemp, Warning, TEXT("?? UpdateSwingPlayback: SwingFrames is empty!"));
            DiagnosePlaybackFailure();
            bFirstWarning = false;
        }
        return;
    }

    // ? 인덱스 범위 확인
    if (CurrentFrameIndex < 0 || CurrentFrameIndex >= SwingFrames.Num())
    {
        UE_LOG(LogTemp, Error, TEXT("? Invalid frame index: %d/%d"),
            CurrentFrameIndex, SwingFrames.Num());
        CurrentFrameIndex = FMath::Clamp(CurrentFrameIndex, 0, SwingFrames.Num() - 1);

        return;
    }


    const FVideoFrame& CurrentFrame = SwingFrames[CurrentFrameIndex];

    // ? 프레임별 검증 (10프레임마다)
    ValidateFrameBeforePlayback(CurrentFrameIndex);

    // ? 텍스처 검사
    if (!CurrentFrame.FrameTexture)
    {
        UE_LOG(LogTemp, Error, TEXT("? Frame [%d] Texture is NULL!"), CurrentFrameIndex);
        DiagnosePlaybackFailure();
        return;
    }

    if (!IsValid(CurrentFrame.FrameTexture))
    {
        UE_LOG(LogTemp, Error, TEXT("? Frame [%d] Texture is INVALID!"), CurrentFrameIndex);
        return;
    }

    // ? VideoDisplay 검사
    if (!VideoDisplay || !IsValid(VideoDisplay))
    {
        UE_LOG(LogTemp, Error, TEXT("? Frame [%d] VideoDisplay is INVALID!"), CurrentFrameIndex);
        return;
    }


    UTexture2D* TargetTexture = SwingFrames[CurrentFrameIndex].FrameTexture;

    // [체크 1] 텍스처 및 리소스 유효성 검사 강화
    if (TargetTexture && TargetTexture->GetResource())
    {
        // [체크 2] 현재 설정된 텍스처와 동일한지 확인 (불필요한 갱신 방지)
        if (LastSetTexture != TargetTexture)
        {
            VideoDisplay->SetBrushFromTexture(TargetTexture);
            //BlitFrameToRT(TargetTexture);

            // [핵심] 일부 UE4 버전에서는 브러시 리소스를 명시적으로 다시 세팅해야 갱신됨
          //  VideoDisplay->GetDynamicBrush()->SetResourceObject(TargetTexture);


            LastSetTexture = TargetTexture;
            bHasEverSetSwingBrush = true;
        }
    }
    else
    {
        // 리소스가 없는 경우 로그 출력 (진단용)
        UE_LOG(LogTemp, Log, TEXT("Frame %d: Texture or Resource is Null"), CurrentFrameIndex);
    }




    // [핵심] 일부 UE4 버전에서는 브러시 리소스를 명시적으로 다시 세팅해야 갱신됨
  //  VideoDisplay->GetDynamicBrush()->SetResourceObject(CurrentFrame.FrameTexture);

    // 진단 로그 (30프레임마다)
    if (CurrentFrameIndex % 30 == 0)
    {
        UE_LOG(LogTemp, Log, TEXT("??  Frame [%d/%d]  VideoDisplay->SetBrush displayed"),
            CurrentFrameIndex + 1, SwingFrames.Num());
    }

    // ? Brush 설정
    FSlateBrush Brush;
    Brush.SetResourceObject(CurrentFrame.FrameTexture);
    Brush.ImageSize = FVector2D(640, 480);
    Brush.DrawAs = ESlateBrushDrawType::Image;

    if (Brush.GetResourceObject() != nullptr)
    {
        VideoDisplay->SetBrush(Brush);

        // 진단 로그 (30프레임마다)
        if (CurrentFrameIndex % 30 == 0)
        {
            UE_LOG(LogTemp, Log, TEXT("??  Frame [%d/%d]  VideoDisplay->SetBrush displayed"),
                CurrentFrameIndex + 1, SwingFrames.Num());
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("? Brush resource is NULL!"));
        DiagnosePlaybackFailure();
    }

}


void USwingVideoWidget::UpdateUI()
{
    if (CurrentVideoMode == EVideoMode::SwingPlayback)
    {
        if (ProgressSlider && IsValid(ProgressSlider))
        {
            float Progress = 0.0f;

            // ? ClipPlayback 모드
            if (!CurrentClipDirectory.IsEmpty() && ClipFramePaths.Num() > 0)
            {
                Progress = ClipFramePaths.Num() > 1
                    ? (float)CurrentClipFrameIndex / (float)(ClipFramePaths.Num() - 1)
                    : 0.0f;

                // TotalPlaybackDuration도 맞춰줌
                TotalPlaybackDuration = ClipFramePaths.Num() / ClipFPS;
            }
            else if (TotalPlaybackDuration > 0.0f)
            {
                // ? SwingFrames 모드
                Progress = CurrentPlaybackTime / TotalPlaybackDuration;
            }

            Progress = FMath::Clamp(Progress, 0.0f, 1.0f);

            ProgressSlider->OnValueChanged.RemoveDynamic(this, &USwingVideoWidget::OnProgressSliderChanged);
            ProgressSlider->SetValue(Progress);
            ProgressSlider->OnValueChanged.AddDynamic(this, &USwingVideoWidget::OnProgressSliderChanged);
        }

        UpdateTimeDisplay();
    }
}

void USwingVideoWidget::UpdateTimeDisplay()
{
    if (!TimeText) return;

    // 기존: 초.1자리 (100ms 단위)
    //   int32 TotalMs = FMath::FloorToInt(T * 10.0f);  // 100ms 단위
    //   int32 Sec  = TotalMs / 10;
    //   int32 Ms10 = TotalMs % 10;
    //   return FString::Printf(TEXT("%02d.%d"), Sec, Ms10);

    // ? 수정: 초.2자리 (10ms 단위)
    auto FormatTimeSec = [](float T) -> FString
        {
            T = FMath::Max(0.0f, T);
            int32 TotalCentisec = FMath::FloorToInt(T * 100.0f);  // 10ms(센티초) 단위
            int32 Sec = TotalCentisec / 100;
            int32 Cs = TotalCentisec % 100;                      // 0~99
            return FString::Printf(TEXT("%02d:%02d"), Sec, Cs);
        };

    // ClipPlayback 모드: ClipFramePaths 기준
    float DisplayCurrent = CurrentPlaybackTime;
    float DisplayTotal = TotalPlaybackDuration;

    // ? TotalPlaybackDuration이 비정상이면 클립 기준으로 보정
    if (DisplayTotal <= 0.0f || !FMath::IsFinite(DisplayTotal))
    {
        if (ClipFramePaths.Num() > 0 && ClipFPS > 0.0f)
            DisplayTotal = (float)ClipFramePaths.Num() / ClipFPS;
        else if (SwingFrames.Num() > 0)
            DisplayTotal = TotalPlaybackDuration;
    }

    FString TimeStr = FString::Printf(TEXT("%s / %s"),
        *FormatTimeSec(DisplayCurrent),
        *FormatTimeSec(DisplayTotal));

    TimeText->SetText(FText::FromString(TimeStr));
}

void USwingVideoWidget::CalculatePlaybackDuration()
{
    if (SwingFrames.Num() < 2)
    {
        TotalPlaybackDuration = 0.0f;
        return;
    }

    const float First = SwingFrames[0].Timestamp;
    const float Last = SwingFrames.Last().Timestamp;
    const float ByTimestamp = Last - First;

    if (ByTimestamp > 0.0f)
    {
        TotalPlaybackDuration = ByTimestamp;   // ? 타임스탬프 기반
    }
    else
    {
        TotalPlaybackDuration = SwingFrames.Num() / FrameRate; // fallback
    }
}



FString USwingVideoWidget::FormatTime(float TimeInSeconds)
{
    int32 Minutes = FMath::FloorToInt(TimeInSeconds / 60.0f);
    int32 Seconds = FMath::FloorToInt(FMath::Fmod(TimeInSeconds, 60.0f));
    int32 Milliseconds = FMath::FloorToInt(FMath::Fmod(TimeInSeconds * 1000.0f, 1000.0f));

    return FString::Printf(TEXT("%02d:%02d.%03d"), Minutes, Seconds, Milliseconds);
}

void USwingVideoWidget::UpdateCurrentFrameIndex()
{
    if (SwingFrames.Num() == 0)
        return;

    float StartTime = SwingFrames[0].Timestamp;
    float TargetTime = StartTime + CurrentPlaybackTime;

    // ?? 기준: 마지막 인덱스에서 시작 (재생은 거의 항상 증가)
    int32 SearchIndex = LastCalculatedFrameIndex;

    // ?? 앞으로 이동 (일반적)
    while (SearchIndex < SwingFrames.Num() - 1 &&
        SwingFrames[SearchIndex + 1].Timestamp <= TargetTime)
    {
        UE_LOG(LogTemp, Log, TEXT("?? Current Frame Count : +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ %d"), SearchIndex + 1);
        SearchIndex++;
    }


    // ? 캐시 업데이트
    CurrentFrameIndex = SearchIndex;
    CurrentClipFrameIndex = SearchIndex;
    LastCalculatedFrameIndex = SearchIndex;

    // 성능 디버그 (선택사항)
    if (GEngine)
    {
        static float DebugTimer = 0.0f;
        DebugTimer += GetWorld()->GetDeltaSeconds();
        if (DebugTimer >= 5.0f)  // 5초마다
        {
            DebugTimer = 0.0f;
            UE_LOG(LogTemp, Log, TEXT("Frame index: %d / %d"),
                CurrentFrameIndex, SwingFrames.Num() - 1);
        }
    }
}

// 버튼 이벤트 핸들러들
void USwingVideoWidget::OnPlayButtonClicked()
{
    UE_LOG(LogTemp, Warning, TEXT("???????????????????????????????????????"));
    UE_LOG(LogTemp, Warning, TEXT("▶? Play Button Clicked"));
    UE_LOG(LogTemp, Warning, TEXT("???????????????????????????????????????"));

    // ? 1?? 일시정지 상태에서 재개하는 경우
    if (bIsPaused)
    {
        bIsPaused = false;
        bIsPlaying = true;
        UE_LOG(LogTemp, Warning, TEXT("? Resuming from pause at %.2f seconds"), CurrentPlaybackTime);
        UE_LOG(LogTemp, Log, TEXT("   Mode: %s"),
            CurrentVideoMode == EVideoMode::SwingPlayback ? TEXT("SwingPlayback") : TEXT("ClipPlayback"));
        return;
    }

    // ? 2?? 클립 재생 중인 경우
    if (!CurrentClipDirectory.IsEmpty() && ClipFramePaths.Num() > 0)
    {
        bIsPlaying = true;
        bIsPaused = false;
        CurrentClipFrameIndex = 0;
        CurrentPlaybackTime = 0.0f;
        PlaybackTimerAccumulator = 0.0f;
        bClipFinishBroadcasted = false;

        UE_LOG(LogTemp, Warning, TEXT("▶? Clip Playback Started"));
        UE_LOG(LogTemp, Log, TEXT("   Frames: %d"), ClipFramePaths.Num());
        UE_LOG(LogTemp, Log, TEXT("   FPS: %.1f"), ClipFPS);
        return;
    }

    // ? 3?? 스윙 프레임 재생하는 경우
    if (SwingFrames.Num() > 0)
    {
        PlaySwingVideo();
        UE_LOG(LogTemp, Warning, TEXT("▶? Swing Playback Started"));
        UE_LOG(LogTemp, Log, TEXT("   Frames: %d"), SwingFrames.Num());
        UE_LOG(LogTemp, Log, TEXT("   Duration: %.2f seconds"), TotalPlaybackDuration);
        return;
    }

    // ? 4?? 재생 불가능한 경우
    UE_LOG(LogTemp, Error, TEXT("? No frames available for playback"));
    if (StatusText && IsValid(StatusText))
    {
        StatusText->SetText(FText::FromString(TEXT("? 재생 데이터 없음")));
    }



    if (StatusText)
    {
        StatusText->SetText(FText::FromString(TEXT("스윙 재생")));
    }
}


void USwingVideoWidget::OnPauseButtonClicked()
{
    UE_LOG(LogTemp, Warning, TEXT("???????????????????????????????????????"));
    UE_LOG(LogTemp, Warning, TEXT("?? Pause Button Clicked"));
    UE_LOG(LogTemp, Warning, TEXT("???????????????????????????????????????"));

    // ? 재생 중인 경우에만 일시정지
    if (!bIsPlaying)
    {
        UE_LOG(LogTemp, Warning, TEXT("?? No playback in progress"));
        if (StatusText && IsValid(StatusText))
        {
            StatusText->SetText(FText::FromString(TEXT("?? Not Play")));
        }
        return;
    }

    if (bIsPaused)
    {
        // 이미 일시정지된 상태 - 무시
        UE_LOG(LogTemp, Log, TEXT("   Already paused at %.2f seconds"), CurrentPlaybackTime);
        return;
    }

    PauseSwingVideo();

    UE_LOG(LogTemp, Warning, TEXT("? Paused"));
    UE_LOG(LogTemp, Log, TEXT("   Current Time: %.2f seconds"), CurrentPlaybackTime);
    UE_LOG(LogTemp, Log, TEXT("   Total Duration: %.2f seconds"), TotalPlaybackDuration);

    if (StatusText && IsValid(StatusText))
    {
        StatusText->SetText(FText::FromString(TEXT(" PAUSE")));
    }
}

void USwingVideoWidget::OnStopButtonClicked()
{
    UE_LOG(LogTemp, Warning, TEXT("???????????????????????????????????????"));
    UE_LOG(LogTemp, Warning, TEXT("?? Stop Button Clicked"));
    UE_LOG(LogTemp, Warning, TEXT("???????????????????????????????????????"));

    if (!bIsPlaying && !bIsPaused)
    {
        UE_LOG(LogTemp, Warning, TEXT("?? Already stopped"));
        return;
    }

    StopSwingVideo();

    UE_LOG(LogTemp, Warning, TEXT("? Stopped"));
    UE_LOG(LogTemp, Log, TEXT("   Playback time: %.2f seconds"), CurrentPlaybackTime);
    UE_LOG(LogTemp, Log, TEXT("   Total frames: %d"),
        !CurrentClipDirectory.IsEmpty() ? ClipFramePaths.Num() : SwingFrames.Num());

    // ? UI 업데이트
    CurrentFrameIndex = 0;
    CurrentPlaybackTime = 0.0f;

    if (StatusText && IsValid(StatusText))
    {
        StatusText->SetText(FText::FromString(TEXT(" STOP")));
    }

    if (ProgressSlider && IsValid(ProgressSlider))
    {
        ProgressSlider->SetValue(0.0f);
    }

    if (TimeText && IsValid(TimeText))
    {
        TimeText->SetText(FText::FromString(TEXT("00:00 / 00:00")));
    }
}

void USwingVideoWidget::OnLiveFeedButtonClicked()
{
    UE_LOG(LogTemp, Warning, TEXT("???????????????????????????????????????"));
    UE_LOG(LogTemp, Warning, TEXT("?? Live Feed Button Clicked"));
    UE_LOG(LogTemp, Warning, TEXT("???????????????????????????????????????"));

    // ? 이미 라이브피드 모드인 경우
    if (CurrentVideoMode == EVideoMode::LiveFeed)
    {
        UE_LOG(LogTemp, Log, TEXT("?? Already in LiveFeed mode"));
        return;
    }

    // ? 현재 재생 상태 로그
    if (bIsPlaying)
    {
        UE_LOG(LogTemp, Log, TEXT("   Stopping current playback..."));
        StopSwingVideo();
    }

    // ? 라이브 피드로 전환
    SwitchToLiveFeed();

    UE_LOG(LogTemp, Warning, TEXT("? Switched to Live Feed"));
    UE_LOG(LogTemp, Log, TEXT("   Mode: LiveFeed"));
    UE_LOG(LogTemp, Log, TEXT("   Webcam: Active"));

    if (StatusText && IsValid(StatusText))
    {
        StatusText->SetText(FText::FromString(TEXT(" Live Feed")));
    }

    // ? 컨트롤 박스 숨기기 (라이브피드는 컨트롤 필요 없음)
    if (ControlsBox && IsValid(ControlsBox))
    {
        ControlsBox->SetVisibility(ESlateVisibility::Collapsed);
        UE_LOG(LogTemp, Log, TEXT("   Controls hidden"));
    }
}

void USwingVideoWidget::CloseSwingMotion()
{
    // Step 1: 재생 중지 (매우 중요!)
    bIsPlaying = false;
    bIsPaused = false;

    // Step 2: 타이머 리셋
    PlaybackTimerAccumulator = 0.0f;
    CurrentFrameIndex = 0;
    CurrentPlaybackTime = 0.0f;

    // Step 3: RootedSwingTextures 정리 (먼저!)
    UnrootSwingTextures();
    RootedSwingTextures.Empty();

    // Step 4: SwingFrames 정리 (나중!)
    SwingFrames.Empty();

    // Step 5: 캐시 정리
    FrameCachePool.Empty();
    for (UTexture2D* Tex : LoadedFrameTextures)
    {
        if (Tex && IsValid(Tex))
        {
            Tex->RemoveFromRoot();      // ? AddToRoot 해제
            Tex->MarkAsGarbage();
        }
    }
    LoadedFrameTextures.Empty();
    bUseTextureCache = false;

    // Step 6: 클립 정보 초기화
    CurrentClipDirectory.Empty();
    ClipFramePaths.Empty();
    ExpectedTotalFrames = 0;
    bClipFinishBroadcasted = false;
    // Step 7: 비디오 모드 초기화
    CurrentVideoMode = EVideoMode::LiveFeed;

    // Step 8: UI 숨기기 (마지막!)
    this->SetVisibility(ESlateVisibility::Collapsed);
}

void USwingVideoWidget::OnProgressSliderChanged(float Value)
{
    if (CurrentVideoMode == EVideoMode::SwingPlayback)
    {
        SetPlaybackPosition(Value);
    }
}

void USwingVideoWidget::SetVideoFrame(const FVideoFrame& Frame)
{
    // ========== Step 1: 기본 유효성 검사 ==========
    if (!Frame.FrameTexture || !IsValid(Frame.FrameTexture))
    {
        static float LastWarningTime = 0.0f;
        float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

        if (CurrentTime - LastWarningTime >= 5.0f)
        {
            UE_LOG(LogTemp, Warning, TEXT("?? SetVideoFrame: FrameTexture is invalid"));
            LastWarningTime = CurrentTime;
        }
        return;
    }

    if (!VideoDisplay || !IsValid(VideoDisplay))
    {
        UE_LOG(LogTemp, Error, TEXT("? SetVideoFrame: VideoDisplay is invalid"));
        return;
    }

    // ========== Step 2: Resource 검증 ==========
    if (Frame.FrameTexture->GetResource() == nullptr)
    {
        static float LastWarningTime = 0.0f;
        float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

        if (CurrentTime - LastWarningTime >= 5.0f)
        {
            UE_LOG(LogTemp, Warning, TEXT("?? SetVideoFrame: Texture Resource is null"));
            LastWarningTime = CurrentTime;
        }

        Frame.FrameTexture->UpdateResource();
        return;
    }

    // ========== Step 3: 중복 설정 방지 (최적화) ==========
    if (LastSetTexture == Frame.FrameTexture)
    {
        return;
    }

    // ========== Step 4: VideoDisplay에 텍스처 설정 ==========
   // VideoDisplay->SetBrushFromTexture(Frame.FrameTexture);

   // BlitFrameToRT(Frame.FrameTexture);
    VideoDisplay->SetBrushFromTexture(Frame.FrameTexture);

    LastSetTexture = Frame.FrameTexture;
    bHasEverSetSwingBrush = true;

    // ========== Step 5: 디버그 로그 (주기적) ==========
    static int32 SetFrameCount = 0;
    if (++SetFrameCount % 60 == 0)  // 60번마다 (약 1초)
    {
        UE_LOG(LogTemp, Log, TEXT("? SetVideoFrame success: %s (count: %d)"),
            *Frame.FrameTexture->GetName(), SetFrameCount);
    }
}


void USwingVideoWidget::DiagnosePlaybackIssue()
{
    UE_LOG(LogTemp, Warning, TEXT("?? === SWING PLAYBACK DIAGNOSIS ==="));

    // 1?? SwingFrames 확인
    UE_LOG(LogTemp, Log, TEXT("1?? SwingFrames Data:"));
    UE_LOG(LogTemp, Log, TEXT("   - Total frames: %d"), SwingFrames.Num());

    if (SwingFrames.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("   ? NO FRAMES! SetSwingFrames() was never called!"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("   ? Has %d frames"), SwingFrames.Num());
    UE_LOG(LogTemp, Log, TEXT("   - Current index: %d"), CurrentFrameIndex);
    UE_LOG(LogTemp, Log, TEXT("   - Duration: %.3f sec"), TotalPlaybackDuration);
    UE_LOG(LogTemp, Log, TEXT("   - FrameRate: %.1f FPS"), FrameRate);

    // 2?? 프레임 텍스처 확인
    UE_LOG(LogTemp, Log, TEXT("2?? Frame Textures:"));

    int32 ValidFrameCount = 0;
    int32 NullTextureCount = 0;
    int32 InvalidFrameCount = 0;
    int32 NullResourceCount = 0;

    for (int32 i = 0; i < SwingFrames.Num(); ++i)
    {
        const FVideoFrame& Frame = SwingFrames[i];

        // 기본 텍스처 체크
        if (!Frame.FrameTexture)
        {
            NullTextureCount++;
            if (i < 5)  // 처음 5개만 로그
            {
                UE_LOG(LogTemp, Warning, TEXT("   ? Frame[%d]: Texture is NULL"), i);
            }
            continue;
        }

        // 유효성 체크
        if (!IsValid(Frame.FrameTexture))
        {
            InvalidFrameCount++;
            if (i < 5)
            {
                UE_LOG(LogTemp, Warning, TEXT("   ? Frame[%d]: Texture is INVALID (destroyed?)"), i);
            }
            continue;
        }

        // Resource 체크
        if (Frame.FrameTexture->GetResource() == nullptr)
        {
            NullResourceCount++;
            if (i < 5)
            {
                UE_LOG(LogTemp, Warning, TEXT("   ?? Frame[%d]: Texture has no Resource"), i);
            }
            continue;
        }

        ValidFrameCount++;
    }

    UE_LOG(LogTemp, Log, TEXT("   - Valid frames: %d/%d"), ValidFrameCount, SwingFrames.Num());
    UE_LOG(LogTemp, Log, TEXT("   - Null textures: %d"), NullTextureCount);
    UE_LOG(LogTemp, Log, TEXT("   - Invalid textures: %d"), InvalidFrameCount);
    UE_LOG(LogTemp, Log, TEXT("   - Null resources: %d"), NullResourceCount);

    if (ValidFrameCount == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("   ? NO VALID FRAMES! All frames are corrupted!"));
        return;
    }

    // 3?? 현재 프레임 상세 확인
    UE_LOG(LogTemp, Log, TEXT("3?? Current Frame State:"));
    UE_LOG(LogTemp, Log, TEXT("   - Index: %d"), CurrentFrameIndex);
    UE_LOG(LogTemp, Log, TEXT("   - Playback time: %.3f sec"), CurrentPlaybackTime);

    if (CurrentFrameIndex >= 0 && CurrentFrameIndex < SwingFrames.Num())
    {
        const FVideoFrame& CurrentFrame = SwingFrames[CurrentFrameIndex];

        if (!CurrentFrame.FrameTexture)
        {
            UE_LOG(LogTemp, Error, TEXT("   ? CurrentFrame.FrameTexture is NULL"));
        }
        else if (!IsValid(CurrentFrame.FrameTexture))
        {
            UE_LOG(LogTemp, Error, TEXT("   ? CurrentFrame.FrameTexture is INVALID"));
        }
        else if (CurrentFrame.FrameTexture->GetResource() == nullptr)
        {
            UE_LOG(LogTemp, Error, TEXT("   ?? CurrentFrame.FrameTexture has no Resource"));
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("   ? CurrentFrame is VALID"));
            UE_LOG(LogTemp, Log, TEXT("      - Size: %dx%d"),
                CurrentFrame.FrameTexture->GetSizeX(),
                CurrentFrame.FrameTexture->GetSizeY());
            UE_LOG(LogTemp, Log, TEXT("      - Format: %d"), (int32)CurrentFrame.FrameTexture->GetPixelFormat());
        }
    }

    // 4?? VideoDisplay 위젯 확인
    UE_LOG(LogTemp, Log, TEXT("4?? VideoDisplay Widget:"));

    if (!VideoDisplay)
    {
        UE_LOG(LogTemp, Error, TEXT("   ? VideoDisplay is NULL"));
        return;
    }

    if (!IsValid(VideoDisplay))
    {
        UE_LOG(LogTemp, Error, TEXT("   ? VideoDisplay is INVALID"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("   ? VideoDisplay is VALID"));

    if (UImage* ImageWidget = Cast<UImage>(VideoDisplay))
    {
        UE_LOG(LogTemp, Log, TEXT("   ? VideoDisplay is UImage"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("   ? VideoDisplay is NOT UImage!"));
        UE_LOG(LogTemp, Log, TEXT("      - Actual type: %s"), *VideoDisplay->GetClass()->GetName());
        return;
    }

    // 5?? 재생 상태 확인
    UE_LOG(LogTemp, Log, TEXT("5?? Playback State:"));
    UE_LOG(LogTemp, Log, TEXT("   - Mode: %s"),
        CurrentVideoMode == EVideoMode::SwingPlayback ? TEXT("SwingPlayback") : TEXT("LiveFeed"));
    UE_LOG(LogTemp, Log, TEXT("   - Playing: %s"), bIsPlaying ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogTemp, Log, TEXT("   - Paused: %s"), bIsPaused ? TEXT("YES") : TEXT("NO"));

    UE_LOG(LogTemp, Warning, TEXT("? === DIAGNOSIS COMPLETE ==="));
}


void USwingVideoWidget::ValidateSwingFramesData()
{
    UE_LOG(LogTemp, Warning, TEXT("???????????????????????????????????????????????????????????"));
    UE_LOG(LogTemp, Warning, TEXT("?? BUFFER DATA VALIDATION - Complete Check"));
    UE_LOG(LogTemp, Warning, TEXT("???????????????????????????????????????????????????????????"));

    // Step 1: 배열 크기 확인
    UE_LOG(LogTemp, Warning, TEXT("[1/5] Array Size Check"));
    UE_LOG(LogTemp, Warning, TEXT("      Total frames: %d"), SwingFrames.Num());

    if (SwingFrames.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("      ? CRITICAL: No frames in array!"));
        PrintValidationStats();
        return;
    }

    // Step 2: 초기화
    ValidationStats.TotalFrames = SwingFrames.Num();
    ValidationStats.ValidFrames = 0;
    ValidationStats.NullTextureFrames = 0;
    ValidationStats.InvalidTextureFrames = 0;
    ValidationStats.ResourceNullFrames = 0;
    ValidationStats.FirstTimestamp = FLT_MAX;
    ValidationStats.LastTimestamp = -FLT_MAX;
    ValidationStats.MinTimestampGap = FLT_MAX;
    ValidationStats.MaxTimestampGap = 0.0f;

    // Step 3: 각 프레임 상세 검사
    UE_LOG(LogTemp, Warning, TEXT("[2/5] Frame-by-frame Analysis"));

    for (int32 i = 0; i < SwingFrames.Num(); ++i)
    {
        const FVideoFrame& Frame = SwingFrames[i];
        bool bFrameValid = true;

        // 3-1: 텍스처 NULL 체크
        if (!Frame.FrameTexture)
        {
            ValidationStats.NullTextureFrames++;
            bFrameValid = false;

            if (i < 5 || i >= SwingFrames.Num() - 5)  // 처음 5개, 마지막 5개만 로그
            {
                UE_LOG(LogTemp, Error, TEXT("      [%d] ? Texture is NULL"), i);
            }
            continue;
        }

        // 3-2: 텍스처 유효성 체크
        if (!IsValid(Frame.FrameTexture))
        {
            ValidationStats.InvalidTextureFrames++;
            bFrameValid = false;

            if (i < 5 || i >= SwingFrames.Num() - 5)
            {
                UE_LOG(LogTemp, Error, TEXT("      [%d] ? Texture is INVALID (destroyed)"), i);
            }
            continue;
        }

        // 3-3: 리소스 체크
        if (Frame.FrameTexture->GetResource() == nullptr)
        {
            ValidationStats.ResourceNullFrames++;
            bFrameValid = false;

            if (i < 5 || i >= SwingFrames.Num() - 5)
            {
                UE_LOG(LogTemp, Warning, TEXT("      [%d] ?? Texture Resource is null"), i);
            }
            continue;
        }

        // 3-4: 타임스탬프 체크
        if (Frame.Timestamp < 0.0f)
        {
            if (i < 5)
            {
                UE_LOG(LogTemp, Warning, TEXT("      [%d] ?? Timestamp is negative: %.3f"), i, Frame.Timestamp);
            }
            bFrameValid = false;
            continue;
        }

        // ? 유효한 프레임
        if (bFrameValid)
        {
            ValidationStats.ValidFrames++;

            // 타임스탐프 범위 업데이트
            ValidationStats.FirstTimestamp = FMath::Min(ValidationStats.FirstTimestamp, Frame.Timestamp);
            ValidationStats.LastTimestamp = FMath::Max(ValidationStats.LastTimestamp, Frame.Timestamp);

            // 타임스탬프 간격 확인
            if (i > 0)
            {
                float TimestampGap = Frame.Timestamp - SwingFrames[i - 1].Timestamp;
                if (TimestampGap > 0.0f)
                {
                    ValidationStats.MinTimestampGap = FMath::Min(ValidationStats.MinTimestampGap, TimestampGap);
                    ValidationStats.MaxTimestampGap = FMath::Max(ValidationStats.MaxTimestampGap, TimestampGap);
                }
            }

            // 처음 5개 프레임 상세 로그
            if (i < 5)
            {
                UE_LOG(LogTemp, Log, TEXT("      [%d] ? Valid - Texture size: %dx%d, Time: %.3f"),
                    i,
                    Frame.FrameTexture->GetSizeX(),
                    Frame.FrameTexture->GetSizeY(),
                    Frame.Timestamp);
            }
        }
    }

    // Step 4: 타임스탬프 정렬 확인
    UE_LOG(LogTemp, Warning, TEXT("[3/5] Timestamp Sequence Check"));
    bool bTimestampValid = true;
    for (int32 i = 1; i < SwingFrames.Num(); ++i)
    {
        if (SwingFrames[i].Timestamp < SwingFrames[i - 1].Timestamp)
        {
            UE_LOG(LogTemp, Error, TEXT("      ? Out of order at [%d]: %.3f < %.3f"),
                i, SwingFrames[i].Timestamp, SwingFrames[i - 1].Timestamp);
            bTimestampValid = false;
        }
    }
    if (bTimestampValid && SwingFrames.Num() > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("      ? Timestamps are in correct order"));
    }

    // Step 5: 통계 출력
    UE_LOG(LogTemp, Warning, TEXT("[4/5] Validation Statistics"));
    PrintValidationStats();

    // Step 6: 결론
    UE_LOG(LogTemp, Warning, TEXT("[5/5] Overall Status"));
    if (ValidationStats.ValidFrames == ValidationStats.TotalFrames)
    {
        UE_LOG(LogTemp, Warning, TEXT("      ? ALL FRAMES VALID - Ready for playback"));
    }
    else if (ValidationStats.ValidFrames > ValidationStats.TotalFrames * 0.8f)
    {
        UE_LOG(LogTemp, Warning, TEXT("      ?? MOSTLY VALID (%.1f%%) - Playback may have issues"),
            (float)ValidationStats.ValidFrames / ValidationStats.TotalFrames * 100.0f);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("      ? TOO MANY INVALID FRAMES (%.1f%%) - Playback WILL FAIL"),
            (float)ValidationStats.ValidFrames / ValidationStats.TotalFrames * 100.0f);
    }

    UE_LOG(LogTemp, Warning, TEXT("???????????????????????????????????????????????????????????"));
}

// ========== Step 2-2: ValidateFrameBeforePlayback() - 재생 전 프레임 검증 ==========

void USwingVideoWidget::ValidateFrameBeforePlayback(int32 FrameIndex)
{
    // 성능 최적화: 10프레임마다만 검증
    static int32 ValidationCounter = 0;
    if (++ValidationCounter % 10 != 0 && ValidationCounter > 30)  // 처음 30프레임은 모두 검증
    {
        return;
    }

    if (FrameIndex < 0 || FrameIndex >= SwingFrames.Num())
    {
        UE_LOG(LogTemp, Error, TEXT("? Frame index out of range: %d (total: %d)"),
            FrameIndex, SwingFrames.Num());
        return;
    }

    const FVideoFrame& Frame = SwingFrames[FrameIndex];

    // 텍스처 체크
    if (!Frame.FrameTexture)
    {
        UE_LOG(LogTemp, Error, TEXT("? [%d] Texture is NULL!"), FrameIndex);
        return;
    }

    if (!IsValid(Frame.FrameTexture))
    {
        UE_LOG(LogTemp, Error, TEXT("? [%d] Texture is INVALID!"), FrameIndex);
        return;
    }

    if (Frame.FrameTexture->GetResource() == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("?? [%d] Texture Resource is null"), FrameIndex);
        return;
    }

    // 로그 (100프레임마다)
    if (ValidationCounter % 100 == 0)
    {
        UE_LOG(LogTemp, Log, TEXT("?? [%d] Frame valid - Size: %dx%d, Time: %.3f"),
            FrameIndex,
            Frame.FrameTexture->GetSizeX(),
            Frame.FrameTexture->GetSizeY(),
            Frame.Timestamp);
    }
}

// ========== Step 2-3: PrintValidationStats() - 통계 출력 ==========

void USwingVideoWidget::PrintValidationStats()
{
    UE_LOG(LogTemp, Warning, TEXT("?? ???? VALIDATION STATISTICS ????"));
    UE_LOG(LogTemp, Warning, TEXT("   Total frames:           %d"), ValidationStats.TotalFrames);
    UE_LOG(LogTemp, Warning, TEXT("   Valid frames:           %d (%.1f%%)"),
        ValidationStats.ValidFrames,
        ValidationStats.TotalFrames > 0 ? (float)ValidationStats.ValidFrames / ValidationStats.TotalFrames * 100.0f : 0.0f);
    UE_LOG(LogTemp, Warning, TEXT("   NULL texture frames:    %d"), ValidationStats.NullTextureFrames);
    UE_LOG(LogTemp, Warning, TEXT("   Invalid texture frames: %d"), ValidationStats.InvalidTextureFrames);
    UE_LOG(LogTemp, Warning, TEXT("   Resource null frames:   %d"), ValidationStats.ResourceNullFrames);
    UE_LOG(LogTemp, Warning, TEXT("   Time range:             %.2f ~ %.2f sec"),
        ValidationStats.FirstTimestamp, ValidationStats.LastTimestamp);
    UE_LOG(LogTemp, Warning, TEXT("   Duration:               %.2f sec"),
        ValidationStats.LastTimestamp - ValidationStats.FirstTimestamp);
    UE_LOG(LogTemp, Warning, TEXT("   Timestamp gap:          min=%.4f, max=%.4f sec"),
        ValidationStats.MinTimestampGap, ValidationStats.MaxTimestampGap);
    UE_LOG(LogTemp, Warning, TEXT("   ??????????????????????????"));
}

// ========== Step 2-4: DiagnosePlaybackFailure() - 재생 실패 진단 ==========

void USwingVideoWidget::DiagnosePlaybackFailure()
{
    UE_LOG(LogTemp, Error, TEXT("???????????????????????????????????????????????????????????"));
    UE_LOG(LogTemp, Error, TEXT("?? PLAYBACK FAILURE DIAGNOSIS"));
    UE_LOG(LogTemp, Error, TEXT("???????????????????????????????????????????????????????????"));

    // 1. 기본 상태 확인
    UE_LOG(LogTemp, Error, TEXT("[1/5] Basic State"));
    UE_LOG(LogTemp, Error, TEXT("   SwingFrames.Num(): %d"), SwingFrames.Num());
    UE_LOG(LogTemp, Error, TEXT("   CurrentVideoMode: %s"),
        CurrentVideoMode == EVideoMode::SwingPlayback ? TEXT("SwingPlayback") : TEXT("LiveFeed"));
    UE_LOG(LogTemp, Error, TEXT("   bIsPlaying: %s"), bIsPlaying ? TEXT("TRUE") : TEXT("FALSE"));
    UE_LOG(LogTemp, Error, TEXT("   VideoDisplay valid: %s"),
        (VideoDisplay && IsValid(VideoDisplay)) ? TEXT("YES") : TEXT("NO"));

    // 2. 현재 프레임 상태 확인
    UE_LOG(LogTemp, Error, TEXT("[2/5] Current Frame State"));
    UE_LOG(LogTemp, Error, TEXT("   CurrentFrameIndex: %d"), CurrentFrameIndex);

    if (CurrentFrameIndex >= 0 && CurrentFrameIndex < SwingFrames.Num())
    {
        const FVideoFrame& Frame = SwingFrames[CurrentFrameIndex];
        UE_LOG(LogTemp, Error, TEXT("   Frame texture: %s"), Frame.FrameTexture ? TEXT("Valid") : TEXT("NULL"));
        if (Frame.FrameTexture)
        {
            UE_LOG(LogTemp, Error, TEXT("   Texture valid: %s"), IsValid(Frame.FrameTexture) ? TEXT("YES") : TEXT("NO"));
            UE_LOG(LogTemp, Error, TEXT("   Texture size: %dx%d"),
                Frame.FrameTexture->GetSizeX(), Frame.FrameTexture->GetSizeY());
            UE_LOG(LogTemp, Error, TEXT("   Timestamp: %.3f"), Frame.Timestamp);
        }
    }

    // 3. WebcamCapture 상태 확인
    UE_LOG(LogTemp, Error, TEXT("[3/5] WebcamCapture State"));
    if (WebcamCaptureRef.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("   WebcamCapture: Valid"));
        UE_LOG(LogTemp, Error, TEXT("   MediaPlayer: %s"),
            WebcamCaptureRef->MediaPlayer ? TEXT("Valid") : TEXT("NULL"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("   WebcamCapture: INVALID"));
    }

    // 4. 프레임 분석
    UE_LOG(LogTemp, Error, TEXT("[4/5] Frame Analysis"));
    int32 NullCount = 0;
    int32 InvalidCount = 0;
    int32 ResourceNullCount = 0;

    for (int32 i = 0; i < SwingFrames.Num(); ++i)
    {
        const FVideoFrame& Frame = SwingFrames[i];
        if (!Frame.FrameTexture)
            NullCount++;
        else if (!IsValid(Frame.FrameTexture))
            InvalidCount++;
        else if (Frame.FrameTexture->GetResource() == nullptr)
            ResourceNullCount++;
    }

    UE_LOG(LogTemp, Error, TEXT("   NULL textures: %d"), NullCount);
    UE_LOG(LogTemp, Error, TEXT("   Invalid textures: %d"), InvalidCount);
    UE_LOG(LogTemp, Error, TEXT("   Resource null: %d"), ResourceNullCount);

    // 5. 권장사항
    UE_LOG(LogTemp, Error, TEXT("[5/5] Recommendations"));
    if (SwingFrames.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("   ?? SetSwingFrames() was not called with valid frames"));
        UE_LOG(LogTemp, Error, TEXT("   ?? Check ProcessShotRecording() in WebcamCapture"));
    }
    else if (NullCount > 0 || InvalidCount > 0)
    {
        UE_LOG(LogTemp, Error, TEXT("   ?? Some frames have NULL or invalid textures"));
        UE_LOG(LogTemp, Error, TEXT("   ?? Check ExtractSwingFrames() in WebcamCapture"));
        UE_LOG(LogTemp, Error, TEXT("   ?? Check GetFramesInRange() in VideoBufferComponent"));
    }
    else if (ResourceNullCount > 0)
    {
        UE_LOG(LogTemp, Error, TEXT("   ?? Texture resources are not initialized"));
        UE_LOG(LogTemp, Error, TEXT("   ?? Check CreateTexture2DFromPixels() in WebcamCapture"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("   ?? All frames appear valid but playback failed"));
        UE_LOG(LogTemp, Error, TEXT("   ?? Check VideoDisplay widget configuration"));
    }

    UE_LOG(LogTemp, Error, TEXT("???????????????????????????????????????????????????????????"));
}

void USwingVideoWidget::PlayVideoFile(const FString& FilePath)
{
    UE_LOG(LogTemp, Log, TEXT("?? PlayVideoFile called: %s"), *FilePath);

    // ? 1. FileMediaSource 생성
    if (!FileMediaSource)
    {
        FileMediaSource = NewObject<UFileMediaSource>(this);
        UE_LOG(LogTemp, Log, TEXT("  ? FileMediaSource created"));
    }

    // ? 2. 파일 경로 설정
    FileMediaSource->SetFilePath(FilePath);

    // ? 3. FileMediaPlayer 생성
    if (!FileMediaPlayer)
    {
        FileMediaPlayer = NewObject<UMediaPlayer>(this);
        FileMediaPlayer->SetLooping(false);  // 테스트용 루프 재생
        FileMediaPlayer->PlayOnOpen = true;

        // ? 이벤트 바인딩
        //FileMediaPlayer->OnMediaOpened.AddDynamic(this, &USwingVideoWidget::OnFileMediaOpened);
        //FileMediaPlayer->OnMediaOpenFailed.AddDynamic(this, &USwingVideoWidget::OnFileMediaOpenFailed);

        UE_LOG(LogTemp, Log, TEXT("  ? FileMediaPlayer created"));
    }

    // ? 4. FileMediaTexture 생성
    if (!FileMediaTexture)
    {
        FileMediaTexture = NewObject<UMediaTexture>(this);
        FileMediaTexture->SetMediaPlayer(FileMediaPlayer);
        FileMediaTexture->UpdateResource();
        UE_LOG(LogTemp, Log, TEXT("  ? FileMediaTexture created"));
    }

    // ? 5. 모드 전환
    CurrentVideoMode = EVideoMode::FilePlayback;

    if (StatusText)
    {
        StatusText->SetText(FText::FromString(TEXT("동영상 로딩 중...")));
    }

    // ? 6. MediaPlayer에 소스 열기
    // 주의: Brush 설정은 OnMediaOpened 이벤트에서 수행
    if (FileMediaPlayer->OpenSource(FileMediaSource))
    {
        UE_LOG(LogTemp, Log, TEXT("  ? MediaPlayer OpenSource called, waiting for OnMediaOpened event..."));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("  ? Failed to call OpenSource: %s"), *FilePath);
    }
}

void USwingVideoWidget::StopVideoFile()
{
    if (FileMediaPlayer && IsValid(FileMediaPlayer))
    {
        FileMediaPlayer->Close();
        UE_LOG(LogTemp, Log, TEXT("?? VideoFile stopped"));
    }

    // LiveFeed로 복귀
    SwitchToLiveFeed();
}


// ? 새로운 함수 추가
void USwingVideoWidget::UpdateFilePlayback()
{
    // ? bIsPlaying 조건 제거 - 탐색(seek) 중에도 렌더되어야 함
    if (!IsValid(this) || !VideoDisplay) return;
    if (ClipFramePaths.Num() == 0 || CurrentClipFrameIndex < 0) return;
    if (!ClipFramePaths.IsValidIndex(CurrentClipFrameIndex)) return;

    // 3. 현재 프레임 가져오기
    UTexture2D* Frame = LoadTextureFromJPG(ClipFramePaths[CurrentClipFrameIndex]);

    // 3. ? 크래시 지점 수정: 반드시 Frame이 유효한지 먼저 확인 후 Resource 체크
    if (Frame && IsValid(Frame))
    {
        if (Frame->GetResource())
        {
            VideoDisplay->SetBrushFromTexture(Frame);
        }
        else
        {
            // 리소스가 없으면 업데이트 요청 후 대기
            Frame->UpdateResource();
            UE_LOG(LogTemp, Warning, TEXT("?? Frame %d: Resource missing, updating..."), CurrentClipFrameIndex);
        }
    }
    else
    {
        // 로드 실패 시 로그만 남기고 리턴 (크래시 방지)
        UE_LOG(LogTemp, Error, TEXT("? Failed to load frame %d from path"), CurrentClipFrameIndex);
    }


    // 5. 화면에 프레임 표시 (핵심!)
    //if (CurrentFrame.FrameTexture && CurrentFrame.FrameTexture->GetResource())
    //{
    //    if (LastSetTexture != CurrentFrame.FrameTexture)
    //    {
    //        VideoDisplay->SetBrushFromTexture(CurrentFrame.FrameTexture);
    //        LastSetTexture = CurrentFrame.FrameTexture;
    //        bHasEverSetSwingBrush = true;
    //    }
    //}
}
// ? 테스트 함수 구현 (cpp 파일 마지막에 추가)
void USwingVideoWidget::TestPlaySavedVideo()
{
    UE_LOG(LogTemp, Warning, TEXT("?? Testing video playback from Saved/Swing.avi"));

    //HandleFilePlaybackMode();

    SwitchToFilePlayback();

    // Saved 폴더의 절대 경로 생성
    FString ProjectDir = FPaths::ProjectDir();
    FString VideoPath = FPaths::Combine(ProjectDir, TEXT("Saved"), TEXT("Swing/Swing.avi"));

    // 경로를 절대 경로로 변환
    VideoPath = FPaths::ConvertRelativePathToFull(VideoPath);

    UE_LOG(LogTemp, Warning, TEXT("?? Video Path: %s"), *VideoPath);

    // 파일 존재 확인
    if (!FPaths::FileExists(VideoPath))
    {
        UE_LOG(LogTemp, Error, TEXT("? Video file does not exist: %s"), *VideoPath);

        // Saved 폴더 내용 확인
        FString SavedDir = FPaths::Combine(ProjectDir, TEXT("Saved"));
        TArray<FString> FoundFiles;
        IFileManager::Get().FindFiles(FoundFiles, *SavedDir, TEXT("*.avi"));

        UE_LOG(LogTemp, Warning, TEXT("?? Files in Saved directory:"));
        for (const FString& File : FoundFiles)
        {
            UE_LOG(LogTemp, Warning, TEXT("   - %s"), *File);
        }

        return;
    }

    // 동영상 파일 재생
    PlayVideoFile(VideoPath);

    UE_LOG(LogTemp, Warning, TEXT("? Video playback started"));
}

// ? MediaPlayer 이벤트 핸들러 추가
void USwingVideoWidget::OnFileMediaOpened(FString OpenedUrl)
{
    UE_LOG(LogTemp, Warning, TEXT("? File Media Opened Successfully: %s"), *OpenedUrl);

    if (FileMediaPlayer && IsValid(FileMediaPlayer))
    {
        UE_LOG(LogTemp, Log, TEXT("   Duration: %.2f seconds"),
            FileMediaPlayer->GetDuration().GetTotalSeconds());
        UE_LOG(LogTemp, Log, TEXT("   Video Track Dimensions: %dx%d"),
            FileMediaPlayer->GetVideoTrackDimensions(0, 0).X,
            FileMediaPlayer->GetVideoTrackDimensions(0, 0).Y);
    }

    // ? MediaTexture가 준비될 때까지 대기 후 Brush 설정
    if (GetWorld())
    {
        FTimerHandle BrushSetupTimer;
        GetWorld()->GetTimerManager().SetTimer(
            BrushSetupTimer,
            [this]()
            {
                if (VideoDisplay && IsValid(VideoDisplay) && FileMediaTexture && IsValid(FileMediaTexture))
                {
                    // ? MediaTexture Resource 유효성 확인
                    if (FileMediaTexture->GetResource() != nullptr)
                    {
                        FSlateBrush Brush;
                        Brush.SetResourceObject(FileMediaTexture);
                        Brush.ImageSize = FVector2D(1920, 1080);
                        Brush.DrawAs = ESlateBrushDrawType::Image;

                        VideoDisplay->SetBrush(Brush);

                        if (StatusText)
                        {
                            StatusText->SetText(FText::FromString(TEXT("동영상 재생 중")));
                        }

                        UE_LOG(LogTemp, Warning, TEXT("  ? VideoDisplay brush updated successfully"));
                    }
                    else
                    {
                        UE_LOG(LogTemp, Error, TEXT("  ? FileMediaTexture Resource is still null"));
                    }
                }
            },
            0.1f,  // 100ms 대기
            false
        );
    }
}

void USwingVideoWidget::OnFileMediaOpenFailed(FString FailedUrl)
{
    UE_LOG(LogTemp, Error, TEXT("? File Media Open FAILED: %s"), *FailedUrl);

    if (StatusText)
    {
        StatusText->SetText(FText::FromString(TEXT("Fail Play")));
    }

    // LiveFeed로 복귀
    CurrentVideoMode = EVideoMode::LiveFeed;
}


void USwingVideoWidget::HandleLiveFeedMode(float InDeltaTime)
{
    // ? LiveFeed 모드 전용 로직

    // WebcamCapture 유효성 검사
    if (!WebcamCaptureRef.IsValid())
    {
        return;
    }

    // MediaTexture 유효성 검사
    if (!WebcamCaptureRef->MediaTexture ||
        !IsValid(WebcamCaptureRef->MediaTexture) ||
        WebcamCaptureRef->MediaTexture->GetResource() == nullptr)
    {
        return;
    }

    // ? 주기적 MediaPlayer 상태 확인 (5초마다)
    static float MediaCheckTimer = 0.0f;
    MediaCheckTimer += InDeltaTime;

    if (MediaCheckTimer >= 5.0f)
    {
        MediaCheckTimer = 0.0f;
        CheckAndRestartMediaPlayer();
    }
}

void USwingVideoWidget::HandleSwingPlaybackMode(float InDeltaTime)
{
    // ? SwingPlayback 모드 전용 로직

    // 재생 중이 아니면 종료
    if (!bIsPlaying || bIsPaused)
    {
        return;
    }

    // 프레임 유효성 검사
    if (!SwingFrames.IsValidIndex(CurrentFrameIndex))
    {
        bIsPlaying = false;
        bIsPaused = false;
        return;
    }

    // 첫 프레임 설정 대기
    if (!bHasEverSetSwingBrush)
    {
        SetVideoFrame(SwingFrames[CurrentFrameIndex]);
        return;
    }

    // ? DeltaTime 누적
    PlaybackTimerAccumulator += InDeltaTime;

    // ? 프레임 간격 계산 (30fps = 0.0333초)
    const float FrameInterval = 1.0f / FrameRate;

    // ? 프레임 진행 처리
    while (PlaybackTimerAccumulator >= FrameInterval)
    {
        // 재생 시간 증가
        CurrentPlaybackTime += FrameInterval * PlaybackSpeed;
        PlaybackTimerAccumulator -= FrameInterval;

        // 재생 종료 확인
        if (CurrentPlaybackTime >= TotalPlaybackDuration)
        {
            if (bLoopPlayback)
            {
                // 루프 재생
                CurrentPlaybackTime = 0.0f;
                CurrentFrameIndex = 0;
                PlaybackTimerAccumulator = 0.0f;
                UE_LOG(LogTemp, Log, TEXT("?? Swing video looping"));
            }
            else
            {
                // 재생 종료
                StopSwingVideo();
                UE_LOG(LogTemp, Log, TEXT("?? Swing video playback finished"));
                return;
            }
        }

        // 현재 시간에 맞는 프레임 인덱스 계산
        UpdateCurrentFrameIndex();
    }
}

void USwingVideoWidget::HandleFilePlaybackMode(float InDeltaTime)
{
    // ? FilePlayback 모드 전용 로직

    // FileMediaPlayer 유효성 검사
    if (!FileMediaPlayer || !IsValid(FileMediaPlayer))
    {
        return;
    }

    // MediaPlayer 에러 확인
    if (FileMediaPlayer->HasError())
    {
        UE_LOG(LogTemp, Error, TEXT("? FileMediaPlayer has error"));

        // ? 1. FileMediaPlayer 정리
        StopVideoFile();

        // ? 2. 모드 변경 (CleanupCurrentMode 호출하지 않음 - 무한 루프 방지)
        CurrentVideoMode = EVideoMode::LiveFeed;

        // ? 3. UI 업데이트
        if (StatusText)
        {
            StatusText->SetText(FText::FromString(TEXT("실시간 영상")));
        }

        if (ControlsBox)
        {
            ControlsBox->SetVisibility(ESlateVisibility::Collapsed);
        }

        // ? 4. VideoDisplay 초기화
        if (VideoDisplay && IsValid(VideoDisplay))
        {
            FSlateBrush EmptyBrush;
            VideoDisplay->SetBrush(EmptyBrush);
        }

        UE_LOG(LogTemp, Warning, TEXT("?? Switched to LiveFeed due to FileMediaPlayer error"));
        return;
    }

    // 재생 중일 때만 시간 업데이트
    if (FileMediaPlayer->IsPlaying())
    {
        // MediaPlayer에서 현재 재생 시간 가져오기
        FTimespan CurrentTime = FileMediaPlayer->GetTime();
        FTimespan Duration = FileMediaPlayer->GetDuration();

        CurrentPlaybackTime = static_cast<float>(CurrentTime.GetTotalSeconds());
        TotalPlaybackDuration = static_cast<float>(Duration.GetTotalSeconds());

        // 재생 완료 확인
        if (CurrentTime >= Duration)
        {
            if (bLoopPlayback)
            {
                FileMediaPlayer->Seek(FTimespan::Zero());
                CurrentPlaybackTime = 0.0f;
                UE_LOG(LogTemp, Log, TEXT("?? File video looping"));
            }
            else
            {
                StopVideoFile();
                UE_LOG(LogTemp, Log, TEXT("?? File video playback finished"));
            }
        }
    }

    // ???????????????????????????????????????????????????????????????????????????
        // ? 프레임 로드 및 표시 (SetBrushFromTexture 사용)
        // ???????????????????????????????????????????????????????????????????????????

    if (CurrentClipFrameIndex >= 0 && CurrentClipFrameIndex < ClipFramePaths.Num())
    {
        UTexture2D* Frame = LoadTextureFromJPG(ClipFramePaths[CurrentClipFrameIndex]);

        if (Frame && IsValid(Frame) && Frame->GetResource())
        {
            // ? [수정] BlitFrameToRT → SetBrushFromTexture
            VideoDisplay->SetBrushFromTexture(Frame);

            // ? 로그 (매 30프레임마다)
            if (CurrentClipFrameIndex % 30 == 0)
            {
                UE_LOG(LogTemp, Log, TEXT("? Frame %d/%d displayed (SetBrushFromTexture)"),
                    CurrentClipFrameIndex + 1, ClipFramePaths.Num());
            }
        }
        else
        {
            // ? 프레임 로드 실패 진단
            static int32 ErrorCount = 0;
            if (++ErrorCount <= 3)
            {
                if (Frame == nullptr)
                    UE_LOG(LogTemp, Error, TEXT("? Frame %d: NULL"), CurrentClipFrameIndex);
                if (!IsValid(Frame))
                    UE_LOG(LogTemp, Error, TEXT("? Frame %d: INVALID"), CurrentClipFrameIndex);
                if (!Frame->GetResource())
                    UE_LOG(LogTemp, Error, TEXT("? Frame %d: NO RESOURCE"), CurrentClipFrameIndex);
            }
        }
    }
}



void USwingVideoWidget::HandleClipPlaybackMode(float InDeltaTime)
{
    if (!bIsPlaying || bIsPaused) return;

    PlaybackTimerAccumulator += InDeltaTime;
    const float FrameInterval = 1.0f / ClipFPS;

    while (PlaybackTimerAccumulator >= FrameInterval)
    {
        PlaybackTimerAccumulator -= FrameInterval;
        CurrentPlaybackTime += FrameInterval;

        // ── 재생 종료 체크 ─────────────────────────────────────────
        if (CurrentClipFrameIndex >= ClipFramePaths.Num() - 1)
        {
            if (bLoopPlayback)
            {
                CurrentClipFrameIndex = 0;
                CurrentPlaybackTime = 0.0f;
            }
            else
            {
                bIsPlaying = false;
                CurrentClipFrameIndex = 0;
                CurrentPlaybackTime = 0.0f;
                PlaybackTimerAccumulator = 0.0f;

                if (ProgressSlider && IsValid(ProgressSlider))
                {
                    ProgressSlider->OnValueChanged.RemoveDynamic(this, &USwingVideoWidget::OnProgressSliderChanged);
                    ProgressSlider->SetValue(0.0f);
                    ProgressSlider->OnValueChanged.AddDynamic(this, &USwingVideoWidget::OnProgressSliderChanged);
                }
                if (TimeText && IsValid(TimeText))
                    TimeText->SetText(FText::FromString(TEXT("00:00 / 00:00")));

                UE_LOG(LogTemp, Log, TEXT("? Clip finished (%d frames)"), ClipFramePaths.Num());

                // [FIX #4] bClipFinishBroadcasted 로 이중 Broadcast 방지
                if (!bClipFinishBroadcasted)
                {
                    bClipFinishBroadcasted = true;
                    OnClipPlaybackFinished.Broadcast();
                }
                return;  // ← while 탈출 후 아래 중복 체크로 가지 않도록
            }
        }
        else
        {
            ++CurrentClipFrameIndex;
        }

        // ── 프레임 표시 ────────────────────────────────────────────
        UTexture2D* Frame = nullptr;
        if (bUseTextureCache && LoadedFrameTextures.IsValidIndex(CurrentClipFrameIndex))
            Frame = LoadedFrameTextures[CurrentClipFrameIndex];

        if (Frame && IsValid(Frame) && Frame->GetResource())
        {
            VideoDisplay->SetBrushFromTexture(Frame);
            if (CurrentClipFrameIndex % 30 == 0)
                UE_LOG(LogTemp, Log, TEXT("▶ Frame %d/%d"), CurrentClipFrameIndex + 1, ClipFramePaths.Num());
        }
    }

    // [FIX #4] while 이후 중복 종료 체크 블록 완전 제거
    // (이전 코드: while 종료 뒤 또다시 ClipFrameIndex 체크 → Broadcast 2회 발생)
}


void USwingVideoWidget::PreloadNextFrame()
{
    int32 NextIndex = CurrentClipFrameIndex + 1;

    if (NextIndex >= 0 && NextIndex < ClipFramePaths.Num())
    {
        // 기존 다음 프레임 정리
        if (NextFrameTexture && IsValid(NextFrameTexture))
        {
            NextFrameTexture->MarkAsGarbage();
            NextFrameTexture = nullptr;
        }

        // 새 프레임 로드
        NextFrameTexture = LoadTextureFromJPG(ClipFramePaths[NextIndex]);
    }
}

void USwingVideoWidget::SwitchToFilePlayback()
{
    UE_LOG(LogTemp, Log, TEXT("?? Switching to FilePlayback mode"));

    // ? 1. 이전 모드 정리
    CleanupCurrentMode();

    // ? 2. 모드 변경
    CurrentVideoMode = EVideoMode::FilePlayback;

    // ? 3. UI 업데이트
    if (StatusText)
    {
        StatusText->SetText(FText::FromString(TEXT("파일 재생")));
    }

    if (ControlsBox)
    {
        ControlsBox->SetVisibility(ESlateVisibility::Visible);
    }

    UE_LOG(LogTemp, Log, TEXT("? Switched to FilePlayback mode"));
}

void USwingVideoWidget::CleanupCurrentMode()
{
    UE_LOG(LogTemp, Log, TEXT("?? Cleaning up current mode: %d"), (int32)CurrentVideoMode);

    switch (CurrentVideoMode)
    {
    case EVideoMode::LiveFeed:
        // LiveFeed는 별도 정리 필요 없음 (MediaPlayer는 WebcamCapture가 관리)
        break;

    case EVideoMode::SwingPlayback:
        // 스윙 재생 중지
        StopSwingVideo();
        break;

    case EVideoMode::FilePlayback:
        // 파일 재생 중지
        StopVideoFile();
        break;
    }

    // ? 수정: VideoDisplay 브러시를 검은색으로 설정
    if (VideoDisplay && IsValid(VideoDisplay))
    {
        FSlateBrush BlackBrush;
        BlackBrush.TintColor = FSlateColor(FLinearColor::Black);  // ? 검은색 설정
        VideoDisplay->SetBrush(BlackBrush);
    }
}

/*
void USwingVideoWidget::DiagnoseLiveFeedIssue()
{
    UE_LOG(LogTemp, Warning, TEXT("========================================"));
    UE_LOG(LogTemp, Warning, TEXT("?? LiveFeed Diagnostic"));
    UE_LOG(LogTemp, Warning, TEXT("========================================"));

    UE_LOG(LogTemp, Log, TEXT("Current Mode: %d (0=LiveFeed, 1=SwingPlayback, 2=FilePlayback)"),
        (int32)CurrentVideoMode);

    if (!WebcamCaptureRef.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("? WebcamCaptureRef is INVALID"));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("? WebcamCaptureRef is valid"));

        if (!WebcamCaptureRef->MediaPlayer || !IsValid(WebcamCaptureRef->MediaPlayer))
        {
            UE_LOG(LogTemp, Error, TEXT("  ? MediaPlayer is INVALID"));
        }
        else
        {
            UMediaPlayer* MP = WebcamCaptureRef->MediaPlayer;
            UE_LOG(LogTemp, Log, TEXT("  ? MediaPlayer is valid"));
            UE_LOG(LogTemp, Log, TEXT("    IsPlaying: %s"), MP->IsPlaying() ? TEXT("YES") : TEXT("NO"));
            UE_LOG(LogTemp, Log, TEXT("    IsPreparing: %s"), MP->IsPreparing() ? TEXT("YES") : TEXT("NO"));
            UE_LOG(LogTemp, Log, TEXT("    HasError: %s"), MP->HasError() ? TEXT("YES") : TEXT("NO"));
        }

        if (!WebcamCaptureRef->MediaTexture || !IsValid(WebcamCaptureRef->MediaTexture))
        {
            UE_LOG(LogTemp, Error, TEXT("  ? MediaTexture is INVALID"));
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("  ? MediaTexture is valid"));
            UE_LOG(LogTemp, Log, TEXT("    Resource: %s"),
                WebcamCaptureRef->MediaTexture->GetResource() ? TEXT("VALID") : TEXT("NULL"));
        }

        UMaterialInstanceDynamic* CachedMat = WebcamCaptureRef->GetCachedDynamicMaterial();
        if (!CachedMat || !IsValid(CachedMat))
        {
            UE_LOG(LogTemp, Error, TEXT("  ? CachedDynamicMaterial is INVALID"));
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("  ? CachedDynamicMaterial is valid"));
            UE_LOG(LogTemp, Log, TEXT("    RenderProxy: %s"),
                CachedMat->GetRenderProxy() ? TEXT("VALID") : TEXT("NULL"));
        }
    }

    if (!VideoDisplay || !IsValid(VideoDisplay))
    {
        UE_LOG(LogTemp, Error, TEXT("? VideoDisplay is INVALID"));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("? VideoDisplay is valid"));

        const FSlateBrush* CurrentBrush = &VideoDisplay->GetBrush();
        if (CurrentBrush)
        {
            UObject* BrushResource = CurrentBrush->GetResourceObject();
            if (BrushResource)
            {
                UE_LOG(LogTemp, Log, TEXT("  Current Brush Resource: %s"),
                    *BrushResource->GetName());
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("  ?? Brush has no resource object"));
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("========================================"));
}
*/

// SwingVideoWidget.cpp

void USwingVideoWidget::PlaySwingClipFromDirectory(const FString& ClipDirectory)
{
    UE_LOG(LogTemp, Warning, TEXT("???????????????????????????????????????"));
    UE_LOG(LogTemp, Warning, TEXT("?? PlaySwingClipFromDirectory START"));
    UE_LOG(LogTemp, Warning, TEXT("   Dir: %s"), *ClipDirectory);
    UE_LOG(LogTemp, Warning, TEXT("???????????????????????????????????????"));

    // [FIX #1] 새 타이머 생성 전 기존 ValidatingTimer 반드시 Clear
    if (GetWorld())
        GetWorld()->GetTimerManager().ClearTimer(ValidatingTimerHandle);

    // [FIX #3] 이미 Async 작업 중이면 중복 실행 방지
    if (bAsyncPreloadInProgress)
    {
        UE_LOG(LogTemp, Warning, TEXT("?? Async preload already in progress, skipping"));
        return;
    }

    // ── Step 1: 디렉토리 확인 ───────────────────────────────────────────────
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*ClipDirectory))
    {
        UE_LOG(LogTemp, Error, TEXT("? Directory not found: %s"), *ClipDirectory);
        return;
    }

    // ── Step 2: 이전 상태 정리 ──────────────────────────────────────────────
    bIsPlaying = false;
    bIsPaused = false;
    bClipFinishBroadcasted = false;

    for (UTexture2D* Tex : LoadedFrameTextures)
    {
        if (Tex && IsValid(Tex)) { Tex->RemoveFromRoot(); Tex->MarkAsGarbage(); }
    }
    LoadedFrameTextures.Empty();
    bUseTextureCache = false;

    ClipFramePaths.Empty();
    CurrentClipDirectory = TEXT("");
    CurrentClipFrameIndex = 0;
    CurrentPlaybackTime = 0.0f;
    PlaybackTimerAccumulator = 0.0f;
    ExpectedTotalFrames = 0;

    // ── Step 3: 메타데이터 로드 ─────────────────────────────────────────────
    FClipMetadata Metadata;
    if (LoadClipMetadata(ClipDirectory, Metadata))
    {
        ClipFPS = (Metadata.FPS > 0.0f) ? Metadata.FPS : 30.0f;
        // [FIX #2] 메타데이터의 실제 프레임 수를 저장
        ExpectedTotalFrames = Metadata.TotalFrames;
        UE_LOG(LogTemp, Log, TEXT("   ? Metadata: %d frames, %.1f fps"),
            Metadata.TotalFrames, ClipFPS);
    }
    else
    {
        ClipFPS = 30.0f;
        ExpectedTotalFrames = 0;
        UE_LOG(LogTemp, Warning, TEXT("   ?? No metadata, default FPS=30"));
    }

    // ── Step 4: JPG 파일 목록 수집 + 정렬 ──────────────────────────────────
    TArray<FString> AllFiles;
    IFileManager::Get().FindFiles(
        AllFiles, *FPaths::Combine(ClipDirectory, TEXT("*")), true, false);

    for (const FString& FileName : AllFiles)
    {
        if (FileName.EndsWith(TEXT(".jpg"), ESearchCase::IgnoreCase) ||
            FileName.EndsWith(TEXT(".jpeg"), ESearchCase::IgnoreCase))
        {
            ClipFramePaths.Add(FPaths::Combine(ClipDirectory, FileName));
        }
    }

    ClipFramePaths.Sort([](const FString& A, const FString& B)
        {
            int32 NumA = FCString::Atoi(*FPaths::GetBaseFilename(A));
            int32 NumB = FCString::Atoi(*FPaths::GetBaseFilename(B));
            return (NumA != 0 || NumB != 0) ? (NumA < NumB) : (A < B);
        });

    if (ClipFramePaths.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("? No JPG files found in: %s"), *ClipDirectory);
        return;
    }

    CurrentClipDirectory = ClipDirectory;
    TotalPlaybackDuration = (float)ClipFramePaths.Num() / ClipFPS;

    UE_LOG(LogTemp, Log, TEXT("   ?? Found %d JPG files"), ClipFramePaths.Num());

    // [FIX #2] 임계값 비교: 140 하드코딩 → 실제 파일 수 vs 메타데이터 프레임 수
    // ExpectedTotalFrames == 0 이면 메타데이터 없음 → 현재 파일 수로 그냥 재생
    const int32 RequiredFiles = (ExpectedTotalFrames > 0) ? ExpectedTotalFrames : ClipFramePaths.Num();
    if (ClipFramePaths.Num() < RequiredFiles)
    {
        //float Progress = (float)ClipFramePaths.Num() / (float)RequiredFiles * 100.0f;
        //UE_LOG(LogTemp, Warning, TEXT("? Not enough files yet (%d < %d)"),
        //    ClipFramePaths.Num(), RequiredFiles);
        //UE_LOG(LogTemp, Warning, TEXT("   Progress: %.1f%% - Retrying in 2 seconds..."), Progress);

        //if (StatusText)
        //    StatusText->SetText(FText::FromString(FString::Printf(
        //        TEXT("파일 저장 중... %.0f%%"), Progress)));

        // [FIX #1] ValidatingTimerHandle 에 타이머를 설정 → Clear 가능
        if (GetWorld())
        {
            GetWorld()->GetTimerManager().SetTimer(
                ValidatingTimerHandle,          // ← 핸들 저장
                [this, ClipDirectory]()
                {
                    ValidatingTimerHandle.Invalidate();
                    PlaySwingClipFromDirectory(ClipDirectory);  // 재시도
                },
                2.0f, false);
        }
        return;
    }

    if (StatusText)
        StatusText->SetText(FText::FromString(TEXT("로딩 중...")));

    // ── Step 5: Async 프리로드 ──────────────────────────────────────────────
    bAsyncPreloadInProgress = true;

    TArray<FString> PathsCopy = ClipFramePaths;
    float           FPSCopy = ClipFPS;

    // [FIX #3] TWeakObjectPtr 로 this 댕글링 방지
    TWeakObjectPtr<USwingVideoWidget> WeakThis(this);

    Async(EAsyncExecution::Thread, [WeakThis, PathsCopy, FPSCopy]()
        {
            TArray<TArray<uint8>> RawFrames;
            TArray<FIntPoint>     FrameSizes;
            RawFrames.SetNum(PathsCopy.Num());
            FrameSizes.SetNum(PathsCopy.Num());

            IImageWrapperModule& IWM =
                FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");

            for (int32 i = 0; i < PathsCopy.Num(); ++i)
            {
                TArray<uint8> FileData;
                if (!FFileHelper::LoadFileToArray(FileData, *PathsCopy[i])) continue;

                auto Wrapper = IWM.CreateImageWrapper(EImageFormat::JPEG);
                if (!Wrapper.IsValid()) continue;
                if (!Wrapper->SetCompressed(FileData.GetData(), FileData.Num())) continue;

                TArray64<uint8> Raw;
                if (Wrapper->GetRaw(ERGBFormat::BGRA, 8, Raw))
                {
                    RawFrames[i].Append(Raw.GetData(), Raw.Num());
                    FrameSizes[i] = FIntPoint(Wrapper->GetWidth(), Wrapper->GetHeight());
                }
            }

            AsyncTask(ENamedThreads::GameThread, [WeakThis, RawFrames, FrameSizes, FPSCopy]()
                {
                    // [FIX #3] 게임 스레드 복귀 시 위젯 유효성 재확인
                    if (!WeakThis.IsValid())
                    {
                        UE_LOG(LogTemp, Warning, TEXT("?? SwingVideoWidget destroyed before async preload completed"));
                        return;
                    }

                    USwingVideoWidget* Self = WeakThis.Get();
                    Self->bAsyncPreloadInProgress = false;

                    Self->LoadedFrameTextures.SetNumZeroed(RawFrames.Num());
                    int32 SuccessCount = 0;

                    for (int32 i = 0; i < RawFrames.Num(); ++i)
                    {
                        if (RawFrames[i].Num() == 0) continue;

                        UTexture2D* Tex = UTexture2D::CreateTransient(
                            FrameSizes[i].X, FrameSizes[i].Y, PF_B8G8R8A8);
                        if (!Tex) continue;

                        Tex->SRGB = true;

#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION <= 26
                        auto& Mip = Tex->PlatformData->Mips[0];
#else
                        auto& Mip = Tex->GetPlatformData()->Mips[0];
#endif
                        void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
                        FMemory::Memcpy(Data, RawFrames[i].GetData(), RawFrames[i].Num());
                        Mip.BulkData.Unlock();
                        Tex->UpdateResource();

                        Tex->AddToRoot();
                        Self->LoadedFrameTextures[i] = Tex;
                        ++SuccessCount;
                    }

                    Self->bUseTextureCache = true;
                    Self->CurrentVideoMode = EVideoMode::SwingPlayback;
                    Self->TotalPlaybackDuration = (float)SuccessCount / FPSCopy;
                    Self->CurrentClipFrameIndex = 0;
                    Self->CurrentPlaybackTime = 0.0f;
                    Self->bLoopPlayback = false;
                    Self->bIsPlaying = true;
                    Self->bIsPaused = false;
                    Self->bClipFinishBroadcasted = false;

                    if (Self->StatusText)
                        Self->StatusText->SetText(FText::FromString(TEXT("스윙 재생")));

                    if (Self->LoadedFrameTextures.IsValidIndex(0) && Self->LoadedFrameTextures[0])
                        Self->BlitFrameToRT(Self->LoadedFrameTextures[0]);

                    UE_LOG(LogTemp, Warning, TEXT("? Async preload done: %d frames"), SuccessCount);
                });
        });
}

UTexture2D* USwingVideoWidget::LoadTextureFromJPG(const FString& FilePath)
{
    // ========== Step 1: 파일 존재 확인 ==========
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.FileExists(*FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("? File not found: %s"), *FilePath);
        return nullptr;
    }

    // ========== Step 2: 파일에서 바이너리 데이터 읽기 ==========
    TArray<uint8> FileData;
    if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("? Failed to load file: %s"), *FilePath);
        return nullptr;
    }

    // ? 파일 크기 검사
    if (FileData.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("? File is empty: %s"), *FilePath);
        return nullptr;
    }

    // ========== Step 3: ImageWrapper로 JPG 디코딩 ==========
    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(
        FName("ImageWrapper"));

    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(
        EImageFormat::JPEG);

    if (!ImageWrapper.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("? Failed to create ImageWrapper"));
        return nullptr;
    }

    if (!ImageWrapper->SetCompressed(FileData.GetData(), FileData.Num()))
    {
        UE_LOG(LogTemp, Error, TEXT("? Failed to decompress JPG: %s"),
            *FPaths::GetBaseFilename(FilePath));
        return nullptr;
    }

    // ========== Step 4: Raw 데이터 추출 ==========
    TArray64<uint8> RawData;
    if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
    {
        UE_LOG(LogTemp, Error, TEXT("? Failed to get raw data"));
        return nullptr;
    }

    // ? Raw 데이터 유효성 검사
    if (RawData.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("? Raw data is empty after decompression"));
        return nullptr;
    }

    int32 Width = ImageWrapper->GetWidth();
    int32 Height = ImageWrapper->GetHeight();

    // ? 해상도 유효성 검사
    if (Width <= 0 || Height <= 0)
    {
        UE_LOG(LogTemp, Error, TEXT("? Invalid image dimensions: %dx%d"), Width, Height);
        return nullptr;
    }

    // ========== Step 5: Texture2D 생성 ==========
    UTexture2D* NewTexture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
    if (!NewTexture)
    {
        UE_LOG(LogTemp, Error, TEXT("? Failed to create texture"));
        return nullptr;
    }

    // ? 생성 후 유효성 검사
    if (!IsValid(NewTexture))
    {
        UE_LOG(LogTemp, Error, TEXT("? Created texture is invalid"));
        return nullptr;
    }

    NewTexture->SRGB = true;
    NewTexture->UpdateResource();

    // ========== Step 6: 픽셀 데이터 복사 ==========
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION <= 26
    FTexture2DMipMap& Mip = NewTexture->PlatformData->Mips[0];
#else
    FTexture2DMipMap& Mip = NewTexture->GetPlatformData()->Mips[0];
#endif

    void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
    if (!TextureData)
    {
        UE_LOG(LogTemp, Error, TEXT("? Failed to lock texture data"));
        NewTexture->MarkAsGarbage();
        return nullptr;
    }

    FMemory::Memcpy(TextureData, RawData.GetData(), RawData.Num());
    Mip.BulkData.Unlock();

    NewTexture->UpdateResource();

    // ? 최종 유효성 검사
    if (!NewTexture->GetResource())
    {
        UE_LOG(LogTemp, Error, TEXT("? Texture resource is null after UpdateResource"));
        NewTexture->MarkAsGarbage();
        return nullptr;
    }

    return NewTexture;
}

bool USwingVideoWidget::LoadClipMetadata(const FString& ClipDir, FClipMetadata& OutMetadata)
{
    FString MetadataPath = FPaths::Combine(ClipDir, TEXT("metadata.json"));

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.FileExists(*MetadataPath))
    {
        return false;
    }

    // JSON 파일 읽기
    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *MetadataPath))
    {
        return false;
    }

    // 간단한 파싱 (정규표현식 또는 수동 파싱)
    // 예: "totalFrames": 150
    int32 TotalFramesStart = JsonString.Find(TEXT("\"totalFrames\":"));
    if (TotalFramesStart != INDEX_NONE)
    {
        FString Sub = JsonString.Mid(TotalFramesStart + 15);  // "totalFrames": 
        int32 CommaPos = Sub.Find(TEXT(","));
        if (CommaPos != INDEX_NONE)
        {
            FString NumStr = Sub.Left(CommaPos).TrimStartAndEnd();
            OutMetadata.TotalFrames = FCString::Atoi(*NumStr);
        }
    }

    // "fps": 30.0
    int32 FPSStart = JsonString.Find(TEXT("\"fps\":"));
    if (FPSStart != INDEX_NONE)
    {
        FString Sub = JsonString.Mid(FPSStart + 6);
        int32 CommaPos = Sub.Find(TEXT(","));
        if (CommaPos != INDEX_NONE)
        {
            FString NumStr = Sub.Left(CommaPos).TrimStartAndEnd();
            OutMetadata.FPS = FCString::Atof(*NumStr);
        }
    }

    // "width": 640
    int32 WidthStart = JsonString.Find(TEXT("\"width\":"));
    if (WidthStart != INDEX_NONE)
    {
        FString Sub = JsonString.Mid(WidthStart + 8);
        int32 CommaPos = Sub.Find(TEXT(","));
        if (CommaPos == INDEX_NONE)
            CommaPos = Sub.Find(TEXT("\n"));
        if (CommaPos != INDEX_NONE)
        {
            FString NumStr = Sub.Left(CommaPos).TrimStartAndEnd();
            OutMetadata.Width = FCString::Atoi(*NumStr);
        }
    }

    // "height": 480
    int32 HeightStart = JsonString.Find(TEXT("\"height\":"));
    if (HeightStart != INDEX_NONE)
    {
        FString Sub = JsonString.Mid(HeightStart + 9);
        int32 CommaPos = Sub.Find(TEXT(","));
        if (CommaPos == INDEX_NONE)
            CommaPos = Sub.Find(TEXT("\n"));
        if (CommaPos != INDEX_NONE)
        {
            FString NumStr = Sub.Left(CommaPos).TrimStartAndEnd();
            OutMetadata.Height = FCString::Atoi(*NumStr);
        }
    }

    return true;
}

UTexture2D* USwingVideoWidget::GetCachedFrame(int32 FrameIndex)
{
    // 캐시에서 찾기
    for (FFrameCache& Cache : FrameCachePool)
    {
        if (Cache.FrameIndex == FrameIndex)
        {
            Cache.LastAccessTime = GetWorld()->GetTimeSeconds();
            return Cache.Texture;
        }
    }

    // 캐시에 없으면 로드
    UTexture2D* NewFrame = LoadTextureFromJPG(ClipFramePaths[FrameIndex]);
    if (NewFrame)
    {
        AddToCache(FrameIndex, NewFrame);
    }

    return NewFrame;
}

void USwingVideoWidget::AddToCache(int32 FrameIndex, UTexture2D* Texture)
{
    float CurrentTime = GetWorld()->GetTimeSeconds();

    // 캐시 크기 제한 확인
    if (FrameCachePool.Num() >= MaxCacheSize)
    {
        CleanupOldCache(CurrentTime);
    }

    // 새 캐시 추가
    FFrameCache NewCache;
    NewCache.FrameIndex = FrameIndex;
    NewCache.Texture = Texture;
    NewCache.LastAccessTime = CurrentTime;

    FrameCachePool.Add(NewCache);
}

void USwingVideoWidget::CleanupOldCache(float CurrentTime)
{
    // 가장 오래된 캐시 찾기
    int32 OldestIndex = -1;
    float OldestTime = CurrentTime;

    for (int32 i = 0; i < FrameCachePool.Num(); i++)
    {
        if (FrameCachePool[i].LastAccessTime < OldestTime)
        {
            OldestTime = FrameCachePool[i].LastAccessTime;
            OldestIndex = i;
        }
    }

    // 제거
    if (OldestIndex >= 0)
    {
        if (FrameCachePool[OldestIndex].Texture)
        {
            FrameCachePool[OldestIndex].Texture->MarkAsGarbage();
        }
        FrameCachePool.RemoveAt(OldestIndex);
    }
}

void USwingVideoWidget::RootSwingTextures()
{
    for (const FVideoFrame& F : SwingFrames)
    {
        if (F.FrameTexture && IsValid(F.FrameTexture) && !F.FrameTexture->IsRooted())
        {
            F.FrameTexture->AddToRoot();
            RootedSwingTextures.Add(F.FrameTexture);
        }
    }
}

void USwingVideoWidget::UnrootSwingTextures()
{
    // ? 이미 비워졌으면 조기 반환
    if (RootedSwingTextures.Num() == 0)
    {
        return;
    }

    int32 UnrootCount = 0;

    for (UTexture2D* Tex : RootedSwingTextures)
    {
        if (Tex && IsValid(Tex))
        {
            if (Tex->IsRooted())
            {
                Tex->RemoveFromRoot();
                UnrootCount++;
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("  → Unrooted %d textures"), UnrootCount);

    // ? Empty() 호출
    RootedSwingTextures.Empty();
}


void USwingVideoWidget::RenderClipFrameAt(int32 FrameIndex)
{
    if (!VideoDisplay || !IsValid(VideoDisplay)) return;
    if (!ClipFramePaths.IsValidIndex(FrameIndex)) return;

    // ? 캐시에서 먼저 확인
    if (bUseTextureCache && LoadedFrameTextures.IsValidIndex(FrameIndex))
    {
        UTexture2D* Frame = LoadedFrameTextures[FrameIndex];
        if (Frame && IsValid(Frame) && Frame->GetResource())
        {
            VideoDisplay->SetBrushFromTexture(Frame);
            LastSetTexture = Frame;
            UE_LOG(LogTemp, Log, TEXT("? RenderClipFrame [%d] from cache"), FrameIndex);
            return;
        }
    }

    // ? 디스크에서 로드
    UTexture2D* Frame = LoadTextureFromJPG(ClipFramePaths[FrameIndex]);
    if (Frame && IsValid(Frame))
    {
        if (!Frame->GetResource())
            Frame->UpdateResource();

        if (Frame->GetResource())
        {
            VideoDisplay->SetBrushFromTexture(Frame);
            LastSetTexture = Frame;
            UE_LOG(LogTemp, Log, TEXT("? RenderClipFrame [%d] from disk"), FrameIndex);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("?? RenderClipFrame [%d]: Resource not ready"), FrameIndex);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("? RenderClipFrame [%d]: Load failed"), FrameIndex);
    }
}