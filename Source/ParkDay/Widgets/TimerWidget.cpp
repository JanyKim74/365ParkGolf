#include "TimerWidget.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "ParkDay/InGameMode.h"
#include "ParkDay/Widgets/TimerEndPopupWidget.h"
#include "ParkDay/Utils/UtilLibrary.h"
#include "ParkDay/SoundManager.h"

void UTimerWidget::NativeConstruct()
{
    Super::NativeConstruct();
    GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());

    TickAccumulatedSec = 0.0f;
    UpdatePerTick();

    GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
}

void UTimerWidget::NativeDestruct()
{
    bRunning = false;
    bHasExpired = false;
    GM = nullptr;
    Super::NativeDestruct();
}

void UTimerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    TickAccumulatedSec += InDeltaTime;
    if (TickAccumulatedSec >= TickIntervalSec)
    {
        TickAccumulatedSec = FMath::Fmod(TickAccumulatedSec, TickIntervalSec);
        UpdatePerTick();
    }
}


double UTimerWidget::Now() const
{
    if (const UWorld* World = GetWorld())
    {
        return bUseRealTime ? World->GetRealTimeSeconds()
            : World->GetTimeSeconds();
    }
    return 0.0;
}

double UTimerWidget::GetElapsedSeconds() const
{
    double Total = AccumulatedSec;
    if (bRunning)
        Total += Now() - StartStampSec;
    return Total;
}

double UTimerWidget::GetRemainingSeconds() const
{
    const double TotalSeconds = FMath::Max(0, InitialTotalSeconds);
    const double Remaining = TotalSeconds - GetElapsedSeconds();
    return FMath::Max(0.0, Remaining);
}

void UTimerWidget::Start()
{
    if (bRunning) return;
    bHasExpired = false;
    LastLoggedSecond = -1;
    StartStampSec = Now();
    bRunning = true;
    UE_LOG(LogTemp, Log, TEXT("TimerWidget: Start (InitialTotalSeconds=%d, UseRealTime=%d)"), InitialTotalSeconds, bUseRealTime);
    UpdatePerTick();
}

void UTimerWidget::Stop()
{
    if (!bRunning) return;
    AccumulatedSec = FMath::Min(
        AccumulatedSec + (Now() - StartStampSec),
        static_cast<double>(FMath::Max(0, InitialTotalSeconds))
    );
    bRunning = false;
    UE_LOG(LogTemp, Log, TEXT("TimerWidget: Stop (AccumulatedSec=%.3f)"), AccumulatedSec);
    UpdatePerTick();
}

void UTimerWidget::Reset()
{
    AccumulatedSec = 0.0;
    bHasExpired = false;
    LastLoggedSecond = -1;
    if (bRunning)
        StartStampSec = Now();
    UE_LOG(LogTemp, Log, TEXT("TimerWidget: Reset (InitialTotalSeconds=%d)"), InitialTotalSeconds);
    UpdatePerTick();
}

FString UTimerWidget::FormatSeconds(int32 TotalSeconds)
{
    const int32 Hours = TotalSeconds / 3600;
    const int32 Minutes = (TotalSeconds / 60) % 60;
    const int32 Seconds = TotalSeconds % 60;
    return FString::Printf(TEXT("%02d:%02d:%02d"), Hours, Minutes, Seconds);
}

FText UTimerWidget::GetFormattedTime() const
{
    const double Remaining = GetRemainingSeconds();
    const int32 Rounded = FMath::Max(0, FMath::CeilToInt(Remaining - KINDA_SMALL_NUMBER));
    return FText::FromString(FormatSeconds(Rounded));
}

void UTimerWidget::UpdatePerTick()
{
    AInGameMode* GMPtr = GM.Get();
    if (!IsValid(GMPtr))
    {
        if (UWorld* World = GetWorld())
        {
            GMPtr = Cast<AInGameMode>(World->GetAuthGameMode());
            GM = GMPtr;
        }
    }

    const double Remaining = GetRemainingSeconds();
    const int32 RemainingInt = FMath::Max(0, FMath::CeilToInt(Remaining - KINDA_SMALL_NUMBER));
    if (RemainingInt != LastLoggedSecond)
    {
        LastLoggedSecond = RemainingInt;
        //if (const UWorld* World = GetWorld())
        //{
        //    UE_LOG(LogTemp, Log, TEXT("TimerWidget: Tick Remaining=%d Running=%d UseRealTime=%d Time=%.3f RealTime=%.3f"),
        //        RemainingInt, bRunning ? 1 : 0, bUseRealTime ? 1 : 0, World->GetTimeSeconds(), World->GetRealTimeSeconds());
        //}
        //else
        //{
        //    UE_LOG(LogTemp, Warning, TEXT("TimerWidget: Tick Remaining=%d (World null)"), RemainingInt);
        //}
    }

    if (bRunning && GetRemainingSeconds() <= 0.0)
    {
        AccumulatedSec = static_cast<double>(FMath::Max(0, InitialTotalSeconds));
        bRunning = false;
		if (!bHasExpired)
		{
            GM->TimerEndWidget->SetVisibility(ESlateVisibility::Visible);
            //GM->Speak(TEXT("연습 시간이 종료되었습니다. 이용해주셔서 감사합니다."));
            //GM->RangeHUDWidgetInstance->OnMenuButtonClicked();
			if (auto* SM = GetGameInstance()->GetSubsystem<USoundManager>())
			{
				SM->PlayTTS_Interrupt_ById(TEXT("Voice.EndGame"));
			}
           // GM->StoppingSensor();
            GM->RangeHUDWidgetInstance->OnMenuButtonClicked();

            //FTimerHandle TH;
            //GetWorld()->GetTimerManager().SetTimer(TH, [this]()
            //    {
            //        UUtilLibrary::FadeIn(GetWorld(), 1.f,
            //            FFadeCallback::CreateLambda([this]
            //                {
            //                    const FString Options = TEXT("?game=/Game/UMG/GM_UMG.GM_UMG_C?bFromInGame=true");
            //                    UGameplayStatics::OpenLevel(GetWorld(), "Level_UI", false, Options);
            //                }
            //        ));
            //    }
            //    ,3.f, false);
		}
    }

    if (TextBlock_Time)
    {
        TextBlock_Time->SetText(GetFormattedTime());
    }
}
