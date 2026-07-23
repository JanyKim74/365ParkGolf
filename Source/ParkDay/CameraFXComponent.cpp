#include "CameraFXComponent.h"

#include "CameraManager.h"
#include "InGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "GolfPlayerController.h"
#include "GolfPlayerManager.h"
#include "Structs/DataTableStruct.h"
#include "InGameMode.h"

void UCameraFXComponent::BeginPlay()
{
	Super::BeginPlay();

    GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
}


void UCameraFXComponent::PlayHoleInFX(int32 inPlayerShotCount)
{
    if (AGolfBall* Ball = Cast<AGolfBall>(GM->PlayerManager->GetPlayerBalls()[GM->CurrentPlayerIndex]))
    {
        int32 CurrentHole = GM->CurrentHole-1;
        int32 ParCount = GM->GameInfo.SelectedMap.ParScores[CurrentHole];
        int32 FinalScore = inPlayerShotCount - ParCount;
        if (inPlayerShotCount == 100)
            FinalScore = inPlayerShotCount;

        if (IsValid(GM->HoleInParticleMap[FinalScore]))
        {
            if (TSubclassOf<AActor> ParticleClass = GM->HoleInParticleMap[FinalScore])
            {
                if (inPlayerShotCount != 1) //홀인원이 아닐시
                {
                    Ball->LinkedCameraManager->SpawnInFrontOfCamera(GetWorld(), ParticleClass, 50.f);
                }
                else
                {
                    ParticleClass = GM->HoleInParticleMap[-4];
                    GetWorld()->SpawnActor<AActor>(ParticleClass.Get(), Ball->LinkedCameraManager->GetCurrentHolecupPosition(), FRotator::ZeroRotator);
                }
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("UCameraFXComponent::PlayHoleInFX(int32 Score) : Ball is null "));
    }
}

void UCameraFXComponent::PlayChanceFX(int32 inPlayerShotCount)
{
    //inPlayerShotCount++;
    //if (AGolfBall* Ball = Cast<AGolfBall>(GM->PlayerManager->GetPlayerBalls()[GM->CurrentPlayerIndex]))
    //{
    //    int32 CurrentHole = GM->CurrentHole-1;
    //    int32 ParCount = GM->GameInfo.SelectedMap.ParScores[CurrentHole];
    //    int32 FinalScore = inPlayerShotCount - ParCount;

    //    UE_LOG(LogTemp, Error, TEXT("%d, %d, %d"), CurrentHole, ParCount, FinalScore);
    //    TSubclassOf<AActor> ParticleClass = GM->ChanceParticleMap[FinalScore];

    //    if (IsValid(ParticleClass))
    //    {
    //        Ball->SpawnBallLocation(GetWorld(), ParticleClass, HeightOffset);
    //    }
    //    else
    //    {
    //        UE_LOG(LogTemp, Warning, TEXT("UCameraFXComponent::PlayChanceFX(int32 inPlayerShotCount) : ParticleClass is null"));
    //    }
    //}
    //else
    //{
    //    UE_LOG(LogTemp, Error, TEXT("UCameraFXComponent::PlayHoleInFX(int32 Score) : Ball is null "));
    //}
}