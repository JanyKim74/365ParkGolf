#include "LicenseManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformMisc.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"
#include "Misc/SecureHash.h"

// ── Windows WMI: COM 헤더 순서가 핵심 ────────────────────────────
// AllowWindowsPlatformAtomics 가 InterlockedIncrement 등을 정의함
// 반드시 AllowWindowsPlatformTypes → AllowWindowsPlatformAtomics 순서
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/AllowWindowsPlatformAtomics.h"
#include <Windows.h>        // DWORD, GetVolumeInformationW
#include <comdef.h>         // _bstr_t, _variant_t
#include <Wbemidl.h>        // IWbemLocator, IWbemServices
#include <bcrypt.h>           // CNG SHA-256
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "iphlpapi.lib")
#include "Windows/HideWindowsPlatformAtomics.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif

// iphlpapi: WMI 블록 밖에서 include (AF_UNSPEC, PIP_ADAPTER_ADDRESSES 정의)
#if PLATFORM_WINDOWS
#include <winsock2.h>   // AF_UNSPEC
#include <ws2tcpip.h>
#include <iphlpapi.h>   // GetAdaptersAddresses, PIP_ADAPTER_ADDRESSES
#endif

// ================================================================
//  초기화
// ================================================================

void ULicenseManager::Initialize()
{
    CachePath = FPaths::ProjectSavedDir() / TEXT("license.cache");
    LoadConfig();
    HwKey = CollectHwKey();
    UE_LOG(LogTemp, Log, TEXT("[License] Initialize 완료. HwKey 앞 16자: %s"), *HwKey.Left(16));
}

void ULicenseManager::LoadConfig()
{
    // FConfigFile 은 // 를 주석으로 처리해 URL 이 잘림 → 줄 단위 직접 파싱
    TArray<FString> Candidates;
    Candidates.Add(FPaths::ProjectContentDir() / TEXT("LicenseConfig.ini"));
    Candidates.Add(FPlatformProcess::GetCurrentWorkingDirectory() / TEXT("LicenseConfig.ini"));
    Candidates.Add(FPlatformProcess::GetCurrentWorkingDirectory() / TEXT("../../../ParkDay/Content/LicenseConfig.ini"));
    Candidates.Add(FPaths::ProjectSavedDir() / TEXT("LicenseConfig.ini"));
    Candidates.Add(FPaths::ProjectDir() / TEXT("LicenseConfig.ini"));

    FString FoundPath;
    for (const FString& P : Candidates)
    {
        FString Abs = FPaths::ConvertRelativePathToFull(P);
        if (FPaths::FileExists(Abs)) { FoundPath = Abs; break; }
    }

    if (!FoundPath.IsEmpty())
    {
        TArray<FString> Lines;
        FFileHelper::LoadFileToStringArray(Lines, *FoundPath);
        bool bIn = false;
        for (FString Line : Lines)
        {
            Line.TrimStartAndEndInline();
            if (Line.IsEmpty() || Line.StartsWith(TEXT(";"))) continue;
            if (Line == TEXT("[License]")) { bIn = true; continue; }
            if (Line.StartsWith(TEXT("["))) { bIn = false; continue; }
            if (!bIn) continue;
            FString K, V;
            if (Line.Split(TEXT("="), &K, &V))
            {
                K.TrimStartAndEndInline(); V.TrimStartAndEndInline();
                if (K == TEXT("WebAppUrl"))   WebAppUrl = V;
                else if (K == TEXT("HmacSecret"))  HmacSecret = V;
                else if (K == TEXT("OfflineDays")) LexFromString(OfflineDays, *V);
            }
        }
    }
    else if (GConfig)
    {
        GConfig->GetString(TEXT("License"), TEXT("WebAppUrl"), WebAppUrl, GGameIni);
        GConfig->GetString(TEXT("License"), TEXT("HmacSecret"), HmacSecret, GGameIni);
        GConfig->GetInt(TEXT("License"), TEXT("OfflineDays"), OfflineDays, GGameIni);
        WebAppUrl.TrimStartAndEndInline();
        HmacSecret.TrimStartAndEndInline();
    }

    UE_LOG(LogTemp, Log, TEXT("[License] WebAppUrl: [%s]"), *WebAppUrl);
    if (WebAppUrl.IsEmpty())
        UE_LOG(LogTemp, Error, TEXT("[License] WebAppUrl 비어 있음!"));
}

// ================================================================
//  비동기 검증
// ================================================================

