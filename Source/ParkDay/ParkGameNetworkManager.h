// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Http.h"
#include "NetworkManager.h"
#include "GameDataStructures.h"
#include "ParkGameNetworkManager.generated.h"

/**
 * ═══════════════════════════════════════════════════════════════════════════════
 * 게임 네트워크 매니저
 *
 * NetworkManager 를 래핑하여 게임 관련 모든 HTTP 통신을 처리
 * 명세서 기준:
 *   2. 맵 정보 조회 / 로딩
 *   3. 게임 옵션 저장/불러오기
 *   4. 게임 데이터 동기화
 *   5. 게임 결과 저장/조회
 *   6. 랭킹 조회
 *   7. 친구 관리
 *
 * [레벨 배치 방법]
 *   - 레벨에 ANetworkManager 먼저 배치 후 이 액터도 배치
 *   - BeginPlay 에서 자동으로 NetworkManager 를 탐색함
 *   - 또는 SetNetworkManager() 로 수동 지정 가능
 * ═══════════════════════════════════════════════════════════════════════════════
 */
UCLASS()
class PARKDAY_API AParkGameNetworkManager : public AActor
{
    GENERATED_BODY()

public:
    AParkGameNetworkManager();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // ─────────────────────────────────────────────────────────────────────────────
    // [네트워크 매니저 참조]
    // ─────────────────────────────────────────────────────────────────────────────

    /**
     * @brief NetworkManager 수동 설정
     */
    UFUNCTION(BlueprintCallable, Category = "Network|Game")
        void SetNetworkManager(ANetworkManager* InNetworkManager);

    // ─────────────────────────────────────────────────────────────────────────────
    // [2. 맵 관련 API]
    // ─────────────────────────────────────────────────────────────────────────────

    /**
     * @brief 맵 목록 조회
     *
     * @param SearchKeyword 검색 키워드 (빈 문자열이면 전체 조회)
     * @param Page 페이지 번호 (1부터 시작)
     * @param OnResponse 응답 콜백 (bool bSuccess, FMapListResponse)
     */
    UFUNCTION(BlueprintCallable, Category = "Network|Game|Map")
        void GetMapList(const FString& SearchKeyword, int32 Page, FOnMapListResponseReceived OnResponse);

    /**
     * @brief 특정 맵 상세 정보 불러오기
     *
     * @param MapName 맵 이름
     * @param OnResponse 응답 콜백 (bool bSuccess, FMapInfo)
     */
    UFUNCTION(BlueprintCallable, Category = "Network|Game|Map")
        void LoadMap(const FString& MapName, FOnMapInfoResponseReceived OnResponse);

    // ─────────────────────────────────────────────────────────────────────────────
    // [3. 게임 옵션 API]
    // ─────────────────────────────────────────────────────────────────────────────

    /**
     * @brief 게임 옵션 불러오기 (서버에서 저장된 옵션)
     *
     * @param Token 인증 토큰
     * @param OnResponse 응답 콜백 (bool bSuccess, FGameOptions)
     */
    UFUNCTION(BlueprintCallable, Category = "Network|Game|Options")
        void GetGameOptions(const FString& Token, FOnGameOptionsResponseReceived OnResponse);

    /**
     * @brief 게임 옵션 저장
     *
     * @param Token 인증 토큰
     * @param Options 저장할 옵션
     * @param OnResponse 응답 콜백 (bool bSuccess)
     */
    UFUNCTION(BlueprintCallable, Category = "Network|Game|Options")
        void SaveGameOptions(const FString& Token, const FGameOptions& Options, FOnSimpleResponse OnResponse);

    // ─────────────────────────────────────────────────────────────────────────────
    // [4. 게임 데이터 API]
    // ─────────────────────────────────────────────────────────────────────────────

    /**
     * @brief 게임 데이터 불러오기 (세션 복원용)
     *
     * @param GameID 게임 세션 ID
     * @param OnResponse 응답 콜백 (bool bSuccess, FGameData)
     */
    UFUNCTION(BlueprintCallable, Category = "Network|Game|GameData")
        void GetGameData(const FString& GameID, FOnGameDataResponseReceived OnResponse);

