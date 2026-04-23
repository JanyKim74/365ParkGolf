#include "ShotDetector.h"
#include "Engine/Texture2D.h"

UShotDetector::UShotDetector()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UShotDetector::BeginPlay()
{
    Super::BeginPlay();
}

void UShotDetector::StartMonitoring()
{
    bIsMonitoring = true;
    PreviousFrame = nullptr;
    UE_LOG(LogTemp, Log, TEXT("Shot monitoring started."));
}

void UShotDetector::StopMonitoring()
{
    bIsMonitoring = false;
    PreviousFrame = nullptr; // ✅ 참조만 해제
    UE_LOG(LogTemp, Log, TEXT("Shot monitoring stopped."));
}

void UShotDetector::AnalyzeFrame(UTexture2D* CurrentFrame, float CurrentTime)
{
    if (!bIsMonitoring || !CurrentFrame)
    {
        return;
    }

    if (PreviousFrame)
    {
        float Difference = CalculateMotionDifference(PreviousFrame, CurrentFrame);

        if (Difference > MotionThreshold)
        {
            OnShotDetected.Broadcast(CurrentTime);
            bIsMonitoring = false;
            UE_LOG(LogTemp, Warning, TEXT("SHOT DETECTED at time: %f (Difference: %f)"),
                CurrentTime, Difference);
        }
    }

    // ✅ 개선: 복사 없이 포인터만 저장
    // WebcamCapture에서 관리하는 텍스처를 참조만 함
    PreviousFrame = CurrentFrame;
}

float UShotDetector::CalculateMotionDifference(UTexture2D* Frame1, UTexture2D* Frame2)
{
    if (!Frame1 || !Frame2 ||
        Frame1->GetSizeX() == 0 || Frame1->GetSizeY() == 0 ||
        Frame2->GetSizeX() == 0 || Frame2->GetSizeY() == 0)
    {
        return 0.0f;
    }

    // ✅ 안전성 검사 추가
    if (!Frame1->IsValidLowLevel() || !Frame2->IsValidLowLevel())
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid texture object"));
        return 0.0f;
    }

    // ✅ PlatformData 유효성 검사
    if (!Frame1->GetPlatformData() || !Frame2->GetPlatformData() ||
        Frame1->GetPlatformData()->Mips.Num() == 0 || Frame2->GetPlatformData()->Mips.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid PlatformData or Mips"));
        return 0.0f;
    }

    // Mip 데이터 잠금
    FColor* Pixels1 = reinterpret_cast<FColor*>(
        Frame1->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_ONLY)
        );
    FColor* Pixels2 = reinterpret_cast<FColor*>(
        Frame2->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_ONLY)
        );

    if (!Pixels1 || !Pixels2)
    {
        if (Pixels1) Frame1->GetPlatformData()->Mips[0].BulkData.Unlock();
        if (Pixels2) Frame2->GetPlatformData()->Mips[0].BulkData.Unlock();
        UE_LOG(LogTemp, Warning, TEXT("Failed to lock texture data"));
        return 0.0f;
    }

    // ✅ 개선: 다운샘플링으로 성능 향상
    int32 Width = Frame1->GetSizeX();
    int32 Height = Frame1->GetSizeY();
    int32 Step = 4; // 4픽셀마다 샘플링 (16배 속도 향상)

    float TotalDifference = 0.0f;
    int32 SampleCount = 0;

    for (int32 y = 0; y < Height; y += Step)
    {
        for (int32 x = 0; x < Width; x += Step)
        {
            int32 Index = y * Width + x;

            // RGB 차이 계산
            TotalDifference += FMath::Abs(Pixels1[Index].R - Pixels2[Index].R);
            TotalDifference += FMath::Abs(Pixels1[Index].G - Pixels2[Index].G);
            TotalDifference += FMath::Abs(Pixels1[Index].B - Pixels2[Index].B);

            SampleCount++;
        }
    }

    Frame1->GetPlatformData()->Mips[0].BulkData.Unlock();
    Frame2->GetPlatformData()->Mips[0].BulkData.Unlock();

    // 정규화 (0.0 ~ 1.0)
    return TotalDifference / (SampleCount * 3.0f * 255.0f);
}