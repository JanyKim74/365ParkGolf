#include "MenuGameMode.h"

#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Utils/UtilLibrary.h"
#include "SoundManager.h"
#include "JsonHandler.h"
#include "DataAsset/UIStateWidgetMapDataAsset.h" // FUIStateWidgetEntry, UUIStateWidgetMapDataAsset
#include "DataAsset/MenuUIImageDataAsset.h" // FUIStateWidgetEntry, UUIStateWidgetMapDataAsset

#include "Widgets/Menu/PlayerSelectWidget.h"
#include "Widgets/Menu/PlayerSelectProfileWidget.h"
#include "Widgets/Menu/CourseSelectWidget.h"
#include "Widgets/Menu/CourseSelectDetailWidget.h"
#include "Widgets/PasswordWidget.h"
#include "Widgets/KeyboardWidget.h"

#include "Components/WrapBox.h"

#include "ParkDay/Utils/UtilLibrary.h"
#include "ParkDay/Widgets/ContinuePopupWidget.h"
#include "ParkDay/TerraParkGameInstance.h"
#include "ParkDay/Widgets/FadeWidget.h"
#include "ParkDay/Utils/JsonLoader.h"

AMenuGameMode::AMenuGameMode()
{
    PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FClassFinder<UContinuePopupWidget> ContinuePopupWidgetBPClass(
		TEXT("/Game/UMG/UI/WBP_ContinuePopup.WBP_ContinuePopup_C")
	);
	if (ContinuePopupWidgetBPClass.Succeeded())
	{
		ContinuePopupWidgetClass = ContinuePopupWidgetBPClass.Class;
		UE_LOG(LogTemp, Log, TEXT("✅ ContinuePopupWidgetClass 로드 성공"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("⚠️ ContinuePopupWidgetClass 로드 실패"));
	}
}

void AMenuGameMode::LoadContinuePopupWidget()
{
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	ContinuePopupWidget = CreateWidget<UContinuePopupWidget>(PC, ContinuePopupWidgetClass);
	if (ContinuePopupWidget)
	{

		ContinuePopupWidget->AddToViewport(9000);
		ContinuePopupWidget->SetVisibility(ESlateVisibility::Collapsed);
		UE_LOG(LogTemp, Log, TEXT("✅ 이어하기 팝업 위젯 로딩 완료"));
	}
}

void AMenuGameMode::LoadPasswordWidget()
{
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);

    FSoftClassPath PasswordWidgetSoftClass(TEXT("/Game/UMG/UI/WBP_Password.WBP_Password_C"));
    UClass* LoadedClass = PasswordWidgetSoftClass.TryLoadClass<UPasswordWidget>();

    PasswordWidget = CreateWidget<UPasswordWidget>(PC, LoadedClass);
    if (PasswordWidget)
    {
        PasswordWidget->AddToViewport(9999);
        PasswordWidget->SetVisibility(ESlateVisibility::Collapsed);
        UE_LOG(LogTemp, Log, TEXT("✅ PasswordWidget 로딩 완료"));
    }
}

void AMenuGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);

    const FString Diff = UGameplayStatics::ParseOption(Options, TEXT("bFromInGame"));
    if (!Diff.IsEmpty())
        bFromInGame = Diff.Equals(TEXT("true"), ESearchCase::IgnoreCase);
}

void AMenuGameMode::LoadDefaultGameOption()
{
    if (!GameInfo.bIsRoundEnd)
    {
        UE_LOG(LogTemp, Log, TEXT("LoadDefaultGameOption skipped"));
        return;
    }

    FDefaultGameOption DefaultGameOption;
    // 가장 간단한 방법: 파일명만 넘기기
    FString SaveFileName = TEXT("defaultGameData.json");
    if (!JsonLoadHelper::LoadSaveJsonToStruct<FDefaultGameOption>(SaveFileName, DefaultGameOption))
    {
        UE_LOG(LogTemp, Warning, TEXT("defaultGameData.json 없음 - 기본값 사용"));
        return;
    }

    int32 GameType = GameInfo.GameOptions.GameType;
    GameInfo.GameOptions = DefaultGameOption.GameOptions;
    GameInfo.GameOptions.GameType = GameType;
    SaveGameInfoToJSON();
    LoadGameInfoFromJSON();
}

