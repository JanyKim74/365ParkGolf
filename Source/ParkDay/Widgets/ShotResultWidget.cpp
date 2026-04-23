#include "ShotResultWidget.h"
#include "Components/Image.h"
#include "../Structs/DataTableStruct.h"
#include "../DataAsset/ShotResultDataAsset.h"
#include "Kismet/GameplayStatics.h"

void UShotResultWidget::NativeConstruct()
{
}

void UShotResultWidget::ApplyLandType(ELandType NewLandType)
{
    CurrentLandType = NewLandType;

    if (!ShotResultDataAsset)
    {
        UE_LOG(LogTemp, Warning, TEXT("ShotResultWidget: ShotResultDataAsset is nullptr"));
        return;
    }

    FShotResultData ResultData;
    if (!ShotResultDataAsset->GetResultData(NewLandType, ResultData))
    {
        UE_LOG(LogTemp, Warning, TEXT("ShotResultWidget: No data for ShotType %d"), (int32)NewLandType);
        return;
    }

    // 1) 이미지 설정
    if (Image_ShotResult)
    {
        ShotResultTexture = ResultData.ShotResultImage.LoadSynchronous();
        if (ShotResultTexture)
        {
            FSlateBrush Brush;
            Brush.SetResourceObject(ShotResultTexture);
            // 필요하면 이미지 사이즈도 세팅
            Brush.ImageSize = FVector2D(ShotResultTexture->GetSizeX(), ShotResultTexture->GetSizeY());

            Image_ShotResult->SetBrush(Brush);
        }
    }

    // 2) 사운드 재생
    ShotResultSound = ResultData.ShotResultSound.LoadSynchronous();
    if (ShotResultSound)
    {
        // 2D 사운드 재생 (UMG 위젯에서 쓰기 편함)
        UGameplayStatics::PlaySound2D(this, ShotResultSound);
    }
}

void UShotResultWidget::PlayShotResult(ELandType NewLandType)
{
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    ApplyLandType(NewLandType);
    RestartAnim(Anim_ShotResult);
}

void UShotResultWidget::PlayShotResult_OB()
{
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    ShotResultTexture = OBTexture.LoadSynchronous();
    ShotResultSound = OBSound.LoadSynchronous();

    FSlateBrush Brush;
    Brush.SetResourceObject(ShotResultTexture);
    // 필요하면 이미지 사이즈도 세팅
    Brush.ImageSize = FVector2D(ShotResultTexture->GetSizeX(), ShotResultTexture->GetSizeY());

    Image_ShotResult->SetBrush(Brush);

    UGameplayStatics::PlaySound2D(this, ShotResultSound);

    RestartAnim(Anim_ShotResult);
}

void UShotResultWidget::RestartAnim(UWidgetAnimation* Anim)
{
    if (!Anim)
    {
        return;
    }
    const UWidgetAnimation* ConstAnim = Anim;
    StopAnimation(ConstAnim);
    AnimPlayer = PlayAnimation(Anim, 0.f, 1);
}
