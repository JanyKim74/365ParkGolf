#include "PlayerProfileWidget.h"
#include "../InGameMode.h"
#include "../GolfPlayerManager.h"

void UPlayerProfileWidget::NativeConstruct()
{
    Super::NativeConstruct();

    GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
}

bool UPlayerProfileWidget::CheckLastPlayer()
{
    if (bIsInGame)
        return (GM->PlayerManager->GetPlayers().Num() < 2 ||
            GM->PlayerManager->GetPlayerBalls().Num() < 2) ? true : false;
    else
        return false;
}