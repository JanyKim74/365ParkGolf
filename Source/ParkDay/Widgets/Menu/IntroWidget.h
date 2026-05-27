// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "IntroWidget.generated.h"

class UButton;
class UMediaPlayer;
class UMediaSource;
class UMediaSoundComponent;
class AMenuGameMode;

UCLASS()
class PARKDAY_API UIntroWidget : public UUserWidget
{
	GENERATED_BODY()
public:

    UPROPERTY(meta = (BindWidget))
	UButton* Button_Intro;

	    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Media")
    UMediaPlayer* MediaPlayer;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Media")
    UMediaSource* MediaSource;

	    // 실제로 소리가 나오는 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="Media")
    UMediaSoundComponent* MediaSoundComp;

	UFUNCTION()
	virtual void NativeConstruct() override;

	void Init();

	UFUNCTION()
	void OnClickVideoImage();

	void PlayIntro();
	void StopIntro();

	UFUNCTION()
	void HandleOnEnterIntro();

	UFUNCTION()
	void OnMediaOpened(FString OpenedUrl);

public:
	AMenuGameMode* GM;
	
private:
	void CreateAndAttachMediaSound();

	FTimerHandle PlayDelayTimer;
};
