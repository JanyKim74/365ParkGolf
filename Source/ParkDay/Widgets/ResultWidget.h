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
	UPROPERTY(meta = (BindWidget)) UImage* Image_Result = nullptr;

	// ★ 메타키 오타 수정: BindWidgetAnim
	UPROPERTY(meta = (BindWidgetAnim), Transient) UWidgetAnimation* Anim_Bogey = nullptr;
	UPROPERTY(meta = (BindWidgetAnim), Transient) UWidgetAnimation* Anim_Par = nullptr;
	UPROPERTY(meta = (BindWidgetAnim), Transient) UWidgetAnimation* Anim_Birdie = nullptr;
	UPROPERTY(meta = (BindWidgetAnim), Transient) UWidgetAnimation* Anim_Eagle = nullptr;
	UPROPERTY(meta = (BindWidgetAnim), Transient) UWidgetAnimation* Anim_Albatross = nullptr;
	UPROPERTY(meta = (BindWidgetAnim), Transient) UWidgetAnimation* Anim_DoublePar = nullptr;
	UPROPERTY(meta = (BindWidgetAnim), Transient) UWidgetAnimation* Anim_Event = nullptr;

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
private:
	void SetSoundAndImage(int32 Score);
};
