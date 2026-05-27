#include "PlayerSelectProfileWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Styling/SlateTypes.h"

#include "PlayerSelectWidget.h"
#include "../../JsonHandler.h"
#include "../KeyboardWidget.h"
#include "../../MenuGameMode.h"

#include "ParkDay/Utils/UtilLibrary.h"
#include "ParkDay/InGameMode.h"
#include "ParkDay/GolfPlayer.h"
#include "ParkDay/Utils/TTSManager.h"
#include "ParkDay/GolfPlayerManager.h"

void UPlayerSelectProfileWidget::NativeConstruct()
{
	Super::NativeConstruct();

	GM = Cast<AMenuGameMode>(GetWorld()->GetAuthGameMode());
	InGM = Cast<AInGameMode>(GetWorld()->GetAuthGameMode());

	if (GM)
	{
		GM->OnEnterPlayerSelectPostDele.AddDynamic(this, &UPlayerSelectProfileWidget::HandleOnEnterPlayerSelectPost);
	}

	// 볼 버튼 원본 스타일 캐시
	BallButtonOriginalStyles.Empty();
	for (UButton* Btn : GetBallButtons())
	{
		BallButtonOriginalStyles.Add(Btn->GetStyle());
	}

	Init();
	Binds();
}

// ────────────────────────────────────────────
// Binds
// ────────────────────────────────────────────
void UPlayerSelectProfileWidget::Binds()
{
	// Before 패널 → "+" 버튼
	Button_AddPlayer->OnPressed.AddDynamic(this, &UPlayerSelectProfileWidget::HandleOnClickedAddPlayerButton);

	// Login 패널
	Button_Login->OnPressed.AddDynamic(this, &UPlayerSelectProfileWidget::HandleOnClickedLoginButton);
	Button_GuestRegist->OnPressed.AddDynamic(this, &UPlayerSelectProfileWidget::HandleOnClickedGuestButton);

	// After 패널
	Button_ModifyNickName->OnPressed.AddDynamic(this, &UPlayerSelectProfileWidget::HandleOnClickedModifyNickNameButton);
	Button_Delete->OnPressed.AddDynamic(this, &UPlayerSelectProfileWidget::HandleOnClickedDeleteButton);

	// Delete 패널
	Button_Delete_Cancel->OnPressed.AddDynamic(this, &UPlayerSelectProfileWidget::HandleOnClickedDeleteCancelButton);
	Button_Delete_Delete->OnPressed.AddDynamic(this, &UPlayerSelectProfileWidget::HandleOnClickedDeleteConfirmButton);

	// 볼 선택
	Button_Ball_0->OnPressed.AddDynamic(this, &UPlayerSelectProfileWidget::HandleOnClickBall_0);
	Button_Ball_1->OnPressed.AddDynamic(this, &UPlayerSelectProfileWidget::HandleOnClickBall_1);
	Button_Ball_2->OnPressed.AddDynamic(this, &UPlayerSelectProfileWidget::HandleOnClickBall_2);
	Button_Ball_3->OnPressed.AddDynamic(this, &UPlayerSelectProfileWidget::HandleOnClickBall_3);
	Button_Ball_4->OnPressed.AddDynamic(this, &UPlayerSelectProfileWidget::HandleOnClickBall_4);
	Button_Ball_5->OnPressed.AddDynamic(this, &UPlayerSelectProfileWidget::HandleOnClickBall_5);
}

// ────────────────────────────────────────────
// Init / SetProfileInfo
// ────────────────────────────────────────────
void UPlayerSelectProfileWidget::Init()
{
	ShowBeforePanel();

	Text_DefaultName->SetText(FText::Format(
		FText::FromString(TEXT("Player {0}")),
		FText::AsNumber(ProfileNumber)
	));

	Text_Distance->SetText(FText::FromString(TEXT("-")));
	Text_Handi->SetText(FText::FromString(TEXT("-")));
	Text_Round->SetText(FText::FromString(TEXT("-")));
	Text_Rank->SetText(FText::FromString(TEXT("-")));
	Text_LastVisit->SetText(FText::FromString(TEXT("-")));

	EditableTextBox_NickName->SetText(FText::GetEmpty());
	EditableTextBox_NickName->SetIsReadOnly(true);

	RefreshBallButtonStyle(0);
}

