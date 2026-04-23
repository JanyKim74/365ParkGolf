// Fill out your copyright notice in the Description page of Project Settings.


#include "BallParticleManager.h"

#include "TimerManager.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "UObject/ConstructorHelpers.h"
#include "Structs/DataTableStruct.h"


// Sets default values
UBallParticleManager::UBallParticleManager()
{
	static ConstructorHelpers::FObjectFinder<UDataTable> DT(
		TEXT("DataTable'/Game/DATA/InGame/DT_BallParticle.DT_BallParticle'")
	);

	if (DT.Succeeded())
		BallParticleDT = DT.Object;

	UE_LOG(LogTemp, Log, TEXT("BallParticleManager Construct Success"));
}


void UBallParticleManager::Init()
{
    BallParticleMap.Reset();

    if (!BallParticleDT) return;

	BallParticleDT->ForeachRow<FBallParticle>(TEXT("Init"),
		[&](const FName& RowName, const FBallParticle& Row)
		{
			UE_LOG(LogTemp, Log, TEXT("BallParticleManager Add Particle Success : %s"), *Row.ParticleName);
			BallParticleMap.Add(Row.ParticleName, Row.BP_Particle);
		});
}

AActor* UBallParticleManager::SpawnParticle(UObject* WorldContextObject, FString ParticleName, FVector SpawnLocation)
{
	UWorld* World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject);

	if (!World) return nullptr;

	TSubclassOf<AActor> SpawnParticleClass = *BallParticleMap.Find(ParticleName);

	AActor* SpawnParticle = World->SpawnActor<AActor>(SpawnParticleClass, SpawnLocation, FRotator::ZeroRotator);
	return SpawnParticle;
}
