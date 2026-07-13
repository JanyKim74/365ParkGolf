#pragma once
#include "CoreMinimal.h"
#include "GolfDataStructures.h"
#include "Components/ActorComponent.h"
#include "UObject/NoExportTypes.h"
#include "CR2SensorManager.h"
#include "GolfPlayerManager.generated.h"

/**
 *
 */
class AGolfPlayer;
class AGolfPlayerController;
class AGolfBall;
class ACameraManager;
class AInGameMode;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PARKDAY_API UGolfPlayerManager : public UActorComponent
{
    GENERATED_BODY()

public:

    UGolfPlayerManager();
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void BeginDestroy() override;

    FTimerHandle TH;
    FTimerHandle TH2;
    AGolfPlayer* LatestShotPlayer;
    bool IsBillboardVisible = false;

    // ⭐ 게임 모드 참조 함수들 추가
    UFUNCTION(BlueprintCallable, Category = "Game Mode")
        bool IsStrokeMode() const;

    UFUNCTION(BlueprintCallable, Category = "Game Mode")
        bool IsTrainingMode() const;

    UFUNCTION(BlueprintCallable, Category = "Game Mode")
        bool IsRangeMode() const;


    void UpdateGameInfoBallPos();

    // 플레이어 초기화
    void InitializePlayers(const TArray<FPlayerInfo>& PlayerInfos, UWorld* World, const FMapInfo& MapInfo, int32 CurrentHole);

    void ResetPlayers();

    // 티샷 순서 정렬
    void SortTeeShotPlayerOrder(int32 CurrentHole, bool bExcludeHoleOut = false);

    // 플레이어 상태 업데이트
    void UpdatePlayerState(int32 PlayerIndex, EPlayerState NewState);

    void ChangeNickName(AGolfPlayer* Player, FString NickName);

    // 볼 상태 업데이트
    void UpdateBallState(int32 PlayerIndex, EBallState NewState);

    // 샷 처리
    void ProcessPlayerShot(int32 PlayerIndex, const FVector& Direction, float Power);

    // 홀 완료 여부 확인
    bool IsHoleComplete(int32 CurrentHole) const;

    // 현재 플레이어 샷 완료 여부
    bool IsCurrentPlayerShotComplete() const;

    void AdvanceToNextPlayer(int32 CurrentPlayerIndex);

    // Rebuild player indices and turn order (PlayerIndex order).
    void RebuildPlayerIndicesAndOrder(int32 PreserveSlotIndex);

    void AddPlayerInfoSlot(AGolfPlayer* Player, FPlayerInfo PlayerInfo);

    void RemovePlayerInfoSlot(FPlayerInfo PlayerInfo);

    // Getter
    TArray<AGolfPlayer*> GetPlayers() const { return Players; }
    AGolfPlayerController* GetPlayerController() const { return PlayerController; }
    TArray<AGolfBall*> GetPlayerBalls() const { return PlayerBalls; }
    ACameraManager* GetCameraManager() const { return CameraManager; }

    void AddPlayer(AGolfPlayer* Player);

    void AddBall(AGolfBall* Ball);

    int32 FindNextPlayer(int32 CurrentPlayerIndex);
    void SetupNextPlayer(int32 NextPlayerIndex);
    void HandleAllPlayersComplete();
    void SortPlayers();
    // 순서 정렬 함수들
    void SortTeeShotPlayerOrderWithInit(int32 CurrentHole);

    UFUNCTION(BlueprintCallable, Category = "Player Management")
        void ResetAllPlayersToDes();

    UFUNCTION(BlueprintCallable, Category = "Player Management")
        void SortPlayersByDistance();


    void AdvanceTurn();

    UPROPERTY(VisibleAnywhere, Category = "Player Management")
        TArray<int32> PlayerOrder;

    // ===== CR2 센서 관련 함수들 =====

    // 센서 매니저 초기화
    void InitializeSensorManager();

    void InGameAddPlayer(UObject* WorldContextObject, FPlayerInfo PlayerInfo);
    void InGameRemovePlayer(UObject* WorldContextObject, FPlayerInfo PlayerInfo);


    void RemoveBallBySlotIndex(int32 TargetSlotIndex);

    void RemovePlayerBySlotIndex(int32 TargetSlotIndex);
    // 센서 준비 상태 확인
    void CheckSensorReadyState(int32 PlayerIndex);

    // 센서를 대기 상태로 전환
    void SetSensorClub(int32 nClub, bool bIsRoughTerrain = false);

    // 센서를 대기 상태로 전환
    void SetSensorToStandby();

    // 센서 이벤트 핸들러들
    UFUNCTION()
        void OnSensorShotDetected(const FCR2ShotData& ShotData);

    UFUNCTION()
        void OnSensorShotDetectedEx(const FCR2ShotDataEx& ShotDataEx);

    UFUNCTION()
        void OnSensorBallReady(const FCR2BallPosition& BallPosition);

    UFUNCTION()
        void OnSensorStatusChanged(int32 Status);

