// WebcamCapture.h - 최적화된 버전 (메모리 관리 개선)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "MediaSource.h"
#include "VideoBufferComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SwingVideoWidget.h"
#include "WebcamConfig.h"
#include "MediaSource.h"
#include "WebcamCapture.generated.h"

// 델리게이트 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSwingDetectedSignature, const TArray<FVideoFrame>&, Frames);

UCLASS()
class PARKDAY_API AWebcamCapture : public AActor
{
    GENERATED_BODY()

public:
    AWebcamCapture();
    ~AWebcamCapture();

    // ========== 주요 컴포넌트 ==========
    UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Webcam")
        class UMediaPlayer* MediaPlayer;

    UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Webcam")
        class UMediaTexture* MediaTexture;

    UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Webcam")
        class UMediaSource* WebcamSource;

    UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Components")
        class UVideoBufferComponent* VideoBufferComponent;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Webcam")
        class UMaterial* MediaTextureMaterial;

    UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Webcam")
        class UTextureRenderTarget2D* CaptureRenderTarget;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "UI")
        TSubclassOf<class USwingVideoWidget> VideoWidgetClass;

    UPROPERTY(BlueprintReadWrite, Category = "UI")
        class USwingVideoWidget* VideoWidget;

    UPROPERTY(BlueprintAssignable, Category = "Swing Detection")
        FOnSwingDetectedSignature OnSwingDetected;

    // ========== 설정 관련 ==========
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Config")
        bool bAutoLoadConfig = true;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Config")
        FString ConfigFilePath;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Config")
        bool bCreateDefaultConfigIfNotFound = true;

    UPROPERTY(BlueprintReadOnly, Category = "Config")
        FWebcamSettings CurrentSettings;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Config")
        class UWebcamConfigAsset* ConfigAsset;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Config")
        int32 TrackIndex = 1;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Config")
        int32 FormatIndex = 41;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Config")
        int32 MaxRetries = 3;

    // ========== 샷 기록 설정 ==========
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Shot Recording")
        float PreShotBufferTime = 2.0f;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Shot Recording")
        float PostShotBufferTime = 3.0f;  // ✅ 수정: 3.0f → 4.0f (프레임 부족 문제 해결)

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Shot Recording")
        int32 MaxBufferFrames = 500;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Shot Recording")
        bool bAutoCleanOldFrames = true;

    // ========== 🔧 최적화: 텍스처 풀 및 SurfaceData 캐싱 ==========
    // ✅ 텍스처 풀 설정
    // ✅ Pool 크기 상수 (Material Pool과 Texture Pool 동기화)
    static constexpr int32 POOL_SIZE = 300;

    UPROPERTY(BlueprintReadOnly, Category = "Performance")
        int32 TexturePoolSize = POOL_SIZE;  // ✅ 읽기 전용으로 변경

    // ========== 함수들 ==========
    UFUNCTION(BlueprintCallable, Category = "Config")
        bool LoadConfig(const FString& FilePath = "");

    UFUNCTION(BlueprintCallable, Category = "Config")
        bool SaveConfig(const FString& FilePath = "");

    UFUNCTION(BlueprintCallable, Category = "Config")
        void ApplySettings(const FWebcamSettings& Settings);

    UFUNCTION(BlueprintCallable, Category = "Config")
        bool CreateDefaultConfigFile();

    UFUNCTION(BlueprintCallable, Category = "Webcam")
        void StartCapture();

    UFUNCTION(BlueprintCallable, Category = "Webcam")
        void StopCapture();

    UFUNCTION(BlueprintCallable, Category = "Shot Recording")
        void TriggerShotRecording();

    UFUNCTION(BlueprintCallable, Category = "Shot Recording")
        void TriggerShotRecordingAtTime(float ShotTime);

    UFUNCTION(BlueprintCallable, Category = "Shot Recording")
        void PlayLastRecordedShot();

    UFUNCTION(BlueprintCallable, Category = "Shot Recording")
        float GetBufferedDuration() const;

    UFUNCTION(BlueprintCallable, Category = "Shot Recording")
        void GetLastRecordedSwingFrames(TArray<FVideoFrame>& OutFrames) const
    {
        OutFrames = LastRecordedShot;
    }

    UFUNCTION(BlueprintCallable, Category = "UI")
        void CreateVideoWidget();

    UFUNCTION(BlueprintCallable, Category = "UI")
        void ShowVideoWidget(bool bValue);

    UFUNCTION(BlueprintCallable, Category = "UI")
        void HideVideoWidget();

    UFUNCTION(BlueprintCallable, Category = "Webcam")
        UTexture2D* GetCurrentFrameAsTexture2D();

    UFUNCTION(BlueprintCallable, Category = "Debug")
        void TestWebcamConnection();

    UFUNCTION(BlueprintCallable, Category = "UI")
        bool CheckVideoWidgetStatus() const;

    bool OpenWebcamByDisplayName(const FString& TargetName);
    bool SelectBest1080p60Format();

    UFUNCTION(BlueprintCallable, Category = "Webcam")
        bool FindAndConnectByVIDPID(const FString& VID, const FString& PID, int32 Index = 0);

    UFUNCTION(BlueprintCallable, Category = "Webcam")
        bool FindWebcamByName(const FString& DeviceName);

    UFUNCTION(BlueprintCallable, Category = "Webcam")
        TArray<FString> EnumerateWebcams();

    UMaterialInstanceDynamic* GetCachedDynamicMaterial() const { return CachedDynamicMaterial; }

    UFUNCTION(BlueprintCallable, Category = "Swing Recording")
        void SetVideoSavingEnabled(bool bEnable);


    // ✅ 디버그용: 현재 프레임을 PNG로 저장
    UFUNCTION(BlueprintCallable, Category = "Debug")
        void SaveFrameToPNG(const FString& FrameName = TEXT("frame"));

    // ✅ 디버그용: 버퍼의 모든 프레임 저장
    UFUNCTION(BlueprintCallable, Category = "Debug")
        void SaveAllBufferedFramesToPNG(const FString& OutputFolder = TEXT(""));

    // ✅ 디버그용: N번째 프레임 저장
    UFUNCTION(BlueprintCallable, Category = "Debug")
        void SaveBufferedFrameAt(int32 Index, const FString& OutputFolder = TEXT(""));

    // ✅ 여러 개의 텍스처를 순환하면서 사용
    UPROPERTY()
        TArray<UTexture2D*> TexturePool;


    /** 샷 대기 전: 스윙 녹화 시작 (깨끗한 상태에서) */
    UFUNCTION(BlueprintCallable, Category = "SwingCapture")
        void StartCaptureForSwing();

    /** 샷 직후: 캡처 완전 중지 + 버퍼 정리 준비 */
    UFUNCTION(BlueprintCallable, Category = "SwingCapture")
        void StopCaptureAfterShot();

    /** 다음 샷 준비: 버퍼 리셋 + 캡처 재개 */
    UFUNCTION(BlueprintCallable, Category = "SwingCapture")
        void ResumeCaptureForNextSwing();

    // ✅ JPG 시퀀스 저장
    UFUNCTION(BlueprintCallable, Category = "Shot Recording")
        FString SaveSwingClipToDisk(const TArray<FVideoFrame>& SwingFrames, float ShotTime);

    // ✅ 클립 재생 요청 (위젯으로 전달)
    UFUNCTION(BlueprintCallable, Category = "Shot Recording")
        void PlaySwingClipFromPath(const FString& ClipPath, float DelaySeconds = 3.0f);

    UFUNCTION(BlueprintCallable, Category = "Shot Recording")
        TArray<FString> GetSavedClipList();

    UFUNCTION(BlueprintCallable, Category = "Shot Recording")
        bool DeleteClip(const FString& ClipDirectory);


    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Shot Recording")
        int32 MaxStoredClips = 100;  // 최대 저장 클립 수

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Shot Recording")
        float MaxStorageSizeGB = 10.0f;  // 최대 저장 용량 (GB)

    void SettingPlaySwingClip();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION()
        void OnMediaOpened(FString OpenedUrl);

    UFUNCTION()
        void OnMediaOpenFailed(FString FailedUrl);

    UFUNCTION()
        void OnPlaybackSuspended();

    void OnApplicationWillDeactivate();
    void OnApplicationHasReactivated();

    // 내부 보조 함수
    void StopCaptureInternal(bool bClearBufferNow);
    void StartCaptureInternal();



