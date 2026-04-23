#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Engine/Texture2D.h"
#include "LoadTexture2DFromFileAsync.generated.h"

// 성공: 로드한 경로와 텍스처를 전달
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLoadTexture2DFromFileSuccess, FString, Path, UTexture2D*, LoadedTexture);
// 실패: 단순 알림 (필요 시 에러메시지 확장 가능)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLoadTexture2DFromFileFailure);

/**
 * PNG/JPG 파일을 비동기 로드하여 UTexture2D를 만드는 Blueprint Async 액션.
 * + C++에서 직접 쓸 수 있는 동기/비동기 정적 함수도 함께 제공.
 */
UCLASS(BlueprintType)
class PARKDAY_API ULoadTexture2DFromFileAsync : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Blueprint에서 호출할 Async 노드 (노드 이름: "Load Texture2D From File Async") */
	UFUNCTION(
		BlueprintCallable,
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Load Texture2D From File Async"),
		Category = "Async|Texture")
		static ULoadTexture2DFromFileAsync* LoadTexture2DFromFileAsync(const UObject* WorldContextObject, const FString& FilePath);

	// UBlueprintAsyncActionBase
	virtual void Activate() override;

	/** 성공 델리게이트 (Blueprint 바인딩용) */
	UPROPERTY(BlueprintAssignable)
		FOnLoadTexture2DFromFileSuccess OnSuccess;

	/** 실패 델리게이트 (Blueprint 바인딩용) */
	UPROPERTY(BlueprintAssignable)
		FOnLoadTexture2DFromFileFailure OnFailure;

	// =============================
	// C++ 전용 편의 API
	// =============================

	/**
	 * C++ 비동기 로드: 작업은 ThreadPool에서 수행되고 결과는 GameThread로 콜백됩니다.
	 * @param FilePath  로드할 파일 절대/상대 경로
	 * @param OnOk      성공 시 호출 (경로, 생성된 텍스처)
	 * @param OnFail    실패 시 호출
	 */
	static void LoadTexture2DFromFileAsync_CPP(
		const FString& FilePath,
		TFunction<void(const FString&, UTexture2D*)> OnOk,
		TFunction<void()> OnFail);

	/**
	 * C++ 동기 로드: 호출 스레드에서 바로 디코드/생성합니다.
	 * GameThread에서 호출하는 것을 권장합니다.
	 * @param FilePath  로드할 파일 경로
	 * @param OutError  실패 사유 (옵션)
	 * @return 성공 시 텍스처, 실패 시 nullptr
	 */
	static UTexture2D* LoadTexture2DFromFileSync(const FString& FilePath, FString* OutError = nullptr);

private:
	// 공통 구현을 위한 헬퍼들 (내부 사용)
	static bool DecodeImageFileToBGRA(const FString& Path, int32& OutWidth, int32& OutHeight, TArray<uint8>& OutBGRA, FString& OutError);
	static UTexture2D* CreateTextureFromBGRA(const TArray<uint8>& BGRA, int32 Width, int32 Height, bool bSRGB = true, bool bNeverStream = true);

private:
	// Async 노드 인스턴스 상태
	FString FilePath;
	TWeakObjectPtr<const UObject> WorldContextObjectPtr;
};

