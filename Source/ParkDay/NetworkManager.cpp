// Fill out your copyright notice in the Description page of Project Settings.

#include "NetworkManager.h"
#include "Http.h"
#include "HttpModule.h"
#include "JsonUtilities.h"
#include "Json.h"
#include "Interfaces/IHttpResponse.h"
#include "Containers/List.h"

ANetworkManager::ANetworkManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;

	// 네트워크 요청이 필요 없으면 틱 비활성화
	PrimaryActorTick.bStartWithTickEnabled = false;

	// HTTP 모듈 캐시
	HttpModule = &FHttpModule::Get();
}

void ANetworkManager::BeginPlay()
{
	Super::BeginPlay();

	if (!HttpModule)
	{
		HttpModule = &FHttpModule::Get();
	}

	UE_LOG(LogTemp, Warning, TEXT("🌐 NetworkManager initialized"));
}

void ANetworkManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ANetworkManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	UE_LOG(LogTemp, Warning, TEXT("🌐 NetworkManager destroyed"));
}

void ANetworkManager::Initialize(const FString& InServerURL, const FString& InAuthToken)
{
	ServerURL = InServerURL;
	AuthToken = InAuthToken;

	if (AuthToken.IsEmpty())
	{
		UE_LOG(LogTemp, Log, TEXT("🌐 NetworkManager initialized with server: %s (not logged in)"), *ServerURL);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("🌐 NetworkManager initialized with server: %s (logged in)"), *ServerURL);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// [공개 인터페이스 - 요청 전송]
// ─────────────────────────────────────────────────────────────────────────────

void ANetworkManager::SendLoginRequest(const FLoginRequest& LoginRequest, FOnLoginResponseReceived OnResponse)
{
	if (bRequestInProgress)
	{
		OnResponse.ExecuteIfBound(false, FLoginResponse()); // 명시적 실패 통보
		UE_LOG(LogTemp, Warning, TEXT("❌ 요청이 진행 중입니다. 다시 시도해주세요."));
		return;
	}

	if (ServerURL.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("❌ 서버 URL이 설정되지 않았습니다."));
		OnResponse.ExecuteIfBound(false, FLoginResponse());
		return;
	}

	bRequestInProgress = true;
	CurrentRequestType = TEXT("Login");
	LoginResponseCallback = OnResponse;

	FString JsonPayload = LoginRequestToJson(LoginRequest);
	SendHttpRequest(TEXT("/api/auth/login"), JsonPayload, false);

	OnRequestStarted.Broadcast(TEXT("Login"));
	LogRequest(TEXT("Login"), TEXT("/api/auth/login"), JsonPayload);
}

void ANetworkManager::SendLogoutRequest(const FLogoutRequest& LogoutRequest, FOnLogoutResponseReceived OnResponse)
{
	if (bRequestInProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("❌ 요청이 진행 중입니다. 다시 시도해주세요."));
		return;
	}

	if (ServerURL.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("❌ 서버 URL이 설정되지 않았습니다."));
		OnResponse.ExecuteIfBound(false, TEXT("Server URL not set"));
		return;
	}

	bRequestInProgress = true;
	CurrentRequestType = TEXT("Logout");
	LogoutResponseCallback = OnResponse;

	FString JsonPayload = LogoutRequestToJson(LogoutRequest);
	SendHttpRequest(TEXT("/api/auth/logout"), JsonPayload, true);

	OnRequestStarted.Broadcast(TEXT("Logout"));
	LogRequest(TEXT("Logout"), TEXT("/api/auth/logout"), JsonPayload);
}

