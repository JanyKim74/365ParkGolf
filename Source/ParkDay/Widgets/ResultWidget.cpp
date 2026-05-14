#include "ResultWidget.h"

#include "Animation/UMGSequencePlayer.h"
#include "Animation/WidgetAnimation.h"
#include "Components/Image.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"                      // ★ 텍스처 타입
#include "TimerManager.h"                          // ★ 타이머
#include "Kismet/KismetSystemLibrary.h"            // ★ 에디터 화면 로그용

#include "ParkDay/CameraFXComponent.h"
#include "ParkDay/InGameMode.h"
#include "ParkDay/Structs/DataTableStruct.h"
// (선택) 아래가 없다면 Ball의 멤버 접근 시 컴파일 에러 가능. 프로젝트 경로에 맞춰 유지/수정하세요.
#include "ParkDay/GolfBall.h"                      // ★ Ball->CameraFXComponent 접근 안전
#include "../GolfPlayerManager.h"
#include "../GolfPlayer.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"

// ──────────────────────────────────────────────────────────────
// 로컬 로그/온스크린 유틸 (간단)
// ──────────────────────────────────────────────────────────────
static void ScreenLog(UObject* WorldContext, const FString& Msg, const FLinearColor& Color, float Time = 2.5f)
{
	if (!WorldContext) return;
	UKismetSystemLibrary::PrintString(WorldContext, Msg, true, true, Color, Time);
}

static void LogError(UObject* WorldContext, const FString& Msg)
{
	UE_LOG(LogTemp, Error, TEXT("%s"), *Msg);
	ScreenLog(WorldContext, Msg, FLinearColor::Red);
}

static void LogWarning(UObject* WorldContext, const FString& Msg)
{
	UE_LOG(LogTemp, Warning, TEXT("%s"), *Msg);
	ScreenLog(WorldContext, Msg, FLinearColor::Yellow);
}

static void LogInfo(UObject* WorldContext, const FString& Msg)
{
	UE_LOG(LogTemp, Log, TEXT("%s"), *Msg);
#if WITH_EDITOR
	ScreenLog(WorldContext, Msg, FLinearColor::Green, 1.5f);
#endif
}

// ──────────────────────────────────────────────────────────────

void UResultWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!GetWorld())
	{
		LogError(this, TEXT("UResultWidget::NativeOnInitialized: GetWorld() == null"));
		return;
	}

	GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());
	if (!GM)
	{
		LogError(this, TEXT("UResultWidget::NativeOnInitialized: InGameMode 캐스팅 실패 (GM == null)"));
	}
}

void UResultWidget::NativeConstruct()
{
	Super::NativeConstruct();

	LogInfo(this, TEXT("UResultWidget: Construct"));

	if (!GM)
	{
		// 혹시 OnInitialized 시점에 월드가 준비 전이었을 수 있으니 한 번 더 시도
		GM = GetWorld() ? Cast<AInGameMode>(GetWorld()->GetAuthGameMode()) : nullptr;
		if (!GM)
		{
			LogError(this, TEXT("UResultWidget::NativeConstruct: GM이 유효하지 않음. ResultMap을 초기화하지 못함"));
			return;
		}
	}

	if (!GM->ResultWidgetDT)
	{
		LogError(this, TEXT("UResultWidget::NativeConstruct: ResultWidgetDT(DataTable) == null"));
		return;
	}

	// DataTable의 RowStruct 검증(선택)
	if (GM->ResultWidgetDT->RowStruct != FResultUI::StaticStruct())
	{
		LogWarning(this, FString::Printf(TEXT("UResultWidget::NativeConstruct: DataTable RowStruct가 FResultUI가 아닙니다. (%s)"),
			*GetNameSafe(GM->ResultWidgetDT->RowStruct)));
	}

	ResultMap.Reset();
	ResultSoundMap.Reset();
	ResultConcedeMap.Reset();
	LogInfo(this, TEXT("DT_ResultUI Init Start"));

	GM->ResultWidgetDT->ForeachRow<FResultUI>(TEXT("UResultWidget_Init"),
		[&](const FName& RowName, const FResultUI& Row)
		{
			if (!Row.ResultTexture)
			{
				LogWarning(this, FString::Printf(TEXT("DT_ResultUI: '%s' 행 ResultTexture가 null"), *RowName.ToString()));
				return;
			}
			ResultMap.Add(Row.Score, Row.ResultTexture);
			ResultSoundMap.Add(Row.Score, Row.ResultSound);
			ResultConcedeMap.Add(Row.Score, Row.ConcedeSound);
			UE_LOG(LogTemp, Log, TEXT("DT_ResultUI Add UI Success : %s (Score=%d)"), *RowName.ToString(), Row.Score);
		});

	LogInfo(this, TEXT("DT_ResultUI Init Done"));
}

