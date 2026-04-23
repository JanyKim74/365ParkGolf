// TerraParkgameInstance.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/DataTable.h"
#include "ParkDay/Utils/TouchDoubleTriggerFilter.h"
#include "TerraParkgameInstance.generated.h"

class ULoadingWidget;
class USoundClass;
class USoundMix;
class USoundConcurrency;
class USoundManager;
class UFadeWidget;

/**
 * Project-wide GameInstance (UE4.26)
 * - OnStart()에서 SoundManager 정책/테이블을 자동 세팅
 * - Blueprint에서도 수동으로 Setup 가능
 */
UCLASS(BlueprintType)
class PARKDAY_API UTerraParkgameInstance : public UGameInstance
{
    GENERATED_BODY()

public:

    UTerraParkgameInstance();
    // ---- GameInstance lifecycle ----
    virtual void Init() override;
    virtual void OnStart() override;     // World가 준비된 뒤 호출
    virtual void Shutdown() override;

    UPROPERTY()
    int32 StartHoleNum = 0;

    UFUNCTION(BlueprintCallable)
    void StartLoadingScreen();
    // ---- Audio/SoundManager hookup ----
    /** BP에서 수동 설정(원하면 BeginPlay 등에서 호출) */
    UFUNCTION(BlueprintCallable, Category = "Audio|Setup")
        void SetupAudioPolicy();

    /** 편의 헬퍼: BP에서 SoundManager 가져오기 */
    UFUNCTION(BlueprintPure, Category = "Audio", meta = (DisplayName = "Get SoundManager"))
        USoundManager* GetSoundManagerBP() const;


public:
    // 에디터에서 세팅(Soft로 들고 와서 런타임에 로드)
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

    /** 시작 시 자동 세팅할지 여부 */
    UPROPERTY(EditDefaultsOnly, Category = "Audio|Setup")
        bool bSetupAudioOnStart = true;

public:
    /** 자동 로딩 화면으로 띄울 UMG */
    UPROPERTY(EditDefaultsOnly, Category = "LoadingScreen")
        TSubclassOf<UUserWidget> LoadingScreenWidgetClass;

    /** 최소 표시 시간(초) */
    UPROPERTY(EditDefaultsOnly, Category = "LoadingScreen")
        float MinimumDisplayTime = 5.f;

    /** 맵 로드가 끝나면 자동 종료할지 */
    UPROPERTY(EditDefaultsOnly, Category = "LoadingScreen")
        bool bAutoCompleteWhenLoadingCompletes = false;

    /** 수동으로 StopLoadingScreen 호출할 때까지 유지할지 */
    UPROPERTY(EditDefaultsOnly, Category = "LoadingScreen")
        bool bPlayUntilStopped = true;

    UFUNCTION(BlueprintCallable, Category = "LoadingScreen")
        void StopLoadingScreen();

    void SetupMoviePlayerWithWidget(UUserWidget* Widget);
    TWeakObjectPtr<ULoadingWidget> ActiveLoadingWidget;
    /** OpenLevel 직전, 엔진이 자동으로 호출해주는 훅 */
    UFUNCTION()
        void HandlePrepareLoadingScreen();

public:
    UPROPERTY(BlueprintReadOnly, Category = "Fade")
    UFadeWidget* FadeWidget;

protected:


    UFUNCTION()
        void OnPostLoadMap(UWorld* LoadedWorld);

private:

    bool CanUseMoviePlayer() const;
    TSharedPtr<FTouchDoubleTriggerFilter> TouchFilter;


private:
    // 내부 재시도(Subsystem 생성 타이밍 보호)
    void SetupAudioPolicy_Internal();
    void LogSoundSetup(const TCHAR* Where, bool bOk) const;

    int32 SetupRetryCount = 0;
    static const int32 kMaxSetupRetry = 3;
};
