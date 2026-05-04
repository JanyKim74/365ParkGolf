#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GolfDataStructures.h"
#include "GolfShotControlWidget.h"
#include "GolfMinimap.h"
#include "Engine/AssetManager.h"
#include "LandscapeChecker.h"  // ? �߰�
#include "TerrainHeightGrid.h"
#include "StrokeWidget.h"
#include "Widgets\InGameScoreBoardWidget.h" // Add this line
#include "SubChangeCourse.h"
#include "SerialPort/AutoTeeController.h"
#include "Widgets/RangeHUDWidget.h"
#include "Widgets/RangeHUDStatWidget.h"
#include "Widgets/TimerWidget.h"
#include "WebcamConfig.h"
#include "Widgets/ResultVideoWidget.h"
#include "Utils\TTSManager.h"  // tts
#include "InGameMode.generated.h"

class UInGamePlayerSelectWidget;
class UResultWidget;
class UGameEndWidget;
class AParticleManager;
class UGolfPlayerManager;
class UPlayerInfoSlotWidget; // ������ ����
class UStrokeMenuWidget;
class UInGameMenuPopup;
class ULoadingWidget;
class UBallParticleManager;
class AGolfPlayer;
class AGolfBall;
class ABoomLine;
class UDataTable;
class AReadyBillboard;
class UAutoTeeController;
class UResultVideoWidget;
class UMediaSoundComponent;
class UShotResultWidget;
class ABallDropMarkerActor;
class UInGameScoreBoardWidget;
class ATourActor;
class URangeHUDStatWidget;
class UInGameScoreBoardStatWidget;
class UBallDistanceWidget;
class APuttingGuide;
class UCameraModePopupWidget;
class UOffscreenIndicatorWidget;
class UTimerEndPopupWidget;


UENUM(BlueprintType)
enum class EGolfGameMode : uint8
{
    StrokeMode      UMETA(DisplayName = "Stroke Mode"),
    TrainingMode    UMETA(DisplayName = "Training Mode"),
    RangeMode       UMETA(DisplayName = "Range Mode")
};


USTRUCT(BlueprintType)
struct FStateTransition
{
    GENERATED_BODY()

        UPROPERTY(EditAnywhere, BlueprintReadWrite)
        EGameState FromState = EGameState::Game_Init;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        EGameState ToState = EGameState::Game_Init;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        float DelayTime = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        bool bRequiresCondition = false;

    FStateTransition()
    {
        FromState = EGameState::Game_Init;
        ToState = EGameState::Game_Init;
        DelayTime = 0.0f;
        bRequiresCondition = false;
    }
};


USTRUCT(BlueprintType)
struct FGameStateMachine
{
    GENERATED_BODY()

        UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
        EGameState CurrentState = EGameState::Game_Init;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
        EGameState PreviousState = EGameState::Game_Init;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
        EGameState PendingState = EGameState::Game_Init;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
        float StateTime = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
        float TransitionDelayTime = 0.0f;

    // ���� ���� �÷��׵�
    bool bStateChanged = false;
    bool bPendingTransition = false;
    bool bStateEntered = false;

    void Update(float DeltaTime)
    {
        StateTime += DeltaTime;


        if (bPendingTransition)
        {
            TransitionDelayTime -= DeltaTime;
            if (TransitionDelayTime <= 0.0f)
            {
                ExecutePendingTransition();
            }
        }

        if (bStateEntered)
        {
            bStateEntered = false;
        }
    }

    void ChangeState(EGameState NewState, float Delay = 0.0f)
    {
        if (CurrentState == NewState)
            return;

        if (Delay > 0.0f)
        {
            // ������ ���� ��ȯ ����
            PendingState = NewState;
            TransitionDelayTime = Delay;
            bPendingTransition = true;
        }
        else
        {
            // ��� ���� ��ȯ
            ExecuteStateChange(NewState);
        }
    }

    bool JustEnteredState() const
    {
        return bStateChanged;
    }

    bool IsTransitioning() const
    {
        return bPendingTransition;
    }

    float GetStateTime() const
    {
        return StateTime;
    }

private:
    void ExecutePendingTransition()
    {
        if (bPendingTransition)
        {
            ExecuteStateChange(PendingState);
            bPendingTransition = false;
            TransitionDelayTime = 0.0f;
        }
    }

    void ExecuteStateChange(EGameState NewState)
    {
        PreviousState = CurrentState;
        CurrentState = NewState;
        StateTime = 0.0f;
        bStateChanged = true;
        bStateEntered = true;
    }
};

/**
 * ������ InGameMode - ��Ȯ�� ���¸ӽ� ����
 */
