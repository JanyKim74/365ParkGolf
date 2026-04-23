// Fill out your copyright notice in the Description page of Project Settings.

#include "ParkGameNetworkManager.h"
#include "Http.h"
#include "HttpModule.h"
#include "JsonUtilities.h"
#include "Json.h"
#include "Interfaces/IHttpResponse.h"
#include "Kismet/GameplayStatics.h"

AParkGameNetworkManager::AParkGameNetworkManager()
{
    PrimaryActorTick.bCanEverTick = false;
    NetworkManager = nullptr;
}

void AParkGameNetworkManager::BeginPlay()
{
    Super::BeginPlay();

    // 레벨에서 NetworkManager 자동 탐색
    if (!NetworkManager)
    {
        TArray<AActor*> FoundActors;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ANetworkManager::StaticClass(), FoundActors);

        if (FoundActors.Num() > 0)
        {
            NetworkManager = Cast<ANetworkManager>(FoundActors[0]);
            UE_LOG(LogTemp, Log, TEXT("✅ GameNetworkManager: NetworkManager를 찾았습니다."));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("❌ GameNetworkManager: NetworkManager를 찾을 수 없습니다."));
        }
    }
}

void AParkGameNetworkManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
    NetworkManager = nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// [네트워크 매니저 참조]
// ─────────────────────────────────────────────────────────────────────────────

void AParkGameNetworkManager::SetNetworkManager(ANetworkManager* InNetworkManager)
{
    NetworkManager = InNetworkManager;
    if (NetworkManager)
    {
        UE_LOG(LogTemp, Log, TEXT("✅ GameNetworkManager: NetworkManager 설정 완료"));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// [2. 맵 관련 API]
// ─────────────────────────────────────────────────────────────────────────────

void AParkGameNetworkManager::GetMapList(const FString& SearchKeyword, int32 Page, FOnMapListResponseReceived OnResponse)
{
    if (!NetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ GetMapList: NetworkManager가 없습니다."));
        OnResponse.ExecuteIfBound(false, FMapListResponse());
        return;
    }

    MapListCallback = OnResponse;
    CurrentRequestType = TEXT("GetMapList");

    // JSON 페이로드 생성
    TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
    JsonObject->SetStringField(TEXT("SearchKeyword"), SearchKeyword);
    JsonObject->SetNumberField(TEXT("Page"), Page);

    FString JsonPayload;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonPayload);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    LogRequest(TEXT("GetMapList"), TEXT("/api/map/list"), JsonPayload);
    OnRequestStarted.Broadcast(TEXT("GetMapList"));

    SendGameHttpRequest(TEXT("/api/map/list"), JsonPayload, false);
}

void AParkGameNetworkManager::LoadMap(const FString& MapName, FOnMapInfoResponseReceived OnResponse)
{
    if (!NetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ LoadMap: NetworkManager가 없습니다."));
        OnResponse.ExecuteIfBound(false, FMapInfo());
        return;
    }

    MapInfoCallback = OnResponse;
    CurrentRequestType = TEXT("LoadMap");

    TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
    JsonObject->SetStringField(TEXT("MapName"), MapName);

    FString JsonPayload;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonPayload);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    LogRequest(TEXT("LoadMap"), TEXT("/api/map/load"), JsonPayload);
    OnRequestStarted.Broadcast(TEXT("LoadMap"));

    SendGameHttpRequest(TEXT("/api/map/load"), JsonPayload, false);
}

// ─────────────────────────────────────────────────────────────────────────────
// [3. 게임 옵션 API]
// ─────────────────────────────────────────────────────────────────────────────

void AParkGameNetworkManager::GetGameOptions(const FString& Token, FOnGameOptionsResponseReceived OnResponse)
{
    if (!NetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ GetGameOptions: NetworkManager가 없습니다."));
        OnResponse.ExecuteIfBound(false, FGameOptions());
        return;
    }

    GameOptionsCallback = OnResponse;
    CurrentRequestType = TEXT("GetGameOptions");

    LogRequest(TEXT("GetGameOptions"), TEXT("/api/game/options"), TEXT("(Token-based)"));
    OnRequestStarted.Broadcast(TEXT("GetGameOptions"));

    SendGameHttpRequest(TEXT("/api/game/options"), TEXT(""), true, Token);
}

void AParkGameNetworkManager::SaveGameOptions(const FString& Token, const FGameOptions& Options, FOnSimpleResponse OnResponse)
{
    if (!NetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ SaveGameOptions: NetworkManager가 없습니다."));
        OnResponse.ExecuteIfBound(false);
        return;
    }

    SimpleResponseCallback = OnResponse;
    CurrentRequestType = TEXT("SaveGameOptions");

    FString JsonPayload = GameOptionsToJson(Options);
    LogRequest(TEXT("SaveGameOptions"), TEXT("/api/game/options/save"), JsonPayload);
    OnRequestStarted.Broadcast(TEXT("SaveGameOptions"));

    SendGameHttpRequest(TEXT("/api/game/options/save"), JsonPayload, true, Token);
}

