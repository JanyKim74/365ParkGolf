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
	// 가시화
	if (!GM)
	{
		LogError(this, TEXT("UResultWidget::PlayResult: GM == null"));
		return;
	}

	for (AGolfPlayer* Player : GM->PlayerManager->GetPlayers())
	{
		if (Player->bIsContinue)
		{
			return;
		}
	}
	

	float DelayTime = 1.f;

	// 현재 홀 인덱스 검증
	const int32 HoleIdx = GM->CurrentHole - 1;
	const TArray<int32>& ParScores = GM->GameInfo.SelectedMap.ParScores;
	if (!ParScores.IsValidIndex(HoleIdx))
	{
		LogError(this, FString::Printf(TEXT("UResultWidget::PlayResult: ParScores 인덱스 범위 초과 (Hole=%d / Num=%d)"),
			GM->CurrentHole, ParScores.Num()));
		return;
	}

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
		// ★ 홀인원 판단식 수정: "ShotCount == 1" 이면 홀인원
		//>> 수정 필요 (만약 컨시드면 홀 인원이 아님)
		if (ShotCount == 1 && !GM->GetCurrentTurnGolfBall()->IsConceded())
		{
			// FX 호출 안전 방어
			auto* Ball = GM->GetCurrentTurnGolfBall();
			if (Ball && Ball->CameraFXComponent)
			{
				GetWorld()->GetTimerManager().SetTimer(
					GM->DelayedReadyTimer, // 기존 타이머 재활용 또는 새 타이머 정의
					[this]() {
						if (GM->PlayerManager)
						{
							GM->PlayerManager->AdvanceTurn(); // 플레이어 매니저에게 턴 진행 명령
						}
						GetWorld()->GetTimerManager().ClearTimer(GM->TurnCountdownTimer); // 턴 진행 후 카운트다운 타이머 종료
						GM->CurrentTurnCountdownTime = 0.0f; // 카운트다운 초기화
					},
					DelayTime,
						false
						);
				GM->ResultVideoWidgetInstance->ChangeVideoPathAndPlay(TEXT("Video_Holeinone.webm"));
				UGameplayStatics::SetGamePaused(GM->GetWorld(), true);
			}
			else
			{
				LogWarning(this, TEXT("UResultWidget::PlayResult: HoleIn FX를 재생하지 못함 (Ball 또는 CameraFXComponent == null)"));
			}

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

	switch (Score)
	{
	case -3: 
		GetWorld()->GetTimerManager().SetTimer(
			GM->DelayedReadyTimer, // 기존 타이머 재활용 또는 새 타이머 정의
			[this]() {
				if (GM->PlayerManager)
				{
					GM->PlayerManager->AdvanceTurn(); // 플레이어 매니저에게 턴 진행 명령
				}
				GetWorld()->GetTimerManager().ClearTimer(GM->TurnCountdownTimer); // 턴 진행 후 카운트다운 타이머 종료
				GM->CurrentTurnCountdownTime = 0.0f; // 카운트다운 초기화
			},
			DelayTime,
				false
				);
		GM->ResultVideoWidgetInstance->ChangeVideoPathAndPlay(TEXT("Video_Albatross.webm"));
		GM->ResultVideoWidgetInstance->ChangeTextBlockPosition(260.f);
		UGameplayStatics::SetGamePaused(GM->GetWorld(), true);
		SetVisibility(ESlateVisibility::Collapsed);
		break;
	case -2: 
		GetWorld()->GetTimerManager().SetTimer(
			GM->DelayedReadyTimer, // 기존 타이머 재활용 또는 새 타이머 정의
			[this]() {
				if (GM->PlayerManager)
				{
					GM->PlayerManager->AdvanceTurn(); // 플레이어 매니저에게 턴 진행 명령
				}
				GetWorld()->GetTimerManager().ClearTimer(GM->TurnCountdownTimer); // 턴 진행 후 카운트다운 타이머 종료
				GM->CurrentTurnCountdownTime = 0.0f; // 카운트다운 초기화
			},
			DelayTime,
				false
				);
		GM->ResultVideoWidgetInstance->ChangeVideoPathAndPlay(TEXT("Video_Eagle.webm"));
		GM->ResultVideoWidgetInstance->ChangeTextBlockPosition(240.f);
		UGameplayStatics::SetGamePaused(GM->GetWorld(), true);
		SetVisibility(ESlateVisibility::Collapsed);
		break;
	case -1: RestartAnim(Anim_Birdie); break;
	case  0: RestartAnim(Anim_Par);    break; // ★ -0 → 0 수정
	case  1: // 이하 보기는 모두 Bogey 애니메이션
	case  2:
	case  3:
	case  4: RestartAnim(Anim_Bogey);     break;
	case 100: RestartAnim(Anim_DoublePar); break;
	case 101: RestartAnim(Anim_Event); break;
	default:
		LogWarning(this, FString::Printf(TEXT("UResultWidget::PlayResult: 처리되지 않은 Score=%d"), Score));
		break;
	}

	// (보기 이하) ShotCount 사운드 재생 (WeakLambda)
	//if (UWorld* World = GetWorld())
	//{
	//	FTimerHandle TH;
	//	World->GetTimerManager().SetTimer(
	//		TH,
	//		FTimerDelegate::CreateWeakLambda(this, [this, Score, ShotCount]()
	//			{
	//				// 기존 로직 유지: Score <= 0 일 때만 카운트 사운드
	//				if (Score <= 0)
	//				{
	//					PlayShotCountSound(ShotCount);
	//				}
	//			}),
	//		1.2f, false);
	//}
}

void UResultWidget::SetSoundAndImage(int32 Score)
{
	if (!Image_Result)
	{
		LogWarning(this, TEXT("UResultWidget::SetImage: Image_Result == null (UMG에 바인딩되어 있는지 확인)"));
		return;
	}

	if (UTexture2D** FoundPtr = ResultMap.Find(Score))
	{
		if (UTexture2D* Texture = *FoundPtr)
		{
			Image_Result->SetBrushFromTexture(Texture, true);
		}
	}

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
