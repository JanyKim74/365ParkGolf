#include "CourseSelectDetailWidget.h"
#include "../../MenuGameMode.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/HorizontalBox.h"
#include "Components/CheckBox.h"
#include "Components/VerticalBox.h"
#include "Components/CanvasPanel.h"
#include "CourseSelectWidget.h"
#include "CourseSelectMapWidget.h"
#include "CourseSelectOptionWidget.h"
#include "CourseSelectMapPanelWidget.h"
#include "CourseSelectDetailParWidget.h"
#include "../../Utils/LoadTexture2DFromFileAsync.h"


UCourseSelectDetailWidget::UCourseSelectDetailWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ParWidgetSoftClass = TSoftClassPtr<UCourseSelectDetailParWidget>(
		FSoftObjectPath(TEXT("/Game/UMG/UI/CorseSelect/Detail/WBP_CourseMap_Detail_Par.WBP_CourseMap_Detail_Par_C"))
	);
}

void UCourseSelectDetailWidget::NativeConstruct()
{
	Super::NativeConstruct();

	GM = Cast<AMenuGameMode>(GetWorld()->GetAuthGameMode());
	GM->OnEnterCourseSelectDele.AddDynamic(this, &UCourseSelectDetailWidget::HandleOnEnterCourseSelect);
}

void UCourseSelectDetailWidget::Init()
{
	UCourseSelectWidget* CourseSelectWidget = Cast<UCourseSelectWidget>(GM->GetStateWidget(EUIState::CourseSelect));
	UCourseSelectMapPanelWidget* MapPanelWidget = CourseSelectWidget->WBP_CorseMap_Panel;

	for (UCourseSelectMapWidget* CourseMapWidget : MapPanelWidget->MapArray)
	{
		CourseMapWidget->OnClickCourseButtonDele.RemoveAll(this);
		CourseMapWidget->OnClickCourseButtonDele.AddDynamic(this, &UCourseSelectDetailWidget::HandleOnClickCourseButton);
		LoadBackgroundImage(CourseMapWidget->CCFolderName);
	}

	BindOptions();
	InitializePar();

	MapPanelWidget->SelectFirstOne();
	bInitialized = true;

	UE_LOG(LogTemp, Log, TEXT("UCourseSelectDetailWidget::Init() ==> Succeeded"));
}

void UCourseSelectDetailWidget::BindOptions()
{
	// 스피너 방식: HorizontalBox당 UCourseSelectOptionWidget 1개
	auto BindOne = [this](UHorizontalBox* Box)
		{
			if (!Box) return;
			for (UWidget* W : Box->GetAllChildren())
			{
				if (UCourseSelectOptionWidget* Opt = Cast<UCourseSelectOptionWidget>(W))
				{
					Opt->OnClickOptionDele.AddDynamic(this, &UCourseSelectDetailWidget::HandleOnClickOption);
				}
			}
		};

	BindOne(HorizontalBox_SelectCourse);
	BindOne(HorizontalBox_Muligan);
	BindOne(HorizontalBox_PinLocation);
	BindOne(HorizontalBox_Concede);
	BindOne(HorizontalBox_GrassCondition);
	BindOne(HorizontalBox_ContinuePutting);
	BindOne(HorizontalBox_CameraMode);
	BindOne(HorizontalBox_SwingMotion);
}

void UCourseSelectDetailWidget::HandleOnEnterCourseSelect()
{
	if (!bInitialized)
		Init();
}

void UCourseSelectDetailWidget::HandleOnClickOption(EGameOption OptionType, int32 OptionValue)
{
	FGameInfo CachedGameInfo = GM->GetGameInfo();

	switch (OptionType)
	{
	case EGameOption::SubLevel:        CachedGameInfo.GameOptions.SelectCourse = OptionValue; break;
	case EGameOption::Mulligan:        CachedGameInfo.GameOptions.Mulligan_Count = OptionValue; break;
	case EGameOption::PinLocation:     CachedGameInfo.GameOptions.Holecup_Position = OptionValue; break;
	case EGameOption::Concede:         CachedGameInfo.GameOptions.Concede_Distance = OptionValue; break;
	case EGameOption::ContinuePutting: CachedGameInfo.GameOptions.ContinuePutting = OptionValue; break;
	case EGameOption::GrassCondition:  CachedGameInfo.GameOptions.Green_Speed = OptionValue; break;
	case EGameOption::CameraMode:      CachedGameInfo.GameOptions.Camera_Mode = OptionValue; break;
	case EGameOption::SwingMotion:     CachedGameInfo.GameOptions.SwingMotion = OptionValue; break;
	case EGameOption::PracticeMode:    CachedGameInfo.GameOptions.PracticeMode = OptionValue; break;
	}

	GM->SetGameInfo(CachedGameInfo);
}

void UCourseSelectDetailWidget::HandleOnClickCourseButton(FFieldMapInfo FieldMapInfo, FString CCFolderName)
{
	TextBlock_Name->SetText(FText::FromString(FieldMapInfo.CCname));
	TextBlock_Distance->SetText(FText::FromString(FieldMapInfo.Address));

	AsyncTask(ENamedThreads::GameThread, [this, CCFolderName]()
		{
			if (!IsValid(this) || !IsValid(Image_CourseTitle)) return;

			if (UTexture2D** FoundPtr = BackgroundImages.Find(CCFolderName))
			{
				UTexture2D* Tex = *FoundPtr;
				if (!IsValid(Tex))
				{
					UE_LOG(LogTemp, Error, TEXT("Invalid Texture: %s"), *CCFolderName);
					return;
				}

				if (UWorld* World = GetWorld())
				{
					World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, Tex]()
						{
							if (!IsValid(this) || !IsValid(Image_CourseTitle) || !IsValid(Tex)) return;
							Tex->UpdateResource();
							Image_CourseTitle->SetBrushFromTexture(Tex, true);
						}));
				}
			}
		});

	UpdateStar();
	UpdatePar();
}

