// CR2SensorManager.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/Engine.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"
#include "Windows/WindowsHWrapper.h"
#include "CR2SensorManager.generated.h"

// CR2 ���� API �������̽� ����
typedef void* HAND;
typedef signed int CR2_result_t;
typedef signed int I32;
typedef unsigned int U32;
typedef signed char I08;
typedef unsigned char U08;

#if defined(_WIN64)
typedef long long PARAM_T;
#else
typedef signed int PARAM_T;
#endif

// CR2 ���� ���� ����
#define CR2_OK                    0x00000000
#define CR2STATUS_READY           0x00000001
#define CR2STATUS_GOODSHOT        0x00000010
#define CR2STATUS_TRIALSHOT       0x00000011
#define CR2STATUS_DISCONNECT      0x00000100
#define CR2STATUS_BIGSHADOW       0x00000101
#define CR2STATUS_NOBALL          0x00000102

// CR2 ��ɾ� ����
#define		CR2CMD_NULL					0x00000000
#define		CR2CMD_DLLVERSION			0x00000001
#define		CR2CMD_SENSORCONFIG			0x00000002
#define		CR2CMD_CAMSENSORCONFIG		0x00000003
#define		CR2CMD_OPERATION_START		0x00000010				// p0: callbackfuncion.   p1; user param.
#define		CR2CMD_OPERATION_STOP		0x00000011
#define		CR2CMD_OPERATION_RESTART	0x00000012					
#define		CR2CMD_OPERATION_ACTIVATE	0x00000013				// p0, 0: No operation,   1: Normal operation
#define		CR2CMD_OPERATION_START1		0x00000014				// p0: callbackfuncion1.   p1: callbackfunction1_id p1; user param.
#define		CR2CMD_USETEE				0x00000020
#define		CR2CMD_USECLUB				0x00000021
#define		CR2CMD_WIND					0x00000022
#define		CR2CMD_SETTEESTATE			0x00000023
#define		CR2CMD_SETRIGHTLEFT			0x00000024				// p0, 0: Right-handed sensor, 1: Left-handed sensor

#define		CR2CMD_SENSORSTATUS			0x00000030
#define		CR2CMD_SENSORSTATUS2		0x00000031
#define		CR2CMD_AREAALLOW			0x00000032
#define		CR2CMD_BALLPOSITION			0x00000033
#define		CR2CMD_SHOTRESULTEX			0x00000034				// 	p0, 0: Don't clear result, 1: Clear result
#define     CR2CMD_CALC_TRAJECTORY      0x00000040

//----		2020/0331
#define		CR2CMD_RGB0_RGB		        0x000000B0			// Set RGB code. p0: 0x00: off ALL RGB LED. 0x01: CAM1, 0x02: CAM2, 0x03; CAM1 and CAM2. both. 
                                                            //				 p1: (((R << 16) & 0x00FF0000) | ((G << 8) & 0x0000FF00) | (B & 0x000000FF))
#define		CR2CMD_RGB1_RGB		        0x000000B1			// Set RGB code. p0: 0x00: off ALL RGB LED. 0x01: CAM1, 0x02: CAM2, 0x03; CAM1 and CAM2. both. 
#define		CR2CMD_RGB2_RGB		        0x000000B2			// Set RGB code. p0: 0x00: off ALL RGB LED. 0x01: CAM1, 0x02: CAM2, 0x03; CAM1 and CAM2. both. 
                                                            //				 p1: (CR2_rgb2_t) data
#define		CR2CMD_RGB3_RGB		        0x000000B3			// Set RGB tablecode. p0: cam. (0x00, 0x01, 0x02, 0x03)
                                                            //				 p1: table code   (0: rgb0.txt ~ 99: rgb99.txt)
                                                            //				 p2:  iteration count (0: STOP now),    p3: direction (0: normal, 1: reverse)
#define		CR2CMD_RGB4_RGB		        0x000000B4			// Set RGB animation code
                                                            //				 p1: rotation type. (0: STOP, 1: Clockwise, 2: Count-Clockwise)
                                                            //				 p2: animation speed. (0: Fastest ~ 15: Slowest)
                                                            //				 p3: rotation count


// Ŭ�� Ÿ�� ����
#define CR2CLUB_DRIVER            1
#define CR2CLUB_IRON7             17
#define CR2CLUB_PUTTER            30


