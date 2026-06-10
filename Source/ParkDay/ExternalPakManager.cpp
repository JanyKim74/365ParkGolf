#include "ExternalPakManager.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"
#include "IPlatformFilePak.h"          // FPakPlatformFile, FPakFile
#include "Modules/ModuleManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Algo/Unique.h"

DEFINE_LOG_CATEGORY(LogExternalPak);

// ─────────────────────────────────────────────────────────────────────────────

static bool IsEngineBasePak(const FString& FullPath)
{
    const FString EnginePaks = FPaths::ConvertRelativePathToFull(FPaths::EngineContentDir() / TEXT("Paks"));
    return FullPath.StartsWith(EnginePaks, ESearchCase::IgnoreCase);
}

static bool IsProjectBasePak_NonExternal(const FString& FullPath)
{
    const FString ProjectPaks = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Paks"));
    const FString ExternalPaks = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("ExternalPaks"));
    // 프로젝트 Paks 안에 있지만 ExternalPaks가 아닌 것 = 기본/자동 마운트 베이스로 간주
    return FullPath.StartsWith(ProjectPaks, ESearchCase::IgnoreCase) &&
        !FullPath.StartsWith(ExternalPaks, ESearchCase::IgnoreCase);
}

static FPakPlatformFile* GetPakPF()
{
    // PakMountLibrary 방식: 체인 전체 순회
    IPlatformFile* Top = &FPlatformFileManager::Get().GetPlatformFile();
    for (IPlatformFile* It = Top; It; It = It->GetLowerLevel())
    {
        if (FCString::Stricmp(It->GetName(), TEXT("PakFile")) == 0)
            return static_cast<FPakPlatformFile*>(It);
    }
    // 없으면 새로 래핑
    FPakPlatformFile* PakPF = new FPakPlatformFile();
    if (!PakPF->Initialize(Top, TEXT(""))) { delete PakPF; return nullptr; }
    FPlatformFileManager::Get().SetPlatformFile(*PakPF);
    return PakPF;
}

bool UExternalPakManager::EnsurePakSystem()
{
    return GetPakPF() != nullptr;
}

FString UExternalPakManager::ExternalPakDir() const
{
    return FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("ExternalPaks"));
}

void UExternalPakManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    EnsurePakSystem();
}

void UExternalPakManager::Deinitialize()
{
    Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
// 기본 탐색 루트 구성
void UExternalPakManager::BuildDefaultSearchRoots(TArray<FString>& OutRoots) const
{
    OutRoots.Reset();

    // 1) /Content/ExternalPaks
    OutRoots.Add(FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("ExternalPaks")));

    // 2) /Content/Paks (자동마운트 기본 위치)
    OutRoots.Add(FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Paks")));

    // 3) 프로젝트 루트 (Content 상위)
    OutRoots.Add(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));

    // 중복 제거
    OutRoots.Sort();
    const int32 UniqueCount = Algo::Unique(OutRoots);
    OutRoots.SetNum(UniqueCount, EAllowShrinking::Yes);
}

static FORCEINLINE bool IsAbsolute(const FString& Path)
{
    return FPaths::IsRelative(Path) == false;
}

bool UExternalPakManager::EndsWithPak(const FString& Name)
{
    return Name.EndsWith(TEXT(".pak"), ESearchCase::IgnoreCase);
}

static bool RegisterAndCheck(const FString& VirtualRoot, const FString& PhysicalRoot)
{
    FPackageName::UnRegisterMountPoint(VirtualRoot, PhysicalRoot);
    FPackageName::RegisterMountPoint(VirtualRoot, PhysicalRoot);

    auto Check = [&](const FString& Ext)
    {
        const FString TestLong = VirtualRoot + TEXT("__Check__");
        const FString Filename = FPackageName::LongPackageNameToFilename(TestLong, *Ext);
        return Filename.StartsWith(PhysicalRoot);
    };

    const bool bAssetOK = Check(FPackageName::GetAssetPackageExtension());
    const bool bMapOK = Check(FPackageName::GetMapPackageExtension());
    UE_LOG(LogTemp, Log, TEXT("[ExtRoot] %s -> %s  AssetOK=%d MapOK=%d"),
        *VirtualRoot, *PhysicalRoot, bAssetOK, bMapOK);
    return bAssetOK && bMapOK;
}

