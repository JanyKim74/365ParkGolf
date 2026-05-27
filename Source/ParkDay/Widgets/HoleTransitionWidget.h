#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Sound/SoundBase.h"
#include "HoleTransitionWidget.generated.h"

//class UEditableTextBox;
class UEditableTextBox;
class UImage;
class UPanelWidget;

// ✅ DECLARE는 반드시 클래스 밖 (전역 스코프) 에 위치해야 합니다
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTransitionFinished);

/**
 * 홀 전환 연출 위젯
 * WBP_transition 의 C++ 바인딩 클래스
 *
 * 사용법:
 *   Cast<UHoleTransitionWidget>(HoleTransitionWidgetInstance)->SetHoleInfo(NextHole, Par);
 *   Cast<UHoleTransitionWidget>(HoleTransitionWidgetInstance)->PlayTransitionAnim();
 */
UCLASS()
class PARKDAY_API UHoleTransitionWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // -----------------------------------------------------------------------
    // 공개 API  (InGameMode 에서 호출)
    // -----------------------------------------------------------------------

    /** 홀 번호·파 텍스트를 갱신합니다. PlayTransitionAnim() 전에 호출하세요. */
    UFUNCTION(BlueprintCallable, Category = "HoleTransition")
    void SetHoleInfo(int32 HoleNumber, int32 Par);

    /**
     * hole_transition 애니메이션을 재생합니다.
     * 애니메이션이 없으면 OnTransitionFinished 델리게이트를 즉시 브로드캐스트합니다.
     */
    UFUNCTION(BlueprintCallable, Category = "HoleTransition")
    void PlayTransitionAnim();


    /** 애니메이션 종료 콜백 (BindToAnimationFinished 에서 자동 호출) */
    UFUNCTION()
    void OnAnimFinished();
    /**
     * 애니메이션 재생이 끝났을 때 BP 에서 호출합니다.
     * (BP 이벤트 디스패처를 쓰지 않고 C++ 델리게이트를 직접 Broadcast)
     */
    UFUNCTION(BlueprintCallable, Category = "HoleTransition")
    void NotifyTransitionFinished();

    /** 전환 애니메이션이 완전히 끝난 뒤 발생 */
    UPROPERTY(BlueprintAssignable, Category = "HoleTransition")
    FOnTransitionFinished OnTransitionFinished;

protected:
    // -----------------------------------------------------------------------
    // UMG 바인딩 (이름이 Blueprint 위젯 이름과 정확히 일치해야 합니다)
    // -----------------------------------------------------------------------

    /** "18" 숫자 텍스트 */
    UPROPERTY(meta = (BindWidget), BlueprintReadWrite)
    UEditableTextBox* txt_hole_number = nullptr;

    /** "3" 숫자 텍스트 */
    UPROPERTY(meta = (BindWidget),BlueprintReadWrite)
    UEditableTextBox* txt_par_number = nullptr;

    /** "HOLE" 레이블 텍스트 */
    //UPROPERTY(meta = (BindWidget))
    //UTextBlock* hole = nullptr;

    ///** "PAR" 레이블 텍스트 */
    //UPROPERTY(meta = (BindWidget))
    //UTextBlock* par = nullptr;

    // -----------------------------------------------------------------------
    // UMG 애니메이션 바인딩
    // -----------------------------------------------------------------------

    /** 애니메이션 패널에 있는 "hole_transition" 애니메이션 */
    UPROPERTY(Transient, meta = (BindWidgetAnim), BlueprintReadWrite)
    UWidgetAnimation* hole_transition = nullptr;

    // -----------------------------------------------------------------------
    // 내부 타이머 (애니메이션이 없을 때 폴백용)
    // -----------------------------------------------------------------------

    virtual void NativeConstruct() override;

private:
    FTimerHandle FallbackTimerHandle;

    /** 숨김 타이머 */
    FTimerHandle HideTimerHandle;

    /** 폴백: 애니메이션이 없을 때 고정 시간(2초) 후 NotifyTransitionFinished 호출 */
    static constexpr float FallbackDuration = 2.0f;

    /** 애니메이션 종료 후 숨김까지 대기 시간 (초) */
    static constexpr float HideDelay = 2.0f;

    // -----------------------------------------------------------------------
    // 사운드 에셋 (NativeConstruct 에서 로드)
    // -----------------------------------------------------------------------

    UPROPERTY(Transient)
    USoundBase* VoiceSound = nullptr;       // test_voice (항상 재생)

    UPROPERTY(Transient)
    USoundBase* Par3Sound = nullptr;        // par3 전용

    UPROPERTY(Transient)
    USoundBase* Par4Sound = nullptr;        // par4 전용

    UPROPERTY(Transient)
    USoundBase* Par5Sound = nullptr;        // par5 전용

    UPROPERTY(Transient)
    USoundBase* CurrentParSound = nullptr;  // SetHoleInfo 에서 선택됨

};