#include "KeyboardWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Blueprint/WidgetTree.h"
#include "ParkDay/Utils/UtilLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/Application/SlateApplication.h"

// ============================================================================
// Hangul Automata Constants & Helpers
// ============================================================================

const int32 HANGUL_BASE = 0xAC00;

// Jamo Tables (Unicode Escaped)
const TArray<FString> Chosungs = {
	TEXT("\u3131"), TEXT("\u3132"), TEXT("\u3134"), TEXT("\u3137"), TEXT("\u3138"),
	TEXT("\u3139"), TEXT("\u3141"), TEXT("\u3142"), TEXT("\u3143"), TEXT("\u3145"),
	TEXT("\u3146"), TEXT("\u3147"), TEXT("\u3148"), TEXT("\u3149"), TEXT("\u314A"),
	TEXT("\u314B"), TEXT("\u314C"), TEXT("\u314D"), TEXT("\u314E")
}; // ㄱ ㄲ ㄴ ㄷ ㄸ ㄹ ㅁ ㅂ ㅃ ㅅ ㅆ ㅇ ㅈ ㅉ ㅊ ㅋ ㅌ ㅍ ㅎ

const TArray<FString> Jungsungs = {
	TEXT("\u314F"), TEXT("\u3150"), TEXT("\u3151"), TEXT("\u3152"), TEXT("\u3153"),
	TEXT("\u3154"), TEXT("\u3155"), TEXT("\u3156"), TEXT("\u3157"), TEXT("\u3158"),
	TEXT("\u3159"), TEXT("\u315A"), TEXT("\u315B"), TEXT("\u315C"), TEXT("\u315D"),
	TEXT("\u315E"), TEXT("\u315F"), TEXT("\u3160"), TEXT("\u3161"), TEXT("\u3162"),
	TEXT("\u3163")
}; // ㅏ ㅐ ㅑ ㅒ ㅓ ㅔ ㅕ ㅖ ㅗ ㅘ ㅙ ㅚ ㅛ ㅜ ㅝ ㅞ ㅟ ㅠ ㅡ ㅢ ㅣ

const TArray<FString> Jongsungs = {
	TEXT(""), TEXT("\u3131"), TEXT("\u3132"), TEXT("\u3133"), TEXT("\u3134"),
	TEXT("\u3135"), TEXT("\u3136"), TEXT("\u3137"), TEXT("\u3139"), TEXT("\u313A"),
	TEXT("\u313B"), TEXT("\u313C"), TEXT("\u313D"), TEXT("\u313E"), TEXT("\u313F"),
	TEXT("\u3140"), TEXT("\u3141"), TEXT("\u3142"), TEXT("\u3144"), TEXT("\u3145"),
	TEXT("\u3146"), TEXT("\u3147"), TEXT("\u3148"), TEXT("\u314A"), TEXT("\u314B"),
	TEXT("\u314C"), TEXT("\u314D"), TEXT("\u314E")
}; // (None) ㄱ ㄲ ㄳ ㄴ ㄵ ㄶ ㄷ ㄹ ㄺ ㄻ ㄼ ㄽ ㄾ ㄿ ㅀ ㅁ ㅂ ㅄ ㅅ ㅆ ㅇ ㅈ ㅊ ㅋ ㅌ ㅍ ㅎ

static int32 GetIndex(const TArray<FString>& Array, const FString& Value)
{
	return Array.Find(Value);
}


static bool TryCombineJung(TCHAR Prev, TCHAR Curr, TCHAR& OutCombined)
{
    // ㅗ (U+3157) 계열
    if (Prev == TEXT('\u3157'))
    {
        if (Curr == TEXT('\u314F')) { OutCombined = TEXT('\u3158'); return true; } // ㅗ + ㅏ = ㅘ
        if (Curr == TEXT('\u3150')) { OutCombined = TEXT('\u3159'); return true; } // ㅗ + ㅐ = ㅙ
        if (Curr == TEXT('\u3163')) { OutCombined = TEXT('\u315A'); return true; } // ㅗ + ㅣ = ㅚ
    }

    // ㅜ (U+315C) 계열
    if (Prev == TEXT('\u315C'))
    {
        if (Curr == TEXT('\u3153')) { OutCombined = TEXT('\u315D'); return true; } // ㅜ + ㅓ = ㅝ
        if (Curr == TEXT('\u3154')) { OutCombined = TEXT('\u315E'); return true; } // ㅜ + ㅔ = ㅞ
        if (Curr == TEXT('\u3163')) { OutCombined = TEXT('\u315F'); return true; } // ㅜ + ㅣ = ㅟ
    }

    // ㅡ (U+3161) 계열
    if (Prev == TEXT('\u3161'))
    {
        if (Curr == TEXT('\u3163')) { OutCombined = TEXT('\u3162'); return true; } // ㅡ + ㅣ = ㅢ
    }

    return false;
}

