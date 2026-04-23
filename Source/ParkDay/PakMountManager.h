#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PakMountManager.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPakMountMgr, Log, All);

/**
 * 에디터(PIE/에디터 월드)에서만 동작하는 Pak 마운터.
 * - 패키지/실행 빌드에서는 모든 API가 NO-OP (false 반환/아무것도 안 함).
 * - 함수 시그니처는 동일하게 유지하여 빌드 환경 상관없이 블루프린트/코드 호출 가능.
 */
UCLASS()
class PARKDAY_API UPakMountManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Pak을 마운트 (에디터에서만 동작). 성공 시 true */
    UFUNCTION(BlueprintCallable, Category = "Pak")
        bool MountPak(const FString& PakAbsPath, int32 Order = 100);

    /** Pak 언마운트 (에디터에서만 동작). 성공 시 true */
    //UFUNCTION(BlueprintCallable, Category = "Pak")
    //    bool UnmountPak(const FString& PakAbsPath);

    /** 에셋 레지스트리 스캔 (에디터에서만 동작). VirtualRoot 예: "/Game" */
    UFUNCTION(BlueprintCallable, Category = "Pak")
        void ScanMountedContent(const FString& VirtualRoot = TEXT("/Game"), bool bForceRescan = true);

    /** 에디터에서 테스트로 맵 열기 (OpenLevel). 에디터에서만 동작 */
    UFUNCTION(BlueprintCallable, Category = "Pak", meta = (WorldContext = "WorldContext"))
        void OpenMountedLevel(UObject* WorldContext, const FString& LevelLongPackageName);

private:
    /** PakFile 파일시스템을 보장 (에디터에서만 호출) */
    void EnsurePakFS();

    /** 현재가 에디터 런타임(에디터/PIE)인가? */
    bool IsEditorRuntime() const;
};