// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerWidget.generated.h"

class UImage;
class UTexture2D;
class AInGameMode;
class UGolfPlayerManager;

UCLASS()
class PARKDAY_API UTimerWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UFUNCTION(BlueprintCallable, Category = "Timer")
        void Start();

    UFUNCTION(BlueprintCallable, Category = "Timer")
        void Stop();

    UFUNCTION(BlueprintCallable, Category = "Timer")
        void Reset();

    // 실시간(일시정지 무시) 사용할지 선택. false면 게임 시간(일시정지에 멈춤).
    UFUNCTION(BlueprintCallable, Category = "Timer")
        void SetUseRealTime(bool bInUseRealTime) { bUseRealTime = bInUseRealTime; }

    UFUNCTION(BlueprintCallable, Category = "Timer")
        void SetInitialTotalSeconds(int32 InSeconds) { InitialTotalSeconds = FMath::Max(0, InSeconds); }

    UFUNCTION(BlueprintPure, Category = "Timer")
        FText GetFormattedTime() const;


protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timer", meta = (ClampMin = "0"))
        int32 InitialTotalSeconds = 60 * 60; // default: 60 minutes

    UPROPERTY(meta = (BindWidget))
        class UTextBlock* TextBlock_Time;

    UFUNCTION()
        void UpdatePerTick();

private:
	float TickAccumulatedSec = 0.0f;
	float TickIntervalSec = 0.25f;

    bool   bRunning = false;
    bool   bHasExpired = false;
    bool   bUseRealTime = false;   // true: GetRealTimeSeconds, false: GetTimeSeconds
    int32  LastLoggedSecond = -1;
    double StartStampSec = 0.0;     // 직전 Start 시각
    double AccumulatedSec = 0.0;     // 누적 초

    double Now() const;
    double GetElapsedSeconds() const;
    double GetRemainingSeconds() const;
    static FString FormatSeconds(int32 TotalSeconds);
    TWeakObjectPtr<AInGameMode> GM;

};
