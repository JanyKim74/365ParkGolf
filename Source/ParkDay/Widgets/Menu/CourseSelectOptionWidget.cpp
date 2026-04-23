#include "CourseSelectOptionWidget.h"
#include "CourseSelectMapWidget.h"
#include "CourseSelectMapPanelWidget.h"
#include "CourseSelectWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "../../MenuGameMode.h"

void UCourseSelectOptionWidget::NativeConstruct()
{
    Super::NativeConstruct();

    GM = Cast<AMenuGameMode>(GetWorld()->GetAuthGameMode());

    Button_Prev->OnPressed.AddDynamic(this, &UCourseSelectOptionWidget::HandleOnClickPrev);
    Button_Next->OnPressed.AddDynamic(this, &UCourseSelectOptionWidget::HandleOnClickNext);

    GM->OnEnterCourseSelectPostDele.AddDynamic(this, &UCourseSelectOptionWidget::HandleOnEnterCourseSelect);

    Init();
}

void UCourseSelectOptionWidget::Init()
{
    if (!GM || OptionValues.Num() == 0) return;

    // 저장된 GameInfo 값과 일치하는 인덱스로 초기화
    FGameInfo CachedGameInfo = GM->GetGameInfo();
    FGameOptionInfo& Opt = CachedGameInfo.GameOptions;

    int32 SavedValue = 0;
    switch (OptionType)
    {
    case EGameOption::SubLevel:        SavedValue = Opt.SelectCourse;      break;
    case EGameOption::Mulligan:        SavedValue = Opt.Mulligan_Count;    break;
    case EGameOption::PinLocation:     SavedValue = Opt.Holecup_Position;  break;
    case EGameOption::Concede:         SavedValue = Opt.Concede_Distance;  break;
    case EGameOption::GrassCondition:  SavedValue = Opt.Green_Speed;       break;
    case EGameOption::ContinuePutting: SavedValue = Opt.ContinuePutting;   break;
    case EGameOption::CameraMode:      SavedValue = Opt.Camera_Mode;       break;
    case EGameOption::SwingMotion:     SavedValue = Opt.SwingMotion;       break;
    default: break;
    }

    SyncToValue(SavedValue);
}

void UCourseSelectOptionWidget::SyncToValue(int32 Value)
{
    CurrentIndex = 0;
    for (int32 i = 0; i < OptionValues.Num(); i++)
    {
        if (OptionValues[i] == Value)
        {
            CurrentIndex = i;
            break;
        }
    }
    RefreshDisplay();
}

void UCourseSelectOptionWidget::SetCurrentIndex(int32 NewIndex)
{
    if (OptionValues.Num() == 0) return;

    CurrentIndex = FMath::Clamp(NewIndex, 0, OptionValues.Num() - 1);
    RefreshDisplay();

    // 값 변경 브로드캐스트
    OnClickOptionDele.Broadcast(OptionType, OptionValues[CurrentIndex]);
}

void UCourseSelectOptionWidget::RefreshDisplay()
{
    if (OptionLabels.IsValidIndex(CurrentIndex))
    {
        TextBlock_Value->SetText(FText::FromString(OptionLabels[CurrentIndex]));
    }
}

void UCourseSelectOptionWidget::HandleOnClickPrev()
{
    if (OptionValues.Num() == 0) return;
    // 순환: 0에서 ◄ 누르면 마지막 인덱스로
    const int32 NewIndex = (CurrentIndex - 1 + OptionValues.Num()) % OptionValues.Num();
    SetCurrentIndex(NewIndex);
}

void UCourseSelectOptionWidget::HandleOnClickNext()
{
    if (OptionValues.Num() == 0) return;
    // 순환: 마지막에서 ► 누르면 0으로
    const int32 NewIndex = (CurrentIndex + 1) % OptionValues.Num();
    SetCurrentIndex(NewIndex);
}

void UCourseSelectOptionWidget::HandleOnEnterCourseSelect()
{
    BindCourseMap();
    UpdateSublevelName();
}

void UCourseSelectOptionWidget::HandleOnClickCourseMap(FFieldMapInfo FieldMapInfo, FString CCFolderName)
{
    // 코스 변경 시 SubLevel 이름 갱신
    UpdateSublevelName();
}

void UCourseSelectOptionWidget::UpdateSublevelName()
{
    if (OptionType != EGameOption::SubLevel) return;

    UCourseSelectWidget* CourseWidget = Cast<UCourseSelectWidget>(GM->GetStateWidget(EUIState::CourseSelect));
    if (!CourseWidget) return;

    UCourseSelectMapWidget* SelectedMap = CourseWidget->WBP_CorseMap_Panel->GetSelectedMapWidget();
    if (!SelectedMap) return;

    // OptionLabels[0] = OutCourse 이름, OptionLabels[1] = InCourse 이름
    if (OptionLabels.IsValidIndex(0))
        OptionLabels[0] = SelectedMap->FieldMapInfo.OutCourse;
    if (OptionLabels.IsValidIndex(1))
        OptionLabels[1] = SelectedMap->FieldMapInfo.InCourse;

    // 현재 표시 갱신
    RefreshDisplay();
}

void UCourseSelectOptionWidget::BindCourseMap()
{
    UCourseSelectWidget* CourseSelectWidget = Cast<UCourseSelectWidget>(GM->GetStateWidget(EUIState::CourseSelect));
    if (!CourseSelectWidget) return;

    UCourseSelectMapPanelWidget* MapPanelWidget = CourseSelectWidget->WBP_CorseMap_Panel;
    for (UCourseSelectMapWidget* CourseMapWidget : MapPanelWidget->MapArray)
    {
        // 중복 바인딩 방지
        CourseMapWidget->OnClickCourseButtonDele.RemoveAll(this);
        CourseMapWidget->OnClickCourseButtonDele.AddDynamic(this, &UCourseSelectOptionWidget::HandleOnClickCourseMap);
    }
}