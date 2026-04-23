// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/UserDefinedStruct.h"
#include "AuthStructures.h"
#include "GolfDataStructures.h"
#include "GameDataStructures.generated.h"

/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * 게임 네트워크 데이터 구조
 * 명세서 기반: 맵/게임옵션/게임데이터/게임결과/랭킹/친구
 * ═══════════════════════════════════════════════════════════════════════════════
 */

 // ─────────────────────────────────────────────────────────────────────────────
 // [2. 맵 정보]
 // ─────────────────────────────────────────────────────────────────────────────

 /**
  * @brief 맵 목록 조회 요청 패킷
  */
USTRUCT(BlueprintType)
struct FMapListRequest
{
    GENERATED_BODY()

        /// 맵 검색 키워드 (선택)
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map")
        FString SearchKeyword;

    /// 페이지 번호 (페이징)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map")
        int32 Page = 1;
};

/**
 * @brief 맵 목록 응답 패킷
 */
USTRUCT(BlueprintType)
struct FMapListResponse
{
    GENERATED_BODY()

        /// 맵 정보 배열 (GolfDataStructures.h 의 FMapInfo 재사용)
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map")
        TArray<FMapInfo> Maps;
};

/**
 * @brief 맵 로딩 요청 패킷
 */
USTRUCT(BlueprintType)
struct FMapLoadRequest
{
    GENERATED_BODY()

        /// 선택할 맵 이름
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map")
        FString MapName;
};

// ─────────────────────────────────────────────────────────────────────────────
// [3. 게임 옵션]
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief 게임 옵션 구조체 (서버 통신용)
 *
 * GolfDataStructures.h 의 FGameOptionInfo 와 매핑됨
 * 필드명은 명세서 기준으로 통일
 *
 * 사용 예시:
 * {
 *   "SelectCourse": 0,
 *   "ContinuePutting": 0,
 *   "HolecupPosition": 1,
 *   "MulliganCount": 3,
 *   "ConcedeDistance": 1,
 *   "GreenSpeed": 1,
 *   "PracticeBall": 0,
 *   "MovieSaveCount": 0,
 *   "CameraMode": 0,
 *   "GameType": 0,
 *   "SwingMotion": 0
 * }
 */
USTRUCT(BlueprintType)
struct FGameOptions
{
    GENERATED_BODY()

        /// 코스 선택 (0: 기본)
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptions")
        int32 SelectCourse = 0;

    /// 연속 퍼팅 여부 (0: 비활성, 1: 활성)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptions")
        int32 ContinuePutting = 0;

    /// 홀컵 위치 옵션
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptions")
        int32 HolecupPosition = 0;

    /// 멀리건 수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptions")
        int32 MulliganCount = 3;

    /// 컨시드 거리
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptions")
        int32 ConcedeDistance = 1;

    /// 그린 속도
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptions")
        int32 GreenSpeed = 1;

    /// 연습볼 여부 (0: 미사용)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptions")
        int32 PracticeBall = 0;

    /// 무비 저장 수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptions")
        int32 MovieSaveCount = 0;

    /// 카메라 모드
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptions")
        int32 CameraMode = 0;

    /// 게임 타입 (0: 스트로크)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptions")
        int32 GameType = 0;

    /// 스윙 모션 타입
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptions")
        int32 SwingMotion = 0;

    /** FGameOptionInfo 로 변환 (로컬 게임 데이터와 연동) */
    FGameOptionInfo ToGameOptionInfo() const
    {
        FGameOptionInfo Result;
        Result.SelectCourse = SelectCourse;
        Result.ContinuePutting = ContinuePutting;
        Result.Holecup_Position = HolecupPosition;
        Result.Mulligan_Count = MulliganCount;
        Result.Concede_Distance = static_cast<float>(ConcedeDistance);
        Result.Green_Speed = static_cast<float>(GreenSpeed);
        Result.PracticeBall = PracticeBall;
        Result.Movie_SaveCount = MovieSaveCount;
        Result.Camera_Mode = CameraMode;
        Result.GameType = GameType;
        Result.SwingMotion = SwingMotion;
        return Result;
    }

