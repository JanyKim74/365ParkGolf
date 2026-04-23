#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "PakMountLibrary.generated.h"

/**
 * /Game (= ../../../ParkDay/Content/)로 외부 pak 마운트/언마운트
 * pak은 반드시 프로젝트 루트의 Content/Paks 안에서만 찾습니다.
 */
UCLASS()
class PARKDAY_API UPakMountLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** 파일명(또는 접두사 패턴)으로 /Content/Paks에서 찾아 마운트 */
    UFUNCTION(BlueprintCallable, Category = "Pak")
        static bool MountPakFromProjectPaksDir(const FString& PakFileNameOrPattern, int32 PakOrder = 0, bool bRescanAssetRegistry = true);

    /** /Content/Paks 아래의 모든 .pak 마운트 (반환=성공 개수) */
    UFUNCTION(BlueprintCallable, Category = "Pak")
        static int32 MountAllPaksFromProjectPaksDir(int32 StartPakOrder = 0, bool bRescanAssetRegistry = true);

    /** /Content/Paks 안의 특정 .pak 언마운트 */
    UFUNCTION(BlueprintCallable, Category = "Pak")
        static bool UnmountPakFromProjectPaksDir(const FString& PakFileName, bool bRescanAssetRegistry = true);

    /** 패키지 존재 확인 (/Game 경로 기준) */
    UFUNCTION(BlueprintCallable, Category = "Pak")
        static bool DoesPackageExistInGame(const FString& LongPackageName);

private:
    static class FPakPlatformFile* EnsurePakPlatform();
    static FString GetProjectPaksDir();                            // .../Content/Paks/
    static void    CollectPakFullPaths(const FString& NameOrPattern, TArray<FString>& OutFullPaths);
    static bool    MountOneFullPath(const FString& FullPakPath, int32 PakOrder);
    static void    RescanGamePath();
};