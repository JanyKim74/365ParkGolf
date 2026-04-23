// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CourseSelectOptionWidget.h"
#include "CourseSelectDetailWidget.generated.h"

class UTexture2D;
class AMenuGameMode;
class UTextBlock;
class UHorizontalBox;
class UImage;
class UButton;
class UCourseSelectDetailParWidget;
class UCanvasPanel;
class UVerticalBox;

UCLASS()
class PARKDAY_API UCourseSelectDetailWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UCourseSelectDetailWidget(const FObjectInitializer& ObjectInitializer);
	virtual void NativeConstruct() override;

	UFUNCTION()
	void Init();

public:
	UPROPERTY(meta = (BindWidget))
	UImage* Image_CourseTitle;
	UPROPERTY(meta = (BindWidget))
	UTextBlock* TextBlock_Name;
	UPROPERTY(meta = (BindWidget))
	UTextBlock* TextBlock_Distance;

	UPROPERTY(meta = (BindWidget))
	UHorizontalBox* HorizontalBox_Stars;
	UPROPERTY(meta = (BindWidget))
	UHorizontalBox* HorizontalBox_APar;
	UPROPERTY(meta = (BindWidget))
	UHorizontalBox* HorizontalBox_BPar;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* TextBlock_OutCourse_Name;
	UPROPERTY(meta = (BindWidget))
	UTextBlock* TextBlock_InCourse_Name;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* TextBlock_OutCourse_Total;
	UPROPERTY(meta = (BindWidget))
	UTextBlock* TextBlock_InCourse_Total;

	UPROPERTY(meta = (BindWidget))
	UHorizontalBox* HorizontalBox_SelectCourse;
	UPROPERTY(meta = (BindWidget))
	UHorizontalBox* HorizontalBox_Muligan;
	UPROPERTY(meta = (BindWidget))
	UHorizontalBox* HorizontalBox_PinLocation;
	UPROPERTY(meta = (BindWidget))
	UHorizontalBox* HorizontalBox_Concede;
	UPROPERTY(meta = (BindWidget))
	UHorizontalBox* HorizontalBox_GrassCondition;
	UPROPERTY(meta = (BindWidget))
	UHorizontalBox* HorizontalBox_ContinuePutting;
	UPROPERTY(meta = (BindWidget))
	UHorizontalBox* HorizontalBox_CameraMode;
	UPROPERTY(meta = (BindWidget))
	UHorizontalBox* HorizontalBox_SwingMotion;

	UFUNCTION()
	void BindOptions();

public:
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSoftClassPtr<UCourseSelectDetailParWidget> ParWidgetSoftClass;

public:
	// ✅ 수정: FFieldMapInfo& → FFieldMapInfo (Dynamic delegate는 참조 파라미터 불가)
	UFUNCTION()
	void HandleOnClickCourseButton(FFieldMapInfo FieldMapInfo, FString CCFolderName);

	UFUNCTION()
	void HandleOnEnterCourseSelect();

	UFUNCTION()
	void HandleOnClickOption(EGameOption OptionType, int32 OptionValue);

public:
	UFUNCTION()
	bool LoadBackgroundImage(FString CCName);

private:
	void InitializePar();
	void InitializeOption();

	void UpdateStar();
	void UpdatePar();

private:
	UPROPERTY()
	TMap<FString, UTexture2D*> BackgroundImages;

	UPROPERTY()
	AMenuGameMode* GM;

	UPROPERTY()
	bool bInitialized = false;
};