// Fill out your copyright notice in the Description page of Project Settings.

#include "AuthenticationManager.h"
#include "NetworkManager.h"
#include "Kismet/GameplayStatics.h"

AAuthenticationManager::AAuthenticationManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AAuthenticationManager::BeginPlay()
{
	Super::BeginPlay();

	// 월드에서 네트워크 매니저 찾기
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ANetworkManager::StaticClass(), FoundActors);

	if (FoundActors.Num() > 0)
	{
		NetworkManager = Cast<ANetworkManager>(FoundActors[0]);
		UE_LOG(LogTemp, Log, TEXT("✅ NetworkManager를 찾았습니다."));

		// 네트워크 이벤트 바인드
		if (NetworkManager)
		{
			NetworkManager->OnRequestStarted.AddDynamic(this, &AAuthenticationManager::OnNetworkRequestStarted);
			NetworkManager->OnRequestFailed.AddDynamic(this, &AAuthenticationManager::OnNetworkRequestFailed);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("❌ NetworkManager를 찾을 수 없습니다. 레벨에 NetworkManager를 배치하세요."));
	}
}

void AAuthenticationManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// 콜백 언바인드
	if (NetworkManager)
	{
		NetworkManager->OnRequestStarted.RemoveDynamic(this, &AAuthenticationManager::OnNetworkRequestStarted);
		NetworkManager->OnRequestFailed.RemoveDynamic(this, &AAuthenticationManager::OnNetworkRequestFailed);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// [사용자 인터페이스]
// ─────────────────────────────────────────────────────────────────────────────

void AAuthenticationManager::Login(const FString& UserID, const FString& Password, const FString& DeviceID, bool bIsGuest)
{
	if (!NetworkManager)
	{
		UE_LOG(LogTemp, Error, TEXT("❌ NetworkManager가 없습니다."));
		OnLoginFailed.Broadcast();
		return;
	}

	if (UserID.IsEmpty() || Password.IsEmpty() || DeviceID.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("❌ 로그인 정보가 불완전합니다."));
		OnLoginFailed.Broadcast();
		return;
	}

	// 로그인 요청 생성
	FLoginRequest LoginRequest;
	LoginRequest.UserID = UserID;
	LoginRequest.Password = Password;
	LoginRequest.DeviceID = DeviceID;
	LoginRequest.bIsGuest = bIsGuest;

	// 로그인 콜백 생성
	FOnLoginResponseReceived LoginCallback;
	LoginCallback.BindDynamic(this, &AAuthenticationManager::OnLoginResponseReceived);

	// 로그인 요청 전송
	CurrentUserID = UserID;
	NetworkManager->SendLoginRequest(LoginRequest, LoginCallback);

	UE_LOG(LogTemp, Log, TEXT("📤 로그인 요청 전송: %s"), *UserID);
}

void AAuthenticationManager::Logout()
{
	if (!NetworkManager)
	{
		UE_LOG(LogTemp, Error, TEXT("❌ NetworkManager가 없습니다."));
		OnLogoutCompleted.Broadcast();
		return;
	}

	if (!bIsLoggedIn || CurrentUserID.IsEmpty() || NetworkManager->GetAuthToken().IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("❌ 로그인 상태가 아닙니다."));
		OnLogoutCompleted.Broadcast();
		return;
	}

	// 로그아웃 요청 생성
	FLogoutRequest LogoutRequest;
	LogoutRequest.UserID = CurrentUserID;
	LogoutRequest.Token = NetworkManager->GetAuthToken();

	// 로그아웃 콜백 생성
	FOnLogoutResponseReceived LogoutCallback;
	LogoutCallback.BindDynamic(this, &AAuthenticationManager::OnLogoutResponseReceived);

	// 로그아웃 요청 전송
	NetworkManager->SendLogoutRequest(LogoutRequest, LogoutCallback);

	UE_LOG(LogTemp, Log, TEXT("📤 로그아웃 요청 전송"));
}