// ─────────────────────────────
// 공통: Content/ 직계 하위 폴더 스캔
static void ScanTopSubdirs(const FString& PhysicalContentRoot, TArray<FString>& OutSubdirs)
{
    IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();

    struct FVisitor : IPlatformFile::FDirectoryVisitor
    {
        TArray<FString>& Out;
        explicit FVisitor(TArray<FString>& In) : Out(In) {}
        virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
        {
            if (bIsDirectory)
            {
                Out.Add(FPaths::GetCleanFilename(FilenameOrDirectory));
            }
            return true;
        }
    };

    OutSubdirs.Reset();
    FVisitor Visitor(OutSubdirs);
    PF.IterateDirectory(*PhysicalContentRoot, Visitor);

    // 중복/공백 제거 (UE4.26 안전한 방식)
    OutSubdirs.Sort();
    {
        TArray<FString> Deduped; Deduped.Reserve(OutSubdirs.Num());
        TSet<FString> Seen;
        for (const FString& S : OutSubdirs)
        {
            if (!S.IsEmpty() && !Seen.Contains(S))
            {
                Seen.Add(S);
                Deduped.Add(S);
            }
        }
        OutSubdirs = MoveTemp(Deduped);
    }
}

bool UExternalPakManager::RegisterAll(const FString& ProjectName)
{
    bool bOK = true;

    // ✅ 패키지 빌드 기준 물리 경로
    // FPaths::ProjectContentDir() = ../../../ParkDay/Content/ (런타임)
    const FString PhysContent = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());

    // 1) /Game/ → Content/ 전체 매핑
    FPackageName::UnRegisterMountPoint(TEXT("/Game/"), PhysContent);
    FPackageName::RegisterMountPoint(TEXT("/Game/"), PhysContent);

    // 2) /Game/PakName/ → Content/PakName/ 매핑
    const FString VGameProj = FString::Printf(TEXT("/Game/%s/"), *ProjectName);
    const FString PPhysProj = PhysContent / ProjectName + TEXT("/");
    FPackageName::UnRegisterMountPoint(VGameProj, PPhysProj);
    FPackageName::RegisterMountPoint(VGameProj, PPhysProj);

    // 검증 로그
    FString TestFile;
    const bool bMapOK = FPackageName::TryConvertLongPackageNameToFilename(
        FString::Printf(TEXT("/Game/%s/%s"), *ProjectName, *ProjectName),
        TestFile, TEXT(".umap"));
    UE_LOG(LogExternalPak, Log,
        TEXT("[RegisterAll] /Game/%s/ → %s  MapOK=%d  TestFile=%s"),
        *ProjectName, *PPhysProj, bMapOK, *TestFile);

    return bMapOK;
}

// ─────────────────────────────────────────────────────────────────────────────
// AssetRegistry 스캔
void UExternalPakManager::RescanAssetRegistry(const TArray<FString>& PathsToScan)
{
    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    if (PathsToScan.Num() > 0)
    {
        ARM.Get().ScanPathsSynchronous(PathsToScan, /*bForceRescan=*/true);
    }
    else
    {
        TArray<FString> All{ TEXT("/Game") };
        ARM.Get().ScanPathsSynchronous(All, true);
    }
}

bool UExternalPakManager::MountPakAbsolute_AutoMountPoint(const FString& PakAbsolutePath, int32 PakOrder)
{
    const FString Full = FPaths::ConvertRelativePathToFull(PakAbsolutePath);

    if (!FPaths::FileExists(Full))
    {
        UE_LOG(LogExternalPak, Error, TEXT("❌ Pak not found: %s"), *Full);
        return false;
    }

    FPakPlatformFile* PakPF = GetPakPF();
    if (!PakPF)
    {
        UE_LOG(LogExternalPak, Error, TEXT("❌ No Pak platform file"));
        return false;
    }

    // 마운트 포인트를 ../../../ParkDay/Content/ 로 명시 (pak 내부 경로와 일치시킴)
    const FString MountPoint = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
    // /Game/ 마운트 포인트 보장
    FPackageName::RegisterMountPoint(TEXT("/Game/"), MountPoint);

    const bool bOK = PakPF->Mount(*Full, PakOrder, *MountPoint);

    UE_LOG(LogExternalPak, Log, TEXT("[MountPakAbsolute_Auto] %s (Order=%d) -> %s  MountPoint=%s"),
        *Full, PakOrder, bOK ? TEXT("OK") : TEXT("FAIL"), *MountPoint);

    if (bOK)
    {
        RescanAssetRegistry({ TEXT("/Game/") });
    }
    return bOK;
}
// ─────────────────────────────────────────────────────────────────────────────
// 이름으로 pak 찾기 → 절대경로 리턴
FString UExternalPakManager::ResolvePakPathByName(const FString& PakFileNameWithExt, bool bSearchRecursively) const
{
    FString Name = PakFileNameWithExt;
    if (!EndsWithPak(Name))
    {
        Name += TEXT(".pak");
    }

    // 이미 절대경로면 그대로 검사
    if (IsAbsolute(Name))
    {
        return FPaths::FileExists(Name) ? FPaths::ConvertRelativePathToFull(Name) : FString();
    }

    // 기본 탐색 루트들
    TArray<FString> Roots;
    BuildDefaultSearchRoots(Roots);

    // 빠른 시도: 각 루트 바로 아래에 파일이 있는지
    for (const FString& Root : Roots)
    {
        const FString Candidate = FPaths::ConvertRelativePathToFull(FPaths::Combine(Root, Name));
        if (FPaths::FileExists(Candidate))
        {
            return Candidate;
        }
    }

    if (!bSearchRecursively)
    {
        return FString();
    }

    // 재귀 검색: 각 루트에서 *.pak 수집 후 파일명 일치 비교
    for (const FString& Root : Roots)
    {
        TArray<FString> Found;
        IFileManager::Get().FindFilesRecursive(Found, *Root, TEXT("*.pak"), /*Files=*/true, /*Directories=*/false);

        for (const FString& Path : Found)
        {
            if (FPaths::GetCleanFilename(Path).Equals(Name, ESearchCase::IgnoreCase))
            {
                return FPaths::ConvertRelativePathToFull(Path);
            }
        }
    }

    return FString();
}

