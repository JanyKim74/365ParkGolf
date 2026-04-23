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
    if (MediaPlayer)
    {
        MediaPlayer->Close();   // 정지 + 리소스 닫기
    }
}

void UIntroWidget::HandleOnEnterIntro()
{
    Init();
}


void UIntroWidget::CreateAndAttachMediaSound()
{
    if (MediaSoundComp || !MediaPlayer)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // 위젯(=UObject) 소유로 컴포넌트 생성
    MediaSoundComp = NewObject<UMediaSoundComponent>(this, UMediaSoundComponent::StaticClass());
    if (!MediaSoundComp)
    {
        return;
    }

    MediaPlayer->OpenSource(MediaSource);

    // (선택) UI 사운드처럼 취급하고 싶으면
    MediaSoundComp->bIsUISound = true;

    // 어떤 MediaPlayer의 오디오를 출력할지 연결
    MediaSoundComp->SetMediaPlayer(MediaPlayer);

    // 월드에 등록해야 소리가 남
    MediaSoundComp->RegisterComponentWithWorld(World);
}
