#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WidgetAnalyzerCommandlet.generated.h"

UCLASS()
class UWidgetAnalyzerCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWidgetAnalyzerCommandlet();

	virtual int32 Main(const FString& Params) override;

private:
	void PrintWidgetHierarchy(class UWidget* Widget, int32 Depth);
};
