// CR2SensorManager.cpp
#include "CR2SensorManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"
#include "Async/Async.h"
#include "ParkDayProfiling.h"

// 정적 인스턴스 초기화
ACR2SensorManager* ACR2SensorManager::Instance = nullptr;

// Native 구조체 정의 (DLL과 호환)
struct CR2_shotdata_native
{
    int32 ballspeedX10;
    int32 clubspeed_BX10;
    int32 clubspeed_AX10;
    int32 clubpathX10;
    int32 clubfaceangleX10;
    int32 sidespin;
    int32 backspin;
    int32 azimuthX10;
    int32 inclineX10;
};

struct CR2_ballposition_native
{
    int32 ballexist;
    int32 shotresult;
    int32 x;
    int32 y;
    int32 z;
};

struct CR2_shotdata_ballEx_native
{
    uint32 valid;
    uint32 reserved;
    double incline;
    double azimuth;
    double vmag;
    double shotAssurance;
    double spinmag;
    double spinaxis[3];
    double spinAssurance;
};

ACR2SensorManager::ACR2SensorManager()
{
    PrimaryActorTick.bCanEverTick = true;

    // 기본값 설정
    DLLHandle = nullptr;
    SensorHandle = nullptr;
    bSensorInitialized = false;
    bSensorStarted = false;
    LastSensorStatus = 0;
    UpdateInterval = 0.02f; // 50Hz
    StatusCheckTimer = 0.0f;

    // DLL 경로 설정 (프로젝트 Binaries 폴더 기준)
#if defined(_WIN64)
    DLLPath = TEXT("XTparkAdapt64.dll");
   // DLLPath = TEXT("XcamAdapt64.dll");
  //  DLLPath = TEXT("z3camAdapt64.dll");
#else
    DLLPath = TEXT("XcamAdapt.dll");
#endif

    // API 함수 포인터 초기화
    CR2_init_func = nullptr;
    CR2_delete_func = nullptr;
    CR2_command_func = nullptr;

    // 정적 인스턴스 설정
    Instance = this;
}

void ACR2SensorManager::BeginPlay()
{
    Super::BeginPlay();

    // 자동으로 센서 초기화
    if (InitializeSensor())
    {
        UE_LOG(LogTemp, Log, TEXT("?? CR2 Sensor initialized successfully"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("?? Failed to initialize CR2 Sensor"));
    }
}

void ACR2SensorManager::BeginDestroy()
{
    ShutdownSensor();
    if (Instance == this)
    {
        Instance = nullptr;
    }
    Super::BeginDestroy();
}
void ACR2SensorManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ShutdownSensor();
    if (Instance == this)
    {
        Instance = nullptr;
    }
    Super::EndPlay(EndPlayReason);
}

void ACR2SensorManager::Tick(float DeltaTime)
{
    SCOPE_CYCLE_COUNTER(STAT_SensorTick);
    Super::Tick(DeltaTime);

    if (bSensorInitialized)
    {
        StatusCheckTimer += DeltaTime;
        if (StatusCheckTimer >= UpdateInterval)
        {
#if !WITH_EDITOR
            CheckSensorStatus();
#endif
            StatusCheckTimer = 0.0f;
        }
    }
}

bool ACR2SensorManager::LoadDLL()
{
    if (DLLHandle != nullptr)
    {
        return true; // 이미 로드됨
    }

#if PLATFORM_64BITS
    FString FullDLLPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Binaries"), TEXT("Win64"), DLLPath);
#else
    FString FullDLLPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Binaries"), TEXT("Win32"), DLLPath);
#endif

    // DLL 로드
    DLLHandle = LoadLibrary(*FullDLLPath);
    if (DLLHandle == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("??Failed to load DLL: %s"), *FullDLLPath);
        return false;
    }

#pragma warning(push)
#pragma warning(disable: 4191) // 이 특정 경고만 잠시 비활성화
    // API 함수 포인터 가져오기 - reinterpret_cast 사용
    CR2_init_func = reinterpret_cast<CR2_init_t>(GetProcAddress(DLLHandle, "CR2_init"));
    CR2_delete_func = reinterpret_cast<CR2_delete_t>(GetProcAddress(DLLHandle, "CR2_delete"));
    CR2_command_func = reinterpret_cast<CR2_command_t>(GetProcAddress(DLLHandle, "CR2_command"));

#pragma warning(pop) // 경고 설정을 다시 복원

    if (!CR2_init_func || !CR2_delete_func || !CR2_command_func)
    {
        UE_LOG(LogTemp, Error, TEXT("??Failed to get API functions from DLL"));
        UnloadDLL();
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("CR2 DLL loaded successfully"));
    return true;
}

void ACR2SensorManager::UnloadDLL()
{
    if (DLLHandle != nullptr)
    {
        FreeLibrary(DLLHandle);
        DLLHandle = nullptr;
        CR2_init_func = nullptr;
        CR2_delete_func = nullptr;
        CR2_command_func = nullptr;
        UE_LOG(LogTemp, Log, TEXT("??CR2 DLL unloaded"));
    }
}

ESensorBackend ACR2SensorManager::ResolveSensorBackend()
{
    if (SensorBackend != ESensorBackend::Auto)
    {
        UE_LOG(LogTemp, Log, TEXT("[Sensor] ini 설정값 사용: %s"),
            SensorBackend == ESensorBackend::CR2_XcamAdapt ? TEXT("CR2") : TEXT("EZSensorSDK"));
        return SensorBackend;
    }

    if (ProbeCR2DLLAvailable())
    {
        UE_LOG(LogTemp, Log, TEXT("[Sensor] 자동감지: %s 발견 → CR2 백엔드 사용"), *DLLPath);
        return ESensorBackend::CR2_XcamAdapt;
    }

    UE_LOG(LogTemp, Log, TEXT("[Sensor] 자동감지: %s 없음 → EZSensorSDK 백엔드로 폴백"), *DLLPath);
    return ESensorBackend::EZSensorSDK;
}

bool ACR2SensorManager::ProbeCR2DLLAvailable() const
{
#if PLATFORM_64BITS
    FString FullDLLPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Binaries"), TEXT("Win64"), DLLPath);
#else
    FString FullDLLPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Binaries"), TEXT("Win32"), DLLPath);
#endif
    return FPaths::FileExists(FullDLLPath);
}

bool ACR2SensorManager::InitializeSensor()
{
    if (bSensorInitialized)
    {
        return true; // 이미 초기화됨
    }

    ResolvedBackend = ResolveSensorBackend();

    return (ResolvedBackend == ESensorBackend::EZSensorSDK)
        ? InitializeSensor_EZ()
        : InitializeSensor_CR2();
}

bool ACR2SensorManager::InitializeSensor_CR2()
{
    // DLL 로드
    if (!LoadDLL())
    {
        return false;
    }

    // 센서 초기화
    SensorHandle = CR2_init_func(0, 0, 0, 0, 0, 0);
    if (SensorHandle == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("??CR2_init failed"));
        return false;
    }

    bSensorInitialized = true;
    UE_LOG(LogTemp, Log, TEXT("??CR2 Sensor initialized"));

    // DLL 버전 확인
    FString Version = GetDLLVersion();
    UE_LOG(LogTemp, Log, TEXT("??CR2 DLL Version: %s"), *Version);

 

    return true;
}

bool ACR2SensorManager::InitializeSensor_EZ()
{
    UGameInstance* GI = GetGameInstance();
    if (!GI)
    {
        UE_LOG(LogTemp, Error, TEXT("?? [EZ] GameInstance를 가져올 수 없음"));
        return false;
    }

    EZSensor = GI->GetSubsystem<UEZSensorSubsystem>();
    if (!EZSensor.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("?? [EZ] UEZSensorSubsystem을 가져올 수 없음 (플러그인 활성화 여부 확인 필요)"));
        return false;
    }

    // EZSensorSubsystem::Initialize()에서 ez_sesnor_init + ez_start_sensor까지 이미 끝낸 상태.
    // 여기서는 게임 쪽 델리게이트만 바인딩한다.
    EZSensor->OnSensorStatusChanged.AddDynamic(this, &ACR2SensorManager::HandleEZSensorStatusChanged);
    EZSensor->OnBallStatusChanged.AddDynamic(this, &ACR2SensorManager::HandleEZBallStatusChanged);
    EZSensor->OnShotInfoReceived.AddDynamic(this, &ACR2SensorManager::HandleEZShotInfoReceived);

    bSensorInitialized = true;
    CachedEZStatus = CR2STATUS_DISCONNECT; // 실제 Ready 콜백이 올 때까지는 미연결로 간주
    UE_LOG(LogTemp, Log, TEXT("?? [EZ] EZSensorSDK 백엔드로 초기화 완료 (델리게이트 바인딩됨)"));

    return true;
}

void ACR2SensorManager::ShutdownSensor()
{
    if (ResolvedBackend == ESensorBackend::EZSensorSDK)
    {
        ShutdownSensor_EZ();
    }
    else
    {
        ShutdownSensor_CR2();
    }
}

