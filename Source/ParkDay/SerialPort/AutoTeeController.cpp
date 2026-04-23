#include "AutoTeeController.h"
#include "Engine/World.h"
#include "TimerManager.h"

#ifdef PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

UAutoTeeController::UAutoTeeController()
{
    SerialHandle = nullptr;
    bIsConnected = false;
    bIsConnecting = false;
    CurrentTeeHeight = 0;
    KeypadVersion = EKeypadVersion::V3;
    BaudRate = 9600;
    CurrentConnectionAttempt = 0;


    // ⭐ 키 반복 설정
    KeyRepeatInterval = 0.3f;  // 300ms 간격 (더 안정적)
    KeyRepeatDelay = 0.3f;     // 첫 반복 전 300ms 딜레이

    ResponseBuffer.Empty();
    KeypadBuffer.Empty();
    KeyRepeatTimers.Empty();

    // ⭐ 키 디바운싱 초기화
    LastPressedKey = EAutoTeeKey::Mulligan;
    LastKeyPressTime = 0.0f;
    KeyDebounceTime = 0.1f;  // 100ms 디바운스
    KeyHoldState.Empty();
}

UAutoTeeController::~UAutoTeeController()
{
    DisconnectFromDevice();
}

void UAutoTeeController::BeginDestroy()
{
    DisconnectFromDevice();

    ResponseBuffer.Empty();
    KeypadBuffer.Empty();

    // 모든 델리게이트 클리어
    OnTeeHeightChanged.Clear();
    OnKeyPressed.Clear();
    OnKeyReleased.Clear();
    OnConnectionStatusChanged.Clear();

    Super::BeginDestroy();

    UE_LOG(LogTemp, Log, TEXT("UAutoTeeController::BeginDestroy completed"));
}

void UAutoTeeController::DisconnectFromDevice()
{
    if (bIsConnected)
    {
        UWorld* World = GetWorld();

        // World가 유효하고 파괴 중이 아닐 때만 타이머 정리
        if (World && !World->bIsTearingDown)
        {
            FTimerManager& TimerManager = World->GetTimerManager();

            if (DataReadTimer.IsValid())
                TimerManager.ClearTimer(DataReadTimer);
            if (ConnectionRetryTimer.IsValid())
                TimerManager.ClearTimer(ConnectionRetryTimer);
            if (CommandTimeoutTimer.IsValid())
                TimerManager.ClearTimer(CommandTimeoutTimer);

            for (auto& Pair : KeyRepeatTimers)
                if (Pair.Value.IsValid())
                    TimerManager.ClearTimer(Pair.Value);
        }

        // 타이머 핸들 무효화 (World 상태 무관)
        DataReadTimer.Invalidate();
        ConnectionRetryTimer.Invalidate();
        CommandTimeoutTimer.Invalidate();
        KeyRepeatTimers.Empty();
        KeyHoldState.Empty();

        CloseSerialPort();
        bIsConnected = false;

        // 델리게이트는 World가 안전할 때만
        if (World && !World->bIsTearingDown)
            OnConnectionStatusChanged.Broadcast();
    }
}

bool UAutoTeeController::SetTeeHeight(int32 Height)
{
    if (!bIsConnected || Height < 0 || Height > 60)
    {
        return false;
    }

    TArray<uint8> Command = CreateHeightCommand(Height);
    if (!SendCommand(Command))
    {
        return false;
    }

    // ⭐ 즉시 응답 체크 (블로킹 없음)
    TArray<uint8> Response;
    if (ReceiveResponseImmediate(Response, 6))
    {
        int32 ResponseHeight = ParseHeightFromResponse(Response);
        if (ResponseHeight >= 0)
        {
            CurrentTeeHeight = ResponseHeight;
            OnTeeHeightChanged.Broadcast(CurrentTeeHeight);
            return true;
        }
    }

    // 응답 없어도 일단 성공으로 처리 (비동기로 나중에 받음)
    UE_LOG(LogTemp, VeryVerbose, TEXT("Command sent, response pending..."));
    return true;
}

bool UAutoTeeController::ReadTeeHeight()
{
    if (!bIsConnected)
    {
        return false;
    }

    TArray<uint8> Command = CreateSimpleCommand(CMD_READ_HEIGHT);
    if (SendCommand(Command))
    {
        TArray<uint8> Response;
        if (ReceiveResponse(Response, 6))
        {
            int32 Height = ParseHeightFromResponse(Response);
            if (Height >= 0)
            {
                CurrentTeeHeight = Height;
                OnTeeHeightChanged.Broadcast(CurrentTeeHeight);
                return true;
            }
        }
    }

    return false;
}

bool UAutoTeeController::MoveToHome()
{
    if (!bIsConnected)
    {
        return false;
    }

    TArray<uint8> Command = CreateSimpleCommand(CMD_HOME);
    return SendCommand(Command);  // ⭐ 응답 대기 제거
}


