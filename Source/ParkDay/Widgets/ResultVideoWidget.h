// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ResultVideoWidget.generated.h"

class UImage;
class UButton;
class UMediaPlayer;
class UMediaTexture;
class UMediaSource;
class UCanvasPanel;
class UMediaSoundComponent;
class UTextBlock;

UCLASS()
class PARKDAY_API UResultVideoWidget : public UUserWidget
{
	GENERATED_BODY()
public:
  virtual void NativeConstruct() override;

    // UMG의 Image 위젯 (디자이너에서 이름을 VideoImage로 맞추기)
    UPROPERTY(meta = (BindWidget))
    UImage* Image_Video;

    UPROPERTY(meta = (BindWidget))
    UCanvasPanel* CanvasPanel_TextBlock;

    // (선택) 재생/일시정지 버튼
    UPROPERTY(meta = (BindWidget))
    UButton* Button_Next;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* TextBlock_Name;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* TextBlock_CourseName;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* TextBlock_HoleNumber;


    //// 에디터에서 세팅할 MediaPlayer / MediaTexture / MediaSource
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video")
	UMediaPlayer* MediaPlayer;

    //UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Video")
    //UMediaTexture* MediaTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video")
		UMediaSource* MediaSource;

    UPROPERTY()
    UMediaSoundComponent* SC;


        UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TextHeight = 270.f;
    UFUNCTION()
    void OnVideoButtonClicked();

    UFUNCTION()
    void ChangeTextBlockPosition(float Y);

    UFUNCTION()
    void HandleMediaOpened(FString OpendUrl);

    UFUNCTION()
    void ChangeVideoPathAndPlay(const FString& NewFilePath);
};