void ACR2SensorManager::ShutdownSensor_CR2()
{
    if (bSensorStarted)
    {
        //SetDisableLED();
        SetLEDColor(CR2STATUS_DISCONNECT);

        StopSensorOperation();
    }

    if (bSensorInitialized && SensorHandle != nullptr)
    {
        CR2_delete_func(SensorHandle);
        SensorHandle = nullptr;
        bSensorInitialized = false;
        UE_LOG(LogTemp, Log, TEXT("??CR2 Sensor shutdown"));
    }

    UnloadDLL();
}

void ACR2SensorManager::ShutdownSensor_EZ()
{
    if (EZSensor.IsValid())
    {
        EZSensor->CancelSensing();
        EZSensor->OnSensorStatusChanged.RemoveDynamic(this, &ACR2SensorManager::HandleEZSensorStatusChanged);
        EZSensor->OnBallStatusChanged.RemoveDynamic(this, &ACR2SensorManager::HandleEZBallStatusChanged);
        EZSensor->OnShotInfoReceived.RemoveDynamic(this, &ACR2SensorManager::HandleEZShotInfoReceived);
    }
    // ez_stop_sensor/ez_sensor_close는 UEZSensorSubsystem::Deinitialize()(GameInstance 종료 시)에서 처리됨.
    bSensorInitialized = false;
    bSensorStarted = false;
    UE_LOG(LogTemp, Log, TEXT("?? [EZ] EZSensorSDK 백엔드 셔다운 (센싱 취소 + 델리게이트 해제)"));
}

bool ACR2SensorManager::StartSensorOperation()
{
    return (ResolvedBackend == ESensorBackend::EZSensorSDK)
        ? StartSensorOperation_EZ()
        : StartSensorOperation_CR2();
}

bool ACR2SensorManager::StartSensorOperation_CR2()
{
    if (!bSensorInitialized || SensorHandle == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("??Sensor not initialized"));
        return false;
    }

    if (bSensorStarted)
    {
        return true; // 이미 시작됨
    }

    PARAM_T p0 = (PARAM_T)&SensorCallback;
    PARAM_T p1 = (PARAM_T)this; // 사용자 파라미터로 this 포인터 전달

    CR2_result_t result = CR2_command_func(SensorHandle, CR2CMD_OPERATION_START, p0, p1, 0, 0);
    if (result != CR2_OK)
    {
        UE_LOG(LogTemp, Error, TEXT("??Failed to start sensor operation: 0x%08x"), result);
        return false;
    }

    bSensorStarted = true;
    UE_LOG(LogTemp, Log, TEXT("??CR2 Sensor operation started"));

    // 센서 시작시 현재 상태에 맞는 LED 색상 설정
    SetLEDColor();

    return true;
}

bool ACR2SensorManager::StartSensorOperation_EZ()
{
    if (!bSensorInitialized || !EZSensor.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("?? [EZ] Sensor not initialized"));
        return false;
    }

    if (bSensorStarted)
    {
        return true;
    }

    // 현재 설정된 클럽(SelectClub)에 해당하는 ground로 센싱을 시작한다.
    CurrentEZGround = ClubCodeToEZGround(SelectClub);
    if (!EZSensor->StartSensing(CurrentEZGround, /*bAllowGroundChange=*/false))
    {
        UE_LOG(LogTemp, Error, TEXT("?? [EZ] StartSensing 실패"));
        return false;
    }

    bSensorStarted = true;
    UE_LOG(LogTemp, Log, TEXT("?? [EZ] EZSensorSDK 센싱 시작 (Ground=%d)"), (int32)CurrentEZGround);
    return true;
}

bool ACR2SensorManager::StopSensorOperation()
{
    return (ResolvedBackend == ESensorBackend::EZSensorSDK)
        ? StopSensorOperation_EZ()
        : StopSensorOperation_CR2();
}

bool ACR2SensorManager::StopSensorOperation_CR2()
{
    if (!bSensorInitialized || SensorHandle == nullptr)
    {
        return false;
    }

    if (!bSensorStarted)
    {
        return true; // 이미 정지됨
    }

 

    CR2_result_t result = CR2_command_func(SensorHandle, CR2CMD_OPERATION_STOP, 0, 0, 0, 0);
    if (result != CR2_OK)
    {
        UE_LOG(LogTemp, Error, TEXT("??Failed to stop sensor operation: 0x%08x"), result);
        return false;
    }

    bSensorStarted = false;
    UE_LOG(LogTemp, Log, TEXT("??CR2 Sensor operation stopped"));
    return true;
}

bool ACR2SensorManager::StopSensorOperation_EZ()
{
    if (!bSensorInitialized || !EZSensor.IsValid())
    {
        return false;
    }

    if (!bSensorStarted)
    {
        return true;
    }

    EZSensor->CancelSensing();
    bSensorStarted = false;
    UE_LOG(LogTemp, Log, TEXT("?? [EZ] EZSensorSDK 센싱 정지"));
    return true;
}

bool ACR2SensorManager::RestartSensorOperation()
{
    return (ResolvedBackend == ESensorBackend::EZSensorSDK)
        ? RestartSensorOperation_EZ()
        : RestartSensorOperation_CR2();
}

bool ACR2SensorManager::RestartSensorOperation_CR2()
{
    if (!bSensorInitialized || SensorHandle == nullptr)
    {
        return false;
    }

    CR2_result_t result = CR2_command_func(SensorHandle, CR2CMD_OPERATION_RESTART, 0, 0, 0, 0);
    if (result != CR2_OK)
    {
        UE_LOG(LogTemp, Error, TEXT("??Failed to restart sensor operation: 0x%08x"), result);
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("??CR2 Sensor operation restarted"));
    return true;
}

bool ACR2SensorManager::RestartSensorOperation_EZ()
{
    if (!bSensorInitialized || !EZSensor.IsValid())
    {
        return false;
    }

    EZSensor->CancelSensing();
    const bool bOk = EZSensor->StartSensing(CurrentEZGround, /*bAllowGroundChange=*/false);
    UE_LOG(LogTemp, Log, TEXT("?? [EZ] EZSensorSDK 센싱 재시작 (Ground=%d, ok=%d)"), (int32)CurrentEZGround, bOk);
    return bOk;
}

bool ACR2SensorManager::ConfigureSensor(float LightHeight, int32 VAngleAdd)
{
    return (ResolvedBackend == ESensorBackend::EZSensorSDK)
        ? ConfigureSensor_EZ(LightHeight, VAngleAdd)
        : ConfigureSensor_CR2(LightHeight, VAngleAdd);
}

bool ACR2SensorManager::ConfigureSensor_CR2(float LightHeight, int32 VAngleAdd)
{


    if (!bSensorInitialized || SensorHandle == nullptr)
    {
        return false;
    }

    PARAM_T p0 = (PARAM_T)(LightHeight * 100);
    PARAM_T p1 = (PARAM_T)VAngleAdd;

    CR2_result_t result = CR2_command_func(SensorHandle, CR2CMD_SENSORCONFIG, p0, p1, 0, 0);
    if (result != CR2_OK)
    {
        UE_LOG(LogTemp, Error, TEXT("??Failed to configure sensor: 0x%08x"), result);
        return false;
    }
 
    UE_LOG(LogTemp, Log, TEXT("??CR2 Sensor configured - Height: %.2f, VAngle: %d"), LightHeight, VAngleAdd);
    return true;
}

bool ACR2SensorManager::ConfigureSensor_EZ(float LightHeight, int32 VAngleAdd)
{
    // EZSensorSDK 헤더(ez_sensor_sdk.h)에는 조명 높이/수직각도 보정에 대응하는 명령이 없음.
    // 하드웨어 설치 시 카메라 자체에서 보정되는 것으로 보이므로 안전하게 무시(성공 처리)한다.
    UE_LOG(LogTemp, Verbose, TEXT("?? [EZ] ConfigureSensor는 EZSensorSDK에서 지원하지 않아 무시됨 (Height=%.2f, VAngle=%d)"),
        LightHeight, VAngleAdd);
    return true;
}

bool ACR2SensorManager::SetClubType(int32 ClubCode, bool bIsRoughTerrain)
{
    SelectClub = ClubCode;
    bLastIsRoughTerrain = bIsRoughTerrain;   // 멤버 변수 추가 필요 (StartSensorOperation_EZ 재시작 시 사용)

    return (ResolvedBackend == ESensorBackend::EZSensorSDK)
        ? SetClubType_EZ(ClubCode, bIsRoughTerrain)
        : SetClubType_CR2(ClubCode);
}