    /**
     * @brief 게임 데이터 업데이트 (실시간 동기화)
     * 스트로크 진행 중 주기적으로 호출
     *
     * @param GameID 게임 세션 ID
     * @param GameData 업데이트할 데이터 (FGameData::FromGameInfo() 로 변환 권장)
     * @param OnResponse 응답 콜백 (bool bSuccess)
     */
    UFUNCTION(BlueprintCallable, Category = "Network|Game|GameData")
        void UpdateGameData(const FString& GameID, const FGameData& GameData, FOnSimpleResponse OnResponse);

    // ─────────────────────────────────────────────────────────────────────────────
    // [5. 게임 결과 API]
    // ─────────────────────────────────────────────────────────────────────────────

    /**
     * @brief 게임 결과 저장 (게임 종료 시 호출)
     *
     * @param Token 인증 토큰
     * @param Result 게임 결과 데이터
     * @param OnResponse 응답 콜백 (bool bSuccess, FGameResult)
     */
    UFUNCTION(BlueprintCallable, Category = "Network|Game|Result")
        void SaveGameResult(const FString& Token, const FGameResult& Result, FOnGameResultResponseReceived OnResponse);

    /**
     * @brief 과거 게임 결과 목록 조회
     *
     * @param Token 인증 토큰
     * @param Page 페이지 번호
     * @param OnResponse 응답 콜백 (bool bSuccess, FGameResult)
     */
    UFUNCTION(BlueprintCallable, Category = "Network|Game|Result")
        void GetGameResults(const FString& Token, int32 Page, FOnGameResultResponseReceived OnResponse);

    // ─────────────────────────────────────────────────────────────────────────────
    // [6. 랭킹 API]
    // ─────────────────────────────────────────────────────────────────────────────

    /**
     * @brief 랭킹 조회
     *
     * @param Type 0: 전체 랭킹, 1: 친구 랭킹
     * @param Page 페이지 번호
     * @param OnResponse 응답 콜백 (bool bSuccess, FRankingResponse)
     */
    UFUNCTION(BlueprintCallable, Category = "Network|Game|Ranking")
        void GetRankings(int32 Type, int32 Page, FOnRankingResponseReceived OnResponse);

    // ─────────────────────────────────────────────────────────────────────────────
    // [7. 친구 API]
    // ─────────────────────────────────────────────────────────────────────────────

    /**
     * @brief 친구 목록 조회
     *
     * @param Token 인증 토큰
     * @param OnResponse 응답 콜백 (bool bSuccess, FFriendListResponse)
     */
    UFUNCTION(BlueprintCallable, Category = "Network|Game|Friend")
        void GetFriendList(const FString& Token, FOnFriendListResponseReceived OnResponse);

    /**
     * @brief 친구 추가
     *
     * @param Token 인증 토큰
     * @param FriendID 추가할 친구 ID
     * @param OnResponse 응답 콜백 (bool bSuccess)
     */
    UFUNCTION(BlueprintCallable, Category = "Network|Game|Friend")
        void AddFriend(const FString& Token, const FString& FriendID, FOnSimpleResponse OnResponse);

    /**
     * @brief 친구 게임 초대
     *
     * @param Token 인증 토큰
     * @param FriendID 초대할 친구 ID
     * @param GameID 초대할 게임 ID
     * @param OnResponse 응답 콜백 (bool bSuccess)
     */
    UFUNCTION(BlueprintCallable, Category = "Network|Game|Friend")
        void InviteFriend(const FString& Token, const FString& FriendID, const FString& GameID, FOnSimpleResponse OnResponse);

    // ─────────────────────────────────────────────────────────────────────────────
    // [이벤트 델리게이트]
    // ─────────────────────────────────────────────────────────────────────────────

    /// 네트워크 요청 시작
    UPROPERTY(BlueprintAssignable, Category = "Network|Game|Events")
        FOnNetworkRequestStarted OnRequestStarted;

