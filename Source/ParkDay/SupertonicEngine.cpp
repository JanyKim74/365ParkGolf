// SupertonicEngine.cpp
#include "SupertonicEngine.h"

#if WITH_SUPERTONIC && PLATFORM_WINDOWS

#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"

// ─────────────────────────────────────────────────────────────────────
// 벤더 헤더 (helper.h → onnxruntime_cxx_api.h 포함)는 이 파일에서만.
// ─────────────────────────────────────────────────────────────────────
#include "Windows/AllowWindowsPlatformTypes.h"
#pragma warning(push)
#pragma warning(disable: 4996)
#pragma warning(disable: 4244)
#pragma warning(disable: 4267)
#include "ThirdParty/Supertonic/src/helper.h"
#pragma warning(pop)
#include "Windows/HideWindowsPlatformTypes.h"

#include <string>
#include <memory>
#include <map>

DEFINE_LOG_CATEGORY_STATIC(LogSupertonic, Log, All);

namespace
{
    // FString(UTF-16) → std::string(UTF-8): 벤더 코드 전체가 UTF-8 std::string 기반
    std::string ToUtf8(const FString& In)
    {
        FTCHARToUTF8 Conv(*In);
        return std::string(Conv.Get(), Conv.Length());
    }
}

// ═════════════════════════════════════════════════════════════════════
// FImpl — 공식 loadTextToSpeech() 의 static 수명 핵을 쓰지 않고 직접 소유
// ═════════════════════════════════════════════════════════════════════
struct FSupertonicEngine::FImpl
{
    // 파괴 순서 주의: 선언 역순으로 파괴됨 →
    // TTS(참조자) → Style → Processor → Sessions → MemoryInfo → Env 순으로
    // 파괴되도록 Env 를 가장 먼저 선언한다.
    std::unique_ptr<Ort::Env> Env;
    std::unique_ptr<Ort::MemoryInfo> MemoryInfo;
    std::unique_ptr<Ort::SessionOptions> SessionOptions;   // ← 값에서 포인터로

    std::unique_ptr<Ort::Session> DpSession;
    std::unique_ptr<Ort::Session> TextEncSession;
    std::unique_ptr<Ort::Session> VectorEstSession;
    std::unique_ptr<Ort::Session> VocoderSession;

    std::unique_ptr<UnicodeProcessor> TextProcessor;
    std::unique_ptr<TextToSpeech> TTS;

    // 보이스 스타일 캐시 (Style 은 복사 가능하지만 로드는 파일 IO — 1회만)
    std::map<std::string, std::unique_ptr<Style>> StyleCache;

    FString AssetDir;
    int32 SampleRate = 44100;

    Style* GetOrLoadStyle(const FString& VoiceName, FString& OutError)
    {
        const std::string Key = ToUtf8(VoiceName);
        auto Found = StyleCache.find(Key);
        if (Found != StyleCache.end())
        {
            return Found->second.get();
        }

        const FString StylePath = AssetDir / TEXT("voice_styles") / (VoiceName + TEXT(".json"));
        if (!FPaths::FileExists(StylePath))
        {
            OutError = FString::Printf(TEXT("보이스 스타일 없음: %s"), *StylePath);
            return nullptr;
        }

        // loadVoiceStyle 은 벤더 함수 (JSON → style_ttl/style_dp 텐서 데이터)
        Style Loaded = loadVoiceStyle({ ToUtf8(StylePath) }, /*verbose=*/false);
        auto Inserted = StyleCache.emplace(Key, std::make_unique<Style>(std::move(Loaded)));
        return Inserted.first->second.get();
    }
};

