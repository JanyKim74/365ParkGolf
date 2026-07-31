// SupertonicTTSSubsystem.h
// Supertonic 3 온디바이스 TTS 서브시스템 (레벨 트래블 무관, GameInstance 수명)
//
// 아키텍처:
//   게임 스레드                          워커 스레드 (FSupertonicWorker)
//   ─────────────                        ──────────────────────────────
//   SpeakDynamic("...")  ──요청 큐──▶    FSupertonicEngine::Synthesize()
//                                        (latest-wins: 대기 요청은 최신 것만 유지)
//   OnSynthesisReady  ◀──AsyncTask──     float PCM 완성
//   → USoundWave 생성 (RawPCMData)
//   → SoundManager->PlayTTS_Interrupt()
//
// ⚠️ USoundWaveProcedural 을 쓰지 않는 이유:
//    procedural wave 는 재생 "종료" 가 발생하지 않아 SoundManager 의
//    OnVoiceFinished() → EndDuckBGM() 이 영원히 호출되지 않는다.
//    RawPCMData 를 채운 transient USoundWave 는 Duration 이 확정되어
//    기존 덕킹/인터럽트 로직이 그대로 동작한다.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
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
     * 고정 안내문(사전 생성 WAV)은 기존 PlayTTS_Interrupt_ById 유지,
     * 이 함수는 플레이어명/점수 등 동적 문장 전용 (하이브리드 전략).
     */
    UFUNCTION(BlueprintCallable, Category = "TTS|Supertonic")
    void SpeakDynamic(const FString& Text,
                      const FString& VoiceName = TEXT("M1"),
                      const FString& Lang = TEXT("ko"));

    /** 재생 없이 합성만 수행. 결과는 OnSynthesized 델리게이트로 수신. */
    UFUNCTION(BlueprintCallable, Category = "TTS|Supertonic")
    void SynthesizeOnly(const FString& Text,
                        const FString& VoiceName = TEXT("M1"),
                        const FString& Lang = TEXT("ko"));

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
    /** float PCM → 16-bit RawPCMData transient USoundWave 생성 */
    USoundWave* CreateSoundWaveFromPCM(const TArray<float>& PCM, int32 SampleRate, const FString& DebugName);

    /** 캐시 키: Voice|Lang|Text 해시 */
    static FString MakeCacheKey(const FString& Text, const FString& VoiceName, const FString& Lang);

    void PlayViaSoundManager(USoundWave* Wave);

    // ── 상태 ────────────────────────────────────────────────────

    /** 합성 엔진 (워커 스레드가 소유적으로 사용, 수명은 서브시스템) */
    TUniquePtr<FSupertonicEngine> Engine;

    /** 워커 스레드 */
    TUniquePtr<FSupertonicWorker> Worker;
    FRunnableThread* WorkerThread = nullptr;

    /** 반복 문장 캐시 (UPROPERTY → GC 보호). 상한 초과 시 전체 클리어 */
    UPROPERTY(Transient)
    TMap<FString, TObjectPtr<USoundWave>> SynthCache;

    static constexpr int32 MaxCacheEntries = 64;

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