UCLASS()
class PARKDAY_API AInGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AInGameMode();

    virtual void BeginPlay() override;
    //virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaTime) override;
    virtual void BeginDestroy() override;
    void EndGame();
    void StopLoading();
    void StartLoading();

    UPROPERTY()
    bool bIsContinueGame = false;
    UFUNCTION()
        void InitRangeMode();

    FGameStateMachine& GetStateMachine();

    UFUNCTION()
        virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

    UPROPERTY()
        AReadyBillboard* ReadyBillboard;

    // ? �߰�: HoleMark �����
    UPROPERTY()
        AActor* HoleMarkBillboard;

    UPROPERTY()
        TSubclassOf<AActor> HoleMarkBillboardClass;

    UPROPERTY()
        ABoomLine* BoomLine;

    UPROPERTY()
        AParticleManager* ParticleManager;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
        int32 MaxHoleCount = 18;

    UPROPERTY()
    UMediaSoundComponent* MSC;

    UPROPERTY()
    ABallDropMarkerActor* DropMarker;

    UFUNCTION()
        void InitTourActor();
    UPROPERTY()
    ATourActor* TourActor;

    // ? ���� ��� ���� �Լ��� �߰�
    UFUNCTION(BlueprintCallable, Category = "Game Mode")
        EGolfGameMode GetCurrentGameMode() const { return CurrentGameMode; }

    UFUNCTION(BlueprintCallable, Category = "Game Mode")
        void SetGameMode(EGolfGameMode NewGameMode);

    UFUNCTION(BlueprintCallable, Category = "Game Mode")
        bool IsStrokeMode() const { return CurrentGameMode == EGolfGameMode::StrokeMode; }

    UFUNCTION(BlueprintCallable, Category = "Game Mode")
        bool IsTrainingMode() const { return CurrentGameMode == EGolfGameMode::TrainingMode; }

    UFUNCTION(BlueprintCallable, Category = "Game Mode")
        bool IsRangeMode() const { return CurrentGameMode == EGolfGameMode::RangeMode; }

    UFUNCTION(BlueprintCallable, Category = "Range Mode")
        void SetRangeMode() { CurrentGameMode = EGolfGameMode::RangeMode; }

    // ? Range Mode ���� ���� ��ȯ �Լ�
    UFUNCTION(BlueprintCallable, Category = "Range Mode")
        void TransitionToRangeLevel();

    // ? ���� ��� ���� �Լ�
    UFUNCTION(BlueprintCallable, Category = "Game Mode")
        void DetermineGameMode();

    // ? ���� ���� ��� ����
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Mode")
        EGolfGameMode CurrentGameMode = EGolfGameMode::StrokeMode;

    // ? Range Mode ���� �̸� ����
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Range Mode")
        FName RangeLevelName = TEXT("RangeLevel");

    UPROPERTY()
        FVector AimLocation;
    UPROPERTY()
        bool bClickedMinimap = false;

    UPROPERTY()
        float DefaultGravity = -980.f;

    UPROPERTY()
        TMap<int32, UTexture2D*> MulliganTextureMap;

    UFUNCTION()
        void InitCourseMapImage();
    UFUNCTION()
        void OnAnyBallSweepHit(AActor* TrackedActor, const FHitResult& Hit);
    UPROPERTY()
        UDataTable* ResultParticleDatatable = nullptr;

    UPROPERTY()
        TArray<FRotator> TeeRotationArray;
    UPROPERTY()
        AActor* TeeAnimInstance;

    UFUNCTION()
        AActor* InitTeeAnim();

    UFUNCTION()
        void ResultParticleBuildIndex();
    UPROPERTY(BlueprintReadWrite, Category = "FX")
        TMap<int32, TSubclassOf<AActor>> HoleInParticleMap;
    UPROPERTY(BlueprintReadWrite, Category = "FX")
        TMap<int32, TSubclassOf<AActor>> ChanceParticleMap;
    UPROPERTY(BlueprintReadWrite, Category = "FX")
        TMap<int32, UTexture2D*> ChanceTextureMap;
    // ���� ���� - ��Ȯ�� ��ȯ ���ǰ� �Բ�
    UFUNCTION(BlueprintCallable, Category = "Game State")
        void ChangeGameState(EGameState NewState, float Delay = 0.0f);

    UFUNCTION(BlueprintCallable, Category = "Game State")
        bool CanTransitionTo(EGameState NewState) const;

    UFUNCTION(BlueprintPure, Category = "Game State")
        EGameState GetCurrentGameState() const { return StateMachine.CurrentState; }

    UFUNCTION(BlueprintPure, Category = "Game State")
        bool IsTransitioning() const { return StateMachine.IsTransitioning(); }

    UFUNCTION()
        AGolfPlayer* FindPlayer(int32 PlayerIndex);

	UFUNCTION()
		AGolfPlayer* FindPlayerSlotIndex(int32 SlotIndex);

    UFUNCTION()
        AGolfBall* FindBall(int32 PlayerIndex);

    // ���� ���� ����
    UFUNCTION(BlueprintCallable, Category = "Game Control")
        void ChangeSensorState(ESensorState NewState);


    // ? �߰�: FPlayerInfo ���� ���� �Լ�
    UFUNCTION(BlueprintCallable, Category = "Game Info Update")
        void UpdateGameInfoPlayers(const TArray<FPlayerInfo>& NewPlayers);

    // ? �߰�: FMapInfo ���� ���� �Լ�
    UFUNCTION(BlueprintCallable, Category = "Game Info Update")
        void UpdateGameInfoMapInfo(const FMapInfo& NewMapInfo);

    // ? �߰�: FGameOptionInfo ���� ���� �Լ�
    UFUNCTION(BlueprintCallable, Category = "Game Info Update")
        void UpdateGameInfoGameOptions(const FGameOptionInfo& NewGameOptions);


    // ���� ����
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Info")
        FMapInfo MapInfo;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Info")
        int32 CurrentHole = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Info")
        int32 CurrentPlayerIndex = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Info")
        UGolfPlayerManager* PlayerManager;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Info")
        FGameInfo GameInfo;

    /* ������ ��� */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Practice")
    EPracticeMode CurrentPracticeMode = EPracticeMode::Driving;

    UPROPERTY() AActor* BP_Target;

    UPROPERTY() AActor* PracticeModeStartPoint;
    UPROPERTY() AActor* PracticeModeEndPoint;
    UPROPERTY() AActor* PracticePuttingModeStartPoint;
    UPROPERTY() AActor* PracticePuttingModeEndPoint;

    void MoveBallOnPracticeMode();

    void SyncPlayerInfosToGameInfo();

    bool CheckTeeShotCountIsZero();

    UFUNCTION()
    void PlaceActorInFrontOnPlane(AActor* SourceActor, AActor* DescActor, float Distance);

    UFUNCTION()
        void HandleOnChangedApproachCheckBoxState();
    UFUNCTION()
		void HandleOnChangedPuttingCheckBoxState();
    UFUNCTION()
		void HandleOnChangedDrivingCheckBoxState();

    // �̴ϸ� ���� �߰�
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
        TSubclassOf<UGolfMiniMap> MiniMapWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Result")
    TSubclassOf<UResultVideoWidget> ResultVideoWidgetClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Result")
    UResultVideoWidget* ResultVideoWidgetInstance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RangeHUD")
        TSubclassOf<URangeHUDWidget> RangeHUDWidgetclass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RangeHUD")
        URangeHUDWidget* RangeHUDWidgetInstance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RangeHUD")
        TSubclassOf<URangeHUDStatWidget> RangeHUDStatWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RangeHUD")
		URangeHUDStatWidget* RangeHUDStatWidgetInstance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timer")
        TSubclassOf<UTimerWidget> TimerWidgetClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timer")
		UTimerWidget* TimerWidgetInstance;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timer")
		UTimerEndPopupWidget* TimerEndWidget;

    
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Indicator")
        FSoftClassPath HolecupIndicatorClassPath = TEXT("/Game/UMG/UI/InGame/WBP_HolecupIndicator.WBP_HolecupIndicator_C");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Indicator")
		UOffscreenIndicatorWidget* HolecupIndicatorWidget;


    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
        UGolfMiniMap* MiniMapWidget;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
        TSubclassOf<UInGamePlayerSelectWidget> InGamePlayerSelectWidgetClass;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
        UInGamePlayerSelectWidget* InGamePlayerSelectWidget;


    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
        TSubclassOf<UBallDistanceWidget> BallDistanceWidgetClass;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
        UBallDistanceWidget* BallDistanceWidget;

    // �̴ϸ� ������Ʈ �Լ�
    UFUNCTION(BlueprintCallable, Category = "MiniMap")
        void UpdateMiniMapForCurrentHole();

    UFUNCTION(BlueprintCallable, Category = "MiniMap")
        void ShowMiniMap(bool bShow);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
        TSubclassOf<UGolfShotControlWidget> DefaultShotControlWidget;

    //������ �ֵ��� ����
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
        TSoftClassPtr<UStrokeWidget> StrokeWidgetClass;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
        UStrokeWidget* StrokeWidgetInstance;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
        UCameraModePopupWidget* CameraModePopupWidget;

    // New: Scoreboard Widget Class and Instance
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
        TSubclassOf<UInGameScoreBoardWidget> InGameScoreBoardWidgetClass; // Add this line

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
        UInGameScoreBoardWidget* InGameScoreBoardWidgetInstance; // Add this line

    // New: Scoreboard Widget Class and Instance
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
        TSubclassOf<UInGameScoreBoardStatWidget> InGameScoreBoardStatWidgetClass; // Add this line

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
        UInGameScoreBoardStatWidget* InGameScoreBoardStatWidgetInstance; // Add this line


        // ? StrokeMenuWidget Ŭ������ ���� ���� (Blueprint���� ����)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
        TSubclassOf<UStrokeMenuWidget> StrokeMenuWidgetClass;

    // ? ������ StrokeMenuWidget �ν��Ͻ�
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
        UStrokeMenuWidget* StrokeMenuWidgetInstance;


    /*   UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
           TSubclassOf<UUserWidget> LoadingScreenWidgetClass;*/

           //UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
           //    ULoadingWidget* LoadingScreenWidgetInstance;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "UI")
        TSubclassOf<UUserWidget> InGamePopupWidgetClass;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "UI")
        UInGameMenuPopup* InGamePopupWidgetInstance;


    void SetShowScoreBoard(int32 iShow);
    void SetShowScoreStatBoard(bool bIsVisible);

    // ? �߰�: LandscapeChecker ���� �Լ���
    UFUNCTION(BlueprintCallable, Category = "Landscape Setup")
        void SetupLandscapeChecker();

    UFUNCTION(BlueprintCallable, Category = "Landscape Setup")
        void SetupPhysicalMaterialMappings(ALandscapeChecker* Checker);

    // ? �߰�: LandscapeChecker ����
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Landscape")
        ALandscapeChecker* LandscapeChecker;


    // �÷��̾� ���� �������� �����ϰ� �ʱ�ȭ�ϴ� �Լ�
    UFUNCTION(BlueprintCallable, Category = "UI Management")
        void SetupPlayerInfoSlots();

    // ���� �� �÷��̾��� ������ ���̶���Ʈ�ϴ� �Լ�
    UFUNCTION(BlueprintCallable, Category = "UI Management")
        void HighlightCurrentPlayerSlot(int32 PlayerIndex);

    // ��� �÷��̾� ������ ������ ������Ʈ�ϴ� �Լ�
    UFUNCTION(BlueprintCallable, Category = "UI Management")
        void UpdateAllPlayerInfoSlots();

    // GolfBall���� �̺�Ʈ�� ó���ϱ� ���� ���ο� �Լ�
    UFUNCTION()
        void HandleBallGameFlowEvent(EBallEvent EventType); // ���ο� �̺�Ʈ �ڵ鷯 �Լ� ����

    UPlayerInfoSlotWidget* GetCurrentSlot();
    UPlayerInfoSlotWidget* FindPlayerInfoSlot(int32 SlotIndex, int32 PlayerIndex);

    // ? ���ο� �Լ� ����: StartTurnTransitionCountdown
    UFUNCTION(BlueprintCallable, Category = "Game Flow")
        void StartTurnTransitionCountdown(float DelayTime = 3.0f); // �⺻ ����� ����

    void HandleOBDropLogic();

    void HandlePanelltyDropLogic();

    // ASubChangeCourse Ŭ������ ������ UPROPERTY �߰�
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Control")
        TSubclassOf<ASubChangeCourse> SubChangeCourseClass;

    // ASubChangeCourse �ν��Ͻ��� ������ UPROPERTY �߰�
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Level Control")
        ASubChangeCourse* SubChangeCourseInstance;

    //void LoadStrokeWidget();

    void ShowStrokeMenu();

    void HideStrokeMenu();

    void ShowInGameMenuPopup();


    // ? Training Mode ���� �Լ��� �߰�
    UFUNCTION(BlueprintCallable, Category = "Training Mode")
        void PrepareNextTrainingShot();

    UFUNCTION(BlueprintCallable, Category = "Training Mode")
        void HandleTrainingHoleIn();

    UFUNCTION(BlueprintCallable, Category = "Training Mode")
        void HandleTrainingOB();

    UFUNCTION(BlueprintCallable, Category = "Training Mode")
        void ResetTrainingBallToTee();

    UFUNCTION(BlueprintCallable, Category = "Training Mode")
        void ResetStartBallPosReturn();


    UFUNCTION(BlueprintCallable, Category = "Training Mode")
        bool IsTrainingModeClickAllowed(const FVector& WorldPosition) const;

    UFUNCTION(BlueprintCallable, Category = "Training Mode")
        void MoveBallToTrainingPosition(const FVector& WorldPosition);

    // ? ���� ��庰 ���� ��ȯ ���� üũ �Լ���
    UFUNCTION(BlueprintPure, Category = "Game State")
        bool ShouldTransitionToHoleOut_TrainingMode() const;

    UFUNCTION(BlueprintPure, Category = "Game State")
        bool ShouldTransitionToHoleOut_StrokeMode() const;

    bool LoadGameInfoFromJSON();
    void SaveGameInfoToJSON();

    TArray<TSubclassOf<AActor>> TeeAnimArray;

    void ApplyCameraModeOptionToCameraManager();

    void SetCameraModeOption(int32 NewCameraModeOption);

    int32 GetCameraModeOption() const;

    void UpdateMiniMapForCurrentPlayer();

    // 2026.01.03 �߰�
