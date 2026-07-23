#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ChanceWidget.generated.h"

class UTextBlock;

UCLASS()
class PARKDAY_API UChanceWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(meta = (BindWidget))
    UTextBlock* TextBlock_ChanceText;

    UFUNCTION(BlueprintCallable, Category = "Chance")
    void SetChanceText(const FString& InText);

    // ⭐ 추가: C++에서 찬스 표시 시 블루프린트로 점수를 넘겨 사운드/애니 재생
    //  FinalScore:  -1 버디 / -2 이글 / -3 알바트로스
    UFUNCTION(BlueprintImplementableEvent, Category = "Chance")
    void OnChanceShown(int32 FinalScore);
};