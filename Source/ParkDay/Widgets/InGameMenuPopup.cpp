#include "InGameMenuPopup.h"
#include "Kismet/GameplayStatics.h" // 예시용, 필요시 포함
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "../InGameMode.h"
#include "../GolfPlayerController.h"
#include "../GolfPlayerManager.h"
#include "../StrokeMenuWidget.h"
#include "ParkDay/SoundManager.h"


void UInGameMenuPopup::NativeConstruct()
{
	Super::NativeConstruct();

	GM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());

	Button_Confirm->OnClicked.AddDynamic(this, &UInGameMenuPopup::HandleClickedPopupConfirmButton);
	Button_Cancel->OnClicked.AddDynamic(this, &UInGameMenuPopup::HandleClickedPopupCancelButton);
}

void UInGameMenuPopup::ChangeDescription(FText Description)
{
	if (TextBlock_Description)
		TextBlock_Description->SetText(Description);
	else
		UE_LOG(LogTemp, Error, TEXT("TextBlock_Description in null"));
}

void UInGameMenuPopup::UpdatePopupForUseMulligan()
{
	if (GM)
	{
		GM->StrokeWidgetInstance->OnMulliganButtonClicked();
	}
}

void UInGameMenuPopup::UpdatePopupForNextHole()
{
	if (!GM || GM->GetCurrentGameState() != EGameState::Game_Play)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetNextHole() = Can move next hole only gameplay state"));
		return;
	}

	AGolfPlayer* Player = GM->PlayerManager->GetPlayers()[GM->CurrentPlayerIndex];

	OnClickedPopupConfirmDele.BindUObject(this, &UInGameMenuPopup::SetNextHole);
	FText Description = NSLOCTEXT("InGameMenuPopup", "GoNextHolePopup", "다음 홀로 이동 하시겠습니까?");

	if (auto* SM = GetGameInstance()->GetSubsystem<USoundManager>())
	{
		SM->Play2D_ById("Voice.Q.UseNextHole");
	}

	if (!Player)
	{
		return;
	}

	EPlayerState PlayerState = Player->GetPlayerState();

	if (PlayerState != EPlayerState::Player_Ready)
	{
		Description = NSLOCTEXT("InGameMenuPopup", "GoNextHolePopup_Fail", "지금은 다음 홀로 이동할 수 없습니다.");
		OnClickedPopupConfirmDele.Unbind();

		UE_LOG(LogTemp, Error, TEXT("Can move next hole only Player ready"));
	}
	ChangeDescription(Description);
	SetVisibility(ESlateVisibility::Visible);
}

void UInGameMenuPopup::UpdatePopupForNextPlayer()
{
	if (!GM || GM->GetCurrentGameState() != EGameState::Game_Play)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetNextHole() = Can move next player only gameplay state"));
		return;
	}

	// 1. 현재 플레이어 포인터 및 기본 상태 확인
	AGolfPlayer* CurrentPlayer = GM->PlayerManager->GetPlayers()[GM->CurrentPlayerIndex];

	// 2. 아직 홀컵에 넣지 않은(남은) 플레이어 수 계산
	int32 RemainingPlayersCount = 0;
	for (AGolfPlayer* P : GM->PlayerManager->GetPlayers())
	{
		if (P
			&& P->GetPlayerState() != EPlayerState::Player_HoleOut
			&& !P->bIsPendingDelete                         // ✅ 삭제 대기 제외
			&& !P->PlayerInfo.bIsPendingDelete)             // ✅ PlayerInfo 플래그도 체크
		
		{
			RemainingPlayersCount++;
		}
	}

	OnClickedPopupConfirmDele.BindUObject(this, &UInGameMenuPopup::SetNextPlayer);
	FText Description = NSLOCTEXT("InGameMenuPopup", "GoNextHolePopup", "플레이어를 넘기시겠습니까?");

	// 3. 조건 체크 강화
	// - 플레이어가 없거나 Ready가 아닐 때
	// - 또는 남은 플레이어가 1명 이하(자기 자신뿐)일 때
	bool bIsOnlyOnePlayerLeft = (RemainingPlayersCount <= 1);
	bool bIsCurrentPlayerReady = (CurrentPlayer && CurrentPlayer->GetPlayerState() == EPlayerState::Player_Ready);

	if (!bIsCurrentPlayerReady || bIsOnlyOnePlayerLeft)
	{
		if (bIsOnlyOnePlayerLeft)
		{
			Description = NSLOCTEXT("InGameMenuPopup", "GoNextHolePopup_Single", "넘길 수 있는 다음 플레이어가 없습니다.");
		}
		else
		{
			Description = NSLOCTEXT("InGameMenuPopup", "GoNextHolePopup_Fail", "지금은 플레이어를 넘길 수 없습니다.");
		}

		OnClickedPopupConfirmDele.Unbind();
		UE_LOG(LogTemp, Error, TEXT("Cannot skip turn: PlayerReady=%d, RemainingPlayers=%d"), bIsCurrentPlayerReady, RemainingPlayersCount);
	}

	// 사운드 및 출력
	if (auto* SM = GetGameInstance()->GetSubsystem<USoundManager>())
	{
		SM->Play2D_ById("Voice.Q.UseNextTurn");
	}

	ChangeDescription(Description);
	SetVisibility(ESlateVisibility::Visible);
}

