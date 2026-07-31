// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ParkDay/Structs/CorseStruct.h"
#include "CourseSelectOptionWidget.generated.h"

UENUM(BlueprintType)
enum class EGameOption : uint8
{
    SubLevel        UMETA(DisplayName = "SubLevel"),
    Mulligan        UMETA(DisplayName = "Mulligan"),
    PinLocation     UMETA(DisplayName = "PinLocation"),
    Concede         UMETA(DisplayName = "Concede"),
    GrassCondition  UMETA(DisplayName = "GrassCondition"),
    ContinuePutting UMETA(DisplayName = "ContinuePutting"),
    CameraMode      UMETA(DisplayName = "CameraMode"),
    SwingMotion     UMETA(DisplayName = "SwingMotion"),
    PracticeMode    UMETA(DisplayName = "PracticeMode"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnClickOption, EGameOption, OptionType, int32, OptionValue);

class UButton;
class UTextBlock;
class AMenuGameMode;

UCLASS()
class PARKDAY_API UCourseSelectOptionWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

public:
    // ◄ 이전 버튼
    UPROPERTY(meta = (BindWidget))
    UButton* Button_Prev;

    // ► 다음 버튼
    UPROPERTY(meta = (BindWidget))
    UButton* Button_Next;

    // 현재 선택값 표시 텍스트
    UPROPERTY(meta = (BindWidget))
    UTextBlock* TextBlock_Value;

    // BP 에디터에서 설정할 옵션 타입
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Setting")
    EGameOption OptionType = EGameOption::SubLevel;

    // 표시 레이블 목록 (OptionValues와 1:1 매핑)
    // 예) SubLevel: ["A코스", "B코스"], Mulligan: ["0", "1", "2", "3"]
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Setting")
    TArray<FString> OptionLabels;

    // 실제 int32 값 목록 (GameOptions에 저장되는 값)
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Setting")
    TArray<int32> OptionValues;

public:
    UPROPERTY()
    FOnClickOption OnClickOptionDele;

    UFUNCTION()
    void HandleOnClickPrev();

    UFUNCTION()
    void HandleOnClickNext();

    UFUNCTION()
    void HandleOnEnterCourseSelect();

    UFUNCTION()
    void HandleOnClickCourseMap(FFieldMapInfo FieldMapInfo, FString CCFolderName);

public:
    void Init();
    void RefreshDisplay();
    void UpdateSublevelName();
    void BindCourseMap();
    void SetCurrentIndex(int32 NewIndex);

    // 외부에서 현재 값(int32)으로 인덱스를 찾아 동기화할 때 사용
    void SyncToValue(int32 Value);

private:
    int32 CurrentIndex = 0;

    AMenuGameMode* GM;
};