    /** FGameOptionInfo 에서 변환 */
    static FGameOptions FromGameOptionInfo(const FGameOptionInfo& Source)
    {
        FGameOptions Result;
        Result.SelectCourse = Source.SelectCourse;
        Result.ContinuePutting = Source.ContinuePutting;
        Result.HolecupPosition = Source.Holecup_Position;
        Result.MulliganCount = Source.Mulligan_Count;
        Result.ConcedeDistance = static_cast<int32>(Source.Concede_Distance);
        Result.GreenSpeed = static_cast<int32>(Source.Green_Speed);
        Result.PracticeBall = Source.PracticeBall;
        Result.MovieSaveCount = Source.Movie_SaveCount;
        Result.CameraMode = Source.Camera_Mode;
        Result.GameType = Source.GameType;
        Result.SwingMotion = Source.SwingMotion;
        return Result;
    }
};

/**
 * @brief 게임 옵션 저장 요청 패킷
 */
USTRUCT(BlueprintType)
struct FSaveOptionsRequest
{
    GENERATED_BODY()

        /// 인증 토큰
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptions")
        FString Token;

    /// 옵션 데이터
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOptions")
        FGameOptions Options;
};

// ─────────────────────────────────────────────────────────────────────────────
// [4. 게임 데이터]
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief 게임 데이터 구조 (서버 동기화용)
 *
 * GolfDataStructures.h 의 FGameInfo 와 매핑됨
 *
 * 사용 예시:
 * {
 *   "Players": [...],
 *   "SelectedMap": {...},
 *   "GameOptions": {...},
 *   "CurrentHole": 1,
 *   "CurrentPlayerIndex": 0,
 *   "GameStartTime": "2025-03-01T10:00:00Z"
 * }
 */
USTRUCT(BlueprintType)
struct FGameData
{
    GENERATED_BODY()

        /// 플레이어 배열 (GolfDataStructures.h 의 FPlayerInfo 재사용)
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameData")
        TArray<FPlayerInfo> Players;

    /// 선택된 맵
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameData")
        FMapInfo SelectedMap;

    /// 게임 옵션
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameData")
        FGameOptions GameOptions;

    /// 현재 홀 번호
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameData")
        int32 CurrentHole = 1;

    /// 현재 턴 플레이어 인덱스
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameData")
        int32 CurrentPlayerIndex = 0;

    /// 최근 멀리건 사용 플레이어 인덱스
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameData")
        int32 LatestUseMulliganPlayerIndex = -1;

    /// 게임 시작 시간 (ISO 8601)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameData")
        FString GameStartTime;

    /** FGameInfo 로 변환 */
    FGameInfo ToGameInfo() const
    {
        FGameInfo Result;
        Result.Players = Players;
        Result.SelectedMap = SelectedMap;
        Result.GameOptions = GameOptions.ToGameOptionInfo();
        Result.CurrentHole = CurrentHole;
        Result.CurrentPlayerIndex = CurrentPlayerIndex;
        Result.LatestUseMulliganPlayerIndex = LatestUseMulliganPlayerIndex;
        return Result;
    }

    /** FGameInfo 에서 변환 */
    static FGameData FromGameInfo(const FGameInfo& Source)
    {
        FGameData Result;
        Result.Players = Source.Players;
        Result.SelectedMap = Source.SelectedMap;
        Result.GameOptions = FGameOptions::FromGameOptionInfo(Source.GameOptions);
        Result.CurrentHole = Source.CurrentHole;
        Result.CurrentPlayerIndex = Source.CurrentPlayerIndex;
        Result.LatestUseMulliganPlayerIndex = Source.LatestUseMulliganPlayerIndex;
        Result.GameStartTime = Source.GameStartTime.ToString();
        return Result;
    }
};

/**
 * @brief 게임 데이터 업데이트 요청 패킷
 */
USTRUCT(BlueprintType)
struct FUpdateGameDataRequest
{
    GENERATED_BODY()

        /// 게임 세션 ID
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameData")
        FString GameID;

    /// 업데이트할 데이터
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameData")
        FGameData Data;
};

// ─────────────────────────────────────────────────────────────────────────────
// [5. 게임 결과]
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief 플레이어 결과 구조체
 */
USTRUCT(BlueprintType)
struct FPlayerResult
{
    GENERATED_BODY()

        /// 플레이어 ID
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameResult")
        FString PlayerID;

    /// 총 스코어
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameResult")
        int32 TotalScore = 0;

    /// 게임 내 랭킹
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameResult")
        int32 RankingInGame = 0;

    /// 평균 거리
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameResult")
        int32 AvgDistance = 0;

    /// 홀별 스코어
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameResult")
        TArray<int32> HoleScores;
};

/**
 * @brief 게임 결과 구조체
 *
 * 사용 예시:
 * {
 *   "GameID": "game_uuid_1234",
 *   "PlayerResults": [...],
 *   "EndTime": "2025-03-01T12:30:00Z"
 * }
 */
