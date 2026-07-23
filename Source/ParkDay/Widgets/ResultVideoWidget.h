// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MediaPlaylist.h"  // 전방선언 대신 include 필요
#include "ResultVideoWidget.generated.h"

class UImage;
class UButton;
class UMediaPlayer;
class UMediaTexture;
class UMediaSource;
class UCanvasPanel;
class UMediaSoundComponent;
class UTextBlock;
class UFileMediaSource;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnResultVideoClosed);

UCLASS()
class PARKDAY_API UResultVideoWidget : public UUserWidget
{
	GENERATED_BODY()
public:
    virtual void NativeOnInitialized() override;  // ★ 추가
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Video")
    UMediaTexture* MediaTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video")
		UMediaSource* MediaSource;


        // 스코어별 MediaSource 하드 레퍼런스
    UPROPERTY() 
    UFileMediaSource* MS_Eagle = nullptr;
    UPROPERTY() 
    UFileMediaSource* MS_Albatross = nullptr;
    UPROPERTY() 
    UFileMediaSource* MS_Holeinone = nullptr;
    UPROPERTY() 
    UFileMediaSource* MS_Victory = nullptr;

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


    UFUNCTION()
    void OnResultMediaOpened(FString OpenedUrl);

    UFUNCTION()
    void  OnResultMediaOpenFailed(FString FailedUrl);

    void CreateAndAttachMediaSound();

    // ⭐ Brush-MediaTexture 연결을 NativeConstruct/ChangeVideoPathAndPlay 양쪽에서 안전하게 재시도할 수 있도록 분리
    void EnsureMediaBrushBound();

    FTimerHandle CloseDelayTimer;  // Close 후 Open 딜레이용

    FTimerHandle TestHandle;  // Close 후 Open 딜레이용

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video")
    UMediaPlaylist* ResultPlaylist;  // BP에서 ResultPlaylist 에셋 지정

    UPROPERTY(BlueprintAssignable, Category = "Video")
    FOnResultVideoClosed OnResultVideoClosed;

};
