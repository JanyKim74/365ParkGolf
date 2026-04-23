// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DataTableStruct.generated.h"


USTRUCT(BlueprintType)
struct FScoreIcon : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScoreIcon")
	int32 Score = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScoreIcon")
	UTexture2D* Icon = nullptr;
};


USTRUCT(BlueprintType)
struct FResultParticle : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	int32 Score = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	TSubclassOf<AActor> BP_Result = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chance")
	TSubclassOf<AActor> BP_Chance = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chance")
	UTexture2D* Chance_PlayerSlot_Texture = nullptr;
};

USTRUCT(BlueprintType)
struct FResultUI : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	int32 Score = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	UTexture2D* ResultTexture = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	TSoftObjectPtr<USoundBase> ResultSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	TSoftObjectPtr<USoundBase> ConcedeSound;
};

USTRUCT(BlueprintType)
struct FBallParticle : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BallParticle")
	FString ParticleName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BallParticle")
	TSubclassOf<AActor> BP_Particle;
};

USTRUCT(BlueprintType)
struct FBlueprintObject : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BPObject")
	FString BPName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BPObject")
	TSubclassOf<UObject> Blueprint;
};

USTRUCT(BlueprintType)
struct FSoundTableRow : public FTableRowBase
{
	GENERATED_BODY()

	// 실제 사운드 (SoftObject: 필요 시에만 로드)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<USoundBase> Sound;
	// 선택: 클래스/볼륨/피치/컨커런시 같은 정책 힌트
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<USoundClass> SoundClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Volume = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Pitch = 1.f;
};

USTRUCT(BlueprintType)
struct FShotResultData
{
	GENERATED_BODY()

public:
	// 결과 이미지 (소프트 레퍼런스)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShotResult")
	TSoftObjectPtr<UTexture2D> ShotResultImage;

	// 결과 사운드 (소프트 레퍼런스)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShotResult")
	TSoftObjectPtr<USoundBase> ShotResultSound;
};