    // 센서 데이터 변환 함수들
    FVector CalculateShotDirection(const FCR2ShotData& ShotData);
    FVector CalculateAimDirection(const FCR2ShotData& ShotData);
    float CalculateShotPower(const FCR2ShotData& ShotData);


    // 현재 플레이어를 건너뛰고 다음 플레이어로 턴 넘기기
    UFUNCTION(BlueprintCallable, Category = "Player Management")
        void SkipCurrentPlayerTurn();
    // 특정 플레이어를 강제 홀 아웃시키고 다음으로 넘기기
    UFUNCTION(BlueprintCallable, Category = "Player Management")
        void ForcePlayerHoleOutAndAdvance(int32 PlayerIndex);

    // 남은 유효한 플레이어 수 반환
    UFUNCTION(BlueprintCallable, Category = "Player Management")
        int32 GetRemainingPlayerCount() const;

    // 현재 플레이어가 마지막 유효한 플레이어인지 확인
    UFUNCTION(BlueprintCallable, Category = "Player Management")
        bool IsLastValidPlayer(int32 CurrentPlayerIndex) const;

        UFUNCTION(BlueprintCallable, Category = "Golf")
        void StartSensorReadyCheck(int32 PlayerIndex);

    /**
     * 센서 준비 상태 체크 중지
     * 레벨 이동 시나 게임 종료 시 호출
     */
    UFUNCTION(BlueprintCallable, Category = "Golf")
        void StopSensorReadyCheck();

    /**
     * 레벨 언로드 시 정리 작업
     */
    UFUNCTION(BlueprintCallable, Category = "Golf")
        void OnLevelUnload();

protected:
    UPROPERTY(VisibleAnywhere, Category = "Player Management")
        TArray<AGolfPlayer*> Players;

    UPROPERTY(VisibleAnywhere, Category = "Player Management")
        AGolfPlayerController* PlayerController;

    UPROPERTY(VisibleAnywhere, Category = "Player Management")
        TArray<AGolfBall*> PlayerBalls;

    UPROPERTY(VisibleAnywhere, Category = "Player Management")
        ACameraManager* CameraManager;

    // ===== CR2 센서 관련 변수들 =====

    UPROPERTY()
        ACR2SensorManager* SensorManager;

    // 센서 준비 상태
    bool bSensorReady;

    // 현재 활성 플레이어 인덱스 (센서가 대기 중인 플레이어)
    int32 CurrentActivePlayerIndex;

    // 센서 상태 체크 타이머
    FTimerHandle SensorCheckTimer;


private:
    FTimerHandle DelayedReadyTimer;
    bool bShuttingDown = false;

    void ShutdownPlayerManager();

    // ⭐ GameMode 참조를 위한 함수
    AInGameMode* GetInGameMode() const;

    // 볼 상태 변경 핸들러
    UFUNCTION()
        void OnPlayerBallStateChanged(EBallState NewState, int32 OwningPlayerIndex);

    bool CheckAllPlayersHaveFirstShot() const;
    float CalculateCurrentBallDistanceToHole(AInGameMode* GameMode) const;
    int32 DetermineNextPlayer(int32 CurrentPlayerIndex, bool bAllHaveFirstShot,
        float HoleDistance, AInGameMode* GameMode);
    bool ShouldContinuePutting(int32 PlayerIndex, float HoleDistance,
        AInGameMode* GameMode) const;
    int32 FindNextPlayerAfterAllFirstShots();
    int32 FindNextPlayerInTeeShotOrder(int32 CurrentPlayerIndex);


    // 현재 플레이어를 제외한 다음 플레이어 찾기
    int32 FindNextPlayerExcludingCurrent(int32 CurrentPlayerIndex);


 
    UFUNCTION()
        FString GetSensorStatusName(int32 Status) const;

    /**
     * @brief 볼 영역 코드를 읽기 쉬운 문자열로 변환
     * @param Area 센서가 감지한 볼의 영역 (EBallArea)
     * @return 영역을 설명하는 문자열 (예: "PUTTING (퍼팅 그린)")
     */
    UFUNCTION()
        FString GetBallAreaName(EBallArea Area) const;

    /**
     * @brief 클럽 코드를 읽기 쉬운 문자열로 변환
     * @param ClubCode 클럽 코드 (CR2CLUB_*)
     * @return 클럽을 설명하는 문자열 (예: "PUTTER")
     */
    UFUNCTION()
        FString GetClubName(int32 ClubCode) const;

    /** 센서 준비 상태 체크 타이머 핸들 */
    FTimerHandle SensorCheckTimerHandle;

    /** 현재 센서 준비를 체크 중인 플레이어 인덱스 */
    int32 SensorCheckingPlayerIndex = -1;

    /** 센서 준비 상태 체크 시도 횟수 */
    int32 SensorCheckAttempts = 0;

    /** 최대 센서 준비 체크 시도 횟수 (약 15초, 0.5초 * 30회) */
    static constexpr int32 MAX_SENSOR_CHECK_ATTEMPTS = 30;
};


