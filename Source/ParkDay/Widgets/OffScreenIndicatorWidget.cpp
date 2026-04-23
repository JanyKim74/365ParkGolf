#include "ParkDay/Widgets/OffscreenIndicatorWidget.h"

#include "Components/Image.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "ParkDay/InGameMode.h"
#include "ParkDay/GolfPlayer.h"

static bool IsOnScreenWithPadding(const FVector2D& P, const FVector2D& ViewSize, float Padding)
{
    return (P.X >= Padding && P.X <= ViewSize.X - Padding &&
        P.Y >= Padding && P.Y <= ViewSize.Y - Padding);
}

void UOffscreenIndicatorWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::Hidden);

    // 초기화(필요 시)
    bWasBehind = false;
    bHasSmoothedPos = false;
    bHasSmoothedDir = false;
    bHasSmoothedAngle = false;
}

void UOffscreenIndicatorWidget::SetAllowedViewTarget(AActor* InViewTarget)
{
    AllowedViewTarget = InViewTarget;
    bAllowedViewTargetInitialized = AllowedViewTarget.IsValid();

    // 상태 리셋(선택)
    bWasBehind = false;
    bHasSmoothedPos = bHasSmoothedDir = bHasSmoothedAngle = false;
    SetVisibility(ESlateVisibility::Hidden);
}

void UOffscreenIndicatorWidget::ClearAllowedViewTarget()
{
    AllowedViewTarget = nullptr;
    bAllowedViewTargetInitialized = false;
    SetVisibility(ESlateVisibility::Hidden);
}

void UOffscreenIndicatorWidget::UpdateForTarget(APlayerController* PC, const FVector& TargetWorldLoc)
{
    if (!PC || !Image_Arrow) return;

    AActor* CurrentVT = PC->GetViewTarget();



    // ✅ 최초 1회만 "허용 카메라"를 고정
    if (!bAllowedViewTargetInitialized)
    {
        AllowedViewTarget = CurrentVT;
        bAllowedViewTargetInitialized = true;
    }

    if (AllowedViewTarget.Get() == CurrentVT)
        SetVisibility(ESlateVisibility::Visible);

    if (AInGameMode* GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode()))
    {
        AGolfPlayer* Player = GM->GetCurrentTurnGolfPlayer();

        if (Player->GetPlayerState() != EPlayerState::Player_Ready)
        {
            if (GetVisibility() != ESlateVisibility::Hidden)
            {
                SetVisibility(ESlateVisibility::Hidden);
            }
            return;
        }
    }

    // ✅ 허용 카메라가 아니면: 아예 안 보이고, 연산도 안 함
    if (AllowedViewTarget.Get() != CurrentVT)
    {
        if (GetVisibility() != ESlateVisibility::Hidden)
        {
            SetVisibility(ESlateVisibility::Hidden);
        }
        return;
    }

    int32 VX = 0, VY = 0;
    PC->GetViewportSize(VX, VY);
    const FVector2D ViewSize((float)VX, (float)VY);
    if (ViewSize.X <= 1.f || ViewSize.Y <= 1.f) return;

    const FVector2D Center(ViewSize.X * 0.5f, ViewSize.Y * 0.5f);
    const float Pad = EdgePadding;

    // 카메라 정보
    FVector CamLoc;
    FRotator CamRot;
    PC->GetPlayerViewPoint(CamLoc, CamRot);

    const FVector CamFwd = CamRot.Vector();
    const FVector CamRight = FRotationMatrix(CamRot).GetScaledAxis(EAxis::Y);
    const FVector CamUp = FRotationMatrix(CamRot).GetScaledAxis(EAxis::Z);

    const FVector ToTarget = (TargetWorldLoc - CamLoc).GetSafeNormal();

    // 카메라 축으로 방향 성분 분해
    float X = FVector::DotProduct(ToTarget, CamRight);
    float Y = FVector::DotProduct(ToTarget, CamUp);
    float Z = FVector::DotProduct(ToTarget, CamFwd); // 앞(+), 뒤(-)

    // ----------------------------
    // Behind hysteresis (경계 튐 방지)
    // ----------------------------
    if (!bWasBehind && Z < EnterBehindDot)
    {
        bWasBehind = true;
    }
    else if (bWasBehind && Z > ExitBehindDot)
    {
        bWasBehind = false;
    }

    // 뒤로 판정된 경우 방향 반전(화면 가장자리에서 자연스럽게 따라오게)
    if (bWasBehind)
    {
        //X *= -1.f;
        Y *= -1.f;
        Z *= -1.f;
    }

    // Dir2D 구성 (스크린 Y는 아래가 +라서 Up과 부호 반대가 일반적)
    FVector2D Dir2D(X, -Y);

    if (bWasBehind)
    {
        // ✅ 뒤에 있으면: 좌/우(X)는 유지하고, 무조건 아래쪽(+Y) 성분을 강화한다.
        // 스크린 좌표계에서 +Y는 "아래" 방향

        // 기존 Y가 위로 향하면(음수) 아래로 꺾어줌
        Dir2D.Y = FMath::Abs(Dir2D.Y);

        // 뒤로 많이 갈수록 더 아래로 보내는 바이어스(튜닝 가능)
        // Z는 원래 (앞:+, 뒤:-)였던 값을 기준으로, 뒤면 Z<0 상태였던 걸 히스테리시스로 bWasBehind로 판정중.
        // 여기서는 ToTarget·CamFwd 값을 다시 써서 "얼마나 뒤인가"를 얻는 게 정확함.
        const float RawZ = FVector::DotProduct(ToTarget, CamFwd); // 앞:+, 뒤:-
        const float BehindAmount = FMath::Clamp(-RawZ, 0.f, 1.f); // 0~1

        const float Bias = 0.35f + 1.25f * BehindAmount; // ✅ 튜닝 포인트
        Dir2D.Y += Bias;
    }

    if (Dir2D.IsNearlyZero())
    {
        SetVisibility(ESlateVisibility::Hidden);
        return;
    }
    Dir2D.Normalize();

    const float DT = (GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f);

    // ----------------------------
    // On-screen check (앞쪽일 때만: 깜빡임 최소화)
    // ----------------------------
    if (!bWasBehind)
    {
        FVector2D ScreenPos;
        if (PC->ProjectWorldLocationToScreen(TargetWorldLoc, ScreenPos, true))
        {
            if (IsOnScreenWithPadding(ScreenPos, ViewSize, Pad))
            {
                SetVisibility(ESlateVisibility::Hidden);
                return;
            }
        }
    }

    // ----------------------------
    // Dir smoothing (뒤에서 민감한 움직임 완화)
    // ----------------------------
    if (!bHasSmoothedDir)
    {
        SmoothedDir2D = Dir2D;
        bHasSmoothedDir = true;
    }
    else
    {
        SmoothedDir2D.X = FMath::FInterpTo(SmoothedDir2D.X, Dir2D.X, DT, DirSmoothSpeed);
        SmoothedDir2D.Y = FMath::FInterpTo(SmoothedDir2D.Y, Dir2D.Y, DT, DirSmoothSpeed);

        if (!SmoothedDir2D.IsNearlyZero())
        {
            SmoothedDir2D.Normalize();
        }
        else
        {
            SmoothedDir2D = Dir2D;
        }
    }

    // "가짜 스크린 포지션"으로 가장자리 교차 계산
    const FVector2D FakeScreenPos = Center + SmoothedDir2D * 100000.f;

    FVector2D Clamped;
    float AngleDeg = 0.f;
    if (!CalcEdgeClampedScreenPoint(FakeScreenPos, ViewSize, Pad, Clamped, AngleDeg))
    {
        SetVisibility(ESlateVisibility::Hidden);
        return;
    }

    // ----------------------------
    // Position smoothing
    // ----------------------------
    if (!bHasSmoothedPos)
    {
        SmoothedPos = Clamped;
        bHasSmoothedPos = true;
    }
    else
    {
        SmoothedPos.X = FMath::FInterpTo(SmoothedPos.X, Clamped.X, DT, PositionSmoothSpeed);
        SmoothedPos.Y = FMath::FInterpTo(SmoothedPos.Y, Clamped.Y, DT, PositionSmoothSpeed);
    }

    SetVisibility(ESlateVisibility::Visible);
    SetPositionInViewport(SmoothedPos, true);

    // ----------------------------
    // Angle smoothing (wrap-safe)
    // ----------------------------
    const float TargetAngle = AngleDeg + ImageBasisCorrectionDeg;

    if (!bHasSmoothedAngle)
    {
        SmoothedAngle = TargetAngle;
        bHasSmoothedAngle = true;
    }
    else
    {
        // 각도 래핑 튐 방지
        const float Delta = FMath::FindDeltaAngleDegrees(SmoothedAngle, TargetAngle);
        SmoothedAngle = SmoothedAngle + Delta * FMath::Clamp(DT * AngleSmoothSpeed, 0.f, 1.f);
    }

    Image_Arrow->SetRenderTransformAngle(SmoothedAngle);
}

