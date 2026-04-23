#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BallDropMarkerLibrary.generated.h"

class ABallDropMarkerActor;
class UMaterialInterface;

UCLASS()
class PARKDAY_API UBallDropMarkerLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/**
	 * 월드에 드랍 위치 빌보드 마커를 생성해서 표시
	 * @param WorldContextObject 월드 컨텍스트
	 * @param MarkerClass 스폰할 마커 클래스(비어있으면 nullptr 반환)
	 * @param DesiredWorldLocation 표시하고 싶은 위치(스냅 옵션에 따라 지면으로 보정됨)
	 * @param BillboardTexture 빌보드 이미지(없으면 마커의 DefaultTexture 사용)
	 */
	UFUNCTION(BlueprintCallable, Category="DropMarker", meta=(WorldContext="WorldContextObject"))
	static ABallDropMarkerActor* SpawnDropBillboardMarker(
		UObject* WorldContextObject,
		TSubclassOf<ABallDropMarkerActor> MarkerClass,
		const FVector& DesiredWorldLocation,
		UTexture2D* BillboardTexture,
		bool bSnapToGround = true,
		float TraceUp = 200.f,
		float TraceDown = 2000.f,
		float GroundOffset = 5.f,
		float LifeTime = 0.f,
		float ScreenSize = -1.f,
		float UniformScale = -1.f
	);

	/**
	 * 기존 마커를 위치 갱신(재스폰 없이 이동)
	 */
	UFUNCTION(BlueprintCallable, Category="DropMarker")
	static void UpdateDropBillboardMarker(
		ABallDropMarkerActor* Marker,
		const FVector& DesiredWorldLocation,
		bool bSnapToGround = true,
		float TraceUp = 200.f,
		float TraceDown = 2000.f,
		float GroundOffset = 5.f,
		float LifeTime = 0.f,
		float ScreenSize = -1.f,
		float UniformScale = -1.f
	);

	/**
	 * 빌보드 텍스처만 교체
	 */
	UFUNCTION(BlueprintCallable, Category="DropMarker")
	static void SetDropBillboardMarkerTexture(
		ABallDropMarkerActor* Marker,
		UTexture2D* BillboardTexture
	);
};