bool ACR2SensorManager::SetClubType_CR2(int32 ClubCode)
{
    UE_LOG(LogTemp, Log, TEXT("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"));
    UE_LOG(LogTemp, Log, TEXT("?? [SetClubType] Starting club type setup for code: %d"), ClubCode);


    // ========================================================
    // [사전 검증] 센서 초기화 상태 확인
    // ========================================================
    if (!bSensorInitialized || SensorHandle == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("? [SetClubType] Sensor not initialized"));
        return false;
    }

    // ========================================================
    // [Step 1] 클럽 타입 설정 (CR6CMD_USECLUB)
    // ========================================================
    UE_LOG(LogTemp, Log, TEXT("  [Step 1] Executing CR6CMD_USECLUB..."));

    CR2_result_t result = CR2_command_func(SensorHandle, CR2CMD_USECLUB, (PARAM_T)ClubCode, 0, 0, 0);

    if (result != CR2_OK)
    {
        UE_LOG(LogTemp, Error, TEXT("? [SetClubType] CR2CMD_USECLUB failed with code: 0x%08x"), result);
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("? [SetClubType] CR2CMD_USECLUB executed successfully"));

    // ========================================================
    // [Step 2] 센서 처리 완료 대기 (중요!)
    // ========================================================
    // ??? 이 부분이 핵심!
    // CR2 센서가 USECLUB 명령을 처리하는 데 걸리는 시간: ~50-100ms
    // 이 대기 시간이 없으면 다음 AREAALLOW 명령이 무시될 수 있음!
    // ========================================================

    const float COMMAND_DELAY = 0.1f;  // 100ms

    UE_LOG(LogTemp, Log, TEXT("  [Step 2] Waiting %.0fms for sensor to process USECLUB command..."), COMMAND_DELAY * 1000.0f);
  //  FPlatformProcess::Sleep(COMMAND_DELAY);
    UE_LOG(LogTemp, Log, TEXT("? [SetClubType] Wait complete, proceeding to area settings"));

    // ========================================================
    // [Step 3] 클럽별 영역 설정 파라미터 결정
    // ========================================================
    UE_LOG(LogTemp, Log, TEXT("  [Step 3] Determining area settings based on club code..."));

    PARAM_T TeeAreaFlag, IronAreaFlag, PuttingAreaFlag;
    FString ClubName = TEXT("UNKNOWN");

    switch (ClubCode)
    {
        // ─────────────────────────────────────────────────
        // ? Driver (코드 1)
        // ─────────────────────────────────────────────────
    case CR2CLUB_DRIVER:
    {
        TeeAreaFlag = (PARAM_T)1;      // ? TeeArea만 활성화
        IronAreaFlag = (PARAM_T)1;     // ? IronArea 비활성화
        PuttingAreaFlag = (PARAM_T)1;  // ? PuttingArea 비활성화

        ClubName = TEXT("DRIVER");

        UE_LOG(LogTemp, Log, TEXT("   ??? DRIVER: TeeArea=ON, IronArea=OFF, PuttingArea=OFF"));
        break;
    }

    // ─────────────────────────────────────────────────
    // ? Iron (모든 아이언 타입)
    // ─────────────────────────────────────────────────
    case CR2CLUB_IRON7:
    {
        TeeAreaFlag = (PARAM_T)1;      // ? TeeArea 비활성화
        IronAreaFlag = (PARAM_T)1;     // ? IronArea만 활성화
        PuttingAreaFlag = (PARAM_T)0;  // ? PuttingArea 비활성화

        ClubName = FString::Printf(TEXT("IRON(%d)"), ClubCode);

        UE_LOG(LogTemp, Log, TEXT("   ??? IRON: TeeArea=OFF, IronArea=ON, PuttingArea=OFF"));
        break;
    }

    // ─────────────────────────────────────────────────
    // ? Putter (코드 30)
    // ─────────────────────────────────────────────────
    case CR2CLUB_PUTTER:
    {
        TeeAreaFlag = (PARAM_T)1;      // ? TeeArea 비활성화
        IronAreaFlag = (PARAM_T)1;     // ? IronArea 비활성화
        PuttingAreaFlag = (PARAM_T)1;  // ? PuttingArea만 활성화

        ClubName = TEXT("PUTTER");

        UE_LOG(LogTemp, Log, TEXT("   ??? PUTTER: TeeArea=OFF, IronArea=OFF, PuttingArea=ON"));
        break;
    }

    // ─────────────────────────────────────────────────
    // ?? Default: 미지의 클럽 코드
    // ─────────────────────────────────────────────────
    default:
    {
        TeeAreaFlag = (PARAM_T)1;
        IronAreaFlag = (PARAM_T)1;
        PuttingAreaFlag = (PARAM_T)1;

        ClubName = FString::Printf(TEXT("UNKNOWN(%d)"), ClubCode);

        UE_LOG(LogTemp, Warning, TEXT("?? [SetClubType] Unknown club code: %d"), ClubCode);
        UE_LOG(LogTemp, Log, TEXT("   ??? FALLBACK: All areas enabled for safety"));
        break;
    }
    }

    // ========================================================
    // [Step 4] 센서 영역 설정 명령 실행 (CR6CMD_AREAALLOW)
    // ========================================================
    UE_LOG(LogTemp, Log, TEXT("  [Step 4] Executing CR6CMD_AREAALLOW..."));
    UE_LOG(LogTemp, Log, TEXT("   Parameters: Club=%d, TeeArea=%d, IronArea=%d, PuttingArea=%d"),
        ClubCode, TeeAreaFlag, IronAreaFlag, PuttingAreaFlag);

    result = CR2_command_func(
        SensorHandle,
        CR2CMD_AREAALLOW,
        (PARAM_T)ClubCode,      // p0: Club Code
        TeeAreaFlag,             // p1: TeeArea allow flag
        IronAreaFlag,            // p2: IronArea allow flag
        PuttingAreaFlag          // p3: PuttingArea allow flag
    );

    if (result != CR2_OK)
    {
        UE_LOG(LogTemp, Error, TEXT("? [SetClubType] CR6CMD_AREAALLOW failed with code: 0x%08x"), result);

        // 실패해도 계속 진행 (일부 센서에서는 AREAALLOW를 지원하지 않을 수 있음)
        UE_LOG(LogTemp, Warning, TEXT("?? [SetClubType] Continuing despite AREAALLOW failure"));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("? [SetClubType] CR6CMD_AREAALLOW executed successfully"));
    }


   // ConfigureSensor();

    // ========================================================
    // [Step 5] LED 색상 설정 (시각적 피드백)
    // ========================================================
   // UE_LOG(LogTemp, Log, TEXT("  [Step 5] Setting LED color..."));
  //  SetLEDColor();  // 클럽에 맞는 LED 색상 설정

    // ========================================================
    // [완료] 클럽 타입 및 영역 설정 완료
    // ========================================================

    UE_LOG(LogTemp, Log, TEXT("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"));
    UE_LOG(LogTemp, Log, TEXT("? [SetClubType] ? CLUB SETUP COMPLETE ?"));
    UE_LOG(LogTemp, Log, TEXT("   Club: %s (Code: %d)"), *ClubName, ClubCode);
    UE_LOG(LogTemp, Log, TEXT("   Areas: TeeArea=%d, IronArea=%d, PuttingArea=%d"),  TeeAreaFlag, IronAreaFlag, PuttingAreaFlag);
    int32 ret = BallCheck();
    UE_LOG(LogTemp, Log, TEXT("  ================================= BallCheck (Code: %d)  0=Noball , 1=TEE, 2 =IRON, 4=PUTTER"), ret);
    UE_LOG(LogTemp, Log, TEXT("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"));
    return true;
}

bool ACR2SensorManager::SetClubType_EZ(int32 ClubCode, bool bIsRoughTerrain)
{
    if (!bSensorInitialized || !EZSensor.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("? [EZ][SetClubType] Sensor not initialized"));
        return false;
    }

    CurrentEZGround = ClubCodeToEZGround(ClubCode, bIsRoughTerrain);

    if (bSensorStarted)
    {
        EZSensor->CancelSensing();
    }

    const bool bOk = EZSensor->StartSensing(CurrentEZGround, /*bAllowGroundChange=*/false);
    if (bOk)
    {
        bSensorStarted = true;
    }

    UE_LOG(LogTemp, Log, TEXT("?? [EZ][SetClubType] Club=%d, Rough=%s → Ground=%d, StartSensing=%s"),
        ClubCode, bIsRoughTerrain ? TEXT("true") : TEXT("false"), (int32)CurrentEZGround, bOk ? TEXT("OK") : TEXT("FAILED"));

    return bOk;
}

EEZGroundType ACR2SensorManager::ClubCodeToEZGround(int32 ClubCode, bool bIsRoughTerrain) const
{
    switch (ClubCode)
    {
    case CR2CLUB_DRIVER: return EEZGroundType::Tee;
    case CR2CLUB_PUTTER: return EEZGroundType::Green;
    default:             return bIsRoughTerrain ? EEZGroundType::Rough : EEZGroundType::Fairway; // IRON류: 실제 지면 반영
    }
}

EBallArea ACR2SensorManager::EZGroundToBallArea(EEZGroundType Ground) const
{
    switch (Ground)
    {
    case EEZGroundType::Tee:     return EBallArea::BALL_AREA_TEE;
    case EEZGroundType::Green:   return EBallArea::BALL_AREA_PUTTING;
    case EEZGroundType::Fairway:
    case EEZGroundType::Rough:
    case EEZGroundType::Sand:
    default:                     return EBallArea::BALL_AREA_IRON;
    }
}

int32 ACR2SensorManager::GetSensorStatus()
{
    return (ResolvedBackend == ESensorBackend::EZSensorSDK)
        ? GetSensorStatus_EZ()
        : GetSensorStatus_CR2();
}

