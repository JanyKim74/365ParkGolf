// Copyright (c) 2025 ParkDay Team. All rights reserved.

#include "JsonHandler.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

bool UJsonHandler::SaveGameInfoToJson(const FGameInfo& GameInfo, const FString& FilePath)
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

	// Players 배열 변환
	TArray<TSharedPtr<FJsonValue>> PlayersArray;
	for (const FPlayerInfo& Player : GameInfo.Players)
	{
		PlayersArray.Add(MakeShareable(new FJsonValueObject(PlayerInfoToJson(Player))));
		UE_LOG(LogTemp, Log, TEXT("-- Player - slotindex [%d]"), Player.SlotIndex);
		UE_LOG(LogTemp, Log, TEXT("-- Player - HoleCount =[%d]"), Player.HoleCount);
		if (Player.HoleScores.Num() > 0)
			UE_LOG(LogTemp, Log, TEXT("-- Player - HoleScores 1 -[%d]"), Player.HoleScores[0]);
		if (Player.ShotCountPerHole.Num() > 0)
			UE_LOG(LogTemp, Log, TEXT("-- Player - ShotCountPerHole 1 - [%d]"), Player.ShotCountPerHole[0]);
	}
	JsonObject->SetArrayField("Players", PlayersArray);
	UE_LOG(LogTemp, Log, TEXT("Saving player info to JSON  -- PlayerCount -[%d]"), PlayersArray.Num());

	// SelectedMap 변환
	JsonObject->SetObjectField("SelectedMap", MapInfoToJson(GameInfo.SelectedMap));

	// GameOptions 변환
	JsonObject->SetObjectField("GameOptions", GameOptionInfoToJson(GameInfo.GameOptions));

	// CurrentHole 및 CurrentPlayerIndex
	JsonObject->SetBoolField("IsRoundEnd", GameInfo.bIsRoundEnd);
	JsonObject->SetBoolField("EventHole", GameInfo.bEventHole);
	JsonObject->SetNumberField("CurrentHole", GameInfo.CurrentHole);
	UE_LOG(LogTemp, Log, TEXT("--JsonSave- GameInfo - CurrentHole -[%d]"), GameInfo.CurrentHole);
	JsonObject->SetNumberField("CurrentPlayerIndex", GameInfo.CurrentPlayerIndex);
	JsonObject->SetNumberField("LatestUseMulliganPlayerIndex", GameInfo.LatestUseMulliganPlayerIndex);
	JsonObject->SetNumberField("LatestShotPlayerSlotIndex", GameInfo.LatestShotPlayerSlotIndex);

	// GameStartTime
	JsonObject->SetStringField("GameStartTime", GameInfo.GameStartTime.ToString());

	// JSON 문자열로 변환
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	if (FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer))
	{
		const bool bSuccess = FFileHelper::SaveStringToFile(OutputString, *FilePath);
		UE_LOG(LogTemp, Log, TEXT("SaveGameInfoToJson: %s to %s"), bSuccess ? TEXT("Success") : TEXT("Failed"), *FilePath);
		return bSuccess;
	}

	UE_LOG(LogTemp, Error, TEXT("SaveGameInfoToJson: Failed to serialize JSON"));
	return false;
}

