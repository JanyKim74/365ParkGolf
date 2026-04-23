// SoundManager.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/StreamableManager.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundBase.h"
#include "SoundManager.generated.h"

class UAudioComponent;

/**
 * Global sound controller (lives across levels).
 * - Single BGM channel (no overlap) with fade/crossfade
 * - Single TTS/Voice channel (no queue): interrupt current and play next immediately
 * - BGM ducking while TTS is playing (SoundMix-based, ref-count safe)
 * - Utility: 2D / 3D SFX fire-and-forget
 */
UCLASS()
class PARKDAY_API USoundManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // Subsystem life-cycle
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    FORCEINLINE float MapImpulseToVolume(float ImpulseSize)
    {
        // 선형 매핑 + 클램프
        return FMath::GetMappedRangeValueClamped(
            FVector2D(50.f, 500.f),
            FVector2D(0.2f, 1.0f),
            ImpulseSize
        );
    }

    UPROPERTY(EditAnywhere, Category = "Sound|Data")
        UDataTable* SoundTable = nullptr;

    // Soft → Hard 로드 캐시 (이미 로드된 것 재사용)
    TMap<FName, TWeakObjectPtr<USoundBase>> LoadedCache;

    // 비동기 로드용 스트리머
    TSharedPtr<FStreamableManager> Streamer;

    // 테이블에서 바로 찾아 재생
    UFUNCTION(BlueprintCallable, Category = "Sound|SFX")
        void Play2D_ById(FName Id);

    UFUNCTION(BlueprintCallable, Category = "Sound|SFX")
        void PlayAtLocation_ById(FName Id, const FVector& Loc, float Volume);

    // BGM/TTS도 ById 버전 준비
    UFUNCTION(BlueprintCallable, Category = "Sound|BGM")
        void PlayBGM_ById(FName Id, float FadeTime = 1.f);

    UFUNCTION(BlueprintCallable, Category = "Sound|Voice")
        void PlayTTS_Interrupt_ById(FName Id, float FadeOutCurrent = 0.05f, float FadeInNext = 0.f);

    UFUNCTION(BlueprintCallable, Category = "Sound|Voice")
        void PlayTTS_Turn(USoundBase* TurnCue, int32 PlayerNo, float FadeOutCurrent = 0.05f, float FadeInNext = 0.f);

    // 플레이어 턴 보이스: DataTable의 Id를 사용해 사운드 큐 재생
    UFUNCTION(BlueprintCallable, Category = "Sound|Voice")
        void PlayTTS_Turn_ById(FName Id, int32 PlayerNo,
            float FadeOutCurrent = 0.05f, float FadeInNext = 0.f);
    // -------- BGM --------
    /** Play/replace BGM with fade-in. Stops previous if different. */
    UFUNCTION(BlueprintCallable, Category = "Sound|BGM")
        void PlayBGM(USoundBase* Sound, float FadeTime = 1.f, bool bLoop = true);

    /** Crossfade helper (alias of PlayBGM). */
    UFUNCTION(BlueprintCallable, Category = "Sound|BGM")
        void CrossfadeBGM(USoundBase* Next, float FadeTime = 1.f) { PlayBGM(Next, FadeTime, true); }

    UFUNCTION()
    bool BGMIsPlaying();
    /** Stop BGM with fade-out. */
    UFUNCTION(BlueprintCallable, Category = "Sound|BGM")
        void StopBGM(float FadeOutTime = 1.f);

    /** Pause / Resume BGM. */
    UFUNCTION(BlueprintCallable, Category = "Sound|BGM")
        void SetBGMPaused(bool bPaused);

    // -------- TTS / Voice (no queue) --------
    /**
     * Interrupt current voice (short fade-out) and play next immediately.
     * @param FadeOutCurrent short fade to avoid click/pop (0~0.1s recommended)
     * @param FadeInNext     optional fade-in on new voice
     */
    UFUNCTION(BlueprintCallable, Category = "Sound|Voice")
        void PlayTTS_Interrupt(USoundBase* Voice, float FadeOutCurrent = 0.05f, float FadeInNext = 0.0f);

    /** Stop current TTS immediately and end duck if no more voice is playing. */
    UFUNCTION(BlueprintCallable, Category = "Sound|Voice")
        void StopTTS();

    /** Is TTS/Voice currently playing? */
    UFUNCTION(BlueprintPure, Category = "Sound|Voice")
        bool IsTTSPlaying() const;

    // -------- SFX / Utility --------
    UFUNCTION(BlueprintCallable, Category = "Sound|SFX")
        void Play2D(USoundBase* Sound, float Volume = 1.f, float Pitch = 1.f);

    UFUNCTION(BlueprintCallable, Category = "Sound|SFX")
        void PlayAtLocation(USoundBase* Sound, const FVector& Loc, float Volume = 1.f, float Pitch = 1.f);

    // -------- Access helper --------
    /** Static accessor for convenience: USoundManager::Get(WorldContext) */
    UFUNCTION(BlueprintPure, Category = "Sound")
        static USoundManager* Get(const UObject* WorldContext);

    // SoundManager.h
    UFUNCTION(BlueprintCallable, Category = "Sound|Setup")
        void SetupSoundPolicy(USoundClass* InBGMClass, USoundClass* InVoiceClass,
            USoundMix* InDuckMix, USoundConcurrency* InVoiceConcurrency, UDataTable* InTable);
private:
    // Runtime audio components (transient; registered to world)
    UPROPERTY(Transient) UAudioComponent* BGMComp = nullptr;
    UPROPERTY(Transient) UAudioComponent* VoiceComp = nullptr;

    // Policy assets (assign in editor or load via soft refs / DT)
    UPROPERTY(EditAnywhere, Category = "Assets|Classes") USoundClass* BGMClass = nullptr;
    UPROPERTY(EditAnywhere, Category = "Assets|Classes") USoundClass* VoiceClass = nullptr;
    UPROPERTY(EditAnywhere, Category = "Assets|Mixes")  USoundMix* DuckMix = nullptr; // lowers only BGM
    UPROPERTY(EditAnywhere, Category = "Assets|Concurrency") USoundConcurrency* VoiceConcurrency = nullptr;

    // Duck mix ref-count to avoid premature pop when voices overlap by timing
    int32 DuckRefCount = 0;

    // Internal helpers
    void EnsureComponents();
    UWorld* GetWorldChecked() const;

    // Ducking control
    void BeginDuckBGM();
    void EndDuckBGM();

    // Voice finished callback
    UFUNCTION() void OnVoiceFinished();
};