bool UAutoTeeController::ReplaceRubberTee()
{
    if (!bIsConnected)
    {
        return false;
    }

    TArray<uint8> Command = CreateSimpleCommand(CMD_REPLACE_TEE);
    bool Result = SendCommand(Command);

    if (Result)
    {
        UE_LOG(LogTemp, Log, TEXT("Rubber tee replacement initiated"));
    }

    return Result;
}

bool UAutoTeeController::InitializeAutoTee()
{
    if (!bIsConnected)
    {
        return false;
    }

    TArray<uint8> Command = CreateSimpleCommand(CMD_INITIALIZE);
    bool Result = SendCommand(Command);  // ⭐ 응답 대기 제거

    if (Result)
    {
        CurrentTeeHeight = 0;
        OnTeeHeightChanged.Broadcast(CurrentTeeHeight);
    }

    return Result;
}

TArray<uint8> UAutoTeeController::CreateHeightCommand(int32 Height)
{
    TArray<uint8> Command;
    Command.Add(STX);
    Command.Add(CMD_SET_HEIGHT);

    // Convert height to 3-digit ASCII (e.g., 45 -> "045")
    FString HeightStr = FString::Printf(TEXT("%03d"), Height);
    Command.Add(HeightStr[0]);
    Command.Add(HeightStr[1]);
    Command.Add(HeightStr[2]);

    Command.Add(ETX);
    return Command;
}

TArray<uint8> UAutoTeeController::CreateSimpleCommand(uint8 CommandByte)
{
    TArray<uint8> Command;
    Command.Add(STX);
    Command.Add(CommandByte);
    Command.Add(ETX);
    return Command;
}

int32 UAutoTeeController::ParseHeightFromResponse(const TArray<uint8>& Response)
{
    if (Response.Num() != 6 || Response[0] != STX || Response[5] != ETX)
    {
        return -1;
    }

    // Extract height from Data1, Data2, Data3
    FString HeightStr;
    HeightStr += (TCHAR)Response[2];
    HeightStr += (TCHAR)Response[3];
    HeightStr += (TCHAR)Response[4];

    return FCString::Atoi(*HeightStr);
}



void UAutoTeeController::ParseKeypadData(const TArray<uint8>& Data)
{
    if (Data.Num() < 3)
    {
        return;
    }

    if (Data[0] != STX || Data[Data.Num() - 1] != ETX)
    {
        return;
    }

    FString Protocol;
    for (int32 i = 1; i < Data.Num() - 1; ++i)
    {
        Protocol += (TCHAR)Data[i];
    }

    if (Protocol.IsEmpty())
    {
        return;
    }

    bool bIsPress = false;
    EAutoTeeKey Key = EAutoTeeKey::Mulligan;
    bool bValidKey = false;

    switch (KeypadVersion)
    {
    case EKeypadVersion::V1:
        if (Protocol.StartsWith(TEXT("K")))
        {
            Key = ParseKeyFromProtocol(Protocol);
            OnKeyPressed.Broadcast(Key);
            bValidKey = true;
            UE_LOG(LogTemp, Log, TEXT("✅ V1 Key: %s"), *Protocol);
        }
        break;

    case EKeypadVersion::V2:
        if (Protocol.StartsWith(TEXT("KP")))
        {
            Key = ParseKeyFromProtocol(Protocol);
            UE_LOG(LogTemp, Warning, TEXT("▶️ V2 Press received: %s, Key: %d"), *Protocol, (int32)Key);

            // ⭐ Press: 즉시 처리
            OnKeyPressed.Broadcast(Key);

            // ⭐ 연속 액션만 키 반복 시작
            if (Key == EAutoTeeKey::Left || Key == EAutoTeeKey::Right ||
                Key == EAutoTeeKey::Up || Key == EAutoTeeKey::Down)
            {
                StartKeyRepeat(Key);
                UE_LOG(LogTemp, Log, TEXT("🔄 V2 Continuous action key"));
            }
            else
            {
                UE_LOG(LogTemp, Log, TEXT("⏸️ V2 Single-action key"));
            }

            bValidKey = true;
            UE_LOG(LogTemp, Log, TEXT("✅ V2 Press: %s"), *Protocol);
        }
        else if (Protocol.StartsWith(TEXT("KR")))
        {
            Key = ParseKeyFromProtocol(Protocol);
            // ⭐ Release: 반복 중지
            StopKeyRepeat(Key);
            OnKeyReleased.Broadcast(Key);
            bValidKey = true;
            UE_LOG(LogTemp, Log, TEXT("✅ V2 Release: %s"), *Protocol);
        }
        break;

    case EKeypadVersion::V3:
        if (Protocol.StartsWith(TEXT("P")))
        {
            Key = ParseKeyFromProtocol(Protocol);
            UE_LOG(LogTemp, Warning, TEXT("▶️ V3 Press received: %s, Key: %d"), *Protocol, (int32)Key);

            // ⭐ Press: 즉시 처리
            OnKeyPressed.Broadcast(Key);

            // ⭐ 연속 액션만 키 반복 시작 (Left, Right, Up, Down)
            if (Key == EAutoTeeKey::Left || Key == EAutoTeeKey::Right ||
                Key == EAutoTeeKey::Up || Key == EAutoTeeKey::Down)
            {
                StartKeyRepeat(Key);
                UE_LOG(LogTemp, Log, TEXT("🔄 Continuous action key, repeat enabled"));
            }
            else
            {
                // Grid, Mulligan, Function은 반복 없음
                UE_LOG(LogTemp, Log, TEXT("⏸️ Single-action key, no repeat"));
            }

            bValidKey = true;
            UE_LOG(LogTemp, Log, TEXT("✅ V3 Press processed: %s"), *Protocol);
        }
        else if (Protocol.StartsWith(TEXT("R")))
        {
            Key = ParseKeyFromProtocol(Protocol);
            UE_LOG(LogTemp, Warning, TEXT("🛑 V3 Release received: %s, Key: %d"), *Protocol, (int32)Key);

            // ⭐ Release: 반복 중지
            StopKeyRepeat(Key);
            OnKeyReleased.Broadcast(Key);
            bValidKey = true;
            UE_LOG(LogTemp, Log, TEXT("✅ V3 Release processed: %s"), *Protocol);
        }
        break;
    }

    if (!bValidKey && !Protocol.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Unknown protocol: %s"), *Protocol);
    }
}