// 복합 받침 (ㄳ, ㄵ, ㄶ, ㄺ, ㄻ, ㄼ, ㄽ, ㄾ, ㄿ, ㅀ, ㅄ) 조합
static bool TryCombineJong(TCHAR Prev, TCHAR Curr, TCHAR& OutCombined)
{
	if (Prev == TEXT('\u3131') && Curr == TEXT('\u3145')) { OutCombined = TEXT('\u3133'); return true; } // ㄱ + ㅅ = ㄳ
	if (Prev == TEXT('\u3134') && Curr == TEXT('\u3148')) { OutCombined = TEXT('\u3135'); return true; } // ㄴ + ㅈ = ㄵ
	if (Prev == TEXT('\u3134') && Curr == TEXT('\u314E')) { OutCombined = TEXT('\u3136'); return true; } // ㄴ + ㅎ = ㄶ
	if (Prev == TEXT('\u3139') && Curr == TEXT('\u3131')) { OutCombined = TEXT('\u313A'); return true; } // ㄹ + ㄱ = ㄺ
	if (Prev == TEXT('\u3139') && Curr == TEXT('\u3141')) { OutCombined = TEXT('\u313B'); return true; } // ㄹ + ㅁ = ㄻ
	if (Prev == TEXT('\u3139') && Curr == TEXT('\u3142')) { OutCombined = TEXT('\u313C'); return true; } // ㄹ + ㅂ = ㄼ
	if (Prev == TEXT('\u3139') && Curr == TEXT('\u3145')) { OutCombined = TEXT('\u313D'); return true; } // ㄹ + ㅅ = ㄽ
	if (Prev == TEXT('\u3139') && Curr == TEXT('\u314C')) { OutCombined = TEXT('\u313E'); return true; } // ㄹ + ㅌ = ㄾ
	if (Prev == TEXT('\u3139') && Curr == TEXT('\u314D')) { OutCombined = TEXT('\u313F'); return true; } // ㄹ + ㅍ = ㄿ
	if (Prev == TEXT('\u3139') && Curr == TEXT('\u314E')) { OutCombined = TEXT('\u3140'); return true; } // ㄹ + ㅎ = ㅀ
	if (Prev == TEXT('\u3142') && Curr == TEXT('\u3145')) { OutCombined = TEXT('\u3144'); return true; } // ㅂ + ㅅ = ㅄ

	return false;
}

// 복합 받침 분해
//  ex) ㄺ → (ㄹ, ㄱ), ㅄ → (ㅂ, ㅅ)
//  첫 번째는 이전 음절 받침, 두 번째는 다음 음절 초성으로 사용
static void SplitCompositeJong(int32 JongIdx, int32& OutFirst, int32& OutSecond)
{
	switch (JongIdx)
	{
	case 3:   // ㄳ
		OutFirst = 1;  OutSecond = 19; break; // ㄱ, ㅅ
	case 5:   // ㄵ
		OutFirst = 4;  OutSecond = 22; break; // ㄴ, ㅈ
	case 6:   // ㄶ
		OutFirst = 4;  OutSecond = 27; break; // ㄴ, ㅎ
	case 9:   // ㄺ
		OutFirst = 8;  OutSecond = 1;  break; // ㄹ, ㄱ
	case 10:  // ㄻ
		OutFirst = 8;  OutSecond = 16; break; // ㄹ, ㅁ
	case 11:  // ㄼ
		OutFirst = 8;  OutSecond = 17; break; // ㄹ, ㅂ
	case 12:  // ㄽ
		OutFirst = 8;  OutSecond = 19; break; // ㄹ, ㅅ
	case 13:  // ㄾ
		OutFirst = 8;  OutSecond = 25; break; // ㄹ, ㅌ
	case 14:  // ㄿ
		OutFirst = 8;  OutSecond = 26; break; // ㄹ, ㅍ
	case 15:  // ㅀ
		OutFirst = 8;  OutSecond = 27; break; // ㄹ, ㅎ
	case 18:  // ㅄ
		OutFirst = 17; OutSecond = 19; break; // ㅂ, ㅅ

	default:
		// 단일 받침: 통째로 다음 음절 초성으로 넘김
		OutFirst = 0;         // 이전 음절은 받침 없음
		OutSecond = JongIdx;   // 전체가 다음 음절 초성
		break;
	}
}

// ============================================================================
// UKeyClickHandler
// ============================================================================

void UKeyClickHandler::OnClick()
{
	if (!ParentWidget)
	{
		return;
	}

	ParentWidget->HandleKeyPress(KeyCode);
}

// ============================================================================
// UKeyboardWidget
// ============================================================================

UKeyboardWidget::UKeyboardWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CurrentMode = EKeyboardMode::Korean;
	CurrentText = TEXT("");
	CommittedText = TEXT("");
	HangulBuffer = TEXT("");
	bKoreanShift = false;
}

void UKeyboardWidget::SetSelectAll(bool bIsSelectAll)
{
	if (EditableTextBox_Box)
	{
		EditableTextBox_Box->SetSelectAllTextWhenFocused(bIsSelectAll);
	}
}

void UKeyboardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	KeyConfigs.Empty();
	ClickHandlers.Empty();

	// Row 1
	SetupKey("WBP_Key_1", TEXT("\u3142"), TEXT("\u3142"), "q", "Q", "1"); // ㅂ
	SetupKey("WBP_Key_2", TEXT("\u3148"), TEXT("\u3148"), "w", "W", "2"); // ㅈ
	SetupKey("WBP_Key_3", TEXT("\u3137"), TEXT("\u3137"), "e", "E", "3"); // ㄷ
	SetupKey("WBP_Key_4", TEXT("\u3131"), TEXT("\u3131"), "r", "R", "4"); // ㄱ
	SetupKey("WBP_Key_5", TEXT("\u3145"), TEXT("\u3145"), "t", "T", "5"); // ㅅ
	SetupKey("WBP_Key_6", TEXT("\u315B"), TEXT("\u315B"), "y", "Y", "6"); // ㅛ
	SetupKey("WBP_Key_7", TEXT("\u3155"), TEXT("\u3155"), "u", "U", "7"); // ㅕ
	SetupKey("WBP_Key_8", TEXT("\u3151"), TEXT("\u3151"), "i", "I", "8"); // ㅑ
	SetupKey("WBP_Key_9", TEXT("\u3150"), TEXT("\u3150"), "o", "O", "9"); // ㅐ
	SetupKey("WBP_Key_10", TEXT("\u3154"), TEXT("\u3154"), "p", "P", "0"); // ㅔ

	// Row 2
	SetupKey("WBP_Key_11", TEXT("\u3141"), TEXT("\u3141"), "a", "A", "@"); // ㅁ
	SetupKey("WBP_Key_12", TEXT("\u3134"), TEXT("\u3134"), "s", "S", "#"); // ㄴ
	SetupKey("WBP_Key_13", TEXT("\u3147"), TEXT("\u3147"), "d", "D", "$"); // ㅇ
	SetupKey("WBP_Key_14", TEXT("\u3139"), TEXT("\u3139"), "f", "F", "%"); // ㄹ
	SetupKey("WBP_Key_15", TEXT("\u314E"), TEXT("\u314E"), "g", "G", "&"); // ㅎ
	SetupKey("WBP_Key_16", TEXT("\u3157"), TEXT("\u3157"), "h", "H", "-"); // ㅗ
	SetupKey("WBP_Key_17", TEXT("\u3153"), TEXT("\u3153"), "j", "J", "+"); // ㅓ
	SetupKey("WBP_Key_18", TEXT("\u314F"), TEXT("\u314F"), "k", "K", "("); // ㅏ
	SetupKey("WBP_Key_19", TEXT("\u3163"), TEXT("\u3163"), "l", "L", ")"); // ㅣ
	SetupKey("WBP_Key_20", ";", ";", ";", ":", "*");

	// Row 3
	SetupKey("WBP_Key_21", TEXT("\u314B"), TEXT("\u314B"), "z", "Z", "!");  // ㅋ
	SetupKey("WBP_Key_22", TEXT("\u314C"), TEXT("\u314C"), "x", "X", "\""); // ㅌ
	SetupKey("WBP_Key_23", TEXT("\u314A"), TEXT("\u314A"), "c", "C", "'");  // ㅊ
	SetupKey("WBP_Key_24", TEXT("\u314D"), TEXT("\u314D"), "v", "V", ":");  // ㅍ
	SetupKey("WBP_Key_25", TEXT("\u3160"), TEXT("\u3160"), "b", "B", ";");  // ㅠ
	SetupKey("WBP_Key_26", TEXT("\u315C"), TEXT("\u315C"), "n", "N", "/");  // ㅜ
	SetupKey("WBP_Key_27", TEXT("\u3161"), TEXT("\u3161"), "m", "M", "?");  // ㅡ

	// Special Keys
	SetupSpecialKey("WBP_FKey_1", "SHIFT", "Shift");
	SetupSpecialKey("WBP_FKey_2", "BACKSPACE", "<-");
	SetupSpecialKey("WBP_FKey", "TOGGLE_LANG", TEXT("\ud55c/\uc601")); // 한/영
	SetupSpecialKey("WBP_FKey_3", "MODE_NUM", ".?123");
	SetupSpecialKey("WBP_FKey_4", "SPACE", "Space");
	SetupSpecialKey("WBP_FKey_5", "ENTER", "Enter");

	// Output TextBlock 찾기
	OutputTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("TextBlock_0")));
	if (!OutputTextBlock)
	{
		WidgetTree->ForEachWidget([&](UWidget* Widget) {
			if (!OutputTextBlock && Widget->GetName() == TEXT("TextBlock_0"))
			{
				OutputTextBlock = Cast<UTextBlock>(Widget);
			}
			});
	}

	SetKeyboardMode(EKeyboardMode::Korean);
	UpdateDisplay();
}

// ----------------------------------------------------------------------------
// 키 세팅 / 찾기
// ----------------------------------------------------------------------------

