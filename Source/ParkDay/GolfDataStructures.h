#pragma once
#include "CoreMinimal.h"
#include "ParkDay/LandscapeChecker.h"
#include "GolfDataStructures.generated.h"

// 게임 상태 열거형
UENUM(BlueprintType)
enum class EGameState : uint8
{
    Game_None,      // 게임 초기화
    Game_Init,      // 게임 초기화
    Game_HoleInit,  // 홀 초기화
    Game_HoleReady, // 홀 준비
    Game_HoleStart, // 게임 시작
    Game_Play,      // 게임 진행
    Game_HoleOut,   // 홀 종료
    Game_HoleResults,   // 홀 결과창
    Game_Results,   // 결과 표시
    Game_End,       // 게임 종료
    Game_Exit       // 게임 나가기
};

// 플레이어 상태 열거형
UENUM(BlueprintType)
enum class EPlayerState : uint8
{   
    Player_Des,    // 플레이어 대기
    Player_Init,    // 플레이어 초기화
    Player_Ready,   // 플레이어 준비
    Player_Shot,    // 플레이어 샷
    Player_Results,  // 플레이어 결과
    Player_HoleOut,  // 홀인,컨시드,더블파일때 홀아웃
    
};

// 센서 상태 열거형
UENUM(BlueprintType)
enum class ESensorState : uint8
{
    Sensor_Init,    // 센서 초기화
    Sensor_Reset,   // 센서 리셋
    Sensor_Ready,   // 센서 준비
    Sensor_Shot     // 센서 샷
};

// 볼 상태 열거형
UENUM(BlueprintType)
enum class EBallState : uint8
{
    Ball_Init,      // 볼 초기화
    Ball_Ready,     // 볼 준비
    Ball_Fly,       // 볼 비행
    Ball_Bound,     // 볼 바운드
    Ball_Rolling,   // 볼 구름
    Ball_Stop,      // 볼 정지
    Ball_Des        // 볼 비활성화상태
};


UENUM(BlueprintType)
enum class ECameraMode : uint8
{
    Ready     UMETA(DisplayName = "Ready"),
    Flying    UMETA(DisplayName = "Flying"),
    Following UMETA(DisplayName = "Following"),
    Stop      UMETA(DisplayName = "Stop"),
    Fixed     UMETA(DisplayName = "Fixed"),// ⭐ New fixed camera mode
    Tour      UMETA(DisplayName = "Tour") // ⭐ [추가] 둘러보기 모드
};

UENUM(BlueprintType)
enum class EPracticeMode : uint8
{
    Driving     UMETA(DisplayName = "Driving"),
    Approach     UMETA(DisplayName = "Approach"),
    Putting     UMETA(DisplayName = "Putting"),
};

USTRUCT(BlueprintType)
struct FRoundStat
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Rank = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ShotCount = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AverageDistanceOfDriver = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxDistance = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FairwayArccuracy = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float GreenArccuracy = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 GreenPuttCount = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PuttCount = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SandSave = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ParSave = 0.f;
};

// 플레이어 정보 구조체
USTRUCT(BlueprintType)
struct FPlayerInfo
{
    GENERATED_BODY()

        // 로그인 아이디
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerInfo")
        FString ID ="Player_";

    // 닉네임
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerInfo")
        FString NickName = "Guest_";

    // 슬롯 인덱스
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerInfo")
        int32 SlotIndex = 0;

    // 게스트?
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerInfo")
        bool IsGuest = 0;

    // 플레이어 레벨
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerInfo")
        int32 Level = 0;

    // 플레이어 랭킹
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerInfo")
        int32 Ranking = 0;

    // 플레이어 포인트
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerInfo")
        int32 Point = 0;

    // 티 높이
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerInfo")
        int32 Tee_Height = 0;

    // 핸디캡
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerInfo")
        int32 HandiCap = 0;

    // 라운드 횟수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerInfo")
        int32 RoundCount = 0;

    // 평균 비거리
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerInfo")
        float Avg_Distance = 0.0f;

