#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "ParkDay/LicenseManager.h"
#include "LicenseErrorWidget.generated.h"

// ================================================================
//  ULicenseErrorWidget
//  라이선스 인증 실패 시 표시되는 팝업 위젯
//
//  Blueprint 설정:
//    1. Content Browser에서 Widget Blueprint 생성
//    2. 부모 클래스를 LicenseErrorWidget 으로 설정
//    3. TxtTitle, TxtMessage, TxtDetail, BtnConfirm 을 디자인
// ================================================================
UCLASS()
class PARKDAY_API ULicenseErrorWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // ── 메시지 설정 후 화면에 표시 ────────────────────────────
    void ShowError(ELicenseStatus Status);

protected:
    virtual void NativeConstruct() override;

    // ── 버튼 클릭 → 게임 종료 ────────────────────────────────
    UFUNCTION()
    void OnConfirmClicked();

private:
    // ── Blueprint에서 바인딩할 UI 요소 ───────────────────────
    // 이름이 Blueprint 위젯 이름과 반드시 일치해야 함
    UPROPERTY(meta = (BindWidget))
    UTextBlock* TxtTitle = nullptr;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* TxtMessage = nullptr;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* TxtDetail = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UButton* BtnConfirm = nullptr;

    // ── 자동 종료 타이머 ──────────────────────────────────────
    FTimerHandle AutoCloseTimer;
    int32        CountdownSec = 10;

    void UpdateCountdown();
    FString GetStatusTitle(ELicenseStatus Status);
    FString GetStatusMessage(ELicenseStatus Status);
    FString GetStatusDetail(ELicenseStatus Status);
};