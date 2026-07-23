#include "ChanceWidget.h"
#include "Components/TextBlock.h"

void UChanceWidget::SetChanceText(const FString& InText)
{
    if (TextBlock_ChanceText)
    {
        TextBlock_ChanceText->SetText(FText::FromString(InText));
    }
}