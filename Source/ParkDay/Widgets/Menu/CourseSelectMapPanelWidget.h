// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../../Structs/CorseStruct.h"
#include "CourseSelectMapPanelWidget.generated.h"

UENUM(BlueprintType)
enum class ESortOrder : uint8
{
	Ascending  UMETA(DisplayName = "Ascending"),
	Descending UMETA(DisplayName = "Descending")
};

UENUM(BlueprintType)
enum class EFieldSortKey : uint8
{
	CCname      UMETA(DisplayName = "Name"),
	CourseLevel UMETA(DisplayName = "Difficulty"),
	Recommend   UMETA(DisplayName = "Recommend"),
	Area        UMETA(DisplayName = "Area")
};

class AMenuGameMode;
class UCourseSelectMapWidget;
class UScrollBox;
class UButton;
class UWrapBox;
class UCheckBox;
class UEditableTextBox;
class UVerticalBox;
class UCanvasPanel;

UCLASS()
class PARKDAY_API UCourseSelectMapPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UCourseSelectMapPanelWidget(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSoftClassPtr<UCourseSelectMapWidget> CourseMapWidgetClassSoft;

	UFUNCTION()
	virtual void NativeConstruct() override;

	UFUNCTION()
	virtual void NativeOnInitialized() override;

	// ✅ 수정: FFieldMapInfo& → FFieldMapInfo (Dynamic delegate는 참조 파라미터 불가)
	UFUNCTION()
	void HandleOnClickCourseButton(FFieldMapInfo FieldMapInfo, FString CCFolderName);

	void SelectFirstOne();

public:
	UPROPERTY(meta = (BindWidget))
	UScrollBox* ScrollBox_CourseMaps;
	UPROPERTY(meta = (BindWidget))
	UWrapBox* WrapBox_CourseMaps;
	UPROPERTY(meta = (BindWidget))
	UButton* Button_ScrollUp;
	UPROPERTY(meta = (BindWidget))
	UButton* Button_ScrollDown;
	UPROPERTY(meta = (BindWidget))
	UCheckBox* CheckBox_All;
	UPROPERTY(meta = (BindWidget))
	UCheckBox* CheckBox_Recommend;
	UPROPERTY(meta = (BindWidget))
	UCheckBox* CheckBox_Difficulty;
	UPROPERTY(meta = (BindWidget))
	UCheckBox* CheckBox_Location;
	UPROPERTY(meta = (BindWidget))
	UButton* Button_Search;
	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* EditableTextBox_Search;

	UPROPERTY(meta = (BindWidget))
	UCanvasPanel* CanvasPanel_Area;
	UPROPERTY(meta = (BindWidget))
	UVerticalBox* VerticalBox_Area;

	UPROPERTY()
	TArray<UCheckBox*> AreaList;
	UFUNCTION() void InitializeAreaList();
	UFUNCTION() void UnCheckAllArea();
	UFUNCTION() void CheckArea(int32 AreaNumber);
	UFUNCTION() void ApplyAreaCheck(UCheckBox* CheckedBox, bool bIsChecked, int32 AreaNumber);
	UFUNCTION() void HandleOnChangeStateAreaCheckBox_1(bool bIsChecked);
	UFUNCTION() void HandleOnChangeStateAreaCheckBox_2(bool bIsChecked);
	UFUNCTION() void HandleOnChangeStateAreaCheckBox_3(bool bIsChecked);
	UFUNCTION() void HandleOnChangeStateAreaCheckBox_4(bool bIsChecked);
	UFUNCTION() void HandleOnChangeStateAreaCheckBox_5(bool bIsChecked);
	UFUNCTION() void HandleOnChangeStateAreaCheckBox_6(bool bIsChecked);
	UFUNCTION() void HandleOnChangeStateAreaCheckBox_7(bool bIsChecked);
	UFUNCTION() void HandleOnChangeStateAreaCheckBox_8(bool bIsChecked);

	UFUNCTION()
	void HandleOnClickScrollDown();
	UFUNCTION()
	void HandleOnClickScrollUp();
	UFUNCTION()
	void HandleOnClickSearch();

	UFUNCTION()
	void HandleOnClickAllCheckBox(bool bIsChecked);
	UFUNCTION()
	void HandleOnClickRecommendCheckBox(bool bIsChecked);
	UFUNCTION()
	void HandleOnClickDifficultyCheckBox(bool bIsChecked);
	UFUNCTION()
	void HandleOnClickLocationCheckBox(bool bIsChecked);

	UFUNCTION() void CollapseAllMapWidget();
	UFUNCTION() void VisibleAllMapWidget();

	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;

	UFUNCTION()
	void HandleEditBoxEnterFocus();

	UFUNCTION()
	void HandleOnClickKeyboardEnter(FText InputText);

public:
	UPROPERTY(BlueprintReadOnly, Category = "Sort")
	EFieldSortKey CurrentSortKey = EFieldSortKey::CCname;

	UPROPERTY(BlueprintReadOnly, Category = "Sort")
	ESortOrder CurrentSortOrder = ESortOrder::Ascending;

	UFUNCTION(BlueprintCallable, Category = "Sort")
	void SortItems(EFieldSortKey SortKey, ESortOrder SortOrder, bool bStable = true);

	UFUNCTION()
	UCourseSelectMapWidget* GetSelectedMapWidget();

	TArray<UCourseSelectMapWidget*> MapArray;

private:
	AMenuGameMode* GM;

	void ReAddChildMapWidget();
};