void UPlayerSelectProfileWidget::HandleOnEnterPlayerSelectPost()
{
	SetProfileInfo();
	PlayerSelectWidget->CheckCanNext();
}

FGameInfo UPlayerSelectProfileWidget::GetGameInfo()
{
	FGameInfo CachedGameInfo;
	UJsonHandler::LoadGameInfoFromJson(CachedGameInfo, FPaths::ProjectSavedDir() + TEXT("GameData.json"));
	return CachedGameInfo;
}

void UPlayerSelectProfileWidget::SetGameInfo(const FGameInfo& GameInfo)
{
	UJsonHandler::SaveGameInfoToJson(GameInfo, FPaths::ProjectSavedDir() + TEXT("GameData.json"));
}

void UPlayerSelectProfileWidget::SetProfileInfo()
{
	FGameInfo CachedGameInfo;
	if (GM)        CachedGameInfo = PlayerSelectWidget->GameInfo;
	else if (InGM) CachedGameInfo = InGM->GameInfo;

	for (const FPlayerInfo& Player : CachedGameInfo.Players)
	{
		if (Player.SlotIndex == ProfileNumber && !Player.bIsPendingDelete)
		{
			EditableTextBox_NickName->SetText(FText::FromString(Player.NickName));
			ShowAfterPanel();
			RefreshBallButtonStyle(Player.BallIndex);
			return;
		}
	}

	// 해당 슬롯 플레이어 없으면 Before 패널로
	ShowBeforePanel();
}

// ────────────────────────────────────────────
// 패널 표시
// ────────────────────────────────────────────
void UPlayerSelectProfileWidget::ShowBeforePanel()
{
	CanvasPanel_Before->SetVisibility(ESlateVisibility::Visible);
	CanvasPanel_Login->SetVisibility(ESlateVisibility::Collapsed);
	CanvasPanel_After->SetVisibility(ESlateVisibility::Collapsed);
	CanvasPanel_Delete->SetVisibility(ESlateVisibility::Collapsed);
}

void UPlayerSelectProfileWidget::ShowLoginPanel()
{
	CanvasPanel_Before->SetVisibility(ESlateVisibility::Collapsed);
	CanvasPanel_Login->SetVisibility(ESlateVisibility::Visible);
	CanvasPanel_After->SetVisibility(ESlateVisibility::Collapsed);
	CanvasPanel_Delete->SetVisibility(ESlateVisibility::Collapsed);
}

void UPlayerSelectProfileWidget::ShowAfterPanel()
{
	CanvasPanel_Before->SetVisibility(ESlateVisibility::Collapsed);
	CanvasPanel_Login->SetVisibility(ESlateVisibility::Collapsed);
	CanvasPanel_After->SetVisibility(ESlateVisibility::Visible);
	CanvasPanel_Delete->SetVisibility(ESlateVisibility::Collapsed);
}

void UPlayerSelectProfileWidget::ShowDeletePanel()
{
	// Delete 패널은 After 위에 오버레이
	CanvasPanel_Delete->SetVisibility(ESlateVisibility::Visible);
}

// ────────────────────────────────────────────
// "+" 버튼 → Login 패널
// ────────────────────────────────────────────
void UPlayerSelectProfileWidget::HandleOnClickedAddPlayerButton()
{
	LockButtons(0.2f);
	ShowLoginPanel();
}

// ────────────────────────────────────────────
// 로그인 버튼 (플레이스홀더)
// ────────────────────────────────────────────
void UPlayerSelectProfileWidget::HandleOnClickedLoginButton()
{
	LockButtons(0.2f);
	// TODO: 로그인 처리
	UE_LOG(LogTemp, Log, TEXT("PlayerSelectProfileWidget: 로그인 버튼 클릭 (SlotIndex=%d)"), ProfileNumber);
}