// ─────────────────────────────────────────────────────────────────────────────
// 요청 함수: pak 이름으로 찾아서 자동마운트 방식으로 마운트
bool UExternalPakManager::MountPakByName(const FString& PakFileNameWithExt, int32 PakOrder, bool bSearchRecursively)
{
    const FString Full = ResolvePakPathByName(PakFileNameWithExt, bSearchRecursively);
    if (Full.IsEmpty())
    {
        UE_LOG(LogExternalPak, Error, TEXT("❌ Pak file not found by name: %s"), *PakFileNameWithExt);
        return false;
    }
    return MountPakAbsolute_AutoMountPoint(Full, PakOrder);
}

// ─────────────────────────────────────────────────────────────────────────────
// 여러 개 마운트(ExternalPaks 폴더 기준)
void UExternalPakManager::MountAllExternalPaks(bool bRescan, int32 PakOrder)
{
    const FString Dir = ExternalPakDir();
    TArray<FString> Files;
    IFileManager::Get().FindFilesRecursive(Files, *Dir, TEXT("*.pak"), /*Files=*/true, /*Directories=*/false);

    if (Files.Num() == 0)
    {
        UE_LOG(LogExternalPak, Log, TEXT("No .pak found under: %s"), *Dir);
        return;
    }

    for (const FString& P : Files)
    {
        MountPakAbsolute_AutoMountPoint(FPaths::ConvertRelativePathToFull(P), PakOrder);
    }

    if (bRescan) { RescanAssetRegistry({ TEXT("/Game") }); }
}

//// ─────────────────────────────────────────────────────────────────────────────
//// 언마운트/조회/덤프
//bool UExternalPakManager::UnmountPak(const FString& PakAbsolutePath)
//{
//    const FString Full = FPaths::ConvertRelativePathToFull(PakAbsolutePath);
//
//    if (FPakPlatformFile* PakPF = GetPakPF())
//    {
//        const bool bOK = PakPF->Unmount(*Full);
//        UE_LOG(LogExternalPak, Log, TEXT("%s pak: %s"),
//            bOK ? TEXT("Unmounted") : TEXT("Failed to unmount"), *Full);
//        return bOK;
//    }
//    return false;
//}