void AAuthenticationManager::RefreshUserInfo()
{
	if (!NetworkManager)
	{
		UE_LOG(LogTemp, Error, TEXT("❌ NetworkManager가 없습니다."));
		return;
	}

	FString AuthToken = NetworkManager->GetAuthToken();
	if (AuthToken.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("❌ 인증 토큰이 없습니다. 로그인하세요."));
		return;
	}

	// 사용자 정보 조회 콜백 생성
	FOnUserInfoResponseReceived UserInfoCallback;
	UserInfoCallback.BindDynamic(this, &AAuthenticationManager::OnUserInfoResponseReceived);

	// 사용자 정보 조회 요청 전송
	NetworkManager->GetUserInfo(AuthToken, UserInfoCallback);

	UE_LOG(LogTemp, Log, TEXT("📤 사용자 정보 조회 요청"));
}

// ─────────────────────────────────────────────────────────────────────────────
// [내부 함수 - 콜백]
// ─────────────────────────────────────────────────────────────────────────────

void AAuthenticationManager::OnLoginResponseReceived(bool bSuccess, const FLoginResponse& Response)
{
	if (bSuccess)
	{
		// 로그인 성공
		bIsLoggedIn = true;
		CurrentUserName = Response.NickName;
		CurrentUserLevel = Response.Level;

		UE_LOG(LogTemp, Log, TEXT("✅ 로그인 성공: %s (레벨 %d)"), *CurrentUserName, CurrentUserLevel);

		// 사용자 정보 자동으로 조회
		RefreshUserInfo();

		OnLoginCompleted.Broadcast();
	}
	else
	{
		// 로그인 실패
		bIsLoggedIn = false;
		CurrentUserName.Empty();
		CurrentUserLevel = 0;

		UE_LOG(LogTemp, Error, TEXT("❌ 로그인 실패: %s"), *Response.ErrorMessage);
		OnLoginFailed.Broadcast();
	}
}

void AAuthenticationManager::OnLogoutResponseReceived(bool bSuccess, const FString& ErrorMessage)
{
	if (bSuccess)
	{
		// 로그아웃 성공
		bIsLoggedIn = false;
		CurrentUserName.Empty();
		CurrentUserLevel = 0;
		CurrentUserRanking = 0;
		CurrentUserPoint = 0;
		CurrentUserID.Empty();

		UE_LOG(LogTemp, Log, TEXT("✅ 로그아웃 성공"));
		OnLogoutCompleted.Broadcast();
	}
	else
	{
		// 로그아웃 실패
		UE_LOG(LogTemp, Error, TEXT("❌ 로그아웃 실패: %s"), *ErrorMessage);
		OnLogoutCompleted.Broadcast();
	}
}

void AAuthenticationManager::OnUserInfoResponseReceived(bool bSuccess, const FUserInfoResponse& UserInfo)
{
	if (bSuccess)
	{
		// 사용자 정보 업데이트
		CurrentUserName = UserInfo.NickName;
		CurrentUserLevel = UserInfo.Level;
		CurrentUserRanking = UserInfo.Ranking;
		CurrentUserPoint = UserInfo.Point;

		UE_LOG(LogTemp, Log, TEXT("✅ 사용자 정보 업데이트: %s (레벨 %d, 랭킹 #%d, 포인트 %d)"),
			*CurrentUserName, CurrentUserLevel, CurrentUserRanking, CurrentUserPoint);

		OnUserInfoUpdated.Broadcast();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("❌ 사용자 정보 조회 실패"));
	}
}

void AAuthenticationManager::OnNetworkRequestStarted(const FString& RequestType)
{
	UE_LOG(LogTemp, Log, TEXT("🌐 네트워크 요청 시작: %s"), *RequestType);
}

void AAuthenticationManager::OnNetworkRequestFailed(const FString& RequestType, int32 ErrorCode, const FString& ErrorMessage)
{
	UE_LOG(LogTemp, Error, TEXT("❌ 네트워크 요청 실패 [%s]: Code=%d, Message=%s"),
		*RequestType, ErrorCode, *ErrorMessage);

	if (RequestType == TEXT("Login"))
	{
		OnLoginFailed.Broadcast();
	}
	else if (RequestType == TEXT("Logout"))
	{
		OnLogoutCompleted.Broadcast();
	}
}
