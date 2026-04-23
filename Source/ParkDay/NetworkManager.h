// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Http.h"
#include "AuthStructures.h"
#include "NetworkManager.generated.h"

/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * 네트워크 매니저
 *
 * 서버와의 HTTP 통신을 담당하는 액터
 * - 로그인/로그아웃 요청 처리
 * - 사용자 정보 조회
 * - JSON 직렬화/역직렬화
 * - 인증 토큰 관리
 * ═══════════════════════════════════════════════════════════════════════════════
 */
UCLASS()
class PARKDAY_API ANetworkManager : public AActor
{
	GENERATED_BODY()

public:
	ANetworkManager();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// ─────────────────────────────────────────────────────────────────────────────
	// [공개 인터페이스]
	// ─────────────────────────────────────────────────────────────────────────────

	/**
	 * @brief 네트워크 매니저 초기화
	 *
	 * @param ServerURL 서버 URL (예: "http://127.0.0.1:8080")
	 * @param InAuthToken 기존 인증 토큰 (선택사항)
	 */
	UFUNCTION(BlueprintCallable, Category = "Network")
		void Initialize(const FString& ServerURL, const FString& InAuthToken = TEXT(""));

	/**
	 * @brief 로그인 요청 전송
	 *
	 * @param LoginRequest 로그인 요청 패킷
	 * @param OnResponse 응답 콜백
	 */
	UFUNCTION(BlueprintCallable, Category = "Network|Auth")
		void SendLoginRequest(const FLoginRequest& LoginRequest, FOnLoginResponseReceived OnResponse);

	/**
	 * @brief 로그아웃 요청 전송
	 *
	 * @param LogoutRequest 로그아웃 요청 패킷
	 * @param OnResponse 응답 콜백
	 */
	UFUNCTION(BlueprintCallable, Category = "Network|Auth")
		void SendLogoutRequest(const FLogoutRequest& LogoutRequest, FOnLogoutResponseReceived OnResponse);

	/**
	 * @brief 사용자 정보 조회
	 *
	 * @param Token 인증 토큰
	 * @param OnResponse 응답 콜백
	 */
	UFUNCTION(BlueprintCallable, Category = "Network|Auth")
		void GetUserInfo(const FString& Token, FOnUserInfoResponseReceived OnResponse);

	/**
	 * @brief 현재 인증 토큰 반환
	 */
	UFUNCTION(BlueprintPure, Category = "Network|Auth")
		FString GetAuthToken() const { return AuthToken; }

	/**
	 * @brief 현재 인증 토큰 설정
	 */
	UFUNCTION(BlueprintCallable, Category = "Network|Auth")
		void SetAuthToken(const FString& InToken) { AuthToken = InToken; }

	/**
	 * @brief 로그인 상태 확인
	 */
	UFUNCTION(BlueprintPure, Category = "Network|Auth")
		bool IsLoggedIn() const { return !AuthToken.IsEmpty(); }

	/**
	 * @brief 현재 서버 URL 반환
	 */
	UFUNCTION(BlueprintPure, Category = "Network")
		FString GetServerURL() const { return ServerURL; }

	/**
	 * @brief 요청 진행 중 상태
	 */
	UFUNCTION(BlueprintPure, Category = "Network")
		bool IsRequestInProgress() const { return bRequestInProgress; }

	// ─────────────────────────────────────────────────────────────────────────────
	// [콜백 위임자]
	// ─────────────────────────────────────────────────────────────────────────────

	/** 네트워크 요청 시작 */
	//UPROPERTY(BlueprintAssignable, Category = "Network|Events")
		FOnNetworkRequestStarted OnRequestStarted;

	/** 네트워크 요청 실패 */
	//UPROPERTY(BlueprintAssignable, Category = "Network|Events")
		FOnNetworkRequestFailed OnRequestFailed;

protected:
	// ─────────────────────────────────────────────────────────────────────────────
	// [내부 변수]
	// ─────────────────────────────────────────────────────────────────────────────

	/// 서버 URL (예: "http://127.0.0.1:8080")
	UPROPERTY(VisibleAnywhere, Category = "Network")
		FString ServerURL;

	/// 인증 토큰 (로그인 후 발급)
	UPROPERTY(VisibleAnywhere, Category = "Network|Auth")
		FString AuthToken;

	/// 요청 진행 중 플래그
	UPROPERTY(VisibleAnywhere, Category = "Network")
		bool bRequestInProgress = false;

	/// HTTP 모듈 참조
	//TSharedPtr<class IHttpModule> HttpModule;
	FHttpModule* HttpModule = nullptr;

	// ─────────────────────────────────────────────────────────────────────────────
	// [콜백 저장소]
	// ─────────────────────────────────────────────────────────────────────────────

	/// 현재 진행 중인 요청 타입
	FString CurrentRequestType;

	/// 로그인 응답 콜백
	FOnLoginResponseReceived LoginResponseCallback;

	/// 로그아웃 응답 콜백
	FOnLogoutResponseReceived LogoutResponseCallback;

	/// 사용자 정보 응답 콜백
	FOnUserInfoResponseReceived UserInfoResponseCallback;

	// ─────────────────────────────────────────────────────────────────────────────
	// [내부 함수]
	// ─────────────────────────────────────────────────────────────────────────────

	/**
	 * @brief HTTP POST 요청 생성 및 전송
	 *
	 * @param Endpoint API 엔드포인트 (예: "/api/auth/login")
	 * @param JsonPayload JSON 페이로드
	 * @param bRequiresAuth 인증 토큰 필요 여부
	 */
	void SendHttpRequest(const FString& Endpoint, const FString& JsonPayload, bool bRequiresAuth = false);

	/**
	 * @brief HTTP 응답 핸들러 (로그인)
	 */
	void OnLoginResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	/**
	 * @brief HTTP 응답 핸들러 (로그아웃)
	 */
	void OnLogoutResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	/**
	 * @brief HTTP 응답 핸들러 (사용자 정보)
	 */
	void OnUserInfoResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	/**
	 * @brief FLoginRequest를 JSON 문자열로 변환
	 */
	FString LoginRequestToJson(const FLoginRequest& Request) const;

	/**
	 * @brief FLogoutRequest를 JSON 문자열로 변환
	 */
	FString LogoutRequestToJson(const FLogoutRequest& Request) const;

	/**
	 * @brief JSON 문자열을 FLoginResponse로 파싱
	 */
	bool ParseLoginResponse(const FString& JsonString, FLoginResponse& OutResponse) const;

	/**
	 * @brief JSON 문자열을 FUserInfoResponse로 파싱
	 */
	bool ParseUserInfoResponse(const FString& JsonString, FUserInfoResponse& OutUserInfo) const;

	/**
	 * @brief JSON 문자열을 FLogoutResponse로 파싱
	 */
	bool ParseLogoutResponse(const FString& JsonString, FLogoutResponse& OutResponse) const;

	/**
	 * @brief 오류 응답 처리
	 */
	void HandleRequestError(const FString& RequestType, int32 ErrorCode, const FString& ErrorMessage);

	/**
	 * @brief 로깅 헬퍼 함수
	 */
	void LogRequest(const FString& RequestType, const FString& Endpoint, const FString& Payload);
	void LogResponse(const FString& RequestType, const FString& ResponseBody, bool bSuccess);
	void LogError(const FString& RequestType, int32 ErrorCode, const FString& ErrorMessage);
};
