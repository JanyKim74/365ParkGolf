#include "PasswordWidget.h"

#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h" // ✅ QuitGame
#include "ParkDay/Widgets/Menu/IntroWidget.h"
#include "ParkDay/MenuGameMode.h"
#include "ParkDay/Utils/JsonLoader.h"
#include "ParkDay/Utils/UtilLibrary.h"
#include "ParkDay/Widgets/KeyboardWidget.h"
#include "ParkDay/Widgets/Menu/ModeSelectWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogPasswordWidget, Log, All);

void UPasswordWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogPasswordWidget, Error, TEXT("NativeConstruct: World is null. Widget=%s"), *GetNameSafe(this));
		return;
	}

	AGameModeBase* AuthGM = World->GetAuthGameMode();
	GM = Cast<AMenuGameMode>(AuthGM);

	if (!GM)
	{
		UE_LOG(LogPasswordWidget, Warning, TEXT("NativeConstruct: MenuGameMode cast failed. AuthGM=%s Widget=%s"),
			*GetNameSafe(AuthGM), *GetNameSafe(this));
	}

	// ✅ UMG 바인딩 확인 (BindWidget 실패/IsVariable 체크 누락 등)
	if (!Button_Confirm)
	{
		UE_LOG(LogPasswordWidget, Error, TEXT("NativeConstruct: Button_Confirm is null (BindWidget 실패 가능). Widget=%s"), *GetNameSafe(this));
	}
	else
	{
		Button_Confirm->OnPressed.RemoveAll(this);
		Button_Confirm->OnPressed.AddDynamic(this, &UPasswordWidget::HandleOnPressedConfirmButton);
	}

	if (!Button_Cancel)
	{
		UE_LOG(LogPasswordWidget, Error, TEXT("NativeConstruct: Button_Cancel is null (BindWidget 실패 가능). Widget=%s"), *GetNameSafe(this));
	}
	else
	{
		Button_Cancel->OnPressed.RemoveAll(this);
		Button_Cancel->OnPressed.AddDynamic(this, &UPasswordWidget::HandleOnPressedCancelButton);
	}

	if (!EditableTextBox_Password)
	{
		UE_LOG(LogPasswordWidget, Error, TEXT("NativeConstruct: EditableTextBox_Password is null (BindWidget 실패 가능). Widget=%s"), *GetNameSafe(this));
	}
	else
	{
		EditableTextBox_Password->OnTextChanged.RemoveAll(this);
		EditableTextBox_Password->OnTextChanged.AddDynamic(this, &UPasswordWidget::HandleOnChangedPasswordEditableText);

		// ✅ 엔터/커밋 처리 바인딩
		EditableTextBox_Password->OnTextCommitted.RemoveAll(this);
		EditableTextBox_Password->OnTextCommitted.AddDynamic(this, &UPasswordWidget::HandleOnCommittedPasswordEditableText);
	}


	UE_LOG(LogPasswordWidget, Log, TEXT("NativeConstruct done. Widget=%s GM=%s"),
		*GetNameSafe(this), *GetNameSafe(GM));
}

void UPasswordWidget::HandleOnCommittedPasswordEditableText(const FText& Text, ETextCommit::Type CommitMethod)
{
	UE_LOG(LogPasswordWidget, Verbose, TEXT("Password committed. Method=%d Len=%d"),
		(int32)CommitMethod, Text.ToString().Len());

	// ✅ 엔터(또는 OnEnter)일 때만 확인 동작
	if (CommitMethod == ETextCommit::OnEnter)
	{
		// 버튼 눌렀을 때와 완전히 동일한 로직 재사용
		HandleOnPressedConfirmButton();
	}
}


