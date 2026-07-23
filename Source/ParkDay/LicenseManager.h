#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "LicenseManager.generated.h"

// 검증 결과 열거형
UENUM(BlueprintType)
enum class ELicenseStatus : uint8
{
    Unknown         UMETA(DisplayName = "Unknown"),
    Valid           UMETA(DisplayName = "Valid"),
    Revoked         UMETA(DisplayName = "Revoked"),
    Expired         UMETA(DisplayName = "Expired"),
    NotFound        UMETA(DisplayName = "NotFound"),
    SignatureFail   UMETA(DisplayName = "SignatureFail"),
    OfflineCached   UMETA(DisplayName = "OfflineCached"),
    OfflineExpired  UMETA(DisplayName = "OfflineExpired"),
    NetworkError    UMETA(DisplayName = "NetworkError"),
};

// 검증 완료 델리게이트
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnLicenseResult, bool, bIsValid, ELicenseStatus, Status);

// 캐시 구조체
USTRUCT()
struct FLicenseCache
{
    GENERATED_BODY()

    UPROPERTY() bool    bValid       = false;
    UPROPERTY() FString Expiry;
    UPROPERTY() FString CustomerName;
    UPROPERTY() FString HwKey;
    UPROPERTY() FString Signature;   // 변조 방지 HMAC
    UPROPERTY() FString CachedAt;    // 마지막 온라인 검증 시각
};

UCLASS(Blueprintable)
class PARKDAY_API ULicenseManager : public UObject
{
    GENERATED_BODY()

public:
    // ── 초기화 (GameInstance::Init() 에서 호출) ──────────────
    UFUNCTION(BlueprintCallable, Category = "License")
    void Initialize();

    // ── 비동기 검증 시작 ─────────────────────────────────────
    UFUNCTION(BlueprintCallable, Category = "License")
    void ValidateAsync(FOnLicenseResult OnComplete);

    // ── 현재 상태 조회 ────────────────────────────────────────
    UFUNCTION(BlueprintPure, Category = "License")
    ELicenseStatus GetStatus() const { return CurrentStatus; }

    UFUNCTION(BlueprintPure, Category = "License")
    bool IsValid() const {
        return CurrentStatus == ELicenseStatus::Valid ||
               CurrentStatus == ELicenseStatus::OfflineCached;
    }

    UFUNCTION(BlueprintPure, Category = "License")
    FString GetCustomerName() const { return CustomerName; }

    UFUNCTION(BlueprintPure, Category = "License")
    FString GetExpiry() const { return Expiry; }

private:
    // ── 설정값 (LicenseConfig.ini 에서 로드) ─────────────────
    FString WebAppUrl;
    FString HmacSecret;
    int32   OfflineDays = 7;

    // ── 상태 ─────────────────────────────────────────────────
    ELicenseStatus CurrentStatus = ELicenseStatus::Unknown;
    FString        CustomerName;
    FString        Expiry;
    FString        HwKey;

    // ── 캐시 경로 ─────────────────────────────────────────────
    FString CachePath;

    // ── 내부 메서드 ───────────────────────────────────────────
    void        LoadConfig();
    FString     CollectHwKey();
    FString     GetCpuId();
    FString     GetMacAddress();
    FString     GetVolumeSerial();
    FString     ComputeSHA256(const FString& Input);
    FString     ComputeHmac(const FString& Message, const FString& Secret);

    void        OnHttpResponse(FHttpRequestPtr Request, FHttpResponsePtr Response,
                               bool bSuccess, FOnLicenseResult OnComplete);

    ELicenseStatus ValidateFromCache();
    void           SaveCache(const FString& InExpiry, const FString& InCustomerName);
    bool           LoadCache(FLicenseCache& OutCache);
    void           DeleteCache();
};
