#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/Engine.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/DateTime.h"
#include "AutoTeeController.generated.h"

// 키패드 버전 enum
UENUM(BlueprintType)
enum class EKeypadVersion : uint8
{
    V1 UMETA(DisplayName = "V1 (1-5 buttons)"),
    V2 UMETA(DisplayName = "V2 (2-7 buttons)"),
    V3 UMETA(DisplayName = "V3 (2-7 buttons)")
};

// 키 타입 enum
UENUM(BlueprintType)
enum class EAutoTeeKey : uint8
{
    Mulligan    UMETA(DisplayName = "멀리건"),
    Function    UMETA(DisplayName = "Function"),
    Right       UMETA(DisplayName = "Right"),
    Left        UMETA(DisplayName = "Left"),
    Up          UMETA(DisplayName = "Up"),
    Down        UMETA(DisplayName = "Down"),
    Grid        UMETA(DisplayName = "그리드")
};

// 명령 타입 enum 추가
UENUM(BlueprintType)
enum class EAutoTeeCommand : uint8
{
    None,
    SetHeight,
    ReadHeight,
    MoveHome,
    ReplaceTee,
    Initialize
};

// 명령 대기 구조체
USTRUCT()
struct FPendingCommand
{
    GENERATED_BODY()

        EAutoTeeCommand CommandType = EAutoTeeCommand::None;
    TArray<uint8> CommandData;
    int32 ExpectedResponseLength = 0;
    float TimeoutTime = 0.0f;
    int32 RequestedHeight = 0;  // SetHeight용

    FPendingCommand() {}

    FPendingCommand(EAutoTeeCommand InType, const TArray<uint8>& InData, int32 InLength, float InTimeout)
        : CommandType(InType)
        , CommandData(InData)
        , ExpectedResponseLength(InLength)
        , TimeoutTime(InTimeout)
        , RequestedHeight(0)
    {}
};

// 델리게이트 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTeeHeightChanged, int32, Height);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnKeyPressed, EAutoTeeKey, Key);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnKeyReleased, EAutoTeeKey, Key);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnConnectionStatusChanged);
// ������ �б� Ÿ�̸� ���� (50ms ����)
/**
 * AutoTee 골프 티업 장치 제어 클래스
 * RS232C 시리얼 통신으로 장치를 제어합니다.
 */
UCLASS(BlueprintType, Blueprintable)
class PARKDAY_API UAutoTeeController : public UObject
{
    GENERATED_BODY()

public:
    UAutoTeeController();
    virtual ~UAutoTeeController();
    virtual void BeginDestroy() override;

    // 시리얼 포트 연결/해제
    UFUNCTION(BlueprintCallable, Category = "AutoTee")
        bool ConnectToDevice(const FString& PortName = TEXT("COM9"), int32 BaudRate = 9600);

    UFUNCTION(BlueprintCallable, Category = "AutoTee")
        void DisconnectFromDevice();

    UFUNCTION(BlueprintCallable, Category = "AutoTee")
        bool IsConnected() const { return bIsConnected; }

    // 티 높이 제어
    UFUNCTION(BlueprintCallable, Category = "AutoTee")
        bool SetTeeHeight(int32 Height);

    UFUNCTION(BlueprintCallable, Category = "AutoTee")
        bool ReadTeeHeight();

    UFUNCTION(BlueprintCallable, Category = "AutoTee")
        int32 GetCurrentTeeHeight() const { return CurrentTeeHeight; }

    // 기본 제어 명령
    UFUNCTION(BlueprintCallable, Category = "AutoTee")
        bool MoveToHome();

    UFUNCTION(BlueprintCallable, Category = "AutoTee")
        bool ReplaceRubberTee();

    UFUNCTION(BlueprintCallable, Category = "AutoTee")
        bool InitializeAutoTee();

    // 키패드 설정
    UFUNCTION(BlueprintCallable, Category = "AutoTee")
        void SetKeypadVersion(EKeypadVersion Version) { KeypadVersion = Version; }

    UFUNCTION(BlueprintCallable, Category = "AutoTee")
        EKeypadVersion GetKeypadVersion() const { return KeypadVersion; }

    // 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "AutoTee")
        FOnTeeHeightChanged OnTeeHeightChanged;

    UPROPERTY(BlueprintAssignable, Category = "AutoTee")
        FOnKeyPressed OnKeyPressed;

    UPROPERTY(BlueprintAssignable, Category = "AutoTee")
        FOnKeyReleased OnKeyReleased;

    UPROPERTY(BlueprintAssignable, Category = "AutoTee")
        FOnConnectionStatusChanged OnConnectionStatusChanged;


    // 비동기 연결 함수
    UFUNCTION(BlueprintCallable, Category = "AutoTee")
        void ConnectToDeviceAsync(const FString& PortName = TEXT("COM9"), int32 BaudRate = 9600);

    // 연결 상태 체크
    UFUNCTION(BlueprintPure, Category = "AutoTee")
        bool IsConnecting() const { return bIsConnecting; }