void ANetworkManager::GetUserInfo(const FString& Token, FOnUserInfoResponseReceived OnResponse)
{
	if (bRequestInProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("❌ 요청이 진행 중입니다. 다시 시도해주세요."));
		return;
	}

	if (ServerURL.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("❌ 서버 URL이 설정되지 않았습니다."));
		OnResponse.ExecuteIfBound(false, FUserInfoResponse());
		return;
	}

	if (Token.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("❌ 인증 토큰이 없습니다."));
		OnResponse.ExecuteIfBound(false, FUserInfoResponse());
		return;
	}

	bRequestInProgress = true;
	CurrentRequestType = TEXT("GetUserInfo");
	UserInfoResponseCallback = OnResponse;

	// 사용자 정보 조회는 POST 바디 없이 토큰만 헤더에 포함
	SendHttpRequest(TEXT("/api/user/info"), TEXT(""), true);

	OnRequestStarted.Broadcast(TEXT("GetUserInfo"));
	LogRequest(TEXT("GetUserInfo"), TEXT("/api/user/info"), TEXT("(Token-based request)"));
}

// ─────────────────────────────────────────────────────────────────────────────
// [내부 함수 - HTTP 통신]
// ─────────────────────────────────────────────────────────────────────────────

void ANetworkManager::SendHttpRequest(const FString& Endpoint, const FString& JsonPayload, bool bRequiresAuth)
{
	if (!HttpModule)
	{
		HandleRequestError(CurrentRequestType, 0, TEXT("HTTP 모듈을 초기화할 수 없습니다."));
		return;
	}

	// HTTP 요청 생성
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = HttpModule->CreateRequest();

	// 요청 URL 설정
	FString RequestURL = ServerURL + Endpoint;
	Request->SetURL(RequestURL);

	// HTTP 메서드 설정
	Request->SetVerb(TEXT("POST"));

	// 헤더 설정
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("User-Agent"), TEXT("UnrealEngine/4.26"));

	// 인증 토큰 헤더 추가
	if (bRequiresAuth && !AuthToken.IsEmpty())
	{
		FString AuthHeader = FString::Printf(TEXT("Bearer %s"), *AuthToken);
		Request->SetHeader(TEXT("Authorization"), *AuthHeader);
		UE_LOG(LogTemp, Log, TEXT("🔐 인증 토큰 헤더 추가: Bearer ***"));
	}

	// 페이로드 설정
	if (!JsonPayload.IsEmpty())
	{
		Request->SetContentAsString(JsonPayload);
	}

	// 응답 콜백 바인딩
	if (CurrentRequestType == TEXT("Login"))
	{
		Request->OnProcessRequestComplete().BindUObject(this, &ANetworkManager::OnLoginResponse);
	}
	else if (CurrentRequestType == TEXT("Logout"))
	{
		Request->OnProcessRequestComplete().BindUObject(this, &ANetworkManager::OnLogoutResponse);
	}
	else if (CurrentRequestType == TEXT("GetUserInfo"))
	{
		Request->OnProcessRequestComplete().BindUObject(this, &ANetworkManager::OnUserInfoResponse);
	}

	// 요청 전송
	if (Request->ProcessRequest())
	{
		UE_LOG(LogTemp, Log, TEXT("✅ HTTP 요청 전송: %s"), *RequestURL);
	}
	else
	{
		HandleRequestError(CurrentRequestType, 0, TEXT("HTTP 요청 전송 실패"));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// [응답 핸들러]
// ─────────────────────────────────────────────────────────────────────────────

void ANetworkManager::OnLoginResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	bRequestInProgress = false;

	if (!bWasSuccessful || !Response.IsValid())
	{
		int32 ErrorCode = Response.IsValid() ? Response->GetResponseCode() : 0;
		HandleRequestError(TEXT("Login"), ErrorCode, TEXT("네트워크 요청 실패"));
		LoginResponseCallback.ExecuteIfBound(false, FLoginResponse());
		return;
	}

	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseBody = Response->GetContentAsString();

	LogResponse(TEXT("Login"), ResponseBody, true);

	FLoginResponse LoginResponse;
	if (ParseLoginResponse(ResponseBody, LoginResponse))
	{
		if (LoginResponse.bSuccess)
		{
			// 로그인 성공: 토큰 저장
			AuthToken = LoginResponse.Token;
			UE_LOG(LogTemp, Log, TEXT("✅ 로그인 성공! 사용자: %s (레벨 %d)"), *LoginResponse.NickName, LoginResponse.Level);
		}
		else
		{
			// 로그인 실패
			UE_LOG(LogTemp, Warning, TEXT("❌ 로그인 실패: %s"), *LoginResponse.ErrorMessage);
		}

		LoginResponseCallback.ExecuteIfBound(LoginResponse.bSuccess, LoginResponse);
	}
	else
	{
		// JSON 파싱 실패
		HandleRequestError(TEXT("Login"), ResponseCode, TEXT("응답 JSON 파싱 실패"));
		LoginResponseCallback.ExecuteIfBound(false, FLoginResponse());
	}
}