bool UJsonHandler::LoadGameInfoFromJson(FGameInfo& OutGameInfo, const FString& FilePath)
{
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("LoadGameInfoFromJson: Failed to load file %s"), *FilePath);
		return false;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("LoadGameInfoFromJson: Failed to parse JSON from %s"), *FilePath);
		return false;
	}

	// Players 배열 로드
	OutGameInfo.Players.Empty();
	const TArray<TSharedPtr<FJsonValue>>* PlayersArray = nullptr;
	if (JsonObject->TryGetArrayField("Players", PlayersArray) && PlayersArray)
	{
		for (const TSharedPtr<FJsonValue>& PlayerValue : *PlayersArray)
		{
			if (PlayerValue.IsValid() && PlayerValue->Type == EJson::Object)
			{
				FPlayerInfo PlayerInfo;
				if (JsonToPlayerInfo(PlayerValue->AsObject(), PlayerInfo))
				{
					OutGameInfo.Players.Add(PlayerInfo);
				}
			}
		}
	}

	// SelectedMap 로드
	const TSharedPtr<FJsonObject>* MapObject = nullptr;
	if (JsonObject->TryGetObjectField("SelectedMap", MapObject) && MapObject && MapObject->IsValid())
	{
		JsonToMapInfo(*MapObject, OutGameInfo.SelectedMap);
	}

	// GameOptions 로드
	const TSharedPtr<FJsonObject>* OptionsObject = nullptr;
	if (JsonObject->TryGetObjectField("GameOptions", OptionsObject) && OptionsObject && OptionsObject->IsValid())
	{
		JsonToGameOptionInfo(*OptionsObject, OutGameInfo.GameOptions);
	}

	// CurrentHole 및 CurrentPlayerIndex
	JsonObject->TryGetBoolField("IsRoundEnd", OutGameInfo.bIsRoundEnd);
	JsonObject->TryGetBoolField("EventHole", OutGameInfo.bEventHole);
	JsonObject->TryGetNumberField("CurrentHole", OutGameInfo.CurrentHole);
	UE_LOG(LogTemp, Log, TEXT("JsonLoadData-- GameInfo - CurrentHole -[%d]"), OutGameInfo.CurrentHole);
	JsonObject->TryGetNumberField("CurrentPlayerIndex", OutGameInfo.CurrentPlayerIndex);

	// ✅ 누락되어 있던 값도 로드 (세이브와 대칭)
	JsonObject->TryGetNumberField("LatestUseMulliganPlayerIndex", OutGameInfo.LatestUseMulliganPlayerIndex);
	JsonObject->TryGetNumberField("LatestShotPlayerSlotIndex", OutGameInfo.LatestShotPlayerSlotIndex);

	// GameStartTime
	FString TimeString;
	if (JsonObject->TryGetStringField("GameStartTime", TimeString))
	{
		FDateTime::Parse(TimeString, OutGameInfo.GameStartTime);
	}

	UE_LOG(LogTemp, Log, TEXT("LoadGameInfoFromJson: Loaded from %s, TeePositions: %d"), *FilePath, OutGameInfo.SelectedMap.TeePositions.Num());
	return true;
}

// -------------------- RoundStat (추가) --------------------

TSharedPtr<FJsonObject> UJsonHandler::RoundStatToJson(const FRoundStat& Stat)
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

	JsonObject->SetNumberField("Rank", Stat.Rank);
	JsonObject->SetNumberField("ShotCount", Stat.ShotCount);
	JsonObject->SetNumberField("AverageDistanceOfDriver", Stat.AverageDistanceOfDriver);
	JsonObject->SetNumberField("MaxDistance", Stat.MaxDistance);
	JsonObject->SetNumberField("FairwayArccuracy", Stat.FairwayArccuracy);
	JsonObject->SetNumberField("GreenArccuracy", Stat.GreenArccuracy);
	JsonObject->SetNumberField("GreenPuttCount", Stat.GreenPuttCount);
	JsonObject->SetNumberField("PuttCount", Stat.PuttCount);
	JsonObject->SetNumberField("SandSave", Stat.SandSave);
	JsonObject->SetNumberField("ParSave", Stat.ParSave);

	return JsonObject;
}

bool UJsonHandler::JsonToRoundStat(const TSharedPtr<FJsonObject>& JsonObject, FRoundStat& OutStat)
{
	if (!JsonObject.IsValid())
	{
		return false;
	}

	JsonObject->TryGetNumberField("Rank", OutStat.Rank);
	JsonObject->TryGetNumberField("ShotCount", OutStat.ShotCount);

	double TempDouble = 0.0;
	if (JsonObject->TryGetNumberField("AverageDistanceOfDriver", TempDouble)) OutStat.AverageDistanceOfDriver = (float)TempDouble;
	if (JsonObject->TryGetNumberField("MaxDistance", TempDouble)) OutStat.MaxDistance = (float)TempDouble;
	if (JsonObject->TryGetNumberField("FairwayArccuracy", TempDouble)) OutStat.FairwayArccuracy = (float)TempDouble;
	if (JsonObject->TryGetNumberField("GreenArccuracy", TempDouble)) OutStat.GreenArccuracy = (float)TempDouble;
	if (JsonObject->TryGetNumberField("SandSave", TempDouble)) OutStat.SandSave = (float)TempDouble;
	if (JsonObject->TryGetNumberField("ParSave", TempDouble)) OutStat.ParSave = (float)TempDouble;

	JsonObject->TryGetNumberField("GreenPuttCount", OutStat.GreenPuttCount);
	JsonObject->TryGetNumberField("PuttCount", OutStat.PuttCount);

	return true;
}

