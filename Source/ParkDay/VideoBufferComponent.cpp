// VideoBufferComponent.cpp - ? PATCHED VERSION
// �ֿ� ����:
// 1. GetFramesInRange - ���� �ʰ� �� GC ������ ��ȭ
// 2. ClearBuffer - ������ ����
// 4. EndPlay - Ÿ�̸� ����� ����

#include "VideoBufferComponent.h"
#include "Engine/Texture2D.h"
#include "UObject/GCObject.h"
#include "UObject/UObjectGlobals.h"
#include "WebcamCapture.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "ParkDayProfiling.h"

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
static bool bDebugSaveFrames = false;
static int32 DebugFrameSaveCounter = 0;
#endif

UVideoBufferComponent::UVideoBufferComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 1.0f;

    FrameBuffer.Reserve(MaxBufferSize);
    bFrameBufferInitialized = false;
}

void UVideoBufferComponent::BeginPlay()
{
    Super::BeginPlay();

    FrameBuffer.SetNum(MaxBufferSize);
    for (FVideoFrame& Frame : FrameBuffer)
    {
        Frame.FrameTexture = nullptr;
        Frame.Timestamp = 0.0f;
    }

    bAutoCleanup = true;
    bFrameBufferInitialized = true;

    UE_LOG(LogTemp, Log, TEXT("? VideoBuffer initialized: %d frames (%.1f seconds at 30fps)"),
        MaxBufferSize, MaxBufferSize / 30.0f);

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            PendingDeleteTimerHandle,
            this,
            &UVideoBufferComponent::ProcessPendingDeleteTextures,
            1.0f,
            true
        );
        UE_LOG(LogTemp, Log, TEXT("? Pending delete timer started"));
    }
}

void UVideoBufferComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UE_LOG(LogTemp, Warning, TEXT("?? VideoBufferComponent::EndPlay START"));

    // ? PATCH 4: Ÿ�̸� ����� ����
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(PendingDeleteTimerHandle);
        UE_LOG(LogTemp, Log, TEXT("? Pending delete timer cleared"));
    }

    // ? GC ���̸� �α� ���
    if (IsGarbageCollecting())
    {
        UE_LOG(LogTemp, Warning, TEXT("?? EndPlay called during GC cycle"));
    }

    Super::EndPlay(EndPlayReason);
    UE_LOG(LogTemp, Warning, TEXT("? VideoBufferComponent::EndPlay COMPLETE"));
}

void UVideoBufferComponent::AddFrame(UTexture2D* Frame, float Time)
{
    SCOPE_CYCLE_COUNTER(STAT_VideoBufferAddFrame);

    if (!bFrameBufferInitialized)
    {
        UE_LOG(LogTemp, Error, TEXT("? VideoBuffer not initialized"));
        return;
    }

    if (!Frame)
    {
        UE_LOG(LogTemp, Error, TEXT("? Frame is null"));
        return;
    }

    if (!bEnableVideoSaving)
    {
        UE_LOG(LogTemp, Error, TEXT("? Video saving disabled"));
        return;
    }

    if (FrameBuffer.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("? VideoBuffer not initialized!"));
        return;
    }

    if (CurrentWriteIndex < 0 || CurrentWriteIndex >= FrameBuffer.Num())
    {
        UE_LOG(LogTemp, Error, TEXT("? Invalid write index: %d (buffer size: %d)"),
            CurrentWriteIndex, FrameBuffer.Num());
        CurrentWriteIndex = 0;
        return;
    }

    FVideoFrame& CurrentSlot = FrameBuffer[CurrentWriteIndex];

    if (CurrentSlot.FrameTexture)
    {
        if (!IsTextureFromPool(CurrentSlot.FrameTexture))
        {
            PendingDeleteTextures.AddUnique(CurrentSlot.FrameTexture);
            UE_LOG(LogTemp, Verbose, TEXT("  ?? Added texture to pending delete"));
        }
        else
        {
            UE_LOG(LogTemp, Verbose, TEXT("  ?? Texture from pool, skipping delete"));
        }

        CurrentSlot.FrameTexture = nullptr;
        CurrentSlot.Timestamp = -1.0f;
    }

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
    if (bDebugSaveFrames && DebugFrameSaveCounter < 200)
    {
        if (DebugFrameSaveCounter > 100)
            SaveDebugFrame(Frame, Time, DebugFrameSaveCounter);
        DebugFrameSaveCounter++;
    }
