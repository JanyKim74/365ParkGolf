// SupertonicTTSSubsystem.cpp
#include "SupertonicTTSSubsystem.h"
#include "SoundManager.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundWaveProcedural.h"
#include "Misc/Paths.h"
#include "Async/Async.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"

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
    // ── 1. DLL 선점 로드 (엔진 NNERuntimeORT 의 동명 DLL 공유) ──
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
        TPri_BelowNormal); // 렌더/게임 스레드 우선 — FPS 경합 방지

    bEngineAvailable = true;
    UE_LOG(LogSupertonicTTS, Log, TEXT("🔊 SupertonicTTS 서브시스템 시작 (에셋: %s)"), *Worker->AssetDir);
#else
    UE_LOG(LogSupertonicTTS, Log, TEXT("🔊 WITH_SUPERTONIC=0 — 서브시스템 비활성"));
#endif
}

void USupertonicTTSSubsystem::Deinitialize()
{
    // 타이머 정리
    if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
    {
        World->GetTimerManager().ClearTimer(DuckReleaseTimer);
    }

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

    PCMCache.Empty();
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

FString USupertonicTTSSubsystem::MakeCacheKey(const FString& Text, const FString& VoiceName, const FString& Lang, float Speed)
{
    return FString::Printf(TEXT("%s|%s|%d|%u"), *VoiceName, *Lang, FMath::RoundToInt(Speed * 100.f), GetTypeHash(Text));
}

void USupertonicTTSSubsystem::SpeakDynamic(const FString& Text, const FString& VoiceName, const FString& Lang, float Speed)
{
    if (Text.IsEmpty())
    {
        return;
    }

    const FString UseVoice = VoiceName.IsEmpty() ? CurrentVoiceName : VoiceName;
    const FString CacheKey = MakeCacheKey(Text, UseVoice, Lang, Speed);

    // ── 캐시 히트: 재합성 생략, PCM으로 새 wave 만들어 재생 ──
    //    (procedural wave는 큐 소진으로 재사용 불가 → 매번 새로 생성)
    if (FCachedPCM* Cached = PCMCache.Find(CacheKey))
    {
        UE_LOG(LogSupertonicTTS, Verbose, TEXT("🔊 캐시 히트(PCM): \"%s\""), *Text.Left(30));
        USoundWave* Wave = CreateSoundWaveFromPCM(Cached->PCM, Cached->SampleRate, Text.Left(20));
        if (Wave)
        {
            const float Dur = (float)Cached->PCM.Num() / FMath::Max(Cached->SampleRate, 1);
            PlayViaSoundManager(Wave, Dur);
        }
        return;
    }

    if (!bEngineAvailable || !Worker)
    {
        UE_LOG(LogSupertonicTTS, Warning, TEXT("🔊 엔진 불가 — SpeakDynamic 무시: \"%s\""), *Text.Left(30));
        return;
    }

    FSupertonicSynthParams Params;
    Params.Text = Text;
    Params.Lang = Lang;
    Params.VoiceName = UseVoice;
    Params.Speed = Speed;
    Worker->Enqueue(Params, CacheKey, /*bAutoPlay=*/true);
}

void USupertonicTTSSubsystem::SynthesizeOnly(const FString& Text, const FString& VoiceName, const FString& Lang, float Speed)
{
    if (Text.IsEmpty() || !bEngineAvailable || !Worker)
    {
        return;
    }
    const FString UseVoice = VoiceName.IsEmpty() ? CurrentVoiceName : VoiceName;
    FSupertonicSynthParams Params;
    Params.Text = Text;
    Params.Lang = Lang;
    Params.VoiceName = UseVoice;
    Params.Speed = Speed;
    Worker->Enqueue(Params, MakeCacheKey(Text, UseVoice, Lang, Speed), /*bAutoPlay=*/false);
}

void USupertonicTTSSubsystem::ClearCache()
{
    PCMCache.Empty();
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

    // 캐시 저장: PCM 원본 (wave 아님 — procedural 재생 시 소진되므로)
    if (PCMCache.Num() >= MaxCacheEntries)
    {
        PCMCache.Empty();
    }
    FCachedPCM Entry;
    Entry.PCM = Result.PCM;           // 복사 (Result는 이 함수 종료 후 소멸)
    Entry.SampleRate = Result.SampleRate;
    PCMCache.Add(CacheKey, Entry);

    USoundWave* Wave = CreateSoundWaveFromPCM(Result.PCM, Result.SampleRate, SourceText.Left(20));
    if (!Wave)
    {
        return;
    }

    if (bAutoPlay)
    {
        PlayViaSoundManager(Wave, Result.DurationSec);
    }

    OnSynthesized.Broadcast(Wave, SourceText);
}

void USupertonicTTSSubsystem::PlayViaSoundManager(USoundWave* Wave, float DurationSec)
{
    USoundManager* SM = USoundManager::Get(this);
    if (!SM || !Wave)
    {
        return;
    }

    UE_LOG(LogSupertonicTTS, Log, TEXT("🔊 재생 시작: Dur=%.2fs, Ch=%d, SR=%d, Vol=%.2f"),
        DurationSec, (int32)Wave->NumChannels, (int32)Wave->GetSampleRateForCurrentPlatform(), TTSVolume);

    // 기존 덕킹/인터럽트 흐름 재사용. FadeInNext 대신 볼륨은 wave/SoundClass로.
    SM->PlayTTS_Interrupt(Wave);

    // ⚠️ USoundWaveProcedural 은 OnAudioFinished 를 쏘지 않아
    //    SoundManager 의 OnVoiceFinished→EndDuckBGM 이 안 온다.
    //    Duration 기반 타이머로 수동 종료(=덕킹 해제).
    if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
    {
        World->GetTimerManager().ClearTimer(DuckReleaseTimer); // 이전 재생 타이머 취소(latest-wins 일관)
        const float Delay = FMath::Max(DurationSec + 0.15f, 0.2f);
        TWeakObjectPtr<USupertonicTTSSubsystem> WeakThis(this);
        World->GetTimerManager().SetTimer(DuckReleaseTimer,
            [WeakThis]()
            {
                if (USupertonicTTSSubsystem* Self = WeakThis.Get())
                {
                    if (USoundManager* SM2 = USoundManager::Get(Self))
                    {
                        SM2->StopTTS(); // 내부에서 EndDuckBGM 호출
                    }
                }
            },
            Delay, false);
    }
}

// ── float PCM → USoundWaveProcedural (16bit) ───────────────────
USoundWave* USupertonicTTSSubsystem::CreateSoundWaveFromPCM(const TArray<float>& PCM, int32 SampleRate, const FString& DebugName)
{
    if (PCM.Num() == 0 || SampleRate <= 0)
    {
        return nullptr;
    }

    constexpr int32 NumChannels = 1;

    USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this,
        MakeUniqueObjectName(this, USoundWaveProcedural::StaticClass(),
            *FString::Printf(TEXT("STT_%s"), *DebugName)));

    Wave->SetSampleRate(SampleRate);
    Wave->NumChannels = NumChannels;
    Wave->Duration = (float)PCM.Num() / SampleRate;
    Wave->SoundGroup = SOUNDGROUP_Voice;
    Wave->bLooping = false;
    Wave->Volume = TTSVolume;   // procedural 에서 반영 폭은 제한적 — 필요시 SoundClass 병행

    // float [-1,1] → int16, 볼륨 곱해 통째로 큐잉 (스트리밍/청크 아님)
    const int32 N = PCM.Num();
    TArray<uint8> Bytes;
    Bytes.SetNumUninitialized(N * sizeof(int16));
    int16* Dst = reinterpret_cast<int16*>(Bytes.GetData());
    for (int32 i = 0; i < N; ++i)
    {
        const float V = PCM[i] * TTSVolume;   // 볼륨을 샘플에 직접 반영 (확실)
        Dst[i] = (int16)FMath::Clamp(FMath::RoundToInt(V * 32767.f), -32768, 32767);
    }
    Wave->QueueAudio(Bytes.GetData(), Bytes.Num());

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
    FString Error;
    if (!Engine->Initialize(AssetDir, Error))
    {
        UE_LOG(LogSupertonicTTS, Error, TEXT("🔊 워커: 엔진 초기화 실패 — %s"), *Error);
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

        FSupertonicSynthResult Result = Engine->Synthesize(Request.Params);

        const FString SourceText = Request.Params.Text;
        const FString CacheKey = Request.CacheKey;
        const bool bAutoPlay = Request.bAutoPlay;
        TWeakObjectPtr<USupertonicTTSSubsystem> OwnerCopy = OwnerWeak;

        AsyncTask(ENamedThreads::GameThread,
            [OwnerCopy, SourceText, CacheKey, Result = MoveTemp(Result), bAutoPlay]() mutable
            {
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