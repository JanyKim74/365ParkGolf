#include "FadeWidget.h"

#include "Animation/WidgetAnimation.h"
#include "TimerManager.h"

UFadeWidget::UFadeWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UFadeWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Hidden);
}

void UFadeWidget::NativeDestruct()
{
	StopAndClear();
	Super::NativeDestruct();
}

void UFadeWidget::FadeIn(float Duration)
{
	FFadeCallback Empty;
	FadeInWithCallback(Duration, Empty);
}

void UFadeWidget::FadeInWithCallback(float Duration, FFadeCallback Callback)
{
	if (!Animation_FadeIn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UFadeWidget] FadeIn failed: Animation_FadeIn not found."));
		return;
	}

	StopAndClear();

	PendingFadeInCallback = Callback;

	SetVisibility(ESlateVisibility::HitTestInvisible);

	UnbindAllFromAnimationFinished(Animation_FadeIn);

	FWidgetAnimationDynamicEvent EndDelegate;
	EndDelegate.BindDynamic(this, &UFadeWidget::HandleFadeInFinished_Single);
	BindToAnimationFinished(Animation_FadeIn, EndDelegate);

	PlayAnimationScaled(Animation_FadeIn, FMath::Max(0.0f, Duration));
}

void UFadeWidget::FadeOut(float Duration)
{
	if (!Animation_FadeOut)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UFadeWidget] FadeOut failed: Animation_FadeOut not found."));
		return;
	}

	StopAndClear();

	SetVisibility(ESlateVisibility::HitTestInvisible);

	UnbindAllFromAnimationFinished(Animation_FadeOut);

	FWidgetAnimationDynamicEvent EndDelegate;
	EndDelegate.BindDynamic(this, &UFadeWidget::HandleFadeOutFinished_Single);
	BindToAnimationFinished(Animation_FadeOut, EndDelegate);

	PlayAnimationScaled(Animation_FadeOut, FMath::Max(0.0f, Duration));
}

void UFadeWidget::FadeInOut(float FadeInDuration, float HoldTime, float FadeOutDuration)
{
	if (!Animation_FadeIn || !Animation_FadeOut)
	{
		if (!Animation_FadeIn)  UE_LOG(LogTemp, Warning, TEXT("[UFadeWidget] Animation_FadeIn not found."));
		if (!Animation_FadeOut) UE_LOG(LogTemp, Warning, TEXT("[UFadeWidget] Animation_FadeOut not found."));
		return;
	}

	StopAndClear();

	PendingHoldTime = FMath::Max(0.0f, HoldTime);
	PendingFadeOutDuration = FMath::Max(0.0f, FadeOutDuration);

	SetVisibility(ESlateVisibility::HitTestInvisible);

	UnbindAllFromAnimationFinished(Animation_FadeIn);

	FWidgetAnimationDynamicEvent EndDelegate;
	EndDelegate.BindDynamic(this, &UFadeWidget::HandleFadeInFinished_Sequence);
	BindToAnimationFinished(Animation_FadeIn, EndDelegate);

	PlayAnimationScaled(Animation_FadeIn, FMath::Max(0.0f, FadeInDuration));
}

void UFadeWidget::HandleFadeInFinished_Single()
{
	if (Animation_FadeIn)
	{
		UnbindAllFromAnimationFinished(Animation_FadeIn);
	}

	OnFadeInFinished.Broadcast();

	// 콜백 1회 실행 후 해제
	if (PendingFadeInCallback.IsBound())
	{
		FFadeCallback Local = PendingFadeInCallback;
		PendingFadeInCallback.Unbind();
		Local.ExecuteIfBound();
	}
}

void UFadeWidget::HandleFadeOutFinished_Single()
{
	if (Animation_FadeOut)
	{
		UnbindAllFromAnimationFinished(Animation_FadeOut);
	}

	SetVisibility(ESlateVisibility::Hidden);
	OnFadeOutFinished.Broadcast();
}

void UFadeWidget::HandleFadeInFinished_Sequence()
{
	if (Animation_FadeIn)
	{
		UnbindAllFromAnimationFinished(Animation_FadeIn);
	}

	if (PendingHoldTime <= 0.0f)
	{
		StartFadeOut_Sequence();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(HoldTimerHandle, this, &UFadeWidget::StartFadeOut_Sequence, PendingHoldTime, false);
	}
	else
	{
		StartFadeOut_Sequence();
	}
}

void UFadeWidget::StartFadeOut_Sequence()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HoldTimerHandle);
	}

	UnbindAllFromAnimationFinished(Animation_FadeOut);

	FWidgetAnimationDynamicEvent EndDelegate;
	EndDelegate.BindDynamic(this, &UFadeWidget::HandleFadeOutFinished_Sequence);
	BindToAnimationFinished(Animation_FadeOut, EndDelegate);

	PlayAnimationScaled(Animation_FadeOut, PendingFadeOutDuration);
}

void UFadeWidget::HandleFadeOutFinished_Sequence()
{
	if (Animation_FadeOut)
	{
		UnbindAllFromAnimationFinished(Animation_FadeOut);
	}

	SetVisibility(ESlateVisibility::Hidden);
	OnFadeSequenceFinished.Broadcast();
}

void UFadeWidget::PlayAnimationScaled(UWidgetAnimation* Anim, float DesiredDuration)
{
	if (!Anim)
	{
		return;
	}

	const float StartTime = Anim->GetStartTime();
	const float EndTime = Anim->GetEndTime();
	const float AnimLength = FMath::Max(EndTime - StartTime, 0.001f);

	float PlayRate = 1.0f;
	if (DesiredDuration > 0.0f)
	{
		PlayRate = AnimLength / DesiredDuration;
		PlayRate = FMath::Max(PlayRate, 0.001f);
	}

	PlayAnimation(Anim, 0.0f, 1, EUMGSequencePlayMode::Forward, PlayRate);
}

void UFadeWidget::StopAndClear()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HoldTimerHandle);
	}

	StopAllAnimations();

	if (Animation_FadeIn)
	{
		UnbindAllFromAnimationFinished(Animation_FadeIn);
	}
	if (Animation_FadeOut)
	{
		UnbindAllFromAnimationFinished(Animation_FadeOut);
	}

	PendingFadeInCallback.Unbind();

	PendingHoldTime = 0.0f;
	PendingFadeOutDuration = 0.0f;
}
