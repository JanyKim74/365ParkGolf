// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "KeyboardWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnClickEnter, FText, InputText);

class UKeyboardWidget;
class UEditableTextBox;

UCLASS()
class UKeyClickHandler : public UObject
{
	GENERATED_BODY()



public:
	UKeyboardWidget* ParentWidget = nullptr;
	FString KeyCode;


	UFUNCTION()
	void OnClick();
};

UENUM(BlueprintType)
enum class EKeyboardMode : uint8
{
	Korean        UMETA(DisplayName = "Korean"),
	EnglishLower  UMETA(DisplayName = "English Lower"),
	EnglishUpper  UMETA(DisplayName = "English Upper"),
	NumberSpecial UMETA(DisplayName = "Number / Special")
};

UCLASS()
class PARKDAY_API UKeyboardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Keyboard")
	FOnClickEnter HandleOnClickEnterDele;

public:
	UKeyboardWidget(const FObjectInitializer& ObjectInitializer);

	float LatestClickSecond = 0.f;
	float DoubleClickTime = 0.15f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(BindWidget))
	UEditableTextBox* EditableTextBox_Box;

	// 현재 출력될 문자열 (조합 중인 글자 포함)
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Keyboard")
	FString CurrentText;

	// 현재 키보드 모드
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Keyboard")
	EKeyboardMode CurrentMode;

	// 확정된 텍스트 (조합 버퍼 제외)
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Keyboard")
	FString CommittedText;

	// 키 눌렀을 때 호출
	UFUNCTION(BlueprintCallable, Category = "Keyboard")
	void HandleKeyPress(FString KeyCode);

	// 모드 전환
	UFUNCTION(BlueprintCallable, Category = "Keyboard")
	void SetKeyboardMode(EKeyboardMode NewMode);

	// 블루프린트에서 UI 라벨 갱신용 이벤트
	UFUNCTION(BlueprintImplementableEvent, Category = "Keyboard")
	void OnUpdateKeyLabels();

	// 블루프린트에서 특정 키 인덱스의 라벨 얻기 (현재 모드/SHIFT 반영)
	UFUNCTION(BlueprintCallable, Category = "Keyboard")
	FString GetLabelForKey(int32 KeyIndex);

	UFUNCTION(BlueprintCallable, Category = "Keyboard")
	void SetSelectAll(bool bIsSelectAll);

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bIsFirstDelete = true;
	// 출력 문자열 갱신 (Committed + HangulBuffer 조합)
	void UpdateDisplay();
protected:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category="Keyboard")
	void ResetKeyboardState(bool bClearText = true);



	// ------------------------------
	// UI Binding Helpers
	// ------------------------------
	struct FKeyConfig
	{
		FString WidgetName;
		FString KeyCode;     // 내부 로직용 코드
		FString LabelKor;    // 한글 모드 라벨
		FString LabelEng;    // 영문 소문자 모드 라벨
		FString LabelShift;  // 영문 대문자 모드 라벨
		FString LabelNum;    // 숫자/기호 모드 라벨

		class UTextBlock* TextBlockRef = nullptr;
	};

	// 키 설정 정보들
	TArray<FKeyConfig> KeyConfigs;

	void SetupKey(const FString& WidgetName, const FString& KeyCode,
	              const FString& Kor, const FString& Eng,
	              const FString& Shift, const FString& Num);

	void SetupSpecialKey(const FString& WidgetName, const FString& KeyCode, const FString& Label);

	class UButton* FindButtonInWidget(class UWidget* Widget);
	class UTextBlock* FindTextBlockInWidget(class UWidget* Widget);

	// 키캡 라벨 갱신
	void RefreshKeyLabels();
	FString GetDisplayLabelForConfig(const FKeyConfig& Config) const;

	// === 선택 영역 인덱스 (CommittedText 기준) ===
	// 선택이 없으면 INDEX_NONE
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Keyboard")
	int32 SelectionStartIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Keyboard")
	int32 SelectionEndIndex = INDEX_NONE;

	// BP에서 드래그 선택 결과를 넘겨줄 함수
	UFUNCTION(BlueprintCallable, Category = "Keyboard")
	void SetSelection(int32 InStart, int32 InEnd);

	UFUNCTION(BlueprintCallable, Category = "Keyboard")
	void ClearSelection();

	bool HasSelection() const;

	// ------------------------------
	// 한글 조합 관련
	// ------------------------------
	// 현재 음절을 위한 자모 버퍼
	FString HangulBuffer;

	// 한글 입력 처리
	void ProcessHangulInput(FString Jamo);
	void CommitHangul();
	void DeleteLastCharacter();

	bool IsHangul(const FString& Text);
	FString ComposeHangul(const FString& Jamos);

	// ------------------------------
	// SHIFT / 한글 쌍자음
	// ------------------------------
	// 한글 모드에서 SHIFT 눌러서 쌍자음 사용할지 여부 (다음 자음 1회용)
	bool bKoreanShift;

	// 한글 자모를 쌍자음으로 변환 (ㅂ→ㅃ, ㅈ→ㅉ, ㄷ→ㄸ, ㄱ→ㄲ, ㅅ→ㅆ)
	FString ApplyKoreanShift(const FString& InputChar);

	// ------------------------------
	// 기타
	// ------------------------------
	UPROPERTY()
	TArray<class UKeyClickHandler*> ClickHandlers;

	UPROPERTY()
	class UTextBlock* OutputTextBlock;


};
