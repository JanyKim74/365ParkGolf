#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ExternalPakManager.generated.h"

/**
 * 외부 pak 마운트 매니저 (UE4.26)
 * - pak 이름만 받아서 기본 탐색 경로에서 찾아 자동마운트 방식(내장 MountPoint 사용)으로 마운트
 * - 절대경로가 들어오면 그대로 마운트
 * - /Content/ExternalPaks, /Content/Paks 등을 기본 탐색
 * - AssetRegistry 스캔 지원
 */

DECLARE_LOG_CATEGORY_EXTERN(LogExternalPak, Log, All);

UCLASS(BlueprintType)
class PARKDAY_API UExternalPakManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    bool RegisterAll(const FString& ProjectName);

    /** /Content/ExternalPaks 아래 모든 .pak 마운트 (pak 내 MountPoint 사용) */
    UFUNCTION(BlueprintCallable, Category = "ExternalPaks")
        void MountAllExternalPaks(bool bRescanAssetRegistry = true, int32 PakOrder = 0);

    /**
     * pak "절대경로"를 자동마운트 방식(내장 MountPoint 사용)으로 마운트
     *  - PakOrder: 자동마운트(보통 0)보다 우선시키려면 큰 수 지정
     */
    UFUNCTION(BlueprintCallable, Category = "ExternalPaks")
        bool MountPakAbsolute_AutoMountPoint(const FString& PakAbsolutePath, int32 PakOrder = 0);

    /** 현재 마운트된 pak 파일 경로 나열 */
    UFUNCTION(BlueprintCallable, Category = "ExternalPaks")
        void GetMountedPaks(TArray<FString>& OutPakFiles) const;

    /** 특정 경로만 강제 재스캔하고 싶을 때 (기본은 /Game 전체 스캔 권장) */
    UFUNCTION(BlueprintCallable, Category = "ExternalPaks")
        void RescanAssetRegistry(const TArray<FString>& PathsToScan);

    /** 디버그: 현재 마운트된 모든 pak의 목록/내용 일부 출력 */
    UFUNCTION(BlueprintCallable, Category = "ExternalPaks|Debug")
        void DumpMountedPakContents(int32 MaxFilesPerPak = 100, bool bShowVirtualGamePath = true);

    /** 디버그: pak 하나만 덤프 */
    UFUNCTION(BlueprintCallable, Category = "ExternalPaks|Debug")
        void DumpSinglePakContents(const FString& PakAbsolutePath, int32 MaxFiles = 100, bool bShowVirtualGamePath = true);

    /**
     * ✅ 요청사항: "pak 파일 이름(.pak 포함)을 주면 찾아서 자동마운트처럼 마운트"
     *  - 입력: Pak 파일명(예: "MyCourse-WindowsNoEditor.pak") 또는 절대경로
     *  - 동작:
     *      1) 절대경로면 그대로 Mount
     *      2) 아니면 기본 탐색 루트에서 재귀 검색하여 첫 일치 파일을 Mount
     *  - 기본 탐색 루트:
     *      - <Project>/Content/ExternalPaks
     *      - <Project>/Content/Paks
     *      - <Project> (Content 상위; 필요시 커버)
     */
    UFUNCTION(BlueprintCallable, Category = "ExternalPaks")
        bool MountPakByName(const FString& PakFileNameWithExt, int32 PakOrder = 0, bool bSearchRecursively = true);

    /** pak 파일명을 절대경로로 해석 (찾아서 풀패스 리턴, 실패 시 빈 문자열) */
    UFUNCTION(BlueprintCallable, Category = "ExternalPaks")
        FString ResolvePakPathByName(const FString& PakFileNameWithExt, bool bSearchRecursively = true) const;


    UFUNCTION(BlueprintCallable, Category = "ExternalPaks")
        bool UnmountPak(const FString& PakAbsolutePath,
            bool bPreFlush = true, bool bPostRescan = true);

    UFUNCTION(BlueprintCallable, Category = "ExternalPaks")
        bool UnmountPakByName(const FString& PakFileNameWithExt,
            bool bSearchRecursively = true,
            bool bPreFlush = true, bool bPostRescan = true);

    UFUNCTION(BlueprintCallable, Category = "ExternalPaks")
        int32 UnmountPaksByNameContains(const FString& NameContains,
            bool bPreFlush = true, bool bPostRescan = true);

    UFUNCTION(BlueprintCallable, Category = "ExternalPaks")
        int32 UnmountAllInSearchRoots(bool bPreFlush = true, bool bPostRescan = true);

    // ExternalPakManager.h
    UFUNCTION(BlueprintCallable, Category = "ExternalPaks")
        int32 UnmountAllMountedPaks(bool bPreFlush = true, bool bPostRescan = true,
            bool bIncludeBasePaks /*위험!*/ = false);

    UFUNCTION(BlueprintCallable, Category = "ExternalPaks")
        void FlushAllAndCollectGarbage();


private:
    // 내부
    bool EnsurePakSystem();
    FString ExternalPakDir() const;

    // 탐색 루트 생성
    void BuildDefaultSearchRoots(TArray<FString>& OutRoots) const;

    // 문자열 유틸
    static bool EndsWithPak(const FString& Name);
};
