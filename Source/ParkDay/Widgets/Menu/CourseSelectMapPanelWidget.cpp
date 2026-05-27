#include "CourseSelectMapPanelWidget.h"
#include "../../Utils/JsonLoader.h"
#include "../../Utils/LoadTexture2DFromFileAsync.h"
#include "../../MenuGameMode.h"
#include "../../DataAsset/MenuUIImageDataAsset.h"
#include "../KeyboardWidget.h"
#include "Components/WrapBox.h"
#include "Components/ScrollBox.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Components/VerticalBox.h"
#include "Components/CanvasPanel.h"
#include "Algo/StableSort.h" // Algo::StableSort
#include "Styling/SlateTypes.h"
#include "Misc/Paths.h"
#include "ParkDay/Utils/UtilLibrary.h"

#include "CourseSelectMapWidget.h"

//PRAGMA_DISABLE_OPTIMIZATION
UCourseSelectMapPanelWidget::UCourseSelectMapPanelWidget(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	// "�ε�"�� �ƴ϶� "��� ����"�� �մϴ�.
	CourseMapWidgetClassSoft = TSoftClassPtr<UCourseSelectMapWidget>(
		FSoftObjectPath(TEXT("/Game/UMG/UI/CorseSelect/Panel/WBP_CourseMap.WBP_CourseMap_C"))
	);

	//static ConstructorHelpers::FClassFinder<UUserWidget> CourseSelectMapWidgetClassBP(
	//	TEXT("/Game/UMG/UI/CorseSelect/Panel/WBP_CourseMap.WBP_CourseMap_C"));

	//if (CourseSelectMapWidgetClassBP.Succeeded())
	//{
	//	CourseSelectMapWidgetClass = CourseSelectMapWidgetClassBP.Class;
	//}
}