    // ⭐ 비동기 명령 전송
    UFUNCTION(BlueprintCallable, Category = "AutoTee")
        void SetTeeHeightAsync(int32 Height);

    UFUNCTION(BlueprintCallable, Category = "AutoTee")
        void ReadTeeHeightAsync();

    UFUNCTION(BlueprintCallable, Category = "AutoTee")
        void MoveToHomeAsync();


    // ⭐ 반복 간격 설정
    UFUNCTION(BlueprintCallable, Category = "AutoTee")
        void SetKeyRepeatInterval(float Interval) { KeyRepeatInterval = Interval; }

    UFUNCTION(BlueprintPure, Category = "AutoTee")
        float GetKeyRepeatInterval() const { return KeyRepeatInterval; }

    UFUNCTION(BlueprintCallable, Category = "AutoTee")
        void SetKeyRepeatDelay(float Delay) { KeyRepeatDelay = Delay; }

protected:
    // 시리얼 통신 관련
    void* SerialHandle;
    bool bIsConnected;
    FString PortName;
    int32 BaudRate;

    // 현재 상태
    int32 CurrentTeeHeight;
    EKeypadVersion KeypadVersion;

    // 통신 프로토콜 상수
    static const uint8 STX = 0x02;
    static const uint8 ETX = 0x03;

    // 명령어 상수
    static const uint8 CMD_SET_HEIGHT = 0x4C;    // 'L'
    static const uint8 CMD_READ_HEIGHT = 0x4D;   // 'M'
    static const uint8 CMD_HOME = 0x48;          // 'H'
    static const uint8 CMD_REPLACE_TEE = 0x43;   // 'C'
    static const uint8 CMD_INITIALIZE = 0x52;    // 'R'

    // 내부 함수
    bool SendCommand(const TArray<uint8>& Command);
    bool ReceiveResponse(TArray<uint8>& Response, int32 ExpectedLength, float TimeoutSeconds = 2.0f);
    TArray<uint8> CreateHeightCommand(int32 Height);
    TArray<uint8> CreateSimpleCommand(uint8 CommandByte);
    int32 ParseHeightFromResponse(const TArray<uint8>& Response);
    void ProcessIncomingData();
    void ParseKeypadData(const TArray<uint8>& Data);
    EAutoTeeKey ParseKeyFromProtocol(const FString& Protocol);

    // 타이머 핸들
    FTimerHandle DataReadTimer;


    // 연결 중 플래그
    bool bIsConnecting;

    // 연결 타이머
    FTimerHandle ConnectionRetryTimer;
    int32 CurrentConnectionAttempt;
    int32 MaxConnectionAttempts;
    FString PendingPortName;
    int32 PendingBaudRate;


    // 대기 중인 명령
    FPendingCommand PendingCommand;

    // 응답 버퍼
    TArray<uint8> ResponseBuffer;
    TArray<uint8> KeypadBuffer;        // 키패드 입력용

    // 명령 타임아웃 타이머
    FTimerHandle CommandTimeoutTimer;

    // 명령 전송 (응답 대기 없음)
    void SendCommandAsync(EAutoTeeCommand CommandType, const TArray<uint8>& Command, int32 ExpectedResponseLength, float TimeoutSeconds = 1.0f);

    // 응답 처리
    void ProcessCommandResponse();

    // 타임아웃 처리
    void OnCommandTimeout();

    void ProcessKeypadBuffer();

    bool IsValidPacket(const TArray<uint8>& Data, int32 StartIndex, int32& PacketLength);
    // ⭐ 키 홀드 반복 기능
    TMap<EAutoTeeKey, FTimerHandle> KeyRepeatTimers;
    TMap<EAutoTeeKey, bool> KeyHoldState;
    float KeyRepeatInterval;  // 반복 간격 (초)
    float KeyRepeatDelay;     // 첫 반복 전 딜레이 (선택사항)

        // ⭐ 키 반복 처리
    void OnKeyRepeat(EAutoTeeKey Key);
    void StartKeyRepeat(EAutoTeeKey Key);
    void StopKeyRepeat(EAutoTeeKey Key);

    // ⭐ 빠른 반응을 위한 플래그
    bool bUseReleaseBasedDebounce;  // Release 기반 디바운싱 사용 여부

    // ⭐ 키 이벤트 필터링
    bool ShouldProcessKeyEvent(EAutoTeeKey Key, bool bIsPress);

private:
    // ⭐ 키 디바운싱용 변수
    EAutoTeeKey LastPressedKey;
    float LastKeyPressTime;
    float KeyDebounceTime;

    // Windows API 함수 래퍼
    bool OpenSerialPort(const FString& Port, int32 Baud);
    void CloseSerialPort();
    bool WriteToSerial(const uint8* Data, int32 Length);
    int32 ReadFromSerial(uint8* Buffer, int32 BufferSize);
    bool ConfigureSerialPort();
    // 비동기 연결 시도
    void TryConnectAsync();

    // 연결 완료 처리
    void OnConnectionComplete(bool bSuccess);

    bool ReceiveResponseImmediate(TArray<uint8>& Response, int32 ExpectedLength);
};