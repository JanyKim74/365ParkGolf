// SupertonicTTSSubsystem.cpp
#include "SupertonicTTSSubsystem.h"
#include "SoundManager.h"
#include "Sound/SoundWave.h"
#include "Misc/Paths.h"
#include "Async/Async.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogSupertonicTTS, Log, All);

// ═════════════════════════════════════════════════════════════════════
// USupertonicTTSSubsystem
// ═════════════════════════════════════════════════════════════════════

void USupertonicTTSSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // SoundManager 이후 초기화 보장 (재생 경로 의존)
    Collection.InitializeDependency<USoundManager>();

#if WITH_SUPERTONIC
    // ── 1. DLL 선점 로드 (엔진 NNERuntimeORT 의 동명 DLL 회피) ──
    //    ⚠️ 반드시 모든 ORT 호출 전, 게임 스레드에서.
    FString DllError;
    if (!FSupertonicEngine::PreloadOrtDll(DllError))
    {
        UE_LOG(LogSupertonicTTS, Warning,
            TEXT("🔊 Supertonic 비활성화 (DLL): %s — 기존 SAPI/사전생성 WAV 경로로 폴백"), *DllError);
        return;
    }

    // ── 2. 엔진 + 워커 생성. 모델 로드는 워커 Init()에서 (게임 스레드 블로킹 방지) ──
    Engine = MakeUnique<FSupertonicEngine>();
    Worker = MakeUnique<FSupertonicWorker>(Engine.Get(), this);

    // FPaths::ProjectContentDir() 는 패키지드에서 신뢰 불가 → ProjectDir 기반
    Worker->AssetDir = FPaths::ConvertRelativePathToFull(
        FPaths::ProjectDir() / TEXT("Content/DATA/Supertonic"));

    WorkerThread = FRunnableThread::Create(
        Worker.Get(),
        TEXT("SupertonicTTSWorker"),
        0,
        TPri_BelowNormal); // 렌더/게임 스레드 우선 — 13FPS 이슈 재발 방지

    bEngineAvailable = true;
    UE_LOG(LogSupertonicTTS, Log, TEXT("🔊 SupertonicTTS 서브시스템 시작 (에셋: %s)"), *Worker->AssetDir);
#else
    UE_LOG(LogSupertonicTTS, Log, TEXT("🔊 WITH_SUPERTONIC=0 — 서브시스템 비활성"));
#endif
}

void USupertonicTTSSubsystem::Deinitialize()
{
    // 순서 중요: 스레드 정지 → 스레드 파괴 → 엔진 해제
    if (Worker)
    {
        Worker->Stop();
    }
    if (WorkerThread)
    {
        WorkerThread->WaitForCompletion(); // 진행 중 합성 완료 대기
        delete WorkerThread;
        WorkerThread = nullptr;
    }
    Worker.Reset();

    if (Engine)
    {
        Engine->Shutdown();
        Engine.Reset();
    }

    SynthCache.Empty();
    bEngineAvailable = false;

    Super::Deinitialize();
}

bool USupertonicTTSSubsystem::IsReady() const
{
    return bEngineAvailable && Engine && Engine->IsInitialized();
}

USupertonicTTSSubsystem* USupertonicTTSSubsystem::Get(const UObject* WorldContext)
{
    if (const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr)
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            return GI->GetSubsystem<USupertonicTTSSubsystem>();
        }
    }
    return nullptr;
}

FString USupertonicTTSSubsystem::MakeCacheKey(const FString& Text, const FString& VoiceName, const FString& Lang)
{
    return FString::Printf(TEXT("%s|%s|%u"), *VoiceName, *Lang, GetTypeHash(Text));
}

