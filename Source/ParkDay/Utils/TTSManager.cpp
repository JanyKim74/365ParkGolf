// Fill out your copyright notice in the Description page of Project Settings.

#include "TTSManager.h"

#if PLATFORM_WINDOWS

#include "Windows/AllowWindowsPlatformTypes.h"

#pragma warning(push)
#pragma warning(disable: 4005)
#pragma warning(disable: 4459)
#pragma warning(disable: 4191)
#pragma warning(disable: 4996)

#include <sapi.h>

#pragma warning(pop)

#include "Windows/HideWindowsPlatformTypes.h"

#endif

// ─────────────────────────────────────────────────────────────────────────────
// [생성자/소멸자]
// ─────────────────────────────────────────────────────────────────────────────

FTTSManager::FTTSManager()
	: bInitialized(false)
	, LastErrorCode(S_OK)
	, LastErrorMessage(TEXT("No Error"))
	, CurrentRate(0)
	, CurrentVolume(90)
#if PLATFORM_WINDOWS
	, Voice(nullptr)
#endif
{
#if PLATFORM_WINDOWS
	// ✅ CoInitializeEx 제거!
	// UE4가 이미 COM을 초기화했으므로 바로 Voice 객체 생성

	if (!InitializeVoice())
	{
		UE_LOG(LogTemp, Error, TEXT("[TTS] Voice 객체 초기화 실패"));
		return;
	}

	bInitialized = true;
	UE_LOG(LogTemp, Warning, TEXT("[TTS] 초기화 완료"));
#else
	UE_LOG(LogTemp, Warning, TEXT("[TTS] Windows 플랫폼이 아니므로 TTS 비활성화"));
#endif
}

