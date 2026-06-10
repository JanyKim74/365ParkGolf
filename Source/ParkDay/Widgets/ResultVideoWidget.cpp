#include "ResultVideoWidget.h"
#include "FileMediaSource.h"
#include "Misc/Paths.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Kismet/GameplayStatics.h"

#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "MediaSource.h"
#include "MediaSoundComponent.h"

#include "../InGameMode.h"
#include "../GolfPlayerManager.h"
#include "../GolfPlayer.h"

void UResultVideoWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    MS_Eagle = LoadObject<UFileMediaSource>(nullptr, TEXT("/Game/Movies/Video_Eagle.Video_Eagle"));
    MS_Albatross = LoadObject<UFileMediaSource>(nullptr, TEXT("/Game/Movies/Video_Albatross.Video_Albatross"));
    MS_Holeinone = LoadObject<UFileMediaSource>(nullptr, TEXT("/Game/Movies/Video_Holeinone.Video_Holeinone"));
    MS_Victory = LoadObject<UFileMediaSource>(nullptr, TEXT("/Game/Movies/Video_victory.Video_victory"));

    UE_LOG(LogTemp, Log, TEXT("MediaSources Eagle:%s Albatross:%s Holeinone:%s Victory:%s"),
        MS_Eagle ? TEXT("OK") : TEXT("NULL"),
        MS_Albatross ? TEXT("OK") : TEXT("NULL"),
        MS_Holeinone ? TEXT("OK") : TEXT("NULL"),
        MS_Victory ? TEXT("OK") : TEXT("NULL"));
}

void UResultVideoWidget::NativeConstruct()
{
    Super::NativeConstruct();

    UE_LOG(LogTemp, Error, TEXT("=== NativeConstruct ==="));
    UE_LOG(LogTemp, Error, TEXT("  MediaPlayer  = %s"), MediaPlayer ? *MediaPlayer->GetName() : TEXT("NULL"));
    UE_LOG(LogTemp, Error, TEXT("  MediaTexture = %s"), MediaTexture ? *MediaTexture->GetName() : TEXT("NULL"));
    UE_LOG(LogTemp, Error, TEXT("  Image_Video  = %s"), Image_Video ? TEXT("OK") : TEXT("NULL"));

    if (Button_Next)
        Button_Next->OnClicked.AddDynamic(this, &UResultVideoWidget::OnVideoButtonClicked);

    if (!MediaPlayer || !MediaTexture)
        return;

    // ★ 반드시 쌍으로 호출
    MediaTexture->SetMediaPlayer(MediaPlayer);
    MediaTexture->UpdateResource();  // ← 이게 없으면 프레임 수신 불가

    if (Image_Video)
    {
        FSlateBrush Brush;
        Brush.SetResourceObject(MediaTexture);
        Brush.ImageSize = FVector2D(1920, 1080);
        Brush.DrawAs = ESlateBrushDrawType::Image;
        Image_Video->SetBrush(Brush);
        UE_LOG(LogTemp, Error, TEXT("  Brush 연결 완료"));
    }
}

void UResultVideoWidget::OnVideoButtonClicked()
{
    GetWorld()->GetTimerManager().ClearTimer(TestHandle);

    if (AInGameMode* GameMode = Cast<AInGameMode>(UGameplayStatics::GetGameMode(GetWorld())))
        UGameplayStatics::SetGamePaused(GameMode->GetWorld(), false);

    if (MediaPlayer && (MediaPlayer->IsPlaying() || MediaPlayer->IsPaused()))
        MediaPlayer->Close();

    SetVisibility(ESlateVisibility::Collapsed);
}

void UResultVideoWidget::ChangeTextBlockPosition(float Y)
{
    if (!CanvasPanel_TextBlock) return;
    UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(CanvasPanel_TextBlock->Slot);
    if (!CanvasSlot) return;
    CanvasSlot->SetPosition(FVector2D(CanvasSlot->GetPosition().X, Y));
}

void UResultVideoWidget::HandleMediaOpened(FString OpendUrl)
{
    // 미사용 (하위호환 유지)
}