void USupertonicTTSSubsystem::SpeakDynamic(const FString& Text, const FString& VoiceName, const FString& Lang)
{
    if (Text.IsEmpty())
    {
        return;
    }

    const FString CacheKey = MakeCacheKey(Text, VoiceName, Lang);

    // ── 캐시 히트: 합성 생략, 즉시 재생 ─────────────────────────
    if (TObjectPtr<USoundWave>* Cached = SynthCache.Find(CacheKey))
    {
        if (IsValid(*Cached))
        {
            UE_LOG(LogSupertonicTTS, Verbose, TEXT("🔊 캐시 히트: \"%s\""), *Text.Left(30));
            PlayViaSoundManager(*Cached);
            return;
        }
        SynthCache.Remove(CacheKey);
    }

    if (!bEngineAvailable || !Worker)
    {
        UE_LOG(LogSupertonicTTS, Warning, TEXT("🔊 엔진 불가 — SpeakDynamic 무시: \"%s\""), *Text.Left(30));
        return;
    }

    FSupertonicSynthParams Params;
    Params.Text = Text;
    Params.Lang = Lang;
    Params.VoiceName = VoiceName;
    Worker->Enqueue(Params, CacheKey, /*bAutoPlay=*/true);
}

void USupertonicTTSSubsystem::SynthesizeOnly(const FString& Text, const FString& VoiceName, const FString& Lang)
{
    if (Text.IsEmpty() || !bEngineAvailable || !Worker)
    {
        return;
    }
    FSupertonicSynthParams Params;
    Params.Text = Text;
    Params.Lang = Lang;
    Params.VoiceName = VoiceName;
    Worker->Enqueue(Params, MakeCacheKey(Text, VoiceName, Lang), /*bAutoPlay=*/false);
}

void USupertonicTTSSubsystem::ClearCache()
{
    SynthCache.Empty();
}

// ── 워커 → 게임 스레드 결과 수신 ───────────────────────────────
void USupertonicTTSSubsystem::HandleSynthesisResult_GameThread(
    const FString& SourceText, const FString& CacheKey,
    FSupertonicSynthResult Result, bool bAutoPlay)
{
    check(IsInGameThread());

    if (!Result.bSuccess)
    {
        UE_LOG(LogSupertonicTTS, Warning, TEXT("🔊 합성 실패 \"%s\": %s"),
            *SourceText.Left(30), *Result.ErrorMessage);
        return;
    }

    USoundWave* Wave = CreateSoundWaveFromPCM(Result.PCM, Result.SampleRate, SourceText.Left(20));
    if (!Wave)
    {
        return;
    }

    // 캐시 저장 (상한 초과 시 단순 전체 클리어 — 안내 문구 특성상 재합성 저비용)
    if (SynthCache.Num() >= MaxCacheEntries)
    {
        SynthCache.Empty();
    }
    SynthCache.Add(CacheKey, Wave);

    if (bAutoPlay)
    {
        PlayViaSoundManager(Wave);
    }

    OnSynthesized.Broadcast(Wave, SourceText);
}

void USupertonicTTSSubsystem::PlayViaSoundManager(USoundWave* Wave)
{
    if (USoundManager* SM = USoundManager::Get(this))
    {
        // 기존 덕킹/인터럽트/OnVoiceFinished 흐름 전부 재사용
        SM->PlayTTS_Interrupt(Wave);
    }
}

// ── float PCM → 16bit transient USoundWave ─────────────────────
USoundWave* USupertonicTTSSubsystem::CreateSoundWaveFromPCM(const TArray<float>& PCM, int32 SampleRate, const FString& DebugName)
{
    if (PCM.Num() == 0 || SampleRate <= 0)
    {
        return nullptr;
    }

    constexpr int32 NumChannels = 1;

    USoundWave* Wave = NewObject<USoundWave>(this,
        MakeUniqueObjectName(this, USoundWave::StaticClass(),
            *FString::Printf(TEXT("STT_%s"), *DebugName)));

    const int32 NumSamples = PCM.Num();
    const int32 DataSize = NumSamples * sizeof(int16);

    // float [-1,1] → int16 변환
    TArray<int16> Converted;
    Converted.SetNumUninitialized(NumSamples);
    for (int32 i = 0; i < NumSamples; ++i)
    {
        Converted[i] = (int16)FMath::Clamp(
            FMath::RoundToInt(PCM[i] * 32767.f), -32768, 32767);
    }

    Wave->RawPCMDataSize = DataSize;
    Wave->RawPCMData = (uint8*)FMemory::Malloc(DataSize);
    FMemory::Memcpy(Wave->RawPCMData, Converted.GetData(), DataSize);

    Wave->Duration = (float)NumSamples / SampleRate;
    Wave->SetSampleRate(SampleRate);
    Wave->NumChannels = NumChannels;
    Wave->TotalSamples = NumSamples;
    Wave->bLooping = false;
    Wave->SoundGroup = SOUNDGROUP_Voice;

    return Wave;
}

