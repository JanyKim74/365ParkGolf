#include "GolfActorFinder.h"
#include "Kismet/GameplayStatics.h"

void UGolfActorFinder::FindHoleActors(int32 CurrentHole)
{
    FString TeeActorName = FString::Printf(TEXT("Tee_hole%d"), CurrentHole);
    FString CupActorName = FString::Printf(TEXT("Cup_hole%d"), CurrentHole);
    FString TeeTag = FString::Printf(TEXT("Tee_hole%d"), CurrentHole);
    FString CupTag = FString::Printf(TEXT("Cup_hole%d"), CurrentHole);

    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), FoundActors);
    for (AActor* Actor : FoundActors)
    {
        if (Actor)
        {
            FString ActorName = Actor->GetName();
            if (ActorName.Equals(TeeActorName, ESearchCase::CaseSensitive))
            {
                HoleTeeActor = Actor;
            }
            else if (ActorName.Equals(CupActorName, ESearchCase::CaseSensitive))
            {
                HoleCupActor = Actor;
            }
        }
    }

    if (!HoleTeeActor)
    {
        for (AActor* Actor : FoundActors)
        {
            if (Actor && Actor->ActorHasTag(FName(*TeeTag)))
            {
                HoleTeeActor = Actor;
                break;
            }
        }
    }
    if (!HoleCupActor)
    {
        for (AActor* Actor : FoundActors)
        {
            if (Actor && Actor->ActorHasTag(FName(*CupTag)))
            {
                HoleCupActor = Actor;
                break;
            }
        }
    }

    // Fallback if actors not found
    if (!HoleTeeActor)
    {
        UE_LOG(LogTemp, Warning, TEXT("FindHoleActors: Tee_hole%d not found, creating default"), CurrentHole);
        HoleTeeActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
    }
    if (!HoleCupActor)
    {
        UE_LOG(LogTemp, Warning, TEXT("FindHoleActors: Cup_hole%d not found, creating default"), CurrentHole);
        HoleCupActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FVector(1000, 0, 0), FRotator::ZeroRotator);
    }
    UE_LOG(LogTemp, Log, TEXT("FindHoleActors: HoleTeeActor=%p, HoleCupActor=%p"), HoleTeeActor, HoleCupActor);
}