// ─────────────────────────────────────────────────────────────────────────────
// [4. 게임 데이터 API]
// ─────────────────────────────────────────────────────────────────────────────

void AParkGameNetworkManager::GetGameData(const FString& GameID, FOnGameDataResponseReceived OnResponse)
{
    if (!NetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ GetGameData: NetworkManager가 없습니다."));
        OnResponse.ExecuteIfBound(false, FGameData());
        return;
    }

    GameDataCallback = OnResponse;
    CurrentRequestType = TEXT("GetGameData");

    TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
    JsonObject->SetStringField(TEXT("GameID"), GameID);

    FString JsonPayload;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonPayload);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    LogRequest(TEXT("GetGameData"), TEXT("/api/game/data"), JsonPayload);
    OnRequestStarted.Broadcast(TEXT("GetGameData"));

    SendGameHttpRequest(TEXT("/api/game/data"), JsonPayload, true);
}

void AParkGameNetworkManager::UpdateGameData(const FString& GameID, const FGameData& GameData, FOnSimpleResponse OnResponse)
{
    if (!NetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ UpdateGameData: NetworkManager가 없습니다."));
        OnResponse.ExecuteIfBound(false);
        return;
    }

    SimpleResponseCallback = OnResponse;
    CurrentRequestType = TEXT("UpdateGameData");

    FString JsonPayload = GameDataToJson(GameData);
    LogRequest(TEXT("UpdateGameData"), TEXT("/api/game/data/update"), JsonPayload);
    OnRequestStarted.Broadcast(TEXT("UpdateGameData"));

    SendGameHttpRequest(TEXT("/api/game/data/update"), JsonPayload, true);
}

// ─────────────────────────────────────────────────────────────────────────────
// [5. 게임 결과 API]
// ─────────────────────────────────────────────────────────────────────────────

void AParkGameNetworkManager::SaveGameResult(const FString& Token, const FGameResult& Result, FOnGameResultResponseReceived OnResponse)
{
    if (!NetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ SaveGameResult: NetworkManager가 없습니다."));
        OnResponse.ExecuteIfBound(false, FGameResult());
        return;
    }

    GameResultCallback = OnResponse;
    CurrentRequestType = TEXT("SaveGameResult");

    FString JsonPayload = GameResultToJson(Result);
    LogRequest(TEXT("SaveGameResult"), TEXT("/api/game/result/save"), JsonPayload);
    OnRequestStarted.Broadcast(TEXT("SaveGameResult"));

    SendGameHttpRequest(TEXT("/api/game/result/save"), JsonPayload, true, Token);
}

void AParkGameNetworkManager::GetGameResults(const FString& Token, int32 Page, FOnGameResultResponseReceived OnResponse)
{
    if (!NetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ GetGameResults: NetworkManager가 없습니다."));
        OnResponse.ExecuteIfBound(false, FGameResult());
        return;
    }

    GameResultCallback = OnResponse;
    CurrentRequestType = TEXT("GetGameResults");

    TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
    JsonObject->SetNumberField(TEXT("Page"), Page);

    FString JsonPayload;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonPayload);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    LogRequest(TEXT("GetGameResults"), TEXT("/api/game/result/list"), JsonPayload);
    OnRequestStarted.Broadcast(TEXT("GetGameResults"));

    SendGameHttpRequest(TEXT("/api/game/result/list"), JsonPayload, true, Token);
}

// ─────────────────────────────────────────────────────────────────────────────
// [6. 랭킹 API]
// ─────────────────────────────────────────────────────────────────────────────

void AParkGameNetworkManager::GetRankings(int32 Type, int32 Page, FOnRankingResponseReceived OnResponse)
{
    if (!NetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ GetRankings: NetworkManager가 없습니다."));
        OnResponse.ExecuteIfBound(false, FRankingResponse());
        return;
    }

    RankingCallback = OnResponse;
    CurrentRequestType = TEXT("GetRankings");

    TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
    JsonObject->SetNumberField(TEXT("Type"), Type);
    JsonObject->SetNumberField(TEXT("Page"), Page);

    FString JsonPayload;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonPayload);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    LogRequest(TEXT("GetRankings"), TEXT("/api/ranking"), JsonPayload);
    OnRequestStarted.Broadcast(TEXT("GetRankings"));

    SendGameHttpRequest(TEXT("/api/ranking"), JsonPayload, true);
}

// ─────────────────────────────────────────────────────────────────────────────
// [7. 친구 API]
// ─────────────────────────────────────────────────────────────────────────────

void AParkGameNetworkManager::GetFriendList(const FString& Token, FOnFriendListResponseReceived OnResponse)
{
    if (!NetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ GetFriendList: NetworkManager가 없습니다."));
        OnResponse.ExecuteIfBound(false, FFriendListResponse());
        return;
    }

    FriendListCallback = OnResponse;
    CurrentRequestType = TEXT("GetFriendList");

    LogRequest(TEXT("GetFriendList"), TEXT("/api/friend/list"), TEXT("(Token-based)"));
    OnRequestStarted.Broadcast(TEXT("GetFriendList"));

    SendGameHttpRequest(TEXT("/api/friend/list"), TEXT(""), true, Token);
}