int32 ACR2SensorManager::GetSensorStatus_CR2()
{
    if (!bSensorInitialized || SensorHandle == nullptr)
    {
        return 0;
    }

    int32 Status = 0;
    PARAM_T p0 = (PARAM_T)&Status;

    CR2_result_t result = CR2_command_func(SensorHandle, CR2CMD_SENSORSTATUS, p0, 0, 0, 0);
    if (result != CR2_OK)
    {
        UE_LOG(LogTemp, Error, TEXT("??Failed to get sensor status: 0x%08x"), result);
        return 0;
    }

    return Status;
}

int32 ACR2SensorManager::GetSensorStatus_EZ()
{
    // EZSensorSDK는 콜백(push) 기반이라 여기서는 콜백이 채워둔 캐시값을 그대로 반환한다.
    // (HandleEZSensorStatusChanged / HandleEZBallStatusChanged 참고)
    return CachedEZStatus;
}

FCR2BallPosition ACR2SensorManager::GetBallPosition()
{
    return (ResolvedBackend == ESensorBackend::EZSensorSDK)
        ? GetBallPosition_EZ()
        : GetBallPosition_CR2();
}

FCR2BallPosition ACR2SensorManager::GetBallPosition_CR2()
{
    FCR2BallPosition Result;

    if (!bSensorInitialized || SensorHandle == nullptr)
    {
        return Result;
    }

    CR2_ballposition_native TeeBallPos, IronBallPos, PuttingBallPos;
    FMemory::Memzero(&TeeBallPos, sizeof(TeeBallPos));
    FMemory::Memzero(&IronBallPos, sizeof(IronBallPos));
    FMemory::Memzero(&PuttingBallPos, sizeof(PuttingBallPos));

    PARAM_T p0 = (PARAM_T)&TeeBallPos;
    PARAM_T p1 = (PARAM_T)&IronBallPos;
    PARAM_T p2 = (PARAM_T)&PuttingBallPos;

    CR2_result_t ApiResult = CR2_command_func(SensorHandle, CR2CMD_BALLPOSITION, p0, p1, p2, 0);
    if (ApiResult != CR2_OK)
    {
        UE_LOG(LogTemp, Error, TEXT("??Failed to get ball position: 0x%08x"), ApiResult);
        return Result;
    }

    // 가장 우선순위가 높은 위치 정보 사용 (Tee > Iron > Putting)
    if (TeeBallPos.ballexist)
    {
        Result.bBallExist = true;
        Result.bShotResult = (TeeBallPos.shotresult != 0);
        Result.Position = FVector(TeeBallPos.x, TeeBallPos.y, TeeBallPos.z);
        UE_LOG(LogTemp, Log, TEXT("? [GetBallPosition] Check TEE POS ?"));
    }
    if (IronBallPos.ballexist)
    {
        Result.bBallExist = true;
        Result.bShotResult = (IronBallPos.shotresult != 0);
        Result.Position = FVector(IronBallPos.x, IronBallPos.y, IronBallPos.z);
        UE_LOG(LogTemp, Log, TEXT("? [GetBallPosition] Check IRON POS ?"));
    }
   if (PuttingBallPos.ballexist)
    {
        Result.bBallExist = true;
        Result.bShotResult = (PuttingBallPos.shotresult != 0);
        Result.Position = FVector(PuttingBallPos.x, PuttingBallPos.y, PuttingBallPos.z);
        UE_LOG(LogTemp, Log, TEXT("? [GetBallPosition] Check PUTTER POS ?"));
    }

    return Result;
}

FCR2BallPosition ACR2SensorManager::GetBallPosition_EZ()
{
    // ez_sensor_sdk.h에는 좌표(x,y,z)를 직접 조회하는 API가 없음(콜백으로 받는 ball status/shot info만 존재).
    // 캐시된 Ex 결과(상태/영역)만 채워서 돌려준다. 실제 좌표는 Position=ZeroVector로 둠.
    FCR2BallPosition Result;
    Result.bBallExist = CachedEZBallPositionEx.bBallExist;
    Result.bShotResult = CachedEZBallPositionEx.bShotResult;
    Result.Position = CachedEZBallPositionEx.Position; // 항상 ZeroVector (EZSensorSDK는 좌표 미제공)
    return Result;
}

FCR2BallPositionEx ACR2SensorManager::GetBallPositionEx()
{
    return (ResolvedBackend == ESensorBackend::EZSensorSDK)
        ? GetBallPositionEx_EZ()
        : GetBallPositionEx_CR2();
}

FCR2BallPositionEx ACR2SensorManager::GetBallPositionEx_CR2()
{
    FCR2BallPositionEx Result;

    // ========================================================
    // [사전 검증]
    // ========================================================
    if (!bSensorInitialized || SensorHandle == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("? [GetBallPositionEx] Sensor not initialized"));
        return Result;
    }

    // ========================================================
    // [Step 1] 3개 영역의 볼 위치 모두 조회
    // ========================================================
    CR2_ballposition_native TeeBallPos, IronBallPos, PuttingBallPos;
    FMemory::Memzero(&TeeBallPos, sizeof(TeeBallPos));
    FMemory::Memzero(&IronBallPos, sizeof(IronBallPos));
    FMemory::Memzero(&PuttingBallPos, sizeof(PuttingBallPos));

    PARAM_T p0 = (PARAM_T)&TeeBallPos;
    PARAM_T p1 = (PARAM_T)&IronBallPos;
    PARAM_T p2 = (PARAM_T)&PuttingBallPos;

    CR2_result_t ApiResult = CR2_command_func(SensorHandle, CR2CMD_BALLPOSITION, p0, p1, p2, 0);

    if (ApiResult != CR2_OK)
    {
        UE_LOG(LogTemp, Error, TEXT("? [GetBallPositionEx] Failed to get ball position: 0x%08x"), ApiResult);
        return Result;
    }

    // ========================================================
    // [Step 2] 우선순위에 따라 볼이 감지된 영역 판단
    // ========================================================
    // 우선순위: Tee(드라이버) > Iron(아이언) > Putting(퍼터)

    UE_LOG(LogTemp, Log, TEXT("?? [GetBallPositionEx] Checking ball positions..."));
    UE_LOG(LogTemp, Log, TEXT("   Tee: %s"), TeeBallPos.ballexist ? TEXT("? YES") : TEXT("? NO"));
    UE_LOG(LogTemp, Log, TEXT("   Iron: %s"), IronBallPos.ballexist ? TEXT("? YES") : TEXT("? NO"));
    UE_LOG(LogTemp, Log, TEXT("   Putting: %s"), PuttingBallPos.ballexist ? TEXT("? YES") : TEXT("? NO"));

    // ─────────────────────────────────────────────────
    // Tee 영역 (드라이버)
    // ─────────────────────────────────────────────────
    if (TeeBallPos.ballexist)
    {
        Result.bBallExist = true;
        Result.bShotResult = (TeeBallPos.shotresult != 0);
        Result.Position = FVector(TeeBallPos.x, TeeBallPos.y, TeeBallPos.z);
        Result.BallArea = EBallArea::BALL_AREA_TEE;

        UE_LOG(LogTemp, Log, TEXT("?? [GetBallPositionEx] ? Ball in TEE AREA (Driver)"));
        UE_LOG(LogTemp, Log, TEXT("   Position: X=%.1f, Y=%.1f, Z=%.1f"),
            Result.Position.X, Result.Position.Y, Result.Position.Z);

        return Result;
    }

    // ─────────────────────────────────────────────────
    // Iron 영역 (아이언)
    // ─────────────────────────────────────────────────
    if (IronBallPos.ballexist)
    {
        Result.bBallExist = true;
        Result.bShotResult = (IronBallPos.shotresult != 0);
        Result.Position = FVector(IronBallPos.x, IronBallPos.y, IronBallPos.z);
        Result.BallArea = EBallArea::BALL_AREA_IRON;

        UE_LOG(LogTemp, Log, TEXT("?? [GetBallPositionEx] ? Ball in IRON AREA"));
        UE_LOG(LogTemp, Log, TEXT("   Position: X=%.1f, Y=%.1f, Z=%.1f"),
            Result.Position.X, Result.Position.Y, Result.Position.Z);

        return Result;
    }

    // ─────────────────────────────────────────────────
    // Putting 영역 (퍼터)
    // ─────────────────────────────────────────────────
    if (PuttingBallPos.ballexist)
    {
        Result.bBallExist = true;
        Result.bShotResult = (PuttingBallPos.shotresult != 0);
        Result.Position = FVector(PuttingBallPos.x, PuttingBallPos.y, PuttingBallPos.z);
        Result.BallArea = EBallArea::BALL_AREA_PUTTING;

        UE_LOG(LogTemp, Log, TEXT("?? [GetBallPositionEx] ? Ball in PUTTING AREA"));
        UE_LOG(LogTemp, Log, TEXT("   Position: X=%.1f, Y=%.1f, Z=%.1f"),
            Result.Position.X, Result.Position.Y, Result.Position.Z);

        return Result;
    }

    // ─────────────────────────────────────────────────
    // 어느 영역에도 볼이 없음
    // ─────────────────────────────────────────────────
    UE_LOG(LogTemp, Warning, TEXT("?? [GetBallPositionEx] No ball detected in any area"));
    Result.BallArea = EBallArea::BALL_AREA_NONE;

    return Result;
}