EAutoTeeKey UAutoTeeController::ParseKeyFromProtocol(const FString& Protocol)
{
    // V1
    if (Protocol == TEXT("K5")) return EAutoTeeKey::Mulligan;
    if (Protocol == TEXT("K2")) return EAutoTeeKey::Function;
    if (Protocol == TEXT("K4")) return EAutoTeeKey::Right;
    if (Protocol == TEXT("K3")) return EAutoTeeKey::Left;
    if (Protocol == TEXT("K1")) return EAutoTeeKey::Grid;

    // V2
    if (Protocol == TEXT("KP07") || Protocol == TEXT("KR07")) return EAutoTeeKey::Mulligan;
    if (Protocol == TEXT("KP05") || Protocol == TEXT("KR05")) return EAutoTeeKey::Function;
    if (Protocol == TEXT("KP04") || Protocol == TEXT("KR04")) return EAutoTeeKey::Right;
    if (Protocol == TEXT("KP03") || Protocol == TEXT("KR03")) return EAutoTeeKey::Left;
    if (Protocol == TEXT("KP01") || Protocol == TEXT("KR01")) return EAutoTeeKey::Up;
    if (Protocol == TEXT("KP02") || Protocol == TEXT("KR02")) return EAutoTeeKey::Down;
    if (Protocol == TEXT("KP06") || Protocol == TEXT("KR06")) return EAutoTeeKey::Grid;

    // V3 - Press와 Release 모두 처리
    if (Protocol == TEXT("PMM") || Protocol == TEXT("RMM")) return EAutoTeeKey::Mulligan;
    if (Protocol == TEXT("PFC") || Protocol == TEXT("RFC")) return EAutoTeeKey::Function;
    if (Protocol == TEXT("PVR") || Protocol == TEXT("RVR")) return EAutoTeeKey::Right;
    if (Protocol == TEXT("PVL") || Protocol == TEXT("RVL")) return EAutoTeeKey::Left;
    if (Protocol == TEXT("PTU") || Protocol == TEXT("RTU")) return EAutoTeeKey::Up;
    if (Protocol == TEXT("PTD") || Protocol == TEXT("RTD")) return EAutoTeeKey::Down;
    if (Protocol == TEXT("PGG") || Protocol == TEXT("RGG")) return EAutoTeeKey::Grid;

    UE_LOG(LogTemp, Warning, TEXT("⚠️ Unknown V3 protocol: %s, defaulting to Function"), *Protocol);
    return EAutoTeeKey::Function; // Default
}

#ifdef PLATFORM_WINDOWS