void AParkGameNetworkManager::AddFriend(const FString& Token, const FString& FriendID, FOnSimpleResponse OnResponse)
{
    if (!NetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ AddFriend: NetworkManager가 없습니다."));
        OnResponse.ExecuteIfBound(false);
        return;
    }

    SimpleResponseCallback = OnResponse;
    CurrentRequestType = TEXT("AddFriend");

    TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
    JsonObject->SetStringField(TEXT("FriendID"), FriendID);

    FString JsonPayload;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonPayload);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    LogRequest(TEXT("AddFriend"), TEXT("/api/friend/add"), JsonPayload);
    OnRequestStarted.Broadcast(TEXT("AddFriend"));

    SendGameHttpRequest(TEXT("/api/friend/add"), JsonPayload, true, Token);
}

void AParkGameNetworkManager::InviteFriend(const FString& Token, const FString& FriendID, const FString& GameID, FOnSimpleResponse OnResponse)
{
    if (!NetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ InviteFriend: NetworkManager가 없습니다."));
        OnResponse.ExecuteIfBound(false);
        return;
    }

    SimpleResponseCallback = OnResponse;
    CurrentRequestType = TEXT("InviteFriend");

    TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
    JsonObject->SetStringField(TEXT("FriendID"), FriendID);
    JsonObject->SetStringField(TEXT("GameID"), GameID);

    FString JsonPayload;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonPayload);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    LogRequest(TEXT("InviteFriend"), TEXT("/api/friend/invite"), JsonPayload);
    OnRequestStarted.Broadcast(TEXT("InviteFriend"));

    SendGameHttpRequest(TEXT("/api/friend/invite"), JsonPayload, true, Token);
}

// ─────────────────────────────────────────────────────────────────────────────
// [내부 함수 - HTTP 통신]
// ─────────────────────────────────────────────────────────────────────────────

void AParkGameNetworkManager::SendGameHttpRequest(const FString& Endpoint, const FString& JsonPayload, bool bRequiresAuth, const FString& Token)
{
    if (!NetworkManager)
    {
        LogError(CurrentRequestType, 0, TEXT("NetworkManager가 없습니다."));
        return;
    }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();

    FString RequestURL = NetworkManager->GetServerURL() + Endpoint;
    Request->SetURL(RequestURL);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("User-Agent"), TEXT("UnrealEngine/4.26"));

    // 인증 토큰 설정 (파라미터 Token 우선, 없으면 NetworkManager 토큰 사용)
    if (bRequiresAuth)
    {
        FString AuthToken = Token.IsEmpty() ? NetworkManager->GetAuthToken() : Token;
        if (!AuthToken.IsEmpty())
        {
            Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
            UE_LOG(LogTemp, Log, TEXT("🔐 인증 토큰 헤더 추가"));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("⚠️ 인증 토큰이 없습니다: %s"), *CurrentRequestType);
        }
    }

    if (!JsonPayload.IsEmpty())
    {
        Request->SetContentAsString(JsonPayload);
    }

    // 요청 타입별 응답 핸들러 바인딩
    if (CurrentRequestType == TEXT("GetMapList"))
    {
        Request->OnProcessRequestComplete().BindUObject(this, &AParkGameNetworkManager::OnMapListResponse);
    }
    else if (CurrentRequestType == TEXT("LoadMap"))
    {
        Request->OnProcessRequestComplete().BindUObject(this, &AParkGameNetworkManager::OnMapInfoResponse);
    }
    else if (CurrentRequestType == TEXT("GetGameOptions"))
    {
        Request->OnProcessRequestComplete().BindUObject(this, &AParkGameNetworkManager::OnGameOptionsResponse);
    }
    else if (CurrentRequestType == TEXT("GetGameData"))
    {
        Request->OnProcessRequestComplete().BindUObject(this, &AParkGameNetworkManager::OnGameDataResponse);
    }
    else if (CurrentRequestType == TEXT("SaveGameResult") || CurrentRequestType == TEXT("GetGameResults"))
    {
        Request->OnProcessRequestComplete().BindUObject(this, &AParkGameNetworkManager::OnGameResultResponse);
    }
    else if (CurrentRequestType == TEXT("GetRankings"))
    {
        Request->OnProcessRequestComplete().BindUObject(this, &AParkGameNetworkManager::OnRankingResponse);
    }
    else if (CurrentRequestType == TEXT("GetFriendList"))
    {
        Request->OnProcessRequestComplete().BindUObject(this, &AParkGameNetworkManager::OnFriendListResponse);
    }
    else
    {
        // SaveGameOptions, UpdateGameData, AddFriend, InviteFriend 등 단순 응답
        Request->OnProcessRequestComplete().BindUObject(this, &AParkGameNetworkManager::OnSimpleResponse);
    }

    if (!Request->ProcessRequest())
    {
        LogError(CurrentRequestType, 0, TEXT("HTTP 요청 전송 실패"));
        OnRequestFailed.Broadcast(CurrentRequestType, 0, TEXT("HTTP 요청 전송 실패"));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("✅ HTTP 요청 전송: %s"), *RequestURL);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// [응답 핸들러]
