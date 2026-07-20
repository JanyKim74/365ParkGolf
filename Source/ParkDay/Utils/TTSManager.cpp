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

// OneCore 보이스 카테고리 (Desktop과 별도 레지스트리 위치).
// Win10 후기 빌드/Win11에서 언어팩과 함께 설치되는 보이스가 여기 등록되는 경우가 많음.
static const wchar_t* GOneCoreVoiceCategoryId = L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Speech_OneCore\\Voices";

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
	, CurrentVoiceName(TEXT("Default"))
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
// [보이스 열거 / 선택 구현]
// ─────────────────────────────────────────────────────────────────────────────

TArray<FTTSVoiceInfo> FTTSManager::GetAvailableVoices(bool bIncludeOneCore) const
{
	TArray<FTTSVoiceInfo> Result;

#if PLATFORM_WINDOWS
	// Desktop 카테고리는 항상 열거 (Win10/11 공통으로 존재)
	Result.Append(EnumerateVoiceCategory(false));

	if (bIncludeOneCore)
	{
		Result.Append(EnumerateVoiceCategory(true));
	}

	UE_LOG(LogTemp, Warning, TEXT("[TTS] 사용 가능한 보이스 %d개 열거됨 (OneCore 포함=%s)"),
		Result.Num(), bIncludeOneCore ? TEXT("true") : TEXT("false"));

	for (const FTTSVoiceInfo& Info : Result)
	{
		UE_LOG(LogTemp, Log, TEXT("[TTS]   - %s%s  (Id=%s)"),
			*Info.DisplayName,
			Info.bIsOneCore ? TEXT(" [OneCore]") : TEXT(" [Desktop]"),
			*Info.TokenId);
	}
#endif

	return Result;
}

bool FTTSManager::SetVoiceByTokenId(const FString& TokenId)
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

	if (TokenId.IsEmpty())
	{
		LastErrorMessage = TEXT("TokenId가 비어있습니다");
		return false;
	}

	// SpGetTokenFromId() 헬퍼(sphelper.h) 대신, 빈 토큰을 만들고
	// SetId()로 레지스트리 경로에 직접 바인딩한다. (sapi.h만으로 동작)
	ISpObjectToken* pToken = nullptr;
	HRESULT hr = CoCreateInstance(
		CLSID_SpObjectToken,
		nullptr,
		CLSCTX_ALL,
		IID_ISpObjectToken,
		(void**)&pToken
	);

	if (FAILED(hr) || !pToken)
	{
		HandleError(hr, TEXT("보이스 토큰 객체 생성"));
		return false;
	}

	hr = pToken->SetId(nullptr, *TokenId, false);
	if (FAILED(hr))
	{
		pToken->Release();
		HandleError(hr, TEXT("보이스 토큰 바인딩"));
		return false;
	}

	hr = Voice->SetVoice(pToken);
	pToken->Release();

	if (FAILED(hr))
	{
		HandleError(hr, TEXT("보이스 지정"));
		return false;
	}

	// 지정 후 속도/볼륨을 다시 적용 (SetVoice가 내부적으로 리셋하는 SAPI 구현이 있음)
	Voice->SetRate(CurrentRate);
	Voice->SetVolume(CurrentVolume);

	CurrentVoiceName = GetCurrentVoiceName();
	UE_LOG(LogTemp, Warning, TEXT("[TTS] 보이스 지정 완료: %s (Id=%s)"), *CurrentVoiceName, *TokenId);
	return true;

#else
	return false;
#endif
}

bool FTTSManager::SetVoiceByNameContains(const FString& NamePart, bool bIncludeOneCore)
{
	if (NamePart.IsEmpty())
	{
		LastErrorMessage = TEXT("검색할 이름이 비어있습니다");
		return false;
	}

	TArray<FTTSVoiceInfo> Voices = GetAvailableVoices(bIncludeOneCore);

	// OneCore 쪽이 대체로 더 자연스러운 보이스인 경우가 많으므로,
	// bIncludeOneCore가 true면 OneCore를 우선 검색하도록 정렬해서 찾는다.
	if (bIncludeOneCore)
	{
		Voices.Sort([](const FTTSVoiceInfo& A, const FTTSVoiceInfo& B)
			{
				return A.bIsOneCore && !B.bIsOneCore;
			});
	}

	for (const FTTSVoiceInfo& Info : Voices)
	{
		if (Info.DisplayName.Contains(NamePart))
		{
			return SetVoiceByTokenId(Info.TokenId);
		}
	}

	LastErrorMessage = FString::Printf(TEXT("'%s'가 포함된 보이스를 찾지 못했습니다"), *NamePart);
	UE_LOG(LogTemp, Warning, TEXT("[TTS] %s"), *LastErrorMessage);
	return false;
}