bool UCourseSelectDetailWidget::LoadBackgroundImage(FString CCName)
{
	FString ImagePath = FString::Printf(TEXT("%sDATA/CourseMap/%s/image1.png"), *FPaths::ProjectContentDir(), *CCName);
	FString Err;
	if (UTexture2D* Tex = ULoadTexture2DFromFileAsync::LoadTexture2DFromFileSync(ImagePath, &Err))
	{
		BackgroundImages.Add(CCName, Tex);
		return true;
	}
	return false;
}

void UCourseSelectDetailWidget::InitializePar()
{
	UClass* LoadedClass = ParWidgetSoftClass.LoadSynchronous();

	for (int32 i = 0; i < 9; i++)
	{
		UCourseSelectDetailParWidget* ParWidget = CreateWidget<UCourseSelectDetailParWidget>(this, LoadedClass);
		HorizontalBox_APar->AddChildToHorizontalBox(ParWidget);
	}

	for (int32 i = 0; i < 9; i++)
	{
		UCourseSelectDetailParWidget* ParWidget = CreateWidget<UCourseSelectDetailParWidget>(this, LoadedClass);
		HorizontalBox_BPar->AddChildToHorizontalBox(ParWidget);
	}
}

void UCourseSelectDetailWidget::InitializeOption()
{
}

void UCourseSelectDetailWidget::UpdateStar()
{
	if (!GM) return;

	UCourseSelectWidget* CourseSelectWidget = Cast<UCourseSelectWidget>(GM->GetStateWidget(EUIState::CourseSelect));
	UCourseSelectMapWidget* MapWidget = CourseSelectWidget->WBP_CorseMap_Panel->GetSelectedMapWidget();
	if (!MapWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("UpdateStar: Can't find selected course map widget"));
		return;
	}

	int32 Difficulty = MapWidget->FieldMapInfo.CourseLevel;
	TArray<UWidget*> Stars = HorizontalBox_Stars->GetAllChildren();

	for (int32 i = 0; i < Stars.Num(); i++)
	{
		Stars[i]->SetVisibility(
			i < Difficulty
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed
		);
	}
}

void UCourseSelectDetailWidget::UpdatePar()
{
	if (!GM) return;

	UCourseSelectWidget* CourseSelectWidget = Cast<UCourseSelectWidget>(GM->GetStateWidget(EUIState::CourseSelect));
	UCourseSelectMapWidget* MapWidget = CourseSelectWidget->WBP_CorseMap_Panel->GetSelectedMapWidget();
	if (!MapWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("UpdatePar: Can't find selected course map widget"));
		return;
	}

	int32 OutTotalPar = 0;
	int32 InTotalPar = 0;

	// A코스 (홀 0~8)
	TArray<UWidget*> AParWidgets = HorizontalBox_APar->GetAllChildren();
	for (int32 i = 0; i < AParWidgets.Num(); i++)
	{
		if (!MapWidget->FieldMapInfo.HoleInfos.IsValidIndex(i))
		{
			UE_LOG(LogTemp, Error, TEXT("%s: A코스 HoleInfos[%d] 없음"), *MapWidget->CCFolderName, i);
			continue;
		}

		int32 ParCount = MapWidget->FieldMapInfo.HoleInfos[i].ParCount;
		UCourseSelectDetailParWidget* ParWidget = Cast<UCourseSelectDetailParWidget>(AParWidgets[i]);

		// ✅ 수정: 1-based 홀 번호 (i+1)
		ParWidget->TextBlock_Par_Index->SetText(FText::AsNumber(i + 1));
		ParWidget->TextBlock_Par_Count->SetText(FText::AsNumber(ParCount));
		OutTotalPar += ParCount;
	}

	// B코스 (홀 9~17)
	TArray<UWidget*> BParWidgets = HorizontalBox_BPar->GetAllChildren();
	for (int32 i = 0; i < BParWidgets.Num(); i++)
	{
		const int32 HoleIndex = i + 9;
		if (!MapWidget->FieldMapInfo.HoleInfos.IsValidIndex(HoleIndex))
		{
			UE_LOG(LogTemp, Error, TEXT("%s: B코스 HoleInfos[%d] 없음"), *MapWidget->CCFolderName, HoleIndex);
			continue;
		}

		int32 ParCount = MapWidget->FieldMapInfo.HoleInfos[HoleIndex].ParCount;
		UCourseSelectDetailParWidget* ParWidget = Cast<UCourseSelectDetailParWidget>(BParWidgets[i]);

		// ✅ 수정: 1-based 홀 번호 (i+1)
		ParWidget->TextBlock_Par_Index->SetText(FText::AsNumber(i + 1));
		ParWidget->TextBlock_Par_Count->SetText(FText::AsNumber(ParCount));
		InTotalPar += ParCount;
	}

	TextBlock_OutCourse_Name->SetText(FText::FromString(MapWidget->FieldMapInfo.OutCourse));
	TextBlock_InCourse_Name->SetText(FText::FromString(MapWidget->FieldMapInfo.InCourse));
	TextBlock_OutCourse_Total->SetText(FText::AsNumber(OutTotalPar));
	TextBlock_InCourse_Total->SetText(FText::AsNumber(InTotalPar));
}