    // 마지막 접속 날짜 (Unix 타임스탬프)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerInfo")
        int32 Last_Date = 0;

    // 플레이어 아이콘 이미지 URL
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerInfo")
        FString Img_Url;

    // 현재 라운드의 홀별 타수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerInfo")
        TArray<int32> HoleScores;

    // 현재 라운드의 홀별 멀리건 사용 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerInfo")
        TArray<bool> HoleMulligans;

    // 플레이어의 공 색상
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerInfo")
        FLinearColor BallColor;

    // 현재 볼 위치 X
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptionInfo")
        float BallPosX = 0.0f;

    // 현재 볼 위치 Y
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptionInfo")
        float BallPosY = 0.0f;

    // 현재 볼 위치 Z
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptionInfo")
        float BallPosZ = 0.0f;

    // 이전 볼 위치 X
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptionInfo")
        float BeforePosX = 0.0f;

    // 이전 볼 위치 Y
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptionInfo")
        float BeforePosY = 0.0f;

    // 이전 볼 위치 Z
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptionInfo")
        float BeforePosZ = 0.0f;

    // 홀별 샷 횟수
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        int32 ShotCount = 0;

    // 홀별 샷 횟수
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        bool OnceHoleMulligan = 0;

    // 총 스코어
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        int32 TotalScore = 0;

    // 현재 홀 번호
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerInfo")
        int32 HoleCount = 0;

    // 멀리건 사용 횟수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerInfo")
        int32 MulliganCount = 0;

    // 홀별 샷 횟수 배열
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerInfo")
        TArray<int32> ShotCountPerHole;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerInfo")
		bool bIsHoleout = false;    
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerInfo")
		bool bIsPendingDelete = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerInfo")
    FRoundStat RoundStat;

    // 볼 선택 인덱스 (0=흰색, 1=노랑, 2=빨강, 3=파랑, 4=주황, 5=검정)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerInfo")
    int32 BallIndex = 0;


    FPlayerInfo()
    {
        ID = "Player_" + FGuid::NewGuid().ToString().Left(8);
        NickName = "Guest_" + ID;
        SlotIndex = 1;
        IsGuest = false;
        Level = 1;
        Ranking = 0;
        Point = 0;
        Tee_Height = 20;
        HandiCap = 0;
        RoundCount = 0;
        Avg_Distance = 0.0f;
        Last_Date = 0;
        Img_Url = "";
        BallPosX = 0.0f;
        BallPosY = 0.0f;
        BallPosZ = 0.0f;
        BeforePosX = 0.0f;
        BeforePosY = 0.0f;
        BeforePosZ = 0.0f;
        TotalScore = 0;
        BallColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f); // White
        HoleScores.Empty();
        for (int32 i = 0; i < 18; i++)
        {
            HoleMulligans.Add(false);
        }
        ShotCount = 0;
        HoleCount = 1;
        OnceHoleMulligan = false;
        MulliganCount = 0;
        ShotCountPerHole.Init(0, 18);
        RoundStat = FRoundStat();
        bIsPendingDelete = false;
        BallIndex = 0;
    }

    void SoftReset()
    {
        TotalScore = 0;
        ShotCount = 0;
        HoleCount = 1;
        HoleScores.Empty();
        ShotCountPerHole.Empty();
        bIsHoleout = false;
        OnceHoleMulligan = false;
        bIsPendingDelete = false;

        for (int32 i = 0; i < 18; i++)
        {
            ShotCountPerHole.Add(0);
        }

        for (int32 i = 0; i < 18; i++)
        {
            HoleMulligans.Add(false);
        }

        BallColor = FColor::White;
        MulliganCount = 0;
    }
};





// 게임 옵션 정보 구조체
USTRUCT(BlueprintType)
struct FGameOptionInfo
{
    GENERATED_BODY()

    //코스선택
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptionInfo")
    int32 SelectCourse = 0;

    // 핀 위치
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptionInfo")
        int32 Holecup_Position = 0;

