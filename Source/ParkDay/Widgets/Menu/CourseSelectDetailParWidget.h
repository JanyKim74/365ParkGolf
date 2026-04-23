// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CourseSelectDetailParWidget.generated.h"

class UTextBlock;


UCLASS()
class PARKDAY_API UCourseSelectDetailParWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(meta=(BindWidget))
	UTextBlock* TextBlock_Par_Index;

	UPROPERTY(meta=(BindWidget))
	UTextBlock* TextBlock_Par_Count;

public:
	UPROPERTY()
	int32 ParIndex;
	UPROPERTY()
	int32 ParCount;
};
