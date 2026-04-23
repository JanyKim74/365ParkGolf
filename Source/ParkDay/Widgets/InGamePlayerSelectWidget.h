#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InGamePlayerSelectWidget.generated.h"



class UImage;
class UTextBlock;
class UVerticalBox;
class UButton;
class UUserWidget;
class UCanvasPanel;
class UPlayerSelectWidget;

UCLASS()
class UInGamePlayerSelectWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UInGamePlayerSelectWidget(const FObjectInitializer& ObjectInitializer);

    virtual void NativeConstruct() override;


    void Init();

        UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UButton* Button_Back;

        UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UButton* Button_Next;

        UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UCanvasPanel* CanvasPanel_PlayerList;

        UFUNCTION()
        void HandleOnClickNextButton();

        UFUNCTION()
        void HandleOnClickBackButton();

public:
    TSubclassOf<UPlayerSelectWidget> PlayerSelectClass;
    UPlayerSelectWidget* PlayerSelect;
};
