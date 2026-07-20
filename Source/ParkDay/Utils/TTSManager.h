// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

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

#pragma comment(lib, "sapi.lib")
#pragma comment(lib, "ole32.lib")

#endif

/**
 * @brief 열거된 SAPI 보이스 하나에 대한 정보
 *
 * TokenId를 보관해두면 나중에 SpGetTokenFromId()로 다시 토큰을 얻어
 * SetVoiceByTokenId()에 넘길 수 있습니다. (ISpObjectToken 포인터를
 * 그대로 배열에 들고 있는 건 참조카운트 관리가 번거로워 피합니다.)
 *
 * 주의: TTSManager.h는 UHT가 처리하는 .generated.h를 include하지 않는
 * 순수 C++ 헤더이므로, 여기서는 USTRUCT/UPROPERTY(리플렉션)를 쓰지 않습니다.
 * 블루프린트에 이 정보를 노출하고 싶다면, 이 struct를 그대로 감싸는
 * UBlueprintFunctionLibrary(별도의 UCLASS 헤더)를 만들어 그쪽에
 * USTRUCT를 선언하고 여기 값을 복사해서 넘기는 방식을 쓰세요.
 */
struct FTTSVoiceInfo
{
	// 사람이 보는 이름 (예: "Microsoft Heami Desktop - Korean")
	FString DisplayName;

	// SpGetTokenFromId에 그대로 넘길 수 있는 레지스트리 토큰 ID
	FString TokenId;

	// OneCore 카테고리에서 나온 보이스인지 (Desktop 카테고리면 false)
	bool bIsOneCore = false;
};

/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * 최종 안정화 TTSManager (COM 초기화 에러 해결)
 *
 * Windows SAPI 5 사용
 * ✅ COM 초기화 제거 (UE4가 이미 초기화)
 * ✅ Desktop / OneCore 보이스 카테고리 열거 + 명시적 선택 지원
 *    (Win10/11에서 기본 보이스가 다르게 잡히는 문제를 코드에서 고정 가능)
 * ═══════════════════════════════════════════════════════════════════════════════
 */
class FTTSManager
{
public:
	/**
	 * @brief 생성자
	 */
	FTTSManager();

	/**
	 * @brief 소멸자
	 */
	~FTTSManager();

	/**
	 * @brief 텍스트를 음성으로 변환하여 재생
	 */
	bool Speak(const FString& Text);

	/**
	 * @brief 음성 재생 중지
	 */
	bool Stop();

	/**
	 * @brief TTS 시스템이 초기화되었는지 확인
	 */
	bool IsInitialized() const;

	/**
	 * @brief 현재 재생 중인지 확인
	 */
	bool IsPlaying() const;

	/**
	 * @brief 음성 속도 설정 (-10 ~ 10)
	 */
	bool SetRate(int32 Rate);

	/**
	 * @brief 음성 볼륨 설정 (0 ~ 100)
	 */
	bool SetVolume(int32 Volume);

	/**
	 * @brief 현재 설정된 음성 속도 조회
	 */
	int32 GetRate() const;

	/**
	 * @brief 현재 설정된 음성 볼륨 조회
	 */
	int32 GetVolume() const;

	/**
	 * @brief 마지막 에러 메시지 조회
	 */
	FString GetLastError() const;

	/**
	 * @brief 마지막 HRESULT 에러 코드 조회
	 */
	long GetLastErrorCode() const;

	// ─────────────────────────────────────────────────────────────
	// [보이스 열거 / 선택]
	// ─────────────────────────────────────────────────────────────

	/**
	 * @brief 설치된 보이스 목록을 열거합니다.
	 * @param bIncludeOneCore true면 Desktop + OneCore 둘 다 열거,
	 *                        false면 Desktop 카테고리만 (기존 동작과 동일한 범위)
	 * @return 열거된 보이스 정보 배열 (비어있으면 실패 또는 설치된 보이스 없음)
	 */
	TArray<FTTSVoiceInfo> GetAvailableVoices(bool bIncludeOneCore = true) const;

	/**
	 * @brief TokenId로 특정 보이스를 지정합니다. (GetAvailableVoices() 결과의 TokenId 사용)
	 */
	bool SetVoiceByTokenId(const FString& TokenId);

	/**
	 * @brief DisplayName에 특정 문자열이 포함된 첫 번째 보이스를 찾아 지정합니다.
	 *        예: SetVoiceByNameContains(TEXT("Heami"))
	 */
	bool SetVoiceByNameContains(const FString& NamePart, bool bIncludeOneCore = true);

	/**
	 * @brief 현재 지정된 보이스의 표시 이름을 반환합니다. ("Default" = 아직 명시적으로 지정 안 함)
	 */
	FString GetCurrentVoiceName() const;

	FTTSManager(const FTTSManager&) = delete;
	FTTSManager& operator=(const FTTSManager&) = delete;

protected:
	bool bInitialized;
	long LastErrorCode;
	FString LastErrorMessage;
	int32 CurrentRate;
	int32 CurrentVolume;
	FString CurrentVoiceName;

#if PLATFORM_WINDOWS
	struct ISpVoice* Voice;

	bool InitializeVoice();
	void HandleError(long hr, const FString& ErrorContext);
	FString HRESULTToString(long hr) const;

	// bIsOneCore == true 면 OneCore 카테고리("HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Speech_OneCore\Voices"),
	// false면 Desktop 카테고리(SPCAT_VOICES)를 연다.
	TArray<FTTSVoiceInfo> EnumerateVoiceCategory(bool bIsOneCore) const;

#else
	void* Voice = nullptr;
#endif
};