void ULicenseManager::ValidateAsync(FOnLicenseResult OnComplete)
{
    if (WebAppUrl.IsEmpty() || HwKey.IsEmpty())
    {
        CurrentStatus = ELicenseStatus::NetworkError;
        OnComplete.ExecuteIfBound(false, CurrentStatus);
        return;
    }

    // HwKey 는 SHA 해시(hex 문자열)라 특수문자 없음 — 그냥 붙여도 안전
    FString Url = FString::Printf(TEXT("%s?action=verify&hwKey=%s"),
        *WebAppUrl, *HwKey);

    UE_LOG(LogTemp, Log, TEXT("[License] 검증 요청 URL: %s"), *Url);

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
        FHttpModule::Get().CreateRequest();

    Request->SetURL(Url);
    Request->SetVerb(TEXT("GET"));
    Request->SetTimeout(15.0f);
    Request->OnProcessRequestComplete().BindUObject(
        this, &ULicenseManager::OnHttpResponse, OnComplete);

    if (!Request->ProcessRequest())
    {
        UE_LOG(LogTemp, Warning, TEXT("[License] HTTP 요청 실패 → 캐시 fallback"));
        CurrentStatus = ValidateFromCache();
        OnComplete.ExecuteIfBound(IsValid(), CurrentStatus);
    }
}

// ── HTTP 응답 처리 ───────────────────────────────────────────────
void ULicenseManager::OnHttpResponse(
    FHttpRequestPtr Request,
    FHttpResponsePtr Response,
    bool bSuccess,
    FOnLicenseResult OnComplete)
{
    if (!bSuccess || !Response.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[License] 네트워크 오류 → 캐시 fallback"));
        CurrentStatus = ValidateFromCache();
        OnComplete.ExecuteIfBound(IsValid(), CurrentStatus);
        return;
    }

    FString Body = Response->GetContentAsString();
    UE_LOG(LogTemp, Log, TEXT("[License] 서버 응답: %s"), *Body);

    TSharedPtr<FJsonObject> Json;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
    if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("[License] JSON 파싱 실패"));
        CurrentStatus = ValidateFromCache();
        OnComplete.ExecuteIfBound(IsValid(), CurrentStatus);
        return;
    }

    bool bOk = Json->GetBoolField(TEXT("ok"));
    bool bValid = Json->GetBoolField(TEXT("valid"));

    if (!bOk)
    {
        UE_LOG(LogTemp, Error, TEXT("[License] 서버 오류: %s"),
            *Json->GetStringField(TEXT("error")));
        CurrentStatus = ELicenseStatus::NetworkError;
        OnComplete.ExecuteIfBound(false, CurrentStatus);
        return;
    }

    if (bValid)
    {
        Expiry = Json->GetStringField(TEXT("expiry"));
        CustomerName = Json->GetStringField(TEXT("customerName"));
        SaveCache(Expiry, CustomerName);
        CurrentStatus = ELicenseStatus::Valid;
        UE_LOG(LogTemp, Log, TEXT("[License] 인증 성공 — %s / 만료: %s"),
            *CustomerName, *Expiry);
    }
    else
    {
        FString Reason = Json->GetStringField(TEXT("reason"));
        DeleteCache();

        if (Reason == TEXT("revoked"))            CurrentStatus = ELicenseStatus::Revoked;
        else if (Reason == TEXT("expired"))            CurrentStatus = ELicenseStatus::Expired;
        else if (Reason == TEXT("not_found"))          CurrentStatus = ELicenseStatus::NotFound;
        else if (Reason == TEXT("signature_mismatch")) CurrentStatus = ELicenseStatus::SignatureFail;
        else                                           CurrentStatus = ELicenseStatus::NetworkError;

        UE_LOG(LogTemp, Warning, TEXT("[License] 인증 실패: %s"), *Reason);
    }

    OnComplete.ExecuteIfBound(IsValid(), CurrentStatus);
}

// ================================================================
//  오프라인 캐시
// ================================================================

ELicenseStatus ULicenseManager::ValidateFromCache()
{
    FLicenseCache Cache;
    if (!LoadCache(Cache))
        return ELicenseStatus::NetworkError;

    if (Cache.HwKey != HwKey)
        return ELicenseStatus::NotFound;

    FString ExpectedSig = ComputeHmac(Cache.HwKey + TEXT("|") + Cache.Expiry, HmacSecret);
    if (Cache.Signature != ExpectedSig)
    {
        DeleteCache();
        return ELicenseStatus::SignatureFail;
    }

    FDateTime CachedTime;
    if (!FDateTime::Parse(Cache.CachedAt, CachedTime))
        return ELicenseStatus::OfflineExpired;

    if ((FDateTime::Now() - CachedTime).GetDays() > OfflineDays)
        return ELicenseStatus::OfflineExpired;

    FDateTime ExpiryDate;
    if (FDateTime::ParseIso8601(*Cache.Expiry, ExpiryDate) && ExpiryDate < FDateTime::Now())
        return ELicenseStatus::Expired;

    Expiry = Cache.Expiry;
    CustomerName = Cache.CustomerName;
    return ELicenseStatus::OfflineCached;
}