FCR2BallPositionEx ACR2SensorManager::GetBallPositionEx_EZ()
{
    // 콜백(HandleEZBallStatusChanged)에서 채워둔 캐시를 그대로 반환.
    // EZSensorSDK는 실좌표를 안 주므로 Position은 항상 ZeroVector.
    return CachedEZBallPositionEx;
}

FCR2ShotDataEx ACR2SensorManager::GetLastShotDataEx(bool bClearResult)
{
    return (ResolvedBackend == ESensorBackend::EZSensorSDK)
        ? GetLastShotDataEx_EZ(bClearResult)
        : GetLastShotDataEx_CR2(bClearResult);
}

FCR2ShotDataEx ACR2SensorManager::GetLastShotDataEx_CR2(bool bClearResult)
{
    FCR2ShotDataEx Result;

    if (!bSensorInitialized || SensorHandle == nullptr)
    {
        return Result;
    }

    CR2_shotdata_ballEx_native ShotDataEx;
    FMemory::Memzero(&ShotDataEx, sizeof(ShotDataEx));

    PARAM_T p0 = bClearResult ? 1 : 0;
    PARAM_T p1 = (PARAM_T)&ShotDataEx;

    CR2_result_t ApiResult = CR2_command_func(SensorHandle, CR2CMD_SHOTRESULTEX, p0, p1, 0, 0);
    if (ApiResult != CR2_OK)
    {
        UE_LOG(LogTemp, Error, TEXT("??Failed to get shot data ex: 0x%08x"), ApiResult);
        return Result;
    }

    // 데이터 변환
    Result.bValid = (ShotDataEx.valid != 0);
    Result.Incline = (float)ShotDataEx.incline;
    Result.Azimuth = (float)ShotDataEx.azimuth;
    Result.VMag = (float)ShotDataEx.vmag;
    Result.ShotAssurance = (float)ShotDataEx.shotAssurance;
    Result.SpinMag = (float)ShotDataEx.spinmag;
    Result.SpinAxis = FVector((float)ShotDataEx.spinaxis[0], (float)ShotDataEx.spinaxis[1], (float)ShotDataEx.spinaxis[2]);
    Result.SpinAssurance = (float)ShotDataEx.spinAssurance;

    return Result;
}

FCR2ShotDataEx ACR2SensorManager::GetLastShotDataEx_EZ(bool bClearResult)
{
    // ez_shot_info에는 ShotAssurance/SpinAssurance/SpinAxis 같은 신뢰도·축 정보가 없음.
    // HandleEZShotInfoReceived에서 채워둔 근사값(캐시)을 반환한다.
    FCR2ShotDataEx Result = CachedEZShotDataEx;
    if (bClearResult)
    {
        CachedEZShotDataEx = FCR2ShotDataEx();
    }
    return Result;
}

FString ACR2SensorManager::GetDLLVersion()
{
    return (ResolvedBackend == ESensorBackend::EZSensorSDK)
        ? GetDLLVersion_EZ()
        : GetDLLVersion_CR2();
}

FString ACR2SensorManager::GetDLLVersion_CR2()
{
    if (!bSensorInitialized || SensorHandle == nullptr)
    {
        return TEXT("Unknown");
    }

    PARAM_T MajorV = 0xFFFFFFFF;
    PARAM_T MinorV = 0xFFFFFFFF;
    PARAM_T BuildNum = 0xFFFFFFFF;

    PARAM_T p0 = (PARAM_T)&MajorV;
    PARAM_T p1 = (PARAM_T)&MinorV;
    PARAM_T p2 = (PARAM_T)&BuildNum;

    CR2_result_t result = CR2_command_func(SensorHandle, CR2CMD_DLLVERSION, p0, p1, p2, 0);
    if (result != CR2_OK)
    {
        return TEXT("Error");
    }

    return FString::Printf(TEXT("%d.%d.%d"), (int32)MajorV, (int32)MinorV, (int32)BuildNum);
}

FString ACR2SensorManager::GetDLLVersion_EZ()
{
    // ez_sensor_sdk.h에는 DLL 버전 조회 API가 없음.
    return TEXT("EZSensorSDK");
}

void ACR2SensorManager::CheckSensorStatus()
{
    SCOPE_CYCLE_COUNTER(STAT_SensorCheck);
    int32 CurrentStatus = GetSensorStatus();

    if (CurrentStatus != LastSensorStatus)
    {
        // 상태 변경시 LED 색상 업데이트
        SetLEDColor(CurrentStatus);

        // 상태 변경 이벤트 발생
        OnSensorStatusChanged.Broadcast(CurrentStatus);

        // 상태별 처리
        switch (CurrentStatus)
        {
        case CR2STATUS_READY:
        {
            FCR2BallPositionEx BallPos = GetBallPositionEx();
            //if (BallPos.bBallExist)
            //{
            //    OnBallReady.Broadcast(BallPos);
            //    UE_LOG(LogTemp, Log, TEXT("??Ball ready at position: %s"), *BallPos.Position.ToString());
            //}
            if (IsBallAreaMatchesClub(BallPos.BallArea, SelectClub))
            {
                FCR2BallPosition tBallPos = GetBallPosition();
                OnBallReady.Broadcast(tBallPos);
                UE_LOG(LogTemp, Log, TEXT("??Ball ready at position: %s"), *BallPos.Position.ToString());
            }
            else
            {
                OnSensorStatusChanged.Broadcast(CR2STATUS_NOBALL);
                UE_LOG(LogTemp, Warning, TEXT("Ball removed during READY state → forcing NOBALL"));
            }
        }
        break;

        case CR2STATUS_DISCONNECT:
         //   UE_LOG(LogTemp, Warning, TEXT("??Sensor disconnected"));
            break;

        case CR2STATUS_BIGSHADOW:
            UE_LOG(LogTemp, Warning, TEXT("??Check sensor light"));
            break;

        case CR2STATUS_NOBALL:
            OnSensorStatusChanged.Broadcast(CR2STATUS_NOBALL);
           // UE_LOG(LogTemp, Log, TEXT("??NoBall to proper area"));
            break;
        }

        LastSensorStatus = CurrentStatus;
    }
}

int CALLBACK ACR2SensorManager::SensorCallback(HAND h, U32 status, void* psd, PARAM_T userparam)
{
    ACR2SensorManager* Self = reinterpret_cast<ACR2SensorManager*>(userparam);
    if (Self && Self->bSensorInitialized)
    {
        Self->HandleSensorCallback(status, psd);
        return CR2_OK;
    }

    if (Instance && Instance->bSensorInitialized)
    {
        Instance->HandleSensorCallback(status, psd);
    }

    return CR2_OK;
}

void ACR2SensorManager::HandleSensorCallback(U32 status, void* psd)
{
    switch (status)
    {
    case CR2STATUS_GOODSHOT:
    {
        UE_LOG(LogTemp, Log, TEXT("??Good shot detected!"));

        // 좋은 샷 감지시 파란색 LED로 변경
        SetLEDColor(CR2STATUS_GOODSHOT);

        if (psd != nullptr)
        {
            // 기본 샷 데이터 처리
            CR2_shotdata_native* NativeShotData = (CR2_shotdata_native*)psd;
            FCR2ShotData ShotData;

            ShotData.BallSpeedX10 = NativeShotData->ballspeedX10;
            ShotData.ClubSpeedBeforeX10 = NativeShotData->clubspeed_BX10;
            ShotData.ClubSpeedAfterX10 = NativeShotData->clubspeed_AX10;
            ShotData.ClubPathX10 = NativeShotData->clubpathX10;
            ShotData.ClubFaceAngleX10 = NativeShotData->clubfaceangleX10;
            ShotData.SideSpin = NativeShotData->sidespin;
            ShotData.BackSpin = NativeShotData->backspin;
            ShotData.AzimuthX10 = NativeShotData->azimuthX10;
            ShotData.InclineX10 = NativeShotData->inclineX10;

            // 이벤트 발생 (게임 스레드에서 실행)
            AsyncTask(ENamedThreads::GameThread, [this, ShotData]()
                {
                    OnShotDetected.Broadcast(ShotData);
                });

            // 확장 샷 데이터도 가져오기
            FCR2ShotDataEx ShotDataEx = GetLastShotDataEx(false);
            if (ShotDataEx.bValid)
            {
                AsyncTask(ENamedThreads::GameThread, [this, ShotDataEx]()
                    {
                        OnShotDetectedEx.Broadcast(ShotDataEx);
                    });
            }
        }
    }
    break;

    case CR2STATUS_TRIALSHOT:
        UE_LOG(LogTemp, Log, TEXT("??Trial shot detected - discarded"));
        // 시행샷 감지시 노란색 LED로 변경
        SetLEDColor(CR2STATUS_TRIALSHOT);
        break;

    case CR2STATUS_DISCONNECT:
        UE_LOG(LogTemp, Warning, TEXT("??Sensor disconnected in callback"));
        // 연결 끊김시 빨간색 LED로 변경
        SetLEDColor(CR2STATUS_DISCONNECT);
        break;

    default:
        UE_LOG(LogTemp, Log, TEXT("??Unknown callback status: 0x%08x"), status);
        break;
    }

    // 센서 재시작
    if (SensorHandle != nullptr)
    {
        CR2_command_func(SensorHandle, CR2CMD_OPERATION_RESTART, 0, 0, 0, 0);

        // 재시작 후 잠시 대기하고 현재 상태에 맞는 LED 색상으로 복원
        AsyncTask(ENamedThreads::GameThread, [this]()
            {
                FTimerHandle TimerHandle;
                GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
                    {
                        SetLEDColor(); // 현재 상태에 맞는 색상으로 복원
                    }, 1.0f, false); // 1초 후 실행
            });
    }
}

