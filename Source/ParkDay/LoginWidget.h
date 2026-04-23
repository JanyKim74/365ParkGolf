// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "AuthStructures.h"
#include "LoginWidget.generated.h"

/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * 로그인 UI 위젯
 *
 * 사용자 입력을 받고 인증 매니저와 통신하는 UI
 * ═══════════════════════════════════════════════════════════════════════════════
 */
UCLASS()
class PARKDAY_API ULoginWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ─────────────────────────────────────────────────────────────────────────────
	// [UI 컴포넌트]
	// ─────────────────────────────────────────────────────────────────────────────
// 수정 후 — 6개 모두 동일하게 BlueprintReadOnly 추가
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "UI")
		UEditableTextBox* UserIDTextBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "UI")
		UEditableTextBox* PasswordTextBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "UI")
		UButton* LoginButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "UI")
		UButton* GuestLoginButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "UI")
		UTextBlock* StatusTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "UI")
		UProgressBar* ProgressBar;

	// ─────────────────────────────────────────────────────────────────────────────
	// [콜백]
	// ─────────────────────────────────────────────────────────────────────────────

protected:
	UFUNCTION()
		void OnLoginButtonClicked();

	UFUNCTION()
		void OnGuestLoginButtonClicked();

	UFUNCTION()
		void OnLoginCompleted();

	UFUNCTION()
		void OnLoginFailed();

	UFUNCTION()
		void OnNetworkRequestStarted(const FString& RequestType);

	UFUNCTION()
		void OnNetworkRequestFailed(const FString& RequestType, int32 ErrorCode, const FString& ErrorMessage);

	// ─────────────────────────────────────────────────────────────────────────────
	// [헬퍼 함수]
	// ─────────────────────────────────────────────────────────────────────────────

	void SetUIState(bool bEnable);
	void ShowStatus(const FString& Message, bool bIsError = false);
	FString HashPassword(const FString& Password);
	FString GetUniqueDeviceID();

	// ─────────────────────────────────────────────────────────────────────────────
	// [내부 변수]
	// ─────────────────────────────────────────────────────────────────────────────

	class AAuthenticationManager* AuthenticationManager;
	bool bIsWaitingForResponse = false;
};