// AB, CD, AC, BD ���� ����
// ����BA ���� ��쵵 ������ �� �ֵ��� ����
    int32 GetPhysicalHoleNum(int32 RoundHoleIdx, int32 Sublevel);
    void UpdateHoleFlagDisplay();

public:
    // ���� �ӽ�
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State Machine")
        FGameStateMachine StateMachine;
protected:
    // ���� ��ȯ ��Ģ
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State Machine")
        TArray<FStateTransition> StateTransitions;


    UFUNCTION(BlueprintCallable, Category = "MiniMap OB")
        void ToggleMiniMapOBLines();

    UFUNCTION(BlueprintCallable, Category = "MiniMap OB")
        void SetMiniMapOBLineColor(FLinearColor NewColor);

    UFUNCTION(BlueprintCallable, Category = "MiniMap OB")
        void RefreshMiniMapOBLines();

    // ? ���� ��庰 �б� ó�� �Լ���
    void ProcessStateMachineByGameMode(float DeltaTime);
    void HandleStateEnterByGameMode(EGameState NewState);
    void HandleStateExitByGameMode(EGameState OldState);


    // ? ���� ��庰 ���� ���� ó�� �Լ���
    void OnEnterGameInit_StrokeMode();
    void OnEnterGameInit_TrainingMode();
    void OnEnterGameInit_RangeMode();

    void OnEnterHoleInit_StrokeMode();
    void OnEnterHoleInit_TrainingMode();

    void OnEnterGamePlay_StrokeMode();
    void OnEnterGamePlay_TrainingMode();

    // ���� ó�� �Լ���
    void ProcessStateMachine(float DeltaTime);
    void HandleStateEnter(EGameState NewState);
    void HandleStateExit(EGameState OldState);
    void UpdateCurrentState(float DeltaTime);

    // ���º� ������Ʈ (�������� ó���� �ʿ��� ���µ�)
    void UpdateGamePlay(float DeltaTime);
    void UpdateHoleStart(float DeltaTime);

    // ���º� ����/���� ó��
    void OnEnterGameInit();
    void OnEnterHoleInit();
    void OnEnterHoleReady();
    void OnEnterHoleStart();
    void OnEnterGamePlay();
    void OnEnterHoleOut();
    void OnEnterHoleResults();
    void OnEnterGameResults();
    void OnEnterGameEnd();

    void OnExitGamePlay();
    void OnExitGameResults();
    void OnExitHoleOut();

    // ���� ����
    void InitializeGame();
    void InitializeGameByMode(); // ? ��庰 �ʱ�ȭ
    void InitializeHole();
    void StartHole();
    void EndHole();
    void HoleResults();

    // ���� ��ȯ ���� �˻�
    bool ShouldTransitionToHoleOut() const;
    bool ShouldTransitionToGameEnd() const;
    bool IsHoleInitializationComplete() const;

    // ��ƿ��Ƽ
    bool RequiresContinuousUpdate(EGameState State) const;
    void InitializeStateTransitions();

    void InitInGameMenu();
    void InitPlayersInfo();

    void LoadMapInfoFromLevel();
    void InitializeOBLines();



    AActor* FindActorByName(const FString& ActorName);

    // ? �߰�: TerrainHeightGrid ����
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain Grid")
        ATerrainHeightGrid* TerrainHeightGrid;

    UFUNCTION(BlueprintCallable, Category = "Terrain Grid Setup")
        void SetupTerrainHeightGrid();

