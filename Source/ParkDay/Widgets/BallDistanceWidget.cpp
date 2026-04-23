#include "ParkDay/Widgets/BallDistanceWidget.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanelSlot.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "ParkDay/GolfBall.h"
#include "ParkDay/GolfPlayer.h"
#include "ParkDay/InGameMode.h"

void UBallDistanceWidget::NativeConstruct()
{
    Super::NativeConstruct();

    GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    CachedBall = nullptr;
    CachedPlayer = nullptr;
    LastBallState = EBallState::Ball_Init;
    bHasShotStart = false;
    bFlyByHeightStarted = false;
    bCarryLocked = false;
    CarryDistanceM = 0.0f;
    ShotStartLocation = FVector::ZeroVector;
    LastValidScreenPos = FVector2D::ZeroVector;
    bHasValidScreenPos = false;
    CustomTargetLocation = FVector::ZeroVector;
    bHasCustomTarget = false;
    SetVisibility(ESlateVisibility::HitTestInvisible);
    SetRenderOpacity(0.f);
}

// ����������������������������������������������������������������������������������������������������������������������������
//  Ÿ�� ��ġ ���� ���� �Լ�
// ����������������������������������������������������������������������������������������������������������������������������

void UBallDistanceWidget::SetCustomTargetLocation(const FVector& InTargetLocation)
{
    CustomTargetLocation = InTargetLocation;
    bHasCustomTarget = true;
}

void UBallDistanceWidget::ClearCustomTargetLocation()
{
    CustomTargetLocation = FVector::ZeroVector;
    bHasCustomTarget = false;
}

FVector UBallDistanceWidget::GetCurrentTargetLocation() const
{
    FVector OutTarget = FVector::ZeroVector;
    GetTargetLocation(OutTarget);
    return OutTarget;
}

// ����������������������������������������������������������������������������������������������������������������������������
//  ���� ����: ���� ��ȿ�� Ÿ�� ��ġ�� ���մϴ�.
//  Ŀ���� Ÿ�� �� Ȧ�� ��ġ ������ �켱������ �����ϴ�.
//  ��ġ�� ������ ���ϸ� false�� ��ȯ�մϴ�.
// ����������������������������������������������������������������������������������������������������������������������������

bool UBallDistanceWidget::GetTargetLocation(FVector& OutTargetLocation) const
{
    // 1����: �ܺο��� ������ Ŀ���� Ÿ��
    if (bHasCustomTarget)
    {
        OutTargetLocation = CustomTargetLocation;
        return true;
    }

    // 2����: ���Ӹ���� Ȧ�� ��ġ
    if (GM && GM->MapInfo.HolecupPositions.IsValidIndex(GM->CurrentHole - 1))
    {
        OutTargetLocation = GM->MapInfo.HolecupPositions[GM->CurrentHole - 1];
        return true;
    }

    return false;
}

// ����������������������������������������������������������������������������������������������������������������������������
//  NativeTick
// ����������������������������������������������������������������������������������������������������������������������������

void UBallDistanceWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    static constexpr float WorldOffsetZ = 30.0f;

    if (!GM)
    {
        GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
    }

    if (!GM)
    {
        return;
    }

    AGolfPlayer* Player = GM->GetCurrentTurnGolfPlayer();
    AGolfBall* Ball = GM->GetCurrentTurnGolfBall();
    if (!Player || !Ball)
    {
        return;
    }

    if (Ball != CachedBall || Player != CachedPlayer)
    {
        CachedBall = Ball;
        CachedPlayer = Player;
        LastBallState = Ball->GetBallState();
        bHasShotStart = false;
        bFlyByHeightStarted = false;
        bCarryLocked = false;
        CarryDistanceM = 0.0f;
        ShotStartLocation = FVector::ZeroVector;
    }

    const EBallState CurrentState = Ball->GetBallState();

    if (CurrentState == EBallState::Ball_Ready
        || CurrentState == EBallState::Ball_Init
        || CurrentState == EBallState::Ball_Des)
    {
        bHasShotStart = false;
        bFlyByHeightStarted = false;
        bCarryLocked = false;
        CarryDistanceM = 0.0f;
        ShotStartLocation = FVector::ZeroVector;
    }

    if (!bHasShotStart
        && CurrentState != EBallState::Ball_Ready
        && CurrentState != EBallState::Ball_Init
        && CurrentState != EBallState::Ball_Des)
    {
        ShotStartLocation = Player->BEFOREPos;
        if (ShotStartLocation.IsNearlyZero())
        {
            ShotStartLocation = Ball->GetActorLocation();
        }

        bHasShotStart = true;
        bCarryLocked = false;
        CarryDistanceM = 0.0f;
        bFlyByHeightStarted = false;
    }

    const FVector BallLocation = Ball->GetActorLocation();
    const FVector StartLocationForCarry = bHasShotStart ? ShotStartLocation : Player->BEFOREPos;

    bool bIsAirborneNow = false;
    if (bHasShotStart)
    {
        if (UWorld* World = GetWorld())
        {
            static constexpr float GroundTraceLengthCm = 10000.0f;
            static constexpr float TraceStartOffsetCm = 5.0f;
            static constexpr float AirborneThresholdCm = 15.0f;

            FVector TraceStart = BallLocation + FVector(0.0f, 0.0f, TraceStartOffsetCm);
            FVector TraceEnd = BallLocation - FVector(0.0f, 0.0f, GroundTraceLengthCm);

            FHitResult Hit;
            FCollisionQueryParams Params(SCENE_QUERY_STAT(BallDistanceCarryTrace), false, Ball);
            if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
            {
                if (Hit.Distance > AirborneThresholdCm)
                {
                    bIsAirborneNow = true;
                }
            }
        }
    }

    if (bHasShotStart && bIsAirborneNow)
    {
        bFlyByHeightStarted = true;
    }

    if (bHasShotStart && bFlyByHeightStarted && !bCarryLocked && !bIsAirborneNow)
    {
        CarryDistanceM = FVector::Dist2D(StartLocationForCarry, BallLocation) * 0.01f;
        bCarryLocked = true;
    }

    FVector StartLocation = bHasShotStart ? ShotStartLocation : Player->BEFOREPos;
    if (StartLocation.IsNearlyZero())
    {
        StartLocation = BallLocation;
    }

    // ���� �Ÿ� ��� ��������������������������������������������������������������������������������������������
    const float TotalDistanceM = FVector::Dist2D(StartLocation, BallLocation) * 0.01f;

    // ���� �Ÿ�: Ŀ���� Ÿ�� �켱, ������ Ȧ��
    float RemainDistanceM = 0.0f;
    FVector TargetLocation;
    if (GetTargetLocation(TargetLocation))
    {
        RemainDistanceM = FVector::Dist2D(BallLocation, TargetLocation) * 0.01f;
    }

    float CarryDistanceDisplayM = 0.0f;
    if (bCarryLocked)
    {
        CarryDistanceDisplayM = CarryDistanceM;
    }
    else if (bHasShotStart && bFlyByHeightStarted)
    {
        CarryDistanceDisplayM = FVector::Dist2D(StartLocation, BallLocation) * 0.01f;
    }

    // ���� TextBlock ������Ʈ ��������������������������������������������������������������������������
    if (TextBlock_TotalDistance)
    {
        TextBlock_TotalDistance->SetText(
            FText::FromString(FString::Printf(TEXT("%.1fm"), TotalDistanceM)));
    }

    if (TextBlock_RemainDistance)
    {
        TextBlock_RemainDistance->SetText(
            FText::FromString(FString::Printf(TEXT("%.1fm"), RemainDistanceM)));
    }

    if (TextBlock_CarryDistance)
    {
        TextBlock_CarryDistance->SetText(
            FText::FromString(FString::Printf(TEXT("%.1fm"), CarryDistanceDisplayM)));
    }

    // ���� ���� ��ġ (�� ����) ������������������������������������������������������������������������
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        FVector2D ScreenPos;
        const FVector WorldPos = BallLocation + FVector(0.0f, 0.0f, WorldOffsetZ);
        const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this);
        const float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(this);
        const float SafeScale = FMath::Max(ViewportScale, 0.01f);

        if (UGameplayStatics::ProjectWorldToScreen(PC, WorldPos, ScreenPos))
        {
            ScreenPos.X += 50.0f;
            ScreenPos.X = FMath::Clamp(ScreenPos.X, 0.0f, FMath::Max(ViewportSize.X - 1.0f, 0.0f));
            ScreenPos.Y = FMath::Clamp(ScreenPos.Y, 0.0f, FMath::Max(ViewportSize.Y - 1.0f, 0.0f));
            LastValidScreenPos = ScreenPos;
            bHasValidScreenPos = true;

            if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
            {
                CanvasSlot->SetPosition(ScreenPos / SafeScale);
            }
            else
            {
                SetPositionInViewport(ScreenPos / SafeScale, false);
            }
        }
        else if (bHasValidScreenPos)
        {
            FVector2D ClampedPos = LastValidScreenPos;
            ClampedPos.X = FMath::Clamp(ClampedPos.X, 0.0f, FMath::Max(ViewportSize.X - 1.0f, 0.0f));
            ClampedPos.Y = FMath::Clamp(ClampedPos.Y, 0.0f, FMath::Max(ViewportSize.Y - 1.0f, 0.0f));
            LastValidScreenPos = ClampedPos;

            if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
            {
                CanvasSlot->SetPosition(ClampedPos / SafeScale);
            }
            else
            {
                SetPositionInViewport(ClampedPos / SafeScale, false);
            }
        }
    }

    LastBallState = CurrentState;
}