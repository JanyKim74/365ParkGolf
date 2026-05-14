#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ResultWidget.generated.h"

class AInGameMode;
struct FResultUI;
class UImage;
class UWidgetAnimation;
class UDataTable;
class UTexture2D;           // ★ 전방 선언 추가
class UUMGSequencePlayer;   // ★ 전방 선언 추가

UCLASS()
class PARKDAY_API UResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	// 다음 홀로 넘어가는 중 한 번만 무시하고 싶을 때 쓰는 플래그로 보임
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Result")
		bool bIsNextHole = false;

	UFUNCTION(BlueprintImplementableEvent, Category = "Sound")
		void PlayShotCountSound(int32 ShotCount);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Sound")
		USoundBase* ResultSound;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Sound")
		USoundBase* ConcedeSound;

public:
	// PlayAnimation이 반환하는 플레이어
	UPROPERTY(Transient) // ★ GC/저장 제외
		UUMGSequencePlayer* AnimPlayer = nullptr;

	// 스코어 → 텍스처 매핑
	UPROPERTY()
		TMap<int32, UTexture2D*> ResultMap;
		TMap<int32, TSoftObjectPtr<USoundBase>> ResultSoundMap;
		TMap<int32, TSoftObjectPtr<USoundBase>> ResultConcedeMap;

	// 위젯 바인딩들 (없어도 안전하게 방어)
	UPROPERTY(meta = (BindWidget)) UImage* Image_BackPanel = nullptr;
	//UPROPERTY(meta = (BindWidget)) UImage* Image_Result = nullptr;

	// ★ 메타키 오타 수정: BindWidgetAnim
	UPROPERTY(meta = (BindWidgetAnim), BlueprintReadOnly, Transient, Category = "Animation") UWidgetAnimation* Result_bogey = nullptr;
	UPROPERTY(meta = (BindWidgetAnim), BlueprintReadOnly, Transient, Category = "Animation")UWidgetAnimation* Result_par = nullptr;
	UPROPERTY(meta = (BindWidgetAnim), BlueprintReadOnly, Transient, Category = "Animation") UWidgetAnimation* Result_birdie = nullptr;
	UPROPERTY(meta = (BindWidgetAnim), BlueprintReadOnly, Transient, Category = "Animation") UWidgetAnimation* Result_tbogey = nullptr;
	UPROPERTY(meta = (BindWidgetAnim), BlueprintReadOnly, Transient, Category = "Animation") UWidgetAnimation* Result_qbogey = nullptr;
	UPROPERTY(meta = (BindWidgetAnim), BlueprintReadOnly, Transient, Category = "Animation") UWidgetAnimation* Result_dbogey = nullptr;

	UPROPERTY(meta = (BindWidgetAnim), BlueprintReadOnly, Transient, Category = "Animation") UWidgetAnimation* Result_dpar = nullptr;

	// ★ 추가: 각 결과 CanvasPanel 바인딩
// BP 위젯 이름과 정확히 일치해야 함
	UPROPERTY(meta = (BindWidget), BlueprintReadWrite) UCanvasPanel* birdie = nullptr;
	UPROPERTY(meta = (BindWidget), BlueprintReadWrite) UCanvasPanel* par = nullptr;
	UPROPERTY(meta = (BindWidget), BlueprintReadWrite) UCanvasPanel* bogey = nullptr;
	UPROPERTY(meta = (BindWidget), BlueprintReadWrite) UCanvasPanel* dbogey = nullptr;
	UPROPERTY(meta = (BindWidget), BlueprintReadWrite) UCanvasPanel* tbogey = nullptr;
	UPROPERTY(meta = (BindWidget), BlueprintReadWrite) UCanvasPanel* dpar = nullptr;


	// ★ 추가: C++에서 직접 패널 켜고 3초 후 끄는 함수
	UFUNCTION(BlueprintCallable, Category = "Result")
	void ShowResultPanel(UCanvasPanel* Panel, float HideDelay = 3.0f);

	// ★ 추가: 모든 패널 일괄 Hide
	UFUNCTION(BlueprintCallable, Category = "Result")
	void HideAllResultPanels();

	//UPROPERTY(meta = (BindWidgetAnim), Transient) UWidgetAnimation* Anim_Event = nullptr;

	UFUNCTION(BlueprintImplementableEvent, Category = "Result")
	void ResultPlay(int32 index);

	UFUNCTION(BlueprintCallable) void PlayAnim(UWidgetAnimation* Anim, bool bLoop = false);
	UFUNCTION(BlueprintCallable) void PauseAnim();
	UFUNCTION(BlueprintCallable) void StopAnim(const UWidgetAnimation* Anim);
	UFUNCTION(BlueprintCallable) void RestartAnim(UWidgetAnimation* Anim);
	UFUNCTION(BlueprintCallable) void ResumeAnim(UWidgetAnimation* Anim);

	UFUNCTION(BlueprintCallable)
		void PlayResult(int32 Score);
		// 게임모드 캐시
	UPROPERTY() // ★ GC에 안전
		AInGameMode* GM = nullptr;

	void SetResultIndex(int value);

private:
	void SetSoundAndImage(int32 Score);

	// ★ 추가: 자동 Hide 타이머 핸들
	FTimerHandle ResultHideTimer;
};
