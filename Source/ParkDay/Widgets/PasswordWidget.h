// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PasswordWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnConfirmPassword);

class UEditableTextBox;
class UButton;
class AMenuGameMode;

UCLASS()
class PARKDAY_API UPasswordWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeConstruct() override;

	FOnConfirmPassword OnConfirmPasswordDele;

	UPROPERTY(meta = (BindWidget)) UEditableTextBox* EditableTextBox_Password;
	UPROPERTY(meta = (BindWidget)) UButton* Button_Confirm;
	UPROPERTY(meta = (BindWidget)) UButton* Button_Cancel;

	UFUNCTION()	void HandleOnPressedConfirmButton();
	UFUNCTION()	void HandleOnPressedCancelButton();
	UFUNCTION() void HandleOnChangedPasswordEditableText(const FText& InputText);
	UFUNCTION() void HandleOnCommittedPasswordEditableText(const FText& Text, ETextCommit::Type CommitMethod);
	void SetFocusTextBox();

	UFUNCTION()
	void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent);
	UFUNCTION() void HandleOnClickKeyboardEnter(FText InputText);
	UFUNCTION()
	void HandleEditBoxEnterFocus();
	UFUNCTION()
	void SetNextUIState(EUIState InNextUI);

private:
	AMenuGameMode* GM;
	FString GetPassword();
	EUIState NextUI;
};