void AMenuGameMode::BeginPlay()
{
    Super::BeginPlay();
    SetupTTS();
    if (UTerraParkgameInstance* GI = Cast<UTerraParkgameInstance>(GetWorld()->GetGameInstance()))
    {
        GI->StopLoadingScreen();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("AMenuGameMode::HandleEnterModeSelect() ==> GI is null!!"));
    }

    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (PC)
    {
        PC->bShowMouseCursor = true;
        PC->bEnableClickEvents = true;
        PC->bEnableMouseOverEvents = true;

        FInputModeUIOnly InputMode;
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

        PC->SetInputMode(InputMode);
    }

    LoadContinuePopupWidget();
    LoadPasswordWidget();
    LoadGameInfoFromJSON();

    CurrentUIState = InitialState;

    //강제종료 였던 경우 (이어하기)
    if (!GameInfo.bIsRoundEnd)
    {
        bIsFirstScreen = true;
    }

    if (bFromInGame)
    {
        CurrentUIState = EUIState::ModeSelect;
    }
    LoadDefaultGameOption();

    // 1) DataAsset에 등록된 모든 상태 위젯을 먼저 생성/등록
    RegisterAllStateWidgetsFromConfig();

    // 2) 초기 상태로 진입 (Visible 토글 + Enter 훅)
    SetStateWidgetVisible(CurrentUIState, true);
    HandleEnterUIState(CurrentUIState);

	if (KeyBoardWidgetClass)
	{
		KeyBoardWidgetInstance = Cast<UKeyboardWidget>(CreateWidget<UUserWidget>(UGameplayStatics::GetPlayerController(GetWorld(), 0), KeyBoardWidgetClass));
		KeyBoardWidgetInstance->AddToViewport(100000);
		KeyBoardWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("KeyBoardWidget Class is null"));
	}
}


void AMenuGameMode::RegisterAllStateWidgetsFromConfig()
{
    if (!UIStateWidgetConfig)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIStateMachine] UIStateWidgetConfig is null"));
        return;
    }

    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIStateMachine] RegisterAllStateWidgetsFromConfig: No PlayerController"));
        return;
    }

    // 기존 캐시 정리(레벨 재진입 대비)
    for (TPair<EUIState, UUserWidget*>& Pair : StateWidgets)
    {
        if (Pair.Value)
        {
            Pair.Value->RemoveFromParent();
        }
    }
    StateWidgets.Empty();

    // DataAsset 엔트리 전체 순회 -> 위젯 생성
    for (const FUIStateWidgetEntry& Entry : UIStateWidgetConfig->Entries)
    {
        if (!Entry.WidgetClass)
        {
            UE_LOG(LogTemp, Warning, TEXT("[UIStateMachine] WidgetClass null for state %d"), (int32)Entry.State);
            continue;
        }

        if (StateWidgets.Contains(Entry.State))
        {
            UE_LOG(LogTemp, Warning, TEXT("[UIStateMachine] Duplicate state entry detected: %d"), (int32)Entry.State);
            continue;
        }

        UUserWidget* Widget = CreateWidget<UUserWidget>(PC, Entry.WidgetClass);
        if (!Widget)
        {
            UE_LOG(LogTemp, Warning, TEXT("[UIStateMachine] CreateWidget failed for state %d"), (int32)Entry.State);
            continue;
        }

        // ZOrder는 여기서 한 번만 적용하면 됨
        Widget->AddToViewport(Entry.ZOrder);

        // 초기에는 전부 숨김(초기 상태만 BeginPlay에서 켬)
        Widget->SetVisibility(ESlateVisibility::Collapsed);

        StateWidgets.Add(Entry.State, Widget);
    }

    UE_LOG(LogTemp, Log, TEXT("[UIStateMachine] Registered widgets: %d"), StateWidgets.Num());
}

void AMenuGameMode::SetStateWidgetVisible(EUIState State, bool bVisible)
{
    if (UUserWidget** Found = StateWidgets.Find(State))
    {
        if (UUserWidget* W = *Found)
        {
            W->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
            return;
        }
    }

    // 등록 누락 디버깅용
    UE_LOG(LogTemp, Warning, TEXT("[UIStateMachine] SetStateWidgetVisible: Widget not found for state %d"), (int32)State);
}