bool UAutoTeeController::OpenSerialPort(const FString& Port, int32 Baud)
{
    FString FullPortName = FString::Printf(TEXT("\\\\.\\%s"), *Port);

    SerialHandle = CreateFile(
        *FullPortName,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (SerialHandle == INVALID_HANDLE_VALUE)
    {
        SerialHandle = nullptr;
        return false;
    }

    // ⭐ 설정 실패 시 즉시 정리
    if (!ConfigureSerialPort())
    {
        CloseHandle((HANDLE)SerialHandle);
        SerialHandle = nullptr;
        return false;
    }

    return ConfigureSerialPort();
}

void UAutoTeeController::CloseSerialPort()
{
    if (SerialHandle != nullptr)
    {
        CloseHandle((HANDLE)SerialHandle);
        SerialHandle = nullptr;
    }
}

//bool UAutoTeeController::ConfigureSerialPort()
//{
//    if (SerialHandle == nullptr)
//    {
//        return false;
//    }
//
//    DCB dcb;
//    if (!GetCommState((HANDLE)SerialHandle, &dcb))
//    {
//        return false;
//    }
//
//    dcb.BaudRate = BaudRate;
//    dcb.ByteSize = 8;
//    dcb.StopBits = ONESTOPBIT;
//    dcb.Parity = NOPARITY;
//    dcb.fBinary = 1;
//    dcb.fParity = 0;
//    dcb.fOutxCtsFlow = 0;
//    dcb.fOutxDsrFlow = 0;
//    dcb.fDtrControl = DTR_CONTROL_DISABLE;
//    dcb.fDsrSensitivity = 0;
//    dcb.fTXContinueOnXoff = 0;
//    dcb.fOutX = 0;
//    dcb.fInX = 0;
//    dcb.fErrorChar = 0;
//    dcb.fNull = 0;
//    dcb.fRtsControl = RTS_CONTROL_DISABLE;
//    dcb.fAbortOnError = 0;
//
//    if (!SetCommState((HANDLE)SerialHandle, &dcb))
//    {
//        return false;
//    }
//
//    COMMTIMEOUTS timeouts;
//    timeouts.ReadIntervalTimeout = 50;
//    timeouts.ReadTotalTimeoutConstant = 50;
//    timeouts.ReadTotalTimeoutMultiplier = 10;
//    timeouts.WriteTotalTimeoutConstant = 50;
//    timeouts.WriteTotalTimeoutMultiplier = 10;
//
//    return SetCommTimeouts((HANDLE)SerialHandle, &timeouts);
//}

bool UAutoTeeController::WriteToSerial(const uint8* Data, int32 Length)
{
    if (SerialHandle == nullptr)
    {
        return false;
    }

    DWORD BytesWritten;
    return WriteFile((HANDLE)SerialHandle, Data, Length, &BytesWritten, NULL) && BytesWritten == Length;
}

int32 UAutoTeeController::ReadFromSerial(uint8* Buffer, int32 BufferSize)
{
    if (SerialHandle == nullptr)
    {
        return 0;
    }

    DWORD BytesRead;
    if (ReadFile((HANDLE)SerialHandle, Buffer, BufferSize, &BytesRead, NULL))
    {
        return BytesRead;
    }

    return 0;
}

bool UAutoTeeController::SendCommand(const TArray<uint8>& Command)
{
    return WriteToSerial(Command.GetData(), Command.Num());
}

bool UAutoTeeController::ReceiveResponse(TArray<uint8>& Response, int32 ExpectedLength, float TimeoutSeconds)
{
    if (!bIsConnected)
    {
        return false;
    }

    Response.Empty();

    // ⭐ 타임아웃을 매우 짧게 (50ms)
    const float ShortTimeout = 0.5f;  // 50ms
    float ElapsedTime = 0.0f;
    const float CheckInterval = 0.1f;  // 10ms

    while (ElapsedTime < ShortTimeout)  // ⭐ 최대 50ms만 대기
    {
        if (!bIsConnected)
        {
            return false;
        }

        uint8 Buffer[64];
        int32 BytesRead = ReadFromSerial(Buffer, sizeof(Buffer));

        if (BytesRead > 0)
        {
            for (int32 i = 0; i < BytesRead; ++i)
            {
                Response.Add(Buffer[i]);
            }

            if (Response.Num() >= ExpectedLength)
            {
                return true;
            }
        }

        // ⭐ Sleep 제거하고 즉시 반환
        // FPlatformProcess::Sleep(CheckInterval);  // 삭제
        ElapsedTime += CheckInterval;

        // 데이터 없으면 즉시 포기
        if (BytesRead == 0)
        {
            break;
        }
    }

    // ⭐ 응답 없어도 경고만 출력 (에러는 아님)
    if (Response.Num() < ExpectedLength)
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("Response incomplete, will retry on next update"));
    }

    return Response.Num() >= ExpectedLength;
}
#else

// Non-Windows platforms (placeholder implementations)
bool UAutoTeeController::OpenSerialPort(const FString& Port, int32 Baud)
{
    UE_LOG(LogTemp, Error, TEXT("Serial port communication not implemented for this platform"));
    return false;
}

void UAutoTeeController::CloseSerialPort() {}
bool UAutoTeeController::ConfigureSerialPort() { return false; }
bool UAutoTeeController::WriteToSerial(const uint8* Data, int32 Length) { return false; }
int32 UAutoTeeController::ReadFromSerial(uint8* Buffer, int32 BufferSize) { return 0; }
bool UAutoTeeController::SendCommand(const TArray<uint8>& Command) { return false; }
bool UAutoTeeController::ReceiveResponse(TArray<uint8>& Response, int32 ExpectedLength, float TimeoutSeconds) { return false; }

#endif


// ⭐ 비동기 연결 시작
void UAutoTeeController::ConnectToDeviceAsync(const FString& InPortName, int32 InBaudRate)
{
    if (bIsConnected)
    {
        UE_LOG(LogTemp, Warning, TEXT("Already connected. Disconnecting first..."));
        DisconnectFromDevice();
    }

    if (bIsConnecting)
    {
        UE_LOG(LogTemp, Warning, TEXT("Connection already in progress"));
        return;
    }

    PendingPortName = InPortName;
    PendingBaudRate = InBaudRate;
    CurrentConnectionAttempt = 0;
    bIsConnecting = true;

    UE_LOG(LogTemp, Log, TEXT("🔌 Starting async connection to AutoTee on %s..."), *InPortName);

    // 첫 번째 시도는 즉시 실행
    TryConnectAsync();
}

