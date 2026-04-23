// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AuthStructures.h"
#include "AuthenticationManager.generated.h"

/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * 인증 매니저
 *
 * 게임 플레이 레벨에서 로그인/로그아웃 등의 인증 플로우를 관리
 * 네트워크 매니저를 래핑하여 게임 로직에 맞게 처리
 * ═══════════════════════════════════════════════════════════════════════════════
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAuthSimpleEvent);

UCLASS()
class PARKDAY_API AAuthenticationManager : public AActor
{
	GENERATED_BODY()

public:
	AAuthenticationManager();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// ─────────────────────────────────────────────────────────────────────────────
	// [사용자 인터페이스]
	// ─────────────────────────────────────────────────────────────────────────────

	/**
	 * @brief 로그인 플로우 시작
	 *
	 * @param UserID 사용자 ID (이메일 또는 닉네임)
	 * @param Password 비밀번호 (클라이언트에서 해시된 형태)
	 * @param DeviceID 디바이스 ID
	 * @param bIsGuest 게스트 로그인 여부
	 */
	UFUNCTION(BlueprintCallable, Category = "Authentication")
		void Login(const FString& UserID, const FString& Password, const FString& DeviceID, bool bIsGuest = false);

	/**
	 * @brief 로그아웃 플로우 시작
	 */
	UFUNCTION(BlueprintCallable, Category = "Authentication")
		void Logout();

	/**
	 * @brief 현재 사용자 정보 조회
	 */
	UFUNCTION(BlueprintCallable, Category = "Authentication")
		void RefreshUserInfo();

	/**
	 * @brief 로그인 상태 반환
	 */
	UFUNCTION(BlueprintPure, Category = "Authentication")
		bool IsLoggedIn() const { return bIsLoggedIn; }

	/**
	 * @brief 현재 사용자 닉네임 반환
	 */
	UFUNCTION(BlueprintPure, Category = "Authentication")
		FString GetCurrentUserName() const { return CurrentUserName; }

	/**
	 * @brief 현재 사용자 레벨 반환
	 */
	UFUNCTION(BlueprintPure, Category = "Authentication")
		int32 GetCurrentUserLevel() const { return CurrentUserLevel; }

	/**
	 * @brief 네트워크 매니저 반환
	 */
	UFUNCTION(BlueprintPure, Category = "Authentication")
		class ANetworkManager* GetNetworkManager() const { return NetworkManager; }

	// ─────────────────────────────────────────────────────────────────────────────
	// [콜백 위임자]
	// ─────────────────────────────────────────────────────────────────────────────

/** 로그인 완료 */
	UPROPERTY(BlueprintAssignable, Category = "Authentication|Events")
		FOnAuthSimpleEvent OnLoginCompleted;

	/** 로그인 실패 */
	UPROPERTY(BlueprintAssignable, Category = "Authentication|Events")
		FOnAuthSimpleEvent OnLoginFailed;

	/** 로그아웃 완료 */
	UPROPERTY(BlueprintAssignable, Category = "Authentication|Events")
		FOnAuthSimpleEvent OnLogoutCompleted;

	/** 사용자 정보 업데이트 */
	UPROPERTY(BlueprintAssignable, Category = "Authentication|Events")
		FOnAuthSimpleEvent OnUserInfoUpdated;

protected:
	// ─────────────────────────────────────────────────────────────────────────────
	// [내부 변수]
	// ─────────────────────────────────────────────────────────────────────────────

	/// 네트워크 매니저 참조
	UPROPERTY(VisibleAnywhere, Category = "Authentication")
		class ANetworkManager* NetworkManager;

	/// 로그인 상태
	UPROPERTY(VisibleAnywhere, Category = "Authentication")
		bool bIsLoggedIn = false;

	/// 현재 사용자 이름
	UPROPERTY(VisibleAnywhere, Category = "Authentication")
		FString CurrentUserName;

	/// 현재 사용자 레벨
	UPROPERTY(VisibleAnywhere, Category = "Authentication")
		int32 CurrentUserLevel = 0;

	/// 현재 사용자 랭킹
	UPROPERTY(VisibleAnywhere, Category = "Authentication")
		int32 CurrentUserRanking = 0;

	/// 현재 사용자 포인트
	UPROPERTY(VisibleAnywhere, Category = "Authentication")
		int32 CurrentUserPoint = 0;

	/// 로그아웃 요청 시 사용할 UserID
	FString CurrentUserID;

	// ─────────────────────────────────────────────────────────────────────────────
	// [내부 함수 - 콜백]
	// ─────────────────────────────────────────────────────────────────────────────

	/**
	 * @brief 로그인 응답 처리
	 */
	UFUNCTION()
		void OnLoginResponseReceived(bool bSuccess, const FLoginResponse& Response);

	/**
	 * @brief 로그아웃 응답 처리
	 */
	UFUNCTION()
		void OnLogoutResponseReceived(bool bSuccess, const FString& ErrorMessage);

	/**
	 * @brief 사용자 정보 응답 처리
	 */
	UFUNCTION()
		void OnUserInfoResponseReceived(bool bSuccess, const FUserInfoResponse& UserInfo);

	/**
	 * @brief 네트워크 요청 시작 처리
	 */
	UFUNCTION()
		void OnNetworkRequestStarted(const FString& RequestType);

	/**
	 * @brief 네트워크 요청 실패 처리
	 */
	UFUNCTION()
		void OnNetworkRequestFailed(const FString& RequestType, int32 ErrorCode, const FString& ErrorMessage);
};