// ═════════════════════════════════════════════════════════════════════
// DLL 선점 로드 — 엔진 NNERuntimeORT의 동명 onnxruntime.dll 회피
// ═════════════════════════════════════════════════════════════════════
bool FSupertonicEngine::PreloadOrtDll(FString& OutError)
{
    // UE5.7 NNERuntimeORT 플러그인이 로드하는 onnxruntime.dll(1.20.x)을 공유한다.
    // 별도 DLL을 배포하지 않으므로 선점 로드 불필요 — NNE 모듈이 이미 로드했거나,
    // delay-load 시 엔진 플러그인 경로에서 해석된다.
    // 확실히 하기 위해 엔진 플러그인 DLL을 명시 경로로 한 번 로드 시도(있으면).
    static void* DllHandle = nullptr;
    if (DllHandle)
    {
        return true;
    }

    const FString EngineOrtPath = FPaths::Combine(
        FPaths::EngineDir(),
        TEXT("Plugins/NNE/NNERuntimeORT/Binaries/ThirdParty/Onnxruntime/Win64/onnxruntime.dll"));

    if (FPaths::FileExists(EngineOrtPath))
    {
        DllHandle = FPlatformProcess::GetDllHandle(*EngineOrtPath);
        UE_LOG(LogSupertonic, Log, TEXT("🔊 엔진 NNERuntimeORT DLL 공유 로드: %s"), *EngineOrtPath);
        return DllHandle != nullptr;
    }

    // 폴백: delay-load 가 알아서 해석하도록 진행 (엔진이 이미 로드했을 것)
    UE_LOG(LogSupertonic, Log, TEXT("🔊 ORT DLL 명시 경로 없음 — delay-load 해석에 위임"));
    return true;
}

// ═════════════════════════════════════════════════════════════════════
FSupertonicEngine::FSupertonicEngine()
    : Impl(MakeUnique<FImpl>())
{
}

FSupertonicEngine::~FSupertonicEngine()
{
    Shutdown();
}

bool FSupertonicEngine::IsInitialized() const
{
    return bInitialized;
}

bool FSupertonicEngine::Initialize(const FString& InAssetDir, FString& OutError)
{
    if (bInitialized)
    {
        return true;
    }

    Impl->AssetDir = InAssetDir;
    const FString OnnxDir = InAssetDir / TEXT("onnx");

    // 필수 파일 사전 검증 (벤더 코드는 예외를 던지므로 미리 친절한 에러로)
    const TCHAR* RequiredFiles[] = {
        TEXT("duration_predictor.onnx"), TEXT("text_encoder.onnx"),
        TEXT("vector_estimator.onnx"),   TEXT("vocoder.onnx"),
        TEXT("tts.json"),                TEXT("unicode_indexer.json")
    };
    for (const TCHAR* File : RequiredFiles)
    {
        if (!FPaths::FileExists(OnnxDir / File))
        {
            OutError = FString::Printf(TEXT("필수 에셋 없음: %s"), *(OnnxDir / File));
            return false;
        }
    }

    // ⚠️ 세션 생성 실패는 정상적으론 Ort::Exception(std::exception)이지만,
    //    DLL 내부 초기화가 덜 된 상태(예: provider_shared 미배치)에서는
    //    C++ 예외가 아니라 SEH access violation(0x50 근처 null)으로 죽는다.
    //    이 경우 catch(std::exception)으로 못 잡아 게임 전체가 크래시하므로
    //    SEH 를 구조화 예외 → 반환값으로 변환하는 별도 함수로 감싼다.
    const bool bOk = InitializeGuarded(OnnxDir, OutError);
    if (!bOk)
    {
        Shutdown();
        return false;
    }

    bInitialized = true;
    UE_LOG(LogSupertonic, Log, TEXT("🔊 Supertonic 엔진 초기화 완료 (SR=%d): %s"),
        Impl->SampleRate, *InAssetDir);
    return true;
}

// SEH 전용 최말단 래퍼: 소멸자 있는 타입(FString 등)을 일절 만지지 않는다.
// __try 함수에는 인자/지역 모두 원시 타입만 허용(C2712 회피).
// 크래시 시 SEH 코드를 out 파라미터로만 넘기고, 에러 메시지 문자열 구성은
// 호출부(InitializeGuarded)에서 SEH 밖에서 수행한다.
 bool RunInitializeSEH(FSupertonicEngine* Self, const FString* OnnxDir, FString* OutError, uint32* OutSEHCode)
{
    __try
    {
        return Self->DoInitialize(*OnnxDir, *OutError);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        *OutSEHCode = (uint32)GetExceptionCode();
        return false;
    }
}

