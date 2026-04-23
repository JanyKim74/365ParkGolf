// SoundManager.cpp

#include "SoundManager.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "AudioDevice.h"
#include "Structs/DataTableStruct.h"

// -------------- Subsystem life-cycle --------------

void USoundManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    DuckRefCount = 0;
    EnsureComponents();
}

void USoundManager::Deinitialize()
{
    // Stop all and fully clear duck
    if (VoiceComp)
    {
        VoiceComp->OnAudioFinished.RemoveAll(this);
        VoiceComp->Stop();
    }
    if (BGMComp)
    {
        BGMComp->Stop();
    }
    // Pop duck as many times as pushed (safety)
    while (DuckRefCount > 0)
    {
        EndDuckBGM();
    }

    Super::Deinitialize();
}

const FSoundTableRow* GetRowChecked(UDataTable* Table, FName Id)
{
    if (!Table) return nullptr;
    return Table->FindRow<FSoundTableRow>(Id, TEXT("SoundLookup"));
}

USoundBase* ResolveSoundSync(const FSoundTableRow* Row, TMap<FName, TWeakObjectPtr<USoundBase>>& Cache, FName Id)
{
    if (!Row) 
    {        
        UE_LOG(LogTemp, Error, TEXT("ResolveSoundSync() ==> SoundTable is null"));
        return nullptr;
    }

    if (TWeakObjectPtr<USoundBase>* Found = Cache.Find(Id))
    {
        if (Found->IsValid()) return Found->Get();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("ResolveSoundSync() ==> %s (id) is null"), *Id.ToString());
    }

    USoundBase* Loaded = Row->Sound.LoadSynchronous(); // 4.26: 동기 로드
    if (Loaded)
    {
        Cache.Add(Id, Loaded);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("ResolveSoundSync() ==> 사운드 로드 실패"));
    }
    return Loaded;
}

// --- 2D/3D 재생 ---
void USoundManager::Play2D_ById(FName Id)
{
    if (!SoundTable) return;
    if (const FSoundTableRow* Row = GetRowChecked(SoundTable, Id))
        if (USoundBase* Snd = ResolveSoundSync(Row, LoadedCache, Id))
        {
            UGameplayStatics::PlaySound2D(GetWorldChecked(), Snd, Row->Volume, Row->Pitch);
        }
}

void USoundManager::PlayAtLocation_ById(FName Id, const FVector& Loc, float Volume)
{
    if (!SoundTable) return;
    if (const FSoundTableRow* Row = GetRowChecked(SoundTable, Id))
        if (USoundBase* Snd = ResolveSoundSync(Row, LoadedCache, Id))
        {
            UGameplayStatics::PlaySoundAtLocation(GetWorldChecked(), Snd, Loc, Volume, Row->Pitch);
        }
}

// --- BGM/TTS도 같은 방식 ---
void USoundManager::PlayBGM_ById(FName Id, float FadeTime)
{
    if (!SoundTable) return;
    if (const FSoundTableRow* Row = GetRowChecked(SoundTable, Id))
        if (USoundBase* Snd = ResolveSoundSync(Row, LoadedCache, Id))
        {
            PlayBGM(Snd, FadeTime, /*bLoop는 에셋 설정*/ true);
        }
}

void USoundManager::PlayTTS_Interrupt_ById(FName Id, float FadeOutCurrent, float FadeInNext)
{
    if (!SoundTable) return;
    if (const FSoundTableRow* Row = GetRowChecked(SoundTable, Id))
        if (USoundBase* Snd = ResolveSoundSync(Row, LoadedCache, Id))
        {
            PlayTTS_Interrupt(Snd, FadeOutCurrent, FadeInNext);
        }
}


// -------------- Internal --------------

UWorld* USoundManager::GetWorldChecked() const
{
    // UGameInstanceSubsystem already provides GetWorld(), but be explicit
    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    check(World);
    return World;
}