// ────────────────────────────────────────────
// 게스트 버튼 → 키보드 입력
// ────────────────────────────────────────────
void UPlayerSelectProfileWidget::HandleOnClickedGuestButton()
{
	LockButtons(0.2f);

	FTimerHandle TF;
	GetWorld()->GetTimerManager().SetTimer(TF,
		FTimerDelegate::CreateLambda([this]()
			{
				FString DefaultNickName = FText::Format(
					FText::FromString(TEXT("Player {0}")),
					FText::AsNumber(ProfileNumber)).ToString();

				PlayerSelectWidget->KeyBoardWidgetInstance->bIsFirstDelete = true;
				PlayerSelectWidget->KeyBoardWidgetInstance->CurrentText = DefaultNickName;
				PlayerSelectWidget->KeyBoardWidgetInstance->CommittedText = DefaultNickName;
				PlayerSelectWidget->KeyBoardWidgetInstance->UpdateDisplay();
				PlayerSelectWidget->KeyBoardWidgetInstance->HandleOnClickEnterDele.AddDynamic(
					this, &UPlayerSelectProfileWidget::HandleOnClickedKeyBoardEnter);
				PlayerSelectWidget->KeyBoardWidgetInstance->SetVisibility(ESlateVisibility::Visible);
				PlayerSelectWidget->KeyBoardWidgetInstance->EditableTextBox_Box->SetFocus();
			}), 0.2f, false);
}

// 키보드 엔터 → 게스트 등록 완료 후 After 패널
void UPlayerSelectProfileWidget::HandleOnClickedKeyBoardEnter(FText InputText)
{
	LockButtons(0.2f);
	ShowAfterPanel();

	if (!InGM) RegistGuest(InputText);
	else       InGame_RegistGuest(InputText);

	PlayerSelectWidget->KeyBoardWidgetInstance->HandleOnClickEnterDele.RemoveAll(this);

	FString EndAnnouncement = FString::Printf(TEXT("%s님 환영합니다!"), *InputText.ToString());
	if (GM)        GM->Speak(EndAnnouncement);
	else if (InGM) InGM->Speak(EndAnnouncement);

	FTimerHandle TF;
	GetWorld()->GetTimerManager().SetTimer(TF,
		FTimerDelegate::CreateLambda([this]()
			{
				PlayerSelectWidget->KeyBoardWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
			}), 0.1f, false);
}

void UPlayerSelectProfileWidget::RegistGuest(FText NickName)
{
	EditableTextBox_NickName->SetText(NickName);

	FPlayerInfo NewPlayerInfo;
	NewPlayerInfo.NickName = NickName.ToString();
	NewPlayerInfo.SlotIndex = ProfileNumber;
	NewPlayerInfo.BallIndex = 0;
	if (InGM) NewPlayerInfo.bIsHoleout = true;

	FGameInfo CachedGameInfo = GetGameInfo();
	CachedGameInfo.Players.Add(NewPlayerInfo);

	if (PlayerSelectWidget)
	{
		UJsonHandler::SaveGameInfoToJson(CachedGameInfo, FPaths::ProjectSavedDir() + TEXT("GameData.json"));
		PlayerSelectWidget->CheckCanNext();
	}

	RefreshBallButtonStyle(0);
}

void UPlayerSelectProfileWidget::InGame_RegistGuest(FText NickName)
{
	EditableTextBox_NickName->SetText(NickName);

	FPlayerInfo NewPlayerInfo;
	NewPlayerInfo.NickName = NickName.ToString();
	NewPlayerInfo.SlotIndex = ProfileNumber;
	NewPlayerInfo.BallIndex = 0;

	FGameInfo CachedGameInfo = GetGameInfo();
	CachedGameInfo.Players.Add(NewPlayerInfo);

	OnModifyPlayersDele.Broadcast(NewPlayerInfo);
	RefreshBallButtonStyle(0);
}

