#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "GameFramework/GameModeBase.h"
#include "Structs/CorseStruct.h"
#include "GolfDataStructures.h"
#include "ParkDay/Utils/TTSManager.h"
#include "MenuGameMode.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEnterPlayerSelect);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEnterPlayerSelectPost);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEnterCourseSelect);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEnterCourseSelectPost);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEnterIntro);

class UUserWidget;
class UUIStateWidgetMapDataAsset;
class UKeyboardWidget;
class UMenuUIImageDataAsset;
class UContinuePopupWidget;
class UPasswordWidget;

UENUM(BlueprintType)
enum class EUIState : uint8
{
    Intro       UMETA(DisplayName="Intro"),
    ModeSelect  UMETA(DisplayName="ModeSelect"),
    PlayerSelect UMETA(DisplayName="PlayerSelect"),
    CourseSelect UMETA(DisplayName="CourseSelect"),
    Loading     UMETA(DisplayName="Loading"),
    InGame      UMETA(DisplayName="InGame"),
    Practice    UMETA(DisplayName="Practice"),
};

// ⭐ 게임 모드 타입 열거형 추가
UENUM(BlueprintType)
enum class EGameType : uint8
{
    StrokeMode      UMETA(DisplayName = "Stroke Mode"),
    TrainingMode    UMETA(DisplayName = "Training Mode"),
    RangeMode       UMETA(DisplayName = "Range Mode")
};

UCLASS()
class PARKDAY_API AMenuGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AMenuGameMode();

    virtual void BeginPlay() override;
	UFUNCTION()
		virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

public:
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="UI")
    TSubclassOf<UKeyboardWidget> KeyBoardWidgetClass;

    UPROPERTY()
    UKeyboardWidget* KeyBoardWidgetInstance;

public:
    UPROPERTY()
    FOnEnterIntro OnEnterIntroDele;
    UPROPERTY()
		FOnEnterPlayerSelect OnEnterPlayerSelectDele;
    UPROPERTY()
        FOnEnterPlayerSelectPost OnEnterPlayerSelectPostDele;
	UPROPERTY()
		FOnEnterCourseSelect OnEnterCourseSelectDele;	
    UPROPERTY()
		FOnEnterCourseSelectPost OnEnterCourseSelectPostDele;

public:
    UFUNCTION()
    UUserWidget* GetStateWidget(EUIState State) const;

    UFUNCTION(BlueprintCallable, Category="UI StateMachine")
    void ChangeUIState(EUIState NewState);

    UFUNCTION(BlueprintCallable, Category="UI StateMachine")
    EUIState GetCurrentUIState() const { return CurrentUIState; }

public:
    // DataAsset로 상태 -> 위젯(WBP) 매핑 설정
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI StateMachine")
    UUIStateWidgetMapDataAsset* UIStateWidgetConfig = nullptr;

    // 상태별 생성된 위젯 인스턴스 캐시
    UPROPERTY(Transient, BlueprintReadOnly, Category="UI StateMachine")
    TMap<EUIState, UUserWidget*> StateWidgets;

    // 시작 상태
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI StateMachine")
    EUIState InitialState = EUIState::Intro;

    UPROPERTY()
    UContinuePopupWidget* ContinuePopupWidget;
    UPROPERTY()
    TSubclassOf<UContinuePopupWidget> ContinuePopupWidgetClass;

    void LoadContinuePopupWidget();
    void LoadPasswordWidget();

    UPROPERTY()
    UPasswordWidget* PasswordWidget;

private:
    // DataAsset Entries 기반으로 모든 위젯을 생성/등록
    void RegisterAllStateWidgetsFromConfig();

    // 상태 위젯 표시/숨김
    void SetStateWidgetVisible(EUIState State, bool bVisible);

    // 상태 전환 훅 (생성 금지: 로직만)
    void HandleExitUIState(EUIState OldState);
    void HandleEnterUIState(EUIState NewState);

    UFUNCTION()
    void HandleFadeInFinished();

public:
    void PlayTTSSoundById(FString Id, float FadeOutTime, float FadeInTime);

public:
    void HandleEnterIntro();
    void HandleEnterModeSelect();
    void HandleEnterPlayerSelect();
    void HandleEnterPlayerSelectPost();
    void HandleEnterCourseSelect();
    void HandleEnterCourseSelectPost();

    void LoadDefaultGameOption();
    void ResetGameData();

public:
    void SetGameInfo(const FGameInfo& PGameInfo);
    const FGameInfo& GetGameInfo() const;

    void LoadGameInfoFromJSON();
    void SaveGameInfoToJSON();

    EGameType GetCurrentGameType();
    void SetCurrentGameType(EGameType ChangeGameType);

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UMenuUIImageDataAsset* DA_MenuUI;
    
	UPROPERTY()
	float DoubleClickBlockSeconds = 0.25f;

	double LastClickTimeSeconds = -1.0;

	bool IsClickAllowed()
	{
		const double Now = FPlatformTime::Seconds();
		if (LastClickTimeSeconds > 0.0 && (Now - LastClickTimeSeconds) < DoubleClickBlockSeconds)
		{
			return false; // 더블클릭/연타로 판단 → 차단
		}
		LastClickTimeSeconds = Now;
		return true;
	}
        /**
     * @brief TTS 시스템 초기화 상태 확인
     *
     * @return true 준비됨, false 미준비
     */
    UFUNCTION(BlueprintCallable, Category = "Game|TTS")
        bool IsTTSReady() const;

	UFUNCTION(BlueprintCallable, Category = "Game|TTS")
		void Speak(const FString& Text);

protected:
        // ─────────────────────────────────────────────────────────────────────────────
        // [TTS 시스템]
        // ─────────────────────────────────────────────────────────────────────────────

        /// TTS 매니저
        FTTSManager TTSManager;

private:
    /**
     * @brief 초기 TTS 설정
     */
    void SetupTTS();

    /**
     * @brief 안전한 음성 재생 (에러 체크 포함)
     *
     * @param Text 재생할 텍스트
     * @return true 성공, false 실패
     */
    bool SafeSpeak(const FString& Text);

private:
    UPROPERTY()
    EUIState PrevUIState = EUIState::Intro;
    UPROPERTY(Transient)
    EUIState CurrentUIState = EUIState::Intro;
    UPROPERTY()
    EUIState PostUIState = EUIState::ModeSelect;

    UPROPERTY()
        EGameType CurrentGameType = EGameType::StrokeMode;

    UPROPERTY()
    FGameInfo GameInfo;

    bool bIsFirstScreen = true;
    bool bFromInGame = false;
};