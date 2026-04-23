#pragma once
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GolfActorFinder.generated.h"

/**
 * 
 */
UCLASS()
class PARKDAY_API UGolfActorFinder : public UObject
{
	GENERATED_BODY()
	
public:
    void FindHoleActors(int32 CurrentHole);
    AActor* GetHoleTeeActor() const { return HoleTeeActor; }
    AActor* GetHoleCupActor() const { return HoleCupActor; }

private:
    AActor* HoleTeeActor;
    AActor* HoleCupActor;
};