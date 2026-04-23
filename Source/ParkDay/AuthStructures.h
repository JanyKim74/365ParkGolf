// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/UserDefinedStruct.h"
#include "AuthStructures.generated.h"

/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * 인증 시스템 데이터 구조
 * ═══════════════════════════════════════════════════════════════════════════════
 */

 // ─────────────────────────────────────────────────────────────────────────────
 // [요청 패킷]
 // ─────────────────────────────────────────────────────────────────────────────

 /**
  * @brief 로그인 요청 패킷
  *
  * 사용 예시:
  * {
  *   "UserID": "test@email.com",
  *   "Password": "hashed_password_hash",
  *   "DeviceID": "device_uuid_1234",
  *   "bIsGuest": false
  * }
  * 
  * 
  * 
  */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAuthEvent);


USTRUCT(BlueprintType)
struct FLoginRequest
{
	GENERATED_BODY()

		/// 사용자 ID (이메일 또는 닉네임)
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auth")
		FString UserID;

	/// 비밀번호 (클라이언트에서 해시된 형태로 전송)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auth")
		FString Password;

	/// 디바이스 식별자 (중복 로그인 방지용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auth")
		FString DeviceID;

	/// 게스트 로그인 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auth")
		bool bIsGuest = false;
};

/**
 * @brief 로그아웃 요청 패킷
 *
 * 사용 예시:
 * {
 *   "UserID": "test@email.com",
 *   "Token": "eyJhbGciOiJIUzI1NiIs..."
 * }
 */
USTRUCT(BlueprintType)
struct FLogoutRequest
{
	GENERATED_BODY()

		/// 사용자 ID
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auth")
		FString UserID;

	/// 인증 토큰 (로그아웃 처리용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auth")
		FString Token;
};

// ─────────────────────────────────────────────────────────────────────────────
// [응답 패킷]
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief 로그인 응답 패킷
 *
 * 성공 응답 예시:
 * {
 *   "bSuccess": true,
 *   "Token": "eyJhbGciOiJIUzI1NiIs...",
 *   "NickName": "PlayerNickname",
 *   "Level": 15,
 *   "ErrorMessage": ""
 * }
 *
 * 실패 응답 예시:
 * {
 *   "bSuccess": false,
 *   "Token": "",
 *   "NickName": "",
 *   "Level": 0,
 *   "ErrorMessage": "Invalid credentials"
 * }
 */
USTRUCT(BlueprintType)
struct FLoginResponse
{
	GENERATED_BODY()

		/// 로그인 성공 여부
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auth")
		bool bSuccess = false;

	/// 인증 토큰 (JWT 등, 이후 API 호출에 사용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auth")
		FString Token;

	/// 사용자 닉네임
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auth")
		FString NickName;

	/// 사용자 레벨
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auth")
		int32 Level = 0;

	/// 오류 메시지 (실패 시)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auth")
		FString ErrorMessage;
};

/**
 * @brief 사용자 정보 응답 패킷
 *
 * 사용 예시:
 * {
 *   "NickName": "PlayerNickname",
 *   "Level": 15,
 *   "Ranking": 42,
 *   "Point": 1500,
 *   "ImgUrl": "https://cdn.example.com/profiles/user_123.jpg"
 * }
 */
USTRUCT(BlueprintType)
struct FUserInfoResponse
{
	GENERATED_BODY()

		/// 사용자 닉네임
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auth")
		FString NickName;

	/// 사용자 레벨
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auth")
		int32 Level = 0;

	/// 랭킹
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auth")
		int32 Ranking = 0;

	/// 포인트
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auth")
		int32 Point = 0;

	/// 프로필 이미지 URL
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auth")
		FString ImgUrl;
};

/**
 * @brief 로그아웃 응답 (단순 성공/실패)
 */
USTRUCT(BlueprintType)
struct FLogoutResponse
{
	GENERATED_BODY()

		/// 로그아웃 성공 여부
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auth")
		bool bSuccess = false;

	/// 오류 메시지
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auth")
		FString ErrorMessage;
};

// ─────────────────────────────────────────────────────────────────────────────
// [콜백 정의]
// ─────────────────────────────────────────────────────────────────────────────

/// 로그인 응답 콜백
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnLoginResponseReceived, bool, bSuccess, const FLoginResponse&, Response);

/// 로그아웃 응답 콜백
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnLogoutResponseReceived, bool, bSuccess, const FString&, ErrorMessage);

/// 사용자 정보 조회 응답 콜백
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnUserInfoResponseReceived, bool, bSuccess, const FUserInfoResponse&, UserInfo);

/// 네트워크 요청 시작 콜백
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNetworkRequestStarted, const FString&, RequestType);

/// 네트워크 요청 실패 콜백
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnNetworkRequestFailed, const FString&, RequestType, int32, ErrorCode, const FString&, ErrorMessage);