// -------------------- Player --------------------

TSharedPtr<FJsonObject> UJsonHandler::PlayerInfoToJson(const FPlayerInfo& PlayerInfo)
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

	JsonObject->SetStringField("ID", PlayerInfo.ID);
	JsonObject->SetStringField("NickName", PlayerInfo.NickName);
	JsonObject->SetNumberField("SlotIndex", PlayerInfo.SlotIndex);
	JsonObject->SetBoolField("IsGuest", PlayerInfo.IsGuest);
	JsonObject->SetNumberField("Level", PlayerInfo.Level);
	JsonObject->SetNumberField("Ranking", PlayerInfo.Ranking);
	JsonObject->SetNumberField("Point", PlayerInfo.Point);
	JsonObject->SetNumberField("Tee_Height", PlayerInfo.Tee_Height);
	JsonObject->SetNumberField("HandiCap", PlayerInfo.HandiCap);
	JsonObject->SetNumberField("RoundCount", PlayerInfo.RoundCount);
	JsonObject->SetNumberField("Avg_Distance", PlayerInfo.Avg_Distance);
	JsonObject->SetNumberField("Last_Date", PlayerInfo.Last_Date);
	JsonObject->SetStringField("Img_Url", PlayerInfo.Img_Url);

	JsonObject->SetNumberField("BallPosX", PlayerInfo.BallPosX);
	JsonObject->SetNumberField("BallPosY", PlayerInfo.BallPosY);
	JsonObject->SetNumberField("BallPosZ", PlayerInfo.BallPosZ);

	JsonObject->SetNumberField("BeforePosX", PlayerInfo.BeforePosX);
	JsonObject->SetNumberField("BeforePosY", PlayerInfo.BeforePosY);
	JsonObject->SetNumberField("BeforePosZ", PlayerInfo.BeforePosZ);

	JsonObject->SetNumberField("TotalScore", PlayerInfo.TotalScore);
	JsonObject->SetNumberField("ShotCount", PlayerInfo.ShotCount);
	JsonObject->SetNumberField("HoleCount", PlayerInfo.HoleCount);
	JsonObject->SetBoolField("bIsHoleout", PlayerInfo.bIsHoleout);
	JsonObject->SetBoolField("bPendingDelete", PlayerInfo.bIsPendingDelete);

	// HoleScores
	TArray<TSharedPtr<FJsonValue>> HoleScoresArray;
	for (int32 Score : PlayerInfo.HoleScores)
	{
		HoleScoresArray.Add(MakeShareable(new FJsonValueNumber(Score)));
	}
	JsonObject->SetArrayField("HoleScores", HoleScoresArray);

	// HoleMulligans
	TArray<TSharedPtr<FJsonValue>> HoleMulliganArray;
	for (bool Mulligan : PlayerInfo.HoleMulligans)
	{
		HoleMulliganArray.Add(MakeShareable(new FJsonValueBoolean(Mulligan)));
	}
	JsonObject->SetArrayField("HoleMulligans", HoleMulliganArray);

	// ShotCountPerHole
	TArray<TSharedPtr<FJsonValue>> ShotCountPerHoleArray;
	for (int32 SC : PlayerInfo.ShotCountPerHole)
	{
		ShotCountPerHoleArray.Add(MakeShareable(new FJsonValueNumber(SC)));
	}
	JsonObject->SetArrayField("ShotCountPerHole", ShotCountPerHoleArray);

	// BallColor
	TSharedPtr<FJsonObject> ColorObject = MakeShareable(new FJsonObject);
	ColorObject->SetNumberField("R", PlayerInfo.BallColor.R);
	ColorObject->SetNumberField("G", PlayerInfo.BallColor.G);
	ColorObject->SetNumberField("B", PlayerInfo.BallColor.B);
	ColorObject->SetNumberField("A", PlayerInfo.BallColor.A);
	JsonObject->SetObjectField("BallColor", ColorObject);
	JsonObject->SetNumberField("Ball_Index", PlayerInfo.BallIndex);

	JsonObject->SetNumberField("MulliganCount", PlayerInfo.MulliganCount);

	// ✅ RoundStat 저장
	JsonObject->SetObjectField("RoundStat", RoundStatToJson(PlayerInfo.RoundStat));

	return JsonObject;
}

