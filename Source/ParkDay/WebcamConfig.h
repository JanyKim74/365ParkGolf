#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WebcamConfig.generated.h"

/**
 * 웹캠 설정을 저장하는 구조체
 */
USTRUCT(BlueprintType)
struct FWebcamSettings
{
    GENERATED_BODY()

        // 웹캠 소스 URL (예: "video=0", "vidcap://...")
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Webcam")
        FString WebcamURL = TEXT("video=0");

    // 해상도 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Webcam")
        int32 Width = 640;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Webcam")
        int32 Height = 480;

    // 프레임레이트
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Webcam")
        int32 FPS = 60;

    // Material 경로
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Webcam")
        FString MaterialPath = TEXT("/Game/Media/M_MediaDisplay");

    // Widget 클래스 경로
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
        FString WidgetClassPath = TEXT("/Game/UI/WBP_SwingVideoWidget");

    // 자동 시작 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Webcam")
        bool bAutoStart = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Webcam")
        bool bEnableVideoSaving = false;

    // 여기부터 추가된 필드들 (중요!)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Webcam")
        FString WebcamName = TEXT("");        // 웹캠 이름 (예: "Logitech C920")

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Webcam")
        FString WebcamVID = TEXT("");         // VID (예: 0C45)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Webcam")
        FString WebcamPID = TEXT("");         // PID (예: 64AB)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Webcam")
        int32 WebcamIndex = 0;                // 같은 VID/PID 중 몇 번째 장치인지

        // 전체 URL 생성 (해상도와 FPS 포함)
    FString GetFullWebcamURL() const
    {
        return FString::Printf(TEXT("%s:width=%d:height=%d:fps=%d"),
            *WebcamURL, Width, Height, FPS);
    }
};
/**
 * 웹캠 설정을 JSON 파일로 저장/로드하는 클래스
 */
UCLASS(BlueprintType)
class PARKDAY_API UWebcamConfigLoader : public UObject
{
    GENERATED_BODY()

public:
    // JSON 파일에서 설정 로드
    UFUNCTION(BlueprintCallable, Category = "Webcam Config")
        static bool LoadConfigFromJSON(const FString& FilePath, FWebcamSettings& OutSettings);

    // 설정을 JSON 파일로 저장
    UFUNCTION(BlueprintCallable, Category = "Webcam Config")
        static bool SaveConfigToJSON(const FString& FilePath, const FWebcamSettings& Settings);

    // 기본 설정 파일 경로 가져오기
    UFUNCTION(BlueprintCallable, Category = "Webcam Config")
        static FString GetDefaultConfigPath();

    // 설정 파일 존재 확인
    UFUNCTION(BlueprintCallable, Category = "Webcam Config")
        static bool ConfigFileExists(const FString& FilePath);

    // 기본 설정 생성
    UFUNCTION(BlueprintCallable, Category = "Webcam Config")
        static FWebcamSettings GetDefaultSettings();
};

/**
 * Data Asset으로 설정을 관리하는 클래스 (에디터용)
 */
UCLASS(BlueprintType)
class PARKDAY_API UWebcamConfigAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Webcam Settings")
        FWebcamSettings Settings;
};