// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "ParkDay/GolfDataStructures.h"
#include "ParkDay/Structs/CorseStruct.h"
#include "ParkDay/Widgets/FadeWidget.h"
#include "UtilLibrary.generated.h"
/**
 * 
 */
UCLASS()
class PARKDAY_API UUtilLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "MyLib|World",
        meta = (WorldContext = "WorldContextObject"))
    static AGameModeBase* GetGameModeBP(UObject* WorldContextObject);

    // 로컬 플레이어 컨트롤러
    UFUNCTION(BlueprintPure, Category = "MyLib|World",
        meta = (WorldContext = "WorldContextObject"))
        static APlayerController* GetPlayerControllerBP(UObject* WorldContextObject);

    UFUNCTION(BlueprintPure, Category = "MyLib|World",
        meta = (WorldContext = "WorldContextObject"))
        static int32 GetCurrentPlayerShotCount(UObject* WorldContextObject);

        UFUNCTION(BlueprintPure, Category = "MyLib|World",
        meta = (WorldContext = "WorldContextObject"))
        static int32 GetCurrentPlayerIndex(UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "MyLib|World",
        meta = (WorldContext = "WorldContextObject"))
        static int32 GetCurrentScore(UObject* WorldContextObject);

    	UFUNCTION(BlueprintPure, Category = "MyLib|World",
        meta = (WorldContext = "WorldContextObject"))
        static bool GetIsConcede(UObject* WorldContextObject);

    UFUNCTION(BlueprintCallable, Category = "MyLib|Level", meta = (WorldContext = "WorldContextObject", DisplayName = "Open Level (Long Package Name)"))
        static void OpenLevelCPP(UObject* WorldContextObject, const FString& LongPackageLevelPath, const FString& Options);

    UFUNCTION(BlueprintCallable, Category = "Util|Sort",
        meta = (WorldContext = "WorldContextObject", HidePin = "WorldContextObject",
            DisplayName = "Sort Players by Slot (In-Place)"))
        static void SortPlayersBySlot(UObject* WorldContextObject,
            UPARAM(ref) TArray<FPlayerInfo>& Players);

        UFUNCTION(BlueprintCallable, Category = "Util|Sort",
        meta = (WorldContext = "WorldContextObject", HidePin = "WorldContextObject",
            DisplayName = "Sort String (In-Place)"))
        static void SortString(UObject* WorldContextObject,
            UPARAM(ref) TArray<FString>& Strings);

    UFUNCTION(BlueprintCallable, Category = "Util|Sort",
        meta = (WorldContext = "WorldContextObject", HidePin = "WorldContextObject",
            DisplayName = "SoftReset"))
        static void SoftResetGameInfo(UObject* WorldContextObject, UPARAM(ref)
            FGameInfo& GameInfo);

    UFUNCTION(BlueprintCallable, Category = "Util|Sort",
        meta = (WorldContext = "WorldContextObject", HidePin = "WorldContextObject",
            DisplayName = "StopLoadingScreen"))
        static void StopLoadingScreen(UObject* WorldContextObject);


    UFUNCTION(BlueprintCallable, Category = "MyLib|Level", meta = (WorldContext = "WorldContextObject", DisplayName = "UnMountPak"))
        static void UnMountPak(UObject* WorldContextObject);

    
    UFUNCTION(BlueprintCallable, Category = "Widget|Fade", meta = (WorldContext = "WorldContextObject", DisplayName = "FadeInOut"))
        static void FadeInOut(UObject* WorldContextObject, float FadeInDuration, float HoldDuration, float FadeOutDuration);
    static void FadeIn(UObject* WorldContextObject, float FadeInDuration);
    static void FadeIn(UObject* WorldContextObject, float FadeInDuration, FFadeCallback CallBack);

	UFUNCTION(BlueprintCallable, Category = "Widget|Fade", meta = (WorldContext = "WorldContextObject", DisplayName = "FadeOut"))
		static void FadeOut(UObject* WorldContextObject, float FadeOutDuration);

	UFUNCTION(BlueprintCallable, Category = "Widget|Fade", meta = (WorldContext = "WorldContextObject", DisplayName = "LockButton"))
		static void LockButtonForSeconds(UButton* Button, UObject* WorldContext, float LockSeconds);
	UFUNCTION(BlueprintCallable, Category = "Widget|Fade", meta = (WorldContext = "WorldContextObject", DisplayName = "LockCheckBox"))
		static void LockCheckBoxForSeconds(UCheckBox* CheckBox, UObject* WorldContext, float LockSeconds);
    /**
	 * A가 B를 향하는 방향(A->B)으로 DistanceUU 만큼 이동.
	 * - 회전은 보존(이동 전 회전으로 복구)
	 */
	UFUNCTION(BlueprintCallable, Category="ParkGolf|Movement")
	static bool MoveActorTowardActorByDistance_KeepRotation(
		AActor* Mover,
		const AActor* Target,
		float DistanceUU,
		bool bKeepMoverZ,
		bool bSweep,
		bool bTeleport,
		FHitResult& OutHit
	);

    
	/**
	 * 간단 버전(수평 이동 기본, 스윕/텔레포트/히트결과 없음)
	 */
	UFUNCTION(BlueprintCallable, Category="ParkGolf|Movement")
	static bool MoveActorTowardActorByDistanceSimple_KeepRotation(
		AActor* Mover,
		const AActor* Target,
		float DistanceUU,
		bool bKeepMoverZ = true
	);

	UFUNCTION(BlueprintCallable, Category = "Widget|Fade", meta = (WorldContext = "WorldContextObject", DisplayName = "LockButton"))
		static bool MoveActorTowardActorByDistance(
            AActor* Mover,
            const AActor* Target,
            float DistanceUU,
            bool bKeepMoverZ,
            bool bSweep,
            bool bTeleport,
            FHitResult& OutHit
        );

    	UFUNCTION(BlueprintCallable, Category="ParkGolf|Movement")
	static bool MoveActorTowardActorByDistanceSimple(
		AActor* Mover,
		const AActor* Target,
		float DistanceUU,
		bool bKeepMoverZ = true
	);

    //UFUNCTION(BlueprintCallable, Category = "Level|Load", meta = (WorldContext = "WorldContextObject", DisplayName = "Open Level (Name or Path)"))
    //    static void OpenLevel_NameOrPath(UObject* WorldContextObject, const FString& NameOrPath, const FString& Options, FString& ResolvedLongPath);

    //UFUNCTION()
    //    bool TryResolveLongFromName(const FString& InNameOrPath, FString& OutLongPath);

    //UFUNCTION()
    //    bool ResolveLevelPath_FromName(const FString& NameOrPath, FString& OutLongPackagePath);

    template<typename RowStruct, typename KeyType, typename ValueType>
    static bool DataTableToMap(
        UDataTable* DataTable,
        TMap<KeyType, ValueType>& OutMap,
        TFunctionRef<KeyType(const RowStruct&)> MakeKey,
        TFunctionRef<ValueType(const RowStruct&)> MakeValue)
    {
        OutMap.Reset();

        if (!DataTable)
        {
            UE_LOG(LogTemp, Error, TEXT("DataTableToMap: DataTable is null"));
            return false;
        }

        if (DataTable->GetRowStruct() != RowStruct::StaticStruct())
        {
            UE_LOG(LogTemp, Error, TEXT("RowStruct type mismatch! Expected %s but got %s"),
                *RowStruct::StaticStruct()->GetName(),
                *DataTable->GetRowStruct()->GetName());
            return false;
        }

        DataTable->ForeachRow<RowStruct>(TEXT("DataTableToMap"),
            [&](const FName& RowName, const RowStruct& Row)
            {
                OutMap.Add(MakeKey(Row), MakeValue(Row));
            });

        return true;
    }

    UFUNCTION(BlueprintCallable, Category = "Util|InGame",
        meta = (WorldContext = "WorldContextObject", HidePin = "WorldContextObject",
            DisplayName = "AddPlayerToInGame"))
    static void AddPlayerToInGame(UObject* WorldContextObject, FPlayerInfo PlayerInfo);

        UFUNCTION(BlueprintCallable, Category = "Util|InGame",
        meta = (WorldContext = "WorldContextObject", HidePin = "WorldContextObject",
            DisplayName = "RemovePlayerToInGame"))
    static void RemovePlayerToInGame(UObject* WorldContextObject, FPlayerInfo PlayerInfo);

        
    UFUNCTION(BlueprintCallable, Category = "Util|Reset",
        meta = (WorldContext = "WorldContextObject", HidePin = "WorldContextObject",
            DisplayName = "ResetGameData"))
    static void ResetGameData(UObject* WorldContextObject);


    static FString GetDataPath(const FString& RelativePath)
    {
        // 에디터:   C:\Dev_SmileUp\365ParkGolf\Content\DATA\...
        // 패키징:   C:\ParkDay\Content\DATA\...   (ProjectDir + Content/)
        // 둘 다 동일하게 동작
        return FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectDir(), TEXT("Content"), RelativePath)
        );
    }

    static FString GetSavedPath(const FString& RelativePath)
    {
        return FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectSavedDir(), RelativePath)
        );
    }
};
