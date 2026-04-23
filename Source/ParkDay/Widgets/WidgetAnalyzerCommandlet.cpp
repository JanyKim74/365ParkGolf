#include "WidgetAnalyzerCommandlet.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h" // 추가: UWidgetBlueprintGeneratedClass 정의 포함

#include "Components/PanelWidget.h"

UWidgetAnalyzerCommandlet::UWidgetAnalyzerCommandlet()
{
	IsClient = false;
	IsEditor = false;
	IsServer = false;
	LogToConsole = true;
}

int32 UWidgetAnalyzerCommandlet::Main(const FString& Params)
{
	UE_LOG(LogTemp, Display, TEXT("Starting Widget Analyzer..."));

	FString ClassPath = TEXT("/Game/KeyboardPro/BP/WBP/WBP_Keyboard.WBP_Keyboard_C");
	UClass* WidgetClass = LoadClass<UUserWidget>(nullptr, *ClassPath);

	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load class: %s"), *ClassPath);
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("Successfully loaded class: %s"), *WidgetClass->GetName());

	// We can inspect the CDO or try to create an instance. 
	// Creating an instance is better to see the tree constructed.
	// However, creating a widget usually requires a World.
	// Let's try to inspect the WidgetTree from the CDO if possible, or the BlueprintGeneratedClass.
	
	UWidgetBlueprintGeneratedClass* BPClass = Cast<UWidgetBlueprintGeneratedClass>(WidgetClass);
    if (BPClass)
    {
        UE_LOG(LogTemp, Display, TEXT("Found WidgetTree in Generated Class."));

        UWidgetTree* WidgetTree = nullptr;

        // ✅ UE5.6 방법 1: GetDefaultObject()를 Outer로 사용
        if (UUserWidget* DefaultWidget = Cast<UUserWidget>(BPClass->GetDefaultObject()))
        {
            WidgetTree = DefaultWidget->WidgetTree;
        }

        // ✅ UE5.6 방법 2: 방법 1 실패 시 ForEachObjectWithOuter로 탐색
        if (!WidgetTree)
        {
            ForEachObjectWithOuter(BPClass->GetDefaultObject(),
                [&WidgetTree](UObject* Obj)
                {
                    if (UWidgetTree* Found = Cast<UWidgetTree>(Obj))
                    {
                        WidgetTree = Found;
                    }
                },
                /*bIncludeNestedObjects=*/false
            );
        }

        // ✅ UE5.6 방법 3: 방법 2도 실패 시 FindObjectFast 사용
        if (!WidgetTree)
        {
            WidgetTree = FindObjectFast<UWidgetTree>(
                BPClass->GetDefaultObject(),
                FName(TEXT("WidgetTree"))
            );
        }

        if (WidgetTree && WidgetTree->RootWidget)
        {
            PrintWidgetHierarchy(WidgetTree->RootWidget, 0);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("WidgetTree or RootWidget is null."));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Could not cast to UWidgetBlueprintGeneratedClass."));
    }

	return 0;
}

void UWidgetAnalyzerCommandlet::PrintWidgetHierarchy(UWidget* Widget, int32 Depth)
{
	if (!Widget) return;

	FString Indent;
	for (int32 i = 0; i < Depth; ++i)
	{
		Indent += TEXT("  ");
	}

	FString WidgetName = Widget->GetName();
	FString WidgetClass = Widget->GetClass()->GetName();

	UE_LOG(LogTemp, Display, TEXT("%s- Name: %s, Type: %s"), *Indent, *WidgetName, *WidgetClass);

	if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
		{
			PrintWidgetHierarchy(Panel->GetChildAt(i), Depth + 1);
		}
	}
}