void UPasswordWidget::HandleOnPressedCancelButton()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogPasswordWidget, Error, TEXT("Cancel: World is null. Widget=%s"), *GetNameSafe(this));
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC)
	{
		UE_LOG(LogPasswordWidget, Warning, TEXT("Cancel: PlayerController is null. World=%s"), *GetNameSafe(World));
		// PC가 없어도 QuitGame은 월드만으로 시도 가능하지만, 여기선 안전하게 리턴
		return;
	}

	if (GM->GetCurrentUIState() == EUIState::Intro)
	{
		UE_LOG(LogPasswordWidget, Log, TEXT("Cancel: QuitGame called. PC=%s"), *GetNameSafe(PC));

		UKismetSystemLibrary::QuitGame(
			World,
			PC,
			EQuitPreference::Quit,
			false
		);
	}
	else
	{
		EditableTextBox_Password->SetText(FText::GetEmpty());
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UPasswordWidget::HandleOnPressedConfirmButton()
{
	if (!EditableTextBox_Password)
	{
		UE_LOG(LogPasswordWidget, Error, TEXT("Confirm: EditableTextBox_Password is null."));
		return;
	}

	UUtilLibrary::LockButtonForSeconds(Button_Confirm, GetWorld(), 0.5f);
	UUtilLibrary::LockButtonForSeconds(Button_Cancel, GetWorld(), 0.5f);

	EditableTextBox_Password->SetIsReadOnly(true);

	FTimerHandle TH;
	GetWorld()->GetTimerManager().SetTimer(TH, [this]() 
		{
			if (EditableTextBox_Password)
				EditableTextBox_Password->SetIsReadOnly(false);
		}
	, 0.3f, false);

	const FString InputPassword = EditableTextBox_Password->GetText().ToString();
	const FString Password = GetPassword();

	UE_LOG(LogPasswordWidget, Verbose, TEXT("Confirm pressed. InputLen=%d, PasswordLoaded=%s"),
		InputPassword.Len(),
		Password.IsEmpty() ? TEXT("false/empty") : TEXT("true"));

	// ✅ 비밀번호 로드 실패(빈 문자열)도 구분해서 로그
	if (Password.IsEmpty())
	{
		UE_LOG(LogPasswordWidget, Warning, TEXT("Confirm: Password is empty. adminConfig.json 로드 실패/비밀번호 미설정 가능."));
	}

	if (Password == InputPassword && !Password.IsEmpty())
	{
		FTimerHandle TH2;
		GetWorld()->GetTimerManager().SetTimer(TH2, [this]()
			{
				EditableTextBox_Password->SetText(FText::GetEmpty());
				SetVisibility(ESlateVisibility::Collapsed);

				if (!GM)
				{
					UE_LOG(LogPasswordWidget, Error, TEXT("Confirm: GM is null. Intro stop / state change skipped."));
					return;
				}

				if (GM->GetCurrentUIState() == EUIState::Intro)
				{
					UUserWidget* StateWidget = GM->GetStateWidget(EUIState::Intro);
					if (!StateWidget)
					{
						UE_LOG(LogPasswordWidget, Warning, TEXT("Confirm: GetStateWidget(Intro) returned null. GM=%s"), *GetNameSafe(GM));
					}
					else
					{
						UIntroWidget* Intro = Cast<UIntroWidget>(StateWidget);
						if (!Intro)
						{
							UE_LOG(LogPasswordWidget, Warning, TEXT("Confirm: StateWidget is not IntroWidget. Widget=%s"),
								*GetNameSafe(StateWidget));
						}
						else
						{
							UE_LOG(LogPasswordWidget, Log, TEXT("Confirm: StopIntro()"));
							Intro->StopIntro();
						}
						UE_LOG(LogPasswordWidget, Log, TEXT("Confirm: ChangeUIState -> ModeSelect"));
						GM->ChangeUIState(EUIState::ModeSelect);
					}
				}
				else
				{
					OnConfirmPasswordDele.Broadcast();
				}
			}
		, 0.3f, false);

	}
	else
	{
		UE_LOG(LogPasswordWidget, Warning, TEXT("Confirm: Wrong password. InputLen=%d"), InputPassword.Len());

		EditableTextBox_Password->SetText(FText::GetEmpty());
		EditableTextBox_Password->SetHintText(FText::FromString(TEXT("비밀번호가 틀립니다.")));

		// 필요하면 포커스 다시 주기 (터치/키보드 UX)
		EditableTextBox_Password->SetKeyboardFocus();
	}
}

void UPasswordWidget::HandleOnChangedPasswordEditableText(const FText& InputText)
{
	// 너무 스팸이면 Verbose 권장
	UE_LOG(LogPasswordWidget, VeryVerbose, TEXT("Password text changed. Len=%d"), InputText.ToString().Len());
}

void UPasswordWidget::SetFocusTextBox()
{
	if (!EditableTextBox_Password)
	{
		UE_LOG(LogPasswordWidget, Warning, TEXT("SetFocusTextBox: EditableTextBox_Password is null."));
		return;
	}

	UE_LOG(LogPasswordWidget, Log, TEXT("SetFocusTextBox"));
	EditableTextBox_Password->SetKeyboardFocus(); // ✅ 키보드 포커스가 더 확실
}

FString UPasswordWidget::GetPassword()
{
	FAdminConfig AdminConfig;

	const bool bLoaded = UJsonLoader::LoadAdminConfigFromJson(TEXT("adminConfig.json"), AdminConfig);
	if (!bLoaded)
	{
		UE_LOG(LogPasswordWidget, Warning, TEXT("GetPassword: LoadAdminConfigFromJson failed. file=adminConfig.json"));
		return TEXT("");
	}

	if (AdminConfig.AdminPassword.IsEmpty())
	{
		UE_LOG(LogPasswordWidget, Warning, TEXT("GetPassword: AdminPassword is empty in adminConfig.json"));
	}

	return AdminConfig.AdminPassword;
}


void UPasswordWidget::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	HandleEditBoxEnterFocus();
}

void UPasswordWidget::HandleEditBoxEnterFocus()
{
	FTimerHandle TH;
	GetWorld()->GetTimerManager().SetTimer(TH,
		[this]()
			{
				if (EditableTextBox_Password->HasKeyboardFocus())
				{
					if (GM)
					{
						GM->KeyBoardWidgetInstance->bIsFirstDelete = true;
						GM->KeyBoardWidgetInstance->CurrentText = EditableTextBox_Password->GetText().ToString();
						GM->KeyBoardWidgetInstance->CommittedText = EditableTextBox_Password->GetText().ToString();
						GM->KeyBoardWidgetInstance->UpdateDisplay();
						GM->KeyBoardWidgetInstance->HandleOnClickEnterDele.RemoveAll(this);
						GM->KeyBoardWidgetInstance->HandleOnClickEnterDele.AddDynamic(this, &UPasswordWidget::HandleOnClickKeyboardEnter);
						GM->KeyBoardWidgetInstance->SetVisibility(ESlateVisibility::Visible);
						GM->KeyBoardWidgetInstance->EditableTextBox_Box->SetFocus();
					}
				}
				else
				{
					return;
				}
			}, 
		0.25f, false);
}

void UPasswordWidget::HandleOnClickKeyboardEnter(FText InputText)
{
	EditableTextBox_Password->SetText(InputText);
	GM->KeyBoardWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
}

void UPasswordWidget::SetNextUIState(EUIState InNextUI)
{
	NextUI = InNextUI;
}