void UResultWidget::PlayAnim(UWidgetAnimation* Anim, bool bLoop)
{
	if (!Anim)
	{
		LogWarning(this, TEXT("UResultWidget::PlayAnim: Anim == null"));
		return;
	}

	const int32 NumLoopsToPlay = bLoop ? 0 /*무한루프*/ : 1;
	AnimPlayer = PlayAnimation(
		Anim,
		/*StartAtTime*/ 0.f,
		/*NumLoopsToPlay*/ NumLoopsToPlay,
		/*PlayMode*/ EUMGSequencePlayMode::Forward,
		/*PlaybackSpeed*/ 1.f,
		/*bRestoreState*/ false
	);
}

void UResultWidget::PauseAnim()
{
	if (AnimPlayer)
	{
		AnimPlayer->Pause();
	}
}

void UResultWidget::StopAnim(const UWidgetAnimation* Anim)
{
	if (Anim) StopAnimation(Anim);
	AnimPlayer = nullptr;
}

void UResultWidget::RestartAnim(UWidgetAnimation* Anim)
{
	if (!Anim)
	{
		LogWarning(this, TEXT("UResultWidget::RestartAnim: Anim == null"));
		return;
	}
	const UWidgetAnimation* ConstAnim = Anim;
	StopAnimation(ConstAnim);
	AnimPlayer = PlayAnimation(Anim, 0.f, 1);

	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UResultWidget::ResumeAnim(UWidgetAnimation* Anim)
{
	if (AnimPlayer)
	{
		// Pause 이후 이어서 재생
		AnimPlayer->Play(0.f, 0, EUMGSequencePlayMode::Forward, 1.0f, false);
	}
	else if (Anim)
	{
		// Stop 상태였다면 처음부터
		AnimPlayer = PlayAnimation(Anim, 0.f, 1);
	}
}

void UResultWidget::PlayResult(int32 Score)
{

	LogInfo(this, FString::Printf(TEXT("UResultWidget:PlayResult  START ------ SCORE -[%d]"), Score));
	// ── GM 유효성 ──────────────────────────────────────────────
	if (!GM)
	{
		LogError(this, TEXT("UResultWidget::PlayResult: GM == null"));
		return;
	}
	LogInfo(this, FString::Printf(TEXT("UResultWidget:PlayResult  START ------ GM->PlayerManager SCORE -[%d]"), Score));
	// ── PlayerManager null 체크 (크래시 원인 #1) ───────────────
	if (!GM->PlayerManager)
	{
		LogError(this, TEXT("UResultWidget::PlayResult: GM->PlayerManager == null"));
		return;
	}
	LogInfo(this, FString::Printf(TEXT("UResultWidget:PlayResult  START ------ SCORE -[%d] - 1"), Score));
	// 수정 코드 - IsValid() 사용
	for (AGolfPlayer* Player : GM->PlayerManager->GetPlayers())
	{
		if (IsValid(Player) && Player->bIsContinue)
		{
			LogError(this, TEXT("UResultWidget::PlayResult: Player->bIsContinue"));
			return;
		}
	}
	LogInfo(this, FString::Printf(TEXT("UResultWidget:PlayResult  START ------ SCORE -[%d] - 2"), Score));
	float DelayTime = 0.5f;

	// 현재 홀 인덱스 검증
	const int32 HoleIdx = GM->CurrentHole - 1;
	const TArray<int32>& ParScores = GM->GameInfo.SelectedMap.ParScores;
	if (!ParScores.IsValidIndex(HoleIdx))
	{
		LogError(this, FString::Printf(TEXT("UResultWidget::PlayResult: ParScores 인덱스 범위 초과 (Hole=%d / Num=%d)"),
			GM->CurrentHole, ParScores.Num()));
		return;
	}
	LogInfo(this, FString::Printf(TEXT("UResultWidget:PlayResult  START ------ SCORE -[%d] - 3"), Score));
	// 홀 이동 중이라면 1회 무시
	//if (bIsNextHole)
	//{
	//	bIsNextHole = false;
	//	return;
	//}

	const int32 Par = ParScores[HoleIdx];
	const int32 ShotCount = Par + Score;   // Score: 파 대비 상대 스코어(-3, -2, -1, 0, 1, ...)

	if (GM->CurrentGameMode == EGolfGameMode::StrokeMode)
	{
		// ── GetCurrentTurnGolfBall() null 체크 (크래시 원인 #2) ─
		// ShotCount == 1 체크 후 Ball을 먼저 얻어서 null 확인
		auto* Ball = GM->GetCurrentTurnGolfBall();
		if (ShotCount == 1 && Ball && !Ball->IsConceded())
		{
			LogInfo(this, FString::Printf(TEXT("UResultWidget:PlayResult  START ------ SCORE -[%d] - 3 - a"), Score));
			// FX 호출 안전 방어
			if (Ball->CameraFXComponent)
			{
				GetWorld()->GetTimerManager().SetTimer(
					GM->DelayedReadyTimer,
					[this]() {
						if (GM && GM->PlayerManager)
						{
							GM->PlayerManager->AdvanceTurn();
						}
						if (GM)
						{
							GetWorld()->GetTimerManager().ClearTimer(GM->TurnCountdownTimer);
							GM->CurrentTurnCountdownTime = 0.0f;
						}
					},
					DelayTime, false);
				LogInfo(this, FString::Printf(TEXT("UResultWidget:PlayResult  START ------ SCORE -[%d] - 3 - b"), Score));
				// ── ResultVideoWidgetInstance null 체크 (크래시 원인 #3) ─
				if (GM->ResultVideoWidgetInstance)
				{
					GM->ResultVideoWidgetInstance->ChangeVideoPathAndPlay(TEXT("Video_Holeinone.webm"));
				}
				else
				{
					LogWarning(this, TEXT("UResultWidget::PlayResult: ResultVideoWidgetInstance == null (HoleInOne)"));
				}
				UGameplayStatics::SetGamePaused(GM->GetWorld(), true);
			}
			else
			{
				LogWarning(this, TEXT("UResultWidget::PlayResult: HoleIn FX를 재생하지 못함 (Ball 또는 CameraFXComponent == null)"));
			}
			LogInfo(this, FString::Printf(TEXT("UResultWidget:PlayResult  START ------ SCORE -[%d] - 3 - c"), Score));
			//// 사운드 지연 재생 (위젯 파괴 시 크래시 방지용 WeakLambda)
			//if (UWorld* World = GetWorld())
			//{
			//	FTimerHandle TH;
			//	World->GetTimerManager().SetTimer(
			//		TH,
			//		FTimerDelegate::CreateWeakLambda(this, [this, ShotCount]()
			//			{
			//				// 홀인원은 Score가 음수(Par-1 또는 그 이하)인 케이스와 상관없이 1타 사운드를 낼 수 있게 ShotCount 기준으로 처리
			//				PlayShotCountSound(ShotCount);
			//			}),
			//		1.2f, false);
			//}
			return; // 홀인원은 아래 결과 연출 스킵(필요시 변경)
		}
	}

	// 결과 이미지/애니메이션
	SetSoundAndImage(Score);

	LogInfo(this, FString::Printf(TEXT("UResultWidget:PlayResult  START ------ SCORE -[%d] - 4"), Score));
	switch (Score)
	{
	case -3:
		GetWorld()->GetTimerManager().SetTimer(
			GM->DelayedReadyTimer,
			[this]() {
				if (GM && GM->PlayerManager)
				{
					GM->PlayerManager->AdvanceTurn();
				}
				if (GM)
				{
					GetWorld()->GetTimerManager().ClearTimer(GM->TurnCountdownTimer);
					GM->CurrentTurnCountdownTime = 0.0f;
				}
			},
			DelayTime, false
		);
		LogInfo(this, TEXT("UResultWidget::PlayResult:Call -BP -  GM->ResultVideoWidgetInstanc"));
		// ── ResultVideoWidgetInstance null 체크 (크래시 원인 #3) ─
		if (GM->ResultVideoWidgetInstance)
		{
			GM->ResultVideoWidgetInstance->ChangeVideoPathAndPlay(TEXT("Video_Albatross.webm"));
			GM->ResultVideoWidgetInstance->ChangeTextBlockPosition(260.f);
		}
		else
		{
			LogWarning(this, TEXT("UResultWidget::PlayResult: ResultVideoWidgetInstance == null (Albatross)"));
		}
		UGameplayStatics::SetGamePaused(GM->GetWorld(), true);
		SetVisibility(ESlateVisibility::Collapsed);
		break;
	case -2:
		GetWorld()->GetTimerManager().SetTimer(
			GM->DelayedReadyTimer,
			[this]() {
				if (GM && GM->PlayerManager)
				{
					GM->PlayerManager->AdvanceTurn();
				}
				if (GM)
				{
					GetWorld()->GetTimerManager().ClearTimer(GM->TurnCountdownTimer);
					GM->CurrentTurnCountdownTime = 0.0f;
				}
			},
			DelayTime, false
		);
		LogInfo(this, TEXT("UResultWidget::PlayResult:Call -BP -  GM->ResultVideoWidgetInstanc"));
		// ── ResultVideoWidgetInstance null 체크 (크래시 원인 #3) ─
		if (GM->ResultVideoWidgetInstance)
		{
			GM->ResultVideoWidgetInstance->ChangeVideoPathAndPlay(TEXT("Video_Eagle.webm"));
			GM->ResultVideoWidgetInstance->ChangeTextBlockPosition(240.f);
		}
		else
		{
			LogWarning(this, TEXT("UResultWidget::PlayResult: ResultVideoWidgetInstance == null (Eagle)"));
		}
		UGameplayStatics::SetGamePaused(GM->GetWorld(), true);
		SetVisibility(ESlateVisibility::Collapsed);
		break;
		/*
	case -1: RestartAnim(Result_birdie); break;
	case  0: RestartAnim(Result_par);    break; // ★ -0 → 0 수정
	case  1: // 이하 보기는 모두 Bogey 애니메이션
	case  2:RestartAnim(Result_bogey);     break;
	case  3:RestartAnim(Result_dbogey);     break;
	case  4: RestartAnim(Result_tbogey);     break;
	case 100: RestartAnim(Result_qbogey); break;

	default:
		LogWarning(this, FString::Printf(TEXT("UResultWidget::PlayResult: 처리되지 않은 Score=%d"), Score));
		break;
	}
	*/

	//case -1: ResultPlay(0); break;		// 버디
	//case  0: ResultPlay(1);    break;	// 파
	//case  1:  ResultPlay(2);   break;	// 보기
	//case  2:  ResultPlay(3);   break;	// 더블보기
	//case  3:  ResultPlay(4);   break;	// 트리플 보기
	//case  4:   ResultPlay(5);   break; // 쿼드로플 보기
	//case 100:  ResultPlay(6); break;   // 더블파

	case -1: ShowResultPanel(birdie, 3.0f); ResultPlay(0); break;   // 버디
	case  0: ShowResultPanel(par, 3.0f); ResultPlay(1); break;   // 파
	case  1: ShowResultPanel(bogey, 3.0f); ResultPlay(2); break;   // 보기
	case  2: ShowResultPanel(dbogey, 3.0f); ResultPlay(3); break;   // 더블보기
	case  3: ShowResultPanel(tbogey, 3.0f); ResultPlay(4); break;   // 트리플보기
	case  4: ShowResultPanel(tbogey, 3.0f); ResultPlay(5); break;   // 쿼드로플 (tbogey 재사용 또는 별도 패널)
	case 100: ShowResultPanel(dpar, 3.0f); ResultPlay(6); break;   // 더블파
		
	}

	LogWarning(this, FString::Printf(TEXT("UResultWidget:PlayResult   ------ SCORE -[%d]"), Score));

}


void UResultWidget::ShowResultPanel(UCanvasPanel* Panel, float HideDelay)
{
	if (!IsValid(Panel))
	{
		LogWarning(this, TEXT("UResultWidget::ShowResultPanel: Panel == null"));
		return;
	}

	// 1) 기존 타이머 클리어 (연속 호출 시 중복 방지)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ResultHideTimer);
	}

	// 2) 모든 패널 먼저 끄고
	HideAllResultPanels();

	// 3) 요청 패널만 켜기
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	LogInfo(this, FString::Printf(TEXT("UResultWidget::ShowResultPanel: [%s] Visible"), *Panel->GetName()));

	// 4) HideDelay 초 후 자동 Hide
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ResultHideTimer,
			FTimerDelegate::CreateWeakLambda(this, [this]()
				{
					if (IsValid(this))
					{
						// ★ 자식 패널 먼저 끄고
						HideAllResultPanels();
						// ★ 루트도 Collapsed (Hit Test 완전 차단 해제)
						SetVisibility(ESlateVisibility::Collapsed);
					}
				}),
			HideDelay, false);
	}
}


