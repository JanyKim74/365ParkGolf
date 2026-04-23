#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InGamePopupWidget.generated.h"

class UImage;
class UTextBlock;
class UVerticalBox;
class UButton;
class UUserWidget;

UCLASS()
class UInGamePopupWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    // UUserWidget �ʱ�ȭ �� ���ε��� ������ �����ϱ� ���� Override
    //virtual void NativeConstruct() override;

    /** �г� ��� �̹��� */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UImage* Image_Panel;
};