public:
    UFUNCTION()
        void ResetGameData();

    UFUNCTION()
        void ReBindBallEvents();
    // PlayerInfoSlot ���� Ŭ���� (UMG �������Ʈ ��θ� �����Ϳ��� ����)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
        TSubclassOf<UPlayerInfoSlotWidget> PlayerInfoSlotWidgetClass;

    // ������ �� �÷��̾� ���� ���� �ν��Ͻ����� ������ �迭
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
        TArray<UPlayerInfoSlotWidget*> PlayerInfoSlotWidgets;

    // ? �߰�: ī�޶� ȸ�� �� UI ������Ʈ�� ���� �Լ�
    //void UpdateAimInfoPanelPosition();

    // BeginPlay �Ǵ� InitializeGame���� �� �Լ��� ���ε��մϴ�.
    void BindBallEvents(); // �� �̺�Ʈ�� ���ε��ϴ� �Լ� ����

        // ? ���ο� Ÿ�̸� �ڵ� �� �ð� ���� ����
    FTimerHandle TurnCountdownTimer;
    float CurrentTurnCountdownTime;
    float MaxTurnCountdownTime;
    FTimerHandle DelayedReadyTimer; // ? �� ���� �߰��մϴ�.

    // ? ���ο� ī��Ʈ�ٿ� ������Ʈ �Լ� ����
    UFUNCTION()
        void UpdateTurnCountdown();
   
    UFUNCTION()
		void UpdateBallNamePlateAndMarker();

    //UFUNCTION(BlueprintCallable, Category = "Level Transition")
    //    void ShowLoadingScreen();

 /*   UFUNCTION(BlueprintCallable, Category = "Level Transition")
        void HideLoadingScreen();*/

    UFUNCTION(BlueprintCallable, Category = "Level Transition")
        void TransitionToLevel(FName LevelName);

    // ? ���� �߰�: Ư�� �÷��̾��� ���¸� �����ϴ� �Լ�
    UFUNCTION(BlueprintCallable, Category = "Player State Management")
        void SetPlayerStateForPlayer(int32 PlayerIndex, EPlayerState NewState);


    // ? Training Mode ���� �̺�Ʈ ó��
    void HandleBallGameFlowEvent_TrainingMode(EBallEvent EventType);

    // ? ���� ��庰 ������Ʈ �Լ���
    void UpdateGamePlay_StrokeMode(float DeltaTime);
    void UpdateGamePlay_TrainingMode(float DeltaTime);

    // ? Training Mode ��������
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training Mode Settings")
        bool bTrainingModeInfiniteShots = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training Mode Settings")
        bool bTrainingModeAllowBallMovement = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training Mode Settings")
        float TrainingModeMinDistanceFromHole = 100.0f; // Ȧ�ſ��� �ּ� �Ÿ� (cm)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training Mode Settings")
        bool bTrainingModeAutoReset = true; // OB�� Ȧ�� �� �ڵ� ����

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training Mode Settings")
        float TrainingModeResetDelay = 2.0f; // ���� ���� �ð�

    // ? Training Mode ���� ����
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training Mode Status")
        int32 TrainingModeShots = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training Mode Status")
        int32 TrainingModeHoleIns = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training Mode Status")
        FVector LastTrainingBallPosition;