private:
    void InitWebcam();
    void DelayedInitialization();
    void SelectTrackAndFormat();
    void CheckPlaybackStatus();

    UTexture2D* CaptureCurrentFrame();
    UTexture2D* CreateTexture2DFromPixels(UTextureRenderTarget2D* RenderTarget);

    void SafeCleanup();
    bool IsValidForOperation() const;
    void SafeStopCapture();
    void CaptureFrame();

    void ProcessShotRecording(float ShotTime);
    TArray<FVideoFrame> ExtractSwingFrames(float ShotTime);

    class UFileMediaSource* CreateMediaSourceFromURL(const FString& URL);

    // ========== 타이머 핸들들 ==========
    FTimerHandle InitTimerHandle;
    FTimerHandle CaptureTimerHandle;
    FTimerHandle PlayCheckTimerHandle;
    FTimerHandle RetryTimerHandle;
    FTimerHandle TrackFormatTimerHandle;
    FTimerHandle PlayStartTimerHandle;

    // ========== 상태 관리 ==========
    int32 RetryCount = 0;
    bool bIsCapturing = false;
    UPROPERTY()
        UMaterialInstanceDynamic* CachedDynamicMaterial = nullptr;
    float CurrentCaptureTime = 0.0f;
    UPROPERTY()
        TArray<FVideoFrame> LastRecordedShot;
    int32 FrameCounter = 0;
    int32 LastCleanupFrame = 0;
    bool bIsInBackground = false;
    int32 FrameSkipCounter = 0;
    static constexpr int32 FRAME_SKIP = 0;

    // ========== 🔧 최적화: 텍스처 풀 ==========

    int32 PoolIndex = 0;
    static const int32 PoolSize = 3;  // 3개만 재사용

    int32 CurrentPoolIndex = 0;

    // ========== 🔧 최적화: SurfaceData 캐싱 ==========


    bool AutoConnectFirstWebcam();

    // 헬퍼 함수: 텍스처를 PNG로 저장
    bool SaveTextureToPNG(UTexture2D* Texture, const FString& FilePath);

    // ✅ 메모리 누수 방지: 텍스처 재사용 풀
    UPROPERTY()
        UTexture2D* CachedFrameTexture = nullptr;

    // ✅ 디버그용 플래그
    bool bDebugSaveNextFrame = false;
    int32 DebugSaveFrameCount = 10;

    void LogVideoTrackInfo() const;

    bool bInitWebcamInProgress = false;
    bool bWebcamOpened = false;


    // ✅ 영상 추출 중 캡처 제어
    bool bIsPausingCapture = false;  // 캡처 일시 중지 상태

    // ✅ 대기 시간 설정

    float ExtractionPauseTime = 0.5f;  // 추출 시작 전 대기 (초)