bool UJsonHandler::JsonToPlayerInfo(const TSharedPtr<FJsonObject>& JsonObject, FPlayerInfo& OutPlayerInfo)
{
	if (!JsonObject.IsValid())
	{
		return false;
	}

	// 기본(문자/정수/불리언)
	JsonObject->TryGetStringField("ID", OutPlayerInfo.ID);
	JsonObject->TryGetStringField("NickName", OutPlayerInfo.NickName);
	JsonObject->TryGetNumberField("SlotIndex", OutPlayerInfo.SlotIndex);
	JsonObject->TryGetBoolField("IsGuest", OutPlayerInfo.IsGuest);
	JsonObject->TryGetNumberField("Level", OutPlayerInfo.Level);
	JsonObject->TryGetNumberField("Ranking", OutPlayerInfo.Ranking);
	JsonObject->TryGetNumberField("Point", OutPlayerInfo.Point);
	JsonObject->TryGetNumberField("Tee_Height", OutPlayerInfo.Tee_Height);
	JsonObject->TryGetNumberField("HandiCap", OutPlayerInfo.HandiCap);
	JsonObject->TryGetNumberField("RoundCount", OutPlayerInfo.RoundCount);
	JsonObject->TryGetNumberField("Last_Date", OutPlayerInfo.Last_Date);
	JsonObject->TryGetStringField("Img_Url", OutPlayerInfo.Img_Url);

	JsonObject->TryGetNumberField("TotalScore", OutPlayerInfo.TotalScore);
	JsonObject->TryGetNumberField("ShotCount", OutPlayerInfo.ShotCount);
	JsonObject->TryGetNumberField("HoleCount", OutPlayerInfo.HoleCount);
	JsonObject->TryGetBoolField("bIsHoleout", OutPlayerInfo.bIsHoleout);
	JsonObject->TryGetBoolField("bPendingDelete", OutPlayerInfo.bIsPendingDelete);
	JsonObject->TryGetNumberField("MulliganCount", OutPlayerInfo.MulliganCount);
	JsonObject->TryGetNumberField("Ball_Index", OutPlayerInfo.BallIndex);

	// float 계열 (JSON number -> double)
	double TempDouble = 0.0;
	if (JsonObject->TryGetNumberField("Avg_Distance", TempDouble)) OutPlayerInfo.Avg_Distance = (float)TempDouble;

	if (JsonObject->TryGetNumberField("BallPosX", TempDouble)) OutPlayerInfo.BallPosX = (float)TempDouble;
	if (JsonObject->TryGetNumberField("BallPosY", TempDouble)) OutPlayerInfo.BallPosY = (float)TempDouble;
	if (JsonObject->TryGetNumberField("BallPosZ", TempDouble)) OutPlayerInfo.BallPosZ = (float)TempDouble;

	if (JsonObject->TryGetNumberField("BeforePosX", TempDouble)) OutPlayerInfo.BeforePosX = (float)TempDouble;
	if (JsonObject->TryGetNumberField("BeforePosY", TempDouble)) OutPlayerInfo.BeforePosY = (float)TempDouble;
	if (JsonObject->TryGetNumberField("BeforePosZ", TempDouble)) OutPlayerInfo.BeforePosZ = (float)TempDouble;

	// HoleScores
	OutPlayerInfo.HoleScores.Empty();
	const TArray<TSharedPtr<FJsonValue>>* HoleScoresArray = nullptr;
	if (JsonObject->TryGetArrayField("HoleScores", HoleScoresArray) && HoleScoresArray)
	{
		for (const TSharedPtr<FJsonValue>& ScoreValue : *HoleScoresArray)
		{
			if (ScoreValue.IsValid())
			{
				OutPlayerInfo.HoleScores.Add((int32)ScoreValue->AsNumber());
			}
		}
	}

	// HoleMulliganArray
	OutPlayerInfo.HoleMulligans.Empty();
	const TArray<TSharedPtr<FJsonValue>>* HoleMulliganArray = nullptr;
	if (JsonObject->TryGetArrayField("HoleMulligans", HoleMulliganArray) && HoleMulliganArray)
	{
		for (const TSharedPtr<FJsonValue>& MulliganValue : *HoleMulliganArray)
		{
			if (MulliganValue.IsValid())
			{
				OutPlayerInfo.HoleMulligans.Add(MulliganValue->AsBool());
			}
		}
	}

	// ShotCountPerHole
	OutPlayerInfo.ShotCountPerHole.Empty();
	const TArray<TSharedPtr<FJsonValue>>* ShotCountPerHoleArray = nullptr;
	if (JsonObject->TryGetArrayField("ShotCountPerHole", ShotCountPerHoleArray) && ShotCountPerHoleArray)
	{
		for (const TSharedPtr<FJsonValue>& ShotCountValue : *ShotCountPerHoleArray)
		{
			if (ShotCountValue.IsValid())
			{
				OutPlayerInfo.ShotCountPerHole.Add((int32)ShotCountValue->AsNumber());
			}
		}
	}

	// BallColor
	const TSharedPtr<FJsonObject>* ColorObject = nullptr;
	if (JsonObject->TryGetObjectField("BallColor", ColorObject) && ColorObject && ColorObject->IsValid())
	{
		double R = 1.0, G = 1.0, B = 1.0, A = 1.0;
		(*ColorObject)->TryGetNumberField("R", R);
		(*ColorObject)->TryGetNumberField("G", G);
		(*ColorObject)->TryGetNumberField("B", B);
		(*ColorObject)->TryGetNumberField("A", A);
		OutPlayerInfo.BallColor = FLinearColor((float)R, (float)G, (float)B, (float)A);
	}

	// ✅ RoundStat 로드 (구버전 JSON에는 없을 수 있음 -> 기본값 유지)
	const TSharedPtr<FJsonObject>* RoundStatObj = nullptr;
	if (JsonObject->TryGetObjectField("RoundStat", RoundStatObj) && RoundStatObj && RoundStatObj->IsValid())
	{
		JsonToRoundStat(*RoundStatObj, OutPlayerInfo.RoundStat);
	}

	return true;
}