// ═════════════════════════════════════════════════════════════════════
// FSupertonicWorker
// ═════════════════════════════════════════════════════════════════════

FSupertonicWorker::FSupertonicWorker(FSupertonicEngine* InEngine, TWeakObjectPtr<USupertonicTTSSubsystem> InOwner)
    : Engine(InEngine)
    , OwnerWeak(InOwner)
{
    WakeEvent = FPlatformProcess::GetSynchEventFromPool(false);
}

FSupertonicWorker::~FSupertonicWorker()
{
    if (WakeEvent)
    {
        FPlatformProcess::ReturnSynchEventToPool(WakeEvent);
        WakeEvent = nullptr;
    }
}

bool FSupertonicWorker::Init()
{
    // 모델 로드 (수 초 소요 가능) — 워커 스레드라 게임 스레드 무영향.
    FString Error;
    if (!Engine->Initialize(AssetDir, Error))
    {
        UE_LOG(LogSupertonicTTS, Error, TEXT("🔊 워커: 엔진 초기화 실패 — %s"), *Error);
        // 스레드는 유지하되 합성 요청은 전부 실패 처리됨
    }
    return true;
}

uint32 FSupertonicWorker::Run()
{
    while (!bStopRequested)
    {
        WakeEvent->Wait();

        if (bStopRequested)
        {
            break;
        }

        // 요청 꺼내기 (latest-wins 슬롯)
        FRequest Request;
        {
            FScopeLock Lock(&RequestLock);
            if (!PendingRequest.bValid)
            {
                continue;
            }
            Request = PendingRequest;
            PendingRequest.bValid = false;
        }

        if (!Engine->IsInitialized())
        {
            UE_LOG(LogSupertonicTTS, Warning, TEXT("🔊 워커: 엔진 미준비 — 요청 폐기"));
            continue;
        }

        // ── 합성 (블로킹, 이 스레드에서만) ──────────────────────
        FSupertonicSynthResult Result = Engine->Synthesize(Request.Params);

        // ── 게임 스레드로 결과 전달 (GC 안전: WeakPtr 재검증) ───
        const FString SourceText = Request.Params.Text;
        const FString CacheKey = Request.CacheKey;
        const bool bAutoPlay = Request.bAutoPlay;
        TWeakObjectPtr<USupertonicTTSSubsystem> OwnerCopy = OwnerWeak;

        AsyncTask(ENamedThreads::GameThread,
            [OwnerCopy, SourceText, CacheKey, Result = MoveTemp(Result), bAutoPlay]() mutable
            {
                // ⚠️ 레벨 트래블/종료 중 서브시스템 소멸 가능 — 반드시 재검증
                if (USupertonicTTSSubsystem* Owner = OwnerCopy.Get())
                {
                    Owner->HandleSynthesisResult_GameThread(SourceText, CacheKey, MoveTemp(Result), bAutoPlay);
                }
            });
    }
    return 0;
}

void FSupertonicWorker::Stop()
{
    bStopRequested = true;
    if (WakeEvent)
    {
        WakeEvent->Trigger();
    }
}

void FSupertonicWorker::Enqueue(const FSupertonicSynthParams& Params, const FString& CacheKey, bool bAutoPlay)
{
    {
        FScopeLock Lock(&RequestLock);
        if (PendingRequest.bValid)
        {
            UE_LOG(LogSupertonicTTS, Verbose, TEXT("🔊 대기 요청 교체 (latest-wins): \"%s\" → \"%s\""),
                *PendingRequest.Params.Text.Left(20), *Params.Text.Left(20));
        }
        PendingRequest.Params = Params;
        PendingRequest.CacheKey = CacheKey;
        PendingRequest.bAutoPlay = bAutoPlay;
        PendingRequest.bValid = true;
    }
    WakeEvent->Trigger();
}
