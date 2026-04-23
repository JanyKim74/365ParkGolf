#include "GolfGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "GolfPlayer.h"
#include "GolfPlayerController.h"
#include "JsonHandler.h"

AGolfGameMode::AGolfGameMode()
{
    // ����Ʈ Pawn�� ��Ʈ�ѷ� Ŭ���� ����
    DefaultPawnClass = AGolfPlayer::StaticClass();
    PlayerControllerClass = AGolfPlayerController::StaticClass();

    SaveFilePath = FPaths::ProjectSavedDir() + TEXT("GameInfo.json");
}

void AGolfGameMode::BeginPlay()
{
    Super::BeginPlay();

    // ����Ʈ �÷��̾� �ʱ�ȭ
    InitializeDefaultPlayers();

    // ���� ���� �� �޴� ���� ��ȯ
    SwitchToInGameMode();

    UE_LOG(LogTemp, Warning, TEXT("AGolfGameMode: BeginPlay() t"));
}

void AGolfGameMode::SwitchToMenuMode()
{
    // �޴� �� �ε�
    UGameplayStatics::OpenLevel(GetWorld(), TEXT("Minimal_Default"));
    UE_LOG(LogTemp, Warning, TEXT("GolfGameMode::SwitchMenuMode()- Succeed !"));
}

void AGolfGameMode::SwitchToInGameMode()
{
    // �ΰ��� �� �ε�
    UGameplayStatics::OpenLevel(GetWorld(), TEXT("SancheoneoPark"));
    UE_LOG(LogTemp, Warning, TEXT("GolfGameMode::SwitchToInGameMode()- SancheoneoPark !"));
}



void AGolfGameMode::InitializeDefaultPlayers()
{
    // ���� JSON ������ �ε� �õ�
    if (UJsonHandler::LoadGameInfoFromJson(GameInfo, SaveFilePath))
    {
        // JSON �����Ͱ� ������ �÷��̾� ����
        if (GameInfo.Players.Num() > 0)
        {
            UE_LOG(LogTemp, Log, TEXT("Loaded %d players from JSON"), GameInfo.Players.Num());
            return;
        }
    }

    // JSON �����Ͱ� ���ų� �÷��̾ ������ ����Ʈ �÷��̾� ����
    GameInfo.Players.Empty();

    // �ִ� 6���� ����Ʈ �÷��̾� �߰�
    for (int32 i = 0; i < 6; ++i)
    {
        FPlayerInfo DefaultPlayer;
        DefaultPlayer.ID = FString::Printf(TEXT("Player_%d"), i + 1);
        DefaultPlayer.NickName = FString::Printf(TEXT("Golfer_%d"), i + 1);
        DefaultPlayer.Level = 1;
        DefaultPlayer.Ranking = 0;
        DefaultPlayer.Point = 0;
        DefaultPlayer.Tee_Height = 25;
        DefaultPlayer.HandiCap = 0;
        DefaultPlayer.RoundCount = 0;
        DefaultPlayer.Avg_Distance = 200.0f;
        DefaultPlayer.Last_Date = FDateTime::Now().ToUnixTimestamp();
        DefaultPlayer.Img_Url = TEXT("");
        DefaultPlayer.TotalScore = 0;
        DefaultPlayer.HoleScores.Empty();


        // �÷��̾ ���� �� ���� ����
        switch (i)
        {
        case 0: DefaultPlayer.BallColor = FLinearColor::Red; break;
        case 1: DefaultPlayer.BallColor = FLinearColor::Blue; break;
        case 2: DefaultPlayer.BallColor = FLinearColor::Green; break;
        case 3: DefaultPlayer.BallColor = FLinearColor::Yellow; break;
        case 4: DefaultPlayer.BallColor = FLinearColor::White; break;
        case 5: DefaultPlayer.BallColor = FLinearColor::Gray; break;
        default: DefaultPlayer.BallColor = FLinearColor::White; break;
        }

        GameInfo.Players.Add(DefaultPlayer);
        UE_LOG(LogTemp, Log, TEXT("Initialized default player: %s"), *DefaultPlayer.NickName);
    }

    // �ʱ�ȭ�� �����͸� JSON�� ����
    UJsonHandler::SaveGameInfoToJson(GameInfo, SaveFilePath);
}