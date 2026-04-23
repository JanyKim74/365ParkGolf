#include "ResultVideoWidget.h"
#include "FileMediaSource.h"
#include "Misc/Paths.h"                          // FPaths
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"

#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "MediaSource.h"
#include "MediaSoundComponent.h"

#include "../InGameMode.h"
#include "../GolfPlayerManager.h"
#include "../GolfPlayer.h"

void UResultVideoWidget::NativeConstruct()
{
    if (Button_Next)
    {
        Button_Next->OnClicked.AddDynamic(this, &UResultVideoWidget::OnVideoButtonClicked);
    }

    if (MediaPlayer)
    {
        // 미디어 열기 완료 시점에 호출되는 델리게이트
        MediaPlayer->OnMediaOpened.AddDynamic(this, &UResultVideoWidget::HandleMediaOpened);
    }
}

void UResultVideoWidget::OnVideoButtonClicked()
{
    AInGameMode* GameMode = Cast<AInGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
    UGameplayStatics::SetGamePaused(GameMode->GetWorld(), false);

    if (MediaPlayer->IsPlaying())
    {
        MediaPlayer->Pause();
        MediaPlayer->Rewind();
    }

    SetVisibility(ESlateVisibility::Collapsed);
}

void UResultVideoWidget::ChangeTextBlockPosition(float Y)
{
    UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(CanvasPanel_TextBlock->Slot);
    float XPosition = CanvasSlot->GetPosition().X;

    CanvasSlot->SetPosition(FVector2D(XPosition, Y));
}

void UResultVideoWidget::HandleMediaOpened(FString OpendUrl)
{
    if (AInGameMode* GameMode = Cast<AInGameMode>(UGameplayStatics::GetGameMode(GetWorld())))
    {
        GameMode->ResultVideoWidgetInstance->SetVisibility(ESlateVisibility::Visible);
        MediaPlayer->Seek(FTimespan::Zero());
        MediaPlayer->Play();
    }
}

void UResultVideoWidget::ChangeVideoPathAndPlay(const FString& NewFilePath)
{
    if (!MediaPlayer || !MediaSource)
    {
        return;
    }

    ChangeTextBlockPosition(TextHeight);

    if (AInGameMode* GameMode = Cast<AInGameMode>(UGameplayStatics::GetGameMode(GetWorld())))
    {
        FString Name = GameMode->PlayerManager->GetPlayers()[GameMode->CurrentPlayerIndex]->PlayerInfo.NickName;
        FString CourseName = GameMode->MapInfo.MapName;
        FString HoleNumber = FString::Printf(TEXT("Hole %d"), GameMode->CurrentHole);
        TextBlock_Name->SetText(FText::FromString(Name));
        TextBlock_CourseName->SetText(FText::FromString(CourseName));
        TextBlock_HoleNumber->SetText(FText::FromString(HoleNumber));
    }

    // MediaSource가 실제로는 UFileMediaSource인지 확인
    if (UFileMediaSource* FileSource = Cast<UFileMediaSource>(MediaSource))
    {

        // 프로젝트 Content/Movies 폴더 기준으로 경로 생성
        const FString MoviesDir = FPaths::ProjectContentDir() / TEXT("Movies");

        // 파일명 조합 (예: Result_1.mp4, Result_2.mp4)
        const FString FileName = NewFilePath;

        const FString FullPath = FPaths::Combine(MoviesDir, FileName);

        // 1) 파일 경로 변경
        FileSource->SetFilePath(FullPath);

        // 2) 변경된 경로로 다시 열기
        MediaPlayer->OpenSource(FileSource);
    }
}