// -------------------- GameOption --------------------

TSharedPtr<FJsonObject> UJsonHandler::GameOptionInfoToJson(const FGameOptionInfo& GameOptionInfo)
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	JsonObject->SetNumberField("SelectCourse", GameOptionInfo.SelectCourse);
	JsonObject->SetNumberField("ContinuePutting", GameOptionInfo.ContinuePutting);
	JsonObject->SetNumberField("Holecup_Position", GameOptionInfo.Holecup_Position);
	JsonObject->SetNumberField("Mulligan_Count", GameOptionInfo.Mulligan_Count);
	JsonObject->SetNumberField("Concede_Distance", GameOptionInfo.Concede_Distance);
	JsonObject->SetNumberField("Green_Speed", GameOptionInfo.Green_Speed);
	JsonObject->SetNumberField("PracticeBall", GameOptionInfo.PracticeBall);
	JsonObject->SetNumberField("Movie_SaveCount", GameOptionInfo.Movie_SaveCount);
	JsonObject->SetNumberField("Camera_Mode", GameOptionInfo.Camera_Mode);
	JsonObject->SetNumberField("GameType", GameOptionInfo.GameType);
	JsonObject->SetNumberField("SwingMotion", GameOptionInfo.SwingMotion);
	JsonObject->SetNumberField("RangeSwingMotion", GameOptionInfo.RangeSwingMotion);  // ✅ 추가

	return JsonObject;
}