private:
    // �ʱ�ȭ �÷��׵�
    bool bGameInitialized = false;
    bool bHoleInitialized = false;
    bool bMapInfoLoaded = false;

    // ����� ����
    UPROPERTY(VisibleAnywhere, Category = "Debug", meta = (CallInEditor = "true"))
        FString CurrentStateDebugInfo;

private:
    // �̴ϸ� ���� �Լ���
    void CreateMiniMapWidget();
    void DestroyMiniMapWidget();
    void RefreshMiniMapData();

    void DebugWidgetClass();
    void TryLoadWidgetClassDirectly(AGolfPlayerController* GolfPC);
    void SearchForWidgetBlueprint(AGolfPlayerController* GolfPC);

    // ĳ�� ��ȸ �Լ���
    UClass* GetWidgetClassByCacheBypass();
    UClass* SearchAllBlueprints();

    void LoadMiniMapWidgetClassFallback();
    UClass* LoadMiniMapWidgetViaLoadClass();
    UClass* LoadMiniMapWidgetViaBlueprint();
    UClass* FindMiniMapWidgetViaAssetRegistry();

    void LoadMiniMapWidgetAtRuntimeSafe();

    UFUNCTION()
        void OnTeeHeightChanged(int32 Height);
    UFUNCTION()
        void OnKeyPressed(EAutoTeeKey Key);

    // ? Training Mode ���� Ÿ�̸�
    FTimerHandle TrainingResetTimer;
    FTimerHandle TrainingFeedbackTimer;