void ULicenseManager::SaveCache(const FString& InExpiry, const FString& InCustomerName)
{
    FLicenseCache Cache;
    Cache.bValid = true;
    Cache.Expiry = InExpiry;
    Cache.CustomerName = InCustomerName;
    Cache.HwKey = HwKey;
    Cache.Signature = ComputeHmac(HwKey + TEXT("|") + InExpiry, HmacSecret);
    Cache.CachedAt = FDateTime::Now().ToString();

    FString Json;
    FJsonObjectConverter::UStructToJsonObjectString(Cache, Json);

    // 간단 XOR 난독화
    TArray<uint8> Bytes;
    FTCHARToUTF8 Conv(*Json);
    Bytes.Append((uint8*)Conv.Get(), Conv.Length());
    for (uint8& B : Bytes) B ^= 0x5A;

    FFileHelper::SaveArrayToFile(Bytes, *CachePath);
}

bool ULicenseManager::LoadCache(FLicenseCache& OutCache)
{
    if (!FPaths::FileExists(CachePath)) return false;

    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *CachePath)) return false;

    for (uint8& B : Bytes) B ^= 0x5A;
    FString Json = FString(UTF8_TO_TCHAR((const char*)Bytes.GetData()));
    return FJsonObjectConverter::JsonObjectStringToUStruct(Json, &OutCache, 0, 0);
}

void ULicenseManager::DeleteCache()
{
    if (FPaths::FileExists(CachePath))
        IFileManager::Get().Delete(*CachePath);
}

// ================================================================
//  HW 키 수집
// ================================================================

FString ULicenseManager::CollectHwKey()
{
    FString CpuId = GetCpuId();
    FString Mac = GetMacAddress();
    FString VolSer = GetVolumeSerial();

    UE_LOG(LogTemp, Log, TEXT("[HwKey] CPU: [%s]"), *CpuId);
    UE_LOG(LogTemp, Log, TEXT("[HwKey] MAC: [%s]"), *Mac);
    UE_LOG(LogTemp, Log, TEXT("[HwKey] Vol: [%s]"), *VolSer);

    if (CpuId.IsEmpty())  CpuId = TEXT("UNKNOWN_CPU");
    if (Mac.IsEmpty())    Mac = TEXT("UNKNOWN_MAC");
    if (VolSer.IsEmpty()) VolSer = TEXT("UNKNOWN_VOL");

    FString Raw = CpuId + TEXT("|") + Mac + TEXT("|") + VolSer;
    UE_LOG(LogTemp, Log, TEXT("[HwKey] Raw: [%s]"), *Raw);
    return ComputeSHA256(Raw);
}

// ── CPU ID: __cpuid 직접 호출 (WMI 권한 문제 우회) ─────────────
FString ULicenseManager::GetCpuId()
{
#if PLATFORM_WINDOWS
    int CpuInfo[4] = {};
    __cpuid(CpuInfo, 1);
    // EDX(feature flags)|EAX(version) → WMI ProcessorId 와 동일 포맷
    return FString::Printf(TEXT("%08X%08X"),
        (uint32)CpuInfo[3],
        (uint32)CpuInfo[0]);
#else
    return FPlatformMisc::GetDeviceId();
#endif
}

