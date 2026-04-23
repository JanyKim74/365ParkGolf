#include "BallNamePlateWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "ParkDay/SoundManager.h"

void UBallNamePlateWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// �ʱⰪ (1.0)
	CachedScale = 1.0f;

	if (CanvasPanel_NamePlate)
	{
		CanvasPanel_NamePlate->SetRenderScale(FVector2D(1.0f, 1.0f));
	}
}

void UBallNamePlateWidget::SetPlayerNameText(const FText& InName)
{
	if (TextBlock_NamePlate)
	{
		TextBlock_NamePlate->SetText(InName);
	}
}

void UBallNamePlateWidget::SetPlayerNameString(const FString& InName)
{
	SetPlayerNameText(FText::FromString(InName));
}

void UBallNamePlateWidget::SetNamePlateScale(float InScale)
{
	// Screen ���������� Ȯ���ϰ� �ȼ� ũ�� ��ȭ�� ������ UMG RenderScale�� ó��
	const float Clamped = FMath::Clamp(InScale, 0.1f, 1.0f);

	if (FMath::IsNearlyEqual(CachedScale, Clamped, 0.001f))
	{
		return;
	}
	CachedScale = Clamped;

	if (CanvasPanel_NamePlate)
	{
		CanvasPanel_NamePlate->SetRenderScale(FVector2D(Clamped, Clamped));
	}
	else if (UWidget* Root = GetRootWidget())
	{
		// ���ε� ���� ���: ��Ʈ���� ����
		Root->SetRenderScale(FVector2D(Clamped, Clamped));
	}
}