void UResultWidget::HideAllResultPanels()
{
	// 패널 배열로 일괄 처리
	TArray<UCanvasPanel*> AllPanels = { birdie, par, bogey, dbogey, tbogey, dpar };
	for (UCanvasPanel* Panel : AllPanels)
	{
		if (IsValid(Panel))
		{
			Panel->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}


void UResultWidget::SetResultIndex(int value)
{

	ResultPlay(value);
}

void UResultWidget::SetSoundAndImage(int32 Score)
{
	//if (!Image_Result)
	//{
	//	LogWarning(this, TEXT("UResultWidget::SetImage: Image_Result == null (UMG에 바인딩되어 있는지 확인)"));
	//	return;
	//}

	//if (UTexture2D** FoundPtr = ResultMap.Find(Score))
	//{
	//	if (UTexture2D* Texture = *FoundPtr)
	//	{
	//		Image_Result->SetBrushFromTexture(Texture, true);
	//	}
	//}

	if (TSoftObjectPtr<USoundBase>* FoundPtr = ResultSoundMap.Find(Score))
	{
		// 이미 로드돼 있으면 그냥 사용
		if (USoundBase* Sound = FoundPtr->Get())
		{
			ResultSound = Sound;
		}
		// 아직 안 로드돼 있으면 동기 로드
		else if (USoundBase* SoundSync = FoundPtr->LoadSynchronous())
		{
			ResultSound = SoundSync;
		}
		else
		{
			LogWarning(this, FString::Printf(TEXT("UResultWidget::SetSoundAndImage: Score=%d에 해당하는 사운드가 없음"), Score));
		}
	}

	if (TSoftObjectPtr<USoundBase>* FoundPtr = ResultConcedeMap.Find(Score))
	{
		// 이미 로드돼 있으면 그냥 사용
		if (USoundBase* Sound = FoundPtr->Get())
		{
			ConcedeSound = Sound;
		}
		// 아직 안 로드돼 있으면 동기 로드
		else if (USoundBase* SoundSync = FoundPtr->LoadSynchronous())
		{
			ConcedeSound = SoundSync;
		}
		else
		{
			LogWarning(this, FString::Printf(TEXT("UResultWidget::SetSoundAndImage: Score=%d에 해당하는 사운드가 없음"), Score));
		}
	}

	// 매칭 실패 시 로그
}