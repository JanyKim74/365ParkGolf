#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ParkDay/GolfDataStructures.h"
#include "BallDistanceWidget.generated.h"

class UImage;
class UTexture2D;
class UTextBlock;
class AInGameMode;
class AGolfBall;
class AGolfPlayer;

UCLASS()
class PARKDAY_API UBallDistanceWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidget))	UTextBlock* TextBlock_TotalDistance;
	UPROPERTY(meta = (BindWidget))	UTextBlock* TextBlock_RemainDistance;
	UPROPERTY(meta = (BindWidget))	UTextBlock* TextBlock_CarryDistance;

	/** Ÿ�� ��ġ�� ���� �����մϴ�. ���� �� Ȧ�� ��ġ ��� �� ��ġ�� ���� �Ÿ��� ����մϴ�. */
	UFUNCTION(BlueprintCallable, Category = "BallDistance")
		void SetCustomTargetLocation(const FVector& InTargetLocation);

	/** Ÿ�� ��ġ�� �ʱ�ȭ�Ͽ� �ٽ� Ȧ�� ��ġ�� ���� �Ÿ��� ����մϴ�. */
	UFUNCTION(BlueprintCallable, Category = "BallDistance")
		void ClearCustomTargetLocation();

	/** ���� ��� ���� Ÿ�� ��ġ�� ��ȯ�մϴ�. */
	UFUNCTION(BlueprintCallable, Category = "BallDistance")
		FVector GetCurrentTargetLocation() const;

	/** ���� Ŀ���� Ÿ���� �����Ǿ� �ִ��� ���� */
	UFUNCTION(BlueprintCallable, Category = "BallDistance")
		bool HasCustomTargetLocation() const { return bHasCustomTarget; }

private:
	AInGameMode* GM;
	AGolfBall* CachedBall = nullptr;
	AGolfPlayer* CachedPlayer = nullptr;
	EBallState LastBallState = EBallState::Ball_Init;
	bool bHasShotStart = false;
	bool bFlyByHeightStarted = false;
	bool bCarryLocked = false;
	float CarryDistanceM = 0.0f;
	FVector ShotStartLocation = FVector::ZeroVector;
	FVector2D LastValidScreenPos = FVector2D::ZeroVector;
	bool bHasValidScreenPos = false;

	/** Ŀ���� Ÿ�� ��ġ (SetCustomTargetLocation���� ����) */
	FVector CustomTargetLocation = FVector::ZeroVector;
	bool bHasCustomTarget = false;

	/** ���� ��ȿ�� Ÿ�� ��ġ�� ��ȯ�ϴ� ���� ���� */
	bool GetTargetLocation(FVector& OutTargetLocation) const;
};