// ─────────────────────────────────────────────────────────────────────────────

void AParkGameNetworkManager::OnMapListResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        int32 ErrorCode = Response.IsValid() ? Response->GetResponseCode() : 0;
        LogError(TEXT("GetMapList"), ErrorCode, TEXT("네트워크 요청 실패"));
        OnRequestFailed.Broadcast(TEXT("GetMapList"), ErrorCode, TEXT("네트워크 요청 실패"));
        MapListCallback.ExecuteIfBound(false, FMapListResponse());
        return;
    }

    FString ResponseBody = Response->GetContentAsString();
    LogResponse(TEXT("GetMapList"), ResponseBody, true);

    FMapListResponse MapList;
    if (ParseMapListResponse(ResponseBody, MapList))
    {
        UE_LOG(LogTemp, Log, TEXT("✅ 맵 목록 조회 성공: %d개"), MapList.Maps.Num());
        MapListCallback.ExecuteIfBound(true, MapList);
    }
    else
    {
        LogError(TEXT("GetMapList"), Response->GetResponseCode(), TEXT("JSON 파싱 실패"));
        MapListCallback.ExecuteIfBound(false, FMapListResponse());
    }
}

void AParkGameNetworkManager::OnMapInfoResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        int32 ErrorCode = Response.IsValid() ? Response->GetResponseCode() : 0;
        LogError(TEXT("LoadMap"), ErrorCode, TEXT("네트워크 요청 실패"));
        OnRequestFailed.Broadcast(TEXT("LoadMap"), ErrorCode, TEXT("네트워크 요청 실패"));
        MapInfoCallback.ExecuteIfBound(false, FMapInfo());
        return;
    }

    FString ResponseBody = Response->GetContentAsString();
    LogResponse(TEXT("LoadMap"), ResponseBody, true);

    FMapInfo MapInfo;
    if (ParseMapInfo(ResponseBody, MapInfo))
    {
        UE_LOG(LogTemp, Log, TEXT("✅ 맵 로딩 성공: %s"), *MapInfo.MapName);
        MapInfoCallback.ExecuteIfBound(true, MapInfo);
    }
    else
    {
        LogError(TEXT("LoadMap"), Response->GetResponseCode(), TEXT("JSON 파싱 실패"));
        MapInfoCallback.ExecuteIfBound(false, FMapInfo());
    }
}

void AParkGameNetworkManager::OnGameOptionsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        int32 ErrorCode = Response.IsValid() ? Response->GetResponseCode() : 0;
        LogError(TEXT("GetGameOptions"), ErrorCode, TEXT("네트워크 요청 실패"));
        OnRequestFailed.Broadcast(TEXT("GetGameOptions"), ErrorCode, TEXT("네트워크 요청 실패"));
        GameOptionsCallback.ExecuteIfBound(false, FGameOptions());
        return;
    }

    FString ResponseBody = Response->GetContentAsString();
    LogResponse(TEXT("GetGameOptions"), ResponseBody, true);

    FGameOptions Options;
    if (ParseGameOptions(ResponseBody, Options))
    {
        UE_LOG(LogTemp, Log, TEXT("✅ 게임 옵션 조회 성공"));
        GameOptionsCallback.ExecuteIfBound(true, Options);
    }
    else
    {
        LogError(TEXT("GetGameOptions"), Response->GetResponseCode(), TEXT("JSON 파싱 실패"));
        GameOptionsCallback.ExecuteIfBound(false, FGameOptions());
    }
}

void AParkGameNetworkManager::OnGameDataResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        int32 ErrorCode = Response.IsValid() ? Response->GetResponseCode() : 0;
        LogError(TEXT("GetGameData"), ErrorCode, TEXT("네트워크 요청 실패"));
        OnRequestFailed.Broadcast(TEXT("GetGameData"), ErrorCode, TEXT("네트워크 요청 실패"));
        GameDataCallback.ExecuteIfBound(false, FGameData());
        return;
    }

    FString ResponseBody = Response->GetContentAsString();
    LogResponse(TEXT("GetGameData"), ResponseBody, true);

    FGameData GameData;
    if (ParseGameData(ResponseBody, GameData))
    {
        UE_LOG(LogTemp, Log, TEXT("✅ 게임 데이터 조회 성공"));
        GameDataCallback.ExecuteIfBound(true, GameData);
    }
    else
    {
        LogError(TEXT("GetGameData"), Response->GetResponseCode(), TEXT("JSON 파싱 실패"));
        GameDataCallback.ExecuteIfBound(false, FGameData());
    }
}

