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
    if (!GM)
    {
        UE_LOG(LogTemp, Error, TEXT("UCourseSelectOptionWidget: GM is null"));
        return;
    }

    Button_Prev->OnPressed.AddDynamic(this, &UCourseSelectOptionWidget::HandleOnClickPrev);
    Button_Next->OnPressed.AddDynamic(this, &UCourseSelectOptionWidget::HandleOnClickNext);

    GM->OnEnterCourseSelectPostDele.AddDynamic(this, &UCourseSelectOptionWidget::HandleOnEnterCourseSelect);

    Init();
}

void UCourseSelectOptionWidget::Init()
{
    if (!GM) return;

    // ── 항상 덮어씀: BP 에디터 값이 잘못 들어있어도 코드 기준으로 재설정 ──
    switch (OptionType)
    {
    case EGameOption::SubLevel:
        OptionLabels = { TEXT("A코스"), TEXT("B코스"), TEXT("AB코스") };
        OptionValues = { 0, 1, 2 };
        break;
    case EGameOption::Mulligan:
        OptionLabels = { TEXT("0"), TEXT("1"), TEXT("2"), TEXT("3"), TEXT("4"), TEXT("5") };
        OptionValues = { 0, 1, 2, 3, 4, 5 };
        break;
    case EGameOption::PinLocation:
        OptionLabels = { TEXT("앞"), TEXT("중간"), TEXT("뒤"), TEXT("랜덤") };
        OptionValues = { 0, 1, 2, 3 };
        break;
    case EGameOption::Concede:
        OptionLabels = { TEXT("없음"), TEXT("1.0m"), TEXT("1.5m"), TEXT("2.0m") };
        OptionValues = { 0, 1, 2, 3 };
        break;
    case EGameOption::GrassCondition:
        OptionLabels = { TEXT("느림"), TEXT("보통"), TEXT("빠름") };
        OptionValues = { 0, 1, 2 };
        break;
    case EGameOption::ContinuePutting:
        OptionLabels = { TEXT("OFF"), TEXT("ON") };
        OptionValues = { 0, 1 };
        break;
    case EGameOption::CameraMode:
        OptionLabels = { TEXT("기본"), TEXT("시네마틱") };
        OptionValues = { 0, 1 };
        break;
    case EGameOption::SwingMotion:
        OptionLabels = { TEXT("OFF"), TEXT("ON") };
        OptionValues = { 0, 1 };
        break;
    default:
        UE_LOG(LogTemp, Warning, TEXT("UCourseSelectOptionWidget: Unknown OptionType"));
        return;
    }

    // 저장된 값으로 인덱스 동기화
    FGameInfo CachedGameInfo = GM->GetGameInfo();
    FGameOptionInfo& Opt = CachedGameInfo.GameOptions;

    int32 SavedValue = 0;
    switch (OptionType)
    {
    case EGameOption::SubLevel:        SavedValue = Opt.SelectCourse;             break;
    case EGameOption::Mulligan:        SavedValue = Opt.Mulligan_Count;           break;
    case EGameOption::PinLocation:     SavedValue = Opt.Holecup_Position;         break;
    case EGameOption::Concede:         SavedValue = (int32)Opt.Concede_Distance;  break;
    case EGameOption::GrassCondition:  SavedValue = (int32)Opt.Green_Speed;       break;
    case EGameOption::ContinuePutting: SavedValue = Opt.ContinuePutting;          break;
    case EGameOption::CameraMode:      SavedValue = Opt.Camera_Mode;              break;
    case EGameOption::SwingMotion:     SavedValue = Opt.SwingMotion;              break;
    default: break;
    }

    SyncToValue(SavedValue);

    UE_LOG(LogTemp, Log, TEXT("UCourseSelectOptionWidget::Init() OptionType=%d SavedValue=%d Label=%s"),
        (int32)OptionType, SavedValue,
        OptionLabels.IsValidIndex(CurrentIndex) ? *OptionLabels[CurrentIndex] : TEXT("INVALID"));
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
        UE_LOG(LogTemp, Warning, TEXT("UCourseSelectOptionWidget:RefreshDisplay"));
        TextBlock_Value->SetText(FText::FromString(OptionLabels[CurrentIndex]));
    }
}

void UCourseSelectOptionWidget::HandleOnClickPrev()
{
    if (OptionValues.Num() == 0) return;

    // 0 이하면 이동 불가 (순환 없음)
    if (CurrentIndex <= 0) return;

    SetCurrentIndex(CurrentIndex - 1);
}

void UCourseSelectOptionWidget::HandleOnClickNext()
{
    if (OptionValues.Num() == 0) return;

    // 마지막 인덱스면 이동 불가 (순환 없음)
    if (CurrentIndex >= OptionValues.Num() - 1) return;

    SetCurrentIndex(CurrentIndex + 1);;
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