// ── MAC 주소: GetAdaptersAddresses + 알파벳 최솟값 선택 ──────────
// 기준: 이더넷/무선 타입 + 6바이트 + 00:00:00 아님 + 알파벳 최솟값
// → 열거 순서에 무관하게 항상 동일한 MAC 선택 (C# 과 동일 기준)
FString ULicenseManager::GetMacAddress()
{
#if PLATFORM_WINDOWS
    ULONG BufLen = 0;
    GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX,
        nullptr, nullptr, &BufLen);
    if (BufLen == 0) return TEXT("");

    TArray<uint8> Buf;
    Buf.SetNumUninitialized((int32)BufLen);
    auto* pAddrs = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(Buf.GetData());

    TArray<FString> Candidates;

    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX,
        nullptr, pAddrs, &BufLen) == ERROR_SUCCESS)
    {
        for (auto* p = pAddrs; p; p = p->Next)
        {
            // 이더넷(6) 또는 무선(71) + 6바이트 + 00:00:00 아님
            if ((p->IfType == IF_TYPE_ETHERNET_CSMACD ||
                p->IfType == IF_TYPE_IEEE80211) &&
                p->PhysicalAddressLength == 6)
            {
                uint8* A = p->PhysicalAddress;
                // 모두 0인 가상 어댑터 제외
                if (A[0] == 0 && A[1] == 0 && A[2] == 0 &&
                    A[3] == 0 && A[4] == 0 && A[5] == 0) continue;

                FString Mac = FString::Printf(TEXT("%02X%02X%02X%02X%02X%02X"),
                    (uint32)A[0], (uint32)A[1], (uint32)A[2],
                    (uint32)A[3], (uint32)A[4], (uint32)A[5]);
                Candidates.Add(Mac);
            }
        }
    }

    if (Candidates.Num() == 0) return TEXT("");

    // 알파벳 최솟값 선택 → C# Candidates.Sort(StringComparer.Ordinal)[0] 과 동일
    Candidates.Sort();
    return Candidates[0];
#else
    return FPlatformProcess::ComputerName();
#endif
}

// ── C드라이브 볼륨 시리얼 ────────────────────────────────────────
FString ULicenseManager::GetVolumeSerial()
{
#if PLATFORM_WINDOWS
    DWORD Serial = 0;
    if (GetVolumeInformationW(L"C:\\", nullptr, 0, &Serial,
        nullptr, nullptr, nullptr, 0))
        return FString::Printf(TEXT("%08X"), Serial);
#endif
    return TEXT("");
}

// ================================================================
//  암호화
// ================================================================

FString ULicenseManager::ComputeSHA256(const FString& Input)
{
    // CNG SHA-256 — C# SHA256.HashData().ToLower() 와 동일 (64자 소문자 hex)
#if PLATFORM_WINDOWS
    const char* DataPtr = TCHAR_TO_UTF8(*Input);
    ULONG DataLen = (ULONG)FCStringAnsi::Strlen(DataPtr);
    uint8 Hash[32] = {};

    BCRYPT_ALG_HANDLE  hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    DWORD cbHash = 0, cbResult = 0;

    if (BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0)))
    {
        if (BCRYPT_SUCCESS(BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH,
            (PUCHAR)&cbHash, sizeof(DWORD), &cbResult, 0))
            && cbHash == 32
            && BCRYPT_SUCCESS(BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0)))
        {
            BCryptHashData(hHash, (PUCHAR)DataPtr, DataLen, 0);
            BCryptFinishHash(hHash, Hash, 32, 0);
            BCryptDestroyHash(hHash);
        }
        BCryptCloseAlgorithmProvider(hAlg, 0);
    }

    FString Result;
    for (int32 i = 0; i < 32; i++)
        Result += FString::Printf(TEXT("%02x"), Hash[i]);
    return Result;
#else
    FSHAHash ShaHash;
    FSHA1::HashBuffer(TCHAR_TO_UTF8(*Input),
        FCStringAnsi::Strlen(TCHAR_TO_UTF8(*Input)), ShaHash.Hash);
    FString Result;
    for (int32 i = 0; i < 20; i++)
        Result += FString::Printf(TEXT("%02x"), ShaHash.Hash[i]);
    return Result;
#endif
}

FString ULicenseManager::ComputeHmac(const FString& Message, const FString& Secret)
{
    // UE5 내장 HMAC-SHA1 — Apps Script SHA256 과 서명 방식 다름
    // 오프라인 캐시 무결성 전용으로만 사용 (온라인 검증은 서버에서 처리)
    const char* MsgPtr = TCHAR_TO_UTF8(*Message);
    const char* KeyPtr = TCHAR_TO_UTF8(*Secret);
    int32 MsgLen = FCStringAnsi::Strlen(MsgPtr);
    int32 KeyLen = FCStringAnsi::Strlen(KeyPtr);

    FSHAHash Hash;
    FSHA1::HMACBuffer((uint8*)KeyPtr, KeyLen, (uint8*)MsgPtr, MsgLen, Hash.Hash);

    FString Result;
    for (int32 i = 0; i < 20; i++)
        Result += FString::Printf(TEXT("%02x"), Hash.Hash[i]);
    return Result;
}