void AParkGameNetworkManager::OnGameResultResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        int32 ErrorCode = Response.IsValid() ? Response->GetResponseCode() : 0;
        LogError(TEXT("GameResult"), ErrorCode, TEXT("네트워크 요청 실패"));
        OnRequestFailed.Broadcast(TEXT("GameResult"), ErrorCode, TEXT("네트워크 요청 실패"));
        GameResultCallback.ExecuteIfBound(false, FGameResult());
        return;
    }

    FString ResponseBody = Response->GetContentAsString();
    LogResponse(TEXT("GameResult"), ResponseBody, true);

    FGameResult Result;
    if (ParseGameResult(ResponseBody, Result))
    {
        UE_LOG(LogTemp, Log, TEXT("✅ 게임 결과 처리 성공: %s"), *Result.GameID);
        GameResultCallback.ExecuteIfBound(true, Result);
    }
    else
    {
        LogError(TEXT("GameResult"), Response->GetResponseCode(), TEXT("JSON 파싱 실패"));
        GameResultCallback.ExecuteIfBound(false, FGameResult());
    }
}

void AParkGameNetworkManager::OnRankingResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        int32 ErrorCode = Response.IsValid() ? Response->GetResponseCode() : 0;
        LogError(TEXT("GetRankings"), ErrorCode, TEXT("네트워크 요청 실패"));
        OnRequestFailed.Broadcast(TEXT("GetRankings"), ErrorCode, TEXT("네트워크 요청 실패"));
        RankingCallback.ExecuteIfBound(false, FRankingResponse());
        return;
    }

    FString ResponseBody = Response->GetContentAsString();
    LogResponse(TEXT("GetRankings"), ResponseBody, true);

    FRankingResponse RankingResponse;
    if (ParseRankingResponse(ResponseBody, RankingResponse))
    {
        UE_LOG(LogTemp, Log, TEXT("✅ 랭킹 조회 성공: %d개 항목"), RankingResponse.Entries.Num());
        RankingCallback.ExecuteIfBound(true, RankingResponse);
    }
    else
    {
        LogError(TEXT("GetRankings"), Response->GetResponseCode(), TEXT("JSON 파싱 실패"));
        RankingCallback.ExecuteIfBound(false, FRankingResponse());
    }
}

void AParkGameNetworkManager::OnFriendListResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        int32 ErrorCode = Response.IsValid() ? Response->GetResponseCode() : 0;
        LogError(TEXT("GetFriendList"), ErrorCode, TEXT("네트워크 요청 실패"));
        OnRequestFailed.Broadcast(TEXT("GetFriendList"), ErrorCode, TEXT("네트워크 요청 실패"));
        FriendListCallback.ExecuteIfBound(false, FFriendListResponse());
        return;
    }

    FString ResponseBody = Response->GetContentAsString();
    LogResponse(TEXT("GetFriendList"), ResponseBody, true);

    FFriendListResponse FriendList;
    if (ParseFriendList(ResponseBody, FriendList))
    {
        UE_LOG(LogTemp, Log, TEXT("✅ 친구 목록 조회 성공: %d명"), FriendList.Friends.Num());
        FriendListCallback.ExecuteIfBound(true, FriendList);
    }
    else
    {
        LogError(TEXT("GetFriendList"), Response->GetResponseCode(), TEXT("JSON 파싱 실패"));
        FriendListCallback.ExecuteIfBound(false, FFriendListResponse());
    }
}

void AParkGameNetworkManager::OnSimpleResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        int32 ErrorCode = Response.IsValid() ? Response->GetResponseCode() : 0;
        LogError(CurrentRequestType, ErrorCode, TEXT("네트워크 요청 실패"));
        OnRequestFailed.Broadcast(CurrentRequestType, ErrorCode, TEXT("네트워크 요청 실패"));
        SimpleResponseCallback.ExecuteIfBound(false);
        return;
    }

    int32 ResponseCode = Response->GetResponseCode();
    FString ResponseBody = Response->GetContentAsString();
    LogResponse(CurrentRequestType, ResponseBody, true);

    // 200~299 범위면 성공으로 처리
    bool bSuccess = (ResponseCode >= 200 && ResponseCode < 300);

    if (bSuccess)
    {
        UE_LOG(LogTemp, Log, TEXT("✅ [%s] 처리 성공"), *CurrentRequestType);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("❌ [%s] 처리 실패: Code=%d"), *CurrentRequestType, ResponseCode);
    }

    SimpleResponseCallback.ExecuteIfBound(bSuccess);
}

// ─────────────────────────────────────────────────────────────────────────────
// [JSON 파싱 함수]
// ─────────────────────────────────────────────────────────────────────────────

bool AParkGameNetworkManager::ParseMapListResponse(const FString& JsonString, FMapListResponse& OutResponse) const
{
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("❌ 맵 목록 JSON 파싱 실패"));
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* MapsArray;
    if (!JsonObject->TryGetArrayField(TEXT("Maps"), MapsArray))
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Maps 필드 없음"));
        return false;
    }

    for (const TSharedPtr<FJsonValue>& MapValue : *MapsArray)
    {
        FMapInfo MapInfo;
        if (ParseMapInfo(MapValue->AsObject(), MapInfo))
        {
            OutResponse.Maps.Add(MapInfo);
        }
    }

    return true;
}