void UKeyboardWidget::SetupKey(const FString& WidgetName, const FString& KeyCode,
	const FString& Kor, const FString& Eng,
	const FString& Shift, const FString& Num)
{
	UWidget* FoundWidget = FindObject<UWidget>(this, *WidgetName);
	if (!FoundWidget)
	{
		FoundWidget = WidgetTree->FindWidget(*WidgetName);
	}

	if (!FoundWidget) return;

	UButton* Btn = FindButtonInWidget(FoundWidget);
	UTextBlock* Txt = FindTextBlockInWidget(FoundWidget);

	if (Btn)
	{
		UKeyClickHandler* Handler = NewObject<UKeyClickHandler>(this);
		Handler->ParentWidget = this;
		Handler->KeyCode = KeyCode;

		Btn->OnClicked.RemoveAll(this);
		Btn->OnClicked.AddDynamic(Handler, &UKeyClickHandler::OnClick);

		ClickHandlers.Add(Handler);
	}

	if (Txt)
	{
		FKeyConfig Config;
		Config.WidgetName = WidgetName;
		Config.KeyCode = KeyCode;
		Config.LabelKor = Kor;
		Config.LabelEng = Eng;
		Config.LabelShift = Shift;
		Config.LabelNum = Num;
		Config.TextBlockRef = Txt;

		KeyConfigs.Add(Config);
	}
}

void UKeyboardWidget::SetupSpecialKey(const FString& WidgetName, const FString& KeyCode, const FString& Label)
{
	SetupKey(WidgetName, KeyCode, Label, Label, Label, Label);
}