// ⭐ 연결 시도 (빠른 포기)
void UAutoTeeController::TryConnectAsync()
{
    CurrentConnectionAttempt++;

    UE_LOG(LogTemp, Log, TEXT("Attempting to connect to AutoTee on %s (Attempt %d/%d)"),
        *PendingPortName, CurrentConnectionAttempt, MaxConnectionAttempts);

    // 단일 연결 시도 (블로킹 최소화)
    if (OpenSerialPort(PendingPortName, PendingBaudRate))
    {
        bIsConnected = true;
        bIsConnecting = false;

        // 데이터 읽기 타이머 시작
        if (UWorld* World = GEngine->GetWorldFromContextObject(this, EGetWorldErrorMode::LogAndReturnNull))
        {
            World->GetTimerManager().SetTimer(DataReadTimer,
                FTimerDelegate::CreateUObject(this, &UAutoTeeController::ProcessIncomingData),
                0.05f, true);
        }

        OnConnectionComplete(true);
        return;
    }

    // 연결 실패
    UE_LOG(LogTemp, Warning, TEXT("Failed to connect to AutoTee on %s (Attempt %d/%d)"),
        *PendingPortName, CurrentConnectionAttempt, MaxConnectionAttempts);

    // 재시도 또는 포기
    if (CurrentConnectionAttempt < MaxConnectionAttempts)
    {
        // 1초 후 재시도 (비동기)
        if (UWorld* World = GEngine->GetWorldFromContextObject(this, EGetWorldErrorMode::LogAndReturnNull))
        {
            World->GetTimerManager().SetTimer(ConnectionRetryTimer,
                FTimerDelegate::CreateUObject(this, &UAutoTeeController::TryConnectAsync),
                1.0f, false);
        }
    }
    else
    {
        // 최대 시도 횟수 초과
        OnConnectionComplete(false);
    }
}

// ⭐ 연결 완료 처리
void UAutoTeeController::OnConnectionComplete(bool bSuccess)
{
    bIsConnecting = false;

    if (bSuccess)
    {
        OnConnectionStatusChanged.Broadcast();
        UE_LOG(LogTemp, Log, TEXT("✅ AutoTee connected to %s"), *PendingPortName);

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
                FString::Printf(TEXT("✅ AutoTee Connected (%s)"), *PendingPortName));
        }
    }
    else
    {
        bIsConnected = false;
        SerialHandle = nullptr;
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to connect to AutoTee on %s after %d attempts"),
            *PendingPortName, MaxConnectionAttempts);

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow,
                FString::Printf(TEXT("⚠️ AutoTee Connection Failed (%s)"), *PendingPortName));
        }
    }

    // 타이머 클리어
    if (UWorld* World = GEngine->GetWorldFromContextObject(this, EGetWorldErrorMode::LogAndReturnNull))
    {
        World->GetTimerManager().ClearTimer(ConnectionRetryTimer);
    }
}

// ⭐ 기존 ConnectToDevice 함수는 유지하되 간소화
bool UAutoTeeController::ConnectToDevice(const FString& InPortName, int32 InBaudRate)
{
    if (bIsConnected)
    {
        DisconnectFromDevice();
    }

    PortName = InPortName;
    BaudRate = InBaudRate;

    // ⭐ 단일 시도만 수행 (블로킹 최소화)
    if (OpenSerialPort(PortName, BaudRate))
    {
        bIsConnected = true;

        // 데이터 읽기 타이머 시작
        if (UWorld* World = GEngine->GetWorldFromContextObject(this, EGetWorldErrorMode::LogAndReturnNull))
        {
            World->GetTimerManager().SetTimer(DataReadTimer,
                FTimerDelegate::CreateUObject(this, &UAutoTeeController::ProcessIncomingData),
                0.05f, true);
        }

        OnConnectionStatusChanged.Broadcast();
        UE_LOG(LogTemp, Log, TEXT("AutoTee connected to %s"), *PortName);
        return true;
    }

    // 즉시 실패
    bIsConnected = false;
    SerialHandle = nullptr;
    UE_LOG(LogTemp, Warning, TEXT("Failed to connect to AutoTee on %s"), *PortName);
    return false;
}

#ifdef PLATFORM_WINDOWS

