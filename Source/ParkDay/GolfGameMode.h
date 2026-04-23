// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GolfDataStructures.h"
#include "GolfGameMode.generated.h"

/**
 * // 최상위 게임 모드: 메뉴와 인게임 모드를 관리하며 디폴트 플레이어 설정
 */
UCLASS()
class PARKDAY_API AGolfGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AGolfGameMode();

    // 게임 시작 시 호출
    virtual void BeginPlay() override;

    // 메뉴 모드로 전환
    UFUNCTION(BlueprintCallable, Category = "GameMode")
        void SwitchToMenuMode();

    // 인게임 모드로 전환
    UFUNCTION(BlueprintCallable, Category = "GameMode")
        void SwitchToInGameMode();

    // 디폴트 플레이어 초기화
    UFUNCTION(BlueprintCallable, Category = "Player")
        void InitializeDefaultPlayers();

    // 게임 정보
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameInfo")
        FGameInfo GameInfo;

protected:
    // JSON 저장 파일 경로
    FString SaveFilePath;
};