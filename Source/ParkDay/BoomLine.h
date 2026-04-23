// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BoomLine.generated.h"

class AGolfBall;

UCLASS()
class PARKDAY_API ABoomLine : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ABoomLine();
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		float CurrentSpeed;

	UPROPERTY(BlueprintREadWrite, EditAnywhere)
		bool bApply;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
