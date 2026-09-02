#include "CourseSelectMapWidget.h"
#include "Kismet/GameplayStatics.h" // 예시용, 필요시 포함
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/HorizontalBox.h"
#include "Components/Button.h"

#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "../../MenuGameMode.h"
#include "ParkDay/SoundManager.h"
#include "../../Utils/JsonLoader.h"
#include "../../Utils/LoadTexture2DFromFileAsync.h"
#include "../../DataAsset/MenuUIImageDataAsset.h"
#include "ParkDay/Utils/UtilLibrary.h"


void UCourseSelectMapWidget::NativeOnInitialized()
{
}

void UCourseSelectMapWidget::NativeConstruct()
{
	Super::NativeConstruct();

	GM = Cast<AMenuGameMode>(GetWorld()->GetAuthGameMode());
	if (!IsValid(GM) || !IsValid(GM->DA_MenuUI))
	{
		UE_LOG(LogTemp, Error, TEXT("CourseSelectMapWidget::NativeConstruct ==> GM 또는 DA_MenuUI 무효"));
		return;
	}
	if (IsValid(Button_CourseMap))
		Button_CourseMap->OnPressed.AddDynamic(this, &UCourseSelectMapWidget::HandleOnClickCourseMap);

	OffImage = GM->DA_MenuUI->GetUIImage(TEXT("CourseSelect.Map.Panel.Off"));
	OnImage = GM->DA_MenuUI->GetUIImage(TEXT("CourseSelect.Map.Panel.On"));
}


void UCourseSelectMapWidget::Init(FString CCName)
{
	if (LoadBackgroundImage(CCName))
	{
		if (LoadFieldMapInfo(CCName))
		{
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UCourseSelectMapWidget::NativeConstruct() ==> Fail to load FieldMapInfo from json"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("CourseSelectMapWidget::NativeConstruct() ==> Fail to load Image from json"));
	}

	GM = Cast<AMenuGameMode>(GetWorld()->GetAuthGameMode());

	Button_CourseMap->OnPressed.AddDynamic(this, &UCourseSelectMapWidget::HandleOnClickCourseMap);

	OffImage = GM->DA_MenuUI->GetUIImage(TEXT("CourseSelect.Map.Panel.Off"));
	OnImage = GM->DA_MenuUI->GetUIImage(TEXT("CourseSelect.Map.Panel.On"));
}

static void ReplaceBrushTextureKeepSize(FSlateBrush& Brush, UTexture2D* NewTex)
{
	// ✅ null뿐 아니라 GC-pending/무효 포인터도 차단
	if (!IsValid(NewTex)) return;
	Brush.SetResourceObject(NewTex);
	if (Brush.DrawAs == ESlateBrushDrawType::NoDrawType)
		Brush.DrawAs = ESlateBrushDrawType::Image;
}

void UCourseSelectMapWidget::UpdateCourseMapPanelImage()
{
	// ✅ 역참조 전에 체크
	if (!IsValid(Button_CourseMap)) return;

	// ✅ 텍스처가 유효할 때만 진행 (둘 다 무효면 스타일 교체 자체를 건너뜀)
	const bool bOnValid = IsValid(OnImage);
	const bool bOffValid = IsValid(OffImage);
	if (bIsSelected && !bOnValid)  return;
	if (!bIsSelected && !bOffValid) return;

	FButtonStyle Style = Button_CourseMap->WidgetStyle;
	FSlateBrush Normal = Style.Normal;
	FSlateBrush Hovered = Style.Hovered;
	FSlateBrush Pressed = Style.Pressed;

	UTexture2D* Tex = bIsSelected ? OnImage : OffImage;
	ReplaceBrushTextureKeepSize(Normal, Tex);
	ReplaceBrushTextureKeepSize(Hovered, Tex);
	ReplaceBrushTextureKeepSize(Pressed, Tex);

	Style.SetNormal(Normal);
	Style.SetHovered(Hovered);
	Style.SetPressed(Pressed);
	Button_CourseMap->SetStyle(Style);
}

void UCourseSelectMapWidget::HandleOnClickCourseMap()
{
	UUtilLibrary::LockButtonForSeconds(Button_CourseMap, GetWorld(), 0.2f);
	if (GM->IsClickAllowed())
	{
		OnClickCourseButtonDele.Broadcast(FieldMapInfo, CCFolderName);
	}
}

void UCourseSelectMapWidget::SetBackgroundImage(UTexture2D* Texture)
{
	//Image_Backgound->SetBrushFromTexture(Texture, true);
}

void UCourseSelectMapWidget::SetMapInfo()
{
	TextBlock_Name->SetText(FText::FromString(FieldMapInfo.CCname));

	// [추가] 지역명 표시
	if (TextBlock_Address)
	{
		FString AreaName;
		switch (FieldMapInfo.Area)
		{
		case 1: AreaName = TEXT("강원도");   break;
		case 2: AreaName = TEXT("수도권");   break;
		case 3: AreaName = TEXT("경상도");   break;
		case 4: AreaName = TEXT("전라도");   break;
		case 5: AreaName = TEXT("제주도");   break;
		case 6: AreaName = TEXT("충청도");   break;
		case 7: AreaName = TEXT("가상");     break;
		case 8: AreaName = TEXT("해외");     break;
		default: AreaName = TEXT("");        break;
		}
		TextBlock_Address->SetText(FText::FromString(AreaName));
	}

	for (int32 i = 0; i < FieldMapInfo.CourseLevel; i++)
	{
		HorizontalBox_Stars->GetAllChildren()[i]->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

bool UCourseSelectMapWidget::LoadBackgroundImage(FString CCName)
{
	FString ImagePath = FString::Printf(
		TEXT("%sDATA/CourseMap/%s/image2.png"), *FPaths::ProjectContentDir(), *CCName);
	FString Err;
	if (UTexture2D* Tex = ULoadTexture2DFromFileAsync::LoadTexture2DFromFileSync(ImagePath, &Err))
	{
		//SetBackgroundImage(Tex);
		return true;
	}
	return false;
}

bool UCourseSelectMapWidget::LoadFieldMapInfo(FString CCName)
{
	// ✅ ProjectDir() 사용
	FString FieldMapInfoPath = FString::Printf(
		TEXT("DATA/CourseMap/%s/FieldMapInfo.json"), *CCName);
	UJsonLoader::LoadFieldMapInfoFromJson(FieldMapInfoPath, FieldMapInfo);

	if (UJsonLoader::LoadFieldMapInfoFromJson(FieldMapInfoPath, FieldMapInfo))
	{
		SetMapInfo();
		return true;
	}

	UE_LOG(LogTemp, Error, TEXT("❌ FieldMapInfo.json 로드 실패: %s"), *FieldMapInfoPath);
	return false;
}