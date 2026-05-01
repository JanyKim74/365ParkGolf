#include "BallNamePlateComponent.h"

#include "Widgets/BallNamePlateWidget.h"
#include "Components/WidgetComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Blueprint/UserWidget.h"

UBallNamePlateComponent::UBallNamePlateComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	WidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("BallNamePlate_WidgetComponent"));
	if (WidgetComponent && !HasAnyFlags(RF_ClassDefaultObject))
	{
		WidgetComponent->SetupAttachment(this);

		// ✅ Screen 공간
		WidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);

		// Screen 공간에서는 DesiredSize 기반이 가장 안정적 (크기 변화는 위젯 RenderScale로 처리)
		WidgetComponent->SetDrawAtDesiredSize(true);

		WidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WidgetComponent->SetGenerateOverlapEvents(false);

		bManualVisible = bStartVisible;
		bAutoVisible = true;

		CurrentScale = 1.0f;
	}
}

void UBallNamePlateComponent::BeginPlay()
{
	Super::BeginPlay();

	bManualVisible = bStartVisible;
	bAutoVisible = true;

	EnsureWidgetCreated();
	ApplyPendingNameIfAny();

	// 스케일 먼저 계산/적용 (오프셋에도 반영)
	if (bScaleByDistance)
	{
		UpdateDistanceScale();
	}
	else
	{
		CurrentScale = 1.0f;
	}

	UpdateWorldPositionByScreenOffset();
	UpdateAutoVisibility();

	ApplyFinalVisibility();
}

void UBallNamePlateComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	Super::OnComponentDestroyed(bDestroyingHierarchy);
	bWidgetReady = false;
}

void UBallNamePlateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bManualVisible)
	{
		ApplyFinalVisibility();
		return;
	}

	if (!bWidgetReady)
	{
		EnsureWidgetCreated();
		ApplyPendingNameIfAny();
	}

	// ✅ 스케일 먼저 적용 → 오프셋도 같은 비율로 줄여서 위치 계산
	if (bScaleByDistance)
	{
		UpdateDistanceScale();
	}
	else
	{
		CurrentScale = 1.0f;
	}

	UpdateWorldPositionByScreenOffset();
	UpdateAutoVisibility();
	ApplyFinalVisibility();
}

void UBallNamePlateComponent::EnsureWidgetCreated()
{
	if (bWidgetReady || !WidgetComponent)
	{
		return;
	}

	if (!NamePlateWidgetClass)
	{
		return;
	}

	WidgetComponent->SetWidgetClass(NamePlateWidgetClass);
	WidgetComponent->InitWidget();
	WidgetComponent->SetDrawAtDesiredSize(true);

	bWidgetReady = (WidgetComponent->GetUserWidgetObject() != nullptr);
}

void UBallNamePlateComponent::ApplyPendingNameIfAny()
{
	if (!bWidgetReady)
	{
		return;
	}

	if (!PendingNameText.IsEmpty())
	{
		if (UBallNamePlateWidget* W = GetNamePlateWidget())
		{
			W->SetPlayerNameText(PendingNameText);
			PendingNameText = FText::GetEmpty();
		}
	}
}

UBallNamePlateWidget* UBallNamePlateComponent::GetNamePlateWidget() const
{
	if (!WidgetComponent)
	{
		return nullptr;
	}
	return Cast<UBallNamePlateWidget>(WidgetComponent->GetUserWidgetObject());
}

void UBallNamePlateComponent::SetWidgetClass(TSubclassOf<UUserWidget> InNamePlateWidgetClass)
{
	NamePlateWidgetClass = InNamePlateWidgetClass;
	bWidgetReady = false;
}

void UBallNamePlateComponent::SetPlayerNameText(const FText& InName)
{
	if (!bWidgetReady)
	{
		PendingNameText = InName;
		return;
	}

	if (UBallNamePlateWidget* W = GetNamePlateWidget())
	{
		W->SetPlayerNameText(InName);
	}
}

void UBallNamePlateComponent::SetPlayerNameString(const FString& InName)
{
	SetPlayerNameText(FText::FromString(InName));
}

void UBallNamePlateComponent::SetNamePlateVisible(bool bIsVisible)
{
	bManualVisible = bIsVisible;
	ApplyFinalVisibility();
}

bool UBallNamePlateComponent::ScreenToWorldAtBallDepth(
	APlayerController* PC,
	const FVector2D& ScreenPos,
	const FVector& BallWorldPos,
	FVector& OutWorldPos) const
{
	if (!PC || !PC->PlayerCameraManager)
	{
		return false;
	}

	FVector RayOrigin, RayDir;
	if (!PC->DeprojectScreenPositionToWorld(ScreenPos.X, ScreenPos.Y, RayOrigin, RayDir))
	{
		return false;
	}

	const FVector CamForward = PC->PlayerCameraManager->GetActorForwardVector();

	const float Denom = FVector::DotProduct(RayDir, CamForward);
	if (FMath::IsNearlyZero(Denom))
	{
		return false;
	}

	const float T = FVector::DotProduct((BallWorldPos - RayOrigin), CamForward) / Denom;
	if (T <= 0.f)
	{
		return false;
	}

	OutWorldPos = RayOrigin + RayDir * T;
	return true;
}

