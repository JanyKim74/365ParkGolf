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
	if (!NewTex) return;

	// 크기(ImageSize) 등은 건드리지 않고, 리소스만 교체
	Brush.SetResourceObject(NewTex);

	// 필요 시(기존 DrawAs가 None이어서 안 보일 때만) Image로 보정
	if (Brush.DrawAs == ESlateBrushDrawType::NoDrawType)
	{
		Brush.DrawAs = ESlateBrushDrawType::Image;
	}
}

void UCourseSelectMapWidget::UpdateCourseMapPanelImage()
{
	FSlateBrush Brush;

	// 기존 스타일을 복사해서 일부만 교체 (중요)
	FButtonStyle Style = Button_CourseMap->GetStyle();

	if (!Button_CourseMap) return;

	FSlateBrush Normal = Style.Normal;
	FSlateBrush Hovered = Style.Hovered;
	FSlateBrush Pressed = Style.Pressed;

	if (bIsSelected)
	{
		ReplaceBrushTextureKeepSize(Normal, OnImage);
		ReplaceBrushTextureKeepSize(Hovered, OnImage);
		ReplaceBrushTextureKeepSize(Pressed, OnImage);
	}
	else
	{
		ReplaceBrushTextureKeepSize(Normal, OffImage);
		ReplaceBrushTextureKeepSize(Hovered, OffImage);
		ReplaceBrushTextureKeepSize(Pressed, OffImage);
	}

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