// ���� ������ ���� ����
UENUM(BlueprintType)
enum class EBallArea : uint8
{
    BALL_AREA_NONE = 0,      // �� ����
    BALL_AREA_TEE = 1,       // Ƽ ���� (Driver)
    BALL_AREA_IRON = 2,      // ���̾� ����
    BALL_AREA_PUTTING = 3    // ���� ����
};

// �� ��ġ ���� (Ȯ�� ���� - ���� ���� ����)
USTRUCT(BlueprintType)
struct FCR2BallPositionEx
{
    GENERATED_BODY()

        UPROPERTY(BlueprintReadOnly, Category = "Ball Position")
        bool bBallExist;

    UPROPERTY(BlueprintReadOnly, Category = "Ball Position")
        bool bShotResult;

    UPROPERTY(BlueprintReadOnly, Category = "Ball Position")
        FVector Position;  // X, Y, Z in mm

    UPROPERTY(BlueprintReadOnly, Category = "Ball Position")
        EBallArea BallArea;  // �� ���� �߰�: ��� ������ �ִ���

    FCR2BallPositionEx()
    {
        bBallExist = false;
        bShotResult = false;
        Position = FVector::ZeroVector;
        BallArea = EBallArea::BALL_AREA_NONE;
    }
};



// CR2 ������ ����ü
USTRUCT(BlueprintType)
struct FCR2ShotData
{
    GENERATED_BODY()

        UPROPERTY(BlueprintReadOnly, Category = "Shot Data")
        int32 BallSpeedX10;

    UPROPERTY(BlueprintReadOnly, Category = "Shot Data")
        int32 ClubSpeedBeforeX10;

    UPROPERTY(BlueprintReadOnly, Category = "Shot Data")
        int32 ClubSpeedAfterX10;

    UPROPERTY(BlueprintReadOnly, Category = "Shot Data")
        int32 ClubPathX10;

    UPROPERTY(BlueprintReadOnly, Category = "Shot Data")
        int32 ClubFaceAngleX10;

    UPROPERTY(BlueprintReadOnly, Category = "Shot Data")
        int32 SideSpin;

    UPROPERTY(BlueprintReadOnly, Category = "Shot Data")
        int32 BackSpin;

    UPROPERTY(BlueprintReadOnly, Category = "Shot Data")
        int32 AzimuthX10;

    UPROPERTY(BlueprintReadOnly, Category = "Shot Data")
        int32 InclineX10;

    FCR2ShotData()
    {
        BallSpeedX10 = 0;
        ClubSpeedBeforeX10 = 0;
        ClubSpeedAfterX10 = 0;
        ClubPathX10 = 0;
        ClubFaceAngleX10 = 0;
        SideSpin = 0;
        BackSpin = 0;
        AzimuthX10 = 0;
        InclineX10 = 0;
    }
};

USTRUCT(BlueprintType)
struct FCR2BallPosition
{
    GENERATED_BODY()

        UPROPERTY(BlueprintReadOnly, Category = "Ball Position")
        bool bBallExist;

    UPROPERTY(BlueprintReadOnly, Category = "Ball Position")
        bool bShotResult;

    UPROPERTY(BlueprintReadOnly, Category = "Ball Position")
        FVector Position; // X, Y, Z in mm

    FCR2BallPosition()
    {
        bBallExist = false;
        bShotResult = false;
        Position = FVector::ZeroVector;
    }
};

USTRUCT(BlueprintType)
struct FCR2ShotDataEx
{
    GENERATED_BODY()

        UPROPERTY(BlueprintReadOnly, Category = "Shot Data Ex")
        bool bValid;

    UPROPERTY(BlueprintReadOnly, Category = "Shot Data Ex")
        float Incline; // degrees

    UPROPERTY(BlueprintReadOnly, Category = "Shot Data Ex")
        float Azimuth; // degrees

    UPROPERTY(BlueprintReadOnly, Category = "Shot Data Ex")
        float VMag; // m/s

    UPROPERTY(BlueprintReadOnly, Category = "Shot Data Ex")
        float ShotAssurance;

    UPROPERTY(BlueprintReadOnly, Category = "Shot Data Ex")
        float SpinMag; // rpm

    UPROPERTY(BlueprintReadOnly, Category = "Shot Data Ex")
        FVector SpinAxis;

    UPROPERTY(BlueprintReadOnly, Category = "Shot Data Ex")
        float SpinAssurance;

