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
    if (!MediaPlayer) return;

    // ✅ 위젯을 반드시 먼저 Visible로 설정
    SetVisibility(ESlateVisibility::Visible);

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

    FString AssetName = FPaths::GetBaseFilename(NewFilePath);
    const FString AssetPath = FString::Printf(TEXT("/Game/Movies/%s.%s"), *AssetName, *AssetName);

    UE_LOG(LogTemp, Log, TEXT("📂 Loading MediaSource: %s"), *AssetPath);

    UFileMediaSource* LoadedSource = Cast<UFileMediaSource>(
        StaticLoadObject(UFileMediaSource::StaticClass(), nullptr, *AssetPath)
    );

    if (!LoadedSource)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to load MediaSource: %s"), *AssetPath);
        return;
    }

    // ✅ 중복 바인딩 방지 후 재바인딩
    MediaPlayer->OnMediaOpened.RemoveAll(this);
    MediaPlayer->OnMediaOpenFailed.RemoveAll(this);
    MediaPlayer->OnMediaOpened.AddDynamic(this, &UResultVideoWidget::OnResultMediaOpened);
    MediaPlayer->OnMediaOpenFailed.AddDynamic(this, &UResultVideoWidget::OnResultMediaOpenFailed);

    // ✅ 이전 재생 중이면 닫고 새로 열기
    if (MediaPlayer->IsPlaying() || MediaPlayer->IsPreparing())
    {
        MediaPlayer->Close();
    }

    MediaPlayer->OpenSource(LoadedSource);
    UE_LOG(LogTemp, Log, TEXT("✅ OpenSource called: %s"), *AssetPath);
}

void UResultVideoWidget::OnResultMediaOpened(FString OpenedUrl)
{
    if (!MediaPlayer) return;

    UE_LOG(LogTemp, Log, TEXT("✅ Media Opened: %s"), *OpenedUrl);

    // 오디오 트랙 선택
    const int32 NumAudioTracks = MediaPlayer->GetNumTracks(EMediaPlayerTrack::Audio);
    UE_LOG(LogTemp, Log, TEXT("🔊 Audio Tracks: %d"), NumAudioTracks);
    if (NumAudioTracks > 0)
    {
        MediaPlayer->SelectTrack(EMediaPlayerTrack::Audio, 0);
        UE_LOG(LogTemp, Log, TEXT("✅ Audio Track 0 selected"));
    }

    // ✅ 비디오 트랙 선택
    const int32 NumVideoTracks = MediaPlayer->GetNumTracks(EMediaPlayerTrack::Video);
    if (NumVideoTracks > 0)
    {
        MediaPlayer->SelectTrack(EMediaPlayerTrack::Video, 0);
    }

    // ✅ 한 프레임 뒤에 Play() 호출 (OpenSource 직후 즉시 Play는 실패할 수 있음)
    if (UWorld* World = GetWorld())
    {
        FTimerHandle PlayTimer;
        World->GetTimerManager().SetTimer(PlayTimer, [this]()
            {
                if (MediaPlayer && !MediaPlayer->IsPlaying())
                {
                    MediaPlayer->Play();
                    UE_LOG(LogTemp, Log, TEXT("▶️ MediaPlayer->Play() called"));
                }
            }, 0.05f, false);
    }
}

void UResultVideoWidget::OnResultMediaOpenFailed(FString FailedUrl)
{
    UE_LOG(LogTemp, Error, TEXT("❌ Media Open Failed: %s"), *FailedUrl);
}