void ANetworkManager::OnLogoutResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	bRequestInProgress = false;

	if (!bWasSuccessful || !Response.IsValid())
	{
		int32 ErrorCode = Response.IsValid() ? Response->GetResponseCode() : 0;
		HandleRequestError(TEXT("Logout"), ErrorCode, TEXT("네트워크 요청 실패"));
		LogoutResponseCallback.ExecuteIfBound(false, TEXT("Network request failed"));
		return;
	}

	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseBody = Response->GetContentAsString();

	LogResponse(TEXT("Logout"), ResponseBody, true);

	FLogoutResponse LogoutResponse;
	if (ParseLogoutResponse(ResponseBody, LogoutResponse))
	{
		if (LogoutResponse.bSuccess)
		{
			// 로그아웃 성공: 토큰 제거
			AuthToken.Empty();
			UE_LOG(LogTemp, Log, TEXT("✅ 로그아웃 성공!"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("❌ 로그아웃 실패: %s"), *LogoutResponse.ErrorMessage);
		}

		LogoutResponseCallback.ExecuteIfBound(LogoutResponse.bSuccess, LogoutResponse.ErrorMessage);
	}
	else
	{
		HandleRequestError(TEXT("Logout"), ResponseCode, TEXT("응답 JSON 파싱 실패"));
		LogoutResponseCallback.ExecuteIfBound(false, TEXT("Response parsing failed"));
	}
}

void ANetworkManager::OnUserInfoResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	bRequestInProgress = false;

	if (!bWasSuccessful || !Response.IsValid())
	{
		int32 ErrorCode = Response.IsValid() ? Response->GetResponseCode() : 0;
		HandleRequestError(TEXT("GetUserInfo"), ErrorCode, TEXT("네트워크 요청 실패"));
		UserInfoResponseCallback.ExecuteIfBound(false, FUserInfoResponse());
		return;
	}

	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseBody = Response->GetContentAsString();

	LogResponse(TEXT("GetUserInfo"), ResponseBody, true);

	FUserInfoResponse UserInfo;
	if (ParseUserInfoResponse(ResponseBody, UserInfo))
	{
		UE_LOG(LogTemp, Log, TEXT("✅ 사용자 정보 조회 성공! 사용자: %s (레벨 %d, 랭킹 #%d)"),
			*UserInfo.NickName, UserInfo.Level, UserInfo.Ranking);
		UserInfoResponseCallback.ExecuteIfBound(true, UserInfo);
	}
	else
	{
		HandleRequestError(TEXT("GetUserInfo"), ResponseCode, TEXT("응답 JSON 파싱 실패"));
		UserInfoResponseCallback.ExecuteIfBound(false, FUserInfoResponse());
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// [JSON 직렬화/역직렬화]
// ─────────────────────────────────────────────────────────────────────────────

FString ANetworkManager::LoginRequestToJson(const FLoginRequest& Request) const
{
	TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();

	JsonObject->SetStringField(TEXT("UserID"), Request.UserID);
	JsonObject->SetStringField(TEXT("Password"), Request.Password);
	JsonObject->SetStringField(TEXT("DeviceID"), Request.DeviceID);
	JsonObject->SetBoolField(TEXT("bIsGuest"), Request.bIsGuest);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

	return JsonString;
}

FString ANetworkManager::LogoutRequestToJson(const FLogoutRequest& Request) const
{
	TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();

	JsonObject->SetStringField(TEXT("UserID"), Request.UserID);
	JsonObject->SetStringField(TEXT("Token"), Request.Token);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

	return JsonString;
}

bool ANetworkManager::ParseLoginResponse(const FString& JsonString, FLoginResponse& OutResponse) const
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("❌ 로그인 응답 JSON 파싱 실패"));
		return false;
	}

	// 필수 필드 확인
	if (!JsonObject->HasField(TEXT("bSuccess")))
	{
		UE_LOG(LogTemp, Error, TEXT("❌ bSuccess 필드가 없습니다."));
		return false;
	}

	OutResponse.bSuccess = JsonObject->GetBoolField(TEXT("bSuccess"));
	OutResponse.Token = JsonObject->GetStringField(TEXT("Token"));
	OutResponse.NickName = JsonObject->GetStringField(TEXT("NickName"));
	OutResponse.Level = JsonObject->GetIntegerField(TEXT("Level"));
	OutResponse.ErrorMessage = JsonObject->GetStringField(TEXT("ErrorMessage"));

	return true;
}

