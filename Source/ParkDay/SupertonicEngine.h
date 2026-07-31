// SupertonicEngine.h
// Supertonic 3 ONNX Runtime 네이티브 래퍼 (UObject 아님, 게임 스레드 비의존)
//
// ⚠️ 이 헤더는 ONNX Runtime / 벤더 헤더를 절대 include하지 않는다 (pimpl).
//    → UE 매크로 충돌 및 컴파일 오염 방지. 구현부는 SupertonicEngine.cpp 에만.
#pragma once

#include "CoreMinimal.h"

// Build.cs 미적용/타 플랫폼 등 어떤 경우에도 매크로 미정의로 인한
// C4668(경고→에러 승격)을 방지하는 폴백
#ifndef WITH_SUPERTONIC
#define WITH_SUPERTONIC 0
#endif

/** 합성 요청 파라미터 */
struct FSupertonicSynthParams
{
    FString Text;                   // 합성할 텍스트 (한국어 포함)
    FString Lang = TEXT("ko");      // 언어 코드 ("ko", "en", "ja", ... "na" = 언어 무관)
    FString VoiceName = TEXT("M1"); // voice_styles/<VoiceName>.json
    int32   TotalSteps = 8;         // 품질 5(저)~12(고), 기본 8
    float   Speed = 1.05f;          // 0.7(느림)~2.0(빠름)
};

/** 합성 결과: float PCM mono (샘플레이트는 tts.json 에서 결정, 보통 44100) */
struct FSupertonicSynthResult
{
    TArray<float> PCM;              // [-1, 1] float 샘플, mono
    int32 SampleRate = 44100;       // 엔진 설정(tts.json ae.sample_rate)에서 채워짐
    float DurationSec = 0.f;
    bool bSuccess = false;
    FString ErrorMessage;
};

/**
 * Supertonic 3 추론 엔진.
 * - Initialize / Synthesize / Shutdown 은 워커 스레드 전용 (게임 스레드 호출 금지 — 블로킹)
 * - 내부 구현은 공식 helper (ThirdParty/Supertonic/src) 벤더 코드에 위임
 * - 공식 loadTextToSpeech() 의 함수-로컬 static 수명 핵은 사용하지 않음 —
 *   세션/프로세서 전부 FImpl 이 직접 소유 (재초기화/종료 안전)
 */
class PARKDAY_API FSupertonicEngine
{
public:
    FSupertonicEngine();
    ~FSupertonicEngine();

    FSupertonicEngine(const FSupertonicEngine&) = delete;
    FSupertonicEngine& operator=(const FSupertonicEngine&) = delete;

    /**
     * @param InAssetDir  모델 에셋 루트 (예: <Project>/Content/DATA/Supertonic)
     *                    구조:
     *                      <AssetDir>/onnx/duration_predictor.onnx
     *                      <AssetDir>/onnx/text_encoder.onnx
     *                      <AssetDir>/onnx/vector_estimator.onnx
     *                      <AssetDir>/onnx/vocoder.onnx
     *                      <AssetDir>/onnx/tts.json
     *                      <AssetDir>/onnx/unicode_indexer.json
     *                      <AssetDir>/voice_styles/M1.json ... F5.json
     */
    bool Initialize(const FString& InAssetDir, FString& OutError);

    /** 로드 완료 여부 (스레드 안전) */
    bool IsInitialized() const;

    /** 동기 합성. 워커 스레드에서만 호출. 한국어는 내부에서 120자 청크 분할 처리됨. */
    FSupertonicSynthResult Synthesize(const FSupertonicSynthParams& Params);

    /** 세션/프로세서 해제 */
    void Shutdown();

    /**
     * ⚠️ 반드시 "모든 ORT 함수 최초 호출 전"에 게임 스레드에서 1회 호출.
     * Binaries/Win64/onnxruntime.dll 을 전체 경로로 명시적 로드하여,
     * 엔진 NNERuntimeORT 가 가진 동명 DLL 대신 우리 버전이 delay-load에
     * 바인딩되도록 선점한다. (CR2 XcamAdapt64.dll 과 동일 패턴)
     */
    static bool PreloadOrtDll(FString& OutError);

private:
    struct FImpl;                  // ORT/벤더 타입은 전부 이 안에 (cpp 정의)
    TUniquePtr<FImpl> Impl;
    FThreadSafeBool bInitialized = false;
};