void UResultVideoWidget::ChangeVideoPathAndPlay(const FString& NewFilePath)
{
    if (!MediaPlayer || !ResultPlaylist) return;

    const FString AssetName = FPaths::GetBaseFilename(NewFilePath);
    int32 PlaylistIndex = -1;

    if (AssetName == TEXT("Video_Eagle"))     PlaylistIndex = 0;
    else if (AssetName == TEXT("Video_Albatross")) PlaylistIndex = 1;
    else if (AssetName == TEXT("Video_Holeinone")) PlaylistIndex = 2;
    else if (AssetName == TEXT("Video_victory"))   PlaylistIndex = 3;

    if (PlaylistIndex < 0)
    {
        UE_LOG(LogTemp, Error, TEXT("Playlist 인덱스 없음: %s"), *AssetName);
        return;
    }

    UMediaSource* Source = ResultPlaylist->Get(PlaylistIndex);
    if (!IsValid(Source))
    {
        UE_LOG(LogTemp, Error, TEXT("Playlist[%d] Source NULL"), PlaylistIndex);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Playlist[%d] -> %s"), PlaylistIndex, *Source->GetName());

    // 텍스트 업데이트
    ChangeTextBlockPosition(TextHeight);
    if (AInGameMode* GameMode = Cast<AInGameMode>(UGameplayStatics::GetGameMode(GetWorld())))
    {
        TArray<AGolfPlayer*> Players = GameMode->PlayerManager->GetPlayers();
        if (Players.IsValidIndex(GameMode->CurrentPlayerIndex) && TextBlock_Name)
            TextBlock_Name->SetText(FText::FromString(
                Players[GameMode->CurrentPlayerIndex]->PlayerInfo.NickName));
        if (TextBlock_CourseName)
            TextBlock_CourseName->SetText(FText::FromString(GameMode->MapInfo.MapName));
        if (TextBlock_HoleNumber)
            TextBlock_HoleNumber->SetText(FText::FromString(
                FString::Printf(TEXT("Hole %d"), GameMode->CurrentHole)));
    }

    // 델리게이트 바인딩 (중복 방지)
    MediaPlayer->OnMediaOpened.RemoveAll(this);
    MediaPlayer->OnMediaOpenFailed.RemoveAll(this);
    MediaPlayer->OnMediaOpened.AddDynamic(this, &UResultVideoWidget::OnResultMediaOpened);
    MediaPlayer->OnMediaOpenFailed.AddDynamic(this, &UResultVideoWidget::OnResultMediaOpenFailed);

    // 재생 중이면 Close 후 딜레이 Open
    if (MediaPlayer->IsPlaying() || MediaPlayer->IsPreparing() || MediaPlayer->IsPaused())
    {
        MediaPlayer->Close();
        GetWorld()->GetTimerManager().SetTimer(CloseDelayTimer, [this, Source]()
            {
                if (IsValid(this) && IsValid(MediaPlayer))
                    MediaPlayer->OpenSource(Source);
            }, 0.1f, false);
    }
    else
    {
        MediaPlayer->OpenSource(Source);
    }

    SetVisibility(ESlateVisibility::Visible);
}

void UResultVideoWidget::OnResultMediaOpened(FString OpenedUrl)
{
    if (!MediaPlayer) return;
    UE_LOG(LogTemp, Log, TEXT("OnResultMediaOpened: %s"), *OpenedUrl);

    // ★ OpenSource 후 반드시 UpdateResource 재호출
    if (MediaTexture)
        MediaTexture->UpdateResource();

    // Play on Open=true — Play()/SelectTrack() 호출 없음

    GetWorld()->GetTimerManager().SetTimer(TestHandle, [this]()
        {
            if (!IsValid(this) || !IsValid(MediaPlayer)) return;
            UE_LOG(LogTemp, Log, TEXT("T=%.3f Playing=%d"),
                MediaPlayer->GetTime().GetTotalSeconds(),
                MediaPlayer->IsPlaying());
        }, 0.5f, true);
}

void UResultVideoWidget::OnResultMediaOpenFailed(FString FailedUrl)
{
    UE_LOG(LogTemp, Error, TEXT("OnResultMediaOpenFailed: %s"), *FailedUrl);
}

void UResultVideoWidget::CreateAndAttachMediaSound()
{
    if (!MediaPlayer) return;
    UWorld* World = GetWorld();
    if (!World) return;

    if (!SC)
    {
        SC = NewObject<UMediaSoundComponent>(this, UMediaSoundComponent::StaticClass());
        SC->bIsUISound = true;
        SC->SetMediaPlayer(MediaPlayer);
        SC->RegisterComponentWithWorld(World);
    }
    else
    {
        SC->SetMediaPlayer(MediaPlayer);
    }
}