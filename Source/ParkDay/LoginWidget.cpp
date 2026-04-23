// Fill out your copyright notice in the Description page of Project Settings.

#include "LoginWidget.h"
#include "AuthenticationManager.h"
#include "NetworkManager.h"          
#include "Kismet/GameplayStatics.h"
#include "Misc/SecureHash.h"

void ULoginWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// UI 컴포넌트 초기화
	if (LoginButton)
	{
		LoginButton->OnClicked.AddDynamic(this, &ULoginWidget::OnLoginButtonClicked);
	}

	if (GuestLoginButton)
	{
		GuestLoginButton->OnClicked.AddDynamic(this, &ULoginWidget::OnGuestLoginButtonClicked);
	}

	if (StatusTextBlock)
	{
		StatusTextBlock->SetText(FText::FromString(TEXT("로그인하세요")));
	}

	if (ProgressBar)
	{
		ProgressBar->SetPercent(0.0f);
		ProgressBar->SetVisibility(ESlateVisibility::Hidden);
	}

	// 인증 매니저 찾기
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAuthenticationManager::StaticClass(), FoundActors);

	if (FoundActors.Num() > 0)
	{
		AuthenticationManager = Cast<AAuthenticationManager>(FoundActors[0]);

		if (AuthenticationManager)
		{
			// 인증 매니저 콜백 바인드
			AuthenticationManager->OnLoginCompleted.AddDynamic(this, &ULoginWidget::OnLoginCompleted);
			AuthenticationManager->OnLoginFailed.AddDynamic(this, &ULoginWidget::OnLoginFailed);

			// 네트워크 매니저 콜백도 바인드
			ANetworkManager* NetMgr = AuthenticationManager->GetNetworkManager();
			if (NetMgr)
			{
				NetMgr->OnRequestStarted.AddDynamic(this, &ULoginWidget::OnNetworkRequestStarted);
				NetMgr->OnRequestFailed.AddDynamic(this, &ULoginWidget::OnNetworkRequestFailed);
			}

			UE_LOG(LogTemp, Log, TEXT("✅ LoginWidget에서 AuthenticationManager를 찾았습니다."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("❌ AuthenticationManager를 찾을 수 없습니다."));
		ShowStatus(TEXT("오류: AuthenticationManager를 찾을 수 없습니다."), true);
	}

	// 초기 상태
	SetUIState(true);
}

void ULoginWidget::NativeDestruct()
{
	// 콜백 언바인드
	if (AuthenticationManager)
	{
		AuthenticationManager->OnLoginCompleted.RemoveDynamic(this, &ULoginWidget::OnLoginCompleted);
		AuthenticationManager->OnLoginFailed.RemoveDynamic(this, &ULoginWidget::OnLoginFailed);

		ANetworkManager* NetMgr = AuthenticationManager->GetNetworkManager();
		if (NetMgr)
		{
			NetMgr->OnRequestStarted.RemoveDynamic(this, &ULoginWidget::OnNetworkRequestStarted);
			NetMgr->OnRequestFailed.RemoveDynamic(this, &ULoginWidget::OnNetworkRequestFailed);
		}
	}

	if (LoginButton)
	{
		LoginButton->OnClicked.RemoveDynamic(this, &ULoginWidget::OnLoginButtonClicked);
	}

	if (GuestLoginButton)
	{
		GuestLoginButton->OnClicked.RemoveDynamic(this, &ULoginWidget::OnGuestLoginButtonClicked);
	}

	Super::NativeDestruct();
}

// ─────────────────────────────────────────────────────────────────────────────
// [콜백]
// ─────────────────────────────────────────────────────────────────────────────

void ULoginWidget::OnLoginButtonClicked()
{
	if (!AuthenticationManager)
	{
		ShowStatus(TEXT("오류: AuthenticationManager를 찾을 수 없습니다."), true);
		return;
	}

	// 입력 값 검증
	if (!UserIDTextBox || UserIDTextBox->GetText().IsEmpty())
	{
		ShowStatus(TEXT("사용자ID를 입력하세요."), true);
		return;
	}

	if (!PasswordTextBox || PasswordTextBox->GetText().IsEmpty())
	{
		ShowStatus(TEXT("비밀번호를 입력하세요."), true);
		return;
	}

	// 입력 데이터 준비
	FString UserID = UserIDTextBox->GetText().ToString();
	FString Password = PasswordTextBox->GetText().ToString();
	FString HashedPassword = HashPassword(Password);
	FString DeviceID = GetUniqueDeviceID();

	UE_LOG(LogTemp, Log, TEXT("📤 로그인 요청 - UserID: %s"), *UserID);

	// UI 비활성화
	SetUIState(false);
	ShowStatus(TEXT("로그인 중..."));

	// 로그인 요청
	AuthenticationManager->Login(UserID, HashedPassword, DeviceID, false);
}

void ULoginWidget::OnGuestLoginButtonClicked()
{
	if (!AuthenticationManager)
	{
		ShowStatus(TEXT("오류: AuthenticationManager를 찾을 수 없습니다."), true);
		return;
	}

	FString GuestUserID = FString::Printf(TEXT("guest_%lld"), FDateTime::Now().ToUnixTimestamp());
	FString GuestPassword = TEXT("guest_pass");
	FString DeviceID = GetUniqueDeviceID();

	UE_LOG(LogTemp, Log, TEXT("📤 게스트 로그인 요청"));

	// UI 비활성화
	SetUIState(false);
	ShowStatus(TEXT("게스트 로그인 중..."));

	// 게스트 로그인 요청
	AuthenticationManager->Login(GuestUserID, GuestPassword, DeviceID, true);
}

void ULoginWidget::OnLoginCompleted()
{
	UE_LOG(LogTemp, Log, TEXT("✅ 로그인 완료"));

	FString UserName = AuthenticationManager->GetCurrentUserName();
	FString SuccessMessage = FString::Printf(TEXT("로그인 성공! %s님, 환영합니다."), *UserName);
	ShowStatus(SuccessMessage, false);

	// 게임 플레이 레벨로 이동
	UGameplayStatics::OpenLevel(GetWorld(), FName(TEXT("GameLevel")));
}

void ULoginWidget::OnLoginFailed()
{
	UE_LOG(LogTemp, Warning, TEXT("❌ 로그인 실패"));

	ShowStatus(TEXT("로그인 실패. 다시 시도해주세요."), true);

	// UI 활성화
	SetUIState(true);
}

void ULoginWidget::OnNetworkRequestStarted(const FString& RequestType)
{
	if (RequestType == TEXT("Login"))
	{
		UE_LOG(LogTemp, Log, TEXT("🌐 로그인 네트워크 요청 시작"));

		if (ProgressBar)
		{
			ProgressBar->SetVisibility(ESlateVisibility::Visible);
			ProgressBar->SetPercent(0.5f);
		}
	}
}

void ULoginWidget::OnNetworkRequestFailed(const FString& RequestType, int32 ErrorCode, const FString& ErrorMessage)
{
	if (RequestType == TEXT("Login"))
	{
		UE_LOG(LogTemp, Error, TEXT("❌ 네트워크 요청 실패: %s"), *ErrorMessage);

		FString ErrorMsg;
		if (ErrorCode == 401)
		{
			ErrorMsg = TEXT("사용자명 또는 비밀번호가 잘못되었습니다.");
		}
		else if (ErrorCode == 0)
		{
			ErrorMsg = TEXT("네트워크 연결을 확인하세요.");
		}
		else
		{
			ErrorMsg = FString::Printf(TEXT("오류 (%d): %s"), ErrorCode, *ErrorMessage);
		}

		ShowStatus(ErrorMsg, true);

		// UI 활성화
		SetUIState(true);

		if (ProgressBar)
		{
			ProgressBar->SetVisibility(ESlateVisibility::Hidden);
			ProgressBar->SetPercent(0.0f);
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// [헬퍼 함수]
// ─────────────────────────────────────────────────────────────────────────────

void ULoginWidget::SetUIState(bool bEnable)
{
	if (UserIDTextBox)
	{
		UserIDTextBox->SetIsReadOnly(!bEnable);
	}

	if (PasswordTextBox)
	{
		PasswordTextBox->SetIsReadOnly(!bEnable);
	}

	if (LoginButton)
	{
		LoginButton->SetIsEnabled(bEnable);
	}

	if (GuestLoginButton)
	{
		GuestLoginButton->SetIsEnabled(bEnable);
	}

	bIsWaitingForResponse = !bEnable;
}

void ULoginWidget::ShowStatus(const FString& Message, bool bIsError)
{
	if (StatusTextBlock)
	{
		StatusTextBlock->SetText(FText::FromString(Message));

		// 오류 메시지는 빨강, 일반 메시지는 흰색
		if (bIsError)
		{
			StatusTextBlock->SetColorAndOpacity(FLinearColor::Red);
		}
		else
		{
			StatusTextBlock->SetColorAndOpacity(FLinearColor::White);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("📍 상태: %s"), *Message);
}

FString ULoginWidget::HashPassword(const FString& Password)
{
	// 실제 구현에서는 SHA-256 또는 bcrypt 사용
	// 이 예시는 MD5를 사용 (프로덕션에서는 사용하지 말 것)

	FString HashedPassword = FMD5::HashAnsiString(*Password);
	UE_LOG(LogTemp, Log, TEXT("🔐 비밀번호 해시 완료"));

	return HashedPassword;
}

FString ULoginWidget::GetUniqueDeviceID()
{
	// 디바이스 고유 ID 생성
	FString DeviceID = FPlatformMisc::GetDeviceId();

	if (DeviceID.IsEmpty())
	{
		// 폴백: UUID 생성
		FGuid GUID = FGuid::NewGuid();
		DeviceID = GUID.ToString();
	}

	UE_LOG(LogTemp, Log, TEXT("🆔 디바이스 ID: %s"), *DeviceID);

	return DeviceID;
}