bool FSupertonicEngine::InitializeGuarded(const FString& OnnxDir, FString& OutError)
{
    uint32 SEHCode = 0;
    const bool bOk = RunInitializeSEH(this, &OnnxDir, &OutError, &SEHCode);

    // SEH 로 죽었던 경우에만 에러 메시지 구성 (여기는 __try 밖이라 FString 안전)
    if (!bOk && SEHCode != 0)
    {
        OutError = FString::Printf(
            TEXT("onnxruntime.dll 내부 크래시 (SEH 0x%08X). ")
            TEXT("onnxruntime_providers_shared.dll 이 Binaries/Win64 에 함께 배치됐는지 확인 필요."),
            SEHCode);
    }
    return bOk;
}
bool FSupertonicEngine::DoInitialize(const FString& OnnxDir, FString& OutError)
{
    try
    {
        // ── 0. ORT API 버전 확인 (진단용, DLL 실제 버전 로깅) ────
        UE_LOG(LogSupertonic, Log, TEXT("🔊 [1/6] ORT 버전: DLL=%hs, 헤더 API=v%d"),
            OrtGetApiBase()->GetVersionString(), ORT_API_VERSION);

        // SessionOptions 는 여기서 생성 (ORT 최초 접촉 지점 — DLL 선점 로드 이후 보장)
        Impl->SessionOptions = std::make_unique<Ort::SessionOptions>();
        Impl->SessionOptions->SetIntraOpNumThreads(2);
        Impl->SessionOptions->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);


        UE_LOG(LogSupertonic, Log, TEXT("🔊 [2/6] Env 생성"));
        Impl->Env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "SupertonicParkDay");
        Impl->MemoryInfo = std::make_unique<Ort::MemoryInfo>(
            Ort::MemoryInfo::CreateCpu(OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault));

        const std::string OnnxDirUtf8 = ToUtf8(OnnxDir);

        UE_LOG(LogSupertonic, Log, TEXT("🔊 [3/6] tts.json 로드"));
        const Config Cfgs = loadCfgs(OnnxDirUtf8);
        Impl->SampleRate = Cfgs.ae.sample_rate;

        UE_LOG(LogSupertonic, Log, TEXT("🔊 [4/6] ONNX 세션 4개 로드"));
        Impl->DpSession = loadOnnx(*Impl->Env, OnnxDirUtf8 + "/duration_predictor.onnx", *Impl->SessionOptions);
        Impl->TextEncSession = loadOnnx(*Impl->Env, OnnxDirUtf8 + "/text_encoder.onnx", *Impl->SessionOptions);
        Impl->VectorEstSession = loadOnnx(*Impl->Env, OnnxDirUtf8 + "/vector_estimator.onnx", *Impl->SessionOptions);
        Impl->VocoderSession = loadOnnx(*Impl->Env, OnnxDirUtf8 + "/vocoder.onnx", *Impl->SessionOptions);

        UE_LOG(LogSupertonic, Log, TEXT("🔊 [5/6] 토크나이저 로드"));
        Impl->TextProcessor = loadTextProcessor(OnnxDirUtf8);

        UE_LOG(LogSupertonic, Log, TEXT("🔊 [6/6] TextToSpeech 조립"));
        Impl->TTS = std::make_unique<TextToSpeech>(
            Cfgs,
            Impl->TextProcessor.get(),
            Impl->DpSession.get(),
            Impl->TextEncSession.get(),
            Impl->VectorEstSession.get(),
            Impl->VocoderSession.get());
    }
    catch (const std::exception& E)
    {
        OutError = FString::Printf(TEXT("Supertonic 초기화 예외: %hs"), E.what());
        return false;
    }
    return true;
}