void ACR2SensorManager::SetEnableLED()
{
    // 이 함수는 더 이상 사용하지 않음. SetLEDColor()를 사용하세요.
    UE_LOG(LogTemp, Warning, TEXT("??SetEnableLED() is deprecated. Use SetLEDColor() instead."));
    SetLEDColor();
}

void ACR2SensorManager::SetDisableLED()
{
  
    PARAM_T p0 = 0; // OFF ALL cameras    
    CR2_result_t result = CR2_command_func(SensorHandle, CR2CMD_RGB0_RGB, p0, 0, 0, 0);

    if (result != CR2_OK)
    {
        UE_LOG(LogTemp, Warning, TEXT("??Failed to set LED OFF"));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("??LED OFF status:"));
    }
}

void ACR2SensorManager::ResetSensorStatus()
{
    if (!bSensorInitialized || SensorHandle == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot reset sensor status - not initialized"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Manual ResetSensorStatus() called - forcing status recheck"));

    // 1. 현재 상태 강제로 다시 읽기
    LastSensorStatus = 0;  // 강제로 이전 상태 지워서 재비교 유도
    CheckSensorStatus();

    // 2. READY 상태라면 즉시 볼 위치 재확인
    if (LastSensorStatus == CR2STATUS_READY)
    {
        BallReadyTimer = BallReadyRecheckInterval;  // 즉시 재체크되게 타이머 강제 만료
    }

    // 3. LED도 현재 상태에 맞게 다시 설정
    SetLEDColor();
}


void ACR2SensorManager::CheckBallPresenceInReadyState()
{
    if (!bSensorInitialized || SensorHandle == nullptr) return;

    FCR2BallPosition CurrentBallPos = GetBallPosition();

    // 이전에 볼이 있었는데 지금 없으면 NOBALL 상태로 강제 전환 유도
    if (!CurrentBallPos.bBallExist)
    {
        // 센서가 READY인데 볼이 없으면 상태를 강제로 갱신하도록 유도
        // 직접 상태를 바꿀 순 없지만, LED + 이벤트로 사용자에게 알림
        SetLEDColor(CR2STATUS_NOBALL);
        OnSensorStatusChanged.Broadcast(CR2STATUS_NOBALL);
        LastSensorStatus = CR2STATUS_NOBALL;  // 강제로 상태 추적 보정

        UE_LOG(LogTemp, Warning, TEXT("Ball removed during READY state → forcing NOBALL"));
    }
    else if (CurrentBallPos.bBallExist && !CurrentBallPos.bShotResult)
    {
        // 볼이 여전히 있고, 샷 결과 없으면 READY 유지하면서 이벤트 재발생 가능
        OnBallReady.Broadcast(CurrentBallPos);
        UE_LOG(LogTemp, Log, TEXT("Ball still in position (reconfirmed in READY)"));
    }
}




//?? GREEN(CR2STATUS_READY) : 볼이 준비되어 샷을 칠 수 있는 상태
//?? BLUE(CR2STATUS_GOODSHOT) : 정상적인 샷이 감지된 상태
//?? YELLOW(CR2STATUS_TRIALSHOT) : 연습샷이나 무효샷이 감지된 상태
//?? RED(CR2STATUS_DISCONNECT) : 센서 연결이 끊어진 상태
//?? ORANGE(CR2STATUS_BIGSHADOW) : 센서 조명에 문제가 있는 상태
//?? PURPLE(CR2STATUS_NOBALL) : 지정된 위치에 볼이 없는 상태
//? WHITE : 알 수 없는 상태



void ACR2SensorManager::SetLEDColor(int32 Status)
{
    if (!bSensorInitialized || SensorHandle == nullptr)
    {
        return;
    }

    if (ResolvedBackend == ESensorBackend::EZSensorSDK)
    {
        // EZSensorSDK(ez_sensor_sdk.h)에는 LED 하드웨어 제어 명령이 없음 — 안전하게 무시.
        return;
    }

    PARAM_T LEDColor = LED_OFF;
    FString StatusName = TEXT("UNKNOWN");

    // 현재 상태가 지정되지 않은 경우 센서 상태를 가져옴
    if (Status == -1)
    {
        Status = GetSensorStatus();
    }

    // 상태별 LED 색상 설정
    switch (Status)
    {
    case CR2STATUS_READY:
        LEDColor = LED_GREEN;
        StatusName = TEXT("READY");
        break;

    case CR2STATUS_GOODSHOT:
        LEDColor = LED_BLUE;
        StatusName = TEXT("GOOD SHOT");
        break;

    case CR2STATUS_TRIALSHOT:
        LEDColor = LED_YELLOW;
        StatusName = TEXT("TRIAL SHOT");
        break;

    case CR2STATUS_DISCONNECT:
        LEDColor = LED_RED;
        StatusName = TEXT("DISCONNECT");
        break;

    case CR2STATUS_BIGSHADOW:
        LEDColor = LED_ORANGE;
        StatusName = TEXT("BIG SHADOW");
        break;

    case CR2STATUS_NOBALL:
        LEDColor = LED_PURPLE;
        StatusName = TEXT("NO BALL");
        break;

    default:
        LEDColor = LED_WHITE;
        StatusName = TEXT("UNKNOWN");
        break;
    }

    // LED 설정
    PARAM_T p0 = 3; // Both cameras
    PARAM_T p1 = LEDColor;

    CR2_result_t result = CR2_command_func(SensorHandle, CR2CMD_RGB0_RGB, p0, p1, 0, 0);

    if (result != CR2_OK)
    {
      //  UE_LOG(LogTemp, Warning, TEXT("??Failed to set LED color for status %s: 0x%08x"), *StatusName, result);
    }
    else
    {
      //  UE_LOG(LogTemp, Log, TEXT("??LED color set to %s for status: %s (0x%08x)"),
      //      *GetLEDColorName(LEDColor), *StatusName, LEDColor);
    }
}

// LED 색상 이름을 반환하는 헬퍼 함수 (디버깅용)
FString ACR2SensorManager::GetLEDColorName(PARAM_T Color)
{
    switch (Color)
    {
    case LED_OFF: return TEXT("OFF");
    case LED_RED: return TEXT("RED");
    case LED_GREEN: return TEXT("GREEN");
    case LED_BLUE: return TEXT("BLUE");
    case LED_YELLOW: return TEXT("YELLOW");
    case LED_PURPLE: return TEXT("PURPLE");
    case LED_CYAN: return TEXT("CYAN");
    case LED_WHITE: return TEXT("WHITE");
    case LED_ORANGE: return TEXT("ORANGE");
    default: return FString::Printf(TEXT("CUSTOM(0x%08x)"), Color);
    }
}


/**
 * @brief 볼 영역이 현재 설정된 클럽과 일치하는지 확인
 * @return true if 볼 영역 == 클럽 설정, false otherwise
 *
 * @details
 * 예:
 * - 볼이 IRON 영역 + 클럽 설정이 IRON → true ?
 * - 볼이 IRON 영역 + 클럽 설정이 DRIVER → false ?
 */
bool ACR2SensorManager::IsBallAreaMatchesCurrentClub()
{
    FCR2BallPositionEx BallPosEx = GetBallPositionEx();
    return IsBallAreaMatchesClub(BallPosEx.BallArea, SelectClub);
}


/**
 * @brief 영역과 클럽이 일치하는지 확인 (정적 함수)
 * @param BallArea 센서가 감지한 볼의 영역
 * @param ClubCode 현재 설정된 클럽 코드
 * @return true if 일치, false if 불일치
 */
