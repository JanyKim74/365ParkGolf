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
    Ort::SessionOptions SessionOptions;

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
    static void* DllHandle = nullptr;
    if (DllHandle)
    {
        return true; // 이미 선점됨
    }

    const FString DllPath = FPaths::ConvertRelativePathToFull(
        FPaths::ProjectDir() / TEXT("Binaries/Win64/onnxruntime.dll"));

    if (!FPaths::FileExists(DllPath))
    {
        OutError = FString::Printf(TEXT("onnxruntime.dll 없음: %s"), *DllPath);
        return false;
    }

    DllHandle = FPlatformProcess::GetDllHandle(*DllPath);
    if (!DllHandle)
    {
        OutError = FString::Printf(TEXT("LoadLibrary 실패: %s"), *DllPath);
        return false;
    }

    UE_LOG(LogSupertonic, Log, TEXT("🔊 onnxruntime.dll 선점 로드 완료: %s"), *DllPath);
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

    try
    {
        // CPU 추론 설정: DX12가 GPU 점유 → CPU EP 고정, 게임 프레임 경합 최소화
        Impl->SessionOptions.SetIntraOpNumThreads(2);
        Impl->SessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        Impl->Env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "SupertonicParkDay");
        Impl->MemoryInfo = std::make_unique<Ort::MemoryInfo>(
            Ort::MemoryInfo::CreateCpu(OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault));

        const std::string OnnxDirUtf8 = ToUtf8(OnnxDir);

        // ── 설정 + 4개 세션 + 토크나이저 로드 (벤더 함수, 소유는 FImpl) ──
        // ⚠️ 공식 loadTextToSpeech() 는 함수-로컬 static 에 세션을 보관하는
        //    수명 핵이 있어 사용하지 않는다. 동일 로직을 직접 소유로 재구성.
        const Config Cfgs = loadCfgs(OnnxDirUtf8);
        Impl->SampleRate = Cfgs.ae.sample_rate;

        Impl->DpSession        = loadOnnx(*Impl->Env, OnnxDirUtf8 + "/duration_predictor.onnx", Impl->SessionOptions);
        Impl->TextEncSession   = loadOnnx(*Impl->Env, OnnxDirUtf8 + "/text_encoder.onnx",      Impl->SessionOptions);
        Impl->VectorEstSession = loadOnnx(*Impl->Env, OnnxDirUtf8 + "/vector_estimator.onnx",  Impl->SessionOptions);
        Impl->VocoderSession   = loadOnnx(*Impl->Env, OnnxDirUtf8 + "/vocoder.onnx",           Impl->SessionOptions);

        Impl->TextProcessor = loadTextProcessor(OnnxDirUtf8);

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
        Shutdown();
        return false;
    }

    bInitialized = true;
    UE_LOG(LogSupertonic, Log, TEXT("🔊 Supertonic 엔진 초기화 완료 (SR=%d): %s"),
        Impl->SampleRate, *InAssetDir);
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
FSupertonicSynthResult FSupertonicEngine::Synthesize(const FSupertonicSynthParams&) { return {}; }
void FSupertonicEngine::Shutdown() {}

#endif // WITH_SUPERTONIC && PLATFORM_WINDOWS