#endif

    FrameBuffer[CurrentWriteIndex].FrameTexture = Frame;
    FrameBuffer[CurrentWriteIndex].Timestamp = Time;
    CurrentWriteIndex = (CurrentWriteIndex + 1) % FrameBuffer.Num();

    if (bAutoCleanup && (Time - LastCleanupTime) >= 10.0f)
    {
        float CutoffTime = Time - 15.0f;
        CleanOldFrames(CutoffTime);
    }
}

bool UVideoBufferComponent::IsTextureFromPool(UTexture2D* Texture)
{
    if (!Texture)
    {
        return false;
    }

    AWebcamCapture* Webcam = GetWebcamCapture();
    if (!Webcam)
    {
        UE_LOG(LogTemp, Warning, TEXT("?? IsTextureFromPool: Owner is not AWebcamCapture"));
        return false;
    }

    return Webcam->TexturePool.Contains(Texture);
}

// ? PATCH 1: GetFramesInRange - ���� �籸�� (������ ��ȭ)
TArray<FVideoFrame> UVideoBufferComponent::GetFramesInRange(float StartTime, float EndTime)
{
    SCOPE_CYCLE_COUNTER(STAT_VideoBufferGetRange);
    TArray<FVideoFrame> ResultFrames;

    // ? Step 1: GC ���̸� ��� ��ȯ (���� �߿�!)
    if (IsGarbageCollecting())
    {
        UE_LOG(LogTemp, Warning, TEXT("?? GetFramesInRange aborted: GC in progress"));
        return ResultFrames;
    }

    // ? Step 2: ���� ���� ����
    if (!bFrameBufferInitialized)
    {
        UE_LOG(LogTemp, Error, TEXT("? GetFramesInRange: Buffer not initialized"));
        return ResultFrames;
    }

    // ? Step 3: ���۰� ����ִ��� Ȯ��
    if (FrameBuffer.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("? GetFramesInRange: Buffer is empty!"));
        return ResultFrames;
    }

    // ? Step 4: CurrentWriteIndex ��ȿ�� �˻�
    if (CurrentWriteIndex < 0 || CurrentWriteIndex >= FrameBuffer.Num())
    {
        UE_LOG(LogTemp, Error, TEXT("? GetFramesInRange: Invalid WriteIndex %d (size: %d)"),
            CurrentWriteIndex, FrameBuffer.Num());
        return ResultFrames;
    }

    // ? Step 5: �ð� ���� ����
    if (StartTime > EndTime)
    {
        UE_LOG(LogTemp, Warning, TEXT("?? GetFramesInRange: StartTime > EndTime! (%.2f > %.2f)"),
            StartTime, EndTime);
        return ResultFrames;
    }

    // ? Step 6: ���� ���� �� ���� ũ�� ĳ�� (���� �� ���� ������)
    int32 BufferSize = FrameBuffer.Num();
    ResultFrames.Reserve(BufferSize);

    int32 TotalFrames = 0;
    int32 NullTextureFrames = 0;
    int32 InvalidTimestampFrames = 0;
    int32 OutOfRangeFrames = 0;

    // ? Step 7: ������ ���� ó��
    for (int32 i = 0; i < BufferSize; ++i)
    {
        // ? ���� �� ���۰� ������¡�Ǹ� ��� �ߴ� (�ſ� �߿�!)
        if (FrameBuffer.Num() != BufferSize)
        {
            UE_LOG(LogTemp, Warning, TEXT("?? Buffer resized during GetFramesInRange, aborting (was %d, now %d)"),
                BufferSize, FrameBuffer.Num());
            break;
        }

        int32 ReadIndex = (CurrentWriteIndex + i) % BufferSize;

        // ? ���� ��Ȯ�� (�ٽ� �ѹ�!)
        if (ReadIndex < 0 || ReadIndex >= FrameBuffer.Num())
        {
            UE_LOG(LogTemp, Error, TEXT("? ReadIndex out of bounds: %d (size: %d)"),
                ReadIndex, FrameBuffer.Num());
            break;
        }

        const FVideoFrame& Frame = FrameBuffer[ReadIndex];

        TotalFrames++;

        // ? ��ȿ�� �˻�
        if (!Frame.FrameTexture)
        {
            NullTextureFrames++;
            continue;
        }

        if (!IsValid(Frame.FrameTexture))
        {
            NullTextureFrames++;
            continue;
        }

        if (Frame.Timestamp < 0.0f)
        {
            InvalidTimestampFrames++;
            continue;
        }

        if (Frame.Timestamp < StartTime || Frame.Timestamp > EndTime)
        {
            OutOfRangeFrames++;
            continue;
        }

        // ? ��� ���� ���!
        ResultFrames.Add(Frame);
    }

    // ? ���� �α�
    UE_LOG(LogTemp, Warning, TEXT("?? GetFramesInRange(%.2f ~ %.2f):"), StartTime, EndTime);
    UE_LOG(LogTemp, Warning, TEXT("   Total frames searched: %d"), TotalFrames);
    UE_LOG(LogTemp, Warning, TEXT("   NULL texture frames: %d"), NullTextureFrames);
    UE_LOG(LogTemp, Warning, TEXT("   Invalid timestamp frames: %d"), InvalidTimestampFrames);
    UE_LOG(LogTemp, Warning, TEXT("   Out of range frames: %d"), OutOfRangeFrames);
    UE_LOG(LogTemp, Warning, TEXT("   ? Extracted frames: %d"), ResultFrames.Num());

    // �ð��� ����
    ResultFrames.Sort([](const FVideoFrame& A, const FVideoFrame& B) {
        return A.Timestamp < B.Timestamp;
        });

    return ResultFrames;
}

