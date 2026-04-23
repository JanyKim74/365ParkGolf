#include "GolfStateMachine.h"

void UGolfStateMachine::SetGameState(EGameState NewState)
{
    CurrentGameState = NewState;
}

void UGolfStateMachine::SetPlayerState(EPlayerState NewState)
{
    CurrentPlayerState = NewState;
}

void UGolfStateMachine::SetSensorState(ESensorState NewState)
{
    CurrentSensorState = NewState;
}

void UGolfStateMachine::SetBallState(EBallState NewState)
{
    CurrentBallState = NewState;
}

void UGolfStateMachine::Update(float DeltaTime)
{
    // ���º� ������Ʈ ���� (�ʿ� �� �߰�)
}