FSupertonicSynthResult FSupertonicEngine::Synthesize(const FSupertonicSynthParams& Params)
{
    FSupertonicSynthResult Result;

    if (!bInitialized || !Impl->TTS)
    {
        Result.ErrorMessage = TEXT("엔진 미초기화");
        return Result;
    }

    const double StartTime = FPlatformTime::Seconds();

    try
    {
        // ── 1. 보이스 스타일 (캐시) ──────────────────────────────
        FString StyleError;
        Style* VoiceStyle = Impl->GetOrLoadStyle(Params.VoiceName, StyleError);
        if (!VoiceStyle)
        {
            Result.ErrorMessage = StyleError;
            return Result;
        }

        // ── 2. 합성 (한국어는 벤더 call() 내부에서 120자 청크 분할) ──
        TextToSpeech::SynthesisResult Synth = Impl->TTS->call(
            *Impl->MemoryInfo,
            ToUtf8(Params.Text),
            ToUtf8(Params.Lang),
            *VoiceStyle,
            Params.TotalSteps,
            Params.Speed);

        // ── 3. duration 기준 트림 (공식 example_onnx 와 동일) ────
        //    vocoder 출력은 청크 패딩을 포함하므로 유효 길이만 사용
        const float Duration = Synth.duration.empty() ? 0.f : Synth.duration[0];
        const int32 ValidLen = FMath::Min(
            (int32)Synth.wav.size(),
            (int32)(Impl->SampleRate * Duration));

        if (ValidLen <= 0)
        {
            Result.ErrorMessage = TEXT("합성 결과가 비어있음");
            return Result;
        }

        Result.PCM.Append(Synth.wav.data(), ValidLen);
        Result.SampleRate = Impl->SampleRate;
        Result.DurationSec = (float)ValidLen / Impl->SampleRate;
        Result.bSuccess = true;

        // 벤더 전역 텐서 버퍼 해제 (호출 안 하면 합성마다 누적)
        // 워커 단일 스레드 전용이므로 여기서 호출해도 안전
        clearTensorBuffers();
    }
    catch (const std::exception& E)
    {
        Result.bSuccess = false;
        Result.ErrorMessage = FString::Printf(TEXT("합성 예외: %hs"), E.what());
        clearTensorBuffers();
    }

    const double ElapsedMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;
    UE_LOG(LogSupertonic, Log, TEXT("🔊 합성 %s: \"%s\" (%.0fms, %.2fs 오디오)"),
        Result.bSuccess ? TEXT("성공") : TEXT("실패"),
        *Params.Text.Left(30), ElapsedMs, Result.DurationSec);

    return Result;
}

void FSupertonicEngine::Shutdown()
{
    if (!Impl)
    {
        return;
    }
    // 파괴 순서: 참조자 → 피참조자 (TTS 가 세션/프로세서 raw pointer 를 들고 있음)
    Impl->TTS.reset();
    Impl->StyleCache.clear();
    Impl->TextProcessor.reset();
    Impl->VocoderSession.reset();
    Impl->VectorEstSession.reset();
    Impl->TextEncSession.reset();
    Impl->DpSession.reset();
    Impl->MemoryInfo.reset();
    Impl->SessionOptions.reset();   // ← 추가
    Impl->Env.reset();
    bInitialized = false;
}

#else // !WITH_SUPERTONIC ─ 스텁 (onnxruntime.lib 미배치 시에도 링크 성공)

struct FSupertonicEngine::FImpl {};
FSupertonicEngine::FSupertonicEngine() {}
FSupertonicEngine::~FSupertonicEngine() {}
bool FSupertonicEngine::IsInitialized() const { return false; }
bool FSupertonicEngine::PreloadOrtDll(FString& OutError) { OutError = TEXT("WITH_SUPERTONIC=0"); return false; }
bool FSupertonicEngine::Initialize(const FString&, FString& OutError) { OutError = TEXT("WITH_SUPERTONIC=0"); return false; }
bool FSupertonicEngine::InitializeGuarded(const FString&, FString&) { return false; }
bool FSupertonicEngine::DoInitialize(const FString&, FString&) { return false; }
FSupertonicSynthResult FSupertonicEngine::Synthesize(const FSupertonicSynthParams&) { return {}; }
void FSupertonicEngine::Shutdown() {}

#endif // WITH_SUPERTONIC && PLATFORM_WINDOWS