bool UJsonHandler::JsonToGameOptionInfo(const TSharedPtr<FJsonObject>& JsonObject, FGameOptionInfo& OutGameOptionInfo)
{
	if (!JsonObject.IsValid())
	{
		return false;
	}

	JsonObject->TryGetNumberField("SelectCourse", OutGameOptionInfo.SelectCourse);
	JsonObject->TryGetNumberField("Holecup_Position", OutGameOptionInfo.Holecup_Position);
	JsonObject->TryGetNumberField("ContinuePutting", OutGameOptionInfo.ContinuePutting);
	JsonObject->TryGetNumberField("Mulligan_Count", OutGameOptionInfo.Mulligan_Count);

	double TempDouble = 0.0;
	if (JsonObject->TryGetNumberField("Concede_Distance", TempDouble)) OutGameOptionInfo.Concede_Distance = (float)TempDouble;
	if (JsonObject->TryGetNumberField("Green_Speed", TempDouble)) OutGameOptionInfo.Green_Speed = (float)TempDouble;

	JsonObject->TryGetNumberField("PracticeBall", OutGameOptionInfo.PracticeBall);
	JsonObject->TryGetNumberField("Movie_SaveCount", OutGameOptionInfo.Movie_SaveCount);
	JsonObject->TryGetNumberField("Camera_Mode", OutGameOptionInfo.Camera_Mode);
	JsonObject->TryGetNumberField("GameType", OutGameOptionInfo.GameType);
	JsonObject->TryGetNumberField("SwingMotion", OutGameOptionInfo.SwingMotion);
	JsonObject->TryGetNumberField("RangeSwingMotion", OutGameOptionInfo.RangeSwingMotion);
	return true;
}

// -------------------- Map --------------------

TSharedPtr<FJsonObject> UJsonHandler::MapInfoToJson(const FMapInfo& MapInfo)
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	JsonObject->SetStringField("MapName", MapInfo.MapName);
	JsonObject->SetStringField("PakName", MapInfo.PakName);
	JsonObject->SetStringField("CCName", MapInfo.CCName);
	JsonObject->SetNumberField("Sublevel", MapInfo.Sublevel);
	JsonObject->SetStringField("MapDescription", MapInfo.MapDescription);
	JsonObject->SetStringField("MapThumbnail", MapInfo.MapThumbnail);
	JsonObject->SetNumberField("HoleCount", MapInfo.HoleCount);

	TArray<TSharedPtr<FJsonValue>> ParScoresArray;
	for (int32 Score : MapInfo.ParScores)
	{
		ParScoresArray.Add(MakeShareable(new FJsonValueNumber(Score)));
	}
	JsonObject->SetArrayField("ParScores", ParScoresArray);

	TArray<TSharedPtr<FJsonValue>> HoleLengthsArray;
	for (float Length : MapInfo.HoleLengths)
	{
		HoleLengthsArray.Add(MakeShareable(new FJsonValueNumber(Length)));
	}
	JsonObject->SetArrayField("HoleLengths", HoleLengthsArray);

	TArray<TSharedPtr<FJsonValue>> TeePositionsArray;
	for (const FVector& Position : MapInfo.TeePositions)
	{
		TSharedPtr<FJsonObject> VectorObject = MakeShareable(new FJsonObject);
		VectorObject->SetNumberField("X", Position.X);
		VectorObject->SetNumberField("Y", Position.Y);
		VectorObject->SetNumberField("Z", Position.Z);
		TeePositionsArray.Add(MakeShareable(new FJsonValueObject(VectorObject)));
	}
	JsonObject->SetArrayField("TeePositions", TeePositionsArray);

	TArray<TSharedPtr<FJsonValue>> HolecupPositionsArray;
	for (const FVector& Position : MapInfo.HolecupPositions)
	{
		TSharedPtr<FJsonObject> VectorObject = MakeShareable(new FJsonObject);
		VectorObject->SetNumberField("X", Position.X);
		VectorObject->SetNumberField("Y", Position.Y);
		VectorObject->SetNumberField("Z", Position.Z);
		HolecupPositionsArray.Add(MakeShareable(new FJsonValueObject(VectorObject)));
	}
	JsonObject->SetArrayField("HolecupPositions", HolecupPositionsArray);

	TArray<TSharedPtr<FJsonValue>> OBLinesArray;
	for (const FOBLine& OBLine : MapInfo.OBLines)
	{
		TArray<TSharedPtr<FJsonValue>> PointsArray;
		for (const FVector& Point : OBLine.Points)
		{
			TSharedPtr<FJsonObject> VectorObject = MakeShareable(new FJsonObject);
			VectorObject->SetNumberField("X", Point.X);
			VectorObject->SetNumberField("Y", Point.Y);
			VectorObject->SetNumberField("Z", Point.Z);
			PointsArray.Add(MakeShareable(new FJsonValueObject(VectorObject)));
		}
		TSharedPtr<FJsonObject> OBLineObject = MakeShareable(new FJsonObject);
		OBLineObject->SetArrayField("Points", PointsArray);
		OBLinesArray.Add(MakeShareable(new FJsonValueObject(OBLineObject)));
	}
	JsonObject->SetArrayField("OBLines", OBLinesArray);

	return JsonObject;
}

