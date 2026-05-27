#include "PakMountLibrary.h"

#include "CoreMinimal.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "IPlatformFilePak.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "AssetRegistry/AssetRegistryModule.h"

// 전용 로그 카테고리 정의
static const TCHAR* NAME_PAKFILE = TEXT("PakFile");

FPakPlatformFile* UPakMountLibrary::EnsurePakPlatform()
{
    IPlatformFile* Top = &FPlatformFileManager::Get().GetPlatformFile();
    for (IPlatformFile* It = Top; It; It = It->GetLowerLevel())
    {
        if (FCString::Stricmp(It->GetName(), NAME_PAKFILE) == 0)
        {
            return static_cast<FPakPlatformFile*>(It);
        }
    }

    // Pak 레이어가 없으면 연결
    FPakPlatformFile* Pak = new FPakPlatformFile();
    Pak->Initialize(Top, TEXT(""));
    FPlatformFileManager::Get().SetPlatformFile(*Pak);
    return Pak;
}

FString UPakMountLibrary::GetProjectPaksDir()
{
    // /Game == ProjectContentDir(), 그 아래 Paks 고정
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Paks")));
}

void UPakMountLibrary::CollectPakFullPaths(const FString& NameOrPattern, TArray<FString>& OutFullPaths)
{
    OutFullPaths.Reset();

    const FString PaksDir = GetProjectPaksDir();

    // 패턴 해석: 비었으면 *.pak, 확장자 없으면 "<pattern>*.pak"
    const bool bHasExt = NameOrPattern.EndsWith(TEXT(".pak"));
    const FString Pattern = NameOrPattern.IsEmpty()
        ? TEXT("*.pak")
        : (bHasExt ? NameOrPattern : NameOrPattern + TEXT("*.pak"));

    // 비재귀 검색 (원하면 재귀로 바꿀 것)
    TArray<FString> FoundNames;
    IFileManager::Get().FindFiles(FoundNames, *(FPaths::Combine(PaksDir, Pattern)), /*Files=*/true, /*Directories=*/false);

    for (const FString& Name : FoundNames)
    {
        OutFullPaths.Add(FPaths::Combine(PaksDir, Name));
    }
}

bool UPakMountLibrary::MountOneFullPath(const FString& FullPakPath, int32 PakOrder)
{
    FPakPlatformFile* PakPlatform = EnsurePakPlatform();
    if (!PakPlatform)
    {
        UE_LOG(LogTemp, Error, TEXT("[Mount] PakPlatform unavailable"));
        return false;
    }

    const FString GameContentPhysical = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()); // ../../../ParkDay/Content/
    // /Game 매핑(대부분 기본 등록되어 있지만, 확실히 보장)
    FPackageName::RegisterMountPoint(TEXT("/Game/"), GameContentPhysical);

    const bool bMounted = PakPlatform->Mount(*FullPakPath, PakOrder, *GameContentPhysical);
    if (!bMounted)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Mount] Mount failed: %s"), *FullPakPath);
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("[Mount] Mounted to /Game : %s (Order=%d)"), *FullPakPath, PakOrder);
    return true;
}

void UPakMountLibrary::RescanGamePath()
{
    // UE 4.26: TArray<FString>로 전달해야 안전
    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    TArray<FString> Paths;
    Paths.Add(TEXT("/Game/"));
    ARM.Get().ScanPathsSynchronous(Paths, /*bForceRescan=*/true);
}

bool UPakMountLibrary::MountPakFromProjectPaksDir(const FString& PakFileNameOrPattern, int32 PakOrder, bool bRescanAssetRegistry)
{
    TArray<FString> FullPaths;
    CollectPakFullPaths(PakFileNameOrPattern, FullPaths);

    if (FullPaths.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Mount] No pak matched in %s (pattern: %s)"),
            *GetProjectPaksDir(), *PakFileNameOrPattern);
        return false;
    }

    bool bAnyMounted = false;
    for (const FString& FullPak : FullPaths)
    {
        if (MountOneFullPath(FullPak, PakOrder))
        {
            bAnyMounted = true;
            ++PakOrder; // 여러 개면 순서 증가
        }
    }

    if (bAnyMounted && bRescanAssetRegistry)
    {
        RescanGamePath();
    }
    return bAnyMounted;
}

int32 UPakMountLibrary::MountAllPaksFromProjectPaksDir(int32 StartPakOrder, bool bRescanAssetRegistry)
{
    TArray<FString> FullPaths;
    CollectPakFullPaths(TEXT(""), FullPaths);

    int32 MountedCount = 0;
    for (const FString& FullPak : FullPaths)
    {
        if (MountOneFullPath(FullPak, StartPakOrder + MountedCount))
        {
            ++MountedCount;
        }
    }

    if (MountedCount > 0 && bRescanAssetRegistry)
    {
        RescanGamePath();
    }

    UE_LOG(LogTemp, Log, TEXT("[Mount] Mounted %d pak(s) from %s"), MountedCount, *GetProjectPaksDir());
    return MountedCount;
}

bool UPakMountLibrary::UnmountPakFromProjectPaksDir(const FString& PakFileName, bool bRescanAssetRegistry)
{
    const FString FullPath = FPaths::Combine(GetProjectPaksDir(), PakFileName);

    FPakPlatformFile* PakPlatform = EnsurePakPlatform();
    if (!PakPlatform)
    {
        UE_LOG(LogTemp, Error, TEXT("[Unmount] PakPlatform unavailable"));
        return false;
    }

    const bool bOk = PakPlatform->Unmount(*FullPath);
    UE_LOG(LogTemp, Log, TEXT("[Unmount] %s : %s"), bOk ? TEXT("OK") : TEXT("FAIL"), *FullPath);

    if (bOk && bRescanAssetRegistry)
    {
        RescanGamePath();
    }
    return bOk;
}

bool UPakMountLibrary::DoesPackageExistInGame(const FString& LongPackageName)
{
    FString OnDiskFilename;
    // UE 5.7: (LongPackageName, OutFilename, bAllowTextFormats)
    const bool bExists = FPackageName::DoesPackageExist(
        LongPackageName, &OnDiskFilename, /*bAllowTextFormats=*/false);

    UE_LOG(LogTemp, Log, TEXT("[Exists] %s : %s (%s)"),
        bExists ? TEXT("FOUND") : TEXT("MISS"),
        *LongPackageName,
        bExists ? *OnDiskFilename : TEXT("n/a"));

    return bExists;
}