void AMenuGameMode::SetupTTS()
{
    if (TTSManager.IsInitialized())
    {
        UE_LOG(LogTemp, Warning, TEXT("[MenuGameMode] TTS 시스템 준비 완료"));

        // 초기 설정
        TTSManager.SetVolume(95);
        TTSManager.SetRate(0);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[MenuGameMode] TTS 시스템 초기화 실패"));
        FString Error = TTSManager.GetLastError();
        UE_LOG(LogTemp, Error, TEXT("[MenuGameMode] 에러: %s"), *Error);
    }
}

bool AMenuGameMode::IsTTSReady() const
{
    return TTSManager.IsInitialized();
}

void AMenuGameMode::Speak(const FString& Text)
{
    if (!IsTTSReady())
    {
        UE_LOG(LogTemp, Warning, TEXT("[InGameMode] TTS가 준비되지 않았습니다"));
        return;
    }

    SafeSpeak(Text);
    UE_LOG(LogTemp, Warning, TEXT("[InGameMode] 커스텀 음성: %s"), *Text);
}

bool AMenuGameMode::SafeSpeak(const FString& Text)
{
    if (!TTSManager.IsInitialized())
    {
        UE_LOG(LogTemp, Error, TEXT("[MenuGameMode] TTS 미초기화 상태에서 음성 재생 시도"));
        return false;
    }

    if (Text.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[MenuGameMode] 빈 텍스트 재생 시도"));
        return false;
    }

    bool bSuccess = TTSManager.Speak(Text);

    if (!bSuccess)
    {
        FString Error = TTSManager.GetLastError();
        UE_LOG(LogTemp, Error, TEXT("[MenuGameMode] 음성 재생 실패: %s"), *Error);
    }

    return bSuccess;
}

void AMenuGameMode::ChangeUIState(EUIState NewState)
{
    PrevUIState = CurrentUIState;

    if (CurrentUIState == NewState)
    {
        return;
    }
    CurrentUIState = NewState;

    UUtilLibrary::FadeIn(GetWorld(), 0.25f, FFadeCallback::CreateUObject(this, &AMenuGameMode::HandleFadeInFinished));
}

void AMenuGameMode::HandleEnterUIState(EUIState NewState)
{

    switch (NewState)
    {
    case EUIState::Intro:
        HandleEnterIntro();
        break;

    case EUIState::ModeSelect:
        HandleEnterModeSelect();
        break;

    case EUIState::PlayerSelect:
        HandleEnterPlayerSelect();
        HandleEnterPlayerSelectPost();
        break;

    case EUIState::CourseSelect:
        HandleEnterCourseSelect();
        HandleEnterCourseSelectPost();
        break;

    case EUIState::Loading:
        break;

    default:
        break;
    }
}

void AMenuGameMode::HandleFadeInFinished()
{
    HandleExitUIState(PrevUIState);
    SetStateWidgetVisible(PrevUIState, false);

    UUtilLibrary::FadeOut(GetWorld(), 0.25f);

    SetStateWidgetVisible(CurrentUIState, true);
    HandleEnterUIState(CurrentUIState);
    UE_LOG(LogTemp, Log, TEXT("[UIStateMachine] Change UI Mode: %d"), CurrentUIState);
}

void AMenuGameMode::ResetGameData()
{
    GameInfo.Reset();
    LoadDefaultGameOption();
}

void AMenuGameMode::HandleEnterIntro()
{
    OnEnterIntroDele.Broadcast();
    if (GameInfo.bIsRoundEnd)
        ResetGameData();

}

