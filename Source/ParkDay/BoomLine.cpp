#include "BoomLine.h"

#include "GolfBall.h"
#include "InGameMode.h"


// Sets default values
ABoomLine::ABoomLine()
{
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ABoomLine::BeginPlay()
{
	Super::BeginPlay();
}


// Called every frame
void ABoomLine::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

