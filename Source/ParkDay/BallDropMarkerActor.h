#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BallDropMarkerActor.generated.h"

class USceneComponent;
class UBillboardComponent;
class UMaterialInterface;
class UTexture2D;

UCLASS()
class PARKDAY_API ABallDropMarkerActor : public AActor
{
	GENERATED_BODY()

public:
	ABallDropMarkerActor();

	virtual void BeginPlay() override;

	/** 위치 표시(지면 스냅 포함) */
	UFUNCTION(BlueprintCallable, Category="DropMarker")
	void UpdateMarker(
		const FVector& DesiredWorldLocation,
		bool bSnapToGround = true,
		float TraceUp = 200.f,
		float TraceDown = 2000.f,
		float GroundOffset = 5.f
	);

	/** 빌보드 텍스처 교체 */
	UFUNCTION(BlueprintCallable, Category="DropMarker")
	void SetMarkerTexture(UTexture2D* InTexture);

	/** 월드 크기(미터 단위 느낌)로 조절하고 싶을 때 사용 */
	UFUNCTION(BlueprintCallable, Category="DropMarker")
	void SetMarkerWorldScale(float UniformScale);

	/** 화면에서 보이는 크기(스크린 사이즈 기반)로 조절하고 싶을 때 사용 */
	UFUNCTION(BlueprintCallable, Category="DropMarker")
	void SetMarkerScreenSize(float InScreenSize);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="DropMarker")
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="DropMarker")
	UBillboardComponent* Billboard;

	/** 에디터에서 기본 텍스처 지정 가능 */
	UPROPERTY(EditDefaultsOnly, Category="DropMarker")
	UTexture2D* DefaultTexture = nullptr;

	/** 기본 스케일 */
	UPROPERTY(EditDefaultsOnly, Category="DropMarker")
	float DefaultUniformScale = 1.0f;

	/** 기본 스크린 사이즈(값이 클수록 화면에서 크게 보임) */
	UPROPERTY(EditDefaultsOnly, Category="DropMarker")
	float DefaultScreenSize = 0.0025f;
};