bool AParkGameNetworkManager::ParseMapInfo(const FString& JsonString, FMapInfo& OutMapInfo) const
{
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("❌ 맵 정보 JSON 파싱 실패"));
        return false;
    }

    return ParseMapInfo(JsonObject, OutMapInfo);
}

bool AParkGameNetworkManager::ParseMapInfo(const TSharedPtr<FJsonObject>& JsonObject, FMapInfo& OutMapInfo) const
{
    if (!JsonObject.IsValid()) return false;

    JsonObject->TryGetStringField(TEXT("MapName"), OutMapInfo.MapName);
    JsonObject->TryGetStringField(TEXT("PakName"), OutMapInfo.PakName);
    JsonObject->TryGetStringField(TEXT("MapDescription"), OutMapInfo.MapDescription);
    JsonObject->TryGetStringField(TEXT("MapThumbnail"), OutMapInfo.MapThumbnail);
    JsonObject->TryGetNumberField(TEXT("HoleCount"), OutMapInfo.HoleCount);

    // ParScores 파싱
    const TArray<TSharedPtr<FJsonValue>>* ParScoresArray;
    if (JsonObject->TryGetArrayField(TEXT("ParScores"), ParScoresArray))
    {
        OutMapInfo.ParScores.Empty();
        for (const TSharedPtr<FJsonValue>& Val : *ParScoresArray)
        {
            OutMapInfo.ParScores.Add(static_cast<int32>(Val->AsNumber()));
        }
    }

    return true;
}

bool AParkGameNetworkManager::ParseGameOptions(const FString& JsonString, FGameOptions& OutOptions) const
{
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("❌ 게임 옵션 JSON 파싱 실패"));
        return false;
    }

    JsonObject->TryGetNumberField(TEXT("SelectCourse"), OutOptions.SelectCourse);
    JsonObject->TryGetNumberField(TEXT("ContinuePutting"), OutOptions.ContinuePutting);
    JsonObject->TryGetNumberField(TEXT("HolecupPosition"), OutOptions.HolecupPosition);
    JsonObject->TryGetNumberField(TEXT("MulliganCount"), OutOptions.MulliganCount);
    JsonObject->TryGetNumberField(TEXT("ConcedeDistance"), OutOptions.ConcedeDistance);
    JsonObject->TryGetNumberField(TEXT("GreenSpeed"), OutOptions.GreenSpeed);
    JsonObject->TryGetNumberField(TEXT("PracticeBall"), OutOptions.PracticeBall);
    JsonObject->TryGetNumberField(TEXT("MovieSaveCount"), OutOptions.MovieSaveCount);
    JsonObject->TryGetNumberField(TEXT("CameraMode"), OutOptions.CameraMode);
    JsonObject->TryGetNumberField(TEXT("GameType"), OutOptions.GameType);
    JsonObject->TryGetNumberField(TEXT("SwingMotion"), OutOptions.SwingMotion);

    return true;
}

bool AParkGameNetworkManager::ParseGameData(const FString& JsonString, FGameData& OutGameData) const
{
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("❌ 게임 데이터 JSON 파싱 실패"));
        return false;
    }

    JsonObject->TryGetNumberField(TEXT("CurrentHole"), OutGameData.CurrentHole);
    JsonObject->TryGetNumberField(TEXT("CurrentPlayerIndex"), OutGameData.CurrentPlayerIndex);
    JsonObject->TryGetNumberField(TEXT("LatestUseMulliganPlayerIndex"), OutGameData.LatestUseMulliganPlayerIndex);
    JsonObject->TryGetStringField(TEXT("GameStartTime"), OutGameData.GameStartTime);

    // GameOptions 파싱
    const TSharedPtr<FJsonObject>* GameOptionsObj;
    if (JsonObject->TryGetObjectField(TEXT("GameOptions"), GameOptionsObj))
    {
        FString OptionsJson;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OptionsJson);
        FJsonSerializer::Serialize((*GameOptionsObj).ToSharedRef(), Writer);
        ParseGameOptions(OptionsJson, OutGameData.GameOptions);
    }

    return true;
}

