// SwingVideoWidget.h - 개선된 버전
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"
#include "VideoBufferComponent.h"
#include "MediaPlayer.h"              // ✅ 추가
#include "MediaTexture.h"             // ✅ 추가
#include "FileMediaSource.h"          // ✅ 추가
// ✅ 필수: 델리게이트 헤더 include 추가 (오류 방지)
#include "Delegates/Delegate.h"
#include "SwingVideoWidget.generated.h"

class AWebcamCapture;

// ✅ 델리게이트 선언을 클래스 내부 public으로 이동 (오류 방지)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnClipPlaybackFinished);

UENUM(BlueprintType)
enum class EVideoMode : uint8
{
    LiveFeed,
    SwingPlayback,
    FilePlayback  // 추가
};


// ✅ 메타데이터 로드: USTRUCT로 변경 (오류 해결)
USTRUCT(BlueprintType)
struct FClipMetadata
{
    GENERATED_BODY()

        UPROPERTY(BlueprintReadWrite)
        FString ClipName;

    UPROPERTY(BlueprintReadWrite)
        int32 TotalFrames = 0;

    UPROPERTY(BlueprintReadWrite)
        float FPS = 30.0f;

    UPROPERTY(BlueprintReadWrite)
        int32 Width = 640;

    UPROPERTY(BlueprintReadWrite)
        int32 Height = 480;

    UPROPERTY(BlueprintReadWrite)
        float ShotTime = 0.0f;
};


// ✅ FFrameCache를 USTRUCT로 변경 (오류 해결: Reflection 시스템 인식)
USTRUCT(BlueprintType)
struct FFrameCache
{
    GENERATED_BODY()

        UPROPERTY(BlueprintReadWrite)
        int32 FrameIndex;

    UPROPERTY(BlueprintReadWrite)
        UTexture2D* Texture;

    UPROPERTY(BlueprintReadWrite)
        float LastAccessTime;
};

UCLASS()
class PARKDAY_API USwingVideoWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    USwingVideoWidget(const FObjectInitializer& ObjectInitializer);



    UPROPERTY(BlueprintAssignable, Category = "Video Control")
        FOnClipPlaybackFinished OnClipPlaybackFinished;  // 이 줄이 오류 발생 지점

    // 위젯 바인딩
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
        class UImage* VideoDisplay;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
        class UButton* PlayButton;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
        class UButton* SwingPauseButton;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
        class UButton* SwingStopButton;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
        class UButton* SwingLiveFeedButton;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
        class UButton* CloseButton;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
        class USlider* ProgressSlider;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
        class UTextBlock* TimeText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
        class UTextBlock* StatusText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
        class UHorizontalBox* ControlsBox;

    // 설정 가능한 속성들
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Video Settings")
        float PlaybackSpeed = 1.0f;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Video Settings")
        bool bLoopPlayback = true;

    // 웹캠 캡처 참조
    UPROPERTY(BlueprintReadWrite, Category = "Webcam")
        TWeakObjectPtr<AWebcamCapture> WebcamCaptureRef;

    // 블루프린트에서 호출 가능한 함수들
    UFUNCTION(BlueprintCallable, Category = "Video Control")
        void SetSwingFrames(const TArray<FVideoFrame>& Frames);

    UFUNCTION(BlueprintCallable, Category = "Video Control")
        void SwitchToLiveFeed();

    UFUNCTION(BlueprintCallable, Category = "Video Control")
        void SwitchToSwingPlayback();

    UFUNCTION(BlueprintCallable, Category = "Video Control")
        void PlaySwingVideo();

    UFUNCTION(BlueprintCallable, Category = "Video Control")
        void PauseSwingVideo();

    UFUNCTION(BlueprintCallable, Category = "Video Control")
        void StopSwingVideo();

    UFUNCTION(BlueprintCallable, Category = "Video Control")
        void CloseSwingMotion();

    UFUNCTION(BlueprintCallable, Category = "Video Control")
        void SetPlaybackPosition(float Position);

    // ✅ 추가: MediaPlayer 상태 확인 및 재시작 함수
    UFUNCTION(BlueprintCallable, Category = "Video Control")
        void CheckAndRestartMediaPlayer();

    UPROPERTY(Transient)
        TArray<FVideoFrame> SwingFrames;

    bool bIsPlaying = false;
    bool bIsPaused = false;


    void SetVideoFrame(const FVideoFrame& Frame);

    UFUNCTION(BlueprintCallable, Category = "Debug")
        void DiagnosePlaybackIssue();

    UFUNCTION(BlueprintCallable, Category = "Video Control")
        void TestPlaySavedVideo();

    // ======== RenderTarget 방식용 ========
    UPROPERTY(Transient)
        UTextureRenderTarget2D* SwingRT = nullptr;

    UPROPERTY(Transient)
        UMaterialInstanceDynamic* SwingMID = nullptr;

    UPROPERTY(EditAnywhere, Category = "Swing Video")
        UMaterialInterface* BlitMaterial = nullptr;




protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual void BeginDestroy() override;

    // 버튼 클릭 이벤트 핸들러들
    UFUNCTION()
        void OnPlayButtonClicked();

    UFUNCTION()
        void OnPauseButtonClicked();

    UFUNCTION()
        void OnStopButtonClicked();

    UFUNCTION()
        void OnLiveFeedButtonClicked();

    UFUNCTION()
        void OnProgressSliderChanged(float Value);

    UFUNCTION(BlueprintCallable, Category = "Video Control")
        void PlayVideoFile(const FString& FilePath);

    // ✅ MediaPlayer 이벤트 핸들러 추가
    UFUNCTION()
        void OnFileMediaOpened(FString OpenedUrl);

    UFUNCTION()
        void OnFileMediaOpenFailed(FString FailedUrl);


private:
    // 비디오 모드
    EVideoMode CurrentVideoMode = EVideoMode::LiveFeed;

    // 스윙 재생 관련 변수들
    int32 CurrentFrameIndex = 0;
    float CurrentPlaybackTime = 0.0f;
    float TotalPlaybackDuration = 0.0f;

    // 타이머 관련
    float PlaybackTimerAccumulator = 0.0f;
    float FrameRate = 30.0f;
    int32 LastCalculatedFrameIndex = 0;  // ✅ 캐시

    UPROPERTY()
        UTexture2D* LastSetTexture = nullptr;

    int32 LastFrameTextureIndex = -1;

    //  "스윙 브러시가 한 번도 성공 못해서 livefeed가 남는" 문제 방지용 플래그
    bool bHasEverSetSwingBrush = false;

    // 내부 함수들
    void UpdateVideoDisplay();
    void UpdateLiveFeed();
    void UpdateSwingPlayback();
    void UpdateUI();
    void UpdateTimeDisplay();
    void CalculatePlaybackDuration();
    void UpdateCurrentFrameIndex();
    FString FormatTime(float TimeInSeconds);


    // ✅ 버퍼 검증 통계
    struct FFrameValidationStats
    {
        int32 TotalFrames = 0;
        int32 ValidFrames = 0;
        int32 NullTextureFrames = 0;
        int32 InvalidTextureFrames = 0;
        int32 ResourceNullFrames = 0;
        float FirstTimestamp = 0.0f;
        float LastTimestamp = 0.0f;
        float MinTimestampGap = FLT_MAX;
        float MaxTimestampGap = 0.0f;
    } ValidationStats;

    // ✅ 검증 함수들
    void ValidateSwingFramesData();
    void ValidateFrameBeforePlayback(int32 FrameIndex);
    void PrintValidationStats();
    void DiagnosePlaybackFailure();



    void BlitFrameToRT(UTexture2D* FrameTex);


    // ✅ 파일 재생용 MediaPlayer 관련
    UPROPERTY()
        UMediaPlayer* FileMediaPlayer;

    UPROPERTY()
        UFileMediaSource* FileMediaSource;

    UPROPERTY()
        UMediaTexture* FileMediaTexture;


    void StopVideoFile();
    void UpdateFilePlayback();


    void HandleLiveFeedMode(float InDeltaTime);
    void HandleSwingPlaybackMode(float InDeltaTime);
    void HandleFilePlaybackMode(float InDeltaTime);
    void HandleClipPlaybackMode(float InDeltaTime);

    void SwitchToFilePlayback();
    void CleanupCurrentMode();

    void InitBlitSystem();

    // void DiagnoseLiveFeedIssue();

public:
    // ✅ JPG 시퀀스 클립 재생
    UFUNCTION(BlueprintCallable, Category = "Video Control")
        void PlaySwingClipFromDirectory(const FString& ClipDirectory);

private:
    // ✅ JPG 파일에서 텍스처 로드

    UTexture2D* LoadTextureFromJPG(const FString& FilePath);


    bool LoadClipMetadata(const FString& ClipDir, FClipMetadata& OutMetadata);

    // ✅ 클립 재생 상태
    FString CurrentClipDirectory;
    TArray<FString> ClipFramePaths;  // 0001.jpg, 0002.jpg, ...
    int32 CurrentClipFrameIndex = 0;
    float ClipFPS = 30.0f;

    // ✅ 텍스처 캐시 (성능 최적화)
    UPROPERTY()
        TArray<UTexture2D*> LoadedFrameTextures;
    bool bUseTextureCache = false;  // 메모리 vs 성능 선택

    UPROPERTY()
        UTexture2D* NextFrameTexture = nullptr;

    bool bLoadNextFrameInBackground = true;

    void PreloadNextFrame();



    UPROPERTY()
        TArray<FFrameCache> FrameCachePool;
    int32 MaxCacheSize = 10;  // 캐시 최대 개수

    UTexture2D* GetCachedFrame(int32 FrameIndex);
    void AddToCache(int32 FrameIndex, UTexture2D* Texture);
    void CleanupOldCache(float CurrentTime);

    UPROPERTY()
        TSet<UTexture2D*> RootedSwingTextures;

    void RootSwingTextures();
    void UnrootSwingTextures();


    void RenderClipFrameAt(int32 FrameIndex);


    // [FIX #1] ValidatingTimer 핸들 선언 - 누출 방지의 핵심
    FTimerHandle ValidatingTimerHandle;

    // [FIX #3] Async 람다 중복 실행 방지
    bool bAsyncPreloadInProgress = false;

    // [FIX #4] OnClipPlaybackFinished 이중 Broadcast 방지
    bool bClipFinishBroadcasted = false;

    // [FIX #2] 하드코딩 140 대체 - 메타데이터에서 읽은 실제 예상 프레임 수
    int32 ExpectedTotalFrames = 0;

};