public:
    bool bIsGameMenuEnd = false;
    
    UFUNCTION()
        void SoftResetGameInfo();

    void SpawnHoleInParticle();

    AGolfBall* GetCurrentTurnGolfBall();
    AGolfPlayer* GetCurrentTurnGolfPlayer();

    void InitBlueprintObjects();
    void InitConcedeLines();

    UPROPERTY()
        UDataTable* BlueprintDT;

    UPROPERTY()
    UDataTable* DT_ScoreIcon;

    UPROPERTY()
        TMap<FString, TSubclassOf<UObject>> BlueprintObjectsMap;

    UPROPERTY()
        UBallParticleManager* BallParticleManager;

    UPROPERTY()
        TSubclassOf<UResultWidget> ResultWidgetClass;
    UPROPERTY()
        UResultWidget* ResultWidgetInstance;
    UPROPERTY()
        UDataTable* ResultWidgetDT;

	UPROPERTY()
		TSubclassOf<UShotResultWidget> ShotResultWidgetClass;
    UPROPERTY()
        UShotResultWidget* ShotResultWidgetInstance;
    //UPROPERTY()
    //    UDataTable* ShotResultWidgetDT;

    UPROPERTY()
        TSubclassOf<UGameEndWidget> GameEndWidgetClass;

    UPROPERTY()
        UGameEndWidget* GameEndWidgetInstance;

    UPROPERTY()
        bool bClickedEndGameButton = false;

    UFUNCTION()
        bool CheckFirstShot();

    UFUNCTION()
        FVector GetCurrentTeeLocation();

    // ? OB ���� üũ �Լ���
    UFUNCTION(BlueprintCallable, Category = "OB Check")
        bool IsPointInOBArea(const FVector& WorldPoint) const;

    void UpdateMiniMapAimLine();

    UPROPERTY()
        bool bSetNextHole = false;


    void OnPlayerIndexChanged(int32 NewPlayerIndex, int32 OldPlayerIndex);

    void DestroyAllPlayersAndBalls();
    void SyncPlayersFromJsonForNextHole();


    UPROPERTY()
        int32 LatestShotSlotIndex = 0; // SlotIndex of latest shot player

    UFUNCTION()
    AGolfBall* FindBallSlotIndex(int32 SlotIndex);
    UFUNCTION()
        void UseMulligan();

    // SystemConfig �ε� �Լ�
    UFUNCTION(BlueprintCallable, Category = "System Config")
        bool LoadSystemConfig();

    // SystemConfig ���� �Լ�
    UFUNCTION(BlueprintCallable, Category = "System Config")
        void SaveSystemConfig();

    // SystemConfig ������
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "System Config")
        FSystemConfig SystemConfig;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Webcam Config")
        FWebcamSettings WebcamSettings;  // ? ���� �߰�: ��ķ ���� ����ü

    UFUNCTION(BlueprintCallable, Category = "Webcam Config")
        void LoadWebcamConfig();  // ? ���� �߰�: ���� �ε� �Լ�


    UFUNCTION()
        void OnAutoTeeConnectionChanged();

    void ShowSwingMotion(bool bShow);