bool AParkGameNetworkManager::ParseGameResult(const FString& JsonString, FGameResult& OutResult) const
{
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("❌ 게임 결과 JSON 파싱 실패"));
        return false;
    }

    JsonObject->TryGetStringField(TEXT("GameID"), OutResult.GameID);
    JsonObject->TryGetStringField(TEXT("EndTime"), OutResult.EndTime);

    // PlayerResults 파싱
    const TArray<TSharedPtr<FJsonValue>>* PlayerResultsArray;
    if (JsonObject->TryGetArrayField(TEXT("PlayerResults"), PlayerResultsArray))
    {
        for (const TSharedPtr<FJsonValue>& Val : *PlayerResultsArray)
        {
            TSharedPtr<FJsonObject> PlayerObj = Val->AsObject();
            if (!PlayerObj.IsValid()) continue;

            FPlayerResult PlayerResult;
            PlayerObj->TryGetStringField(TEXT("PlayerID"), PlayerResult.PlayerID);
            PlayerObj->TryGetNumberField(TEXT("TotalScore"), PlayerResult.TotalScore);
            PlayerObj->TryGetNumberField(TEXT("RankingInGame"), PlayerResult.RankingInGame);
            PlayerObj->TryGetNumberField(TEXT("AvgDistance"), PlayerResult.AvgDistance);

            const TArray<TSharedPtr<FJsonValue>>* HoleScoresArray;
            if (PlayerObj->TryGetArrayField(TEXT("HoleScores"), HoleScoresArray))
            {
                for (const TSharedPtr<FJsonValue>& ScoreVal : *HoleScoresArray)
                {
                    PlayerResult.HoleScores.Add(static_cast<int32>(ScoreVal->AsNumber()));
                }
            }

            OutResult.PlayerResults.Add(PlayerResult);
        }
    }

    return true;
}

bool AParkGameNetworkManager::ParseRankingResponse(const FString& JsonString, FRankingResponse& OutResponse) const
{
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("❌ 랭킹 JSON 파싱 실패"));
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* EntriesArray;
    if (!JsonObject->TryGetArrayField(TEXT("Entries"), EntriesArray))
    {
        return false;
    }

    for (const TSharedPtr<FJsonValue>& Val : *EntriesArray)
    {
        TSharedPtr<FJsonObject> EntryObj = Val->AsObject();
        if (!EntryObj.IsValid()) continue;

        FRankingEntry Entry;
        EntryObj->TryGetStringField(TEXT("NickName"), Entry.NickName);
        EntryObj->TryGetNumberField(TEXT("Type"), Entry.Type);
        EntryObj->TryGetNumberField(TEXT("Rank"), Entry.Rank);
        EntryObj->TryGetNumberField(TEXT("Score"), Entry.Score);
        EntryObj->TryGetNumberField(TEXT("Level"), Entry.Level);

        OutResponse.Entries.Add(Entry);
    }

    return true;
}

