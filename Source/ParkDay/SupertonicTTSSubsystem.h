// SupertonicTTSSubsystem.h
// Supertonic 3 온디바이스 TTS 서브시스템 (레벨 트래블 무관, GameInstance 수명)
//
// 아키텍처:
//   게임 스레드                          워커 스레드 (FSupertonicWorker)
//   ─────────────                        ──────────────────────────────
//   SpeakDynamic("...")  ──요청 큐──▶    FSupertonicEngine::Synthesize()
//                                        (latest-wins: 대기 요청은 최신 것만 유지)
//   OnSynthesisReady  ◀──AsyncTask──     float PCM 완성
//   → PCM 캐시 저장 + USoundWaveProcedural 생성
//   → SoundManager->PlayTTS_Interrupt()
//
// ⚠️ 캐시는 wave가 아니라 PCM(float)을 저장한다.
//    USoundWaveProcedural 은 재생 시 내부 큐가 소진되어 재사용 불가 —
//    같은 문장을 다시 재생하면 빈 wave가 되어 무음이 된다.
//    따라서 재사용 대상은 "무거운 ONNX 추론 결과 PCM"으로 두고,
//    재생할 때마다 그 PCM으로 새 procedural wave 를 만든다.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Engine/TimerHandle.h"
#include "SupertonicEngine.h"
#include "SupertonicTTSSubsystem.generated.h"

class USoundWave;
class FSupertonicWorker;

/** 합성 완료 시 게임 스레드에서 브로드캐스트 (UI 자막 연동 등에 활용) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTTSSynthesized, USoundWave*, Sound, const FString&, SourceText);

UCLASS()
class PARKDAY_API USupertonicTTSSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ── Subsystem 수명 ──────────────────────────────────────────
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ── 공개 API ────────────────────────────────────────────────

    /**
     * 동적 텍스트 합성 → 완성 즉시 SoundManager::PlayTTS_Interrupt() 재생.
     * 합성 중 재호출 시 대기 중이던 이전 요청은 폐기 (latest-wins).
     * VoiceName 을 비우면 현재 설정된 기본 화자(CurrentVoiceName)를 사용.
     */
    UFUNCTION(BlueprintCallable, Category = "TTS|Supertonic")
    void SpeakDynamic(const FString& Text,
        const FString& VoiceName = TEXT(""),
        const FString& Lang = TEXT("ko"),
        float Speed = 1.05f);

    /** 재생 없이 합성만 수행. 결과는 OnSynthesized 델리게이트로 수신. */
    UFUNCTION(BlueprintCallable, Category = "TTS|Supertonic")
    void SynthesizeOnly(const FString& Text,
        const FString& VoiceName = TEXT(""),
        const FString& Lang = TEXT("ko"),
        float Speed = 1.05f);

    /** 기본 화자 설정. 이후 VoiceName 을 생략한 SpeakDynamic 호출에 적용된다. */
    UFUNCTION(BlueprintCallable, Category = "TTS|Supertonic")
    void SetVoice(const FString& VoiceName) { if (!VoiceName.IsEmpty()) CurrentVoiceName = VoiceName; }

    /** 현재 기본 화자 이름 */
    UFUNCTION(BlueprintPure, Category = "TTS|Supertonic")
    FString GetVoice() const { return CurrentVoiceName; }

    /** TTS 재생 볼륨 (0~2, 기본 1.5). 다음 재생부터 적용. */
    UFUNCTION(BlueprintCallable, Category = "TTS|Supertonic")
    void SetVolume(float InVolume) { TTSVolume = FMath::Clamp(InVolume, 0.f, 2.f); }

    /** 엔진 사용 가능 여부 (DLL 로드 + 모델 로드 완료) */
    UFUNCTION(BlueprintPure, Category = "TTS|Supertonic")
    bool IsReady() const;

    /** 캐시 비우기 (메모리 압박 시) */
    UFUNCTION(BlueprintCallable, Category = "TTS|Supertonic")
    void ClearCache();

    /** 편의 접근자: USupertonicTTSSubsystem::Get(WorldContext) */
    UFUNCTION(BlueprintPure, Category = "TTS|Supertonic")
    static USupertonicTTSSubsystem* Get(const UObject* WorldContext);

    UPROPERTY(BlueprintAssignable, Category = "TTS|Supertonic")
    FOnTTSSynthesized OnSynthesized;

    // ── 워커 → 게임 스레드 콜백 (내부용) ────────────────────────
    void HandleSynthesisResult_GameThread(const FString& SourceText,
        const FString& CacheKey,
        FSupertonicSynthResult Result,
        bool bAutoPlay);