private:
    // OB ��� ���� �Լ���
    int32 CalculateWindingNumber(const FVector2D& TestPoint, const TArray<FVector>& Polygon) const;
    float CalculateCrossProduct2D(const FVector2D& P1, const FVector2D& P2, const FVector2D& TestPoint) const;

    void SetupMaskTexture(ALandscapeChecker* Checker);
    void SetupCupActorMesh(AActor* CupActor, int32 HoleNumber);
    // Cup �޽� ���� ��� (�����Ϳ��� ���� ����)
    UPROPERTY(EditAnywhere, Category = "Cup Settings")
        FString CupMeshAssetPath = TEXT("StaticMesh'/Game/model_data/hole_cup/cup_in'");

    // ����׿� �Լ�
    void DebugCupComponents(AActor* CupActor);

    FString RangeHUDWidgetPath;
    TSubclassOf<UUserWidget> RangeHUDWidgetBPclass;


    void LoadStrokeWidgetClassSafe();
    void LoadResultVideoWidgetClassSafe();


public:
    // ? AutoTee ���� �Լ�
    UPROPERTY()
        class UAutoTeeController* AutoTeeController;

    // AutoTee ���� �Լ�
    UFUNCTION(BlueprintCallable, Category = "AutoTee")
        void ConnectAutoTeeDevice();

    // AutoTee ���� ���� �Լ�
    UFUNCTION(BlueprintCallable, Category = "AutoTee")
        void DisconnectAutoTeeDevice();

    // Ƽ ���� ���� �Լ�
    UFUNCTION(BlueprintCallable, Category = "AutoTee")
        void SetAutoTeeHeight();

    // Ƽ ���� ���� �̺�Ʈ �ڵ鷯
    UFUNCTION()
        void OnAutoTeeHeightChanged(int32 Height);

    // Ű�е� �Է� �̺�Ʈ �ڵ鷯
    UFUNCTION()
        void OnAutoTeeKeyPressed(EAutoTeeKey Key);

    UFUNCTION()  // ? �� �� �߰�!
        void OnAutoTeeKeyReleased(EAutoTeeKey Key);

    UFUNCTION(BlueprintCallable, Category = "Hole Mark")
        void UpdateHoleMarkPosition();

    UFUNCTION(BlueprintCallable, Category = "Hole Mark")
        void ShowHoleMark(bool bShow);

    // ? ���� �簳/�ʱ�ȭ �Լ�
    UFUNCTION(BlueprintCallable, Category = "Game Flow")
        void ResetRoundStatus();

    UFUNCTION(BlueprintCallable, Category = "Game Flow")
        bool CanResumeRound() const;

    FPlayerInfo FindPlayerInfo(int32 SlotIndex);

    FPlayerInfo* FindPlayerInfoPtr(int32 SlotIndex);

    FPlayerInfo* FindOrAddPlayerInfo(const FPlayerInfo& PlayerInfo);

    void DeduplicatePlayerInfos();

    UFUNCTION()
    void HandleOnModifyPlayers(FPlayerInfo PlayerInfo);

    UFUNCTION()
    void HandleOnDeletePlayers(FPlayerInfo PlayerInfo);

    void LoadMulliganTextures();

