#include "ShotResultDataAsset.h"
#include "../LandscapeChecker.h"
#include "../Structs/DataTableStruct.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"

bool UShotResultDataAsset::GetResultData(ELandType LandType, FShotResultData& OutData) const
{
    if (const FShotResultData* Found = LandResultMap.Find(LandType))
    {
        OutData = *Found;
        return true;
    }

    return false;
}

UTexture2D* UShotResultDataAsset::GetResultImage(ELandType LandType) const
{
    if (const FShotResultData* Found = LandResultMap.Find(LandType))
    {
        // SoftObjectPtr -> 실제 로딩 (필요하면 비동기 방식으로 바꿔도 됨)
        return Found->ShotResultImage.LoadSynchronous();
    }

    return nullptr;
}

USoundBase* UShotResultDataAsset::GetResultSound(ELandType LandType) const
{
    if (const FShotResultData* Found = LandResultMap.Find(LandType))
    {
        return Found->ShotResultSound.LoadSynchronous();
    }

    return nullptr;
}