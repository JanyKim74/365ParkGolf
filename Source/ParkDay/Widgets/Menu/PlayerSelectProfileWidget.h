#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateTypes.h"
#include "../../GolfDataStructures.h"
#include "PlayerSelectProfileWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnModifyPlayers, FPlayerInfo, PlayerInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeletePlayers, FPlayerInfo, PlayerInfo);

class UTextBlock;
class UCanvasPanel;
class UEditableTextBox;
class UButton;
class UPlayerSelectWidget;
class AMenuGameMode;
class AInGameMode;

UCLASS()
class PARKDAY_API UPlayerSelectProfileWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	void Init();

	UPROPERTY()
	FOnModifyPlayers OnModifyPlayersDele;
	UPROPERTY()
	FOnDeletePlayers OnDeletePlayersDele;

public:
	void SetProfileInfo();
	void Binds();

	UFUNCTION() void HandleOnClickedGuestButton();
	UFUNCTION() void HandleOnClickedLoginButton();
	UFUNCTION() void HandleOnClickedDeleteButton();
	UFUNCTION() void HandleOnClickedDeleteConfirmButton();
	UFUNCTION() void HandleOnClickedDeleteCancelButton();
	UFUNCTION() void HandleOnClickedModifyNickNameButton();
	UFUNCTION() void HandleOnClickedKeyBoardEnter(FText InputText);
	UFUNCTION() void HandleOnClickedKeyBoardEnterForNickNameModify(FText InputText);

	void RegistGuest(FText NickName);
	void InGame_RegistGuest(FText NickName);
	void LockButtons(float Duration);

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Profile")
	int32 ProfileNumber;

public:
	// ─────────────────────────────────
	// Before 패널: 빈 슬롯 "+" 버튼만
	// ─────────────────────────────────
	UPROPERTY(meta = (BindWidget)) UCanvasPanel* CanvasPanel_Before;
	UPROPERTY(meta = (BindWidget)) UButton* Button_AddPlayer;   // "+" 버튼
	UPROPERTY(meta = (BindWidget)) UTextBlock* Text_DefaultName;

	UFUNCTION() void HandleOnClickedAddPlayerButton();  // "+" → Login 패널 표시
	UFUNCTION() void ShowBeforePanel();
	UFUNCTION() void HandleOnEnterPlayerSelectPost();

public:
	// ─────────────────────────────────
	// Login 패널: 로그인 / 게스트 버튼
	// "+" 클릭 시 표시
	// ─────────────────────────────────
	UPROPERTY(meta = (BindWidget)) UCanvasPanel* CanvasPanel_Login;
	UPROPERTY(meta = (BindWidget)) UButton* Button_Login;       // 로그인
	UPROPERTY(meta = (BindWidget)) UButton* Button_GuestRegist; // 게스트

	UFUNCTION() void ShowLoginPanel();

public:
	// ─────────────────────────────────
	// After 패널: 등록된 플레이어 정보
	// 게스트 등록 완료 후 표시
	// ─────────────────────────────────
	UPROPERTY(meta = (BindWidget)) UCanvasPanel* CanvasPanel_After;
	UPROPERTY(meta = (BindWidget)) UEditableTextBox* EditableTextBox_NickName;
	UPROPERTY(meta = (BindWidget)) UButton* Button_ModifyNickName;
	UPROPERTY(meta = (BindWidget)) UButton* Button_Delete;
	UPROPERTY(meta = (BindWidget)) UTextBlock* Text_Handi;
	UPROPERTY(meta = (BindWidget)) UTextBlock* Text_Distance;
	UPROPERTY(meta = (BindWidget)) UTextBlock* Text_Round;
	UPROPERTY(meta = (BindWidget)) UTextBlock* Text_Rank;
	UPROPERTY(meta = (BindWidget)) UTextBlock* Text_LastVisit;

	// 볼 선택 버튼 6개 (0=흰색 1=노랑 2=빨강 3=파랑 4=주황 5=검정)
	UPROPERTY(meta = (BindWidget)) UButton* Button_Ball_0;
	UPROPERTY(meta = (BindWidget)) UButton* Button_Ball_1;
	UPROPERTY(meta = (BindWidget)) UButton* Button_Ball_2;
	UPROPERTY(meta = (BindWidget)) UButton* Button_Ball_3;
	UPROPERTY(meta = (BindWidget)) UButton* Button_Ball_4;
	UPROPERTY(meta = (BindWidget)) UButton* Button_Ball_5;

	UFUNCTION() void HandleOnClickBall_0();
	UFUNCTION() void HandleOnClickBall_1();
	UFUNCTION() void HandleOnClickBall_2();
	UFUNCTION() void HandleOnClickBall_3();
	UFUNCTION() void HandleOnClickBall_4();
	UFUNCTION() void HandleOnClickBall_5();

	void SelectBall(int32 BallIdx);
	void RefreshBallButtonStyle(int32 SelectedBallIdx);

	UFUNCTION() void ShowAfterPanel();

public:
	// ─────────────────────────────────
	// Delete 패널
	// ─────────────────────────────────
	UPROPERTY(meta = (BindWidget)) UCanvasPanel* CanvasPanel_Delete;
	UPROPERTY(meta = (BindWidget)) UButton* Button_Delete_Delete;
	UPROPERTY(meta = (BindWidget)) UButton* Button_Delete_Cancel;

	UFUNCTION() void ShowDeletePanel();

	UPlayerSelectWidget* PlayerSelectWidget;

private:
	FGameInfo GetGameInfo();
	void SetGameInfo(const FGameInfo& GameInfo);

	TArray<UButton*> GetBallButtons() const;
	TArray<FButtonStyle> BallButtonOriginalStyles;

	AMenuGameMode* GM;
	AInGameMode* InGM;
};