void USoundManager::EnsureComponents()
{
    UWorld* World = GetWorldChecked();

    if (!BGMComp)
    {
        BGMComp = NewObject<UAudioComponent>(World);
        BGMComp->bAutoActivate = false;
        BGMComp->bIsUISound = false;
        if (BGMClass) BGMComp->SoundClassOverride = BGMClass;
        BGMComp->RegisterComponentWithWorld(World);
    }

    if (!VoiceComp)
    {
        VoiceComp = NewObject<UAudioComponent>(World);
        VoiceComp->bAutoActivate = false;
        VoiceComp->bIsUISound = false;
        if (VoiceClass)       VoiceComp->SoundClassOverride = VoiceClass;
        if (VoiceConcurrency) VoiceComp->ConcurrencySet.Add(VoiceConcurrency);
        VoiceComp->OnAudioFinished.AddDynamic(this, &USoundManager::OnVoiceFinished);
        VoiceComp->RegisterComponentWithWorld(World);
    }
}

void USoundManager::SetupSoundPolicy(USoundClass* InBGMClass, USoundClass* InVoiceClass,
    USoundMix* InDuckMix, USoundConcurrency* InVoiceConcurrency, UDataTable* InTable)
{
    BGMClass = InBGMClass;
    VoiceClass = InVoiceClass;
    DuckMix = InDuckMix;                // ← 에디터에서 만든 DuckMix 자산
    VoiceConcurrency = InVoiceConcurrency;
    SoundTable = InTable;


    EnsureComponents(); // 있으면 즉시 반영
    if (BGMComp && BGMClass)   BGMComp->SoundClassOverride = BGMClass;
    if (VoiceComp && VoiceClass) {
        VoiceComp->SoundClassOverride = VoiceClass;
        VoiceComp->ConcurrencySet.Empty();       // 4.26은 ConcurrencySet
        if (VoiceConcurrency) VoiceComp->ConcurrencySet.Add(VoiceConcurrency);
    }

    UE_LOG(LogTemp, Log, TEXT("GameInstance Setup Success"));
}

// -------------- BGM --------------

void USoundManager::PlayBGM(USoundBase* Sound, float FadeTime, bool bLoop)
{
    EnsureComponents();
    if (!Sound || !BGMComp) return;

    // 다른 트랙이 재생 중이면 페이드아웃
    if (BGMComp->IsPlaying() && BGMComp->Sound != Sound)
    {
        BGMComp->FadeOut(FadeTime, 0.f);
    }

    BGMComp->SetSound(Sound);

    // ★ AudioComponent에는 bLooping이 없음 → USoundWave일 때만 에셋 플래그로 제어
    if (USoundWave* Wave = Cast<USoundWave>(Sound))
    {
        Wave->bLooping = bLoop; // 주의: 에셋 인스턴스 전체에 적용됨(런타임 메모리 상)
    }
    else
    {
        // SoundCue는 코드로 못 바꿈. Cue 내부에 Looping 노드가 있어야 루프됨.
         UE_LOG(LogTemp, Warning, TEXT("[SoundManager] SoundCue uses its own Looping node; bLoop param is ignored."));
    }

    BGMComp->FadeIn(FadeTime, 1.f); // 루프는 에셋 설정에 따름
}

bool USoundManager::BGMIsPlaying()
{
    if (BGMComp && BGMComp->IsPlaying())
        return true;
    return false;
}

void USoundManager::StopBGM(float FadeOutTime)
{
    if (BGMComp && BGMComp->IsPlaying())
    {
        BGMComp->FadeOut(FadeOutTime, 0.f);
    }
}

void USoundManager::SetBGMPaused(bool bPaused)
{
    if (BGMComp)
    {
        BGMComp->SetPaused(bPaused);
    }
}



// -------------- TTS / Voice (no queue) --------------

void USoundManager::PlayTTS_Interrupt(USoundBase* Voice, float FadeOutCurrent, float FadeInNext)
{
    EnsureComponents();
    if (!Voice || !VoiceComp) return;

    // 1) Short fade-out current to avoid click/pop
    if (VoiceComp->IsPlaying())
    {
        if (FadeOutCurrent > 0.f) VoiceComp->FadeOut(FadeOutCurrent, 0.f);
        else VoiceComp->Stop();
        // Note: OnAudioFinished will fire after FadeOut; duck should remain active.
    }

    // 2) Begin duck (ref-counted)
    BeginDuckBGM();

    // 3) Play next immediately
    VoiceComp->SetSound(Voice);
    if (FadeInNext > 0.f) VoiceComp->FadeIn(FadeInNext, 1.f);
    else                  VoiceComp->Play();
}

