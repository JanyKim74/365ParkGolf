// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../../Structs/CorseStruct.h"
#include "UObject/ScriptMacros.h"
#include "CourseSelectMapWidget.generated.h"



DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnClickCourseButton, FFieldMapInfo, FieldMapInfo, FString, CCFolderName);

class AMenuGameMode;
class UImage;
class UTextBlock;
class UHorizontalBox;
class UTexture2D;
class UButton;

UCLASS()
class PARKDAY_API UCourseSelectMapWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FOnClickCourseButton OnClickCourseButtonDele;

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

public:
	UPROPERTY(meta = (BindWidget))
	UImage* Image_Backgound;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* TextBlock_Name;

	// [추가] 지역명 표시 (FieldMapInfo.Address)
	UPROPERTY(meta = (BindWidget))
	UTextBlock* TextBlock_Address;

	UPROPERTY(meta = (BindWidget))
	UHorizontalBox* HorizontalBox_Stars;

	UPROPERTY(meta = (BindWidget))
	UButton* Button_CourseMap;

	UFUNCTION()
	void HandleOnClickCourseMap();

public:
	void Init(FString CCName);
	FString CCFolderName;
	FFieldMapInfo FieldMapInfo;

	void UpdateCourseMapPanelImage();
	bool bIsSelected = false;

private:
	void SetBackgroundImage(UTexture2D* Texture);
	void SetMapInfo();

	bool LoadBackgroundImage(FString CCName);
	bool LoadFieldMapInfo(FString CCName);

private:
	UTexture2D* OnImage;
	UTexture2D* OffImage;

	AMenuGameMode* GM;
};