#pragma once
#include "CoreMinimal.h"
#include "GolfDataStructures.h"
#include "UObject/NoExportTypes.h"
#include "GolfStateMachine.generated.h"

/**
 * 
 */
UCLASS()
class PARKDAY_API UGolfStateMachine : public UObject
{
	GENERATED_BODY()
	
public:
    void SetGameState(EGameState NewState);
    void SetPlayerState(EPlayerState NewState);
    void SetSensorState(ESensorState NewState);
    void SetBallState(EBallState NewState);
    void Update(float DeltaTime);

    EGameState GetGameState() const { return CurrentGameState; }
    EPlayerState GetPlayerState() const { return CurrentPlayerState; }
    ESensorState GetSensorState() const { return CurrentSensorState; }
    EBallState GetBallState() const { return CurrentBallState; }

private:
    EGameState CurrentGameState;
    EPlayerState CurrentPlayerState;
    ESensorState CurrentSensorState;
    EBallState CurrentBallState;
};