bool ANetworkManager::ParseUserInfoResponse(const FString& JsonString, FUserInfoResponse& OutUserInfo) const
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("❌ 사용자 정보 응답 JSON 파싱 실패"));
		return false;
	}

	OutUserInfo.NickName = JsonObject->GetStringField(TEXT("NickName"));
	OutUserInfo.Level = JsonObject->GetIntegerField(TEXT("Level"));
	OutUserInfo.Ranking = JsonObject->GetIntegerField(TEXT("Ranking"));
	OutUserInfo.Point = JsonObject->GetIntegerField(TEXT("Point"));
	OutUserInfo.ImgUrl = JsonObject->GetStringField(TEXT("ImgUrl"));

	return true;
}

bool ANetworkManager::ParseLogoutResponse(const FString& JsonString, FLogoutResponse& OutResponse) const
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("❌ 로그아웃 응답 JSON 파싱 실패"));
		return false;
	}

	OutResponse.bSuccess = JsonObject->GetBoolField(TEXT("bSuccess"));
	OutResponse.ErrorMessage = JsonObject->GetStringField(TEXT("ErrorMessage"));

	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// [에러 처리 및 로깅]
// ─────────────────────────────────────────────────────────────────────────────

void ANetworkManager::HandleRequestError(const FString& RequestType, int32 ErrorCode, const FString& ErrorMessage)
{
	bRequestInProgress = false;
	LogError(RequestType, ErrorCode, ErrorMessage);
	OnRequestFailed.Broadcast(RequestType, ErrorCode, ErrorMessage);
}

void ANetworkManager::LogRequest(const FString& RequestType, const FString& Endpoint, const FString& Payload)
{
	FString LogMessage = FString::Printf(TEXT("📤 [%s 요청]\n   URL: %s%s\n   Payload: %s"),
		*RequestType, *ServerURL, *Endpoint, *Payload);
	UE_LOG(LogTemp, Log, TEXT("%s"), *LogMessage);
}

void ANetworkManager::LogResponse(const FString& RequestType, const FString& ResponseBody, bool bSuccess)
{
	if (bSuccess)
	{
		FString LogMessage = FString::Printf(TEXT("📥 [%s 응답 성공]\n   Body: %s"),
			*RequestType, *ResponseBody);
		UE_LOG(LogTemp, Log, TEXT("%s"), *LogMessage);
	}
	else
	{
		FString LogMessage = FString::Printf(TEXT("📥 [%s 응답 실패]\n   Body: %s"),
			*RequestType, *ResponseBody);
		UE_LOG(LogTemp, Warning, TEXT("%s"), *LogMessage);
	}
}

void ANetworkManager::LogError(const FString& RequestType, int32 ErrorCode, const FString& ErrorMessage)
{
	FString LogMessage = FString::Printf(TEXT("❌ [%s 오류]\n   Code: %d\n   Message: %s"),
		*RequestType, ErrorCode, *ErrorMessage);
	UE_LOG(LogTemp, Error, TEXT("%s"), *LogMessage);
}
