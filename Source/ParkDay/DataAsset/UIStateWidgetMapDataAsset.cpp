#include "UIStateWidgetMapDataAsset.h"
#include "Blueprint/UserWidget.h"

const FUIStateWidgetEntry* UUIStateWidgetMapDataAsset::FindEntry(EUIState State) const
{
    for (const FUIStateWidgetEntry& Entry : Entries)
    {
        if (Entry.State == State)
        {
            return &Entry;
        }
    }
    return nullptr;
}

TSubclassOf<UUserWidget> UUIStateWidgetMapDataAsset::GetWidgetClass(EUIState State) const
{
    if (const FUIStateWidgetEntry* Found = FindEntry(State))
    {
        return Found->WidgetClass;
    }
    return nullptr;
}

bool UUIStateWidgetMapDataAsset::GetEntry(EUIState State, FUIStateWidgetEntry& OutEntry) const
{
    if (const FUIStateWidgetEntry* Found = FindEntry(State))
    {
        OutEntry = *Found;
        return true;
    }
    return false;
}

void UUIStateWidgetMapDataAsset::GetPreCreateEntries(TArray<FUIStateWidgetEntry>& OutEntries) const
{
    OutEntries.Reset();

    for (const FUIStateWidgetEntry& Entry : Entries)
    {
        if (Entry.bPreCreate && Entry.WidgetClass)
        {
            OutEntries.Add(Entry);
        }
    }
}

#if WITH_EDITOR
void UUIStateWidgetMapDataAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    // 에디터에서 실수 방지용: 같은 State가 중복 등록되면 경고 로그
    // (자동으로 제거하진 않고, 경고만 띄움)
    TSet<EUIState> Seen;
    for (const FUIStateWidgetEntry& Entry : Entries)
    {
        if (Seen.Contains(Entry.State))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[UUIStateWidgetMapDataAsset] Duplicate State entry detected: %d (asset=%s)"),
                (int32)Entry.State, *GetName());
        }
        Seen.Add(Entry.State);
    }
}
#endif
