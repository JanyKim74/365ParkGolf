#include "LoadTexture2DFromFileAsync.h"
#include "Async/Async.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Rendering/Texture2DResource.h"

// --- 내부 헬퍼: 파일 → BGRA8 디코드 -------------------------------------------------
bool ULoadTexture2DFromFileAsync::DecodeImageFileToBGRA(const FString& Path, int32& OutWidth, int32& OutHeight, TArray<uint8>& OutBGRA, FString& OutError)
{
	OutWidth = 0; OutHeight = 0; OutBGRA.Reset(); OutError.Reset();

	if (!FPaths::FileExists(Path))
	{
		OutError = FString::Printf(TEXT("파일이 존재하지 않습니다: %s"), *Path);
		return false;
	}

	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *Path))
	{
		OutError = FString::Printf(TEXT("파일 읽기 실패: %s"), *Path);
		return false;
	}

	const FString Ext = FPaths::GetExtension(Path).ToLower();
	EImageFormat ImgFmt = EImageFormat::Invalid; // UE4.26: enum class EImageFormat
	if (Ext == TEXT("png")) ImgFmt = EImageFormat::PNG;
	else if (Ext == TEXT("jpg") || Ext == TEXT("jpeg")) ImgFmt = EImageFormat::JPEG;
	else if (Ext == TEXT("bmp")) ImgFmt = EImageFormat::BMP; // 선택적 지원
	else
	{
		OutError = FString::Printf(TEXT("지원하지 않는 확장자: .%s"), *Ext);
		return false;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> Wrapper = ImageWrapperModule.CreateImageWrapper(ImgFmt);
	if (!Wrapper.IsValid())
	{
		OutError = TEXT("ImageWrapper 생성 실패");
		return false;
	}

	if (!Wrapper->SetCompressed(FileData.GetData(), FileData.Num()))
	{
		OutError = TEXT("이미지 압축 해제(SetCompressed) 실패");
		return false;
	}

	if (!Wrapper->GetRaw(ERGBFormat::BGRA, 8, OutBGRA) || OutBGRA.Num() == 0)
	{
		OutError = TEXT("Raw BGRA 추출 실패");
		return false;
	}

	OutWidth = Wrapper->GetWidth();
	OutHeight = Wrapper->GetHeight();
	if (OutWidth <= 0 || OutHeight <= 0)
	{
		OutError = TEXT("잘못된 이미지 크기");
		OutBGRA.Reset();
		return false;
	}

	return true;
}

// --- 내부 헬퍼: BGRA8 → UTexture2D ---------------------------------------------------
UTexture2D* ULoadTexture2DFromFileAsync::CreateTextureFromBGRA(const TArray<uint8>& BGRA, int32 Width, int32 Height, bool bSRGB, bool bNeverStream)
{
	if (Width <= 0 || Height <= 0) return nullptr;
	const int64 Expected = (int64)Width * Height * 4;
	if (BGRA.Num() < Expected) return nullptr;

	UTexture2D* Tex = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
	if (!Tex || !Tex->GetPlatformData() || Tex->GetPlatformData()->Mips.Num() == 0) return nullptr;

	UTexture* Base = static_cast<UTexture*>(Tex);
#if WITH_EDITORONLY_DATA
	Base->MipGenSettings = TMGS_NoMipmaps; // 필요 시 변경
#endif
	Base->SRGB = bSRGB;
	Base->CompressionSettings = TC_Default;
	Base->NeverStream = bNeverStream;
	Base->Filter = TF_Default;

	void* MipData = Tex->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	if (!MipData) return nullptr;
	FMemory::Memcpy(MipData, BGRA.GetData(), Expected);
	Tex->GetPlatformData()->Mips[0].BulkData.Unlock();

	Tex->UpdateResource();
	return Tex;
}

// --- Blueprint Async 액션 -------------------------------------------------------------
ULoadTexture2DFromFileAsync* ULoadTexture2DFromFileAsync::LoadTexture2DFromFileAsync(const UObject* WorldContextObject, const FString& InFilePath)
{
	ULoadTexture2DFromFileAsync* Node = NewObject<ULoadTexture2DFromFileAsync>();
	Node->FilePath = InFilePath;
	Node->WorldContextObjectPtr = WorldContextObject;
	Node->AddToRoot();
	return Node;
}

