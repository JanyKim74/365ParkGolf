// Fill out your copyright notice in the Description page of Project Settings.


#include "SubChangeCourse.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
ASubChangeCourse::ASubChangeCourse()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	SubLevelName = TEXT("info_AB"); // �⺻�� ���� (�����Ϳ��� ���� ����)
	bIsSubLevelLoaded = false;

}

// Called when the game starts or when spawned
void ASubChangeCourse::BeginPlay()
{
	Super::BeginPlay();
    ToggleSubLevel();
}

// Called every frame
void ASubChangeCourse::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}



void ASubChangeCourse::ToggleSubLevel()
{
    if (SubLevelName.IsNone())
    {
        UE_LOG(LogTemp, Warning, TEXT("SubLevelName is not set for ToggleSubLevel!"));
        return;
    }

    FLatentActionInfo LatentInfo;
    LatentInfo.CallbackTarget = this;
    LatentInfo.UUID = FGuid::NewGuid().A; // ���� ID ����

    if (!bIsSubLevelLoaded)
    {
        // ���� �ε�
        LatentInfo.ExecutionFunction = TEXT("OnSubLevelLoaded"); // �ε� �Ϸ� �� ȣ��� �Լ� �̸�
        LatentInfo.Linkage = 0; // �� ���� �����Ͽ� ���� LatentActionInfo�� ü�̴��� �� �ֽ��ϴ�.

        UGameplayStatics::LoadStreamLevel(
            this,
            SubLevelName,
            true,  // �ε� �Ϸ� �� ��� ���̰� ��
            false, // Initial Level Stream In (���� �׻� false)
            LatentInfo
        );
        UE_LOG(LogTemp, Log, TEXT("Attempting to load sublevel: %s"), *SubLevelName.ToString());
    }
    else
    {
        // ���� ��ε�
        LatentInfo.ExecutionFunction = TEXT("OnSubLevelUnloaded"); // ��ε� �Ϸ� �� ȣ��� �Լ� �̸�
        LatentInfo.Linkage = 0;

        UGameplayStatics::UnloadStreamLevel(
            this,
            SubLevelName,
            LatentInfo,
            false // ��ε带 ���� ���� �����带 ������� ���� (�񵿱�)
        );
        UE_LOG(LogTemp, Log, TEXT("Attempting to unload sublevel: %s"), *SubLevelName.ToString());
    }
}

void ASubChangeCourse::OnSubLevelLoaded()
{
    bIsSubLevelLoaded = true;
    UE_LOG(LogTemp, Log, TEXT("Sublevel %s loaded successfully!"), *SubLevelName.ToString());
}

void ASubChangeCourse::OnSubLevelUnloaded()
{
    bIsSubLevelLoaded = false;
    UE_LOG(LogTemp, Log, TEXT("Sublevel %s unloaded successfully!"), *SubLevelName.ToString());
}