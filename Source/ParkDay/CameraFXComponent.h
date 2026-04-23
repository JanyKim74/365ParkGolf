// CameraFXComponent.h
#pragma once
#include "CoreMinimal.h"
#include "CameraFXComponent.generated.h"

class UCameraComponent;
class UNiagaraSystem;
class UNiagaraComponent;
class UParticleSystem;
class UParticleSystemComponent;
class AInGameMode;

UCLASS(ClassGroup = (FX), meta = (BlueprintSpawnableComponent))
class UCameraFXComponent : public USceneComponent
{
    GENERATED_BODY()

    virtual void BeginPlay() override;
public:
    UPROPERTY(EditDefaultsOnly, Category = "FX") float HeightOffset = 80.f;

    void PlayHoleInFX(int32 inPlayerShotCount);
    void PlayChanceFX(int32 inPlayerShotCount);
private:
    AInGameMode* GM;
};