bool AParkGameNetworkManager::ParseFriendList(const FString& JsonString, FFriendListResponse& OutFriendList) const
{
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("❌ 친구 목록 JSON 파싱 실패"));
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* FriendsArray;
    if (!JsonObject->TryGetArrayField(TEXT("Friends"), FriendsArray))
    {
        return false;
    }

    for (const TSharedPtr<FJsonValue>& Val : *FriendsArray)
    {
        TSharedPtr<FJsonObject> FriendObj = Val->AsObject();
        if (!FriendObj.IsValid()) continue;

        FFriendInfo Friend;
        FriendObj->TryGetStringField(TEXT("FriendID"), Friend.FriendID);
        FriendObj->TryGetStringField(TEXT("NickName"), Friend.NickName);
        FriendObj->TryGetBoolField(TEXT("bOnline"), Friend.bOnline);
        FriendObj->TryGetNumberField(TEXT("Level"), Friend.Level);

        OutFriendList.Friends.Add(Friend);
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// [JSON 직렬화 함수]
// ─────────────────────────────────────────────────────────────────────────────

FString AParkGameNetworkManager::GameOptionsToJson(const FGameOptions& Options) const
{
    TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();

    JsonObject->SetNumberField(TEXT("SelectCourse"), Options.SelectCourse);
    JsonObject->SetNumberField(TEXT("ContinuePutting"), Options.ContinuePutting);
    JsonObject->SetNumberField(TEXT("HolecupPosition"), Options.HolecupPosition);
    JsonObject->SetNumberField(TEXT("MulliganCount"), Options.MulliganCount);
    JsonObject->SetNumberField(TEXT("ConcedeDistance"), Options.ConcedeDistance);
    JsonObject->SetNumberField(TEXT("GreenSpeed"), Options.GreenSpeed);
    JsonObject->SetNumberField(TEXT("PracticeBall"), Options.PracticeBall);
    JsonObject->SetNumberField(TEXT("MovieSaveCount"), Options.MovieSaveCount);
    JsonObject->SetNumberField(TEXT("CameraMode"), Options.CameraMode);
    JsonObject->SetNumberField(TEXT("GameType"), Options.GameType);
    JsonObject->SetNumberField(TEXT("SwingMotion"), Options.SwingMotion);

    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    return JsonString;
}

FString AParkGameNetworkManager::GameDataToJson(const FGameData& GameData) const
{
    TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();

    JsonObject->SetNumberField(TEXT("CurrentHole"), GameData.CurrentHole);
    JsonObject->SetNumberField(TEXT("CurrentPlayerIndex"), GameData.CurrentPlayerIndex);
    JsonObject->SetNumberField(TEXT("LatestUseMulliganPlayerIndex"), GameData.LatestUseMulliganPlayerIndex);
    JsonObject->SetStringField(TEXT("GameStartTime"), GameData.GameStartTime);

    // GameOptions 직렬화
    FString OptionsJson = GameOptionsToJson(GameData.GameOptions);
    TSharedPtr<FJsonObject> OptionsObject;
    TSharedRef<TJsonReader<>> OptionsReader = TJsonReaderFactory<>::Create(OptionsJson);
    if (FJsonSerializer::Deserialize(OptionsReader, OptionsObject) && OptionsObject.IsValid())
    {
        JsonObject->SetObjectField(TEXT("GameOptions"), OptionsObject);
    }

    // Players 직렬화 (기본 정보만 전송)
    TArray<TSharedPtr<FJsonValue>> PlayersArray;
    for (const FPlayerInfo& Player : GameData.Players)
    {
        TSharedPtr<FJsonObject> PlayerObj = MakeShared<FJsonObject>();
        PlayerObj->SetStringField(TEXT("ID"), Player.ID);
        PlayerObj->SetStringField(TEXT("NickName"), Player.NickName);
        PlayerObj->SetNumberField(TEXT("SlotIndex"), Player.SlotIndex);
        PlayerObj->SetBoolField(TEXT("IsGuest"), Player.IsGuest);
        PlayerObj->SetNumberField(TEXT("TotalScore"), Player.TotalScore);
        PlayerObj->SetNumberField(TEXT("ShotCount"), Player.ShotCount);
        PlayerObj->SetNumberField(TEXT("HoleCount"), Player.HoleCount);

        // HoleScores 배열
        TArray<TSharedPtr<FJsonValue>> HoleScoresArray;
        for (int32 Score : Player.HoleScores)
        {
            HoleScoresArray.Add(MakeShared<FJsonValueNumber>(Score));
        }
        PlayerObj->SetArrayField(TEXT("HoleScores"), HoleScoresArray);

        // BallPos
        PlayerObj->SetNumberField(TEXT("BallPosX"), Player.BallPosX);
        PlayerObj->SetNumberField(TEXT("BallPosY"), Player.BallPosY);
        PlayerObj->SetNumberField(TEXT("BallPosZ"), Player.BallPosZ);

        PlayerObj->SetNumberField(TEXT("MulliganCount"), Player.MulliganCount);

        PlayersArray.Add(MakeShared<FJsonValueObject>(PlayerObj));
    }
    JsonObject->SetArrayField(TEXT("Players"), PlayersArray);

    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    return JsonString;
}

FString AParkGameNetworkManager::GameResultToJson(const FGameResult& Result) const
{
    TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();

    JsonObject->SetStringField(TEXT("GameID"), Result.GameID);
    JsonObject->SetStringField(TEXT("EndTime"), Result.EndTime);

    // PlayerResults 직렬화
    TArray<TSharedPtr<FJsonValue>> PlayerResultsArray;
    for (const FPlayerResult& PlayerResult : Result.PlayerResults)
    {
        TSharedPtr<FJsonObject> PlayerObj = MakeShared<FJsonObject>();
        PlayerObj->SetStringField(TEXT("PlayerID"), PlayerResult.PlayerID);
        PlayerObj->SetNumberField(TEXT("TotalScore"), PlayerResult.TotalScore);
        PlayerObj->SetNumberField(TEXT("RankingInGame"), PlayerResult.RankingInGame);
        PlayerObj->SetNumberField(TEXT("AvgDistance"), PlayerResult.AvgDistance);

        TArray<TSharedPtr<FJsonValue>> HoleScoresArray;
        for (int32 Score : PlayerResult.HoleScores)
        {
            HoleScoresArray.Add(MakeShared<FJsonValueNumber>(Score));
        }
        PlayerObj->SetArrayField(TEXT("HoleScores"), HoleScoresArray);

        PlayerResultsArray.Add(MakeShared<FJsonValueObject>(PlayerObj));
    }
    JsonObject->SetArrayField(TEXT("PlayerResults"), PlayerResultsArray);

    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    return JsonString;
}

// ─────────────────────────────────────────────────────────────────────────────
// [로깅 헬퍼]
// ─────────────────────────────────────────────────────────────────────────────

void AParkGameNetworkManager::LogRequest(const FString& RequestType, const FString& Endpoint, const FString& Payload)
{
    UE_LOG(LogTemp, Log, TEXT("📤 [%s 요청]\n   URL: %s\n   Payload: %s"),
        *RequestType,
        NetworkManager ? *(NetworkManager->GetServerURL() + Endpoint) : *Endpoint,
        *Payload);
}

void AParkGameNetworkManager::LogResponse(const FString& RequestType, const FString& ResponseBody, bool bSuccess)
{
    if (bSuccess)
    {
        UE_LOG(LogTemp, Log, TEXT("📥 [%s 응답 성공]\n   Body: %s"), *RequestType, *ResponseBody);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("📥 [%s 응답 실패]\n   Body: %s"), *RequestType, *ResponseBody);
    }
}

void AParkGameNetworkManager::LogError(const FString& RequestType, int32 ErrorCode, const FString& ErrorMessage)
{
    UE_LOG(LogTemp, Error, TEXT("❌ [%s 오류]\n   Code: %d\n   Message: %s"),
        *RequestType, ErrorCode, *ErrorMessage);
}