int32 UVideoBufferComponent::GetBufferedFrameCount() const
{
    if (!bFrameBufferInitialized)
    {
        UE_LOG(LogTemp, Error, TEXT("? GetBufferedFrameCount: Frame not Initialized"));
        return 0;
    }

    int32 Count = 0;
    float LatestTime = 0.0f;
    int32 LatestIndex = -1;

    for (int32 i = 0; i < FrameBuffer.Num(); ++i)
    {
        int32 Index = (CurrentWriteIndex + i) % FrameBuffer.Num();
        const FVideoFrame& Frame = FrameBuffer[Index];

        if (Frame.FrameTexture != nullptr && Frame.Timestamp > 0.0f)
        {
            Count++;
            if (Frame.Timestamp > LatestTime)
            {
                LatestTime = Frame.Timestamp;
                LatestIndex = Index;
            }
        }
    }

    return Count;
}

// ? PATCH 2: ClearBuffer - ������ ����
void UVideoBufferComponent::ClearBuffer()
{
    UE_LOG(LogTemp, Warning, TEXT("?? ClearBuffer called"));

    // ? GC ���̸� �������� �ʱ�
    if (IsGarbageCollecting())
    {
        UE_LOG(LogTemp, Warning, TEXT("?? GC in progress, deferring clear"));
        return;
    }

    // ? ���÷��� ���̸� �������� �ʱ�
    if (bIsReplaying)
    {
        UE_LOG(LogTemp, Warning, TEXT("?? Replaying, skipping clear"));
        return;
    }

    if (!bFrameBufferInitialized || FrameBuffer.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("?? Buffer not initialized, skipping clear"));
        return;
    }

    int32 ClearedCount = 0;
    for (int32 i = 0; i < FrameBuffer.Num(); ++i)
    {
        FVideoFrame& Frame = FrameBuffer[i];

        if (Frame.FrameTexture)
        {
            if (!IsTextureFromPool(Frame.FrameTexture))
            {
                PendingDeleteTextures.AddUnique(Frame.FrameTexture);
                ClearedCount++;
            }
            Frame.FrameTexture = nullptr;
        }
        Frame.Timestamp = -1.0f;
    }

    CurrentWriteIndex = 0;
    LastCleanupTime = 0.0f;

    UE_LOG(LogTemp, Log, TEXT("? VideoBuffer cleared (%d textures queued for deletion)"), ClearedCount);
}