    /// 네트워크 요청 실패
    UPROPERTY(BlueprintAssignable, Category = "Network|Game|Events")
        FOnNetworkRequestFailed OnRequestFailed;

protected:
    // ─────────────────────────────────────────────────────────────────────────────
    // [내부 변수]
    // ─────────────────────────────────────────────────────────────────────────────

    /// 참조하는 NetworkManager
    UPROPERTY(VisibleAnywhere, Category = "Network|Game")
        ANetworkManager* NetworkManager;

    /// 현재 처리 중인 요청 타입
    FString CurrentRequestType;

    // ─────────────────────────────────────────────────────────────────────────────
    // [콜백 저장소 - 요청당 하나만 유지]
    // ─────────────────────────────────────────────────────────────────────────────

    FOnMapListResponseReceived      MapListCallback;
    FOnMapInfoResponseReceived      MapInfoCallback;
    FOnGameOptionsResponseReceived  GameOptionsCallback;
    FOnGameDataResponseReceived     GameDataCallback;
    FOnGameResultResponseReceived   GameResultCallback;
    FOnRankingResponseReceived      RankingCallback;
    FOnFriendListResponseReceived   FriendListCallback;
    FOnSimpleResponse               SimpleResponseCallback;

    // ─────────────────────────────────────────────────────────────────────────────
    // [내부 함수 - HTTP]
    // ─────────────────────────────────────────────────────────────────────────────

    /**
     * @brief 게임 API 용 HTTP POST 요청 전송
     *
     * @param Endpoint API 엔드포인트 (예: "/api/map/list")
     * @param JsonPayload JSON 페이로드 (빈 문자열 가능)
     * @param bRequiresAuth 인증 토큰 필요 여부
     * @param Token 명시적 토큰 (비어있으면 NetworkManager 토큰 사용)
     */
    void SendGameHttpRequest(const FString& Endpoint, const FString& JsonPayload, bool bRequiresAuth, const FString& Token = TEXT(""));

    // ─────────────────────────────────────────────────────────────────────────────
    // [응답 핸들러]
    // ─────────────────────────────────────────────────────────────────────────────

    void OnMapListResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void OnMapInfoResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void OnGameOptionsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void OnGameDataResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void OnGameResultResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void OnRankingResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void OnFriendListResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void OnSimpleResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

    // ─────────────────────────────────────────────────────────────────────────────
    // [JSON 파싱 함수]
    // ─────────────────────────────────────────────────────────────────────────────

    bool ParseMapListResponse(const FString& JsonString, FMapListResponse& OutResponse) const;
    bool ParseMapInfo(const FString& JsonString, FMapInfo& OutMapInfo) const;
    bool ParseMapInfo(const TSharedPtr<FJsonObject>& JsonObject, FMapInfo& OutMapInfo) const; // 오버로드 (내부용)
    bool ParseGameOptions(const FString& JsonString, FGameOptions& OutOptions) const;
    bool ParseGameData(const FString& JsonString, FGameData& OutGameData) const;
    bool ParseGameResult(const FString& JsonString, FGameResult& OutResult) const;
    bool ParseRankingResponse(const FString& JsonString, FRankingResponse& OutResponse) const;
    bool ParseFriendList(const FString& JsonString, FFriendListResponse& OutFriendList) const;

    // ─────────────────────────────────────────────────────────────────────────────
    // [JSON 직렬화 함수]
    // ─────────────────────────────────────────────────────────────────────────────

    FString GameOptionsToJson(const FGameOptions& Options) const;
    FString GameDataToJson(const FGameData& GameData) const;
    FString GameResultToJson(const FGameResult& Result) const;

    // ─────────────────────────────────────────────────────────────────────────────
    // [로깅 헬퍼]
    // ─────────────────────────────────────────────────────────────────────────────

    void LogRequest(const FString& RequestType, const FString& Endpoint, const FString& Payload);
    void LogResponse(const FString& RequestType, const FString& ResponseBody, bool bSuccess);
    void LogError(const FString& RequestType, int32 ErrorCode, const FString& ErrorMessage);
};