bool UJsonHandler::JsonToMapInfo(const TSharedPtr<FJsonObject>& JsonObject, FMapInfo& OutMapInfo)
{
	if (!JsonObject.IsValid())
	{
		return false;
	}

	JsonObject->TryGetStringField("MapName", OutMapInfo.MapName);
	JsonObject->TryGetStringField("PakName", OutMapInfo.PakName);
	JsonObject->TryGetStringField("CCName", OutMapInfo.CCName);
	JsonObject->TryGetNumberField("Sublevel", OutMapInfo.Sublevel);
	JsonObject->TryGetStringField("MapDescription", OutMapInfo.MapDescription);
	JsonObject->TryGetStringField("MapThumbnail", OutMapInfo.MapThumbnail);
	JsonObject->TryGetNumberField("HoleCount", OutMapInfo.HoleCount);

	OutMapInfo.ParScores.Empty();
	const TArray<TSharedPtr<FJsonValue>>* ParScoresArray = nullptr;
	if (JsonObject->TryGetArrayField("ParScores", ParScoresArray) && ParScoresArray)
	{
		for (const TSharedPtr<FJsonValue>& ScoreValue : *ParScoresArray)
		{
			if (ScoreValue.IsValid())
			{
				OutMapInfo.ParScores.Add((int32)ScoreValue->AsNumber());
			}
		}
	}

	OutMapInfo.HoleLengths.Empty();
	const TArray<TSharedPtr<FJsonValue>>* HoleLengthsArray = nullptr;
	if (JsonObject->TryGetArrayField("HoleLengths", HoleLengthsArray) && HoleLengthsArray)
	{
		for (const TSharedPtr<FJsonValue>& LengthValue : *HoleLengthsArray)
		{
			if (LengthValue.IsValid())
			{
				double TempDouble = 0.0;
				if (LengthValue->TryGetNumber(TempDouble))
				{
					OutMapInfo.HoleLengths.Add((float)TempDouble);
				}
			}
		}
	}

	OutMapInfo.TeePositions.Empty();
	const TArray<TSharedPtr<FJsonValue>>* TeePositionsArray = nullptr;
	if (JsonObject->TryGetArrayField("TeePositions", TeePositionsArray) && TeePositionsArray)
	{
		for (const TSharedPtr<FJsonValue>& PositionValue : *TeePositionsArray)
		{
			if (PositionValue.IsValid() && PositionValue->Type == EJson::Object)
			{
				TSharedPtr<FJsonObject> VectorObject = PositionValue->AsObject();
				double X = 0.0, Y = 0.0, Z = 0.0;
				VectorObject->TryGetNumberField("X", X);
				VectorObject->TryGetNumberField("Y", Y);
				VectorObject->TryGetNumberField("Z", Z);
				OutMapInfo.TeePositions.Add(FVector((float)X, (float)Y, (float)Z));
			}
		}
	}

	OutMapInfo.HolecupPositions.Empty();
	const TArray<TSharedPtr<FJsonValue>>* HolecupPositionsArray = nullptr;
	if (JsonObject->TryGetArrayField("HolecupPositions", HolecupPositionsArray) && HolecupPositionsArray)
	{
		for (const TSharedPtr<FJsonValue>& PositionValue : *HolecupPositionsArray)
		{
			if (PositionValue.IsValid() && PositionValue->Type == EJson::Object)
			{
				TSharedPtr<FJsonObject> VectorObject = PositionValue->AsObject();
				double X = 0.0, Y = 0.0, Z = 0.0;
				VectorObject->TryGetNumberField("X", X);
				VectorObject->TryGetNumberField("Y", Y);
				VectorObject->TryGetNumberField("Z", Z);
				OutMapInfo.HolecupPositions.Add(FVector((float)X, (float)Y, (float)Z));
			}
		}
	}

	OutMapInfo.OBLines.Empty();
	const TArray<TSharedPtr<FJsonValue>>* OBLinesArray = nullptr;
	if (JsonObject->TryGetArrayField("OBLines", OBLinesArray) && OBLinesArray)
	{
		for (const TSharedPtr<FJsonValue>& OBLineValue : *OBLinesArray)
		{
			if (OBLineValue.IsValid() && OBLineValue->Type == EJson::Object)
			{
				TSharedPtr<FJsonObject> OBLineObject = OBLineValue->AsObject();

				FOBLine OBLine;
				const TArray<TSharedPtr<FJsonValue>>* PointsArray = nullptr;
				if (OBLineObject->TryGetArrayField("Points", PointsArray) && PointsArray)
				{
					for (const TSharedPtr<FJsonValue>& PointValue : *PointsArray)
					{
						if (PointValue.IsValid() && PointValue->Type == EJson::Object)
						{
							TSharedPtr<FJsonObject> VectorObject = PointValue->AsObject();
							double X = 0.0, Y = 0.0, Z = 0.0;
							VectorObject->TryGetNumberField("X", X);
							VectorObject->TryGetNumberField("Y", Y);
							VectorObject->TryGetNumberField("Z", Z);
							OBLine.Points.Add(FVector((float)X, (float)Y, (float)Z));
						}
					}
				}

				OutMapInfo.OBLines.Add(OBLine);
			}
		}
	}

	return true;
}

