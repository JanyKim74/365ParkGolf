#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "../MenuGameMode.h"          // ★ 여기 안에 EUIState 가 있어야 합니다.
#include "UIStateWidgetMapDataAsset.generated.h"

class UUserWidget;

/**
 * UI State(예: Intro, ModeSelect...) 별로 사용할 WBP(Widget Blueprint Class)와
 * 생성 정책(ZOrder, PreCreate)을 설정하는 DataAsset
 */
USTRUCT(BlueprintType)
struct FUIStateWidgetEntry
{
    GENERATED_BODY()

public:
    // 어떤 UI 상태인가
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI")
    EUIState State = EUIState::Intro;

    // 해당 상태에서 표시할 위젯 클래스(WBP)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI")
    TSubclassOf<UUserWidget> WidgetClass;

    // AddToViewport ZOrder (값이 높을수록 위)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI")
    int32 ZOrder = 0;

    // BeginPlay 등에서 미리 CreateWidget 해둘지 여부
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI")
    bool bPreCreate = true;
};

UCLASS(BlueprintType)
class PARKDAY_API UUIStateWidgetMapDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    // 에디터에서 상태별 항목을 추가/편집
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI")
    TArray<FUIStateWidgetEntry> Entries;

public:
    // 해당 상태의 위젯 클래스를 가져옴 (없으면 nullptr)
    UFUNCTION(BlueprintCallable, Category="UI")
    TSubclassOf<UUserWidget> GetWidgetClass(EUIState State) const;

    // 해당 상태 엔트리를 가져옴 (없으면 false)
    UFUNCTION(BlueprintCallable, Category="UI")
    bool GetEntry(EUIState State, FUIStateWidgetEntry& OutEntry) const;

    // PreCreate가 true인 엔트리들만 뽑아서 반환
    UFUNCTION(BlueprintCallable, Category="UI")
    void GetPreCreateEntries(TArray<FUIStateWidgetEntry>& OutEntries) const;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
    const FUIStateWidgetEntry* FindEntry(EUIState State) const;
};
