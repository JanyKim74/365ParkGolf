#include "BallDropMarkerActor.h"
#include "Components/DecalComponent.h"
#include "Components/BillboardComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

ABallDropMarkerActor::ABallDropMarkerActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Billboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	Billboard->SetupAttachment(Root);

	// 기본값들
	Billboard->bHiddenInGame = false;
	Billboard->bIsScreenSizeScaled = true;  // 화면 크기 기반으로 유지
	Billboard->ScreenSize = DefaultScreenSize;

	SetActorEnableCollision(false);
}

void ABallDropMarkerActor::SetMarkerTexture(UTexture2D* InTexture)
{
	if (!Billboard) return;
	if (!InTexture) return;

	Billboard->SetSprite(InTexture);
}

void ABallDropMarkerActor::SetMarkerWorldScale(float UniformScale)
{
	SetActorScale3D(FVector(UniformScale));
}

void ABallDropMarkerActor::SetMarkerScreenSize(float InScreenSize)
{
	if (!Billboard) return;
	Billboard->ScreenSize = InScreenSize;
	Billboard->bIsScreenSizeScaled = true;
}

void ABallDropMarkerActor::BeginPlay()
{
	Super::BeginPlay();

	// 기본 텍스처 적용(스폰 후 한번도 안 넣었을 때)
	if (DefaultTexture)
	{
		Billboard->SetSprite(DefaultTexture);
	}
}

void ABallDropMarkerActor::UpdateMarker(
	const FVector& DesiredWorldLocation,
	bool bSnapToGround,
	float TraceUp,
	float TraceDown,
	float GroundOffset
)
{
	// 기본 텍스처 적용(스폰 후 한번도 안 넣었을 때)
	if (Billboard && !Billboard->Sprite && DefaultTexture)
	{
		Billboard->SetSprite(DefaultTexture);
	}

	// 기본 스케일 적용
	SetActorScale3D(FVector(DefaultUniformScale));

	FVector FinalLoc = DesiredWorldLocation;

	if (bSnapToGround)
	{
		const FVector Start = DesiredWorldLocation + FVector(0, 0, TraceUp);
		const FVector End = DesiredWorldLocation - FVector(0, 0, TraceDown);

		FHitResult Hit;
		TArray<AActor*> Ignore;
		const bool bHit = UKismetSystemLibrary::LineTraceSingle(
			this,
			Start,
			End,
			UEngineTypes::ConvertToTraceType(ECC_Visibility),
			false,
			Ignore,
			EDrawDebugTrace::None,
			Hit,
			true
		);

		if (bHit)
		{
			FinalLoc = Hit.ImpactPoint + Hit.ImpactNormal * GroundOffset;
		}
	}

	SetActorLocation(FinalLoc);

	// Billboard는 기본적으로 카메라를 바라보므로 보통 회전은 건드리지 않습니다.
}