// ────────────────────────────────────────────
// 닉네임 수정
// ────────────────────────────────────────────
void UPlayerSelectProfileWidget::HandleOnClickedModifyNickNameButton()
{
	LockButtons(0.2f);

	FTimerHandle TF;
	GetWorld()->GetTimerManager().SetTimer(TF,
		FTimerDelegate::CreateLambda([this]()
			{
				PlayerSelectWidget->KeyBoardWidgetInstance->bIsFirstDelete = true;

				FGameInfo CachedGameInfo = GetGameInfo();
				FString DefaultNickName;
				for (const FPlayerInfo& Player : CachedGameInfo.Players)
				{
					if (Player.SlotIndex == ProfileNumber)
					{
						DefaultNickName = Player.NickName;
						break;
					}
				}

				PlayerSelectWidget->KeyBoardWidgetInstance->CurrentText = DefaultNickName;
				PlayerSelectWidget->KeyBoardWidgetInstance->CommittedText = DefaultNickName;
				PlayerSelectWidget->KeyBoardWidgetInstance->UpdateDisplay();
				PlayerSelectWidget->KeyBoardWidgetInstance->HandleOnClickEnterDele.AddDynamic(
					this, &UPlayerSelectProfileWidget::HandleOnClickedKeyBoardEnterForNickNameModify);
				PlayerSelectWidget->KeyBoardWidgetInstance->SetVisibility(ESlateVisibility::Visible);
				PlayerSelectWidget->KeyBoardWidgetInstance->EditableTextBox_Box->SetFocus();
			}), 0.2f, false);
}

void UPlayerSelectProfileWidget::HandleOnClickedKeyBoardEnterForNickNameModify(FText InputText)
{
	LockButtons(0.2f);
	EditableTextBox_NickName->SetText(InputText);

	FGameInfo CachedGameInfo = GetGameInfo();
	FPlayerInfo FindPlayerInfo;
	for (int32 i = 0; i < CachedGameInfo.Players.Num(); i++)
	{
		if (CachedGameInfo.Players[i].SlotIndex == ProfileNumber)
		{
			FindPlayerInfo = CachedGameInfo.Players[i];
			FindPlayerInfo.NickName = InputText.ToString();
			CachedGameInfo.Players[i] = FindPlayerInfo;
			break;
		}
	}

	PlayerSelectWidget->KeyBoardWidgetInstance->HandleOnClickEnterDele.RemoveAll(this);
	UJsonHandler::SaveGameInfoToJson(CachedGameInfo, FPaths::ProjectSavedDir() + TEXT("GameData.json"));
	PlayerSelectWidget->KeyBoardWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
	OnModifyPlayersDele.Broadcast(FindPlayerInfo);
}

// ────────────────────────────────────────────
// 삭제
// ────────────────────────────────────────────
void UPlayerSelectProfileWidget::HandleOnClickedDeleteButton()
{
	LockButtons(0.2f);
	ShowDeletePanel();
}

void UPlayerSelectProfileWidget::HandleOnClickedDeleteCancelButton()
{
	LockButtons(0.2f);
	CanvasPanel_Delete->SetVisibility(ESlateVisibility::Collapsed);
}

void UPlayerSelectProfileWidget::HandleOnClickedDeleteConfirmButton()
{
	Init(); // Before 패널로 초기화
	LockButtons(0.5f);

	if (GM)
	{
		FGameInfo CachedGameInfo = GetGameInfo();
		for (int32 i = 0; i < CachedGameInfo.Players.Num(); i++)
		{
			if (CachedGameInfo.Players[i].SlotIndex == ProfileNumber)
			{
				CachedGameInfo.Players.RemoveAt(i);
				break;
			}
		}
		UJsonHandler::SaveGameInfoToJson(CachedGameInfo, FPaths::ProjectSavedDir() + TEXT("GameData.json"));
		PlayerSelectWidget->CheckCanNext();
	}
	else if (InGM)
	{
		bool bIsCurrentTurnPlayer = false;
		AGolfPlayer* CurrentTurnPlayer = InGM->PlayerManager->GetPlayers().IsValidIndex(InGM->CurrentPlayerIndex)
			? InGM->PlayerManager->GetPlayers()[InGM->CurrentPlayerIndex] : nullptr;

		if (CurrentTurnPlayer && CurrentTurnPlayer->PlayerInfo.SlotIndex == ProfileNumber)
			bIsCurrentTurnPlayer = true;

		for (int32 i = 0; i < InGM->PlayerManager->GetPlayers().Num(); i++)
		{
			if (InGM->PlayerManager->GetPlayers()[i]->PlayerInfo.SlotIndex == ProfileNumber)
			{
				InGM->PlayerManager->GetPlayers()[i]->PlayerInfo.bIsPendingDelete = true;
				OnDeletePlayersDele.Broadcast(InGM->PlayerManager->GetPlayers()[i]->PlayerInfo);
			}
		}
		InGM->SyncPlayerInfosToGameInfo();
		InGM->SaveGameInfoToJSON();

		if (bIsCurrentTurnPlayer)
			InGM->PlayerManager->AdvanceTurn();
	}
}