bool ACR2SensorManager::IsBallAreaMatchesClub(EBallArea BallArea, int32 ClubCode)
{
    FString AreaName = TEXT("UNKNOWN");
    FString ClubName = TEXT("UNKNOWN");
    bool bMatches = false;

    // ===== Step 1: 영역명 결정 =====
    switch (BallArea)
    {
    case EBallArea::BALL_AREA_TEE:
        AreaName = TEXT("TEE");
        break;

    case EBallArea::BALL_AREA_IRON:
        AreaName = TEXT("IRON");
        break;

    case EBallArea::BALL_AREA_PUTTING:
        AreaName = TEXT("PUTTING");
        break;

    case EBallArea::BALL_AREA_NONE:
        AreaName = TEXT("NONE");
        break;

    default:
        AreaName = FString::Printf(TEXT("INVALID(%d)"), (int32)BallArea);
        break;
    }

    // ===== Step 2: 2분기 메칭 로직 =====

    if (ClubCode == CR2CLUB_DRIVER)
    {
        // ===== Driver: TEE 또는 IRON =====
        ClubName = TEXT("DRIVER");
        bMatches = (BallArea == EBallArea::BALL_AREA_TEE);
    }
    if (ClubCode == CR2CLUB_IRON7)
    {
        // ===== Putter: PUTTING만 =====
        ClubName = TEXT("IRON");
        bMatches = (BallArea == EBallArea::BALL_AREA_IRON ||BallArea == EBallArea::BALL_AREA_PUTTING);
    }
    if (ClubCode == CR2CLUB_PUTTER)
    {
        // ===== Putter: PUTTING만 =====
        ClubName = TEXT("PUTTER");
        bMatches = (BallArea == EBallArea::BALL_AREA_IRON || BallArea == EBallArea::BALL_AREA_PUTTING);
    }


    // ===== Step 3: 로깅 =====
    if (bMatches)
    {
        UE_LOG(LogTemp, Log,
            TEXT("? [IsBallAreaMatchesClub] MATCH: Area=%s, Club=%s"),
            *AreaName, *ClubName);
    }
    else
    {
        UE_LOG(LogTemp, Warning,
            TEXT("? [IsBallAreaMatchesClub] MISMATCH: Area=%s ≠ Club=%s"),
            *AreaName, *ClubName);
    }

    return bMatches;
}

/**
 * @brief 각 영역(Tee/Iron/Putting)의 볼 감지 상태를 확인하고 비트마스크로 반환
 * @return 비트마스크 (0x0001=TEE, 0x0002=IRON, 0x0004=PUTTING)
 *
 * @details
 * 센서에서 3개 영역의 볼 위치를 동시에 확인합니다.
 * - Tee 영역: 드라이버 샷을 위한 ティ 영역
 * - Iron 영역: 페어웨이/롱게임 영역
 * - Putting 영역: 그린/퍼팅 영역
 *
 * 반환값:
 * - 0x0001 (1): Tee 영역에 볼 감지
 * - 0x0002 (2): Iron 영역에 볼 감지
 * - 0x0004 (4): Putting 영역에 볼 감지
 * - 0x0000 (0): 모든 영역에서 볼 미감지
 *
 * 예시:
 * - return 1: Tee만 감지
 * - return 3: Tee + Iron 감지 (0x0001 | 0x0002)
 * - return 5: Tee + Putting 감지 (0x0001 | 0x0004)
 * - return 7: 모든 영역 감지 (0x0001 | 0x0002 | 0x0004)
 */
int32 ACR2SensorManager::BallCheck()
{
    return (ResolvedBackend == ESensorBackend::EZSensorSDK)
        ? BallCheck_EZ()
        : BallCheck_CR2();
}

