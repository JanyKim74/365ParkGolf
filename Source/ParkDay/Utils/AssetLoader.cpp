#include "AssetLoader.h"
#include "Engine/Texture2D.h"
#include "UObject/SoftObjectPtr.h"

UTexture2D* UAssetLoader::LoadTexture2DFromPath(UObject* /*WorldContextObject*/, const FString& AssetPath)
{
    if (AssetPath.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("LoadTexture2DFromPath: �� ����Դϴ�."));
        return nullptr;
    }

    // StaticLoadObject ��� LoadObject ���ø� �Լ��� ���
    UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *AssetPath);
    if (!Tex)
    {
        UE_LOG(LogTemp, Warning, TEXT("LoadTexture2DFromPath: ������ ã�� �� �����ϴ�: %s"), *AssetPath);
    }
    return Tex;
}