void UBallNamePlateComponent::UpdateWorldPositionByScreenOffset()
{
	if (!WidgetComponent)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, CameraPlayerIndex);
	if (!PC)
	{
		return;
	}

	const FVector BallWorldPos = Owner->GetActorLocation();

	FVector2D BallScreenPos(0.f, 0.f);
	const bool bProjected = UGameplayStatics::ProjectWorldToScreen(PC, BallWorldPos, BallScreenPos, true);
	if (!bProjected)
	{
		return;
	}

	// ✅ 오프셋도 네임플레이트 스케일과 같은 비율로 줄이기
	const float ScaleForOffset = bScaleByDistance ? CurrentScale : 1.0f;
	const FVector2D ScaledOffset = ScreenOffsetPx * ScaleForOffset;

	const FVector2D PlateScreenPos = BallScreenPos + ScaledOffset;

	FVector PlateWorldPos;
	if (!ScreenToWorldAtBallDepth(PC, PlateScreenPos, BallWorldPos, PlateWorldPos))
	{
		return;
	}

	WidgetComponent->SetWorldLocation(PlateWorldPos);
}

void UBallNamePlateComponent::UpdateAutoVisibility()
{
	bAutoVisible = true;

	UWorld* World = GetWorld();
	if (!World || !WidgetComponent)
	{
		bAutoVisible = false;
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, CameraPlayerIndex);
	if (!PC || !PC->PlayerCameraManager)
	{
		bAutoVisible = false;
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		bAutoVisible = false;
		return;
	}

	const FVector BallWorldPos = Owner->GetActorLocation();

	// 카메라 뒤면 숨김
	const FVector CamLoc = PC->PlayerCameraManager->GetCameraLocation();
	const FVector CamForward = PC->PlayerCameraManager->GetActorForwardVector();
	const FVector ToBall = (BallWorldPos - CamLoc);

	if (FVector::DotProduct(CamForward, ToBall.GetSafeNormal()) <= 0.0f)
	{
		bAutoVisible = false;
		return;
	}

	// 화면 밖이면 숨김
	int32 ViewX = 0, ViewY = 0;
	PC->GetViewportSize(ViewX, ViewY);
	if (ViewX <= 0 || ViewY <= 0)
	{
		bAutoVisible = false;
		return;
	}

	FVector2D BallScreenPos(0.f, 0.f);
	const bool bProjected = UGameplayStatics::ProjectWorldToScreen(PC, BallWorldPos, BallScreenPos, true);
	if (!bProjected)
	{
		bAutoVisible = false;
		return;
	}

	const float MinX = -ScreenMarginPx;
	const float MinY = -ScreenMarginPx;
	const float MaxX = (float)ViewX + ScreenMarginPx;
	const float MaxY = (float)ViewY + ScreenMarginPx;

	if (BallScreenPos.X < MinX || BallScreenPos.X > MaxX || BallScreenPos.Y < MinY || BallScreenPos.Y > MaxY)
	{
		bAutoVisible = false;
		return;
	}

	bAutoVisible = true;
}

float UBallNamePlateComponent::ComputeDistanceScale() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 1.0f;
	}

	APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(World, CameraPlayerIndex);
	if (!Cam)
	{
		return 1.0f;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return 1.0f;
	}

	const float Dist = FVector::Dist(Cam->GetCameraLocation(), Owner->GetActorLocation());

	const float HoldD = FMath::Max(0.f, HoldDistance);
	const float EndD = FMath::Max(HoldD + 1.f, MinScaleDistance);

	float Scale = 1.0f;

	if (Dist <= HoldD)
	{
		Scale = 1.0f;
	}
	else if (Dist >= EndD)
	{
		Scale = MinScale;
	}
	else
	{
		const float Alpha = (Dist - HoldD) / (EndD - HoldD);
		Scale = FMath::Lerp(1.0f, MinScale, FMath::Clamp(Alpha, 0.f, 1.f));
	}

	return FMath::Clamp(Scale, 0.1f, 1.0f);
}

void UBallNamePlateComponent::UpdateDistanceScale()
{
	UBallNamePlateWidget* W = GetNamePlateWidget();
	if (!W)
	{
		return;
	}

	CurrentScale = ComputeDistanceScale();

	// ✅ 위젯(CanvasPanel_NamePlate) 전체 스케일 적용
	W->SetNamePlateScale(CurrentScale);
}

void UBallNamePlateComponent::ApplyFinalVisibility()
{
	if (!WidgetComponent)
	{
		return;
	}

	const bool bFinalVisible = bManualVisible && bAutoVisible;
	WidgetComponent->SetVisibility(bFinalVisible, true);
	WidgetComponent->SetHiddenInGame(!bFinalVisible, true);
}