int32 ACR2SensorManager::BallCheck_CR2()
{
    // ===== [사전 검증] =====


    UE_LOG(LogTemp, Log, TEXT("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"));
    UE_LOG(LogTemp, Log, TEXT("?? [BallCheck] START - Checking all ball areas"));

    // ===== [Step 1: 센서 명령 준비] =====
    int32 CheckResult = 0;  // 비트마스크 결과

    CR2_ballposition_native TeeBallPos, IronBallPos, PuttingBallPos;
    FMemory::Memzero(&TeeBallPos, sizeof(TeeBallPos));
    FMemory::Memzero(&IronBallPos, sizeof(IronBallPos));
    FMemory::Memzero(&PuttingBallPos, sizeof(PuttingBallPos));

    PARAM_T p0 = (PARAM_T)&TeeBallPos;
    PARAM_T p1 = (PARAM_T)&IronBallPos;
    PARAM_T p2 = (PARAM_T)&PuttingBallPos;

    // 포인터를 U64/U32로 변환 (플랫폼별 처리)


    // ===== [Step 2: 센서 명령 실행] =====
    CR2_result_t ApiResult = CR2_command_func(SensorHandle, CR2CMD_BALLPOSITION, p0, p1, p2, 0);
    if (ApiResult != CR2_OK)
    {
        UE_LOG(LogTemp, Error, TEXT("??Failed to get ball position: 0x%08x"), ApiResult);
        return CheckResult;
    }


    UE_LOG(LogTemp, Log, TEXT("? [Step 2] Sensor command executed successfully"));

    // ===== [Step 3: Tee 영역 검사] =====
    if (TeeBallPos.ballexist)
    {
        // 밀리미터 단위 좌표를 센티미터로 변환
        float TeeX = TeeBallPos.x / 1000.0f;
        float TeeY = TeeBallPos.y / 1000.0f;
        float TeeZ = TeeBallPos.z / 1000.0f;

        CheckResult |= BALL_AREA_TEE_BIT;  // 0x0001 설정

        UE_LOG(LogTemp, Log, TEXT("   ? BALL DETECTED in TEE area"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("   ??  No ball in TEE area"));
    }

    // ===== [Step 4: Iron 영역 검사] =====
    if (IronBallPos.ballexist)
    {
        // 밀리미터 단위 좌표를 센티미터로 변환
        float IronX = IronBallPos.x / 1000.0f;
        float IronY = IronBallPos.y / 1000.0f;
        float IronZ = IronBallPos.z / 1000.0f;

        CheckResult |= BALL_AREA_IRON_BIT;  // 0x0002 설정

        UE_LOG(LogTemp, Log, TEXT("   ? BALL DETECTED in IRON area"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("   ??  No ball in IRON area"));
    }

    // ===== [Step 5: Putting 영역 검사] =====
    if (PuttingBallPos.ballexist)
    {
        // 밀리미터 단위 좌표를 센티미터로 변환
        float PuttingX = PuttingBallPos.x / 1000.0f;
        float PuttingY = PuttingBallPos.y / 1000.0f;
        float PuttingZ = PuttingBallPos.z / 1000.0f;

        CheckResult |= BALL_AREA_PUTTING_BIT;  // 0x0004 설정

        UE_LOG(LogTemp, Log, TEXT("   ? BALL DETECTED in PUTTING area"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("   ??  No ball in PUTTING area"));
    }
    // ===== [Step 6: 최종 결과 요약] =====


    // 비트마스크를 읽기 쉬운 형태로 표시
    FString AreaInfo = GetBallCheckResultInfo(CheckResult);

    UE_LOG(LogTemp, Log,        TEXT("   Detection Summary: %s"),        *AreaInfo);
    UE_LOG(LogTemp, Log,        TEXT("   Bitmask Result: 0x%04X (%d)"),        CheckResult, CheckResult);

    if (CheckResult == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("   ??  WARNING: No ball detected in any area!"));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("   ??  Ball detection completed successfully"));
    }

    UE_LOG(LogTemp, Log, TEXT("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"));

    return CheckResult;
}

/**
 * @brief BallCheck 결과의 비트마스크를 읽기 쉬운 문자열로 변환
 * @param BallCheckResult BallCheck()의 반환값
 * @return 감지된 영역을 표시하는 문자열
 *
 * 예시:
 * - 1 (0x0001) → "?? TEE"
 * - 2 (0x0002) → "?? IRON"
 * - 3 (0x0001|0x0002) → "?? TEE + IRON"
 * - 7 (0x0001|0x0002|0x0004) → "?? TEE + IRON + PUTTING"
 */
FString ACR2SensorManager::GetBallCheckResultInfo(int32 BallCheckResult) const
{
    FString ResultString = TEXT("");
    bool bIsFirstArea = true;

    // Tee 영역 확인
    if (BallCheckResult & BALL_AREA_TEE_BIT)
    {
        if (!bIsFirstArea) ResultString += TEXT(" + ");
        ResultString += TEXT("?? TEE");
        bIsFirstArea = false;
    }

    // Iron 영역 확인
    if (BallCheckResult & BALL_AREA_IRON_BIT)
    {
        if (!bIsFirstArea) ResultString += TEXT(" + ");
        ResultString += TEXT("?? IRON");
        bIsFirstArea = false;
    }

    // Putting 영역 확인
    if (BallCheckResult & BALL_AREA_PUTTING_BIT)
    {
        if (!bIsFirstArea) ResultString += TEXT(" + ");
        ResultString += TEXT("?? PUTTING");
        bIsFirstArea = false;
    }

    // 아무 영역도 감지되지 않음
    if (ResultString.IsEmpty())
    {
        ResultString = TEXT("?? NO DETECTION");
    }

    return ResultString;
}

/**
 * @brief BallCheck 결과를 확인하는 헬퍼 함수
 * @param BallCheckResult BallCheck()의 반환값
 * @param Area 확인할 영역
 * @return true if 해당 영역에서 볼 감지됨, false if 감지 안 됨
 *
 * 예시:
 * int32 result = BallCheck();
 * if (IsBallDetectedInArea(result, BALL_AREA_TEE_BIT)) {
 *     // Tee 영역에서 볼 감지됨
 * }
 */
bool ACR2SensorManager::IsBallDetectedInArea(int32 BallCheckResult, int32 AreaBit) const
{
    return (BallCheckResult & AreaBit) != 0;
}

/**
 * @brief EZSensorSDK 백엔드용 BallCheck.
 *
 * CR2와 달리 EZSensorSDK는 한 번에 한 ground만 감시하므로 "3곳 동시 감지"는 불가능하다.
 * 대신 SetClubType()/StartSensorOperation() 시점에 정해진 CurrentEZGround 하나에 대해서만
 * 마지막으로 캐시된 ball-ready 상태를 비트로 변환해 반환한다.
 * (실제 코스 운영상 3개 영역을 동시에 봐야 하는 시나리오가 있다면 별도로 알려주세요 - 이 부분은
 *  EZSensorSDK 헤더(ez_sensor_sdk.h)의 API 범위로는 동시 멀티 영역 감지가 불가능합니다.)
 */
int32 ACR2SensorManager::BallCheck_EZ()
{
    int32 CheckResult = 0;

    const bool bBallReadyOnCurrentGround = (CachedEZStatus == CR2STATUS_READY);

    if (bBallReadyOnCurrentGround)
    {
        switch (CurrentEZGround)
        {
        case EEZGroundType::Tee:
            CheckResult |= BALL_AREA_TEE_BIT;
            break;
        case EEZGroundType::Green:
            CheckResult |= BALL_AREA_PUTTING_BIT;
            break;
        default: // Fairway / Rough / Sand
            CheckResult |= BALL_AREA_IRON_BIT;
            break;
        }
    }

    UE_LOG(LogTemp, Log, TEXT("?? [EZ][BallCheck] Ground=%d, Ready=%d → Bitmask=0x%04X (%s)"),
        (int32)CurrentEZGround, bBallReadyOnCurrentGround, CheckResult, *GetBallCheckResultInfo(CheckResult));

    return CheckResult;
}

// =====================================================================================
// UEZSensorSubsystem 델리게이트 핸들러
// 플러그인 쪽에서 이미 게임 스레드로 마샬링해서 Broadcast하므로, 여기서는 바로 UObject/델리게이트를
// 다뤄도 안전하다. CR2STATUS_* 값으로 변환해서 캐시 + 같은 모양의 델리게이트를 그대로 재발행한다.
// =====================================================================================

void ACR2SensorManager::HandleEZSensorStatusChanged(EEZSensorStatus Status)
{
    if (Status != EEZSensorStatus::Ready)
    {
        // 카메라/센서 자체가 연결 안 됨 (NoCamera, NoResponse 모두 DISCONNECT로 취급)
        CachedEZStatus = CR2STATUS_DISCONNECT;
        OnSensorStatusChanged.Broadcast(CachedEZStatus);
        UE_LOG(LogTemp, Warning, TEXT("?? [EZ] SensorStatus=%d → DISCONNECT로 매핑"), (int32)Status);
    }
    // Ready인 경우는 아직 "볼이 준비됐다"는 뜻이 아니라 "카메라가 정상 동작 중"이라는 뜻이므로
    // 여기서는 별도 처리 없이 HandleEZBallStatusChanged의 결과를 기다린다.
}

void ACR2SensorManager::HandleEZBallStatusChanged(EEZBallStatus Status, EEZGroundType Ground)
{
    CurrentEZGround = Ground;

    int32 NewStatus = CR2STATUS_DISCONNECT;
    switch (Status)
    {
    case EEZBallStatus::Ready:
        NewStatus = CR2STATUS_READY;
        CachedEZBallPositionEx.bBallExist = true;
        CachedEZBallPositionEx.bShotResult = false;
        CachedEZBallPositionEx.Position = FVector::ZeroVector; // EZSensorSDK는 좌표 미제공
        CachedEZBallPositionEx.BallArea = EZGroundToBallArea(Ground);
        break;

    case EEZBallStatus::Finding:
        NewStatus = CR2STATUS_NOBALL;
        CachedEZBallPositionEx.bBallExist = false;
        CachedEZBallPositionEx.BallArea = EBallArea::BALL_AREA_NONE;
        break;

    case EEZBallStatus::MissingShot:
        NewStatus = CR2STATUS_TRIALSHOT;
        break;
    }

    // 연습샷/무효샷(MissingShot)도 CR2의 TRIALSHOT처럼 watch 세션을 끝내는 이벤트로 보임 -
    // 다음 샷을 잡으려면 여기서도 재무장이 필요하다.
    if (Status == EEZBallStatus::MissingShot && EZSensor.IsValid())
    {
        EZSensor->StartSensing(CurrentEZGround, /*bAllowGroundChange=*/false);
    }

    if (NewStatus != CachedEZStatus)
    {
        CachedEZStatus = NewStatus;
        OnSensorStatusChanged.Broadcast(CachedEZStatus);

        if (NewStatus == CR2STATUS_READY)
        {
            if (IsBallAreaMatchesClub(CachedEZBallPositionEx.BallArea, SelectClub))
            {
                OnBallReady.Broadcast(GetBallPosition_EZ());
            }
            else
            {
                CachedEZStatus = CR2STATUS_NOBALL;
                OnSensorStatusChanged.Broadcast(CR2STATUS_NOBALL);
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("?? [EZ] BallStatus=%d, Ground=%d → CR2STATUS=0x%X"),
        (int32)Status, (int32)Ground, CachedEZStatus);
}

void ACR2SensorManager::HandleEZShotInfoReceived(FEZShotInfo ShotInfo)
{
    UE_LOG(LogTemp, Log, TEXT("?? [EZ] Shot detected! velocity=%.1f m/s"), ShotInfo.Velocity);

    // 좋은 샷 = GOODSHOT으로 상태 전환 알림 (CR2의 HandleSensorCallback과 동일한 흐름)
    CachedEZStatus = CR2STATUS_GOODSHOT;
    OnSensorStatusChanged.Broadcast(CachedEZStatus);

    // ── 기본 FCR2ShotData로 변환 (단위 변환: m/s, degree → X10 정수 포맷) ──
    FCR2ShotData ShotData;
    ShotData.BallSpeedX10 = FMath::RoundToInt(ShotInfo.Velocity * 10.0f);
    ShotData.ClubSpeedBeforeX10 = FMath::RoundToInt(ShotInfo.ClubSpeed * 10.0f);
    ShotData.ClubSpeedAfterX10 = FMath::RoundToInt(ShotInfo.ClubSpeed * 10.0f); // EZSensorSDK는 전/후 구분 없음
    ShotData.ClubPathX10 = FMath::RoundToInt(ShotInfo.ClubPathAngle * 10.0f);
    ShotData.ClubFaceAngleX10 = FMath::RoundToInt(ShotInfo.ClubFaceAngle * 10.0f);
    ShotData.SideSpin = FMath::RoundToInt(ShotInfo.SideSpin);
    ShotData.BackSpin = FMath::RoundToInt(ShotInfo.BackSpin);
    ShotData.AzimuthX10 = FMath::RoundToInt(ShotInfo.AzimuthAngle * 10.0f);
    ShotData.InclineX10 = FMath::RoundToInt(ShotInfo.LaunchAngle * 10.0f);

    OnShotDetected.Broadcast(ShotData);

    // ── 확장(Ex) 데이터로도 캐시 (ShotAssurance/SpinAssurance/SpinAxis는 EZSensorSDK가 안 줘서 근사값) ──
    CachedEZShotDataEx.bValid = true;
    CachedEZShotDataEx.Incline = ShotInfo.LaunchAngle;
    CachedEZShotDataEx.Azimuth = ShotInfo.AzimuthAngle;
    CachedEZShotDataEx.VMag = ShotInfo.Velocity;
    CachedEZShotDataEx.ShotAssurance = 1.0f;  // EZSensorSDK 미제공 - 임시 기본값
    CachedEZShotDataEx.SpinMag = FMath::Sqrt(ShotInfo.BackSpin * ShotInfo.BackSpin + ShotInfo.SideSpin * ShotInfo.SideSpin);
    CachedEZShotDataEx.SpinAxis = FVector::ZeroVector; // EZSensorSDK 미제공
    CachedEZShotDataEx.SpinAssurance = 1.0f;  // EZSensorSDK 미제공 - 임시 기본값

    OnShotDetectedEx.Broadcast(CachedEZShotDataEx);

    // ── 다음 샷을 위해 재무장(rearm) ──
    // CR2가 HandleSensorCallback 끝에서 CR2CMD_OPERATION_RESTART를 무조건 호출하는 것과 동일한 이유:
    // EZSensorSDK도 한 번 샷을 감지하면 그 ground에 대한 센싱이 멈추는 것으로 보이므로,
    // 여기서 다시 StartSensing을 걸어주지 않으면 두 번째 샷부터는 콜백이 더 이상 오지 않는다.
    if (EZSensor.IsValid())
    {
        const bool bRearmed = EZSensor->StartSensing(CurrentEZGround, /*bAllowGroundChange=*/false);
        UE_LOG(LogTemp, Log, TEXT("?? [EZ] 다음 샷을 위해 재무장: Ground=%d, %s"),
            (int32)CurrentEZGround, bRearmed ? TEXT("OK") : TEXT("FAILED"));
    }
}