// ────────────────────────────────────────────
// 볼 선택
// ────────────────────────────────────────────
TArray<UButton*> UPlayerSelectProfileWidget::GetBallButtons() const
{
	return { Button_Ball_0, Button_Ball_1, Button_Ball_2,
			 Button_Ball_3, Button_Ball_4, Button_Ball_5 };
}

void UPlayerSelectProfileWidget::HandleOnClickBall_0() { SelectBall(0); }
void UPlayerSelectProfileWidget::HandleOnClickBall_1() { SelectBall(1); }
void UPlayerSelectProfileWidget::HandleOnClickBall_2() { SelectBall(2); }
void UPlayerSelectProfileWidget::HandleOnClickBall_3() { SelectBall(3); }
void UPlayerSelectProfileWidget::HandleOnClickBall_4() { SelectBall(4); }
void UPlayerSelectProfileWidget::HandleOnClickBall_5() { SelectBall(5); }

void UPlayerSelectProfileWidget::SelectBall(int32 BallIdx)
{
	RefreshBallButtonStyle(BallIdx);

	FGameInfo CachedGameInfo = GetGameInfo();
	for (FPlayerInfo& Player : CachedGameInfo.Players)
	{
		if (Player.SlotIndex == ProfileNumber)
		{
			Player.BallIndex = BallIdx;
			break;
		}
	}
	UJsonHandler::SaveGameInfoToJson(CachedGameInfo, FPaths::ProjectSavedDir() + TEXT("GameData.json"));

	for (const FPlayerInfo& Player : CachedGameInfo.Players)
	{
		if (Player.SlotIndex == ProfileNumber)
		{
			OnModifyPlayersDele.Broadcast(Player);
			break;
		}
	}
}

void UPlayerSelectProfileWidget::RefreshBallButtonStyle(int32 SelectedBallIdx)
{
	TArray<UButton*> Buttons = GetBallButtons();
	for (int32 i = 0; i < Buttons.Num(); i++)
	{
		if (!Buttons[i]) continue;

		if (i == SelectedBallIdx)
		{
			FButtonStyle Style = Buttons[i]->GetStyle();
			FSlateBrush Pressed = Style.Pressed;
			Style.Normal = Pressed;
			Style.Hovered = Pressed;
			Buttons[i]->SetStyle(Style);
		}
		else
		{
			if (BallButtonOriginalStyles.IsValidIndex(i))
				Buttons[i]->SetStyle(BallButtonOriginalStyles[i]);
		}
	}
}

// ────────────────────────────────────────────
// LockButtons
// ────────────────────────────────────────────
void UPlayerSelectProfileWidget::LockButtons(float Duration)
{
	auto Lock = [&](UButton* Btn) { if (Btn) UUtilLibrary::LockButtonForSeconds(Btn, GetWorld(), Duration); };

	Lock(Button_AddPlayer);
	Lock(Button_Login);
	Lock(Button_GuestRegist);
	Lock(Button_ModifyNickName);
	Lock(Button_Delete);
	Lock(Button_Delete_Cancel);
	Lock(Button_Delete_Delete);
	Lock(Button_Ball_0);
	Lock(Button_Ball_1);
	Lock(Button_Ball_2);
	Lock(Button_Ball_3);
	Lock(Button_Ball_4);
	Lock(Button_Ball_5);
}