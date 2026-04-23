// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BallParticleManager.generated.h"

class UDataTable;

UCLASS()
class PARKDAY_API UBallParticleManager : public UObject
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	UBallParticleManager();

	UPROPERTY()
		UDataTable* BallParticleDT;

	UPROPERTY()
		TMap<FString, TSubclassOf<AActor>> BallParticleMap;

	UFUNCTION()
		void Init();

	UFUNCTION(meta = (WorldContext = "WorldContextObject"))
		AActor* SpawnParticle(UObject* WorldContextObject, FString ParticleName, FVector SpawnLocation);
};
