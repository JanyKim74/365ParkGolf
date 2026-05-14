#include "HoleTransitionWidget.h"
//#include "Components/EditableTextBox.h"
#include "Components/EditableTextBox.h"
#include "Animation/WidgetAnimation.h"
#include "../SoundManager.h"


// 사운드 웨이브 에셋 경로
static const TCHAR* TransitionSoundPath =
TEXT("/Game/365_widget/transition_widget/sound/test_voice.test_voice");

void UHoleTransitionWidget::NativeConstruct()
{
    Super::NativeConstruct();
    // 위젯이 처음 만들어질 때 숨김 상태로 시작
    SetVisibility(ESlateVisibility::Collapsed);

    // NativeConstruct 시점에 사운드 미리 로드 (재생 시 딜레이 방지)
    TransitionSound = LoadObject<USoundBase>(nullptr, TransitionSoundPath);
    if (!TransitionSound)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("⚠️ HoleTransitionWidget: 사운드 로드 실패 → %s"), TransitionSoundPath);
    }
    else
    {
        UE_LOG(LogTemp, Log,
            TEXT("✅ HoleTransitionWidget: 사운드 로드 성공 → %s"), TransitionSoundPath);
    }
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

    // hole / par 레이블은 고정 텍스트이므로 보통 건드릴 필요 없지만
    // 언어 변경 등이 필요하면 아래 주석을 해제해 사용하세요.
    // if (hole) hole->SetText(FText::FromString(TEXT("HOLE")));
    // if (par)  par->SetText(FText::FromString(TEXT("PAR")));
}

// ---------------------------------------------------------------------------
// PlayTransitionAnim
// ---------------------------------------------------------------------------
void UHoleTransitionWidget::PlayTransitionAnim()
{
    SetVisibility(ESlateVisibility::Visible);

    // ✅ 사운드 재생
    if (TransitionSound)
    {
        if (USoundManager* SM = USoundManager::Get(this))
        {
            SM->Play2D(TransitionSound);
            UE_LOG(LogTemp, Log, TEXT("🔊 HoleTransition 사운드 재생"));
        }
    }
    else
    {
        // 혹시 NativeConstruct 때 로드 실패했으면 재시도
        TransitionSound = LoadObject<USoundBase>(nullptr, TransitionSoundPath);
        if (TransitionSound)
        {
            if (USoundManager* SM = USoundManager::Get(this))
                SM->Play2D(TransitionSound);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("⚠️ HoleTransition: 사운드 없음 → 스킵"));
        }
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