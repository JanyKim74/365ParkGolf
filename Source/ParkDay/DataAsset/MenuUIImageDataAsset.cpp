#include "MenuUIImageDataAsset.h"
#include "../LandscapeChecker.h"
#include "../Structs/DataTableStruct.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"

UTexture2D* UMenuUIImageDataAsset::GetUIImage(FName Id) const
{
    const FUIImage* Found = UIImages.FindByPredicate([Id](const FUIImage& E)
        {
            return E.Id == Id;
        });

    if (Found)
    {
        if (UTexture2D* Tex = Found->Texture.LoadSynchronous())
        {
            return Tex;
        }
    }
    return DefaultTexture.LoadSynchronous();
}
