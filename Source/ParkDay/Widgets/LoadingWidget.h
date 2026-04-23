// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoadingWidget.generated.h"

class UImage;
class UTexture2D;

UCLASS()
class PARKDAY_API ULoadingWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeConstruct() override;

public:
	UPROPERTY(meta = (BindWidget)) UImage* Image_Background;

	// �ٷ� �ݺ� ������ ���ù�
	UPROPERTY(VisibleAnywhere, Category = "Loading")
		TArray<int32> ShuffleBag;

	// (�ɼ�) �õ� ������
	UPROPERTY(EditAnywhere, Category = "Loading")
		int32 RandomSeed = 0; // 0�̸� �ð迭 �õ�


	UFUNCTION()
		void SetRandomImageIndex();
	UFUNCTION()
		void ShowRandomImage();
	UFUNCTION()
		void StopBGM();
	UFUNCTION()
		void PlayBGM();

	UPROPERTY()
	int32 RandomIndex = 0;
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<UTexture2D*> BackgroundImages;

};
