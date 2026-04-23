// Fill out your copyright notice in the Description page of Project Settings.


#include "ReadyBillboard.h"

#include "GolfPlayer.h"
#include "InGameMode.h"
#include "Components/BillboardComponent.h"

// Sets default values
AReadyBillboard::AReadyBillboard()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	Billboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));

	static ConstructorHelpers::FObjectFinder<UTexture2D> BillboardImage(
		TEXT("Texture2D'/Game/UMG/Resources/Images/InGame_365/365_ready_mark.365_ready_mark'")
	);

	static ConstructorHelpers::FObjectFinder<UTexture2D> BillboardImage2(
		TEXT("Texture2D'/Game/UMG/Resources/Images/InGame_365/365_noready_mark.365_noready_mark'")
	);

	FVector BillSize = FVector(0.5f, 0.5f, 0.5f);

	if (BillboardImage.Succeeded())
	{
		ReadyImage = BillboardImage.Object;

		Billboard->SetWorldScale3D(BillSize);
		Billboard->SetupAttachment(RootComponent);
		Billboard->SetHiddenInGame(false);
		Billboard->SetVisibility(false);
		Billboard->bIsScreenSizeScaled = true;  // 거리 따라 화면 크기 유지());
	}

	if (BillboardImage2.Succeeded())
	{
		NoReadyImage = BillboardImage2.Object;

		Billboard->SetWorldScale3D(BillSize);
		Billboard->SetSprite(NoReadyImage);
		Billboard->SetupAttachment(RootComponent);
		Billboard->SetHiddenInGame(false);
		Billboard->SetVisibility(false);
		Billboard->bIsScreenSizeScaled = true;  // 거리 따라 화면 크기 유지());
	}

	SetActorEnableCollision(false);
}

void AReadyBillboard::BeginPlay()
{
	Super::BeginPlay();
	GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());

}

// Called every frame
void AReadyBillboard::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (GM)
	{
		if (GM->GetCurrentGameState() == EGameState::Game_Play)
		{
			if (GM->GetCurrentTurnGolfBall())
			{
				FVector BallLocation = GM->GetCurrentTurnGolfBall()->GetActorLocation();
				SetActorLocation(BallLocation + FVector(0.f, 0.f, 10.f));
			}
		}
	}
}