float UVideoBufferComponent::GetBufferUsage() const
{
    if (MaxBufferSize == 0)
    {
        return 0.0f;
    }

    int32 UsedFrames = GetBufferedFrameCount();
    return static_cast<float>(UsedFrames) / static_cast<float>(MaxBufferSize);
}

void UVideoBufferComponent::SetMaxBufferSize(int32 NewSize)
{
    if (NewSize < 30 || NewSize > 900)
    {
        return;
    }

    if (NewSize == MaxBufferSize)
    {
        return;
    }

    if (GetBufferedFrameCount() > 0)
    {
        UE_LOG(LogTemp, Error, TEXT("? Cannot change buffer size while recording!"));
        return;
    }

    TArray<FVideoFrame> OldBuffer = FrameBuffer;
    int32 OldWriteIndex = CurrentWriteIndex;
    int32 OldMaxSize = MaxBufferSize;

    MaxBufferSize = NewSize;
    FrameBuffer.SetNum(MaxBufferSize);

    for (FVideoFrame& Frame : FrameBuffer)
    {
        Frame.FrameTexture = nullptr;
        Frame.Timestamp = 0.0f;
    }

    int32 FramesToCopy = FMath::Min(OldMaxSize, MaxBufferSize);
    CurrentWriteIndex = 0;

    for (int32 i = 0; i < FramesToCopy; ++i)
    {
        int32 OldIndex = (OldWriteIndex - FramesToCopy + i + OldMaxSize) % OldMaxSize;
        if (OldBuffer[OldIndex].FrameTexture)
        {
            FrameBuffer[CurrentWriteIndex] = OldBuffer[OldIndex];
            CurrentWriteIndex = (CurrentWriteIndex + 1) % MaxBufferSize;
        }
    }

    UE_LOG(LogTemp, Log, TEXT("? Buffer resized: %d �� %d"), OldMaxSize, NewSize);
}

void UVideoBufferComponent::SetAutoCleanup(bool bEnabled)
{
    bAutoCleanup = bEnabled;
    UE_LOG(LogTemp, Log, TEXT("?? Auto cleanup %s"), bEnabled ? TEXT("ENABLED") : TEXT("DISABLED"));
}

void UVideoBufferComponent::CleanOldFrames(float CutoffTime)
{
    if (bIsReplaying)
    {
        UE_LOG(LogTemp, Log, TEXT("?? Cleanup skipped: Replaying"));
        return;
    }

    int32 CleanedCount = 0;
    for (int32 i = 0; i < MaxBufferSize; ++i)
    {
        FVideoFrame& Frame = FrameBuffer[i];

        if (!Frame.FrameTexture)
        {
            continue;
        }

        if (Frame.Timestamp < CutoffTime)
        {
            PendingDeleteTextures.AddUnique(Frame.FrameTexture);
            Frame.FrameTexture = nullptr;
            Frame.Timestamp = -1.0f;
            CleanedCount++;
        }
    }

    LastCleanupTime = CutoffTime;
}

