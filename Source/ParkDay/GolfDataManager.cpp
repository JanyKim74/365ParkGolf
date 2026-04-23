#include "GolfDataManager.h"
#include "JsonHandler.h"

void UGolfDataManager::LoadGameInfo(const FString& FilePath)
{
    if (!FPaths::FileExists(FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("GameInfo file does not exist: %s, initializing default"), *FilePath);
        GameInfo = FGameInfo();
        //GameInfo.CurrentHole = 1;
        GameInfo.GameOptions.Concede_Distance = 100.0f;
        FPlayerInfo DefaultPlayer;
        DefaultPlayer.ID = TEXT("Player_1");
        DefaultPlayer.NickName = TEXT("Golfer_1");
        DefaultPlayer.BallColor = FLinearColor::Red;
        GameInfo.Players.Add(DefaultPlayer);
        SaveGameInfo(FilePath);
        return;
    }
    if (!UJsonHandler::LoadGameInfoFromJson(GameInfo, FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load GameInfo from %s, initializing default"), *FilePath);
        GameInfo = FGameInfo();
        GameInfo.CurrentHole = 1;
        GameInfo.GameOptions.Concede_Distance = 100.0f;
        FPlayerInfo DefaultPlayer;
        DefaultPlayer.ID = TEXT("Player_1");
        DefaultPlayer.NickName = TEXT("Golfer_1");
        DefaultPlayer.BallColor = FLinearColor::Red;
        GameInfo.Players.Add(DefaultPlayer);
        SaveGameInfo(FilePath);
    }


    // ��� �÷��̾��� ID Ȯ�� �� ����
    for (int32 i = 0; i < GameInfo.Players.Num(); ++i)
    {
        if (GameInfo.Players[i].ID.IsEmpty())
        {
            GameInfo.Players[i].ID = FString::Printf(TEXT("Player_%d"), i + 1);
            GameInfo.Players[i].NickName = FString::Printf(TEXT("Golfer_%d"), i + 1);
            UE_LOG(LogTemp, Error, TEXT("LoadGameInfo: Player %d isNot Defaults ID: %s"), i, *GameInfo.Players[i].ID);
        }
    }


    UE_LOG(LogTemp, Log, TEXT("Loaded GameInfo with %d players"), GameInfo.Players.Num());
}
void UGolfDataManager::SaveGameInfo(const FString& FilePath)
{
    UJsonHandler::SaveGameInfoToJson(GameInfo, FilePath);
}