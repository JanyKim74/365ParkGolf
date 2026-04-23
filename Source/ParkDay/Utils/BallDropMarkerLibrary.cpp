#include "BallDropMarkerLibrary.h"
#include "../BallDropMarkerActor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"

ABallDropMarkerActor* UBallDropMarkerLibrary::SpawnDropBillboardMarker(
	UObject* WorldContextObject,
	TSubclassOf<ABallDropMarkerActor> MarkerClass,
	const FVector& DesiredWorldLocation,
	UTexture2D* BillboardTexture,
	bool bSnapToGround,
	float TraceUp,
	float TraceDown,
	float GroundOffset,
	float LifeTime,
	float ScreenSize,
	float UniformScale
)
{
	if (!WorldContextObject || !MarkerClass)
	{
		return nullptr;
	}

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABallDropMarkerActor* Marker = World->SpawnActor<ABallDropMarkerActor>(
		MarkerClass,
		DesiredWorldLocation,
		FRotator::ZeroRotator,
		Params
		);

	if (!Marker)
	{
		return nullptr;
	}

	// 텍스처 지정(없으면 마커 내부 DefaultTexture가 사용됨)
	if (BillboardTexture)
	{
		Marker->SetMarkerTexture(BillboardTexture);
	}

	// 크기 옵션(음수면 "변경하지 않음")
	if (ScreenSize > 0.f)
	{
		Marker->SetMarkerScreenSize(ScreenSize);
	}
	if (UniformScale > 0.f)
	{
		Marker->SetMarkerWorldScale(UniformScale);
	}

	// 위치 갱신(지면 스냅 포함)
	Marker->UpdateMarker(DesiredWorldLocation, bSnapToGround, TraceUp, TraceDown, GroundOffset);

	// 수명
	if (LifeTime > 0.f)
	{
		Marker->SetLifeSpan(LifeTime);
	}

	return Marker;
}

void UBallDropMarkerLibrary::UpdateDropBillboardMarker(
	ABallDropMarkerActor* Marker,
	const FVector& DesiredWorldLocation,
	bool bSnapToGround,
	float TraceUp,
	float TraceDown,
	float GroundOffset,
	float LifeTime,
	float ScreenSize,
	float UniformScale
)
{
	if (!Marker)
	{
		return;
	}

	// 크기 옵션(음수면 "변경하지 않음")
	if (ScreenSize > 0.f)
	{
		Marker->SetMarkerScreenSize(ScreenSize);
	}
	if (UniformScale > 0.f)
	{
		Marker->SetMarkerWorldScale(UniformScale);
	}

	Marker->UpdateMarker(DesiredWorldLocation, bSnapToGround, TraceUp, TraceDown, GroundOffset);

	if (LifeTime > 0.f)
	{
		// "업데이트할 때마다 수명을 리셋"하고 싶을 때 유용
		Marker->SetLifeSpan(LifeTime);
	}
}

void UBallDropMarkerLibrary::SetDropBillboardMarkerTexture(
	ABallDropMarkerActor* Marker,
	UTexture2D* BillboardTexture
)
{
	if (!Marker || !BillboardTexture)
	{
		return;
	}

	Marker->SetMarkerTexture(BillboardTexture);
}