    // 멀리건 횟수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptionInfo")
        int32 Mulligan_Count = 0;

    // 컨시드 거리 (미터)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptionInfo")
        float Concede_Distance = 0.0f;

    // 그린 속도
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptionInfo")
        float Green_Speed = 0.0f;

    // 연습 볼 개수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptionInfo")
        int32 PracticeBall = 0;

    // 동영상 저장 횟수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptionInfo")
        int32 Movie_SaveCount = 0;

    //퍼팅 이어하기
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptionInfo")
        int32 ContinuePutting = 0;

    // 카메라 모드 (0: 기본, 1: 시네마틱 등)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptionInfo")
        int32 Camera_Mode = 0;

    // 게임타입 (0: 스트로크, 1: 연습장)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptionInfo")
        int32 GameType = 0;

    //스윙모션
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptionInfo")
        int32 SwingMotion = 0;

    // ✅ 추가: 연습장(Range) 전용 스윙모션
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptionInfo")
        int32 RangeSwingMotion = 0; // 0: OFF, 1: ON
        // ✅ 추가: 연습 모드 (0: Driving, 1: Approach, 2: Putting)
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptionInfo")
        int32 PracticeMode = 0;

    FGameOptionInfo()
    {
        SelectCourse = 2;
        ContinuePutting = 0;
        Holecup_Position = 0;
        Mulligan_Count = 3;
        Concede_Distance = 1.0f;
        Green_Speed = 1.0f;
        PracticeBall = 0;
        Movie_SaveCount = 0;
        Camera_Mode = 0;
        GameType = 0;
        SwingMotion = 0;
        RangeSwingMotion = 0;  // ✅ 추가
        PracticeMode = 0;  // ✅ 추가
    }
};

// OB 라인 구조체
USTRUCT(BlueprintType)
struct FOBLine
{
    GENERATED_BODY()
        UPROPERTY(EditAnywhere, BlueprintReadWrite)
        TArray<FVector> Points;
};


USTRUCT(BlueprintType)
struct FGreenHolePositions
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Center = FVector::ZeroVector; // 0: 중앙
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Right = FVector::ZeroVector; // 1: 오른쪽
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Left = FVector::ZeroVector; // 2: 왼쪽
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Front = FVector::ZeroVector; // 3: 앞
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Back = FVector::ZeroVector; // 4: 뒤
};
// 맵 정보 구조체
USTRUCT(BlueprintType)
struct FMapInfo
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString MapName = "SancheoneoPark";
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString PakName = "SancheoneoPark";
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString CCName = "SancheoneoPark";
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Sublevel = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString MapDescription = "Default golf course description";;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString MapThumbnail = "";
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 HoleCount = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<int32> ParScores;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<float> HoleLengths;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FVector> TeePositions;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FVector> HolecupPositions;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FOBLine> OBLines; // 중첩 제거

    // ⭐ 추가
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FGreenHolePositions> GreenHolePositions; // 홀별 그린 5개 위치


    FMapInfo()
    {
        MapName = "SancheoneoPark";
        PakName = "SancheoneoPark";
        CCName = "SancheoneoPark";
        MapDescription = "Default golf course description";
        MapThumbnail = "";
        Sublevel = 0;
        HoleCount = 18;
        ParScores.Init(4, 18);
        HoleLengths.Init(400.0f, 18);
        TeePositions.Init(FVector(100.0f, 100.0f, 0.0f), 18); // Default tee positions
        HolecupPositions.Init(FVector(500.0f, 500.0f, 0.0f), 18); // Default holecup positions
        OBLines.SetNum(18);
    }
};

