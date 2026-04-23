#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ReadyBillboard.generated.h"

class AInGameMode;
class UBillboardComponent;

UCLASS()
class PARKDAY_API AReadyBillboard : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AReadyBillboard();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UTexture2D* ReadyImage;
	UTexture2D* NoReadyImage;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBillboardComponent* Billboard;

private:
	AInGameMode* GM;
};
