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

    // ⭐ MediaPlayer/MediaTexture가 이 시점에 아직 null일 수 있으므로(외부에서 나중에 할당하는 흐름이면
    //    여기서 조용히 스킵되어 Brush가 영원히 연결 안 됨) - 별도 헬퍼로 빼서 ChangeVideoPathAndPlay()에서도 재시도
    EnsureMediaBrushBound();
}

void UResultVideoWidget::EnsureMediaBrushBound()
{
    if (!MediaPlayer || !MediaTexture || !Image_Video)
    {
        UE_LOG(LogTemp, Warning, TEXT("EnsureMediaBrushBound: MediaPlayer/MediaTexture/Image_Video 중 NULL 있음 - 나중에 재시도 필요"));
        return;
    }

    // ⭐ 이전엔 bMediaBrushBound 플래그로 "한 번만" 실행되게 막았는데, 이 플래그가 원인 불명으로
    //    최초 호출 시점부터 true로 읽히는 문제가 있었습니다 (그 결과 아래 바인딩 코드가 세션 내내
    //    단 한 번도 실행되지 않아 화면에 영상이 안 보이는 근본 원인이었습니다).
    //    SetMediaPlayer/SetBrush는 여러 번 호출해도 안전(idempotent)한 작업이라
    //    플래그로 막지 않고 매번 확실하게 재적용합니다.
    MediaTexture->SetMediaPlayer(MediaPlayer);
    MediaTexture->UpdateResource();  // ← 이게 없으면 프레임 수신 불가

    FSlateBrush Brush;
    Brush.SetResourceObject(MediaTexture);
    Brush.ImageSize = FVector2D(1920, 1080);
    Brush.DrawAs = ESlateBrushDrawType::Image;
    Image_Video->SetBrush(Brush);

    UE_LOG(LogTemp, Error, TEXT("  Brush 연결 완료 (MediaTexture=%s -> Image_Video)"), *MediaTexture->GetName());

    if (!SC)
    {
        CreateAndAttachMediaSound();
    }
}

//void UResultVideoWidget::OnVideoButtonClicked()
//{
//    GetWorld()->GetTimerManager().ClearTimer(TestHandle);
//    GetWorld()->GetTimerManager().ClearTimer(CloseDelayTimer);  // ⭐ 빠른 연속 클릭 시 지연된 OpenSource가 나중에 튀어나오는 것 방지
//
//    if (AInGameMode* GameMode = Cast<AInGameMode>(UGameplayStatics::GetGameMode(GetWorld())))
//        UGameplayStatics::SetGamePaused(GameMode->GetWorld(), false);
//
//    if (MediaPlayer && (MediaPlayer->IsPlaying() || MediaPlayer->IsPaused()))
//        MediaPlayer->Close();
//
//    SetVisibility(ESlateVisibility::Collapsed);
//}

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

    // ⭐ NativeConstruct 시점에 MediaPlayer/MediaTexture가 null이라 브러시 연결이 스킵됐을 경우를 대비한 재시도
    EnsureMediaBrushBound();


    UE_LOG(LogTemp, Warning, TEXT("ChangeVideoPathAndPlay: %s"), *NewFilePath);


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

    if (MediaTexture)
        MediaTexture->UpdateResource();

    if (!MediaPlayer->IsPlaying())
    {
        const bool bPlayResult = MediaPlayer->Play();
        UE_LOG(LogTemp, Log, TEXT("  Explicit Play() called, result=%d"), bPlayResult);
    }

    // ★ 재생 시작 후 0.5초마다 진행 상태를 로그로 확인 (진단용)
    FTimerManager& TimerManager = GetWorld()->GetTimerManager();
    TimerManager.SetTimer(TestHandle, [this]()
        {
            if (!IsValid(this) || !IsValid(MediaPlayer)) return;

            bool bIsPaused = UGameplayStatics::IsGamePaused(GetWorld());

            UE_LOG(LogTemp, Log, TEXT("T=%.3f Playing=%d IsPaused=%d"),
                MediaPlayer->GetTime().GetTotalSeconds(),
                MediaPlayer->IsPlaying(),
                bIsPaused ? 1 : 0);
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

        // ★ 핵심: bNeverTick 대신 아래 함수를 사용하여 일시정지 중에도 틱이 돌도록 설정합니다.
        SC->SetTickableWhenPaused(true);

        SC->SetMediaPlayer(MediaPlayer);
        SC->RegisterComponentWithWorld(World);
    }
    else
    {
        SC->SetMediaPlayer(MediaPlayer);
        SC->SetTickableWhenPaused(true); // 혹시 기존에 생성되어 있었다면 여기서도 설정
    }
}


void UResultVideoWidget::OnVideoButtonClicked()
{
    GetWorld()->GetTimerManager().ClearTimer(TestHandle);
    GetWorld()->GetTimerManager().ClearTimer(CloseDelayTimer);

    if (AInGameMode* GameMode = Cast<AInGameMode>(UGameplayStatics::GetGameMode(GetWorld())))
        UGameplayStatics::SetGamePaused(GameMode->GetWorld(), false);

    if (MediaPlayer && (MediaPlayer->IsPlaying() || MediaPlayer->IsPaused()))
        MediaPlayer->Close();

    SetVisibility(ESlateVisibility::Collapsed);

    // ⭐ 추가: 버튼 클릭으로 영상 종료 → 대기 중이던 다음 스테이트 진행 트리거
    OnResultVideoClosed.Broadcast();
}