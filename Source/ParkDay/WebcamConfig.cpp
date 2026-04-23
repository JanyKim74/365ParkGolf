#include "WebcamConfig.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"

// JSON 파일에서 설정 로드
bool UWebcamConfigLoader::LoadConfigFromJSON(const FString& FilePath, FWebcamSettings& OutSettings)
{
    FString JsonString;

    // 파일 읽기
    if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("📹 Failed to load config file: %s"), *FilePath);
        return false;
    }

    // JSON 파싱
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("📹 Failed to parse JSON config file"));
        return false;
    }

    // 설정 읽기
    if (JsonObject->HasField(TEXT("WebcamURL")))
    {
        OutSettings.WebcamURL = JsonObject->GetStringField(TEXT("WebcamURL"));
    }

    if (JsonObject->HasField(TEXT("WebcamName")))
    {
        OutSettings.WebcamName = JsonObject->GetStringField(TEXT("WebcamName"));
    }

    if (JsonObject->HasField(TEXT("WebcamVID")))
    {
        OutSettings.WebcamVID = JsonObject->GetStringField(TEXT("WebcamVID"));
    }

    if (JsonObject->HasField(TEXT("WebcamPID")))
    {
        OutSettings.WebcamPID = JsonObject->GetStringField(TEXT("WebcamPID"));
    }

    if (JsonObject->HasField(TEXT("WebcamIndex")))
    {
        OutSettings.WebcamIndex = JsonObject->GetIntegerField(TEXT("WebcamIndex"));
    }

    if (JsonObject->HasField(TEXT("Width")))
    {
        OutSettings.Width = JsonObject->GetIntegerField(TEXT("Width"));
    }

    if (JsonObject->HasField(TEXT("Height")))
    {
        OutSettings.Height = JsonObject->GetIntegerField(TEXT("Height"));
    }

    if (JsonObject->HasField(TEXT("FPS")))
    {
        OutSettings.FPS = JsonObject->GetIntegerField(TEXT("FPS"));
    }

    if (JsonObject->HasField(TEXT("MaterialPath")))
    {
        OutSettings.MaterialPath = JsonObject->GetStringField(TEXT("MaterialPath"));
    }

    if (JsonObject->HasField(TEXT("WidgetClassPath")))
    {
        OutSettings.WidgetClassPath = JsonObject->GetStringField(TEXT("WidgetClassPath"));
    }

    if (JsonObject->HasField(TEXT("AutoStart")))
    {
        OutSettings.bAutoStart = JsonObject->GetBoolField(TEXT("AutoStart"));
    }

    if (JsonObject->HasField(TEXT("EnableVideoSaving")))
    {
        OutSettings.bEnableVideoSaving = JsonObject->GetBoolField(TEXT("EnableVideoSaving"));
    }

    UE_LOG(LogTemp, Log, TEXT("📹 Config loaded successfully from: %s"), *FilePath);
    UE_LOG(LogTemp, Log, TEXT("   WebcamURL: %s"), *OutSettings.WebcamURL);
    UE_LOG(LogTemp, Log, TEXT("   WebcamName: %s"), *OutSettings.WebcamName);
    UE_LOG(LogTemp, Log, TEXT("   Resolution: %dx%d @ %dfps"),
        OutSettings.Width, OutSettings.Height, OutSettings.FPS);

    return true;
}

// 설정을 JSON 파일로 저장
bool UWebcamConfigLoader::SaveConfigToJSON(const FString& FilePath, const FWebcamSettings& Settings)
{
    // JSON 객체 생성
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

    JsonObject->SetStringField(TEXT("WebcamURL"), Settings.WebcamURL);
    JsonObject->SetStringField(TEXT("WebcamName"), Settings.WebcamName);
    JsonObject->SetStringField(TEXT("WebcamVID"), Settings.WebcamVID);
    JsonObject->SetStringField(TEXT("WebcamPID"), Settings.WebcamPID);
    JsonObject->SetNumberField(TEXT("WebcamIndex"), Settings.WebcamIndex);
    JsonObject->SetNumberField(TEXT("Width"), Settings.Width);
    JsonObject->SetNumberField(TEXT("Height"), Settings.Height);
    JsonObject->SetNumberField(TEXT("FPS"), Settings.FPS);
    JsonObject->SetStringField(TEXT("MaterialPath"), Settings.MaterialPath);
    JsonObject->SetStringField(TEXT("WidgetClassPath"), Settings.WidgetClassPath);
    JsonObject->SetBoolField(TEXT("AutoStart"), Settings.bAutoStart);
    JsonObject->SetBoolField(TEXT("EnableVideoSaving"), Settings.bEnableVideoSaving);

    // JSON 문자열로 변환
    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);

    if (!FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer))
    {
        UE_LOG(LogTemp, Error, TEXT("📹 Failed to serialize config to JSON"));
        return false;
    }

    // 파일로 저장
    if (!FFileHelper::SaveStringToFile(JsonString, *FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("📹 Failed to save config file: %s"), *FilePath);
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("📹 Config saved successfully to: %s"), *FilePath);
    return true;
}

// 기본 설정 파일 경로 가져오기
FString UWebcamConfigLoader::GetDefaultConfigPath()
{
    // 프로젝트의 Saved/Config 폴더에 저장
    return FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("Config"),
        TEXT("SwingcamConfig.json")
    );
}

// 설정 파일 존재 확인
bool UWebcamConfigLoader::ConfigFileExists(const FString& FilePath)
{
    return FPaths::FileExists(FilePath);
}

// 기본 설정 생성
FWebcamSettings UWebcamConfigLoader::GetDefaultSettings()
{
    FWebcamSettings DefaultSettings;
    DefaultSettings.WebcamURL = TEXT("");  // MediaSource 에셋 사용 권장
    DefaultSettings.WebcamName = TEXT("");
    DefaultSettings.WebcamVID = TEXT("0C45");  // 수정: 새로운 VID 설정 (사용자 제공 정보 기반)
    DefaultSettings.WebcamPID = TEXT("64AB");  // 수정: 새로운 PID 설정 (사용자 제공 정보 기반)
    DefaultSettings.WebcamIndex = 0;
    DefaultSettings.Width = 640;  // 수정: 요청된 640x480 해상도
    DefaultSettings.Height = 480;  // 수정: 요청된 640x480 해상도
    DefaultSettings.FPS = 60;
    DefaultSettings.MaterialPath = TEXT("Game/GolfGameBluePrint/SwingAnalyzer/M_MediaTexture");
    DefaultSettings.WidgetClassPath = TEXT("/Game/GolfGameBluePrint/SwingAnalyzer/WBP_SwingAnalyzer");
    DefaultSettings.bAutoStart = false;
    DefaultSettings.bEnableVideoSaving = false;

    return DefaultSettings;
}