#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FadeWidget.generated.h"

class UWidgetAnimation;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFadeWidgetEvent);

// BP/Reflection ������ �ݹ� ��������Ʈ (�Ķ���� ����)
DECLARE_DELEGATE(FFadeCallback);

UCLASS()
class PARKDAY_API UFadeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFadeWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category="Fade")
	void FadeIn(float Duration);

	void FadeInWithCallback(float Duration, FFadeCallback Callback);

	UFUNCTION(BlueprintCallable, Category="Fade")
	void FadeOut(float Duration);

	UFUNCTION(BlueprintCallable, Category="Fade")
	void FadeInOut(float FadeInDuration, float HoldTime, float FadeOutDuration);

	UPROPERTY(BlueprintAssignable, Category="Fade")
	FFadeWidgetEvent OnFadeInFinished;

	UPROPERTY(BlueprintAssignable, Category="Fade")
	FFadeWidgetEvent OnFadeOutFinished;

	UPROPERTY(BlueprintAssignable, Category="Fade")
	FFadeWidgetEvent OnFadeSequenceFinished;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void PlayAnimationScaled(UWidgetAnimation* Anim, float DesiredDuration);
	void StopAndClear();

	UFUNCTION()
	void HandleFadeInFinished_Single();

	UFUNCTION()
	void HandleFadeOutFinished_Single();

	UFUNCTION()
	void HandleFadeInFinished_Sequence();

	UFUNCTION()
	void StartFadeOut_Sequence();

	UFUNCTION()
	void HandleFadeOutFinished_Sequence();

private:
	UPROPERTY(Transient, meta=(BindWidgetAnim))
	UWidgetAnimation* Animation_FadeIn = nullptr;

	UPROPERTY(Transient, meta=(BindWidgetAnim))
	UWidgetAnimation* Animation_FadeOut = nullptr;

	FFadeCallback PendingFadeInCallback;

	float PendingHoldTime = 0.0f;
	float PendingFadeOutDuration = 0.0f;

	FTimerHandle HoldTimerHandle;
};
