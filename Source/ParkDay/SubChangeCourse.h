// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h" // UGameplayStatics�� ����ϱ� ���� �ʿ�
#include "SubChangeCourse.generated.h"

UCLASS()
class PARKDAY_API ASubChangeCourse : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ASubChangeCourse();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Streaming")
		FName SubLevelName; // ����� ���극���� �̸� (�����Ϳ��� ����)

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Level Streaming")
		bool bIsSubLevelLoaded;

	UFUNCTION(BlueprintCallable, Category = "Level Streaming")
		void ToggleSubLevel();

private:
	// �ε� �� ��ε� �Ϸ� �� ȣ��� �ݹ� �Լ���
	void OnSubLevelLoaded();
	void OnSubLevelUnloaded();

};
