#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GolfDataStructures.h"
#include "Delegates/DelegateCombinations.h" // ⭐ 이 라인을 추가합니다.
#include "GolfPlayer.generated.h"


// 순방향 선언
class AInGameMode;
class AGolfBall;
class GolfPlayerManager;


UCLASS()
class PARKDAY_API AGolfPlayer : public AActor
{
    GENERATED_BODY()

public:
    AGolfPlayer();

public:
    // 플레이어 정보 설정
    void SetPlayerInfo(const FPlayerInfo& Info);

    // 플레이어 상태 설정
    void SetPlayerState(EPlayerState NewState);

    // 볼 위치 업데이트
    void UpdateBallPosition(FVector NewPosition);

    // 샷 준비
    void PrepareShot(FVector Direction, float Power);

    // 샷 실행
    void ExecuteShot();

    void SetPlayerInfoToGameInfo();
    bool CheckLastHoleOut();
    
    //드랍
    bool bDropAlready = false;
    // 샷 횟수 증가
    UFUNCTION(BlueprintCallable, Category = "Player")
        void IncrementShotCount();

    // 현재 홀 샷 횟수 조회
    UFUNCTION(BlueprintPure, Category = "Player")
        int32 GetCurrentHoleShotCount() const;

    // 홀 시작 시 샷 횟수 초기화
    UFUNCTION(BlueprintCallable, Category = "Player")
        void ResetShotCountForHole(int32 HoleIndex);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player")
        int32 PlayerIndex;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player")
        int32 SlotIndex;

    UPROPERTY()
    FRoundStat RoundStat;

    bool bIsRuntimeAdded = false;
    bool bIsPendingDelete = false;

    const FPlayerInfo& GetPlayerInfo() const { return PlayerInfo; }

    // 플레이어 상태 설정
    const EPlayerState& GetPlayerState() const { return CurrentPlayerState; }

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player")
        FPlayerInfo PlayerInfo;

    UFUNCTION(BlueprintCallable, Category = "Player Result")
        void ProcessShotResult(bool bHoleIn, bool bOutOfBounds, bool bIsConceded = false);


    UFUNCTION(BlueprintCallable, Category = "Player Result")
        void ProcessOBResult();

    // 더블파 체크
    UFUNCTION(BlueprintCallable, Category = "Player")
        bool CheckDoublePar() const;

    // 홀인 상태 반환
    UFUNCTION(BlueprintPure, Category = "Player")
        bool IsHoleIn() const { return bIsHoleIn; }

    // 홀인 상태 설정
    UFUNCTION(BlueprintCallable, Category = "Player")
        void SetHoleIn(bool bHoleIn);

    bool CheckChance();

    bool EnableMulligan();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mulligan")
        FVector BEFOREPos; // 샷 전 위치 저장

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mulligan")
        int32 LastStrokeCount; // 샷 전 타수 저장

    void TakeShot(FVector Direction, float Power);
    void UseMulligan();
    void UseOK();

    // 플레이어 상태가 변경될 때 호출될 델리게이트
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerStateChanged, int32, InPlayerIndex, EPlayerState, NewState);

    UPROPERTY(BlueprintAssignable, Category = "Player|Events")
        FOnPlayerStateChanged OnPlayerStateChangedDelegate;

    bool bIsContinue = false;
    float BallSpeed = 0;
    float ShotDistance = 0;
    float ShotPitchAngle = 0;
    float ShotYawAngle = 0;

    void UpdateBallSpeedAndAngles();
    void UpdateShotDistance();

    UFUNCTION()
        float GetSecsorShotPower();
protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player")
        bool bIsHoleIn;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Settings")
        int32 DoubleParThreshold = 2; // Par + 2

    UFUNCTION(BlueprintCallable, Category = "Aim")
        FVector FindAimPosition();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player")
        EPlayerState CurrentPlayerState;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player")
        FVector ShotDirection;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player")
        float ShotPower;


    UFUNCTION(BlueprintPure, Category = "Player Debug")
        FString GetPlayerStateString() const;

    UFUNCTION(BlueprintCallable, Category = "Player State")
        bool CanTransitionToState(EPlayerState NewState) const;

    void OnPlayerStateChanged(EPlayerState OldState, EPlayerState NewState);
    void OnEnterReadyState();
    void OnEnterShotState();
    void OnEnterResultsState();
    void OnEnterInitState();
    void OnEnterDesState();
    void OnEnterHoleOutState();

    void HandleHoleInResult();
    void HandleNormalResult();
    void DisplayShotResult();
    void SpawnShotParticle();


    // GolfPlayer.h에 추가할 함수 선언들
private:
    void SafeHandleTeeAnimation(AInGameMode* GameMode);
    void SafeHandleStrokeWidget(AInGameMode* GameMode, AGolfBall* Ball, float Distance, float Height);
    void SafeHandleChanceDisplay(AInGameMode* GameMode, AGolfBall* Ball);


public:
    bool bShotResultProcessed = false;
    bool bLastShotHoleIn = false;
    bool bLastShotOB = false;
    int32 CurrentHoleScore = 0;

    // ⭐ 새로 추가: 샷 위젯을 숨기기 위한 타이머 핸들
    FTimerHandle HoleOutWidgetHideTimerHandle;
    UFUNCTION()
        void HideHoleOutWidgetTimed();


public:
    // 첫 샷을 위한 최적 에임 위치 찾기
    UFUNCTION(BlueprintCallable, Category = "Aim")
        FVector FindFirstShotAimPosition();

private:
    // 티-홀 방향 50미터 지점에서 OB 회피 위치 찾기
    FVector FindSafeAimPositionFromTee(
        const FVector& StartPosition,
        const FVector& HolecupPosition,
        const FRotator& TeeRotation,    // ⭐ 추가
        float TargetDistance
    );

    // OB 라인 충돌 검사
    bool CheckOBCollisionAlongPath(const FVector& StartPos, const FVector& EndPos, int32 SampleCount = 10);

    // OB 회피 위치 계산
    FVector CalculateOBAvoidancePosition(const FVector& TeePos, const FVector& HolecupPos, const FVector& ProblemDirection, float Distance);
};