// ✅ 추출 완료 후 캡처 재개 지연

    float ResumeCaptureDuration = 1.0f;  // 추출 완료 후 재개까지의 시간 (초)

    FTimerHandle PauseCaptureTimerHandle;
    FTimerHandle ResumeCaptureTimerHandle;

    // ✅ 캡처 일시 중지/재개 함수
    void PauseCapture();
    void ResumeCapture();

    // ═══════════════════════════════════════════════════════════════════════════
    // ✅ 추가: SwingClips 폴더 자동 삭제 함수
    // ═══════════════════════════════════════════════════════════════════════════
    void CleanupSwingClipsFolder();
    bool bSwingClipsDeleted = false;

    FTimerHandle PendingShotTimerHandle;
    bool bShotPending = false;
    float PendingShotTime = 0.f;

    void  ProcessPendingShotRecording();
    UPROPERTY()
        TArray<FColor> ReusableSurfaceData;

    // RT 크기 튐 방지용
    int32 LastValidW = 0;
    int32 LastValidH = 0;

    bool UpdateTexture2DFromRenderTarget(UTexture2D* TargetTexture, UTextureRenderTarget2D* RenderTarget);

    bool FindYUY2_640x480_30fps(int32& OutTrackIndex, int32& OutFormatIndex);
    bool SetVideoFormat(int32 InTrackIndex, int32 InFormatIndex);


    void ValidateCapturedTexture(UTexture2D* Texture);

    UPROPERTY()
        TArray<UMaterialInstanceDynamic*> MaterialPool;
    int32 MaterialPoolIndex = 0;


    // ✅ 헬퍼: 단일 프레임을 JPG로 저장
    bool SaveFrameAsJPG(UTexture2D* Texture, const FString& FilePath, int32 Quality = 50);

    // ✅ 헬퍼: 메타데이터 저장
    bool SaveClipMetadata(const FString& ClipDir, int32 TotalFrames, float FPS,
        int32 Width, int32 Height, float ShotTime);

    // ✅ 타이머: 딜레이 후 재생
    FTimerHandle PlayClipTimerHandle;
    FString LastSavedClipPath;  // ✅ 마지막 저장된 클립 경로 추적
    void OnPlayClipTimerComplete(FString ClipPath);

    void CleanupOldClips();
    int64 GetDirectorySize(const FString& Directory);


    // ✅ 비동기 저장용
    TFuture<void> AsyncSaveTask;
    bool bIsSavingAsync = false;

    // ✅ 스레드 안전 JPG 저장 함수 (static)
    static bool SaveFrameAsJPG_ThreadSafe(UTexture2D* Texture, const FString& FilePath, int32 Quality);

    // ✅ 워커 스레드 함수
    static void SaveFramesWorker(TArray<FVideoFrame> Frames, FString ClipDir, int32 Quality);

    // ═══════════════════════════════════════════════════════════════════════════
    // ✅ 타임싱크 관련 멤버 변수 (Static 대신 멤버 변수로 변경)
    // ═══════════════════════════════════════════════════════════════════════════
    // 📍 문제: 기존 코드에서 static float CaptureStartTime을 사용하면
    //          게임 재시작 시에도 이전 값이 유지되어 타임싱크 에러 발생
    // ✅ 해결: 멤버 변수로 변경하여 BeginPlay에서 초기화 가능
    float CaptureStartTime = -1.0f;           // ✅ 캡처 시작 시간
    bool bCaptureStartTimeInitialized = false; // ✅ 초기화 여부 추적

    // ═══════════════════════════════════════════════════════════════════════════
    // ✅ 더미 스윙 프로세스: 첫 샷 버퍼 워밍업 (NEW)
    // ═══════════════════════════════════════════════════════════════════════════
    // 📍 목적: 
    //   - 캡처 시작 후 첫 1초 동안 모든 시스템 초기화 & 워밍업
    //   - 버퍼 완전 채우기 + 텍스처 풀 인덱스 안정화
    //   - 파일 저장 로직 워밍업 (I/O 지연 최소화)
    //   - 결과: 두 번째 스윙부터 안정적인 비디오 품질

    bool bDummySwingInProgress = false;      // 더미 스윙 처리 중 플래그
    float DummySwingStartTime = 0.0f;        // 더미 스윙 시작 시간
    static constexpr float DUMMY_SWING_DURATION = 1.0f;  // 1초 더미 프로세스

    // 더미 스윙 프로세스 함수들
    void StartDummySwingProcess();
    void ProcessDummySwing(float DeltaTime);
    void CompleteDummySwingProcess();
    void LogDummySwingStatus() const;

    bool LoadClipMetadata(const FString& ClipDir, FClipMetadata& OutMetadata);


    // AWebcamCapture 클래스 내 private 섹션에 추가
private:
    // 비동기 처리 중인지 확인하는 플래그 (중복 실행 방지)
    bool bIsAsyncReading = false;

    // 픽셀 데이터를 임시로 담을 재사용 버퍼
    UPROPERTY()
        TArray<FColor> AsyncSurfaceData;

    // 비동기 업데이트 핵심 함수 (기존 함수 대체)
    bool UpdateTexture2DFromRenderTargetAsync(UTexture2D* TargetTexture, UTextureRenderTarget2D* RenderTarget);
};