bool UOffscreenIndicatorWidget::CalcEdgeClampedScreenPoint(
    const FVector2D& ScreenPos,
    const FVector2D& ViewSize,
    float InEdgePadding,
    FVector2D& OutClamped,
    float& OutAngleDeg) const
{
    const FVector2D Center(ViewSize.X * 0.5f, ViewSize.Y * 0.5f);
    const FVector2D Dir = ScreenPos - Center;

    if (Dir.IsNearlyZero())
        return false;

    // atan2 기준: 0=오른쪽, +90=아래
    OutAngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));

    const float MinX = InEdgePadding;
    const float MaxX = ViewSize.X - InEdgePadding;
    const float MinY = InEdgePadding;
    const float MaxY = ViewSize.Y - InEdgePadding;

    float T = 1e9f;

    if (!FMath::IsNearlyZero(Dir.X))
    {
        const float Tx1 = (MinX - Center.X) / Dir.X;
        const float Tx2 = (MaxX - Center.X) / Dir.X;
        if (Tx1 > 0.f) T = FMath::Min(T, Tx1);
        if (Tx2 > 0.f) T = FMath::Min(T, Tx2);
    }

    if (!FMath::IsNearlyZero(Dir.Y))
    {
        const float Ty1 = (MinY - Center.Y) / Dir.Y;
        const float Ty2 = (MaxY - Center.Y) / Dir.Y;
        if (Ty1 > 0.f) T = FMath::Min(T, Ty1);
        if (Ty2 > 0.f) T = FMath::Min(T, Ty2);
    }

    if (T == 1e9f)
        return false;

    FVector2D P = Center + Dir * T;

    P.X = FMath::Clamp(P.X, MinX, MaxX);
    P.Y = FMath::Clamp(P.Y, MinY, MaxY);

    OutClamped = P;
    return true;
}