void UCourseSelectMapPanelWidget::NativeOnInitialized()
{
	GM = Cast<AMenuGameMode>(GetWorld()->GetAuthGameMode());

	Button_ScrollDown->OnPressed.AddDynamic(this, &UCourseSelectMapPanelWidget::HandleOnClickScrollDown);
	Button_ScrollUp->OnPressed.AddDynamic(this, &UCourseSelectMapPanelWidget::HandleOnClickScrollUp);
	CheckBox_All->OnCheckStateChanged.AddDynamic(this, &UCourseSelectMapPanelWidget::HandleOnClickAllCheckBox);
	CheckBox_Recommend->OnCheckStateChanged.AddDynamic(this, &UCourseSelectMapPanelWidget::HandleOnClickRecommendCheckBox);
	CheckBox_Difficulty->OnCheckStateChanged.AddDynamic(this, &UCourseSelectMapPanelWidget::HandleOnClickDifficultyCheckBox);
	CheckBox_Location->OnCheckStateChanged.AddDynamic(this, &UCourseSelectMapPanelWidget::HandleOnClickLocationCheckBox);
	Button_Search->OnPressed.AddDynamic(this, &UCourseSelectMapPanelWidget::HandleOnClickSearch);

	/*1~9 �� area ���� ���� �Լ� bind �ϰ�
		�ű⼭ ���� �� ��° ��Ұ� check ���ִ��� Ȯ��
			�ش� ��ȣ�� CheckArea �ϸ��
	*/

	UClass* LoadedClass = CourseMapWidgetClassSoft.LoadSynchronous(); // ���⼭ ���� �ε�

	FCCList CachedCCList;
	FString CCListPath = TEXT("DATA/CourseMap/CCList.json");
	//if (UJsonLoader::LoadCCListFromJson(FPaths::ProjectDir() + TEXT("Content/DATA/CourseMap/CCList.json", CachedCCList))
	if (UJsonLoader::LoadCCListFromJson(CCListPath, CachedCCList))
	{
		for (int32 i = 0; i < CachedCCList.CCNames.Num(); i++)
		{
			FString CCName = CachedCCList.CCNames[i].CCName;

			UCourseSelectMapWidget* CourseMapInstance = CreateWidget<UCourseSelectMapWidget>(this, LoadedClass);
			CourseMapInstance->CCFolderName = CCName;
			CourseMapInstance->Init(CCName);
			CourseMapInstance->OnClickCourseButtonDele.AddDynamic(this, &UCourseSelectMapPanelWidget::HandleOnClickCourseButton);
			MapArray.Add(CourseMapInstance);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UCourseSelectMapPanelWidget::NativeConstruct() ==> Fail to load cclist from json"));
	}

	CanvasPanel_Area->SetVisibility(ESlateVisibility::Collapsed);
	InitializeAreaList();
}

void UCourseSelectMapPanelWidget::InitializeAreaList()
{
	for (UWidget* Widget : VerticalBox_Area->GetAllChildren())
	{
		if (Widget)
		{
			UCheckBox* Area = Cast<UCheckBox>(Widget);

			if (Area)
			{
				AreaList.Add(Area);
			}
		}
	}

	if (AreaList.Num() > 0)
	{
		AreaList[0]->OnCheckStateChanged.AddDynamic(this, &UCourseSelectMapPanelWidget::HandleOnChangeStateAreaCheckBox_1);
		AreaList[1]->OnCheckStateChanged.AddDynamic(this, &UCourseSelectMapPanelWidget::HandleOnChangeStateAreaCheckBox_2);
		AreaList[2]->OnCheckStateChanged.AddDynamic(this, &UCourseSelectMapPanelWidget::HandleOnChangeStateAreaCheckBox_3);
		AreaList[3]->OnCheckStateChanged.AddDynamic(this, &UCourseSelectMapPanelWidget::HandleOnChangeStateAreaCheckBox_4);
		AreaList[4]->OnCheckStateChanged.AddDynamic(this, &UCourseSelectMapPanelWidget::HandleOnChangeStateAreaCheckBox_5);
		AreaList[5]->OnCheckStateChanged.AddDynamic(this, &UCourseSelectMapPanelWidget::HandleOnChangeStateAreaCheckBox_6);
		AreaList[6]->OnCheckStateChanged.AddDynamic(this, &UCourseSelectMapPanelWidget::HandleOnChangeStateAreaCheckBox_7);
		AreaList[7]->OnCheckStateChanged.AddDynamic(this, &UCourseSelectMapPanelWidget::HandleOnChangeStateAreaCheckBox_8);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UCourseSelectMapPanelWidget::InitializeAreaList() ==> AreaList is empty"));
	}
}

void UCourseSelectMapPanelWidget::HandleOnChangeStateAreaCheckBox_1(bool bIsChecked) { ApplyAreaCheck(AreaList[0], bIsChecked, 1); }
void UCourseSelectMapPanelWidget::HandleOnChangeStateAreaCheckBox_2(bool bIsChecked) { ApplyAreaCheck(AreaList[1], bIsChecked, 2); }
void UCourseSelectMapPanelWidget::HandleOnChangeStateAreaCheckBox_3(bool bIsChecked) { ApplyAreaCheck(AreaList[2], bIsChecked, 3); }
void UCourseSelectMapPanelWidget::HandleOnChangeStateAreaCheckBox_4(bool bIsChecked) { ApplyAreaCheck(AreaList[3], bIsChecked, 4); }
void UCourseSelectMapPanelWidget::HandleOnChangeStateAreaCheckBox_5(bool bIsChecked) { ApplyAreaCheck(AreaList[4], bIsChecked, 5); }
void UCourseSelectMapPanelWidget::HandleOnChangeStateAreaCheckBox_6(bool bIsChecked) { ApplyAreaCheck(AreaList[5], bIsChecked, 6); }
void UCourseSelectMapPanelWidget::HandleOnChangeStateAreaCheckBox_7(bool bIsChecked) { ApplyAreaCheck(AreaList[6], bIsChecked, 7); }
void UCourseSelectMapPanelWidget::HandleOnChangeStateAreaCheckBox_8(bool bIsChecked) { ApplyAreaCheck(AreaList[7], bIsChecked, 8); }

void UCourseSelectMapPanelWidget::ApplyAreaCheck(UCheckBox* CheckedBox, bool bIsChecked, int32 AreaNumber)
{
	UUtilLibrary::LockCheckBoxForSeconds(CheckedBox, GetWorld(), 0.25f);
	//�̹� üũ�� �� ���� ���
	if (!bIsChecked)
	{
		CheckedBox->SetIsChecked(true);
		return;
	}
	else
	{
		EditableTextBox_Search->SetText(FText::GetEmpty());
		UnCheckAllArea();
		CheckedBox->SetIsChecked(true);
		CollapseAllMapWidget();

		for (UCourseSelectMapWidget* MapWidget : MapArray)
		{
			if (MapWidget->FieldMapInfo.Area == AreaNumber)
			{
				MapWidget->SetVisibility(ESlateVisibility::Visible);
			}
		}
	}
}

void UCourseSelectMapPanelWidget::UnCheckAllArea()
{
	if (AreaList.Num() > 0)
	{
		for (UCheckBox* Area : AreaList)
		{
			if (Area)
			{
				Area->SetIsChecked(false);
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UCourseSelectMapPanelWidget::UnCheckAllArea() ==> AreaList is null or empty"));
	}
}

void UCourseSelectMapPanelWidget::CheckArea(int32 AreaNumber)
{
	UnCheckAllArea();

	int32 AreaIndex = AreaNumber - 1;

	if (AreaList.IsValidIndex(AreaIndex))
	{
		AreaList[AreaIndex]->SetIsChecked(true);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UCourseSelectMapPanelWidget::CheckArea(int32 AreaNumber) ==> AreaList[%d] is null index"), AreaIndex);
	}
}

void UCourseSelectMapPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	//����
	SortItems(CurrentSortKey, CurrentSortOrder, true);

	for (UCourseSelectMapWidget* CourseMap : MapArray)
	{
		WrapBox_CourseMaps->AddChildToWrapBox(CourseMap);
	}
}

void UCourseSelectMapPanelWidget::SelectFirstOne()
{
	if (MapArray.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("SelectFirstOne: MapArray 비어있음 - CCList.json 확인 필요"));
		return;
	}
	MapArray[0]->HandleOnClickCourseMap();
}

void UCourseSelectMapPanelWidget::ReAddChildMapWidget()
{
	WrapBox_CourseMaps->ClearChildren();

	for (UCourseSelectMapWidget* CourseMap : MapArray)
	{
		WrapBox_CourseMaps->AddChildToWrapBox(CourseMap);
	}
}


void UCourseSelectMapPanelWidget::HandleOnClickCourseButton(FFieldMapInfo FieldMapInfo, FString CCFolderName)
{
	for (UCourseSelectMapWidget* Widget : MapArray)
	{
		Widget->bIsSelected = false;

		if (Widget->CCFolderName == CCFolderName)
		{
			Widget->bIsSelected = true;
		}

		Widget->UpdateCourseMapPanelImage();
	}

	FTimerHandle TH;
	GetWorld()->GetTimerManager().SetTimer(TH, [WeakThis = TWeakObjectPtr<UCourseSelectMapPanelWidget>(this)]
		{
			WeakThis->CanvasPanel_Area->SetVisibility(ESlateVisibility::Collapsed);
		}
	, 0.25f, false);
}


void UCourseSelectMapPanelWidget::HandleOnClickScrollDown()
{
	float NewScrollOffSet = ScrollBox_CourseMaps->GetScrollOffset() + ScrollBox_CourseMaps->GetScrollOffsetOfEnd() * 0.1f;
	ScrollBox_CourseMaps->SetScrollOffset(NewScrollOffSet);
}

void UCourseSelectMapPanelWidget::HandleOnClickScrollUp()
{
	float NewScrollOffSet = ScrollBox_CourseMaps->GetScrollOffset() - ScrollBox_CourseMaps->GetScrollOffsetOfEnd() * 0.1f;
	ScrollBox_CourseMaps->SetScrollOffset(NewScrollOffSet);
}

void UCourseSelectMapPanelWidget::HandleOnClickSearch()
{
	if (GM->IsClickAllowed())
	{
		FText SearchText = EditableTextBox_Search->GetText();

		if (SearchText.IsEmpty())
		{
			for (UCourseSelectMapWidget* Widget : MapArray)
			{
				Widget->SetVisibility(ESlateVisibility::Visible);
			}
		}
		else
		{
			for (UCourseSelectMapWidget* Widget : MapArray)
			{
				Widget->SetVisibility(ESlateVisibility::Visible);

				if (!Widget->FieldMapInfo.CCname.Contains(SearchText.ToString()))
				{
					Widget->SetVisibility(ESlateVisibility::Collapsed);
				}
			}
		}

		SelectFirstOne();
	}
}

void UCourseSelectMapPanelWidget::HandleOnClickAllCheckBox(bool bIsChecked)
{
	UUtilLibrary::LockCheckBoxForSeconds(CheckBox_All, GetWorld(), 0.25f);
	if (GM->IsClickAllowed())
	{
		if (bIsChecked)
		{
			VisibleAllMapWidget();
			EditableTextBox_Search->SetText(FText::GetEmpty());
			SortItems(EFieldSortKey::CCname, CurrentSortOrder, true);
			ReAddChildMapWidget();
		}
		CanvasPanel_Area->SetVisibility(ESlateVisibility::Collapsed);
		CheckBox_Difficulty->SetIsChecked(false);
		CheckBox_Recommend->SetIsChecked(false);
		CheckBox_Location->SetIsChecked(false);
	}

	if (!bIsChecked)
	{
		CheckBox_All->SetIsChecked(true);
	}
}


void UCourseSelectMapPanelWidget::HandleOnClickRecommendCheckBox(bool bIsChecked)
{
	UUtilLibrary::LockCheckBoxForSeconds(CheckBox_Recommend, GetWorld(), 0.25f);
	if (GM->IsClickAllowed())
	{
		if (bIsChecked)
		{
			VisibleAllMapWidget();
			EditableTextBox_Search->SetText(FText::GetEmpty());
			SortItems(EFieldSortKey::Recommend, CurrentSortOrder, true);
			ReAddChildMapWidget();
		}
		CanvasPanel_Area->SetVisibility(ESlateVisibility::Collapsed);
		CheckBox_All->SetIsChecked(false);
		CheckBox_Difficulty->SetIsChecked(false);
		CheckBox_Location->SetIsChecked(false);
	}

	if (!bIsChecked)
	{
		CheckBox_Recommend->SetIsChecked(true);
	}
}

void UCourseSelectMapPanelWidget::HandleOnClickDifficultyCheckBox(bool bIsChecked)
{
	UUtilLibrary::LockCheckBoxForSeconds(CheckBox_Difficulty, GetWorld(), 0.25f);
	if (GM->IsClickAllowed())
	{
		if (bIsChecked)
		{
			VisibleAllMapWidget();
			EditableTextBox_Search->SetText(FText::GetEmpty());
			SortItems(EFieldSortKey::CourseLevel, CurrentSortOrder, true);
			ReAddChildMapWidget();
		}

		CanvasPanel_Area->SetVisibility(ESlateVisibility::Collapsed);
		CheckBox_All->SetIsChecked(false);
		CheckBox_Recommend->SetIsChecked(false);
		CheckBox_Location->SetIsChecked(false);
	}

	if (!bIsChecked)
	{
		CheckBox_Difficulty->SetIsChecked(true);
	}
}

void UCourseSelectMapPanelWidget::VisibleAllMapWidget()
{
	for (UCourseSelectMapWidget* MapWidget : MapArray)
	{
		MapWidget->SetVisibility(ESlateVisibility::Visible);
	}
}

void UCourseSelectMapPanelWidget::CollapseAllMapWidget()
{
	for (UCourseSelectMapWidget* MapWidget : MapArray)
	{
		MapWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UCourseSelectMapPanelWidget::HandleOnClickLocationCheckBox(bool bIsChecked)
{
	UUtilLibrary::LockCheckBoxForSeconds(CheckBox_Location, GetWorld(), 0.25f);
	if (GM->IsClickAllowed())
	{
		if (bIsChecked)
		{
			//SortItems(EFieldSortKey::Area, CurrentSortOrder, true);
			//ReAddChildMapWidget();
			EditableTextBox_Search->SetText(FText::GetEmpty());
			CanvasPanel_Area->SetVisibility(ESlateVisibility::Visible);
			CollapseAllMapWidget();
			UnCheckAllArea();
		}

		CheckBox_All->SetIsChecked(false);
		CheckBox_Recommend->SetIsChecked(false);
		CheckBox_Difficulty->SetIsChecked(false);

		if (!bIsChecked)
		{
			if (CanvasPanel_Area->IsVisible())
				CanvasPanel_Area->SetVisibility(ESlateVisibility::Collapsed);
			else
				CanvasPanel_Area->SetVisibility(ESlateVisibility::Visible);
			CheckBox_Location->SetIsChecked(true);
		}
	}
}

void UCourseSelectMapPanelWidget::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	HandleEditBoxEnterFocus();
}

void UCourseSelectMapPanelWidget::HandleEditBoxEnterFocus()
{
	GetWorld()->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateLambda([this]()
			{
				if (EditableTextBox_Search->HasKeyboardFocus())
				{
					if (GM)
					{
						GM->KeyBoardWidgetInstance->bIsFirstDelete = true;
						GM->KeyBoardWidgetInstance->CurrentText = EditableTextBox_Search->GetText().ToString();
						GM->KeyBoardWidgetInstance->CommittedText = EditableTextBox_Search->GetText().ToString();
						GM->KeyBoardWidgetInstance->UpdateDisplay();
						GM->KeyBoardWidgetInstance->HandleOnClickEnterDele.RemoveAll(GM->GetStateWidget(EUIState::PlayerSelect));
						GM->KeyBoardWidgetInstance->HandleOnClickEnterDele.RemoveAll(this);
						GM->KeyBoardWidgetInstance->HandleOnClickEnterDele.AddDynamic(this, &UCourseSelectMapPanelWidget::HandleOnClickKeyboardEnter);
						GM->KeyBoardWidgetInstance->SetVisibility(ESlateVisibility::Visible);
						GM->KeyBoardWidgetInstance->EditableTextBox_Box->SetFocus();
					}
				}
				else
				{
					return;
				}
			})
	);
}

void UCourseSelectMapPanelWidget::HandleOnClickKeyboardEnter(FText InputText)
{
	EditableTextBox_Search->SetText(InputText);
	HandleOnClickSearch();
	GM->KeyBoardWidgetInstance->SetVisibility(ESlateVisibility::Collapsed);
}

static int32 CmpInt(int32 A, int32 B)
{
	return (A < B) ? -1 : (A > B) ? 1 : 0;
}

void UCourseSelectMapPanelWidget::SortItems(EFieldSortKey SortKey, ESortOrder SortOrder, bool bStable)
{
	CurrentSortKey = SortKey;
	CurrentSortOrder = SortOrder;

	const bool bAsc = (SortOrder == ESortOrder::Ascending);

	auto Less = [SortKey, bAsc](const UCourseSelectMapWidget& L, const UCourseSelectMapWidget& R)
		{
			const FFieldMapInfo& A = L.FieldMapInfo;
			const FFieldMapInfo& B = R.FieldMapInfo;

			int32 Cmp = 0;

			switch (SortKey)
			{
			case EFieldSortKey::CCname:
				// ���ڿ� ��(��ҹ��� ����). �ѱ��� �����ڵ� �ڵ�����Ʈ ���� ������ �˴ϴ�.
				Cmp = A.CCname.Compare(B.CCname, ESearchCase::IgnoreCase);
				break;

			case EFieldSortKey::CourseLevel:
				Cmp = CmpInt(A.CourseLevel, B.CourseLevel);
				break;

			case EFieldSortKey::Recommend:
				Cmp = CmpInt(A.Recommend, B.Recommend);
				break;

			case EFieldSortKey::Area:
				Cmp = CmpInt(A.Area, B.Area);
				break;

			default:
				Cmp = 0;
				break;
			}

			if (Cmp == 0)
			{
				// ��Ű �ϳ��� ���ġ��̸� ������ �״�� �δ� ���� �½��ϴ�.
				// StableSort ��� ��: ���� ���� ������ �������� �����˴ϴ�.
				// �Ϲ� Sort ��� ��: ������ ��� ������ �ٲ� �� �ֽ��ϴ�.
				return false;
			}

			return bAsc ? (Cmp < 0) : (Cmp > 0);
		};

	if (bStable)
	{
		Algo::StableSort(MapArray, [&Less](const UCourseSelectMapWidget* A, const UCourseSelectMapWidget* B)
			{
				if (!A || !B) return false; // nullptr ���
				return Less(*A, *B);
			});
	}
	else
	{
		MapArray.Sort([&Less](const UCourseSelectMapWidget& A, const UCourseSelectMapWidget& B)
			{
				return Less(A, B);
			});
	}
}

UCourseSelectMapWidget* UCourseSelectMapPanelWidget::GetSelectedMapWidget()
{
	for (int32 i = 0; i < MapArray.Num(); i++)
	{
		if (MapArray[i]->bIsSelected)
		{
			return MapArray[i];
		}
	}

	return nullptr;
}