USTRUCT(BlueprintType)
struct FGameResult
{
    GENERATED_BODY()

        /// 게임 ID
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameResult")
        FString GameID;

    /// 플레이어 결과 배열
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameResult")
        TArray<FPlayerResult> PlayerResults;

    /// 게임 종료 시간 (ISO 8601)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameResult")
        FString EndTime;
};

/**
 * @brief 게임 결과 저장 요청 패킷
 */
USTRUCT(BlueprintType)
struct FSaveGameResultRequest
{
    GENERATED_BODY()

        /// 인증 토큰
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameResult")
        FString Token;

    /// 결과 데이터
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameResult")
        FGameResult Result;
};

// ─────────────────────────────────────────────────────────────────────────────
// [6. 랭킹]
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief 랭킹 조회 요청 패킷
 */
USTRUCT(BlueprintType)
struct FRankingRequest
{
    GENERATED_BODY()

        /// 랭킹 타입 (0: 전체, 1: 친구)
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranking")
        int32 Type = 0;

    /// 페이지 번호
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranking")
        int32 Page = 1;
};

/**
 * @brief 랭킹 항목 구조체
 */
USTRUCT(BlueprintType)
struct FRankingEntry
{
    GENERATED_BODY()

        /// 닉네임
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranking")
        FString NickName;

    /// 랭킹 타입
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranking")
        int32 Type = 0;

    /// 랭킹 순위
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranking")
        int32 Rank = 0;

    /// 스코어/포인트
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranking")
        int32 Score = 0;

    /// 레벨
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranking")
        int32 Level = 0;
};

/**
 * @brief 랭킹 응답 패킷
 */
USTRUCT(BlueprintType)
struct FRankingResponse
{
    GENERATED_BODY()

        /// 랭킹 항목 배열
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranking")
        TArray<FRankingEntry> Entries;
};

// ─────────────────────────────────────────────────────────────────────────────
// [7. 친구]
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief 친구 정보 구조체
 */
USTRUCT(BlueprintType)
struct FFriendInfo
{
    GENERATED_BODY()

        /// 친구 ID
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend")
        FString FriendID;

    /// 닉네임
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend")
        FString NickName;

    /// 온라인 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend")
        bool bOnline = false;

    /// 레벨
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend")
        int32 Level = 0;
};

/**
 * @brief 친구 목록 응답 패킷
 */
USTRUCT(BlueprintType)
struct FFriendListResponse
{
    GENERATED_BODY()

        /// 친구 배열
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend")
        TArray<FFriendInfo> Friends;
};

/**
 * @brief 친구 추가 요청 패킷
 */
USTRUCT(BlueprintType)
struct FAddFriendRequest
{
    GENERATED_BODY()

        /// 인증 토큰
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend")
        FString Token;

    /// 추가할 친구 ID
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend")
        FString FriendID;
};

/**
 * @brief 친구 초대 요청 패킷 (게임 초대)
 */
USTRUCT(BlueprintType)
struct FInviteFriendRequest
{
    GENERATED_BODY()

        /// 인증 토큰
        UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend")
        FString Token;

    /// 초대할 친구 ID
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend")
        FString FriendID;

    /// 초대할 게임 ID
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend")
        FString GameID;
};

// ─────────────────────────────────────────────────────────────────────────────
// [콜백 정의]
// ─────────────────────────────────────────────────────────────────────────────

/// 맵 목록 응답 콜백
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnMapListResponseReceived, bool, bSuccess, const FMapListResponse&, Response);

/// 단일 맵 정보 응답 콜백
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnMapInfoResponseReceived, bool, bSuccess, const FMapInfo&, MapInfo);

/// 게임 옵션 응답 콜백
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnGameOptionsResponseReceived, bool, bSuccess, const FGameOptions&, Options);

/// 게임 데이터 응답 콜백
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnGameDataResponseReceived, bool, bSuccess, const FGameData&, GameData);

/// 게임 결과 응답 콜백 (저장/조회 공용)
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnGameResultResponseReceived, bool, bSuccess, const FGameResult&, Result);

/// 랭킹 응답 콜백
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnRankingResponseReceived, bool, bSuccess, const FRankingResponse&, Response);

/// 친구 목록 응답 콜백
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnFriendListResponseReceived, bool, bSuccess, const FFriendListResponse&, FriendList);

/// 단순 성공/실패 콜백 (친구추가, 친구초대, 옵션저장 등)
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnSimpleResponse, bool, bSuccess);
