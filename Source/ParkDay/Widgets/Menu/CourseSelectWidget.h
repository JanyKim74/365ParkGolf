// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CourseSelectWidget.generated.h"

class UTexture2D;
class UCourseSelectMapPanelWidget;
class UCourseSelectDetailWidget;
class AMenuGameMode;
class UButton;

UCLASS()
class PARKDAY_API UCourseSelectWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(meta = (BindWidget))
	UCourseSelectDetailWidget* WBP_CourseMapDetail;
	UPROPERTY(meta = (BindWidget))
	UCourseSelectMapPanelWidget* WBP_CorseMap_Panel;
	UPROPERTY(meta = (BindWidget))
		UButton* Button_Back;
	UPROPERTY(meta = (BindWidget))
		UButton* Button_GameStart;

	UFUNCTION()
	virtual void NativeConstruct() override;

	UFUNCTION()
		void HandleOnClickBackButton();
	UFUNCTION()
		void HandleOnClickGameStartButton();

	void UpdateSelectedMapInfo();

public:
	void Init();


private:
	AMenuGameMode* GM;
};
