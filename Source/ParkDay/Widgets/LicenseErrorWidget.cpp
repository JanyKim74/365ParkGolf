#include "LicenseErrorWidget.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "TimerManager.h"

void ULicenseErrorWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 버튼 클릭 이벤트 바인딩
    if (BtnConfirm)
    {
        BtnConfirm->OnClicked.AddDynamic(this, &ULicenseErrorWidget::OnConfirmClicked);
    }
}

// ── 에러 상태 → 위젯 표시 ────────────────────────────────────────
void ULicenseErrorWidget::ShowError(ELicenseStatus Status)
{
    // 텍스트 설정
    if (TxtTitle)
        TxtTitle->SetText(FText::FromString(GetStatusTitle(Status)));

    if (TxtMessage)
        TxtMessage->SetText(FText::FromString(GetStatusMessage(Status)));

    if (TxtDetail)
        TxtDetail->SetText(FText::FromString(GetStatusDetail(Status)));

    // 버튼 텍스트 초기화
    UpdateCountdown();

    // 1초마다 카운트다운 갱신
    GetWorld()->GetTimerManager().SetTimer(
        AutoCloseTimer,
        this,
        &ULicenseErrorWidget::UpdateCountdown,
        1.0f,
        true   // 반복
    );
}

// ── 카운트다운 갱신 ───────────────────────────────────────────────
void ULicenseErrorWidget::UpdateCountdown()
{
    if (BtnConfirm)
    {
        // 버튼에 남은 시간 표시
        FString BtnText = FString::Printf(TEXT("확인 후 종료 (%d초)"), CountdownSec);

        // TextBlock이 버튼 안에 있다면 GetChildAt 으로 접근
        // 또는 버튼 위에 텍스트 오버레이로 처리
        if (UTextBlock* BtnLabel = Cast<UTextBlock>(BtnConfirm->GetChildAt(0)))
        {
            BtnLabel->SetText(FText::FromString(BtnText));
        }
    }

    CountdownSec--;

    if (CountdownSec < 0)
    {
        // 카운트다운 종료 → 자동 종료
        GetWorld()->GetTimerManager().ClearTimer(AutoCloseTimer);
        OnConfirmClicked();
    }
}

// ── 확인 버튼 클릭 ────────────────────────────────────────────────
void ULicenseErrorWidget::OnConfirmClicked()
{
    GetWorld()->GetTimerManager().ClearTimer(AutoCloseTimer);
    UKismetSystemLibrary::QuitGame(GetWorld(), nullptr,
        EQuitPreference::Quit, false);
  
}

// ================================================================
//  에러 상태별 텍스트
// ================================================================

FString ULicenseErrorWidget::GetStatusTitle(ELicenseStatus Status)
{
    switch (Status)
    {
    case ELicenseStatus::Revoked:       return TEXT("라이선스 취소됨");
    case ELicenseStatus::Expired:       return TEXT("라이선스 만료");
    case ELicenseStatus::NotFound:      return TEXT("라이선스 없음");
    case ELicenseStatus::SignatureFail: return TEXT("라이선스 손상");
    case ELicenseStatus::OfflineExpired:return TEXT("오프라인 기간 초과");
    default:                            return TEXT("인증 서버 연결 실패");
    }
}

FString ULicenseErrorWidget::GetStatusMessage(ELicenseStatus Status)
{
    switch (Status)
    {
    case ELicenseStatus::Revoked:
        return TEXT("이 PC의 라이선스가 관리자에 의해 취소되었습니다.");
    case ELicenseStatus::Expired:
        return TEXT("라이선스 유효기간이 만료되었습니다.");
    case ELicenseStatus::NotFound:
        return TEXT("이 PC에 등록된 라이선스를 찾을 수 없습니다.");
    case ELicenseStatus::SignatureFail:
        return TEXT("라이선스 파일이 손상되었거나 변조되었습니다.");
    case ELicenseStatus::OfflineExpired:
        return TEXT("오프라인 상태가 허용 기간을 초과했습니다.");
    default:
        return TEXT("라이선스 인증 서버에 연결할 수 없습니다.");
    }
}

FString ULicenseErrorWidget::GetStatusDetail(ELicenseStatus Status)
{
    switch (Status)
    {
    case ELicenseStatus::Revoked:
        return TEXT("관리자에게 문의하여 라이선스를 재발급 받으세요.");
    case ELicenseStatus::Expired:
        return TEXT("관리자에게 라이선스 갱신을 요청하세요.");
    case ELicenseStatus::NotFound:
        return TEXT("ClientApp을 실행하여 라이선스 등록 요청을 진행하세요.");
    case ELicenseStatus::SignatureFail:
        return TEXT("소프트웨어를 재설치하거나 관리자에게 문의하세요.");
    case ELicenseStatus::OfflineExpired:
        return TEXT("인터넷에 연결한 후 프로그램을 다시 실행하세요.");
    default:
        return TEXT("인터넷 연결을 확인한 후 다시 시도하세요.");
    }
}