void AMenuGameMode::HandleEnterModeSelect()
{
    UE_LOG(LogTemp, Log, TEXT("HandleEnterModeSelect : 1"));
    //강제종료인경우
     if (!GameInfo.bIsRoundEnd)
	 {
		 ContinuePopupWidget->SetVisibility(ESlateVisibility::Visible);
	 }

    PlayTTSSoundById("Voice.ModeSelect", 0.5f, 0.5f);
    UE_LOG(LogTemp, Log, TEXT("HandleEnterModeSelect : 2"));
    if (bFromInGame)
    {
        bIsFirstScreen = true;
        bFromInGame = false;
        if (auto* SM = GetGameInstance()->GetSubsystem<USoundManager>())
        {
            SM->PlayBGM_ById(TEXT("BGM.ModeSelect"));
            UE_LOG(LogTemp, Log, TEXT("HandleEnterModeSelect : 2 - BGM.ModeSelect Play"));
        }
    }
    UE_LOG(LogTemp, Log, TEXT("HandleEnterModeSelect : 3"));
    if (bIsFirstScreen)
    {
        if (auto* SM = GetGameInstance()->GetSubsystem<USoundManager>())
        {
            if (!SM->BGMIsPlaying())
            {
                SM->PlayBGM_ById(TEXT("BGM.ModeSelect"));
                UE_LOG(LogTemp, Log, TEXT("HandleEnterModeSelect : 3 - BGM.ModeSelect Play"));
            }
        }

        AsyncTask(ENamedThreads::GameThread, [=, this]()
            {
                UUtilLibrary::UnMountPak(GetWorld());
            });
    }
}

void AMenuGameMode::HandleEnterPlayerSelect()
{
    OnEnterPlayerSelectDele.Broadcast();
    PlayTTSSoundById("Voice.PlayerSelect", 0.5f, 0.5f);
}

void AMenuGameMode::HandleEnterPlayerSelectPost()
{
    OnEnterPlayerSelectPostDele.Broadcast();
}

void AMenuGameMode::HandleEnterCourseSelect()
{

    LoadGameInfoFromJSON();
    OnEnterCourseSelectDele.Broadcast();
    PlayTTSSoundById("Voice.CourseSelect", 0.5f, 0.5f);
}

void AMenuGameMode::HandleEnterCourseSelectPost()
{
    OnEnterCourseSelectPostDele.Broadcast();
}

void AMenuGameMode::HandleExitUIState(EUIState OldState)
{
    // ★ 여기서는 위젯 생성/탐색(GetOrCreate) 절대 하지 마세요.
    // 상태별로 나가기 전에 필요한 로직만 작성합니다.

    switch (OldState)
    {
    case EUIState::Intro:

        if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
        {
			FInputModeGameAndUI InputMode;
			InputMode.SetHideCursorDuringCapture(false);
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

            PC->bShowMouseCursor = true;
            PC->SetInputMode(InputMode);
        }
        break;

    case EUIState::ModeSelect:
        bIsFirstScreen = false;
        break;

    case EUIState::PlayerSelect:
        break;

    case EUIState::CourseSelect:
        break;

    case EUIState::Loading:
        break;

    default:
        break;
    }
}

void AMenuGameMode::PlayTTSSoundById(FString Id, float FadeOutTime, float FadeInTime)
{
    if (auto* SM = GetGameInstance()->GetSubsystem<USoundManager>())
    {
        FName Name = FName(*Id);
        SM->PlayTTS_Interrupt_ById(Name, FadeOutTime, FadeInTime);
    }
}

UUserWidget* AMenuGameMode::GetStateWidget(EUIState State) const
{
    if (UUserWidget* const* Found = StateWidgets.Find(State))
    {
        return *Found;
    }
    return nullptr;
}

void AMenuGameMode::SetGameInfo(const FGameInfo& PGameInfo)
{
    GameInfo = PGameInfo;
}

const FGameInfo& AMenuGameMode::GetGameInfo() const
{
    return GameInfo;
}


void AMenuGameMode::LoadGameInfoFromJSON()
{
    FString SaveFilePath = FPaths::ProjectSavedDir() + TEXT("GameData.json");

    UJsonHandler::LoadGameInfoFromJson(GameInfo, SaveFilePath);
}

void AMenuGameMode::SaveGameInfoToJSON()
{
    bool bSuccess = UJsonHandler::SaveGameInfoToJson(GameInfo, FPaths::ProjectSavedDir() + TEXT("GameData.json"));
    if (bSuccess)
    {
        UE_LOG(LogTemp, Log, TEXT("SaveGameInfo : Success"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("SaveGameInfo : Fail"));
    }
}

EGameType AMenuGameMode::GetCurrentGameType()
{
    return CurrentGameType;
}

void AMenuGameMode::SetCurrentGameType(EGameType ChangeGameType)
{
    CurrentGameType = ChangeGameType;
    GameInfo.GameOptions.GameType = static_cast<int32>(ChangeGameType);
    SaveGameInfoToJSON();
}

