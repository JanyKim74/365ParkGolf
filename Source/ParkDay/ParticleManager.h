// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ParticleManager.generated.h"

class AInGameMode;

UCLASS()
class PARKDAY_API AParticleManager : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AParticleManager();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

public:
	void PlayChanceFX(int32 inPlayerShotCount);
	void StopChanceFX();
public:
	UPROPERTY(EditDefaultsOnly, Category = "FX") float HeightOffset = 80.f;


private:
	AInGameMode* GM;
	UPROPERTY()
	AActor* ChanceFXActor = nullptr;
};