void USoundManager::PlayTTS_Turn_ById(FName Id, int32 PlayerNo,
    float FadeOutCurrent, float FadeInNext)
{
    EnsureComponents();
    if (!SoundTable || !VoiceComp) return;

    if (const FSoundTableRow* Row = GetRowChecked(SoundTable, Id))
        if (USoundBase* Snd = ResolveSoundSync(Row, LoadedCache, Id))
        {
            // 2) Switch(Int) 파라미터 세팅 (큐 내부의 Switch 노드 'TurnIndex'와 이름 일치해야 함)
            static const FName ParamName_TurnIndex(TEXT("PlayerIndex"));
            const int32 SwitchIndex = FMath::Clamp(PlayerNo, 0, 5); // 플레이어 1~6 → 인덱스 0~5
            VoiceComp->SetIntParameter(ParamName_TurnIndex, SwitchIndex);

            // 3) 우리의 즉시 교체 재생 함수 사용 (덕팅 자동)
            PlayTTS_Interrupt(Snd, FadeOutCurrent, FadeInNext);
        }
}

void USoundManager::PlayTTS_Turn(USoundBase* TurnCue, int32 PlayerNo, float FadeOutCurrent, float FadeInNext)
{
    EnsureComponents();
    if (!TurnCue || !VoiceComp) return;

    static const FName ParamName(TEXT("PlayerIndex"));
    const int32 SwitchIndex = FMath::Clamp(PlayerNo - 1, 0, 5); // 0~5
    VoiceComp->SetIntParameter(ParamName, SwitchIndex);         // ★ 플레이 전 파라미터 세팅

    PlayTTS_Interrupt(TurnCue, FadeOutCurrent, FadeInNext);     // 우리 TTS 즉시 교체 함수
}

void USoundManager::StopTTS()
{
    if (!VoiceComp) return;

    const bool bWasPlaying = VoiceComp->IsPlaying();
    VoiceComp->Stop();

    // If we explicitly stopped and nothing else will finish-callback us, end duck once.
    if (bWasPlaying)
    {
        EndDuckBGM();
    }
}

bool USoundManager::IsTTSPlaying() const
{
    return VoiceComp && VoiceComp->IsPlaying();
}

void USoundManager::OnVoiceFinished()
{
    // Voice finished naturally: end one level of duck.
    EndDuckBGM();
}

// -------------- Ducking (SoundMix) --------------

void USoundManager::BeginDuckBGM()
{
    if (!DuckMix) return;

    UWorld* World = GetWorldChecked();
    UGameplayStatics::PushSoundMixModifier(World, DuckMix);
    ++DuckRefCount;
}

void USoundManager::EndDuckBGM()
{
    if (!DuckMix) return;
    if (DuckRefCount <= 0) return; // safety against double-pop

    UWorld* World = GetWorldChecked();
    UGameplayStatics::PopSoundMixModifier(World, DuckMix);
    --DuckRefCount;
}

// -------------- SFX / Utility --------------

void USoundManager::Play2D(USoundBase* Sound, float Volume, float Pitch)
{
    if (!Sound) return;
    UGameplayStatics::PlaySound2D(GetWorldChecked(), Sound, Volume, Pitch);
}

void USoundManager::PlayAtLocation(USoundBase* Sound, const FVector& Loc, float Volume, float Pitch)
{
    if (!Sound) return;
    UGameplayStatics::PlaySoundAtLocation(GetWorldChecked(), Sound, Loc, Volume, Pitch);
}

// -------------- Access helper --------------

USoundManager* USoundManager::Get(const UObject* WorldContext)
{
    if (!WorldContext) return nullptr;
    if (const UWorld* World = WorldContext->GetWorld())
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            return GI->GetSubsystem<USoundManager>();
        }
    }
    return nullptr;
}