    FCR2ShotDataEx()
    {
        bValid = false;
        Incline = 0.0f;
        Azimuth = 0.0f;
        VMag = 0.0f;
        ShotAssurance = 0.0f;
        SpinMag = 0.0f;
        SpinAxis = FVector::ZeroVector;
        SpinAssurance = 0.0f;
    }
};

// ��������Ʈ ����
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShotDetected, const FCR2ShotData&, ShotData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShotDetectedEx, const FCR2ShotDataEx&, ShotDataEx);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBallReady, const FCR2BallPosition&, BallPosition);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSensorStatusChanged, int32, Status);

// CR2 API �Լ� ������ Ÿ�� ����
#if defined(_WIN64)
typedef HAND(*CR2_init_t)(U32 sensorcode, U32 sensornum, PARAM_T p0, PARAM_T p1, PARAM_T p2, PARAM_T p3);
typedef CR2_result_t(*CR2_delete_t)(HAND h);
typedef CR2_result_t(*CR2_command_t)(HAND h, U32 cmd, PARAM_T p0, PARAM_T p1, PARAM_T p2, PARAM_T p3);
#else
typedef HAND(*CR2_init_t)(U32 sensorcode, U32 sensornum, PARAM_T p0, PARAM_T p1, PARAM_T p2, PARAM_T p3);
typedef CR2_result_t(*CR2_delete_t)(HAND h);
typedef CR2_result_t(*CR2_command_t)(HAND h, U32 cmd, PARAM_T p0, PARAM_T p1, PARAM_T p2, PARAM_T p3);
#endif

// �ݹ� �Լ� Ÿ�� ����
#if defined(_WIN64)
typedef int(CALLBACK* CR2_CALLBACKFUNC)(HAND h, U32 status, void* psd, PARAM_T userparam);
#else
typedef int(CALLBACK* CR2_CALLBACKFUNC)(HAND h, U32 status, void* psd, PARAM_T userparam);
#endif

UCLASS(BlueprintType, Blueprintable)
class PARKDAY_API ACR2SensorManager : public AActor
{
    GENERATED_BODY()

public:
    ACR2SensorManager();

protected:
    virtual void BeginPlay() override;
    virtual void BeginDestroy() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void Tick(float DeltaTime) override; 

    // �������Ʈ���� ȣ�� ������ �Լ���
    UFUNCTION(BlueprintCallable, Category = "CR2 Sensor")
        bool InitializeSensor();

    UFUNCTION(BlueprintCallable, Category = "CR2 Sensor")
        void ShutdownSensor();

    UFUNCTION(BlueprintCallable, Category = "CR2 Sensor")
        bool StartSensorOperation();

    UFUNCTION(BlueprintCallable, Category = "CR2 Sensor")
        bool StopSensorOperation();

    UFUNCTION(BlueprintCallable, Category = "CR2 Sensor")
        bool RestartSensorOperation();

    UFUNCTION(BlueprintCallable, Category = "CR2 Sensor")
        bool ConfigureSensor(float LightHeight = 2.67f, int32 VAngleAdd = 2);

    UFUNCTION(BlueprintCallable, Category = "CR2 Sensor")
        bool SetClubType(int32 ClubCode);

    UFUNCTION(BlueprintCallable, Category = "CR2 Sensor")
        int32 GetSensorStatus();

    UFUNCTION(BlueprintCallable, Category = "CR2 Sensor")
        FCR2BallPosition GetBallPosition();

    UFUNCTION(BlueprintCallable, Category = "CR2 Sensor")
        FCR2ShotDataEx GetLastShotDataEx(bool bClearResult = false);

    UFUNCTION(BlueprintCallable, Category = "CR2 Sensor")
        FString GetDLLVersion();

    // ��������Ʈ
    UPROPERTY(BlueprintAssignable, Category = "CR2 Sensor Events")
        FOnShotDetected OnShotDetected;

    UPROPERTY(BlueprintAssignable, Category = "CR2 Sensor Events")
        FOnShotDetectedEx OnShotDetectedEx;

    UPROPERTY(BlueprintAssignable, Category = "CR2 Sensor Events")
        FOnBallReady OnBallReady;

    UPROPERTY(BlueprintAssignable, Category = "CR2 Sensor Events")
        FOnSensorStatusChanged OnSensorStatusChanged;
    // LED ���� ���� �Լ���
    UFUNCTION(BlueprintCallable, Category = "CR2 Sensor")
        void SetLEDColor(int32 Status = -1);

