// VideoBufferComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/Texture2D.h"
#include "VideoBufferComponent.generated.h"



class AWebcamCapture;

USTRUCT(BlueprintType)
struct FVideoFrame
{
    GENERATED_BODY()

        UPROPERTY(BlueprintReadWrite)
        UTexture2D* FrameTexture = nullptr;

    UPROPERTY(BlueprintReadWrite)
        float Timestamp = 0.0f;
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PARKDAY_API UVideoBufferComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UVideoBufferComponent();

    // ========== 기본 기능 ==========

    UFUNCTION(BlueprintCallable, Category = "Video Buffer")
        void AddFrame(UTexture2D* Frame, float Time);

    UFUNCTION(BlueprintCallable, Category = "Video Buffer")
        TArray<FVideoFrame> GetFramesInRange(float StartTime, float EndTime);

    UFUNCTION(BlueprintCallable, Category = "Video Buffer")
        int32 GetBufferedFrameCount() const;

    UFUNCTION(BlueprintCallable, Category = "Video Buffer")
        void ClearBuffer();

    UFUNCTION(BlueprintPure, Category = "Video Buffer")
        int32 GetMaxBufferSize() const { return MaxBufferSize; }

    float GetBufferUsage() const;

    UFUNCTION(BlueprintCallable, Category = "Video Buffer")
        void SetMaxBufferSize(int32 NewSize);

    UFUNCTION(BlueprintCallable, Category = "Video Buffer")
        void SetAutoCleanup(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "Video Buffer")
        void CleanOldFrames(float CutoffTime);

    UFUNCTION(BlueprintPure, Category = "Video Buffer")
        float GetEstimatedMemoryUsageMB() const;

    UFUNCTION(BlueprintCallable, Category = "Video Buffer")
        void ToggleVideoSaving(bool bEnable);

    // ✅ 추가: 리플레이 상태 설정
    UFUNCTION(BlueprintCallable, Category = "Video Buffer")
        void SetIsReplaying(bool bReplaying);

    UFUNCTION(BlueprintPure, Category = "Video Buffer")
        bool GetIsReplaying() const { return bIsReplaying; }

    // ✅ 추가: 모든 프레임 반환
    UFUNCTION(BlueprintCallable, Category = "Video Buffer")
        TArray<FVideoFrame> GetAllFrames() const
    {
        return FrameBuffer;
    }

    // ✅ 추가: 총 프레임 수 반환
    UFUNCTION(BlueprintCallable, Category = "Video Buffer")
        int32 GetTotalFrameCount() const
    {
        return FrameBuffer.Num();
    }

    // ✅ 추가: 버퍼 지속 시간 반환
    UFUNCTION(BlueprintCallable, Category = "Video Buffer")
        float GetBufferedDuration() const
    {
        if (FrameBuffer.Num() < 2)
        {
            return 0.0f;
        }
        return FrameBuffer.Last().Timestamp - FrameBuffer[0].Timestamp;
    }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video Buffer")
        bool bEnableVideoSaving = true;

private:
    // ========== 내부 변수 ==========
    UPROPERTY()
        TArray<FVideoFrame> FrameBuffer;

    UPROPERTY(EditAnywhere, Category = "Video Buffer", meta = (ClampMin = "30", ClampMax = "900"))
        int32 MaxBufferSize = 1500;  // ✅ 30fps × 30초 = 900프레임 (충분한 버퍼)

    bool bAutoCleanup = false;
    float LastCleanupTime = 0.0f;
    int32 CurrentWriteIndex = 0;

    // ✅ 추가: 리플레이 중 상태
    bool bIsReplaying = false;

    // ✅ 추가: 지연 삭제 큐
    UPROPERTY()
        TArray<UTexture2D*> PendingDeleteTextures;

    // ✅ 추가: 타이머 핸들 (Tick 대신 사용)
    FTimerHandle PendingDeleteTimerHandle;

    // ✅ 추가: 지연 삭제 처리 (일반 함수, override 아님!)
    void ProcessPendingDeleteTextures();

    // ✅ 초기화 상태 추적
    bool bFrameBufferInitialized = false;

    // ✅ 버퍼 접근 보호 (향후 멀티스레드 대비)
    mutable FCriticalSection BufferLock;


    void SaveDebugFrame(UTexture2D* Frame, float Time, int32 FrameIndex);

    bool IsTextureFromPool(UTexture2D* Texture);
    // ✅ WebcamCapture 참조 획득 헬퍼
    AWebcamCapture* GetWebcamCapture() const;
};