// -------------------- SystemConfig --------------------

bool UJsonHandler::LoadSystemConfigFromJson(FSystemConfig& OutConfig, const FString& FilePath)
{
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to load SystemConfig from: %s"), *FilePath);

		// 파일이 없으면 기본값으로 생성
		SaveSystemConfigToJson(OutConfig, FilePath);
		return false;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to parse SystemConfig JSON"));
		return false;
	}

	if (JsonObject->HasField(TEXT("ComPort")))
	{
		OutConfig.ComPort = JsonObject->GetIntegerField(TEXT("ComPort"));
	}

	if (JsonObject->HasField(TEXT("BaudRate")))
	{
		OutConfig.BaudRate = JsonObject->GetIntegerField(TEXT("BaudRate"));
	}

	if (JsonObject->HasField(TEXT("AutoTeeEnabled")))
	{
		OutConfig.bAutoTeeEnabled = JsonObject->GetBoolField(TEXT("AutoTeeEnabled"));
	}

	if (JsonObject->HasField(TEXT("KeyRepeatInterval")))
	{
		OutConfig.KeyRepeatInterval = (float)JsonObject->GetNumberField(TEXT("KeyRepeatInterval"));
	}

	if (JsonObject->HasField(TEXT("KeyRepeatDelay")))
	{
		OutConfig.KeyRepeatDelay = (float)JsonObject->GetNumberField(TEXT("KeyRepeatDelay"));
	}

	UE_LOG(LogTemp, Log, TEXT("✅ SystemConfig loaded: ComPort=%d, BaudRate=%d, Enabled=%s"),
		OutConfig.ComPort, OutConfig.BaudRate,
		OutConfig.bAutoTeeEnabled ? TEXT("True") : TEXT("False"));

	return true;
}

bool UJsonHandler::SaveSystemConfigToJson(const FSystemConfig& Config, const FString& FilePath)
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

	JsonObject->SetNumberField(TEXT("ComPort"), Config.ComPort);
	JsonObject->SetNumberField(TEXT("BaudRate"), Config.BaudRate);
	JsonObject->SetBoolField(TEXT("AutoTeeEnabled"), Config.bAutoTeeEnabled);
	JsonObject->SetNumberField(TEXT("KeyRepeatInterval"), Config.KeyRepeatInterval);
	JsonObject->SetNumberField(TEXT("KeyRepeatDelay"), Config.KeyRepeatDelay);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);

	if (!FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to serialize SystemConfig"));
		return false;
	}

	if (!FFileHelper::SaveStringToFile(OutputString, *FilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to save SystemConfig to: %s"), *FilePath);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("✅ SystemConfig saved to: %s"), *FilePath);
	return true;
}
