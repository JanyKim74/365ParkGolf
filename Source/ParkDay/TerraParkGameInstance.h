// TerraParkgameInstance.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/DataTable.h"
#include "ParkDay/Utils/TouchDoubleTriggerFilter.h"
#include "LicenseManager.h"
#include "Widgets/LicenseErrorWidget.h"          // ← 추가
#include "TerraParkgameInstance.generated.h"

class ULoadingWidget;
class USoundClass;
class USoundMix;
class USoundConcurrency;
class USoundManager;
class UFadeWidget;

UCLASS(BlueprintType)
class PARKDAY_API UTerraParkgameInstance : public UGameInstance
{
    GENERATED_BODY()

public:

    UTerraParkgameInstance();

    virtual void Init() override;
    virtual void OnStart() override;
    virtual void Shutdown() override;

    UPROPERTY()
    int32 StartHoleNum = 0;

    UFUNCTION(BlueprintCallable)
    void StartLoadingScreen();

    UFUNCTION(BlueprintCallable, Category = "Audio|Setup")
    void SetupAudioPolicy();

    UFUNCTION(BlueprintPure, Category = "Audio", meta = (DisplayName = "Get SoundManager"))
    USoundManager* GetSoundManagerBP() const;

public:
    UPROPERTY(EditDefaultsOnly, Category = "Audio|Policy")
    TSoftObjectPtr<USoundClass> BGMClass;

    UPROPERTY(EditDefaultsOnly, Category = "Audio|Policy")
    TSoftObjectPtr<USoundClass> VoiceClass;

    UPROPERTY(EditDefaultsOnly, Category = "Audio|Policy")
    TSoftObjectPtr<USoundMix> DuckMix;

    UPROPERTY(EditDefaultsOnly, Category = "Audio|Policy")
    TSoftObjectPtr<USoundConcurrency> VoiceConcurrency;

    UPROPERTY(EditDefaultsOnly, Category = "Audio|Data")
    TSoftObjectPtr<UDataTable> SoundTable;

    UPROPERTY(EditDefaultsOnly, Category = "Audio|Setup")
    bool bSetupAudioOnStart = true;

public:
    UPROPERTY(EditDefaultsOnly, Category = "LoadingScreen")
    TSubclassOf<UUserWidget> LoadingScreenWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = "LoadingScreen")
    float MinimumDisplayTime = 5.f;

    UPROPERTY(EditDefaultsOnly, Category = "LoadingScreen")
    bool bAutoCompleteWhenLoadingCompletes = false;

    UPROPERTY(EditDefaultsOnly, Category = "LoadingScreen")
    bool bPlayUntilStopped = true;

    UFUNCTION(BlueprintCallable, Category = "LoadingScreen")
    void StopLoadingScreen();

    void SetupMoviePlayerWithWidget(UUserWidget* Widget);
    TWeakObjectPtr<ULoadingWidget> ActiveLoadingWidget;

    UFUNCTION()
    void HandlePrepareLoadingScreen();

public:
    UPROPERTY(BlueprintReadOnly, Category = "Fade")
    UFadeWidget* FadeWidget;

    // ── 라이선스 에러 위젯 클래스 ────────────────────────────
    // 에디터 디테일 패널에서 WBP_LicenseError 를 선택
    UPROPERTY(EditDefaultsOnly, Category = "License",
        meta = (DisplayName = "라이선스 에러 위젯 클래스"))
    TSubclassOf<ULicenseErrorWidget> LicenseErrorWidgetClass;

protected:
    UFUNCTION()
    void OnPostLoadMap(UWorld* LoadedWorld);

private:
    bool CanUseMoviePlayer() const;
    TSharedPtr<FTouchDoubleTriggerFilter> TouchFilter;
    TSubclassOf<UFadeWidget> FadeWidgetClass_Ref;

private:
    void SetupAudioPolicy_Internal();
    void LogSoundSetup(const TCHAR* Where, bool bOk) const;

    int32 SetupRetryCount = 0;
    static const int32 kMaxSetupRetry = 3;

private:
    // ── 라이선스 ──────────────────────────────────────────────
    UPROPERTY()
    ULicenseManager* LicenseManager = nullptr;

    UPROPERTY()
    ULicenseErrorWidget* LicenseErrorWidget = nullptr;   // ← 추가

    UFUNCTION()
    void OnLicenseResult(bool bIsValid, ELicenseStatus Status);

    void ShowLicenseError(ELicenseStatus Status);
};