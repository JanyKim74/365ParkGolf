#include "PakMountManager.h"
#include "IPlatformFilePak.h"
#include "HAL/PlatformFilemanager.h"
#include "Kismet/GameplayStatics.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY(LogPakMountMgr);

// ─────────────────────────────────────────────────────────────
// PIE/Standalone/GamePreview 에서만 true
bool UPakMountManager::IsEditorRuntime() const
{
#if WITH_EDITOR
    if (!GIsEditor) return false;
    UWorld* World = GetWorld();
    if (!World) return false;
    const EWorldType::Type WT = World->WorldType;
    return (WT == EWorldType::PIE || WT == EWorldType::Game || WT == EWorldType::GamePreview);
#else
    return false;
#endif
}

void UPakMountManager::EnsurePakFS()
{
#if WITH_EDITOR
    // PakFile 이미 설치돼 있으면 패스
    if (FPlatformFileManager::Get().FindPlatformFile(TEXT("PakFile"))) return;

    IPlatformFile& Inner = FPlatformFileManager::Get().GetPlatformFile();
    FPakPlatformFile* PakPlatform = new FPakPlatformFile();
    if (PakPlatform->Initialize(&Inner, TEXT("")))
    {
        FPlatformFileManager::Get().SetPlatformFile(*PakPlatform);
        UE_LOG(LogPakMountMgr, Log, TEXT("[EnsurePakFS] FPakPlatformFile initialized."));
    }
    else
    {
        delete PakPlatform;
        UE_LOG(LogPakMountMgr, Error, TEXT("[EnsurePakFS] Failed to initialize FPakPlatformFile."));
    }
#endif
}

// ★★★ 핵심 1: MountPoint 인자를 비워서, pak 내부에 기록된 MountPoint를 그대로 사용
bool UPakMountManager::MountPak(const FString& PakAbsPath, int32 Order)
{
#if WITH_EDITOR
    if (!IsEditorRuntime())
    {
        UE_LOG(LogPakMountMgr, Warning, TEXT("[MountPak] 이 호출은 PIE/Standalone에서만 유효. (%s)"), *PakAbsPath);
        return false;
    }

    EnsurePakFS();

    if (!FPaths::FileExists(PakAbsPath))
    {
        UE_LOG(LogPakMountMgr, Error, TEXT("[MountPak] Pak 파일이 없음: %s"), *PakAbsPath);
        return false;
    }

    FPakPlatformFile* Pak = static_cast<FPakPlatformFile*>(FPlatformFileManager::Get().FindPlatformFile(TEXT("PakFile")));
    if (!Pak)
    {
        UE_LOG(LogPakMountMgr, Error, TEXT("[MountPak] Pak platform file 미초기화"));
        return false;
    }

    // MountPoint 비움 → pak에 내장된 MountPoint 사용 (불일치 문제 회피)
    const bool bOk = Pak->Mount(*PakAbsPath, Order, TEXT(""));

    UE_LOG(LogPakMountMgr, Log, TEXT("[MountPak] %s (Pak=%s, Order=%d)"),
        bOk ? TEXT("Success") : TEXT("Failed"), *PakAbsPath, Order);
    return bOk;
#else
    return false;
#endif
}

// ★★★ 핵심 2: 스캔 + 존재 확인을 강하게 진단
void UPakMountManager::ScanMountedContent(const FString& VirtualRoot, bool bForceRescan)
{
#if WITH_EDITOR
    if (!IsEditorRuntime())
    {
        UE_LOG(LogPakMountMgr, Warning, TEXT("[ScanMountedContent] PIE/Standalone에서 호출하세요. Root=%s"), *VirtualRoot);
        return;
    }

    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    TArray<FString> Paths; Paths.Add(VirtualRoot);
    ARM.Get().ScanPathsSynchronous(Paths, bForceRescan);

    UE_LOG(LogPakMountMgr, Log, TEXT("[ScanMountedContent] ScanPathsSynchronous(%s, Force=%s) 완료"),
        *VirtualRoot, bForceRescan ? TEXT("true") : TEXT("false"));
#endif
}

// ★★★ 핵심 3: OpenLevel 전, 해당 맵 패키지가 보이는지 즉석 검사
void UPakMountManager::OpenMountedLevel(UObject* WorldContext, const FString& LevelLongPackageName)
{
#if WITH_EDITOR
    if (!IsEditorRuntime())
    {
        UE_LOG(LogPakMountMgr, Warning, TEXT("[OpenMountedLevel] PIE/Standalone에서 호출하세요. Level=%s"),
            *LevelLongPackageName);
        return;
    }
    if (!WorldContext)
    {
        UE_LOG(LogPakMountMgr, Warning, TEXT("[OpenMountedLevel] WorldContext is null"));
        return;
    }

    // 1) /Game/.. 형태 보장
    if (!LevelLongPackageName.StartsWith(TEXT("/Game/")))
    {
        UE_LOG(LogPakMountMgr, Error, TEXT("[OpenMountedLevel] LongPackageName은 /Game/ 로 시작해야 함: %s"),
            *LevelLongPackageName);
        return;
    }

    // 2) 에셋 존재 검증: DoesPackageExist & Filename 변환 체크
    FString DummyFilename;
    const bool bPkgExists = FPackageName::DoesPackageExist(LevelLongPackageName, nullptr, &DummyFilename);
    UE_LOG(LogPakMountMgr, Log, TEXT("[OpenMountedLevel] DoesPackageExist(%s) = %s, File=%s"),
        *LevelLongPackageName, bPkgExists ? TEXT("true") : TEXT("false"), *DummyFilename);

    FString MapFilename;
    const bool bToFileOK = FPackageName::TryConvertLongPackageNameToFilename(LevelLongPackageName, MapFilename, TEXT(".umap"));
    UE_LOG(LogPakMountMgr, Log, TEXT("[OpenMountedLevel] ToFilename(.umap) = %s, File=%s"),
        bToFileOK ? TEXT("true") : TEXT("false"), *MapFilename);

    if (!bPkgExists || !bToFileOK)
    {
        UE_LOG(LogPakMountMgr, Error, TEXT("[OpenMountedLevel] 맵 파일을 찾을 수 없음. pak MountPoint/경로 재점검 필요."));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("-----------------OpenMountedLevel =[%s]"), *LevelLongPackageName);
    // 3) 실제 트래블 (반환형: void)
    UGameplayStatics::OpenLevel(WorldContext, FName(*LevelLongPackageName));

#else

#endif
}