void UExternalPakManager::GetMountedPaks(TArray<FString>& OutPakFiles) const
{
    OutPakFiles.Reset();
    IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
    if (PF.GetName() == FPakPlatformFile::GetTypeName())
    {
        static_cast<FPakPlatformFile&>(PF).GetMountedPakFilenames(OutPakFiles);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 디버그 도우미
static FORCEINLINE FString MakeUnixPath(const FString& In)
{
    FString Out = In;
    Out.ReplaceInline(TEXT("\\"), TEXT("/"));
    return Out;
}

void UExternalPakManager::DumpSinglePakContents(const FString& PakAbsolutePath, int32 MaxFiles, bool bShowVirtualGamePath)
{
    const FString FullPak = FPaths::ConvertRelativePathToFull(PakAbsolutePath);

    FPakPlatformFile* PakPF = GetPakPF();
    if (!PakPF)
    {
        UE_LOG(LogExternalPak, Error, TEXT("[DumpSinglePakContents] No Pak platform file"));
        return;
    }

    // ✅ UE5.6: FPakListEntry(private) 대신 GetMountedPakFilenames()로 마운트 확인
    TArray<FString> MountedFilenames;
    PakPF->GetMountedPakFilenames(MountedFilenames);

    const bool bIsMounted = MountedFilenames.ContainsByPredicate([&](const FString& F)
        {
            return FPaths::ConvertRelativePathToFull(F).Equals(FullPak, ESearchCase::IgnoreCase);
        });

    // 마운트 안 되어 있으면 임시로 마운트
    bool bDidTempMount = false;
    if (!bIsMounted)
    {
        UE_LOG(LogExternalPak, Warning,
            TEXT("[DumpSinglePakContents] Not mounted, temp mounting: %s"), *FullPak);
        bDidTempMount = PakPF->Mount(*FullPak, 0);
        if (!bDidTempMount)
        {
            UE_LOG(LogExternalPak, Error,
                TEXT("[DumpSinglePakContents] Failed to mount: %s"), *FullPak);
            return;
        }
    }

    UE_LOG(LogExternalPak, Log, TEXT("┌────────────────────────────────────────────"));
    UE_LOG(LogExternalPak, Log, TEXT("│ PAK: %s"), *FullPak);

    // ✅ UE5.6: FPakPlatformFile로 /Game/ 아래 파일 목록 수집
    TArray<FString> FoundFiles;
    PakPF->FindFilesRecursively(FoundFiles, TEXT("/Game/"), nullptr);

    // 이 pak에 속한 파일만 필터링 (마운트 직후라면 전체가 이 pak 소속)
    int32 Printed = 0;
    for (const FString& FilePath : FoundFiles)
    {
        if (MaxFiles > 0 && Printed >= MaxFiles)
        {
            UE_LOG(LogExternalPak, Log, TEXT("│  ...more entries omitted"));
            break;
        }
        UE_LOG(LogExternalPak, Log, TEXT("│  - %s"), *FilePath);
        ++Printed;
    }

    UE_LOG(LogExternalPak, Log, TEXT("│ Total printed: %d"), Printed);
    UE_LOG(LogExternalPak, Log, TEXT("└────────────────────────────────────────────"));

    // 임시 마운트했으면 다시 언마운트
    if (bDidTempMount)
    {
        PakPF->Unmount(*FullPak);
    }
}

void UExternalPakManager::DumpMountedPakContents(int32 MaxFilesPerPak, bool bShowVirtualGamePath)
{
    TArray<FString> MountedPakFiles;
    GetMountedPaks(MountedPakFiles);

    if (MountedPakFiles.Num() == 0)
    {
        UE_LOG(LogExternalPak, Warning, TEXT("[DumpMountedPakContents] No mounted pak files."));
        return;
    }

    for (const FString& PakPath : MountedPakFiles)
    {
        const FString Full = FPaths::ConvertRelativePathToFull(PakPath);
        DumpSinglePakContents(Full, MaxFilesPerPak, bShowVirtualGamePath);
    }
}


// 절대경로 언마운트
bool UExternalPakManager::UnmountPak(const FString& PakAbsolutePath, bool bPreFlush, bool bPostRescan)
{
    const FString Full = FPaths::ConvertRelativePathToFull(PakAbsolutePath);

    if (!FPaths::FileExists(Full))
    {
        UE_LOG(LogExternalPak, Warning, TEXT("[UnmountPak] File not on disk (still try): %s"), *Full);
    }

    if (bPreFlush) { FlushAllAndCollectGarbage(); }

    if (FPakPlatformFile* PakPF = GetPakPF())
    {
        const bool bOK = PakPF->Unmount(*Full);
        UE_LOG(LogExternalPak, Log, TEXT("[UnmountPak] %s -> %s"),
            *Full, bOK ? TEXT("OK") : TEXT("FAIL"));

        if (bOK && bPostRescan)
        {
            RescanAssetRegistry({ TEXT("/Game") });
        }
        return bOK;
    }

    UE_LOG(LogExternalPak, Error, TEXT("[UnmountPak] No Pak platform file"));
    return false;
}

// 이름(.pak 포함 가능)으로 찾아 언마운트
bool UExternalPakManager::UnmountPakByName(const FString& PakFileNameWithExt,
    bool bSearchRecursively,
    bool bPreFlush, bool bPostRescan)
{
    const FString Full = ResolvePakPathByName(PakFileNameWithExt, bSearchRecursively);
    if (Full.IsEmpty())
    {
        UE_LOG(LogExternalPak, Error, TEXT("❌ Pak not found by name: %s"), *PakFileNameWithExt);
        return false;
    }
    return UnmountPak(Full, bPreFlush, bPostRescan);
}

// 파일명/경로에 특정 문자열이 포함된 pak들 일괄 언마운트(대소문자 무시)
int32 UExternalPakManager::UnmountPaksByNameContains(const FString& NameContains,
    bool bPreFlush, bool bPostRescan)
{
    if (NameContains.IsEmpty())
    {
        UE_LOG(LogExternalPak, Warning, TEXT("[UnmountPaksByNameContains] Empty key"));
        return 0;
    }

    if (bPreFlush) { FlushAllAndCollectGarbage(); }

    TArray<FString> Mounted;
    GetMountedPaks(Mounted);

    int32 Count = 0;
    for (const FString& PakPath : Mounted)
    {
        if (PakPath.Contains(NameContains, ESearchCase::IgnoreCase))
        {
            if (FPakPlatformFile* PakPF = GetPakPF())
            {
                const bool bOK = PakPF->Unmount(*PakPath);
                UE_LOG(LogExternalPak, Log, TEXT("[UnmountByContains] %s -> %s"),
                    *PakPath, bOK ? TEXT("OK") : TEXT("FAIL"));
                if (bOK) { ++Count; }
            }
        }
    }

    if (Count > 0 && bPostRescan)
    {
        RescanAssetRegistry({ TEXT("/Game") });
    }

    return Count;
}

// 기본 탐색 루트(ExternalPaks/Paks/Project)에 있는 것만 전부 언마운트
int32 UExternalPakManager::UnmountAllInSearchRoots(bool bPreFlush, bool bPostRescan)
{
    if (bPreFlush) { FlushAllAndCollectGarbage(); }

    TArray<FString> Roots;
    BuildDefaultSearchRoots(Roots);

    TArray<FString> Mounted;
    GetMountedPaks(Mounted);

    int32 Count = 0;
    for (const FString& PakPath : Mounted)
    {
        const FString Full = FPaths::ConvertRelativePathToFull(PakPath);

        bool bInRoots = false;
        for (const FString& R : Roots)
        {
            if (Full.StartsWith(R, ESearchCase::IgnoreCase)) { bInRoots = true; break; }
        }

        if (bInRoots)
        {
            if (FPakPlatformFile* PakPF = GetPakPF())
            {
                const bool bOK = PakPF->Unmount(*Full);
                UE_LOG(LogExternalPak, Log, TEXT("[UnmountInRoots] %s -> %s"),
                    *Full, bOK ? TEXT("OK") : TEXT("FAIL"));
                if (bOK) { ++Count; }
            }
        }
    }

    if (Count > 0 && bPostRescan)
    {
        RescanAssetRegistry({ TEXT("/Game") });
    }

    return Count;
}

int32 UExternalPakManager::UnmountAllMountedPaks(bool bPreFlush, bool bPostRescan, bool bIncludeBasePaks)
{
    if (bPreFlush) { FlushAllAndCollectGarbage(); }

    TArray<FString> Mounted;
    GetMountedPaks(Mounted);

    int32 Count = 0;
    for (const FString& PakPath : Mounted)
    {
        const FString Full = FPaths::ConvertRelativePathToFull(PakPath);

        // 기본값: 엔진/프로젝트 베이스 pak은 건드리지 않음
        if (!bIncludeBasePaks && (IsEngineBasePak(Full) || IsProjectBasePak_NonExternal(Full)))
        {
            UE_LOG(LogExternalPak, VeryVerbose, TEXT("[KEEP-BASE] %s"), *Full);
            continue;
        }

        if (FPakPlatformFile* PakPF = GetPakPF())
        {
            const bool bOK = PakPF->Unmount(*Full);
            UE_LOG(LogExternalPak, Log, TEXT("[UnmountAllMounted] %s -> %s"),
                *Full, bOK ? TEXT("OK") : TEXT("FAIL"));
            if (bOK) { ++Count; }
        }
    }

    if (Count > 0 && bPostRescan)
    {
        RescanAssetRegistry({ TEXT("/Game") });
    }
    return Count;
}
void UExternalPakManager::FlushAllAndCollectGarbage()
{
    // 1) 레벨 스트리밍 정리 (월드가 있으면)
    if (UWorld* World = GetWorld())
    {
        World->FlushLevelStreaming(EFlushLevelStreamingType::Full);
    }

    // 2) 렌더 명령 큐 비우기 (4.26 사용 가능)
    FlushRenderingCommands();

    // 3) 패키지 비동기 로딩 완료 대기
    FlushAsyncLoading();

    // 4) 강제 GC
    GEngine->ForceGarbageCollection(true);

    // 5) (선택) 한 번 더 렌더 큐 정리
    FlushRenderingCommands();
}