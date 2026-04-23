#pragma once
#include "CoreMinimal.h"
#include "GolfDataStructures.h"
#include "UObject/NoExportTypes.h"
#include "GolfDataManager.generated.h"

/**
 * 
 */
UCLASS()
class PARKDAY_API UGolfDataManager : public UObject
{
	GENERATED_BODY()
	
public:
    void LoadGameInfo(const FString& FilePath);
    void SaveGameInfo(const FString& FilePath);
    FGameInfo& GetGameInfo() { return GameInfo; }

private:
    FGameInfo GameInfo;
};