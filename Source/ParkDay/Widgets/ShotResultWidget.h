#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../GolfDataStructures.h"
#include "../LandScapeChecker.h"
#include "ShotResultWidget.generated.h"

class UShotResultDataAsset;
class UImage;
class UTexture2D;
class USoundBase;

UCLASS()
class PARKDAY_API UShotResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="ShotResult")
	UShotResultDataAsset* ShotResultDataAsset;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="ShotResult")
	TSoftObjectPtr<UTexture2D> OBTexture;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="ShotResult")
	TSoftObjectPtr<USoundBase> OBSound;

public:
	UPROPERTY(meta = (BindWidget))
	UImage* Image_ShotResult;

public:
	UFUNCTION()
	void ApplyLandType(ELandType NewLandType);

	UFUNCTION()
	void PlayShotResult(ELandType NewLandType);

	UFUNCTION()
	void PlayShotResult_OB();

public:
		// PlayAnimation이 반환하는 플레이어
	UPROPERTY(Transient) // ★ GC/저장 제외
		UUMGSequencePlayer* AnimPlayer = nullptr;

	//UFUNCTION(BlueprintCallable) void PlayAnim(UWidgetAnimation* Anim, bool bLoop = false);
	//UFUNCTION(BlueprintCallable) void PauseAnim();
	//UFUNCTION(BlueprintCallable) void StopAnim(const UWidgetAnimation* Anim);
	UFUNCTION() void RestartAnim(UWidgetAnimation* Anim);
	//UFUNCTION(BlueprintCallable) void ResumeAnim(UWidgetAnimation* Anim);


protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ShotResult")
	ELandType CurrentLandType;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ShotResult")
	UTexture2D* ShotResultTexture;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ShotResult")
	USoundBase* ShotResultSound;

	UPROPERTY(meta = (BindWidgetAnim), Transient) 
	UWidgetAnimation* Anim_ShotResult;
};
