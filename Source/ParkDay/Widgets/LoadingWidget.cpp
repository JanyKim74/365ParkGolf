#include "LoadingWidget.h"
#include "Components/Image.h"
#include "ParkDay/SoundManager.h"

void ULoadingWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (Image_Background && BackgroundImages.IsValidIndex(0))
    {
        Image_Background->SetBrushFromTexture(BackgroundImages[0], true);
    }
}


void ULoadingWidget::SetRandomImageIndex()
{
    if (BackgroundImages.Num() == 0)
    {
        RandomIndex = INDEX_NONE;
        return;
    }

    RandomIndex = 0;
}

void ULoadingWidget::ShowRandomImage()
{
	if (Image_Background && BackgroundImages.IsValidIndex(RandomIndex))
	{
		Image_Background->SetBrushFromTexture(BackgroundImages[RandomIndex], true);
		return;
	}

	if (Image_Background && BackgroundImages.IsValidIndex(0))
	{
		Image_Background->SetBrushFromTexture(BackgroundImages[0], true);
	}
}



void ULoadingWidget::PlayBGM()
{
    if (USoundManager* SM = Cast<USoundManager>(GetGameInstance()->GetSubsystem<USoundManager>()))
    {
        SM->PlayBGM_ById("BGM.Loading", 0.67);
    }
}

void ULoadingWidget::StopBGM()
{
    if (USoundManager* SM = Cast<USoundManager>(GetGameInstance()->GetSubsystem<USoundManager>()))
    {
        SM->StopBGM(1.f);
    }
}