    UFUNCTION(BlueprintCallable, Category = "CR2 Sensor")
        void ResetSensorStatus();

    void CheckBallPresenceInReadyState();

    FCR2BallPositionEx GetBallPositionEx();  // �� ���ο� �Լ�


    static bool IsBallAreaMatchesClub(EBallArea BallArea, int32 ClubCode);


    int32 SelectClub = CR2CLUB_DRIVER;

    UFUNCTION(BlueprintCallable, Category = "Sensor|Ball")
        int32 BallCheck();

    UFUNCTION(BlueprintCallable, Category = "Sensor|Ball")
        FString GetBallCheckResultInfo(int32 BallCheckResult) const;

    // Ball area detection bitmask constants
    static constexpr int32 BALL_AREA_TEE_BIT = 0x0001;      // 0x0001 = Tee �������� �� ����
    static constexpr int32 BALL_AREA_IRON_BIT = 0x0002;     // 0x0002 = Iron �������� �� ����
    static constexpr int32 BALL_AREA_PUTTING_BIT = 0x0004;  // 0x0004 = Putting �������� �� ����

protected:
    // DLL �ε� ����
    bool LoadDLL();
    void UnloadDLL();

    // �ݹ� �Լ�
    static int CALLBACK SensorCallback(HAND h, U32 status, void* psd, PARAM_T userparam);
    void HandleSensorCallback(U32 status, void* psd);

    // ���� ���� üũ
    void CheckSensorStatus();

    void SetEnableLED();
    void SetDisableLED();

private:
    // DLL �ڵ�
    HINSTANCE DLLHandle;

    // API �Լ� ������
    CR2_init_t CR2_init_func;
    CR2_delete_t CR2_delete_func;
    CR2_command_t CR2_command_func;

    // ���� �ڵ�
    HAND SensorHandle;

    // ���� ����
    bool bSensorInitialized;
    bool bSensorStarted;
    int32 LastSensorStatus;

    // DLL ���
    UPROPERTY(EditAnywhere, Category = "CR2 Sensor")
        FString DLLPath;

    // ���� ����
    UPROPERTY(EditAnywhere, Category = "CR2 Sensor")
        float UpdateInterval;

    // Ÿ�̸�
    float StatusCheckTimer;

    // ���� �ν��Ͻ� (�ݹ��)
    static ACR2SensorManager* Instance;


    // LED ���� ��� ����
    static const PARAM_T LED_OFF = 0x00000000;
    static const PARAM_T LED_RED = 0x00FF0000;
    static const PARAM_T LED_GREEN = 0x0000FF00;
    static const PARAM_T LED_BLUE = 0x000000FF;
    static const PARAM_T LED_YELLOW = 0x00FFFF00;
    static const PARAM_T LED_PURPLE = 0x00FF00FF;
    static const PARAM_T LED_CYAN = 0x0000FFFF;
    static const PARAM_T LED_WHITE = 0x00FFFFFF;
    static const PARAM_T LED_ORANGE = 0x00FF8000;

    // ���� �Լ�
    FString GetLEDColorName(PARAM_T Color);

    UPROPERTY(EditAnywhere, Category = "CR2 Sensor")
        float BallReadyRecheckInterval = 0.3f;  // READY ������ �� �� ��üũ �ֱ� (�⺻ 0.3��)

private:
    float BallReadyTimer = 0.0f;  // READY ���¿����� ��üũ Ÿ�̸�

 

    // ���� ������ Ŭ�� ��ȯ

        int32 GetCurrentClubCode() const { return SelectClub; }

    // �� ������ ���� ������ Ŭ���� ��ġ�ϴ��� Ȯ��

        bool IsBallAreaMatchesCurrentClub();

    // ������ Ŭ���� ��ġ�ϴ��� Ȯ���ϴ� ���� �Լ�

        /**
         * @brief Ư�� �������� ���� �����Ǿ����� Ȯ��
         * @param BallCheckResult BallCheck()�� ��ȯ��
         * @param AreaBit Ȯ���� ���� ��Ʈ (BALL_AREA_*_BIT)
         * @return true if �ش� �������� �� ������
         */
        UFUNCTION()
            bool IsBallDetectedInArea(int32 BallCheckResult, int32 AreaBit) const;

   
};