// ⭐ OpenSerialPort도 타임아웃 줄이기
bool UAutoTeeController::ConfigureSerialPort()
{
    if (SerialHandle == nullptr)
    {
        return false;
    }

    DCB dcb;
    if (!GetCommState((HANDLE)SerialHandle, &dcb))
    {
        return false;
    }

    dcb.BaudRate = BaudRate;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    dcb.fBinary = 1;
    dcb.fParity = 0;
    dcb.fOutxCtsFlow = 0;
    dcb.fOutxDsrFlow = 0;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fDsrSensitivity = 0;
    dcb.fTXContinueOnXoff = 0;
    dcb.fOutX = 0;
    dcb.fInX = 0;
    dcb.fErrorChar = 0;
    dcb.fNull = 0;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fAbortOnError = 0;

    if (!SetCommState((HANDLE)SerialHandle, &dcb))
    {
        return false;
    }

    // ⭐ 타임아웃 대폭 줄임 (50ms → 10ms)
    COMMTIMEOUTS timeouts;
    timeouts.ReadIntervalTimeout = MAXDWORD;        // ⭐ 즉시 반환
    timeouts.ReadTotalTimeoutConstant = 0;          // ⭐ 0ms
    timeouts.ReadTotalTimeoutMultiplier = 0;        // ⭐ 0ms
    timeouts.WriteTotalTimeoutConstant = 50;        // 쓰기는 50ms
    timeouts.WriteTotalTimeoutMultiplier = 10;

    return SetCommTimeouts((HANDLE)SerialHandle, &timeouts);
}

#endif


// ⭐ 즉시 응답 체크 (블로킹 없음)
bool UAutoTeeController::ReceiveResponseImmediate(TArray<uint8>& Response, int32 ExpectedLength)
{
    if (!bIsConnected)
    {
        return false;
    }

    Response.Empty();

    uint8 Buffer[64];
    int32 BytesRead = ReadFromSerial(Buffer, sizeof(Buffer));

    if (BytesRead > 0)
    {
        for (int32 i = 0; i < BytesRead; ++i)
        {
            Response.Add(Buffer[i]);
        }
    }

    return Response.Num() >= ExpectedLength;
}

// ⭐ 비동기 명령 전송
void UAutoTeeController::SendCommandAsync(EAutoTeeCommand CommandType, const TArray<uint8>& Command, int32 ExpectedResponseLength, float TimeoutSeconds)
{
    if (!bIsConnected)
    {
        UE_LOG(LogTemp, Warning, TEXT("AutoTee not connected"));
        return;
    }

    // 이전 명령 취소
    if (UWorld* World = GEngine->GetWorldFromContextObject(this, EGetWorldErrorMode::LogAndReturnNull))
    {
        World->GetTimerManager().ClearTimer(CommandTimeoutTimer);
    }

    // 명령 전송
    if (!SendCommand(Command))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to send AutoTee command"));
        return;
    }

    // 대기 명령 설정
    PendingCommand = FPendingCommand(CommandType, Command, ExpectedResponseLength, TimeoutSeconds);
    ResponseBuffer.Empty();

    // 타임아웃 타이머 설정
    if (UWorld* World = GEngine->GetWorldFromContextObject(this, EGetWorldErrorMode::LogAndReturnNull))
    {
        World->GetTimerManager().SetTimer(CommandTimeoutTimer,
            FTimerDelegate::CreateUObject(this, &UAutoTeeController::OnCommandTimeout),
            TimeoutSeconds, false);
    }

    UE_LOG(LogTemp, VeryVerbose, TEXT("AutoTee command sent, waiting for response..."));
}

// ⭐ ProcessIncomingData 수정 (명령 응답 처리 추가)
void UAutoTeeController::ProcessIncomingData()
{
    if (!bIsConnected)
    {
        return;
    }

    uint8 Buffer[256];
    int32 BytesRead = ReadFromSerial(Buffer, sizeof(Buffer));

    if (BytesRead > 0)
    {
        // ⭐ 읽은 데이터를 키패드 버퍼에 추가
        for (int32 i = 0; i < BytesRead; ++i)
        {
            KeypadBuffer.Add(Buffer[i]);
        }

        // ⭐ 대기 중인 명령이 있으면 응답 처리
        if (PendingCommand.CommandType != EAutoTeeCommand::None)
        {
            // 명령 응답은 ResponseBuffer 사용
            ResponseBuffer = KeypadBuffer;
            ProcessCommandResponse();

            // 명령 처리 후 해당 데이터는 제거
            if (PendingCommand.CommandType == EAutoTeeCommand::None)
            {
                KeypadBuffer.Empty();
            }
        }

        // ⭐ 키패드 데이터 처리
        ProcessKeypadBuffer();

        // ⭐ 버퍼 크기 제한
        if (KeypadBuffer.Num() > 1024)
        {
            KeypadBuffer.RemoveAt(0, KeypadBuffer.Num() - 512);
        }
    }
}

// ⭐ 키패드 버퍼 처리 (새로운 함수)
void UAutoTeeController::ProcessKeypadBuffer()
{
    // STX를 찾아서 패킷 파싱
    for (int32 i = 0; i < KeypadBuffer.Num(); ++i)
    {
        if (KeypadBuffer[i] == STX)
        {
            int32 PacketLength = 0;
            if (IsValidPacket(KeypadBuffer, i, PacketLength))
            {
                // 유효한 패킷 추출
                TArray<uint8> Packet;
                for (int32 j = 0; j < PacketLength; ++j)
                {
                    Packet.Add(KeypadBuffer[i + j]);
                }

                // 패킷 파싱
                ParseKeypadData(Packet);

                // 처리한 데이터 제거
                KeypadBuffer.RemoveAt(i, PacketLength);
                i--; // 인덱스 조정
            }
        }
    }
}

