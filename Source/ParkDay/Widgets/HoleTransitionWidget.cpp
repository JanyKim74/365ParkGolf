#include "HoleTransitionWidget.h"
//#include "Components/EditableTextBox.h"
#include "Components/EditableTextBox.h"
#include "Animation/WidgetAnimation.h"
#include "../SoundManager.h"


// 사운드 에셋 경로
static const TCHAR* SoundPath_Voice = TEXT("/Game/365_widget/transition_widget/sound/test_voice.test_voice");
static const TCHAR* SoundPath_Par3 = TEXT("/Game/365_widget/transition_widget/sound/par3.par3");
static const TCHAR* SoundPath_Par4 = TEXT("/Game/365_widget/transition_widget/sound/par4.par4");
static const TCHAR* SoundPath_Par5 = TEXT("/Game/365_widget/transition_widget/sound/par5.par5");


void UHoleTransitionWidget::NativeConstruct()
{
    Super::NativeConstruct();
    // 위젯이 처음 만들어질 때 숨김 상태로 시작
    SetVisibility(ESlateVisibility::Collapsed);

    // 위젯 생성 시점에 모든 사운드 미리 로드 (재생 딜레이 방지)
    VoiceSound = LoadObject<USoundBase>(nullptr, SoundPath_Voice);
    Par3Sound = LoadObject<USoundBase>(nullptr, SoundPath_Par3);
    Par4Sound = LoadObject<USoundBase>(nullptr, SoundPath_Par4);
    Par5Sound = LoadObject<USoundBase>(nullptr, SoundPath_Par5);

    UE_LOG(LogTemp, Log, TEXT("HoleTransitionWidget 사운드 로드: Voice=%s, Par3=%s, Par4=%s, Par5=%s"),
        VoiceSound ? TEXT("✅") : TEXT("❌"),
        Par3Sound ? TEXT("✅") : TEXT("❌"),
        Par4Sound ? TEXT("✅") : TEXT("❌"),
        Par5Sound ? TEXT("✅") : TEXT("❌"));
}


// ---------------------------------------------------------------------------
// SetHoleInfo
// ---------------------------------------------------------------------------
void UHoleTransitionWidget::SetHoleInfo(int32 HoleNumber, int32 Par)
{
    //if (txt_hole_number)
    //    txt_hole_number->SetText(FText::AsNumber(HoleNumber));

    //if (txt_par_number)
    //    txt_par_number->SetText(FText::AsNumber(Par));
    if (txt_hole_number)
        txt_hole_number->SetText(FText::AsNumber(HoleNumber));

    if (txt_par_number)
        txt_par_number->SetText(FText::AsNumber(Par));

    // Par 값에 따라 재생할 사운드 결정
    switch (Par)
    {
    case 3:  CurrentParSound = Par3Sound;  break;
    case 4:  CurrentParSound = Par4Sound;  break;
    case 5:  CurrentParSound = Par5Sound;  break;
    default:
        CurrentParSound = nullptr;
        UE_LOG(LogTemp, Warning, TEXT("⚠️ HoleTransition: Par%d 사운드 없음"), Par);
        break;
    }

    UE_LOG(LogTemp, Log, TEXT("🏌️ HoleInfo 설정 — Hole:%d, Par:%d, 사운드:%s"),
        HoleNumber, Par, CurrentParSound ? TEXT("있음") : TEXT("없음"));
}

// ---------------------------------------------------------------------------
// PlayTransitionAnim
// ---------------------------------------------------------------------------
void UHoleTransitionWidget::PlayTransitionAnim()
{
    SetVisibility(ESlateVisibility::Visible);

    if (USoundManager* SM = USoundManager::Get(this))
    {
        // ✅ 1) test_voice 재생
        if (VoiceSound)
        {
            SM->Play2D(VoiceSound);
            UE_LOG(LogTemp, Log, TEXT("🔊 test_voice 재생"));
        }

        // ✅ 2) Par별 사운드 재생
        if (CurrentParSound)
        {
            SM->Play2D(CurrentParSound);
            UE_LOG(LogTemp, Log, TEXT("🔊 Par 사운드 재생"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ SoundManager 없음 — 사운드 스킵"));
    }


    if (hole_transition)
    {
        FWidgetAnimationDynamicEvent AnimFinishedEvent;
        AnimFinishedEvent.BindDynamic(this, &UHoleTransitionWidget::OnAnimFinished);
        BindToAnimationFinished(hole_transition, AnimFinishedEvent);

        PlayAnimation(hole_transition, 0.0f, 1, EUMGSequencePlayMode::Forward, 0.8f);
        UE_LOG(LogTemp, Log, TEXT("▶️ hole_transition 애니메이션 재생"));
    }
    else
    {
        UE_LOG(LogTemp, Warning,
            TEXT("⚠️ hole_transition 없음 → %.1f초 폴백 타이머"), FallbackDuration);

        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(
                HideTimerHandle,
                this,
                &UHoleTransitionWidget::NotifyTransitionFinished,
                FallbackDuration,
                false
            );
        }
    }
}



void UHoleTransitionWidget::OnAnimFinished()
{
    UE_LOG(LogTemp, Log, TEXT("🎬 hole_transition 애니메이션 종료 → 2초 후 숨김"));

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            HideTimerHandle,
            this,
            &UHoleTransitionWidget::NotifyTransitionFinished,
            HideDelay,       // 2초
            false
        );
    }
}

// ---------------------------------------------------------------------------
// NotifyTransitionFinished  (BP 또는 타이머에서 호출)
// ---------------------------------------------------------------------------
void UHoleTransitionWidget::NotifyTransitionFinished()
{
    // 위젯 숨기기
    SetVisibility(ESlateVisibility::Collapsed);

    // InGameMode 에게 알림
    OnTransitionFinished.Broadcast();

    UE_LOG(LogTemp, Log, TEXT("✅ HoleTransitionWidget: 전환 완료 → OnTransitionFinished Broadcast"));
}