float UVideoBufferComponent::GetEstimatedMemoryUsageMB() const
{
    int32 UsedFrames = GetBufferedFrameCount();
    constexpr int32 EstimatedBytesPerFrame = 640 * 480 * 4;
    int64 TotalBytes = static_cast<int64>(UsedFrames) * EstimatedBytesPerFrame;

    return static_cast<float>(TotalBytes) / (640.0f * 480.0f);
}

void UVideoBufferComponent::ToggleVideoSaving(bool bEnable)
{
    bEnableVideoSaving = bEnable;
    UE_LOG(LogTemp, Log, TEXT("?? Video Buffer Saving %s"), bEnable ? TEXT("ENABLED") : TEXT("DISABLED"));
}

void UVideoBufferComponent::SetIsReplaying(bool bReplaying)
{
    if (bReplaying != bIsReplaying)
    {
        bIsReplaying = bReplaying;
        UE_LOG(LogTemp, Warning, TEXT("?? SetIsReplaying: %s"),
            bReplaying ? TEXT("START") : TEXT("STOP"));

        if (!bReplaying)
        {
            LastCleanupTime = 0.0f;
            UE_LOG(LogTemp, Log, TEXT("?? Cleanup will be performed after replay ends"));
        }
    }
}

void UVideoBufferComponent::ProcessPendingDeleteTextures()
{
    if (PendingDeleteTextures.Num() == 0 || bIsReplaying)
        return;

    int32 DeletedCount = 0;
    TArray<UTexture2D*> RemainingTextures;

    for (UTexture2D* Texture : PendingDeleteTextures)
    {
        if (IsGarbageCollecting())
        {
            RemainingTextures.Add(Texture);
            return;
        }

        if (IsValid(Texture) && !Texture->IsRooted())
        {
            Texture->MarkAsGarbage();
            DeletedCount++;
        }
        else if (IsValid(Texture))
        {
            RemainingTextures.Add(Texture);
        }
    }

    PendingDeleteTextures = RemainingTextures;
}

AWebcamCapture* UVideoBufferComponent::GetWebcamCapture() const
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return nullptr;
    }

    return Cast<AWebcamCapture>(Owner);
}

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
void UVideoBufferComponent::SaveDebugFrame(UTexture2D* Frame, float Time, int32 FrameIndex)
{
    if (!IsValid(Frame))
        return;

    const FString SaveDir = FPaths::ProjectSavedDir() / TEXT("DebugFrames/");
    IFileManager::Get().MakeDirectory(*SaveDir, true);

    const FString FileName = FString::Printf(TEXT("buffer_%04d_t%.2f.jpg"), FrameIndex, Time);
    const FString FilePath = SaveDir / FileName;

    const int32 Width = Frame->GetSizeX();
    const int32 Height = Frame->GetSizeY();
    if (Width <= 0 || Height <= 0)
        return;

#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION <= 26
    if (!Frame->PlatformData || Frame->PlatformData->Mips.Num() == 0)
        return;

    FTexture2DMipMap& Mip = Frame->PlatformData->Mips[0];
#else
    if (!Frame->GetPlatformData() || Frame->GetPlatformData()->Mips.Num() == 0)
        return;

    FTexture2DMipMap& Mip = Frame->GetPlatformData()->Mips[0];
#endif

    void* Data = Mip.BulkData.Lock(LOCK_READ_ONLY);
    const int32 BulkSize = Mip.BulkData.GetBulkDataSize();

    if (!Data || BulkSize <= 0)
    {
        Mip.BulkData.Unlock();
        return;
    }

    IImageWrapperModule& ImageWrapperModule =
        FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

    TSharedPtr<IImageWrapper> ImageWrapper =
        ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);

    bool bOK = false;

    if (ImageWrapper.IsValid() &&
        ImageWrapper->SetRaw(Data, BulkSize, Width, Height, ERGBFormat::BGRA, 8))
    {
        const TArray64<uint8>& JPEGData = ImageWrapper->GetCompressed(50);
        bOK = FFileHelper::SaveArrayToFile(JPEGData, *FilePath);
    }

    Mip.BulkData.Unlock();
}
#endif