private:
    /** float PCM → 16-bit USoundWaveProcedural 생성 (일회용, 재생마다 새로 만듦) */
    USoundWave* CreateSoundWaveFromPCM(const TArray<float>& PCM, int32 SampleRate, const FString& DebugName);

    /** 캐시 키: Voice|Lang|Speed|Text 해시 */
    static FString MakeCacheKey(const FString& Text, const FString& VoiceName, const FString& Lang, float Speed);

    void PlayViaSoundManager(USoundWave* Wave, float DurationSec);

    // ── 상태 ────────────────────────────────────────────────────

    /** 합성 엔진 (워커 스레드가 소유적으로 사용, 수명은 서브시스템) */
    TUniquePtr<FSupertonicEngine> Engine;

    /** 워커 스레드 */
    TUniquePtr<FSupertonicWorker> Worker;
    FRunnableThread* WorkerThread = nullptr;

    /** 반복 문장 캐시: wave가 아니라 PCM(float) 저장.
     *  procedural wave는 재생 시 큐가 소진되어 재사용 불가하므로,
     *  재합성(무거운 ONNX 추론) 회피용으로 PCM만 보관하고
     *  재생 때마다 새 wave를 만든다. PCM은 UObject가 아니라 GC 무관. */
    struct FCachedPCM
    {
        TArray<float> PCM;
        int32 SampleRate = 44100;
    };
    TMap<FString, FCachedPCM> PCMCache;

    static constexpr int32 MaxCacheEntries = 64;

    /** 기본 화자 (voice_styles/<이름>.json). SetVoice 로 변경. */
    FString CurrentVoiceName = TEXT("M1");

    /** TTS 재생 볼륨 */
    float TTSVolume = 1.8f;

    /** procedural wave 는 OnAudioFinished 를 쏘지 않음 → Duration 기반 덕킹 해제용 */
    FTimerHandle DuckReleaseTimer;

    bool bEngineAvailable = false;
};

// ═════════════════════════════════════════════════════════════════════
// FSupertonicWorker — 합성 전용 스레드 (latest-wins 큐)
// ═════════════════════════════════════════════════════════════════════
class FSupertonicWorker : public FRunnable
{
public:
    struct FRequest
    {
        FSupertonicSynthParams Params;
        FString CacheKey;
        bool bAutoPlay = true;
        bool bValid = false;
    };

    FSupertonicWorker(FSupertonicEngine* InEngine, TWeakObjectPtr<USupertonicTTSSubsystem> InOwner);
    virtual ~FSupertonicWorker() override;

    // FRunnable
    virtual bool Init() override;
    virtual uint32 Run() override;
    virtual void Stop() override;

    /** 게임 스레드에서 호출. 대기 중 요청이 있으면 교체 (latest-wins). */
    void Enqueue(const FSupertonicSynthParams& Params, const FString& CacheKey, bool bAutoPlay);

    /** 모델 에셋 경로 (Init 에서 엔진 초기화에 사용) */
    FString AssetDir;

private:
    FSupertonicEngine* Engine = nullptr;                       // 소유 아님 (Subsystem 소유)
    TWeakObjectPtr<USupertonicTTSSubsystem> OwnerWeak;         // 결과 전달 대상 (GC 안전)

    FCriticalSection RequestLock;
    FRequest PendingRequest;                                    // 단일 슬롯 = latest-wins

    FEvent* WakeEvent = nullptr;
    FThreadSafeBool bStopRequested = false;
};