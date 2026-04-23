// Fill out your copyright notice in the Description page of Project Settings.


#include "ParticleManager.h"

#include "GolfBall.h"
#include "GolfPlayerManager.h"
#include "InGameMode.h"


// Sets default values
AParticleManager::AParticleManager()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AParticleManager::BeginPlay()
{
	Super::BeginPlay();
    GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
}

// Called every frame
void AParticleManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}


void AParticleManager::PlayChanceFX(int32 inPlayerShotCount)
{
    if (GM->IsStrokeMode())
    {
        StopChanceFX();

        //inPlayerShotCount++;
        if (AGolfBall* Ball = GM->GetCurrentTurnGolfBall())
        {
            int32 CurrentHole = GM->CurrentHole - 1;
            int32 ParCount = GM->GameInfo.SelectedMap.ParScores[CurrentHole];
            int32 FinalScore = inPlayerShotCount - ParCount;

            if (FinalScore <= 0)
            {
                if (GM->ChanceParticleMap.Contains(FinalScore))
                {
                    TSubclassOf<AActor> ParticleClass = GM->ChanceParticleMap[FinalScore];

                    if (IsValid(ParticleClass))
                    {
                        ChanceFXActor = Ball->SpawnBallLocation(GetWorld(), ParticleClass, HeightOffset);
                    }
                    else
                    {
                        UE_LOG(LogTemp, Warning, TEXT("UCameraFXComponent::PlayChanceFX(int32 inPlayerShotCount) : ParticleClass is null"));
                    }
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("Invalid Score ChanceParticle"));
                }
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("UCameraFXComponent::PlayHoleInFX(int32 Score) : Ball is null "));
        }
    }
}

void AParticleManager::StopChanceFX()
{
	if (IsValid(ChanceFXActor))
	{
		ChanceFXActor->Destroy();
		ChanceFXActor = nullptr;
	}
}
