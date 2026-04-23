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
 * ═══════════════════════════════════════════════════════════════════════════════
 * 최종 안정화 TTSManager (COM 초기화 에러 해결)
 *
 * Windows SAPI 5 사용
 * ✅ COM 초기화 제거 (UE4가 이미 초기화)
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

	FTTSManager(const FTTSManager&) = delete;
	FTTSManager& operator=(const FTTSManager&) = delete;

protected:
	bool bInitialized;
	long LastErrorCode;
	FString LastErrorMessage;
	int32 CurrentRate;
	int32 CurrentVolume;

#if PLATFORM_WINDOWS
	struct ISpVoice* Voice;

	bool InitializeVoice();
	void HandleError(long hr, const FString& ErrorContext);
	FString HRESULTToString(long hr) const;

#else
	void* Voice = nullptr;
#endif
};