FTTSManager::~FTTSManager()
{
#if PLATFORM_WINDOWS
	if (Voice)
	{
		Voice->Release();
		Voice = nullptr;
	}

	// ✅ CoUninitialize 제거!
	// UE4가 COM을 관리하므로 우리가 해제하면 안 됨

	UE_LOG(LogTemp, Warning, TEXT("[TTS] 정리 완료"));
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// [공개 인터페이스 구현]
// ─────────────────────────────────────────────────────────────────────────────

bool FTTSManager::Speak(const FString& Text)
{
	if (!bInitialized)
	{
		LastErrorMessage = TEXT("TTS 시스템이 초기화되지 않았습니다");
		UE_LOG(LogTemp, Error, TEXT("[TTS] %s"), *LastErrorMessage);
		return false;
	}

	if (Text.IsEmpty())
	{
		LastErrorMessage = TEXT("재생할 텍스트가 비어있습니다");
		UE_LOG(LogTemp, Warning, TEXT("[TTS] %s"), *LastErrorMessage);
		return false;
	}

#if PLATFORM_WINDOWS
	if (!Voice)
	{
		LastErrorMessage = TEXT("Voice 객체가 유효하지 않습니다");
		UE_LOG(LogTemp, Error, TEXT("[TTS] %s"), *LastErrorMessage);
		return false;
	}

	const wchar_t* WideText = *Text;
	HRESULT hr = Voice->Speak(WideText, SPF_ASYNC, nullptr);

	if (FAILED(hr))
	{
		HandleError(hr, TEXT("음성 재생"));
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("[TTS] 재생: %s"), *Text);
	LastErrorMessage = TEXT("No Error");
	return true;

#else
	LastErrorMessage = TEXT("Windows 플랫폼이 아닙니다");
	UE_LOG(LogTemp, Warning, TEXT("[TTS] %s"), *LastErrorMessage);
	return false;
#endif
}

bool FTTSManager::Stop()
{
	if (!bInitialized)
	{
		LastErrorMessage = TEXT("TTS 시스템이 초기화되지 않았습니다");
		return false;
	}

#if PLATFORM_WINDOWS
	if (!Voice)
	{
		LastErrorMessage = TEXT("Voice 객체가 유효하지 않습니다");
		return false;
	}

	HRESULT hr = Voice->Skip(L"SENTENCE", 1000, nullptr);

	if (FAILED(hr))
	{
		HandleError(hr, TEXT("음성 중지"));
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("[TTS] 재생 중지"));
	return true;

#else
	return false;
#endif
}

bool FTTSManager::IsInitialized() const
{
	return bInitialized;
}

bool FTTSManager::IsPlaying() const
{
#if PLATFORM_WINDOWS
	if (!bInitialized || !Voice)
	{
		return false;
	}

	SPVOICESTATUS VoiceStatus;
	HRESULT hr = Voice->GetStatus(&VoiceStatus, nullptr);

	if (SUCCEEDED(hr))
	{
		return VoiceStatus.dwRunningState == SPRS_IS_SPEAKING;
	}

	return false;

#else
	return false;
#endif
}

bool FTTSManager::SetRate(int32 Rate)
{
	if (!bInitialized)
	{
		LastErrorMessage = TEXT("TTS 시스템이 초기화되지 않았습니다");
		return false;
	}

	int32 ClampedRate = FMath::Clamp(Rate, -10, 10);

#if PLATFORM_WINDOWS
	if (!Voice)
	{
		LastErrorMessage = TEXT("Voice 객체가 유효하지 않습니다");
		return false;
	}

	HRESULT hr = Voice->SetRate(ClampedRate);

	if (FAILED(hr))
	{
		HandleError(hr, TEXT("음성 속도 설정"));
		return false;
	}

	CurrentRate = ClampedRate;
	UE_LOG(LogTemp, Warning, TEXT("[TTS] 음성 속도 설정: %d"), ClampedRate);
	return true;

#else
	CurrentRate = ClampedRate;
	return false;
#endif
}

bool FTTSManager::SetVolume(int32 Volume)
{
	if (!bInitialized)
	{
		LastErrorMessage = TEXT("TTS 시스템이 초기화되지 않았습니다");
		return false;
	}

	int32 ClampedVolume = FMath::Clamp(Volume, 0, 100);

#if PLATFORM_WINDOWS
	if (!Voice)
	{
		LastErrorMessage = TEXT("Voice 객체가 유효하지 않습니다");
		return false;
	}

	HRESULT hr = Voice->SetVolume(ClampedVolume);

	if (FAILED(hr))
	{
		HandleError(hr, TEXT("볼륨 설정"));
		return false;
	}

	CurrentVolume = ClampedVolume;
	UE_LOG(LogTemp, Warning, TEXT("[TTS] 볼륨 설정: %d"), ClampedVolume);
	return true;

#else
	CurrentVolume = ClampedVolume;
	return false;
#endif
}

int32 FTTSManager::GetRate() const
{
	return CurrentRate;
}

int32 FTTSManager::GetVolume() const
{
	return CurrentVolume;
}

FString FTTSManager::GetLastError() const
{
	return LastErrorMessage;
}

long FTTSManager::GetLastErrorCode() const
{
	return LastErrorCode;
}

// ─────────────────────────────────────────────────────────────────────────────
// [Windows 전용 헬퍼 함수 구현]
// ─────────────────────────────────────────────────────────────────────────────

#if PLATFORM_WINDOWS

bool FTTSManager::InitializeVoice()
{
	// ✅ CoInitializeEx 제거!
	// UE4가 이미 COM을 초기화했으므로 바로 Voice 생성

	// Voice 객체 생성 (COM 이미 초기화됨)
	HRESULT hr = CoCreateInstance(
		CLSID_SpVoice,
		nullptr,
		CLSCTX_ALL,
		IID_ISpVoice,
		(void**)&Voice
	);

	if (FAILED(hr) || !Voice)
	{
		LastErrorCode = hr;
		LastErrorMessage = HRESULTToString(hr);
		UE_LOG(LogTemp, Error, TEXT("[TTS] Voice 객체 생성 실패: 0x%08X - %s"), hr, *LastErrorMessage);
		Voice = nullptr;
		return false;
	}

	// 기본 설정
	Voice->SetRate(CurrentRate);
	Voice->SetVolume(CurrentVolume);

	UE_LOG(LogTemp, Warning, TEXT("[TTS] Voice 객체 생성 완료"));
	return true;
}

void FTTSManager::HandleError(long hr, const FString& ErrorContext)
{
	LastErrorCode = hr;
	LastErrorMessage = HRESULTToString(hr);

	UE_LOG(LogTemp, Error, TEXT("[TTS] %s 실패: 0x%08X - %s"), *ErrorContext, hr, *LastErrorMessage);
}

FString FTTSManager::HRESULTToString(long hr) const
{
	switch (hr)
	{
	case S_OK:
		return TEXT("성공");
	case S_FALSE:
		return TEXT("실패");
	case E_INVALIDARG:
		return TEXT("잘못된 인수");
	case E_OUTOFMEMORY:
		return TEXT("메모리 부족");
	case E_NOINTERFACE:
		return TEXT("인터페이스를 찾을 수 없음");
	case E_POINTER:
		return TEXT("포인터 오류");
	case E_HANDLE:
		return TEXT("핸들 오류");
	case E_ABORT:
		return TEXT("작업 중단");
	case E_FAIL:
		return TEXT("일반 실패");
	case E_UNEXPECTED:
		return TEXT("예상치 못한 오류");
	case 0x80070002:
		return TEXT("파일을 찾을 수 없음");
	default:
		return FString::Printf(TEXT("알 수 없는 오류 (0x%08X)"), hr);
	}
}

#endif