UButton* UKeyboardWidget::FindButtonInWidget(UWidget* Widget)
{
	if (UButton* Btn = Cast<UButton>(Widget)) return Btn;

	if (UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
	{
		if (UserWidget->WidgetTree)
		{
			UButton* FoundBtn = nullptr;
			UserWidget->WidgetTree->ForEachWidget([&](UWidget* Child) {
				if (!FoundBtn)
				{
					if (UButton* B = Cast<UButton>(Child))
					{
						FoundBtn = B;
					}
				}
				});
			return FoundBtn;
		}
	}
	return nullptr;
}

UTextBlock* UKeyboardWidget::FindTextBlockInWidget(UWidget* Widget)
{
	if (UTextBlock* Txt = Cast<UTextBlock>(Widget)) return Txt;

	if (UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
	{
		if (UserWidget->WidgetTree)
		{
			UTextBlock* FoundTxt = nullptr;
			UserWidget->WidgetTree->ForEachWidget([&](UWidget* Child) {
				if (!FoundTxt)
				{
					if (UTextBlock* T = Cast<UTextBlock>(Child))
					{
						FoundTxt = T;
					}
				}
				});
			return FoundTxt;
		}
	}
	return nullptr;
}

// ----------------------------------------------------------------------------
// 한글 조합
// ----------------------------------------------------------------------------
FString UKeyboardWidget::ComposeHangul(const FString& Jamos)
{
	FString Result;

	int32 ChoIdx = -1;
	int32 JungIdx = -1;
	int32 JongIdx = -1;

	// 현재까지 쌓인 (초성/중성/종성)으로 한 글자를 Result에 밀어 넣고 상태 초기화
	auto FlushCurrent = [&]()
	{
		if (ChoIdx != -1 && JungIdx != -1)
		{
			const int32 FinalIdx = (JongIdx == -1 ? 0 : JongIdx);
			const TCHAR Code = HANGUL_BASE
				+ (ChoIdx * 21 * 28)
				+ (JungIdx * 28)
				+ FinalIdx;

			Result.AppendChar(Code);
		}
		else if (ChoIdx != -1)
		{
			Result += Chosungs[ChoIdx];
		}
		else if (JungIdx != -1)
		{
			Result += Jungsungs[JungIdx];
		}

		ChoIdx = -1;
		JungIdx = -1;
		JongIdx = -1;
	};

	for (int32 i = 0; i < Jamos.Len(); ++i)
	{
		const TCHAR C = Jamos[i];

		FString CharStr;
		CharStr.AppendChar(C);

		const int32 NewCho = GetIndex(Chosungs, CharStr);
		const int32 NewJung = GetIndex(Jungsungs, CharStr);
		const int32 NewJong = GetIndex(Jongsungs, CharStr);

		// --------------------------------------------------------------------
		// Case 0: 아무 것도 쌓이지 않은 상태
		// --------------------------------------------------------------------
		if (ChoIdx == -1 && JungIdx == -1 && JongIdx == -1)
		{
			if (NewCho != -1)
			{
				// 자음 → 초성 시작
				ChoIdx = NewCho;
			}
			else if (NewJung != -1)
			{
				// 모음 → 초성 없이 모음만 있는 음절 시작
				JungIdx = NewJung;
			}
			else
			{
				// 한글 자모가 아니면 그대로 출력
				Result.AppendChar(C);
			}
			continue;
		}

		// --------------------------------------------------------------------
		// Case V: 초성 없이 모음만 있는 상태 (← 여기 분기가 기존 코드에는 없어서 문제)
		// ChoIdx == -1, JungIdx != -1, JongIdx == -1
		// --------------------------------------------------------------------
		if (ChoIdx == -1 && JungIdx != -1 && JongIdx == -1)
		{
			if (NewJung != -1)
			{
				// 모음 + 모음 → 복합 모음 시도 (ㅘ, ㅙ, ㅚ, ㅝ, ㅞ, ㅟ, ㅢ)
				TCHAR PrevVowel = Jungsungs[JungIdx][0];
				TCHAR Combined = 0;

				if (TryCombineJung(PrevVowel, C, Combined))
				{
					FString CombStr;
					CombStr.AppendChar(Combined);
					const int32 CombIdx = GetIndex(Jungsungs, CombStr);

					if (CombIdx != -1)
					{
						// ㅗ + ㅏ → ㅘ, ㅜ + ㅣ → ㅟ 등
						JungIdx = CombIdx;
					}
					else
					{
						// 이론상 거의 안 오지만, 혹시 못 찾으면 이전 모음 확정 후 새 모음 시작
						Result += Jungsungs[JungIdx];
						JungIdx = NewJung;
					}
				}
				else
				{
					// 복합 모음이 아니면 이전 모음을 확정하고 새 모음 시작
					Result += Jungsungs[JungIdx];
					JungIdx = NewJung;
				}
			}
			else if (NewCho != -1)
			{
				// 모음 상태에서 자음이 오면 → 기존 모음은 단독 출력, 새 자음은 초성
				Result += Jungsungs[JungIdx];
				ChoIdx = NewCho;
				JungIdx = -1;
			}
			else
			{
				// 한글 자모가 아니면 현재 모음을 확정하고 그대로 출력
				FlushCurrent();
				Result.AppendChar(C);
			}
			continue;
		}

		// --------------------------------------------------------------------
		// Case 1: 초성만 있는 상태 (Cho, -, -)
		// --------------------------------------------------------------------
		if (ChoIdx != -1 && JungIdx == -1)
		{
			if (NewJung != -1)
			{
				// 초성 + 모음 → 기본 '가' 형태
				JungIdx = NewJung;
			}
			else if (NewCho != -1)
			{
				// 자음 + 자음 → 앞 자음은 단독 글자로 확정, 새 자음이 초성
				Result += Chosungs[ChoIdx];
				ChoIdx = NewCho;
			}
			else
			{
				// 한글이 아니면 현재 초성 출력 후 그대로 추가
				FlushCurrent();
				Result.AppendChar(C);
			}
			continue;
		}

		// --------------------------------------------------------------------
		// Case 2: 초성 + 중성 (받침 없음): (Cho, Jung, -)
		// --------------------------------------------------------------------
		if (ChoIdx != -1 && JungIdx != -1 && JongIdx == -1)
		{
			if (NewJung != -1)
			{
				// 모음 + 모음 → 복합 모음 시도
				TCHAR PrevVowel = Jungsungs[JungIdx][0];
				TCHAR Combined = 0;

				if (TryCombineJung(PrevVowel, C, Combined))
				{
					FString CombStr;
					CombStr.AppendChar(Combined);
					const int32 CombIdx = GetIndex(Jungsungs, CombStr);

					if (CombIdx != -1)
					{
						JungIdx = CombIdx; // ex) ㅗ + ㅏ = ㅘ
					}
					else
					{
						const TCHAR Code = HANGUL_BASE
							+ (ChoIdx * 21 * 28)
							+ (JungIdx * 28);

						Result.AppendChar(Code);
						ChoIdx = -1;
						JungIdx = NewJung;
						JongIdx = -1;
					}
				}
				else
				{
					// 복합 모음이 아니면 현재 음절을 확정하고 새 모음만 있는 음절 시작
					const TCHAR Code = HANGUL_BASE
						+ (ChoIdx * 21 * 28)
						+ (JungIdx * 28);

					Result.AppendChar(Code);
					ChoIdx = -1;
					JungIdx = NewJung;
					JongIdx = -1;
				}
			}
			else if (NewCho != -1)
			{
				// 자음 → 받침 후보
				if (NewJong > 0)
				{
					// 받침으로 사용 가능 → 종성 설정 (ex: 가 + ㄱ = 각)
					JongIdx = NewJong;
				}
				else
				{
					// 받침으로 쓸 수 없는 자음 → 음절 끊고 새 초성 시작
					const TCHAR Code = HANGUL_BASE
						+ (ChoIdx * 21 * 28)
						+ (JungIdx * 28);

					Result.AppendChar(Code);
					ChoIdx = NewCho;
					JungIdx = -1;
					JongIdx = -1;
				}
			}
			else
			{
				// 한글 아님 → 현재 음절 확정 후 그대로 추가
				FlushCurrent();
				Result.AppendChar(C);
			}
			continue;
		}

		// --------------------------------------------------------------------
		// Case 3: 초성 + 중성 + 종성(받침) 있는 상태: (Cho, Jung, Jong)
		// --------------------------------------------------------------------
		if (ChoIdx != -1 && JungIdx != -1 && JongIdx != -1)
		{
			if (NewJung != -1)
			{
				// 받침 뒤에 모음이 오면:
				//  - 받침을 (단일/복합에 따라) 앞음절/다음음절로 나누기
				//    ex) 박 + ㅏ → 바 + 가
				//        밝 + ㅏ → 발 + 가
				int32 FirstJongIdx = 0;
				int32 SecondJongIdx = 0;
				SplitCompositeJong(JongIdx, FirstJongIdx, SecondJongIdx);

				// 이전 음절: 첫 번째 받침 사용 (0이면 받침 없음)
				const TCHAR CodePrev = HANGUL_BASE
					+ (ChoIdx * 21 * 28)
					+ (JungIdx * 28)
					+ FirstJongIdx;

				Result.AppendChar(CodePrev);

				// 새 음절의 초성은 두 번째 받침에서 가져온다 (0이면 초성 없음)
				int32 NextChoIdx = -1;
				if (SecondJongIdx > 0)
				{
					const FString SecondChar = Jongsungs[SecondJongIdx];
					NextChoIdx = GetIndex(Chosungs, SecondChar);
				}

				ChoIdx = NextChoIdx;
				JungIdx = NewJung;
				JongIdx = -1;
			}
			else if (NewCho != -1)
			{
				// 받침 뒤에 자음이 오면: 복합 받침 시도
				TCHAR PrevFinal = Jongsungs[JongIdx][0];
				TCHAR Combined = 0;

				if (TryCombineJong(PrevFinal, C, Combined))
				{
					FString CombStr;
					CombStr.AppendChar(Combined);
					const int32 CombIdx = GetIndex(Jongsungs, CombStr);

					if (CombIdx != -1)
					{
						// ex) ㄱ + ㅅ = ㄳ, ㄹ + ㄱ = ㄺ, ㅂ + ㅅ = ㅄ ...
						JongIdx = CombIdx;
					}
					else
					{
						// 안전장치: 실패 시 음절 끊고 새 초성 시작
						const TCHAR Code = HANGUL_BASE
							+ (ChoIdx * 21 * 28)
							+ (JungIdx * 28)
							+ JongIdx;

						Result.AppendChar(Code);

						ChoIdx = NewCho;
						JungIdx = -1;
						JongIdx = -1;
					}
				}
				else
				{
					// 복합 받침 안 되면 현재 음절 확정 후 새 초성 시작
					const TCHAR Code = HANGUL_BASE
						+ (ChoIdx * 21 * 28)
						+ (JungIdx * 28)
						+ JongIdx;

					Result.AppendChar(Code);

					ChoIdx = NewCho;
					JungIdx = -1;
					JongIdx = -1;
				}
			}
			else
			{
				// 한글 아님 → 현재 음절 확정 후 그대로 추가
				FlushCurrent();
				Result.AppendChar(C);
			}
			continue;
		}
	}

	// 루프 끝난 뒤 남아있는 조합 처리
	if (ChoIdx != -1 || JungIdx != -1 || JongIdx != -1)
	{
		FlushCurrent();
	}

	return Result;
}


void UKeyboardWidget::ResetKeyboardState(bool bClearText)
{
	HangulBuffer.Empty();
	bKoreanShift = false;
	ClearSelection();

	if (bClearText)
	{
		CommittedText.Empty();
		CurrentText.Empty();
	}

	UpdateDisplay();

	// 포커스/선택 상태도 초기화하고 싶으면
	if (EditableTextBox_Box)
	{
		EditableTextBox_Box->SetSelectAllTextWhenFocused(false);
	}
}

// ----------------------------------------------------------------------------
// 표시 문자열 갱신
// ----------------------------------------------------------------------------

void UKeyboardWidget::UpdateDisplay()
{
	if (CurrentMode == EKeyboardMode::Korean && !HangulBuffer.IsEmpty())
	{
		CurrentText = CommittedText + ComposeHangul(HangulBuffer);
	}
	else
	{
		CurrentText = CommittedText;
	}

	if (OutputTextBlock)
	{
		OutputTextBlock->SetText(FText::FromString(CurrentText));
	}

	// 추가: EditableTextBox도 동일하게 갱신
	if (EditableTextBox_Box)
	{
		EditableTextBox_Box->SetText(FText::FromString(CurrentText));
	}
}

// ----------------------------------------------------------------------------
// 키 입력 처리
// ----------------------------------------------------------------------------

void UKeyboardWidget::HandleKeyPress(FString KeyCode)
{
	UE_LOG(LogTemp, Warning, TEXT("HandleKeyPress: %s"), *KeyCode);

	const float Now = UGameplayStatics::GetRealTimeSeconds(GetWorld());
	if (Now - LatestClickSecond < DoubleClickTime)
	{
		return;
	}

	// 현재 모드에 따른 기본 입력 문자
	FString InputChar = KeyCode;
	const FKeyConfig* FoundConfig = KeyConfigs.FindByPredicate(
		[&](const FKeyConfig& Config) { return Config.KeyCode == KeyCode; });

	if (FoundConfig)
	{
		switch (CurrentMode)
		{
		case EKeyboardMode::Korean:
			InputChar = FoundConfig->LabelKor;
			break;
		case EKeyboardMode::EnglishLower:
			InputChar = FoundConfig->LabelEng;
			break;
		case EKeyboardMode::EnglishUpper:
			InputChar = FoundConfig->LabelShift;
			break;
		case EKeyboardMode::NumberSpecial:
			InputChar = FoundConfig->LabelNum;
			break;
		}
	}

	// 특수키 처리
	if (KeyCode == "BACKSPACE")
	{
		DeleteLastCharacter();
	}
	else if (KeyCode == "SPACE")
	{
		CommitHangul();
		CommittedText += " ";
	}
	else if (KeyCode == "ENTER")
	{
		CommitHangul();
		UpdateDisplay(); // CurrentText 최신화

		HandleOnClickEnterDele.Broadcast(FText::FromString(CurrentText));
		UWidget* FoundWidget = FindObject<UWidget>(this, TEXT("WBP_FKey_5"));

		UUtilLibrary::LockButtonForSeconds(Cast<UButton>(FoundWidget), GetWorld(), 0.2f);

		// 키보드 닫기 전에 내부 상태 리셋 (다음 입력에 이전 값 남지 않게)
		ResetKeyboardState(true);

		return;
	}
	else if (KeyCode == "SHIFT")
	{
		if (CurrentMode == EKeyboardMode::Korean)
		{
			// 한글 모드: 쌍자음 토글 (다음 자음 1회용)
			bKoreanShift = !bKoreanShift;
			RefreshKeyLabels();
		}
		else if (CurrentMode == EKeyboardMode::EnglishLower)
		{
			SetKeyboardMode(EKeyboardMode::EnglishUpper);
		}
		else if (CurrentMode == EKeyboardMode::EnglishUpper)
		{
			SetKeyboardMode(EKeyboardMode::EnglishLower);
		}

		UpdateDisplay();
		return; // SHIFT 자체는 문자 입력 없음
	}
	else if (KeyCode == "TOGGLE_LANG")
	{
		if (CurrentMode == EKeyboardMode::Korean)
		{
			SetKeyboardMode(EKeyboardMode::EnglishLower);
		}
		else
		{
			SetKeyboardMode(EKeyboardMode::Korean);
		}
	}
	else if (KeyCode == "MODE_KOR")
	{
		SetKeyboardMode(EKeyboardMode::Korean);
	}
	else if (KeyCode == "MODE_ENG")
	{
		SetKeyboardMode(EKeyboardMode::EnglishLower);
	}
	else if (KeyCode == "MODE_NUM")
	{
		if (CurrentMode == EKeyboardMode::NumberSpecial)
		{
			SetKeyboardMode(EKeyboardMode::Korean);
		}
		else
		{
			SetKeyboardMode(EKeyboardMode::NumberSpecial);
		}
	}
	else
	{
		// 일반 문자 키
		if (CurrentMode == EKeyboardMode::Korean)
		{
			if (IsHangul(InputChar))
			{
				// SHIFT 상태면 쌍자음으로 변환 후 1회 사용
				if (bKoreanShift)
				{
					InputChar = ApplyKoreanShift(InputChar);
					bKoreanShift = false;
					RefreshKeyLabels();
				}

				ProcessHangulInput(InputChar);
			}
			else
			{
				CommitHangul();
				CommittedText += InputChar;
			}
		}
		else
		{
			CommitHangul();
			CommittedText += InputChar;
		}
	}

	UpdateDisplay();

	if (GEngine)
	{
		FString ModeString;
		switch (CurrentMode)
		{
		case EKeyboardMode::Korean:        ModeString = TEXT("Korean"); break;
		case EKeyboardMode::EnglishLower:  ModeString = TEXT("English (Lower)"); break;
		case EKeyboardMode::EnglishUpper:  ModeString = TEXT("English (Upper)"); break;
		case EKeyboardMode::NumberSpecial: ModeString = TEXT("Number/Special"); break;
		}

		const FString DebugMsg = FString::Printf(TEXT("Mode: %s | Input: %s | Result: %s"),
			*ModeString, *InputChar, *CurrentText);
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, DebugMsg);
	}


	EditableTextBox_Box->SelectAllTextWhenFocused = false;
	//FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);

	LatestClickSecond = Now;

	bIsFirstDelete = false;
}

// ----------------------------------------------------------------------------
// 모드 전환 + 키캡 갱신
// ----------------------------------------------------------------------------

void UKeyboardWidget::SetKeyboardMode(EKeyboardMode NewMode)
{
	UE_LOG(LogTemp, Warning, TEXT("Switching Mode: %d -> %d"),
		static_cast<int32>(CurrentMode), static_cast<int32>(NewMode));

	CommitHangul();
	CurrentMode = NewMode;

	// 한글 모드가 아니면 쌍자음 상태 초기화
	if (CurrentMode != EKeyboardMode::Korean)
	{
		bKoreanShift = false;
	}

	RefreshKeyLabels();
	UpdateDisplay();
}

// 현재 Config에 대해 보여줄 라벨 계산
FString UKeyboardWidget::GetDisplayLabelForConfig(const FKeyConfig& Config) const
{
	FString NewLabel = Config.LabelEng;

	switch (CurrentMode)
	{
	case EKeyboardMode::Korean:
		NewLabel = Config.LabelKor;
		if (bKoreanShift)
		{
			// 한글 모드 + SHIFT → 키캡도 쌍자음으로 표시
			NewLabel = const_cast<UKeyboardWidget*>(this)->ApplyKoreanShift(NewLabel);
		}
		break;

	case EKeyboardMode::EnglishLower:
		NewLabel = Config.LabelEng;
		break;

	case EKeyboardMode::EnglishUpper:
		NewLabel = Config.LabelShift;
		break;

	case EKeyboardMode::NumberSpecial:
		NewLabel = Config.LabelNum;
		break;
	}

	return NewLabel;
}

// 실제 텍스트블록에 라벨 반영
void UKeyboardWidget::RefreshKeyLabels()
{
	int32 UpdatedCount = 0;

	for (FKeyConfig& Config : KeyConfigs)
	{
		if (Config.TextBlockRef)
		{
			const FString NewLabel = GetDisplayLabelForConfig(Config);
			Config.TextBlockRef->SetText(FText::FromString(NewLabel));
			UpdatedCount++;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("Updated Labels for %d keys"), UpdatedCount);

	OnUpdateKeyLabels();
}

// 블루프린트에서 키 인덱스로 라벨 얻기
FString UKeyboardWidget::GetLabelForKey(int32 KeyIndex)
{
	if (!KeyConfigs.IsValidIndex(KeyIndex))
	{
		return TEXT("");
	}

	const FKeyConfig& Config = KeyConfigs[KeyIndex];
	return GetDisplayLabelForConfig(Config);
}

bool UKeyboardWidget::HasSelection() const
{
	return SelectionStartIndex != INDEX_NONE &&
		SelectionEndIndex != INDEX_NONE &&
		SelectionStartIndex != SelectionEndIndex;
}

void UKeyboardWidget::SetSelection(int32 InStart, int32 InEnd)
{
	// 한글 버퍼는 편의상 커밋한 뒤 CommittedText 기준으로 인덱싱
	CommitHangul();

	const int32 Len = CommittedText.Len();

	int32 Start = FMath::Clamp(InStart, 0, Len);
	int32 End = FMath::Clamp(InEnd, 0, Len);

	// 둘이 같으면 선택 없음으로 취급
	if (Start == End)
	{
		SelectionStartIndex = INDEX_NONE;
		SelectionEndIndex = INDEX_NONE;
		return;
	}

	if (Start > End)
	{
		Swap(Start, End);
	}

	SelectionStartIndex = Start;
	SelectionEndIndex = End;
}

void UKeyboardWidget::ClearSelection()
{
	SelectionStartIndex = INDEX_NONE;
	SelectionEndIndex = INDEX_NONE;
}

// ----------------------------------------------------------------------------
// 한글 상태/삭제 등
// ----------------------------------------------------------------------------

bool UKeyboardWidget::IsHangul(const FString& Text)
{
	if (Text.IsEmpty()) return false;
	const TCHAR Char = Text[0];
	return (Char >= 0xAC00 && Char <= 0xD7A3) || (Char >= 0x3131 && Char <= 0x3163);
}
void UKeyboardWidget::ProcessHangulInput(FString Jamo)
{
	// 그냥 버퍼에 자모를 쌓기만 한다.
	// 화면에 보이는 글자는 UpdateDisplay에서
	//   CurrentText = CommittedText + ComposeHangul(HangulBuffer)
	// 로 계산.
	HangulBuffer += Jamo;
}

void UKeyboardWidget::CommitHangul()
{
	if (!HangulBuffer.IsEmpty())
	{
		// 지금까지 쌓인 자모 전체를 한 번에 완성 글자로 변환
		CommittedText += ComposeHangul(HangulBuffer);
		HangulBuffer.Empty();
	}
}


void UKeyboardWidget::DeleteLastCharacter()
{
	if (bIsFirstDelete)
	{
		// 한글 버퍼는 먼저 커밋해서 인덱스 정리
		CommitHangul();
		EditableTextBox_Box->SetFocus();
		SetSelectAll(true);

		CommittedText.Empty();
		ClearSelection();
	}
	else
	{
		if (!HangulBuffer.IsEmpty())
		{
			// 마지막 자모 하나만 제거
			HangulBuffer = HangulBuffer.Left(HangulBuffer.Len() - 1);
		}
		else if (!CommittedText.IsEmpty())
		{
			CommittedText = CommittedText.Left(CommittedText.Len() - 1);
		}
	}

	UpdateDisplay();
}

// ----------------------------------------------------------------------------
// 한글 SHIFT용 쌍자음 매핑
// ----------------------------------------------------------------------------

FString UKeyboardWidget::ApplyKoreanShift(const FString& InputChar)
{
	// 자음 쌍자음
	// ㅂ -> ㅃ
	if (InputChar == TEXT("\u3142")) return TEXT("\u3143");
	// ㅈ -> ㅉ
	if (InputChar == TEXT("\u3148")) return TEXT("\u3149");
	// ㄷ -> ㄸ
	if (InputChar == TEXT("\u3137")) return TEXT("\u3138");
	// ㄱ -> ㄲ
	if (InputChar == TEXT("\u3131")) return TEXT("\u3132");
	// ㅅ -> ㅆ
	if (InputChar == TEXT("\u3145")) return TEXT("\u3146");

	// 모음 (SHIFT 시 ㅐ/ㅔ → ㅒ/ㅖ)
	// ㅐ -> ㅒ
	if (InputChar == TEXT("\u3150")) return TEXT("\u3152");
	// ㅔ -> ㅖ
	if (InputChar == TEXT("\u3154")) return TEXT("\u3156");

	// 그 외에는 그대로
	return InputChar;
}