// ⭐ 패킷 검증 함수 (새로운 함수)
bool UAutoTeeController::IsValidPacket(const TArray<uint8>& Data, int32 StartIndex, int32& PacketLength)
{
    if (StartIndex >= Data.Num() || Data[StartIndex] != STX)
    {
        return false;
    }

    // ETX 찾기 (최대 20바이트까지만 검색)
    for (int32 i = StartIndex + 1; i < FMath::Min(Data.Num(), StartIndex + 20); ++i)
    {
        if (Data[i] == ETX)
        {
            PacketLength = i - StartIndex + 1;

            // 최소 패킷 크기 체크 (STX + 최소 1바이트 + ETX)
            if (PacketLength >= 3)
            {
                return true;
            }
        }
    }

    return false;
}


// ⭐ 명령 응답 처리
void UAutoTeeController::ProcessCommandResponse()
{
    if (ResponseBuffer.Num() < PendingCommand.ExpectedResponseLength)
    {
        return;
    }

    // ⭐ STX로 시작하는 응답 찾기
    int32 StartIndex = -1;
    for (int32 i = 0; i < ResponseBuffer.Num(); ++i)
    {
        if (ResponseBuffer[i] == STX)
        {
            StartIndex = i;
            break;
        }
    }

    if (StartIndex < 0)
    {
        return;
    }

    // 유효한 응답 데이터 추출
    TArray<uint8> Response;
    for (int32 i = StartIndex; i < FMath::Min(StartIndex + PendingCommand.ExpectedResponseLength, ResponseBuffer.Num()); ++i)
    {
        Response.Add(ResponseBuffer[i]);
    }

    if (Response.Num() < PendingCommand.ExpectedResponseLength)
    {
        return;
    }

    // 타임아웃 타이머 취소
    if (UWorld* World = GEngine->GetWorldFromContextObject(this, EGetWorldErrorMode::LogAndReturnNull))
    {
        World->GetTimerManager().ClearTimer(CommandTimeoutTimer);
    }

    // 응답 파싱
    bool bSuccess = false;

    switch (PendingCommand.CommandType)
    {
    case EAutoTeeCommand::SetHeight:
    case EAutoTeeCommand::ReadHeight:
    {
        int32 Height = ParseHeightFromResponse(Response);
        if (Height >= 0)
        {
            CurrentTeeHeight = Height;
            OnTeeHeightChanged.Broadcast(CurrentTeeHeight);
            bSuccess = true;
            UE_LOG(LogTemp, Log, TEXT("✅ Tee height: %d mm"), CurrentTeeHeight);
        }
        break;
    }

    case EAutoTeeCommand::MoveHome:
    case EAutoTeeCommand::Initialize:
    {
        if (Response.Num() >= 3 && Response[0] == STX && Response[Response.Num() - 1] == ETX)
        {
            CurrentTeeHeight = 0;
            OnTeeHeightChanged.Broadcast(CurrentTeeHeight);
            bSuccess = true;
            UE_LOG(LogTemp, Log, TEXT("✅ AutoTee command completed"));
        }
        break;
    }

    default:
        break;
    }

    if (!bSuccess)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Failed to parse AutoTee response"));
    }

    // 명령 완료
    PendingCommand.CommandType = EAutoTeeCommand::None;
    ResponseBuffer.Empty();
}

// ⭐ 타임아웃 처리
void UAutoTeeController::OnCommandTimeout()
{
    UE_LOG(LogTemp, Warning, TEXT("⚠️ AutoTee command timeout"));

    PendingCommand.CommandType = EAutoTeeCommand::None;
    ResponseBuffer.Empty();
}

// ⭐ 비동기 API 구현
void UAutoTeeController::SetTeeHeightAsync(int32 Height)
{
    if (Height < 0 || Height > 60)
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid tee height: %d"), Height);
        return;
    }

    TArray<uint8> Command = CreateHeightCommand(Height);
    PendingCommand.RequestedHeight = Height;
    SendCommandAsync(EAutoTeeCommand::SetHeight, Command, 6, 1.0f);
}

void UAutoTeeController::ReadTeeHeightAsync()
{
    TArray<uint8> Command = CreateSimpleCommand(CMD_READ_HEIGHT);
    SendCommandAsync(EAutoTeeCommand::ReadHeight, Command, 6, 1.0f);
}

void UAutoTeeController::MoveToHomeAsync()
{
    TArray<uint8> Command = CreateSimpleCommand(CMD_HOME);
    SendCommandAsync(EAutoTeeCommand::MoveHome, Command, 6, 1.0f);
}