// 게임 정보 구조체 (전체 설정)
USTRUCT(BlueprintType)
struct FGameInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FPlayerInfo> Players;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMapInfo SelectedMap;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameOptionInfo GameOptions;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 CurrentHole;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 CurrentPlayerIndex;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FDateTime GameStartTime;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 LatestUseMulliganPlayerIndex;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 LatestShotPlayerSlotIndex;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIsRoundEnd = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEventHole = false;

    FGameInfo()
    {
        Players.SetNum(1); // Default: 2 players
        for (FPlayerInfo& Player : Players)
        {
            Player = FPlayerInfo();
        }
        SelectedMap = FMapInfo();
        GameOptions = FGameOptionInfo();
        CurrentHole = 1;
        CurrentPlayerIndex = 0;
        GameStartTime = FDateTime::Now();
        LatestUseMulliganPlayerIndex = -1;
        LatestShotPlayerSlotIndex = -1;
        bIsRoundEnd = true; // ⭐ 추가
    }

    void SoftReset()
    {
        for (FPlayerInfo& Player : Players)
        {
            Player.SoftReset();
        }
        CurrentHole = 1;
        CurrentPlayerIndex = 0;
        LatestUseMulliganPlayerIndex = -1;
        LatestShotPlayerSlotIndex = -1;
        bIsRoundEnd = true; // ⭐ 추가
    }

    void Reset()
    {
        FGameInfo();
        Players.Empty();
        CurrentHole = 1;
        CurrentPlayerIndex = 0;
        LatestUseMulliganPlayerIndex = -1;
        LatestShotPlayerSlotIndex = -1;
        bIsRoundEnd = true; // ⭐ 추가
    }
};

USTRUCT(BlueprintType)
struct FSystemConfig
{
    GENERATED_BODY()

        UPROPERTY(EditAnywhere, BlueprintReadWrite)
        int32 ComPort = 9;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        int32 BaudRate = 9600;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        bool bAutoTeeEnabled = true;

    // ⭐ 키 반복 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        float KeyRepeatInterval = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        float KeyRepeatDelay = 0.15f;

    bool bEnableVideoSaving = true;

    FSystemConfig()
        : ComPort(9)
        , BaudRate(9600)
        , bAutoTeeEnabled(true)
        , KeyRepeatInterval(0.15f)
        , KeyRepeatDelay(0.15f)
        , bEnableVideoSaving(true)
    {
    }
};

// 게임 옵션 정보 구조체 (DefaultGameInfo 는 한번 더 씌워져있어서 이걸 사용해야함)
USTRUCT(BlueprintType)
struct FDefaultGameOption
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DefaultGameOption")
    FGameOptionInfo GameOptions;

    FDefaultGameOption()
    {
        GameOptions = FGameOptionInfo();
    }
};

USTRUCT()
struct FShotPath
{
    GENERATED_BODY()

    FVector StartBall = FVector::ZeroVector;   // 샷 시작 위치(월드)
    FVector Holecup   = FVector::ZeroVector;   // 해당 홀컵 위치(월드)
    TArray<FVector> Samples;                   // 0.5초(기본) 간격 샘플(월드)
};

USTRUCT()
struct FShotStat
{
    GENERATED_BODY()

    int32 ShotCount = 0;
    float Distance = 0;
    float BallSpeed = 0;
    float LaunchAngle = 0;
    float DirectionAngle = 0;
    float RemainDistance = 0;
};


USTRUCT(BlueprintType)
struct FShotInfo
{
    GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector ShotLocation = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector StopLocation = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ELandType ShotLocationLandType = ELandType::Rough;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ELandType StopLocationLandType = ELandType::Rough;
};

USTRUCT(BlueprintType)
struct FHardwareStatus
{
	GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool MotionCam = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool AutoTee = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool Sensor = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool Projector = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool Kiosk = false;
};

USTRUCT(BlueprintType)
struct FAdminConfig
{
	GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString AdminPassword = TEXT("");
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PracticeTimeMinutes = 5;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString DeviceId = TEXT("");
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString RoomNumber = TEXT("");
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool SwingMotionEnabled = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 GazeControl = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString BallColor = TEXT("Brown");
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FHardwareStatus HardwareStatus;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool UsePassword = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool StrokePW = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool TrainingPW = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool RangePW = false;
};