protected:
    // AutoTee ���� ����
    bool bAutoTeeConnected;

    // ���� Ƽ ���� (mm)
    int32 CurrentAutoTeeHeight;

    // ? ī�޶� ȸ�� ��ٿ�
    float LastRotationTime;
    float RotationCooldown;

    // ? ȸ�� ���� ���� üũ
    bool CanRotateCamera();

    void LoadWidgetClasses();

public:
    // ����������������������������������������������������������������������������������������������������������������������������������������������������������
    // [TTS ���� �ȳ� �Լ�]
    // ����������������������������������������������������������������������������������������������������������������������������������������������������������

    /**
     * @brief ���� ���� �ȳ�
     */
    UFUNCTION(BlueprintCallable, Category = "Game|TTS")
        void AnnounceGameStart();

    /**
     * @brief Ư�� Ȧ �ȳ�
     *
     * @param HoleNumber Ȧ ��ȣ (1-18)
     * @param Par �� ���ھ�
     */
    UFUNCTION(BlueprintCallable, Category = "Game|TTS")
        void AnnounceHole(int32 HoleNumber, int32 Par);

    /**
     * @brief Ÿ�� �ȳ�
     *
     * @param StrokeCount ���� Ÿ�� Ƚ��
     */
    UFUNCTION(BlueprintCallable, Category = "Game|TTS")
        void AnnounceStroke(int32 StrokeCount);

    /**
     * @brief ���� ���ھ� �ȳ�
     *
     * @param Score ���� ���ھ�
     */
    UFUNCTION(BlueprintCallable, Category = "Game|TTS")
        void AnnounceScore(int32 Score);

    /**
     * @brief Ȧ �Ϸ� �ȳ�
     *
     * @param HoleNumber �Ϸ��� Ȧ ��ȣ
     * @param Result "����", "��", "����" ��
     */
    UFUNCTION(BlueprintCallable, Category = "Game|TTS")
        void AnnounceHoleComplete(int32 HoleNumber, const FString& Result);

    /**
     * @brief ���� ���� �ȳ�
     *
     * @param FinalScore ���� ���ھ�
     */
    UFUNCTION(BlueprintCallable, Category = "Game|TTS")
        void AnnounceGameEnd(int32 FinalScore);

    /**
     * @brief Ŀ���� ���� ���
     *
     * @param Text ����� �ؽ�Ʈ
     */
    UFUNCTION(BlueprintCallable, Category = "Game|TTS")
        void Speak(const FString& Text);

    /**
     * @brief ���� ��� ����
     */
    UFUNCTION(BlueprintCallable, Category = "Game|TTS")
        void StopSpeaking();

    /**
     * @brief TTS �ý��� �ʱ�ȭ ���� Ȯ��
     *
     * @return true �غ��, false ���غ�
     */
    UFUNCTION(BlueprintCallable, Category = "Game|TTS")
        bool IsTTSReady() const;

    /**
     * @brief ���� ��� ������ Ȯ��
     *
     * @return true ��� ��, false ��� �� ��
     */
    UFUNCTION(BlueprintCallable, Category = "Game|TTS")
        bool IsSpeaking() const;

    /**
     * @brief ���� ���� ���� (0-100)
     *
     * @param Volume ���� ��
     */
    UFUNCTION(BlueprintCallable, Category = "Game|TTS")
        void SetTTSVolume(int32 Volume);

    /**
     * @brief ���� �ӵ� ���� (-10~10)
     *
     * @param Rate �ӵ� ��
     */
    UFUNCTION(BlueprintCallable, Category = "Game|TTS")
        void SetTTSRate(int32 Rate);

    /**
     * @brief ������ TTS ���� �޽��� ��ȸ
     *
     * @return ���� �޽���
     */
    UFUNCTION(BlueprintCallable, Category = "Game|TTS")
        FString GetTTSError() const;

    UPROPERTY()
        class APuttingGuide* PuttingGuideActor = nullptr;

    void SpawnPuttingGuide();

    UFUNCTION(BlueprintCallable, Category = "Game|TTS")
    void StoppingSensor();

protected:
    // ����������������������������������������������������������������������������������������������������������������������������������������������������������
    // [TTS �ý���]
    // ����������������������������������������������������������������������������������������������������������������������������������������������������������

    /// TTS �Ŵ���
    FTTSManager TTSManager;

    void CollectOBLinesFromCurrentLevel(); // ⭐ 서브레벨 로드 완료 후 OBPoints 수집
    FTimerHandle OBLevelWaitTimer;  // 폴링 타이머
    bool bOBLevelWaiting = false;   // 중복 타이머 방지 플래그

private:
    /**
     * @brief �ʱ� TTS ����
     */
    void SetupTTS();

    /**
     * @brief ������ ���� ��� (���� üũ ����)
     *
     * @param Text ����� �ؽ�Ʈ
     * @return true ����, false ����
     */
    bool SafeSpeak(const FString& Text);

    void OnStreamingLevelLoaded();
};