FString FTTSManager::GetCurrentVoiceName() const
{
#if PLATFORM_WINDOWS
	if (!bInitialized || !Voice)
	{
		return TEXT("Uninitialized");
	}

	ISpObjectToken* pToken = nullptr;
	HRESULT hr = Voice->GetVoice(&pToken);

	if (FAILED(hr) || !pToken)
	{
		return TEXT("Default");
	}

	FString Name = TEXT("Default");

	// SpGetDescription() 헬퍼(sphelper.h) 대신, 토큰의 "기본값"(이름 없는 값)을
	// 직접 읽는다. SAPI 토큰은 등록될 때 기본값에 사람이 읽는 이름을 저장한다.
	LPWSTR pDesc = nullptr;
	if (SUCCEEDED(pToken->GetStringValue(nullptr, &pDesc)) && pDesc)
	{
		Name = FString(pDesc);
		::CoTaskMemFree(pDesc);
	}

	pToken->Release();
	return Name;

#else
	return TEXT("N/A");
#endif
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

TArray<FTTSVoiceInfo> FTTSManager::EnumerateVoiceCategory(bool bIsOneCore) const
{
	TArray<FTTSVoiceInfo> Result;
	const wchar_t* CategoryId = bIsOneCore ? GOneCoreVoiceCategoryId : SPCAT_VOICES;

	// SpEnumTokens() 헬퍼(sphelper.h) 대신, 카테고리 객체를 직접 만들어
	// SetId()로 원하는 레지스트리 경로를 지정한 뒤 EnumTokens()를 호출한다.
	ISpObjectTokenCategory* pCategory = nullptr;
	HRESULT hr = CoCreateInstance(
		CLSID_SpObjectTokenCategory,
		nullptr,
		CLSCTX_ALL,
		IID_ISpObjectTokenCategory,
		(void**)&pCategory
	);

	if (FAILED(hr) || !pCategory)
	{
		UE_LOG(LogTemp, Log, TEXT("[TTS] %s 카테고리 객체 생성 실패 (0x%08X)"),
			bIsOneCore ? TEXT("OneCore") : TEXT("Desktop"), hr);
		return Result;
	}

	hr = pCategory->SetId(CategoryId, false);
	if (FAILED(hr))
	{
		// OneCore 카테고리는 OS/언어팩에 따라 레지스트리 자체가 없을 수 있음 — 정상 상황.
		UE_LOG(LogTemp, Log, TEXT("[TTS] %s 카테고리 없음 (0x%08X)"),
			bIsOneCore ? TEXT("OneCore") : TEXT("Desktop"), hr);
		pCategory->Release();
		return Result;
	}

	IEnumSpObjectTokens* pEnum = nullptr;
	hr = pCategory->EnumTokens(nullptr, nullptr, &pEnum);
	pCategory->Release();

	if (FAILED(hr) || !pEnum)
	{
		UE_LOG(LogTemp, Log, TEXT("[TTS] %s 카테고리 열거 실패 (0x%08X)"),
			bIsOneCore ? TEXT("OneCore") : TEXT("Desktop"), hr);
		return Result;
	}

	ISpObjectToken* pToken = nullptr;
	while (pEnum->Next(1, &pToken, nullptr) == S_OK)
	{
		FTTSVoiceInfo Info;
		Info.bIsOneCore = bIsOneCore;

		// 토큰의 "기본값"(이름 없는 값)에 사람이 읽는 표시 이름이 들어있다.
		LPWSTR pDesc = nullptr;
		if (SUCCEEDED(pToken->GetStringValue(nullptr, &pDesc)) && pDesc)
		{
			Info.DisplayName = FString(pDesc);
			::CoTaskMemFree(pDesc);
		}

		LPWSTR pId = nullptr;
		if (SUCCEEDED(pToken->GetId(&pId)) && pId)
		{
			Info.TokenId = FString(pId);
			::CoTaskMemFree(pId);
		}

		pToken->Release();
		pToken = nullptr;

		if (!Info.TokenId.IsEmpty())
		{
			Result.Add(Info);
		}
	}

	pEnum->Release();
	return Result;
}

#endif