void ULoadTexture2DFromFileAsync::Activate()
{
	const FString Path = FilePath;

	Async(EAsyncExecution::ThreadPool, [this, Path]()
		{
			int32 W = 0, H = 0; TArray<uint8> BGRA; FString Err;
			if (!DecodeImageFileToBGRA(Path, W, H, BGRA, Err))
			{
				UE_LOG(LogTemp, Warning, TEXT("[LoadTexture2DFromFileAsync] %s"), *Err);
				AsyncTask(ENamedThreads::GameThread, [this]()
					{
						OnFailure.Broadcast();
						RemoveFromRoot();
					});
				return;
			}

			AsyncTask(ENamedThreads::GameThread, [this, Path, W, H, Data = MoveTemp(BGRA)]() mutable
			{
				if (UTexture2D* Tex = CreateTextureFromBGRA(Data, W, H))
				{
					OnSuccess.Broadcast(Path, Tex);
				}
				else
				{
					OnFailure.Broadcast();
				}
				RemoveFromRoot();
			});
		});
}

// --- C++ 비동기 편의 함수 ------------------------------------------------------------
void ULoadTexture2DFromFileAsync::LoadTexture2DFromFileAsync_CPP(
	const FString& FilePath,
	TFunction<void(const FString&, UTexture2D*)> OnOk,
	TFunction<void()> OnFail)
{
	Async(EAsyncExecution::ThreadPool, [FilePath, OnOk = MoveTemp(OnOk), OnFail = MoveTemp(OnFail)]() mutable
	{
		int32 W = 0, H = 0; TArray<uint8> BGRA; FString Err;
		if (!DecodeImageFileToBGRA(FilePath, W, H, BGRA, Err))
		{
			UE_LOG(LogTemp, Warning, TEXT("[LoadTexture2DFromFileAsync_CPP] %s"), *Err);
			AsyncTask(ENamedThreads::GameThread, [OnFail = MoveTemp(OnFail)]() mutable
			{
				if (OnFail) OnFail();
			});
			return;
		}

		AsyncTask(ENamedThreads::GameThread, [FilePath, W, H, Data = MoveTemp(BGRA), OnOk = MoveTemp(OnOk), OnFail = MoveTemp(OnFail)]() mutable
		{
			if (UTexture2D* Tex = CreateTextureFromBGRA(Data, W, H))
			{
				if (OnOk) OnOk(FilePath, Tex);
			}
			else
			{
				if (OnFail) OnFail();
			}
		});
	});
}

// --- C++ 동기 편의 함수 --------------------------------------------------------------
UTexture2D* ULoadTexture2DFromFileAsync::LoadTexture2DFromFileSync(const FString& FilePath, FString* OutError)
{
	int32 W = 0, H = 0; TArray<uint8> BGRA; FString Err;
	if (!DecodeImageFileToBGRA(FilePath, W, H, BGRA, Err))
	{
		if (OutError) *OutError = MoveTemp(Err);
		return nullptr;
	}

	if (UTexture2D* Tex = CreateTextureFromBGRA(BGRA, W, H))
	{
		return Tex;
	}

	if (OutError) *OutError = TEXT("텍스처 생성 실패");
	return nullptr;
}

/*
// ============================= Build.cs 참고 =============================
// .Build.cs 내 의존성 (예)
PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
PrivateDependencyModuleNames.AddRange(new string[] { "ImageWrapper" });

// ============================= 사용 예시 =============================
// 1) C++ 비동기
ULoadTexture2DFromFileAsync::LoadTexture2DFromFileAsync_CPP(
	MyImagePath,
	[](const FString& Path, UTexture2D* Tex)
	{
		UE_LOG(LogTemp, Log, TEXT("로드 성공: %s (%dx%d)"), *Path, Tex->GetSizeX(), Tex->GetSizeY());
		// 예: UImage->SetBrushFromTexture(Tex);
	},
	[]()
	{
		UE_LOG(LogTemp, Warning, TEXT("이미지 로드 실패"));
	});

// 2) C++ 동기 (GameThread 권장)
FString Err;
if (UTexture2D* Tex = ULoadTexture2DFromFileAsync::LoadTexture2DFromFileSync(MyImagePath, &Err))
{
	// 사용
}
else
{
	UE_LOG(LogTemp, Warning, TEXT("실패: %s"), *Err);
}
*/
