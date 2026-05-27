#include "IntroWidget.h"
#include "Components/Button.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "MediaSoundComponent.h"
#include "../../MenuGameMode.h"
#include "Components/SceneComponent.h"
#include "Components/EditableTextBox.h"
#include "ParkDay/Widgets/PasswordWidget.h"
#include "ParkDay/Utils/UtilLibrary.h"
#include "ParkDay/Utils/JsonLoader.h"

void UIntroWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Button_Intro)
    {
        Button_Intro->OnClicked.AddDynamic(this, &UIntroWidget::OnClickVideoImage);
    }

    GM = Cast<AMenuGameMode>(GetWorld()->GetAuthGameMode());
    GM->OnEnterIntroDele.AddDynamic(this, &UIntroWidget::HandleOnEnterIntro);
}

void UIntroWidget::Init()
{
    CreateAndAttachMediaSound();
    // ✅ 영상이 완전히 열린 후 재생 시작
    if (MediaPlayer)
    {
        MediaPlayer->OnMediaOpened.AddDynamic(
            this, &UIntroWidget::OnMediaOpened);
    }
}


void UIntroWidget::OnMediaOpened(FString OpenedUrl)
{
    UWorld* World = GetWorld();
    if (!World || !MediaPlayer) return;

    // 1초 딜레이 후 재생
    World->GetTimerManager().SetTimer(
        PlayDelayTimer,
        [this]()
        {
            if (MediaPlayer)
            {
                MediaPlayer->Play();
            }
        },
        2.0f,   // ← 딜레이 초
        false   // 반복 없음
    );
}


void UIntroWidget::OnClickVideoImage()
{
	//StopIntro();
    FAdminConfig AdminConfig;
    UJsonLoader::LoadAdminConfigFromJson(TEXT("adminConfig.json"), AdminConfig);

    if (AdminConfig.UsePassword)
    {
        GM->PasswordWidget->SetNextUIState(EUIState::ModeSelect);
        GM->PasswordWidget->SetVisibility(ESlateVisibility::Visible);
    }
    else
    {
        StopIntro();
        GM->ChangeUIState(EUIState::ModeSelect);
    }
    //GM->PasswordWidget->EditableTextBox_Password->SetFocus();
}

void UIntroWidget::PlayIntro()
{
    if (MediaPlayer)
    {
        MediaPlayer->Play();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Media Play is null"));
    }
}

void UIntroWidget::StopIntro()
{
    // 타이머가 대기 중이면 취소
    UWorld* World = GetWorld();
    if (World)
    {
        World->GetTimerManager().ClearTimer(PlayDelayTimer);
    }

    if (MediaPlayer)
    {
        MediaPlayer->Close();
    }

    if (MediaSoundComp)
    {
        MediaSoundComp->SetMediaPlayer(nullptr);
        MediaSoundComp->UnregisterComponent();
        MediaSoundComp = nullptr;
    }
}

void UIntroWidget::HandleOnEnterIntro()
{
    Init();
}


void UIntroWidget::CreateAndAttachMediaSound()
{
    if (!MediaPlayer || !MediaSource) return;

    UWorld* World = GetWorld();
    if (!World) return;

    if (!MediaSoundComp)
    {
        MediaSoundComp = NewObject<UMediaSoundComponent>(
            this, UMediaSoundComponent::StaticClass());
        if (!MediaSoundComp) return;

        MediaSoundComp->bIsUISound = true;
        MediaSoundComp->SetMediaPlayer(MediaPlayer);
        MediaSoundComp->RegisterComponentWithWorld(World);
    }

    // OnMediaOpened 중복 바인딩 방지
    MediaPlayer->OnMediaOpened.RemoveDynamic(this, &UIntroWidget::OnMediaOpened);
    MediaPlayer->OnMediaOpened.AddDynamic(this, &UIntroWidget::OnMediaOpened);

    MediaPlayer->OpenSource(MediaSource);
}