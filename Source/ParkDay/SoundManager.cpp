// SoundManager.cpp

#include "SoundManager.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "AudioDevice.h"
#include "Structs/DataTableStruct.h"

// -------------- Subsystem life-cycle --------------

void USoundManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    DuckRefCount = 0;

    // ✅ 월드 정리 직전에 컴포넌트를 미리 unregister 하도록 훅 등록.
    //    (부팅 시 임시 "Untitled" 월드가 CleanupWorld 될 때 발생하던
    //     "components incorrectly unregistered after world cleanup" ensure 방지)
    WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
        this, &USoundManager::HandleWorldCleanup);

    // ⚠️ 여기서 EnsureComponents()를 호출하지 않는다.
    //    Initialize 시점엔 유효한 게임 월드가 없을 수 있어(부팅 임시 월드) 등록이
    //    잘못된 월드에 걸린다. 컴포넌트는 최초 재생 시 EnsureComponents()에서
    //    현재 월드 기준으로 생성/등록된다.
}

void USoundManager::Deinitialize()
{
    // ✅ 델리게이트 해제 (댕글링 방지)
    if (WorldCleanupHandle.IsValid())
    {
        FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
        WorldCleanupHandle.Reset();
    }

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

    // ✅ 컴포넌트를 현재 월드에서 떼어낸다
    UnregisterAudioComponents();

    // Pop duck as many times as pushed (safety)
    while (DuckRefCount > 0)
    {
        EndDuckBGM();
    }

    Super::Deinitialize();
}

// ✅ 월드 정리 직전 콜백: 우리가 등록해 둔 월드가 정리될 때만 컴포넌트를 떼어낸다.
void USoundManager::HandleWorldCleanup(UWorld* World, bool /*bSessionEnded*/, bool /*bCleanupResources*/)
{
    if (World && World == RegisteredWorld)
    {
        UnregisterAudioComponents();
    }
}

// ✅ BGM/Voice 컴포넌트를 현재 등록 월드에서 안전하게 unregister
void USoundManager::UnregisterAudioComponents()
{
    if (BGMComp)
    {
        if (BGMComp->IsPlaying()) BGMComp->Stop();
        if (BGMComp->IsRegistered()) BGMComp->UnregisterComponent();
    }
    if (VoiceComp)
    {
        if (VoiceComp->IsPlaying()) VoiceComp->Stop();
        if (VoiceComp->IsRegistered()) VoiceComp->UnregisterComponent();
    }
    RegisteredWorld = nullptr;
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
    // ✅ check() 제거: 월드가 없을 수 있는 시점(부팅/전환)에 호출돼도 크래시 대신
    //    nullptr을 반환해 호출부가 안전하게 방어하도록 한다.
    return GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
}

void USoundManager::EnsureComponents()
{
    UWorld* World = GetWorldChecked();

    // ✅ 유효한 게임 월드가 아직 없으면(부팅 임시 월드 등) 등록을 미룬다.
    if (!IsValid(World) || !World->IsGameWorld())
    {
        return;
    }

    // ✅ 월드가 바뀌었으면 이전 월드에서 먼저 떼어낸 뒤 새 월드에 재등록한다.
    //    (컴포넌트 오브젝트 자체는 GameInstance 수명이라 유지, 등록만 갈아끼움)
    if (RegisteredWorld != World)
    {
        if (BGMComp && BGMComp->IsRegistered())     BGMComp->UnregisterComponent();
        if (VoiceComp && VoiceComp->IsRegistered()) VoiceComp->UnregisterComponent();
        RegisteredWorld = World;
    }

    if (!BGMComp)
    {
        // ✅ Outer = this(SoundManager, GameInstance 수명) → 월드에 종속되지 않음
        BGMComp = NewObject<UAudioComponent>(this);
        BGMComp->bAutoActivate = false;
        BGMComp->bIsUISound = false;
        if (BGMClass) BGMComp->SoundClassOverride = BGMClass;
    }
    if (!BGMComp->IsRegistered())
    {
        BGMComp->RegisterComponentWithWorld(World);
    }

    if (!VoiceComp)
    {
        VoiceComp = NewObject<UAudioComponent>(this);   // ✅ Outer = this
        VoiceComp->bAutoActivate = false;
        VoiceComp->bIsUISound = false;
        if (VoiceClass)       VoiceComp->SoundClassOverride = VoiceClass;
        if (VoiceConcurrency) VoiceComp->ConcurrencySet.Add(VoiceConcurrency);
        VoiceComp->OnAudioFinished.AddDynamic(this, &USoundManager::OnVoiceFinished);
    }
    if (!VoiceComp->IsRegistered())
    {
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


    // 유효한 게임 월드가 있으면 컴포넌트를 즉시 생성/등록 (없으면 최초 재생 시 생성됨)
    EnsureComponents();
    // ✅ 컴포넌트가 아직 생성 전일 수 있으므로 null 방어 (정책 값은 위에서 이미 멤버에 저장됨 →
    //    이후 EnsureComponents에서 SoundClassOverride 등이 반영된다)
    if (BGMComp && BGMClass)   BGMComp->SoundClassOverride = BGMClass;
    if (VoiceComp && VoiceClass) {
        VoiceComp->SoundClassOverride = VoiceClass;
        VoiceComp->ConcurrencySet.Empty();       // 4.26은 ConcurrencySet
        if (VoiceConcurrency) VoiceComp->ConcurrencySet.Add(VoiceConcurrency);
    }

    UE_LOG(LogTemp, Log, TEXT("GameInstance Setup Success"));
}

// ✅ 레벨 전환 직전 명시적 정리. 멱등(여러 번 호출/이미 정리됨 상태에도 안전).
void USoundManager::CleanupBeforeLevelTravel()
{
    // 재생 중인 보이스가 있으면 덕팅을 먼저 해제해 ref-count가 새는 것을 막는다.
    if (VoiceComp && VoiceComp->IsPlaying())
    {
        VoiceComp->Stop();
    }
    if (BGMComp && BGMComp->IsPlaying())
    {
        BGMComp->Stop();
    }

    // 덕팅 ref-count를 모두 되돌린다 (전환 후 잔여 duck 방지).
    while (DuckRefCount > 0)
    {
        EndDuckBGM();
    }

    // 컴포넌트를 현재 월드에서 떼어낸다. (다음 재생 시 EnsureComponents가 새 월드에 재등록)
    UnregisterAudioComponents();

    UE_LOG(LogTemp, Log, TEXT("[SoundManager] CleanupBeforeLevelTravel: audio stopped & unregistered"));
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
    if (!IsValid(World)) return;   // ✅ 월드 없으면 ref count 손상 방지 위해 skip
    UGameplayStatics::PushSoundMixModifier(World, DuckMix);
    ++DuckRefCount;
}

void USoundManager::EndDuckBGM()
{
    if (!DuckMix) return;
    if (DuckRefCount <= 0) return; // safety against double-pop

    UWorld* World = GetWorldChecked();
    if (!IsValid(World)) { --DuckRefCount; return; }  // ✅ 월드 없으면 pop 없이 카운트만 정리
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