// ⭐ 키 반복 시작
void UAutoTeeController::StartKeyRepeat(EAutoTeeKey Key)
{
    UWorld* World = GEngine->GetWorldFromContextObject(this, EGetWorldErrorMode::LogAndReturnNull);
    // World 상태 확인 추가
    if (!World || World->bIsTearingDown)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot start key repeat: World invalid"));
        return;
    }


    float CurrentTime = World->GetTimeSeconds();

    // ⭐ 같은 키가 이미 활성화되어 있으면 무시
    if (KeyHoldState.Contains(Key) && KeyHoldState[Key])
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("⚠️ Key %d already active, ignoring"), (int32)Key);
        return;
    }

    // ⭐ 다른 키로 전환할 때 디바운스 체크
    if (Key != LastPressedKey)
    {
        float TimeSinceLastPress = CurrentTime - LastKeyPressTime;
        if (TimeSinceLastPress < KeyDebounceTime)
        {
            UE_LOG(LogTemp, VeryVerbose, TEXT("⚠️ Key switch too fast (%.3fs), ignoring Key %d"),
                TimeSinceLastPress, (int32)Key);
            return;
        }

        // ⭐ 모든 다른 키 강제 정지
        UE_LOG(LogTemp, Log, TEXT("🛑 Stopping all other keys before starting Key %d"), (int32)Key);
        TArray<EAutoTeeKey> KeysToStop;
        for (auto& Pair : KeyHoldState)
        {
            if (Pair.Key != Key && Pair.Value)
            {
                KeysToStop.Add(Pair.Key);
            }
        }

        for (EAutoTeeKey KeyToStop : KeysToStop)
        {
            StopKeyRepeat(KeyToStop);
        }
    }

    // 마지막 키 정보 업데이트
    LastPressedKey = Key;
    LastKeyPressTime = CurrentTime;

    // ⭐ 혹시 남아있을 수 있는 타이머 클리어
    if (KeyRepeatTimers.Contains(Key))
    {
        World->GetTimerManager().ClearTimer(KeyRepeatTimers[Key]);
        KeyRepeatTimers.Remove(Key);
    }

    // ⭐ 홀드 상태로 설정
    KeyHoldState.Add(Key, true);

    // ⭐ 첫 반복 타이머 시작 (KeyRepeatDelay 후 시작)
    FTimerHandle TimerHandle;
    World->GetTimerManager().SetTimer(
        TimerHandle,
        FTimerDelegate::CreateUObject(this, &UAutoTeeController::OnKeyRepeat, Key),
        KeyRepeatDelay,  // 첫 반복은 딜레이 후
        false  // 일회성
    );

    KeyRepeatTimers.Add(Key, TimerHandle);

    UE_LOG(LogTemp, Log, TEXT("⏱️ Key repeat started for Key %d (Delay: %.2fs, Interval: %.2fs)"),
        (int32)Key, KeyRepeatDelay, KeyRepeatInterval);
}

// ⭐ 키 반복 중지
void UAutoTeeController::StopKeyRepeat(EAutoTeeKey Key)
{
    UWorld* World = GEngine->GetWorldFromContextObject(this, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        return;
    }

    // ⭐ 홀드 상태 먼저 해제 (타이머 콜백에서 바로 체크되도록)
    KeyHoldState.Add(Key, false);

    // 타이머 클리어
    if (KeyRepeatTimers.Contains(Key))
    {
        World->GetTimerManager().ClearTimer(KeyRepeatTimers[Key]);
        KeyRepeatTimers.Remove(Key);
    }

    UE_LOG(LogTemp, Log, TEXT("⏹️ Key repeat stopped for Key %d"), (int32)Key);
}

// ⭐ 키 반복 콜백
void UAutoTeeController::OnKeyRepeat(EAutoTeeKey Key)
{
    UWorld* World = GEngine->GetWorldFromContextObject(this, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        return;
    }

    // ⭐ 홀드 상태가 아니면 즉시 중지
    if (!KeyHoldState.Contains(Key) || !KeyHoldState[Key])
    {
        UE_LOG(LogTemp, Log, TEXT("⏹️ Key %d not held, stopping repeat"), (int32)Key);
        StopKeyRepeat(Key);
        return;
    }

    // ⭐ 키 이벤트 다시 발생
    OnKeyPressed.Broadcast(Key);
    UE_LOG(LogTemp, Log, TEXT("🔁 Key repeat: %d"), (int32)Key);

    // ⭐ 다음 반복 타이머 설정 (이제부터는 KeyRepeatInterval 사용)
    FTimerHandle TimerHandle;
    World->GetTimerManager().SetTimer(
        TimerHandle,
        FTimerDelegate::CreateUObject(this, &UAutoTeeController::OnKeyRepeat, Key),
        KeyRepeatInterval,  // 반복 간격
        false  // 일회성 (재귀적으로 호출됨)
    );

    // ⭐ 기존 타이머가 있다면 먼저 클리어
    if (KeyRepeatTimers.Contains(Key))
    {
        KeyRepeatTimers[Key] = TimerHandle;
    }
    else
    {
        KeyRepeatTimers.Add(Key, TimerHandle);
    }
}