void UInGameMenuPopup::UpdatePopupForEndRound()
{
	if (!GM)
	{
		return;
	}

	OnClickedPopupConfirmDele.BindUObject(this, &UInGameMenuPopup::SetEndRound);
	FText Description = NSLOCTEXT("InGameMenuPopup", "EndRoundPopup", "게임을 종료 하시겠습니까?");

	// ✅ Player_Ready 상태에서만 게임 종료 허용
	AGolfPlayer* CurrentPlayer = GM->PlayerManager->GetPlayers()[GM->CurrentPlayerIndex];
	if (!CurrentPlayer || CurrentPlayer->GetPlayerState() != EPlayerState::Player_Ready)
	{
		Description = NSLOCTEXT("InGameMenuPopup", "EndRoundPopup_Fail", "지금은 게임을 종료할 수 없습니다.");
		OnClickedPopupConfirmDele.Unbind();
		UE_LOG(LogTemp, Warning, TEXT("UpdatePopupForEndRound: Cannot end round, player not in Ready state"));
	}

	ChangeDescription(Description);
	SetVisibility(ESlateVisibility::Visible);
}

void UInGameMenuPopup::UpdatePopupForOK()
{
	if (!GM)
	{
		return;
	}

	OnClickedPopupConfirmDele.BindUObject(this, &UInGameMenuPopup::SetOK);
	FText Description = NSLOCTEXT("InGameMenuPopup", "OKPopup", "OK를 주시겠습니까?");
	ChangeDescription(Description);
	SetVisibility(ESlateVisibility::Visible);
}



void UInGameMenuPopup::UseMulligan()
{
	AGolfPlayer* Player = GM->PlayerManager->GetPlayers()[GM->CurrentPlayerIndex];

	if (!GM || !Player)
	{
		UE_LOG(LogTemp, Error, TEXT("SetNextPlayer() = GM or Player is null"));
		return;
	}
	Player->UseMulligan();
	GM->StrokeMenuWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
}

void UInGameMenuPopup::SetNextHole()
{
	if (!GM)
	{
		UE_LOG(LogTemp, Error, TEXT("SetNextHole() = GM is null"));
		return;
	}

	AGolfPlayerController* PC = Cast<AGolfPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("SetNextHole() = PC is null"));
		return;
	}
	GM->bSetNextHole = true;
	PC->SetNextHole();
	if (GM->StrokeMenuWidgetInstance)
	{
		GM->StrokeMenuWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SetNextHole() = StrokeMenuWidgetInstance is null"));
	}
}

void UInGameMenuPopup::SetNextPlayer()
{
	AGolfPlayer* Player = GM->PlayerManager->GetPlayers()[GM->CurrentPlayerIndex];
	UGolfPlayerManager* PM = GM->PlayerManager;
	if (!GM || !Player)
	{
		UE_LOG(LogTemp, Error, TEXT("SetNextPlayer() = GM or Player is null"));
		return;
	}
	Player->SetPlayerState(EPlayerState::Player_Results);

	if (PM->GetPlayers().Num() > 1)
	{
		PM->SkipCurrentPlayerTurn();
	}

	GM->StrokeMenuWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
}

void UInGameMenuPopup::SetEndRound()
{
	if (!GM)
	{
		UE_LOG(LogTemp, Error, TEXT("SetNextPlayer() = GM or Player is null"));
		return;
	}

	auto* SM = GetGameInstance()->GetSubsystem<USoundManager>();
	if (SM)
	{
		SM->PlayTTS_Interrupt_ById("Voice.EndGame");
	}
	GM->bIsGameMenuEnd = true;

	FTimerHandle TH;
	GetWorld()->GetTimerManager().SetTimer(
		TH,
		[this]() {
			GM->bClickedEndGameButton = true;
			GM->GameInfo.bIsRoundEnd = true;
			GM->GetStateMachine().ChangeState(EGameState::Game_Results);
			GM->StrokeMenuWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);

		}, 2.5f,
		false
			);
}

UFUNCTION() void UInGameMenuPopup::SetOK()
{
	if (GM)
	{
		GM->GetCurrentTurnGolfPlayer()->UseOK();
	}
}

void UInGameMenuPopup::HandleClickedPopupConfirmButton()
{
	if (OnClickedPopupConfirmDele.IsBound())
		OnClickedPopupConfirmDele.ExecuteIfBound();

	SetVisibility(ESlateVisibility::Collapsed);
}

void UInGameMenuPopup::HandleClickedPopupCancelButton()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
