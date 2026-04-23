#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/Texture2D.h"
#include "AssetLoader.generated.h"

/**
 * Blueprint���� ���ڿ� ��η� ������ �ε��� �� �ְ� �� �ִ� �Լ� ���̺귯��
 */
UCLASS()
class PARKDAY_API UAssetLoader : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * �־��� ��ο��� Texture2D ������ �ε��մϴ�.
     *
     * @param AssetPath   "/Game/Textures/MyFolder/MyImage.MyImage" ������ ���
     * @return            �ε�� UTexture2D �ν��Ͻ� (���� �� nullptr)
     */
    UFUNCTION(BlueprintCallable, Category = "Asset Loading", meta = (WorldContext = "WorldContextObject"))
        static UTexture2D* LoadTexture2DFromPath(UObject* WorldContextObject, const FString& AssetPath);
};
