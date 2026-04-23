// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ParkDay/GolfDataStructures.h"
#include "PlayerSelectWidget.generated.h"

class UWrapBox;
class AMenuGameMode;
class AInGameMode;
class UButton;
class UKeyboardWidget;
class UImage;
class UPlayerSelectProfileWidget;

UCLASS()
class PARKDAY_API UPlayerSelectWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPlayerSelectWidget(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(meta=(BindWidget))
	UWrapBox* WrapBox_PlayerProfiles;

	UPROPERTY(meta=(BindWidget))
	UButton* Button_Back;
	UPROPERTY(meta=(BindWidget))
	UButton* Button_Next;
	UPROPERTY(meta=(BindWidget))
	UImage* Image_Background;

public:
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="UI")
    TSubclassOf<UKeyboardWidget> KeyBoardWidgetClass;

    UPROPERTY()
    UKeyboardWidget* KeyBoardWidgetInstance;

public:
	UFUNCTION()
	void HandleOnClickButtonBack();

	UFUNCTION()
	void HandleOnClickButtonNext();

	UFUNCTION()
	void HandleOnEnterPlayerSelect();

public:
	UFUNCTION()
	void CheckCanNext();

public:
	UFUNCTION()
	virtual void NativeConstruct() override;
	void Init();

	void SortPlayerInfo(FGameInfo& InGameInfo);

	void UpdateButtonStatus();

	UPlayerSelectProfileWidget* FindProfile(int32 SlotIndex);
	FGameInfo GameInfo;
private:
	AMenuGameMode* GM;
	AInGameMode* InGM;
};
